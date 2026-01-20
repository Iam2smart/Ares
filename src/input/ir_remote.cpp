#include "ir_remote.h"
#include "core/logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <linux/input.h>
#include <chrono>

namespace ares {
namespace input {

IRRemote::IRRemote() {
    LOG_INFO("Input", "IRRemote created");

    // Initialize button mapping (common IR remote codes)
    // These map Linux input event codes (KEY_*) to RemoteButton enum
    m_button_map[KEY_UP] = RemoteButton::UP;
    m_button_map[KEY_DOWN] = RemoteButton::DOWN;
    m_button_map[KEY_LEFT] = RemoteButton::LEFT;
    m_button_map[KEY_RIGHT] = RemoteButton::RIGHT;
    m_button_map[KEY_OK] = RemoteButton::OK;
    m_button_map[KEY_ENTER] = RemoteButton::OK;
    m_button_map[KEY_BACK] = RemoteButton::BACK;
    m_button_map[KEY_ESC] = RemoteButton::BACK;
    m_button_map[KEY_MENU] = RemoteButton::MENU;

    // Alternative menu keys (for keyboards without KEY_MENU)
    m_button_map[KEY_F12] = RemoteButton::MENU;     // F12 to open OSD
    m_button_map[KEY_M] = RemoteButton::MENU;       // 'M' key to open OSD

    // Numbers
    m_button_map[KEY_0] = RemoteButton::NUM_0;
    m_button_map[KEY_1] = RemoteButton::NUM_1;
    m_button_map[KEY_2] = RemoteButton::NUM_2;
    m_button_map[KEY_3] = RemoteButton::NUM_3;
    m_button_map[KEY_4] = RemoteButton::NUM_4;
    m_button_map[KEY_5] = RemoteButton::NUM_5;
    m_button_map[KEY_6] = RemoteButton::NUM_6;
    m_button_map[KEY_7] = RemoteButton::NUM_7;
    m_button_map[KEY_8] = RemoteButton::NUM_8;
    m_button_map[KEY_9] = RemoteButton::NUM_9;

    // Color buttons
    m_button_map[KEY_RED] = RemoteButton::RED;
    m_button_map[KEY_GREEN] = RemoteButton::GREEN;
    m_button_map[KEY_YELLOW] = RemoteButton::YELLOW;
    m_button_map[KEY_BLUE] = RemoteButton::BLUE;

    // Playback
    m_button_map[KEY_PLAY] = RemoteButton::PLAY;
    m_button_map[KEY_PAUSE] = RemoteButton::PAUSE;
    m_button_map[KEY_STOP] = RemoteButton::STOP;
    m_button_map[KEY_REWIND] = RemoteButton::REWIND;
    m_button_map[KEY_FASTFORWARD] = RemoteButton::FORWARD;

    // Volume
    m_button_map[KEY_VOLUMEUP] = RemoteButton::VOL_UP;
    m_button_map[KEY_VOLUMEDOWN] = RemoteButton::VOL_DOWN;
    m_button_map[KEY_MUTE] = RemoteButton::MUTE;

    // Power
    m_button_map[KEY_POWER] = RemoteButton::POWER;
}

IRRemote::~IRRemote() {
    shutdown();
}

Result IRRemote::initialize(const std::string& device_path) {
    if (m_initialized) {
        LOG_WARN("Input", "IRRemote already initialized");
        return Result::SUCCESS;
    }

    LOG_INFO("Input", "Initializing IR remote input");

    // Find IR remote device
    glob_t glob_result;
    std::memset(&glob_result, 0, sizeof(glob_result));

    int ret = glob(device_path.c_str(), GLOB_TILDE, nullptr, &glob_result);
    if (ret != 0 || glob_result.gl_pathc == 0) {
        LOG_WARN("Input", "No IR remote device found at %s", device_path.c_str());
        LOG_INFO("Input", "IR remote will not be available");
        globfree(&glob_result);
        return Result::ERROR_NOT_FOUND;
    }

    // Try each matching device
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        const char* path = glob_result.gl_pathv[i];
        LOG_DEBUG("Input", "Trying IR device: %s", path);

        // Open device
        m_fd = open(path, O_RDONLY | O_NONBLOCK);
        if (m_fd < 0) {
            LOG_DEBUG("Input", "Failed to open %s: %s", path, strerror(errno));
            continue;
        }

        // Create libevdev device
        ret = libevdev_new_from_fd(m_fd, &m_dev);
        if (ret < 0) {
            LOG_DEBUG("Input", "Failed to create libevdev from %s", path);
            close(m_fd);
            m_fd = -1;
            continue;
        }

        // Check if this is an IR remote (should have KEY events)
        if (!libevdev_has_event_type(m_dev, EV_KEY)) {
            LOG_DEBUG("Input", "%s is not a KEY input device", path);
            libevdev_free(m_dev);
            m_dev = nullptr;
            close(m_fd);
            m_fd = -1;
            continue;
        }

        // Success!
        m_device_path = path;
        LOG_INFO("Input", "IR remote initialized: %s", path);
        LOG_INFO("Input", "Device: %s", libevdev_get_name(m_dev));
        break;
    }

    globfree(&glob_result);

    if (m_fd < 0) {
        LOG_ERROR("Input", "Failed to find valid IR remote device");
        return Result::ERROR_NOT_FOUND;
    }

    m_initialized = true;
    return Result::SUCCESS;
}

void IRRemote::shutdown() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Input", "Shutting down IR remote");

    if (m_dev) {
        libevdev_free(m_dev);
        m_dev = nullptr;
    }

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    m_initialized = false;
}

Result IRRemote::pollEvents() {
    if (!m_initialized) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    struct input_event ev;
    int ret;

    while ((ret = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) >= 0) {
        if (ret == LIBEVDEV_READ_STATUS_SUCCESS) {
            handleEvent(ev);
            m_stats.events_received++;
        } else if (ret == LIBEVDEV_READ_STATUS_SYNC) {
            // Device was out of sync, resync
            LOG_WARN("Input", "IR remote out of sync, resyncing");
            while (libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_SYNC, &ev) >= 0) {
                handleEvent(ev);
            }
        }
    }

    if (ret < 0 && ret != -EAGAIN) {
        LOG_ERROR("Input", "Error reading IR remote events: %s", strerror(-ret));
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

void IRRemote::handleEvent(const struct input_event& ev) {
    // Only handle key events
    if (ev.type != EV_KEY) {
        return;
    }

    // Map event code to button
    RemoteButton button = mapEventCode(ev.code);
    if (button == RemoteButton::UNKNOWN) {
        m_stats.events_dropped++;
        return;
    }

    // Create button event
    ButtonEvent event;
    event.button = button;
    event.pressed = (ev.value == 1);  // 1 = pressed, 0 = released, 2 = repeat
    event.timestamp_ns = (ev.time.tv_sec * 1000000000ULL) + (ev.time.tv_usec * 1000ULL);

    // Update state
    m_last_event = event;
    m_stats.last_button = button;
    m_stats.events_processed++;

    // Call callback
    if (m_callback) {
        m_callback(event);
    }

    LOG_DEBUG("Input", "Button %d %s", static_cast<int>(button),
             event.pressed ? "pressed" : "released");
}

RemoteButton IRRemote::mapEventCode(int code) const {
    auto it = m_button_map.find(code);
    if (it != m_button_map.end()) {
        return it->second;
    }
    return RemoteButton::UNKNOWN;
}

} // namespace input
} // namespace ares
