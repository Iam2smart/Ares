/**
 * OSD Test Tool - Test OSD rendering without DeckLink capture
 *
 * This tool tests the OSD system by generating a test pattern
 * and compositing the OSD on top of it.
 *
 * Usage: ./test_osd
 */

#include <ares/types.h>
#include <ares/osd_config.h>
#include "osd/osd_renderer.h"
#include "osd/menu_system.h"
#include "input/ir_remote.h"
#include "display/drm_display.h"
#include "core/logger.h"

#include <chrono>
#include <thread>
#include <signal.h>
#include <atomic>

using namespace ares;

static std::atomic<bool> g_running{true};

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        LOG_INFO("TestOSD", "Received shutdown signal");
        g_running = false;
    }
}

// Generate a test pattern frame
VideoFrame generateTestPattern(uint32_t width, uint32_t height, uint32_t frame_number) {
    VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.format = PixelFormat::RGB_8BIT;
    frame.size = width * height * 3;
    frame.data = new uint8_t[frame.size];
    frame.pts = Timestamp(std::chrono::nanoseconds(frame_number * 16667000)); // ~60 FPS

    // Generate color bars test pattern
    const int num_bars = 8;
    const int bar_width = width / num_bars;

    // Standard SMPTE color bars
    uint8_t colors[8][3] = {
        {180, 180, 180}, // White/Gray
        {180, 180, 16},  // Yellow
        {16,  180, 180}, // Cyan
        {16,  180, 16},  // Green
        {180, 16,  180}, // Magenta
        {180, 16,  16},  // Red
        {16,  16,  180}, // Blue
        {16,  16,  16}   // Black
    };

    // Add some animation based on frame number
    int offset = (frame_number / 4) % num_bars;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            int bar = ((x / bar_width) + offset) % num_bars;
            size_t idx = (y * width + x) * 3;

            // Top half: color bars
            if (y < height / 2) {
                frame.data[idx + 0] = colors[bar][0]; // R
                frame.data[idx + 1] = colors[bar][1]; // G
                frame.data[idx + 2] = colors[bar][2]; // B
            } else {
                // Bottom half: gradient
                uint8_t gray = (uint8_t)(255.0f * x / width);
                frame.data[idx + 0] = gray;
                frame.data[idx + 1] = gray;
                frame.data[idx + 2] = gray;
            }
        }
    }

    return frame;
}

int main(int argc, char* argv[]) {
    printf("=================================\n");
    printf("Ares OSD Test Tool\n");
    printf("=================================\n\n");

    // Initialize logging
    core::Logger::getInstance().setLevel(core::LogLevel::INFO);
    LOG_INFO("TestOSD", "Starting OSD test tool");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize display (for output)
    display::DRMDisplay display;
    DisplayConfig display_config;
    display_config.mode.width = 1920;
    display_config.mode.height = 1080;
    display_config.mode.refresh_rate = 60.0;

    Result result = display.initialize(display_config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("TestOSD", "Failed to initialize display");
        return 1;
    }
    LOG_INFO("TestOSD", "Display initialized: %dx%d @ %.2f Hz",
             display_config.mode.width, display_config.mode.height,
             display_config.mode.refresh_rate);

    // Initialize OSD renderer
    osd::OSDRenderer osd_renderer;
    OSDConfig osd_config = {}; // Use defaults
    result = osd_renderer.initialize(display_config.mode.width,
                                     display_config.mode.height,
                                     osd_config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("TestOSD", "Failed to initialize OSD renderer");
        display.shutdown();
        return 1;
    }
    LOG_INFO("TestOSD", "OSD renderer initialized");

    // Initialize IR remote (optional)
    input::IRRemote ir_remote;
    result = ir_remote.initialize("/dev/input/event0");
    if (result != Result::SUCCESS) {
        LOG_WARN("TestOSD", "Failed to initialize IR remote (continuing without it)");
    } else {
        LOG_INFO("TestOSD", "IR remote initialized");
    }

    // Initialize menu system
    osd::MenuSystem menu;
    result = menu.initialize(&osd_renderer, &ir_remote, osd_config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("TestOSD", "Failed to initialize menu system");
        osd_renderer.shutdown();
        display.shutdown();
        return 1;
    }
    LOG_INFO("TestOSD", "Menu system initialized");

    // Show menu immediately for testing
    menu.show();

    printf("\n");
    printf("OSD Test Running\n");
    printf("================\n");
    printf("- Color bar test pattern will be displayed\n");
    printf("- OSD menu is visible\n");
    printf("- Use IR remote or keyboard to test OSD navigation\n");
    printf("- Press Ctrl+C to exit\n");
    printf("\n");

    // Main loop
    uint32_t frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (g_running) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        // Generate test pattern
        VideoFrame test_frame = generateTestPattern(
            display_config.mode.width,
            display_config.mode.height,
            frame_count
        );

        // Update menu (handles input)
        menu.update(16.67f); // ~60 FPS frame time

        // Render OSD on test pattern
        if (menu.isVisible()) {
            menu.render();
            // The OSD is composited by the renderer
        }

        // Display frame (note: DRMDisplay uses pageFlip with framebuffer IDs,
        // this test tool currently only tests OSD rendering logic, not actual display output)
        (void)test_frame; // Frame would be presented via Vulkan presenter in real usage

        // Clean up frame data
        delete[] test_frame.data;

        frame_count++;

        // Print stats every 5 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= 5 && elapsed % 5 == 0 && frame_count % 300 == 0) {
            double avg_fps = frame_count / (double)elapsed;
            LOG_INFO("TestOSD", "Stats: %u frames, %.2f fps avg, menu %s",
                     frame_count, avg_fps, menu.isVisible() ? "visible" : "hidden");
        }

        // Frame pacing (target 60 FPS)
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_time = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
        auto target_frame_time = std::chrono::microseconds(16667); // 60 FPS

        if (frame_time < target_frame_time) {
            std::this_thread::sleep_for(target_frame_time - frame_time);
        }
    }

    // Cleanup
    LOG_INFO("TestOSD", "Shutting down...");
    menu.shutdown();
    ir_remote.shutdown();
    osd_renderer.shutdown();
    display.shutdown();

    // Print final stats
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time).count();
    double avg_fps = frame_count / (double)total_time;

    printf("\n=== Test Complete ===\n");
    printf("Total frames: %u\n", frame_count);
    printf("Total time: %ld seconds\n", total_time);
    printf("Average FPS: %.2f\n", avg_fps);
    printf("\n");

    LOG_INFO("TestOSD", "OSD test complete");
    return 0;
}
