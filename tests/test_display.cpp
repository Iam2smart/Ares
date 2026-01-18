#include <iostream>
#include <iomanip>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <chrono>

#include "display/drm_display.h"
#include "display/vulkan_presenter.h"
#include "processing/vulkan_context.h"
#include "core/logger.h"

using namespace ares;
using namespace ares::display;
using namespace ares::processing;

static volatile bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    g_running = false;
}

void printDisplayInfo(const DRMDisplay::DisplayInfo& info) {
    std::cout << "\n========== Display Information ==========\n";
    std::cout << "Connector:      " << info.connector_name << " (ID: " << info.connector_id << ")\n";
    std::cout << "CRTC ID:        " << info.crtc_id << "\n";
    std::cout << "Current mode:   " << info.width << "x" << info.height
              << "@" << std::fixed << std::setprecision(2) << info.refresh_rate << "Hz\n";
    std::cout << "HDR supported:  " << (info.hdr_supported ? "Yes" : "No") << "\n";
    std::cout << "\nAvailable modes (" << info.available_modes.size() << "):\n";

    for (size_t i = 0; i < std::min(info.available_modes.size(), size_t(10)); i++) {
        const auto& mode = info.available_modes[i];
        std::cout << "  " << std::setw(2) << (i + 1) << ". "
                  << std::setw(4) << mode.width << "x" << std::setw(4) << mode.height
                  << " @ " << std::setw(6) << std::setprecision(2) << mode.refresh_rate << " Hz";
        if (mode.interlaced) std::cout << " (interlaced)";
        std::cout << "\n";
    }

    if (info.available_modes.size() > 10) {
        std::cout << "  ... and " << (info.available_modes.size() - 10) << " more\n";
    }

    std::cout << "=========================================\n\n";
}

void printStats(const DRMDisplay::Stats& drm_stats, const VulkanPresenter::Stats& vk_stats) {
    std::cout << "\n========== Display Statistics ==========\n";

    std::cout << "\n----- DRM Display -----\n";
    std::cout << "Frames presented: " << drm_stats.frames_presented << "\n";
    std::cout << "VBlank waits:     " << drm_stats.vblank_waits << "\n";
    std::cout << "Missed VBlanks:   " << drm_stats.missed_vblanks << "\n";
    std::cout << "Avg frame time:   " << std::fixed << std::setprecision(2)
              << drm_stats.avg_frame_time_ms << " ms\n";
    std::cout << "Last frame time:  " << drm_stats.last_frame_time_ms << " ms\n";

    std::cout << "\n----- Vulkan Presenter -----\n";
    std::cout << "Frames presented: " << vk_stats.frames_presented << "\n";
    std::cout << "Buffer swaps:     " << vk_stats.buffer_swaps << "\n";
    std::cout << "Dropped frames:   " << vk_stats.dropped_frames << "\n";
    std::cout << "Avg present time: " << vk_stats.avg_present_time_ms << " ms\n";
    std::cout << "Last present time:" << vk_stats.last_present_time_ms << " ms\n";

    if (vk_stats.frames_presented > 0) {
        double fps = 1000.0 / vk_stats.avg_present_time_ms;
        std::cout << "\nEffective FPS:    " << std::setprecision(1) << fps << "\n";
    }

    std::cout << "========================================\n\n";
}

int main(int argc, char** argv) {
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "Ares Display Output Test\n";
    std::cout << "========================\n\n";

    // Parse command line arguments
    std::string connector = "auto";
    std::string card = "/dev/dri/card0";
    bool vsync = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--connector" && i + 1 < argc) {
            connector = argv[++i];
        } else if (arg == "--card" && i + 1 < argc) {
            card = argv[++i];
        } else if (arg == "--no-vsync") {
            vsync = false;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "\nOptions:\n";
            std::cout << "  --connector <name>  Specify display connector (default: auto)\n";
            std::cout << "                      Examples: HDMI-A-1, DP-1, auto\n";
            std::cout << "  --card <device>     Specify DRM card (default: /dev/dri/card0)\n";
            std::cout << "  --no-vsync          Disable vsync\n";
            std::cout << "  --help              Show this help message\n";
            return 0;
        }
    }

    std::cout << "Configuration:\n";
    std::cout << "  DRM card:   " << card << "\n";
    std::cout << "  Connector:  " << connector << "\n";
    std::cout << "  VSync:      " << (vsync ? "enabled" : "disabled") << "\n";
    std::cout << "\n";

    // Create display configuration
    DisplayConfig display_config;
    display_config.connector = connector;
    display_config.card = card;
    display_config.auto_mode = true;
    display_config.vsync = vsync;
    display_config.buffer_count = 3;  // Triple buffering

    // Initialize Vulkan context
    std::cout << "Initializing Vulkan context...\n";
    VulkanContext vk_context;

    Result result = vk_context.initialize(false);  // No validation for performance
    if (result != Result::SUCCESS) {
        std::cerr << "ERROR: Failed to initialize Vulkan context\n";
        return 1;
    }

    auto vk_stats = vk_context.getStats();
    std::cout << "Vulkan initialized:\n";
    std::cout << "  GPU: " << vk_stats.device_name << "\n";
    std::cout << "  Driver: " << vk_stats.driver_version << "\n";
    std::cout << "  VRAM: " << vk_stats.total_memory_mb << " MB\n\n";

    // Initialize DRM display
    std::cout << "Initializing DRM display...\n";
    DRMDisplay drm_display;

    result = drm_display.initialize(display_config);
    if (result != Result::SUCCESS) {
        std::cerr << "ERROR: Failed to initialize DRM display\n";
        std::cerr << "Make sure you have permissions to access " << card << "\n";
        std::cerr << "You may need to add your user to the 'video' group:\n";
        std::cerr << "  sudo usermod -a -G video $USER\n";
        return 1;
    }

    auto display_info = drm_display.getDisplayInfo();
    printDisplayInfo(display_info);

    // Initialize Vulkan presenter
    std::cout << "Initializing Vulkan presenter...\n";
    VulkanPresenter presenter;

    result = presenter.initialize(&vk_context, &drm_display, display_config);
    if (result != Result::SUCCESS) {
        std::cerr << "ERROR: Failed to initialize Vulkan presenter\n";
        return 1;
    }

    std::cout << "Display system initialized successfully!\n\n";

    // Create test frame (solid color pattern)
    std::cout << "Creating test pattern...\n";
    VideoFrame test_frame;
    test_frame.width = display_info.width;
    test_frame.height = display_info.height;
    test_frame.format = PixelFormat::RGB_8BIT;
    test_frame.size = test_frame.width * test_frame.height * 3;
    test_frame.data = new uint8_t[test_frame.size];

    // Fill with gradient pattern
    for (uint32_t y = 0; y < test_frame.height; y++) {
        for (uint32_t x = 0; x < test_frame.width; x++) {
            size_t offset = (y * test_frame.width + x) * 3;

            // Create RGB gradient
            test_frame.data[offset + 0] = static_cast<uint8_t>((x * 255) / test_frame.width);  // R
            test_frame.data[offset + 1] = static_cast<uint8_t>((y * 255) / test_frame.height); // G
            test_frame.data[offset + 2] = 128;  // B (constant)
        }
    }

    std::cout << "Test pattern created: " << test_frame.width << "x" << test_frame.height << "\n";
    std::cout << "\nPresenting frames...\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    uint64_t frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    // Present frames
    while (g_running && frame_count < 300) {  // Limit to 300 frames (5 seconds at 60fps)
        test_frame.pts = frame_count * 16666667;  // 60 fps timestamp

        result = presenter.presentFrame(test_frame);
        if (result != Result::SUCCESS) {
            std::cerr << "ERROR: Failed to present frame " << frame_count << "\n";
            break;
        }

        frame_count++;

        // Print stats every 60 frames (1 second at 60fps)
        if (frame_count % 60 == 0) {
            auto drm_stats = drm_display.getStats();
            auto vk_stats = presenter.getStats();
            printStats(drm_stats, vk_stats);
        }

        // Update test pattern (animate)
        for (uint32_t y = 0; y < test_frame.height; y++) {
            for (uint32_t x = 0; x < test_frame.width; x++) {
                size_t offset = (y * test_frame.width + x) * 3;
                // Rotate colors
                test_frame.data[offset + 2] = static_cast<uint8_t>((frame_count * 2) % 256);
            }
        }

        // Maintain frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(end_time - start_time).count();

    // Final statistics
    std::cout << "\n\n========== Final Statistics ==========\n";
    std::cout << "Total frames:   " << frame_count << "\n";
    std::cout << "Total time:     " << std::fixed << std::setprecision(2) << elapsed << " seconds\n";
    std::cout << "Average FPS:    " << std::setprecision(1) << (frame_count / elapsed) << "\n";

    auto final_drm_stats = drm_display.getStats();
    auto final_vk_stats = presenter.getStats();
    printStats(final_drm_stats, final_vk_stats);

    // Cleanup
    delete[] test_frame.data;

    std::cout << "Test completed successfully!\n";
    std::cout << "\nNote: This is a basic display test. Full integration with\n";
    std::cout << "      processing pipeline will be implemented in the next step.\n";

    return 0;
}
