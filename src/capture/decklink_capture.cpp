#include "decklink_capture.h"
#include "core/logger.h"

// DeckLink SDK headers
#include "DeckLinkAPI.h"

#include <cstring>
#include <cstdlib>
#include <dlfcn.h>

namespace ares {
namespace capture {

// DeckLink callback implementation
class DeckLinkCaptureCallback : public IDeckLinkInputCallback {
public:
    DeckLinkCaptureCallback(DeckLinkCapture* capture) : m_capture(capture), m_ref_count(1) {}

    // IUnknown interface
    virtual HRESULT QueryInterface(REFIID iid, LPVOID* ppv) override {
        return E_NOINTERFACE;
    }

    virtual ULONG AddRef() override {
        return __sync_add_and_fetch(&m_ref_count, 1);
    }

    virtual ULONG Release() override {
        int32_t new_ref = __sync_sub_and_fetch(&m_ref_count, 1);
        if (new_ref == 0) {
            delete this;
            return 0;
        }
        return new_ref;
    }

    // IDeckLinkInputCallback interface
    virtual HRESULT VideoInputFormatChanged(
        BMDVideoInputFormatChangedEvents events,
        IDeckLinkDisplayMode* display_mode,
        BMDDetectedVideoInputFormatFlags flags) override {

        LOG_INFO("Capture", "Video input format changed");
        // TODO: Handle format changes dynamically
        return S_OK;
    }

    virtual HRESULT VideoInputFrameArrived(
        IDeckLinkVideoInputFrame* video_frame,
        IDeckLinkAudioInputPacket* audio_packet) override {

        if (!video_frame) {
            return S_OK;
        }

        // Get hardware timestamp
        BMDTimeValue frame_time;
        BMDTimeValue frame_duration;
        BMDTimeScale time_scale;

        if (video_frame->GetStreamTime(&frame_time, &frame_duration, time_scale) == S_OK) {
            // Convert to nanoseconds
            int64_t pts_ns = (frame_time * 1000000000LL) / time_scale;
            m_capture->onFrameReceived(video_frame, pts_ns);
        } else {
            // Use system time if hardware timestamp unavailable
            auto now = std::chrono::steady_clock::now();
            int64_t pts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count();
            m_capture->onFrameReceived(video_frame, pts_ns);
        }

        return S_OK;
    }

private:
    DeckLinkCapture* m_capture;
    int32_t m_ref_count;
};

// Helper function to create DeckLink iterator
static IDeckLinkIterator* CreateDeckLinkIterator() {
    // Load the DeckLink library dynamically
    void* lib = dlopen("libDeckLinkAPI.so", RTLD_NOW);
    if (!lib) {
        LOG_ERROR("Capture", "Failed to load libDeckLinkAPI.so: %s", dlerror());
        return nullptr;
    }

    typedef IDeckLinkIterator* (*CreateIteratorFunc)();
    CreateIteratorFunc create_iterator = (CreateIteratorFunc)dlsym(lib, "CreateDeckLinkIteratorInstance");

    if (!create_iterator) {
        LOG_ERROR("Capture", "Failed to find CreateDeckLinkIteratorInstance");
        return nullptr;
    }

    return create_iterator();
}

DeckLinkCapture::DeckLinkCapture() {
    LOG_INFO("Capture", "DeckLinkCapture created");
}

DeckLinkCapture::~DeckLinkCapture() {
    stop();

    if (m_callback) {
        m_callback->Release();
        m_callback = nullptr;
    }

    if (m_decklink_input) {
        m_decklink_input->Release();
        m_decklink_input = nullptr;
    }

    if (m_decklink) {
        m_decklink->Release();
        m_decklink = nullptr;
    }

    LOG_INFO("Capture", "DeckLinkCapture destroyed");
}

Result DeckLinkCapture::initialize(int device_index) {
    CaptureConfig config;
    config.device_index = device_index;
    return initialize(config);
}

Result DeckLinkCapture::initialize(const CaptureConfig& config) {
    if (m_initialized) {
        LOG_WARN("Capture", "Already initialized");
        return Result::SUCCESS;
    }

    m_config = config;

    LOG_INFO("Capture", "Initializing DeckLink device %d", config.device_index);

    // Create DeckLink iterator
    IDeckLinkIterator* iterator = CreateDeckLinkIterator();
    if (!iterator) {
        LOG_ERROR("Capture", "Failed to create DeckLink iterator");
        return Result::ERROR_NOT_FOUND;
    }

    // Find the requested device
    IDeckLink* decklink = nullptr;
    int current_index = 0;

    while (iterator->Next(&decklink) == S_OK) {
        if (current_index == config.device_index) {
            m_decklink = decklink;
            break;
        }
        decklink->Release();
        current_index++;
    }

    iterator->Release();

    if (!m_decklink) {
        LOG_ERROR("Capture", "DeckLink device %d not found", config.device_index);
        return Result::ERROR_NOT_FOUND;
    }

    // Get device name
    const char* device_name = nullptr;
    if (m_decklink->GetDisplayName(&device_name) == S_OK) {
        LOG_INFO("Capture", "Found device: %s", device_name);
        free((void*)device_name);
    }

    // Get input interface
    if (m_decklink->QueryInterface(IID_IDeckLinkInput, (void**)&m_decklink_input) != S_OK) {
        LOG_ERROR("Capture", "Failed to get input interface");
        return Result::ERROR_GENERIC;
    }

    // Find display mode
    IDeckLinkDisplayModeIterator* mode_iterator = nullptr;
    if (m_decklink_input->GetDisplayModeIterator(&mode_iterator) != S_OK) {
        LOG_ERROR("Capture", "Failed to get display mode iterator");
        return Result::ERROR_GENERIC;
    }

    IDeckLinkDisplayMode* display_mode = nullptr;
    BMDDisplayMode selected_mode = bmdModeUnknown;

    while (mode_iterator->Next(&display_mode) == S_OK) {
        BMDTimeValue frame_duration;
        BMDTimeScale time_scale;
        display_mode->GetFrameRate(&frame_duration, &time_scale);

        double fps = (double)time_scale / frame_duration;
        long width = display_mode->GetWidth();
        long height = display_mode->GetHeight();

        // Match requested resolution and frame rate
        if (width == (long)config.width && height == (long)config.height &&
            std::abs(fps - config.frame_rate) < 0.1) {
            selected_mode = display_mode->GetDisplayMode();
            LOG_INFO("Capture", "Selected mode: %ldx%ld @ %.2f fps", width, height, fps);
            display_mode->Release();
            break;
        }

        display_mode->Release();
    }

    mode_iterator->Release();

    if (selected_mode == bmdModeUnknown) {
        LOG_WARN("Capture", "Exact mode not found, using automatic detection");
        selected_mode = bmdModeHD1080p60; // Fallback
    }

    // Choose pixel format (10-bit YUV or 8-bit)
    BMDPixelFormat pixel_format = config.enable_10bit ? bmdFormat10BitYUV : bmdFormat8BitYUV;

    // Enable video input
    BMDVideoInputFlags flags = bmdVideoInputEnableFormatDetection;

    if (m_decklink_input->EnableVideoInput(selected_mode, pixel_format, flags) != S_OK) {
        LOG_ERROR("Capture", "Failed to enable video input");
        return Result::ERROR_GENERIC;
    }

    // Create and set callback
    m_callback = new DeckLinkCaptureCallback(this);
    if (m_decklink_input->SetCallback(m_callback) != S_OK) {
        LOG_ERROR("Capture", "Failed to set input callback");
        return Result::ERROR_GENERIC;
    }

    m_initialized = true;
    LOG_INFO("Capture", "DeckLink initialization complete");

    return Result::SUCCESS;
}

Result DeckLinkCapture::start() {
    if (!m_initialized) {
        LOG_ERROR("Capture", "Not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    if (m_running) {
        LOG_WARN("Capture", "Already running");
        return Result::SUCCESS;
    }

    LOG_INFO("Capture", "Starting capture");

    if (m_decklink_input->StartStreams() != S_OK) {
        LOG_ERROR("Capture", "Failed to start streams");
        return Result::ERROR_GENERIC;
    }

    m_running = true;
    m_last_frame_time = std::chrono::steady_clock::now();

    LOG_INFO("Capture", "Capture started successfully");
    return Result::SUCCESS;
}

Result DeckLinkCapture::stop() {
    if (!m_running) {
        return Result::SUCCESS;
    }

    LOG_INFO("Capture", "Stopping capture");

    if (m_decklink_input) {
        m_decklink_input->StopStreams();
        m_decklink_input->DisableVideoInput();
    }

    m_running = false;

    // Clear frame queue
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    while (!m_frame_queue.empty()) {
        m_frame_queue.pop();
    }

    LOG_INFO("Capture", "Capture stopped");
    return Result::SUCCESS;
}

Result DeckLinkCapture::getFrame(VideoFrame& frame, int timeout_ms) {
    std::unique_lock<std::mutex> lock(m_queue_mutex);

    // Wait for frame with timeout
    if (m_frame_queue.empty()) {
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (!m_queue_cv.wait_for(lock, timeout, [this] { return !m_frame_queue.empty(); })) {
            return Result::ERROR_TIMEOUT;
        }
    }

    if (m_frame_queue.empty()) {
        return Result::ERROR_TIMEOUT;
    }

    // Get frame from queue
    QueuedFrame queued = std::move(m_frame_queue.front());
    m_frame_queue.pop();

    // Copy to output frame
    frame.data = queued.data.release();
    frame.size = queued.size;
    frame.width = queued.width;
    frame.height = queued.height;
    frame.format = queued.format;
    frame.pts = queued.pts;
    frame.hdr_metadata = queued.hdr_metadata;

    return Result::SUCCESS;
}

bool DeckLinkCapture::hasFrame() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return !m_frame_queue.empty();
}

DeckLinkCapture::Stats DeckLinkCapture::getStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    Stats stats = m_stats;

    std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
    stats.queue_size = m_frame_queue.size();

    return stats;
}

void DeckLinkCapture::onFrameReceived(IDeckLinkVideoInputFrame* video_frame, int64_t pts_ns) {
    if (!m_running) {
        return;
    }

    // Get frame properties
    long width = video_frame->GetWidth();
    long height = video_frame->GetHeight();
    long row_bytes = video_frame->GetRowBytes();
    BMDPixelFormat pixel_format = video_frame->GetPixelFormat();

    void* frame_data = nullptr;
    if (video_frame->GetBytes(&frame_data) != S_OK) {
        LOG_ERROR("Capture", "Failed to get frame bytes");
        return;
    }

    // Allocate frame buffer
    size_t frame_size = row_bytes * height;
    auto buffer = std::make_unique<uint8_t[]>(frame_size);
    std::memcpy(buffer.get(), frame_data, frame_size);

    // Determine pixel format
    PixelFormat ares_format = PixelFormat::UNKNOWN;
    if (pixel_format == bmdFormat10BitYUV) {
        ares_format = PixelFormat::YUV422_10BIT;
    } else if (pixel_format == bmdFormat8BitYUV) {
        ares_format = PixelFormat::YUV422_8BIT;
    }

    // Parse HDR metadata
    HDRMetadata hdr_metadata;
    parseHDRMetadata(video_frame, hdr_metadata);

    // Create queued frame
    QueuedFrame queued;
    queued.data = std::move(buffer);
    queued.size = frame_size;
    queued.width = width;
    queued.height = height;
    queued.format = ares_format;
    queued.pts = std::chrono::steady_clock::now(); // Will be converted from pts_ns in production
    queued.hdr_metadata = hdr_metadata;

    // Add to queue
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);

        // Drop oldest frame if queue is full
        if (m_frame_queue.size() >= MAX_QUEUE_SIZE) {
            m_frame_queue.pop();

            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            m_stats.frames_dropped++;
        }

        m_frame_queue.push(std::move(queued));
        m_queue_cv.notify_one();
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.frames_captured++;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_frame_time);
        if (elapsed.count() > 0) {
            m_stats.current_fps = 1000.0 / elapsed.count();
        }
        m_last_frame_time = now;
    }
}

void DeckLinkCapture::parseHDRMetadata(IDeckLinkVideoInputFrame* video_frame, HDRMetadata& metadata) {
    // Initialize to no HDR
    metadata.type = HDRType::NONE;

    // Query for HDR metadata interface
    IDeckLinkVideoFrameMetadataExtensions* metadata_ext = nullptr;
    if (video_frame->QueryInterface(IID_IDeckLinkVideoFrameMetadataExtensions,
                                     (void**)&metadata_ext) != S_OK) {
        return; // No HDR metadata available
    }

    // Check for HDR signaling
    int64_t colorspace = 0;
    if (metadata_ext->GetInt(bmdDeckLinkFrameMetadataColorspace, &colorspace) == S_OK) {
        if (colorspace == bmdColorspaceRec2020) {
            // Check transfer function
            int64_t gamma_curve = 0;
            if (metadata_ext->GetInt(bmdDeckLinkFrameMetadataGammaCurve, &gamma_curve) == S_OK) {
                if (gamma_curve == bmdGammaCurveST2084) {
                    metadata.type = HDRType::HDR10;
                } else if (gamma_curve == bmdGammaCurveHLG) {
                    metadata.type = HDRType::HLG;
                }
            }
        }
    }

    // Parse HDR10 static metadata (SMPTE ST 2086)
    if (metadata.type == HDRType::HDR10) {
        // Try to get HDR metadata
        // Note: DeckLink SDK may not expose all HDR metadata directly
        // This is a simplified implementation

        // Set default HDR10 values
        metadata.max_cll = 1000;  // Default max content light level
        metadata.max_fall = 400;  // Default max frame-average light level
        metadata.max_luminance = 1000; // cd/m²
        metadata.min_luminance = 50;   // cd/m² * 10000 (0.005 cd/m²)

        // Default BT.2020 primaries
        metadata.mastering_display.primary_r_x = 34000;
        metadata.mastering_display.primary_r_y = 16000;
        metadata.mastering_display.primary_g_x = 13250;
        metadata.mastering_display.primary_g_y = 34500;
        metadata.mastering_display.primary_b_x = 7500;
        metadata.mastering_display.primary_b_y = 3000;
        metadata.mastering_display.white_point_x = 15635;
        metadata.mastering_display.white_point_y = 16450;

        LOG_DEBUG("Capture", "HDR10 metadata detected: MaxCLL=%d, MaxFALL=%d",
                  metadata.max_cll, metadata.max_fall);
    }

    metadata_ext->Release();
}

} // namespace capture
} // namespace ares
