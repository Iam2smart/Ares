#include <ares/version.h>
#include <ares/types.h>
#include <ares/ares_config.h>
#include "config/config_manager.h"
#include "capture/decklink_capture.h"
#include "processing/processing_pipeline.h"
#include "display/drm_display.h"
#include "display/frame_rate_matcher.h"
#include "input/ir_remote.h"
#include "input/receiver_control.h"
#include "osd/menu_system.h"
#include "osd/osd_renderer.h"
#include "core/logger.h"
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <signal.h>

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        LOG_INFO("Main", "Received shutdown signal");
        g_running = false;
    }
}

static void print_version() {
    printf("Ares HDR Video Processor v%s\n", ares::VERSION_STRING);
    printf("Copyright (C) 2026\n");
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --config PATH    Configuration file path (default: /etc/ares/ares.ini)\n");
    printf("  -v, --version        Print version information\n");
    printf("  -h, --help           Print this help message\n");
    printf("  -d, --daemon         Run as daemon (suppress console output)\n");
    printf("  --validate-config    Validate configuration and exit\n");
}

int main(int argc, char* argv[]) {
    using namespace ares;

    const char* config_path = "/etc/ares/ares.ini";
    bool daemon_mode = false;
    bool validate_only = false;

    // Command-line argument parsing
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"daemon", no_argument, 0, 'd'},
        {"validate-config", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "c:vhd", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                config_path = optarg;
                break;
            case 'v':
                print_version();
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'd':
                daemon_mode = true;
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "validate-config") == 0) {
                    validate_only = true;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Print startup banner
    if (!daemon_mode) {
        print_version();
        printf("\n");
        printf("Starting Ares HDR Video Processor...\n");
        printf("Configuration: %s\n", config_path);
        printf("\n");
    }

    // Initialize logging
    Logger::initialize(daemon_mode ? LogLevel::WARN : LogLevel::INFO);
    LOG_INFO("Main", "Ares HDR Video Processor starting");
    LOG_INFO("Main", "Version: %s", VERSION_STRING);

    // Load configuration
    config::ConfigManager config_manager;
    AresConfig config;
    Result result = config_manager.loadConfig(config_path, config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Main", "Failed to load configuration");
        return 1;
    }

    // Configure logger based on config
    if (config.log_level == "DEBUG") Logger::setLevel(LogLevel::DEBUG);
    else if (config.log_level == "INFO") Logger::setLevel(LogLevel::INFO);
    else if (config.log_level == "WARN") Logger::setLevel(LogLevel::WARN);
    else if (config.log_level == "ERROR") Logger::setLevel(LogLevel::ERROR);

    // If validate-only, just exit
    if (validate_only) {
        printf("Configuration validation successful\n");
        return 0;
    }

    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize DeckLink capture
    LOG_INFO("Main", "Initializing DeckLink capture...");
    capture::DeckLinkCapture capture;
    result = capture.initialize(config.capture.device_index);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Main", "Failed to initialize DeckLink capture");
        return 1;
    }
    LOG_INFO("Main", "DeckLink capture initialized successfully");

    // Initialize DRM display
    LOG_INFO("Main", "Initializing DRM display...");
    display::DRMDisplay display;
    result = display.initialize(config.display);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Main", "Failed to initialize DRM display");
        capture.shutdown();
        return 1;
    }
    LOG_INFO("Main", "DRM display initialized successfully");

    // Initialize frame rate matcher
    LOG_INFO("Main", "Initializing frame rate matcher...");
    display::FrameRateMatcher framerate_matcher;
    result = framerate_matcher.initialize(&display, &capture);
    if (result != Result::SUCCESS) {
        LOG_WARN("Main", "Failed to initialize frame rate matcher");
    }

    // Initialize processing pipeline
    LOG_INFO("Main", "Initializing processing pipeline...");
    processing::ProcessingPipeline pipeline;
    result = pipeline.initialize(config.processing, &display);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Main", "Failed to initialize processing pipeline");
        display.shutdown();
        capture.shutdown();
        return 1;
    }
    LOG_INFO("Main", "Processing pipeline initialized successfully");

    // Initialize OSD renderer
    LOG_INFO("Main", "Initializing OSD...");
    osd::OSDRenderer osd_renderer;
    result = osd_renderer.initialize(config.display.mode.width, config.display.mode.height, config.osd);
    if (result != Result::SUCCESS) {
        LOG_WARN("Main", "Failed to initialize OSD renderer");
    }

    // Initialize IR remote
    LOG_INFO("Main", "Initializing IR remote...");
    input::IRRemote ir_remote;
    result = ir_remote.initialize("/dev/input/event0");  // TODO: Make configurable
    if (result != Result::SUCCESS) {
        LOG_WARN("Main", "Failed to initialize IR remote (continuing without remote)");
    }

    // Initialize menu system
    LOG_INFO("Main", "Initializing menu system...");
    osd::MenuSystem menu;
    result = menu.initialize(config.osd, &osd_renderer, &ir_remote);
    if (result != Result::SUCCESS) {
        LOG_WARN("Main", "Failed to initialize menu system");
    }

    // Initialize receiver (optional)
    input::ReceiverControl receiver;
    bool receiver_enabled = false;
    if (config.receiver.enabled) {
        LOG_INFO("Main", "Initializing receiver control...");
        result = receiver.initialize(config.receiver.ip_address, config.receiver.port);
        if (result == Result::SUCCESS) {
            receiver.setMonitoringEnabled(true);
            receiver_enabled = true;
            LOG_INFO("Main", "Receiver control initialized successfully");
        } else {
            LOG_WARN("Main", "Failed to initialize receiver control (continuing without receiver)");
        }
    }

    // Start capture
    LOG_INFO("Main", "Starting capture...");
    result = capture.start();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Main", "Failed to start capture");
        pipeline.shutdown();
        display.shutdown();
        capture.shutdown();
        return 1;
    }

    LOG_INFO("Main", "Initialization complete, entering main loop");
    printf("\nAres is now running. Press Ctrl+C to stop.\n\n");

    // Statistics
    uint64_t frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_stats_time = start_time;

    // Main processing loop
    while (g_running) {
        // Get frame from capture
        VideoFrame input_frame;
        result = capture.getFrame(input_frame, 100);  // 100ms timeout

        if (result == Result::ERROR_TIMEOUT) {
            // No frame available, continue
            continue;
        } else if (result != Result::SUCCESS) {
            LOG_ERROR("Main", "Failed to get frame from capture");
            break;
        }

        // Update frame rate matcher (automatic display mode switching)
        framerate_matcher.update();

        // Process frame through pipeline
        VideoFrame output_frame;
        result = pipeline.processFrame(input_frame, output_frame);
        if (result != Result::SUCCESS) {
            LOG_ERROR("Main", "Failed to process frame");
            continue;
        }

        // Update menu system (handle input, update OSD)
        menu.update();

        // Render volume overlay if receiver is active and volume changed recently
        if (receiver_enabled) {
            auto volume_info = receiver.getVolumeInfo();
            if (volume_info.changed) {
                // Check if volume change was recent (within last 3 seconds)
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                if (now - volume_info.last_change_ms < 3000) {
                    float opacity = 1.0f;
                    // Fade out in last 500ms
                    if (now - volume_info.last_change_ms > 2500) {
                        opacity = 1.0f - (now - volume_info.last_change_ms - 2500) / 500.0f;
                    }
                    osd_renderer.drawVolumeOverlay(volume_info.level, volume_info.muted, opacity);
                }
            }
        }

        // Composite OSD over video if menu is visible
        if (menu.isVisible()) {
            menu.render();
        }

        // Present frame to display
        result = display.present(output_frame);
        if (result != Result::SUCCESS) {
            LOG_ERROR("Main", "Failed to present frame");
            continue;
        }

        frame_count++;

        // Print statistics every 10 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count();
        if (elapsed >= 10) {
            auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            double avg_fps = frame_count / (double)total_elapsed;

            auto capture_stats = capture.getStats();
            auto pipeline_stats = pipeline.getStats();
            auto matcher_stats = framerate_matcher.getStats();

            LOG_INFO("Main", "=== Statistics ===");
            LOG_INFO("Main", "Frames processed: %lu (%.2f fps avg)", frame_count, avg_fps);
            LOG_INFO("Main", "Frames dropped: %lu", capture_stats.frames_dropped);
            LOG_INFO("Main", "Source FPS: %.3f (stable: %s)",
                     matcher_stats.current_source_fps,
                     matcher_stats.mode_matched ? "yes" : "no");
            LOG_INFO("Main", "Display refresh: %.2f Hz", matcher_stats.current_display_refresh);
            LOG_INFO("Main", "Mode switches: %lu", matcher_stats.mode_switches);
            LOG_INFO("Main", "Processing time: %.2f ms/frame", pipeline_stats.avg_processing_time_ms);

            last_stats_time = now;
        }
    }

    // Shutdown sequence
    LOG_INFO("Main", "Shutting down...");

    // Stop capture first
    capture.stop();

    // Shutdown all modules in reverse order
    if (receiver_enabled) {
        receiver.shutdown();
    }
    menu.shutdown();
    ir_remote.shutdown();
    osd_renderer.shutdown();
    pipeline.shutdown();
    display.shutdown();
    capture.shutdown();

    // Print final statistics
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time).count();
    double avg_fps = frame_count / (double)total_time;

    printf("\n=== Final Statistics ===\n");
    printf("Total frames: %lu\n", frame_count);
    printf("Total time: %lu seconds\n", total_time);
    printf("Average FPS: %.2f\n", avg_fps);
    printf("\n");

    LOG_INFO("Main", "Ares shutdown complete");
    printf("Ares stopped cleanly.\n");

    return 0;
}
