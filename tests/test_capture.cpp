#include <capture/decklink_capture.h>
#include <capture/frame_buffer.h>
#include <sync/master_clock.h>
#include <core/logger.h>

#include <iostream>
#include <iomanip>
#include <csignal>
#include <atomic>

using namespace ares;

static std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

void print_hdr_type(HDRType type) {
    switch (type) {
        case HDRType::NONE:
            std::cout << "SDR";
            break;
        case HDRType::HDR10:
            std::cout << "HDR10";
            break;
        case HDRType::HLG:
            std::cout << "HLG";
            break;
        case HDRType::DOLBY_VISION:
            std::cout << "Dolby Vision";
            break;
    }
}

void print_pixel_format(PixelFormat format) {
    switch (format) {
        case PixelFormat::YUV422_8BIT:
            std::cout << "YUV422 8-bit";
            break;
        case PixelFormat::YUV422_10BIT:
            std::cout << "YUV422 10-bit";
            break;
        case PixelFormat::RGB_8BIT:
            std::cout << "RGB 8-bit";
            break;
        case PixelFormat::RGB_10BIT:
            std::cout << "RGB 10-bit";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "==================================" << std::endl;
    std::cout << "Ares Capture Module Test" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set log level to INFO
    core::Logger::getInstance().setLevel(core::LogLevel::INFO);

    // Create master clock
    LOG_INFO("Test", "Initializing master clock...");
    sync::MasterClock clock;

    auto clock_stats = clock.getStats();
    std::cout << "Clock resolution: " << clock_stats.resolution_ns << " ns" << std::endl;
    std::cout << std::endl;

    // Create frame buffer
    LOG_INFO("Test", "Creating frame buffer...");
    capture::FrameBuffer frame_buffer(3);

    // Create capture device
    LOG_INFO("Test", "Initializing DeckLink capture...");
    capture::DeckLinkCapture capture;

    // Configure capture (4K60 10-bit)
    capture::CaptureConfig config;
    config.device_index = 0;
    config.width = 3840;
    config.height = 2160;
    config.frame_rate = 60.0f;
    config.enable_10bit = true;

    Result result = capture.initialize(config);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Test", "Failed to initialize capture device");
        std::cerr << "ERROR: Failed to initialize DeckLink device" << std::endl;
        std::cerr << "Make sure:" << std::endl;
        std::cerr << "  1. DeckLink device is connected" << std::endl;
        std::cerr << "  2. decklink kernel module is loaded (modprobe decklink)" << std::endl;
        std::cerr << "  3. /dev/blackmagic* devices exist" << std::endl;
        return 1;
    }

    // Start capture
    LOG_INFO("Test", "Starting capture...");
    result = capture.start();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Test", "Failed to start capture");
        return 1;
    }

    std::cout << "Capture started successfully" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    std::cout << std::endl;

    // Capture loop
    uint64_t frame_count = 0;
    auto start_time = clock.now();
    auto last_stats_time = start_time;

    while (g_running) {
        VideoFrame frame;

        // Get frame from capture (100ms timeout)
        result = capture.getFrame(frame, 100);

        if (result == Result::ERROR_TIMEOUT) {
            continue;
        }

        if (result != Result::SUCCESS) {
            LOG_ERROR("Test", "Failed to get frame");
            break;
        }

        frame_count++;

        // Push to frame buffer
        frame_buffer.push(frame, true);

        // Print frame info every second
        auto now = clock.now();
        auto elapsed_since_stats = clock.elapsed(last_stats_time);

        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed_since_stats).count() >= 1) {
            // Get statistics
            auto capture_stats = capture.getStats();
            auto buffer_stats = frame_buffer.getStats();

            // Clear screen and move cursor to top
            std::cout << "\033[2J\033[1;1H";

            std::cout << "==================================" << std::endl;
            std::cout << "Ares Capture Test - Live Statistics" << std::endl;
            std::cout << "==================================" << std::endl;
            std::cout << std::endl;

            // Frame info
            std::cout << "Current Frame:" << std::endl;
            std::cout << "  Resolution:    " << frame.width << "x" << frame.height << std::endl;
            std::cout << "  Format:        ";
            print_pixel_format(frame.format);
            std::cout << std::endl;
            std::cout << "  HDR Type:      ";
            print_hdr_type(frame.hdr_metadata.type);
            std::cout << std::endl;
            std::cout << "  Frame Size:    " << (frame.size / 1024 / 1024) << " MB" << std::endl;
            std::cout << std::endl;

            // HDR metadata
            if (frame.hdr_metadata.type == HDRType::HDR10) {
                std::cout << "HDR10 Metadata:" << std::endl;
                std::cout << "  MaxCLL:        " << frame.hdr_metadata.max_cll << " nits" << std::endl;
                std::cout << "  MaxFALL:       " << frame.hdr_metadata.max_fall << " nits" << std::endl;
                std::cout << "  Max Luminance: " << frame.hdr_metadata.max_luminance << " cd/m²" << std::endl;
                std::cout << std::endl;
            }

            // Capture statistics
            std::cout << "Capture Statistics:" << std::endl;
            std::cout << "  Frames Captured:  " << capture_stats.frames_captured << std::endl;
            std::cout << "  Frames Dropped:   " << capture_stats.frames_dropped << std::endl;
            std::cout << "  Current FPS:      " << std::fixed << std::setprecision(2)
                     << capture_stats.current_fps << std::endl;
            std::cout << "  Queue Size:       " << capture_stats.queue_size << std::endl;
            std::cout << std::endl;

            // Buffer statistics
            std::cout << "Frame Buffer Statistics:" << std::endl;
            std::cout << "  Frames Pushed:    " << buffer_stats.frames_pushed << std::endl;
            std::cout << "  Frames Popped:    " << buffer_stats.frames_popped << std::endl;
            std::cout << "  Frames Dropped:   " << buffer_stats.frames_dropped << std::endl;
            std::cout << "  Frames Repeated:  " << buffer_stats.frames_repeated << std::endl;
            std::cout << "  Frames Late:      " << buffer_stats.frames_late << std::endl;
            std::cout << "  Avg Latency:      " << std::fixed << std::setprecision(2)
                     << buffer_stats.avg_latency_ms << " ms" << std::endl;
            std::cout << "  Max Latency:      " << std::fixed << std::setprecision(2)
                     << buffer_stats.max_latency_ms << " ms" << std::endl;
            std::cout << "  Queue Size:       " << buffer_stats.current_queue_size << "/"
                     << frame_buffer.capacity() << std::endl;
            std::cout << std::endl;

            // Clock statistics
            auto clock_stats = clock.getStats();
            std::cout << "Master Clock Statistics:" << std::endl;
            std::cout << "  Resolution:       " << clock_stats.resolution_ns << " ns" << std::endl;
            std::cout << "  Uptime:           " << (clock_stats.uptime_ns / 1000000000.0)
                     << " seconds" << std::endl;
            std::cout << "  now() calls:      " << clock_stats.now_calls << std::endl;
            std::cout << "  Avg call time:    " << std::fixed << std::setprecision(2)
                     << clock_stats.avg_call_time_ns << " ns" << std::endl;
            std::cout << std::endl;

            std::cout << "Press Ctrl+C to stop..." << std::endl;

            last_stats_time = now;
        }

        // Clean up frame data
        if (frame.data) {
            delete[] frame.data;
        }
    }

    std::cout << std::endl;
    std::cout << "Stopping capture..." << std::endl;

    // Stop capture
    capture.stop();

    // Print final statistics
    auto end_time = clock.now();
    auto total_time = clock.elapsed(start_time);
    double total_seconds = std::chrono::duration<double>(total_time).count();

    std::cout << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Final Statistics" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Total frames captured: " << frame_count << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(2)
             << total_seconds << " seconds" << std::endl;
    std::cout << "Average FPS: " << std::fixed << std::setprecision(2)
             << (frame_count / total_seconds) << std::endl;

    return 0;
}
