#pragma once

#include <ares/types.h>
#include <libevdev/libevdev.h>
#include <string>
#include <functional>
#include <map>

namespace ares {
namespace input {

// IR Remote button codes
enum class RemoteButton {
    // Navigation
    UP,
    DOWN,
    LEFT,
    RIGHT,
    OK,
    BACK,
    MENU,

    // Numbers
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4,
    NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,

    // Color buttons (common on remotes)
    RED,
    GREEN,
    YELLOW,
    BLUE,

    // Playback
    PLAY,
    PAUSE,
    STOP,
    REWIND,
    FORWARD,

    // Volume
    VOL_UP,
    VOL_DOWN,
    MUTE,

    // Power
    POWER,

    // Custom
    CUSTOM_1,
    CUSTOM_2,
    CUSTOM_3,
    CUSTOM_4,

    UNKNOWN
};

// Button event
struct ButtonEvent {
    RemoteButton button;
    bool pressed;           // true = pressed, false = released
    uint64_t timestamp_ns;  // Timestamp in nanoseconds
};

// IR Remote input handler
class IRRemote {
public:
    IRRemote();
    ~IRRemote();

    // Initialize IR remote input
    Result initialize(const std::string& device_path = "/dev/input/by-id/usb-*-event-ir");

    // Shutdown
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Poll for button events (non-blocking)
    Result pollEvents();

    // Set button callback
    using ButtonCallback = std::function<void(const ButtonEvent&)>;
    void setButtonCallback(ButtonCallback callback) { m_callback = callback; }

    // Get last button event
    ButtonEvent getLastEvent() const { return m_last_event; }

    // Statistics
    struct Stats {
        uint64_t events_received = 0;
        uint64_t events_processed = 0;
        uint64_t events_dropped = 0;
        RemoteButton last_button = RemoteButton::UNKNOWN;
    };

    Stats getStats() const { return m_stats; }

private:
    // Map Linux input event codes to RemoteButton
    RemoteButton mapEventCode(int code) const;

    // Handle input event
    void handleEvent(const struct input_event& ev);

    // Device
    int m_fd = -1;
    struct libevdev* m_dev = nullptr;
    std::string m_device_path;

    // Callback
    ButtonCallback m_callback;

    // State
    ButtonEvent m_last_event;
    Stats m_stats;
    bool m_initialized = false;

    // Button mapping
    std::map<int, RemoteButton> m_button_map;
};

} // namespace input
} // namespace ares
