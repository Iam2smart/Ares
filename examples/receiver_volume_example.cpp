// Example: Integrating Receiver Volume Display with Ares OSD
// This shows how to connect to an Integra/Onkyo receiver and display volume changes

#include "input/receiver_control.h"
#include "osd/osd_renderer.h"
#include "core/logger.h"
#include <chrono>
#include <thread>

using namespace ares;
using namespace ares::input;
using namespace ares::osd;

// Volume overlay manager
class VolumeOverlayManager {
public:
    VolumeOverlayManager(OSDRenderer* osd_renderer)
        : m_osd_renderer(osd_renderer) {}

    // Callback when volume changes
    void onVolumeChanged(const ReceiverControl::VolumeInfo& volume_info) {
        m_current_volume = volume_info;
        m_visible = true;
        m_last_change_time = std::chrono::steady_clock::now();

        LOG_INFO("Volume", "Volume changed: %d%% (muted: %s)",
                 volume_info.level, volume_info.muted ? "yes" : "no");
    }

    // Update and render volume overlay
    void update() {
        if (!m_visible) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_last_change_time).count();

        // Auto-hide after 3 seconds
        const int64_t display_duration = 3000;
        const int64_t fade_duration = 500;

        if (elapsed_ms > display_duration + fade_duration) {
            m_visible = false;
            return;
        }

        // Calculate opacity (fade out in last 500ms)
        float opacity = 1.0f;
        if (elapsed_ms > display_duration) {
            int64_t fade_elapsed = elapsed_ms - display_duration;
            opacity = 1.0f - (float)fade_elapsed / fade_duration;
        }

        // Draw volume overlay
        m_osd_renderer->drawVolumeOverlay(
            m_current_volume.level,
            m_current_volume.muted,
            opacity
        );
    }

    bool isVisible() const { return m_visible; }

private:
    OSDRenderer* m_osd_renderer;
    ReceiverControl::VolumeInfo m_current_volume;
    bool m_visible = false;
    std::chrono::steady_clock::time_point m_last_change_time;
};

int main(int argc, char** argv) {
    // Initialize logging
    LOG_INFO("Example", "Receiver Volume Display Example");

    // Configuration
    const std::string receiver_ip = "192.168.1.100";  // Your Integra receiver IP
    const uint16_t receiver_port = 60128;             // Default EISCP port

    // Initialize OSD renderer
    OSDRenderer osd_renderer;
    OSDConfig osd_config;
    osd_config.font_family = "Sans";
    osd_config.font_size = 24;

    Result result = osd_renderer.initialize(1920, 1080, osd_config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Example", "Failed to initialize OSD renderer");
        return 1;
    }

    // Initialize receiver control
    ReceiverControl receiver;
    result = receiver.initialize(receiver_ip, receiver_port);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Example", "Failed to connect to receiver at %s:%d",
                 receiver_ip.c_str(), receiver_port);
        return 1;
    }

    LOG_INFO("Example", "Connected to receiver successfully!");

    // Create volume overlay manager
    VolumeOverlayManager overlay_manager(&osd_renderer);

    // Register volume change callback
    receiver.setVolumeCallback([&overlay_manager](const ReceiverControl::VolumeInfo& info) {
        overlay_manager.onVolumeChanged(info);
    });

    // Enable monitoring for automatic updates
    receiver.setMonitoringEnabled(true);

    LOG_INFO("Example", "Monitoring receiver volume...");
    LOG_INFO("Example", "Adjust volume on your receiver to see the overlay");
    LOG_INFO("Example", "Press Ctrl+C to exit");

    // Main loop - normally this would be your video rendering loop
    while (true) {
        // Begin OSD frame
        osd_renderer.beginFrame();

        // Update and render volume overlay if visible
        overlay_manager.update();

        // End OSD frame
        osd_renderer.endFrame();

        // In a real application, you would composite this OSD over video here
        // For this example, we just sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    // Cleanup
    receiver.shutdown();
    osd_renderer.shutdown();

    return 0;
}

/*
 * Integration with Main Ares Pipeline:
 *
 * In your main processing loop (e.g., processing_pipeline.cpp):
 *
 * 1. Initialize receiver control during startup:
 *
 *    ReceiverControl m_receiver;
 *    VolumeOverlayManager m_volume_overlay;
 *
 *    m_receiver.initialize(config.receiver.ip_address, config.receiver.port);
 *    m_receiver.setVolumeCallback([this](const auto& info) {
 *        m_volume_overlay.onVolumeChanged(info);
 *    });
 *    m_receiver.setMonitoringEnabled(true);
 *
 * 2. In your render loop:
 *
 *    // Render video frame
 *    processVideoFrame(input, output);
 *
 *    // Render OSD (menu, stats, etc.)
 *    m_osd_renderer.beginFrame();
 *    renderMainOSD();  // Your existing OSD rendering
 *
 *    // Add volume overlay if visible
 *    m_volume_overlay.update();
 *
 *    m_osd_renderer.endFrame();
 *
 *    // Composite OSD over video
 *    m_osd_compositor.composite(output, osd_data, ...);
 *
 * 3. The volume overlay will automatically show when volume changes and
 *    hide after 3 seconds with a smooth fade-out animation.
 */
