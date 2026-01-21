#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>
#include <signal.h>
#include <unistd.h>

#include "processing/processing_pipeline.h"
#include "core/logger.h"

using namespace ares;
using namespace ares::processing;

static volatile bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    g_running = false;
}

void printStats(const ProcessingPipeline::Stats& stats) {
    std::cout << "\n========== Processing Pipeline Statistics ==========\n";
    std::cout << "Frames processed: " << stats.frames_processed << "\n";
    std::cout << "Avg frame time:   " << std::fixed << std::setprecision(2)
              << stats.avg_frame_time_ms << " ms ("
              << (1000.0 / stats.avg_frame_time_ms) << " fps)\n";
    std::cout << "Last frame time:  " << stats.total_frame_time_ms << " ms\n";

    std::cout << "\n----- Frame Dimensions -----\n";
    std::cout << "Input:       " << stats.input_width << "x" << stats.input_height << "\n";
    std::cout << "After crop:  " << stats.after_crop_width << "x" << stats.after_crop_height << "\n";
    std::cout << "After NLS:   " << stats.after_nls_width << "x" << stats.after_nls_height << "\n";
    std::cout << "Output:      " << stats.output_width << "x" << stats.output_height << "\n";

    if (stats.current_crop.top > 0 || stats.current_crop.bottom > 0 ||
        stats.current_crop.left > 0 || stats.current_crop.right > 0) {
        std::cout << "\n----- Black Bar Detection -----\n";
        std::cout << "Crop region: T=" << stats.current_crop.top
                  << " B=" << stats.current_crop.bottom
                  << " L=" << stats.current_crop.left
                  << " R=" << stats.current_crop.right << "\n";
        std::cout << "Confidence:  " << (stats.current_crop.confidence * 100.0f) << "%\n";
        std::cout << "Stable:      " << (stats.crop_stable ? "YES" : "NO") << "\n";
        std::cout << "Symmetric:   " << (stats.current_crop.is_symmetric ? "YES" : "NO") << "\n";
    }

    std::cout << "\n----- Black Bar Detector Stats -----\n";
    std::cout << "Frames analyzed: " << stats.black_bar_stats.frames_analyzed << "\n";
    std::cout << "Bars detected:   " << stats.black_bar_stats.bars_detected << "\n";
    std::cout << "Confidence:      " << (stats.black_bar_stats.current_confidence * 100.0f) << "%\n";

    if (stats.nls_stats.frames_processed > 0) {
        std::cout << "\n----- NLS Warping Stats -----\n";
        std::cout << "Frames processed: " << stats.nls_stats.frames_processed << "\n";
        std::cout << "Avg frame time:   " << stats.nls_stats.avg_frame_time_ms << " ms\n";
        std::cout << "Target aspect:    " << stats.nls_stats.current_aspect_ratio << ":1\n";
    }

    std::cout << "\n----- Tone Mapping Stats -----\n";
    std::cout << "Frames processed: " << stats.tone_mapping_stats.frames_processed << "\n";
    std::cout << "Avg frame time:   " << stats.tone_mapping_stats.avg_frame_time_ms << " ms\n";

    std::cout << "===================================================\n\n";
}

int main(int argc, char** argv) {
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "Ares Processing Pipeline Test\n";
    std::cout << "==============================\n\n";

    // Create default configuration
    ProcessingConfig config;

    // Tone mapping settings
    config.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
    config.tone_mapping.target_nits = 100.0f;
    config.tone_mapping.source_nits = 1000.0f;
    config.tone_mapping.contrast = 1.0f;
    config.tone_mapping.saturation = 1.0f;

    // Black bar detection
    config.black_bars.enabled = true;
    config.black_bars.threshold = 16;
    config.black_bars.min_content_height = 0.5f;
    config.black_bars.min_content_width = 0.5f;
    config.black_bars.detection_frames = 10;
    config.black_bars.confidence_threshold = 0.8f;
    config.black_bars.symmetric_only = true;
    config.black_bars.auto_crop = true;
    config.black_bars.crop_smoothing = 0.3f;

    // NLS (Non-Linear Stretch) - aspect ratio warping
    config.nls.enabled = false;  // Disabled by default for testing
    config.nls.target_aspect = NLSConfig::TargetAspect::SCOPE_235;
    config.nls.center_protect = 1.0f;
    config.nls.horizontal_stretch = 0.5f;
    config.nls.vertical_stretch = 0.5f;
    config.nls.interpolation = NLSConfig::InterpolationQuality::BICUBIC;

    // Quality
    config.quality = ProcessingConfig::Quality::BALANCED;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--enable-nls") {
            config.nls.enabled = true;
            std::cout << "NLS aspect ratio warping enabled (16:9 -> 2.35:1)\n";
        } else if (arg == "--target-nits") {
            if (i + 1 < argc) {
                config.tone_mapping.target_nits = std::stof(argv[++i]);
                std::cout << "Target nits: " << config.tone_mapping.target_nits << "\n";
            }
        } else if (arg == "--algorithm") {
            if (i + 1 < argc) {
                std::string algo = argv[++i];
                if (algo == "bt2390") config.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
                else if (algo == "reinhard") config.tone_mapping.algorithm = ToneMappingAlgorithm::REINHARD;
                else if (algo == "hable") config.tone_mapping.algorithm = ToneMappingAlgorithm::HABLE;
                else if (algo == "mobius") config.tone_mapping.algorithm = ToneMappingAlgorithm::MOBIUS;
                else if (algo == "clip") config.tone_mapping.algorithm = ToneMappingAlgorithm::CLIP;
                std::cout << "Tone mapping algorithm: " << algo << "\n";
            }
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n";
            std::cout << "\nOptions:\n";
            std::cout << "  --enable-nls              Enable NLS aspect ratio warping\n";
            std::cout << "  --target-nits <value>     Set target brightness (default: 100)\n";
            std::cout << "  --algorithm <name>        Tone mapping algorithm:\n";
            std::cout << "                            bt2390, reinhard, hable, mobius, clip\n";
            std::cout << "  --help                    Show this help message\n";
            return 0;
        }
    }

    std::cout << "\n";

    // Create and initialize pipeline
    ProcessingPipeline pipeline;

    std::cout << "Initializing processing pipeline...\n";
    Result result = pipeline.initialize(config);
    if (result != Result::SUCCESS) {
        std::cerr << "ERROR: Failed to initialize processing pipeline\n";
        return 1;
    }

    std::cout << "Pipeline initialized successfully!\n\n";

    // Create test frame (simulated 1920x1080 HDR10 content with letterbox)
    VideoFrame test_input;
    test_input.width = 1920;
    test_input.height = 1080;
    test_input.format = PixelFormat::YUV420P_10BIT;
    test_input.size = test_input.width * test_input.height * 2;  // Simplified
    test_input.data = new uint8_t[test_input.size];
    test_input.pts = std::chrono::steady_clock::now();

    // Fill with test pattern (black bars on top/bottom)
    memset(test_input.data, 0, test_input.size);
    // Content area (no bars for now - in real scenario would have letterbox)
    for (uint32_t y = 0; y < test_input.height; y++) {
        for (uint32_t x = 0; x < test_input.width; x++) {
            test_input.data[y * test_input.width + x] = 128;  // Gray
        }
    }

    // HDR metadata
    test_input.hdr_metadata.type = HDRType::HDR10;
    test_input.hdr_metadata.max_cll = 1000;   // nits
    test_input.hdr_metadata.max_fall = 400;   // nits
    test_input.hdr_metadata.max_luminance = 1000;     // cd/m²
    test_input.hdr_metadata.min_luminance = 1;        // cd/m² * 10000

    std::cout << "Processing test frames...\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    uint64_t frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    // Process frames in a loop
    while (g_running && frame_count < 100) {  // Limit to 100 frames for testing
        VideoFrame output;

        test_input.pts = std::chrono::steady_clock::now();  // Current timestamp

        result = pipeline.processFrame(test_input, output);
        if (result != Result::SUCCESS) {
            std::cerr << "ERROR: Frame processing failed\n";
            break;
        }

        frame_count++;

        // Print stats every 30 frames
        if (frame_count % 30 == 0) {
            auto stats = pipeline.getStats();
            printStats(stats);
        }

        // Free output frame
        if (output.data) {
            delete[] output.data;
        }

        // Simulate 60 fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "\n\n========== Final Statistics ==========\n";
    std::cout << "Total frames processed: " << frame_count << "\n";
    std::cout << "Total time:             " << elapsed << " seconds\n";
    std::cout << "Average FPS:            " << (frame_count / elapsed) << "\n";

    auto final_stats = pipeline.getStats();
    printStats(final_stats);

    // Cleanup
    delete[] test_input.data;

    std::cout << "Test completed successfully!\n";

    return 0;
}
