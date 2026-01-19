#pragma once

#include <ares/types.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace ares {
namespace input {

// EISCP (Ethernet-based protocol for Onkyo/Integra receivers)
// Based on the protocol reverse-engineered by various projects
class ReceiverControl {
public:
    ReceiverControl() = default;
    ~ReceiverControl();

    // Receiver information
    struct ReceiverInfo {
        std::string model;
        std::string ip_address;
        uint16_t port = 60128;  // Default EISCP port
        bool connected = false;
    };

    // Volume information
    struct VolumeInfo {
        int level = 0;           // Volume level (0-100 or receiver-specific range)
        int raw_level = 0;       // Raw receiver value (e.g., 0-80 for Integra)
        bool muted = false;
        int max_volume = 80;     // Maximum volume (receiver-specific)
        bool changed = false;    // Flag for recent change
        uint64_t last_change_ms = 0;  // Timestamp of last change
    };

    // Callbacks for volume changes
    using VolumeCallback = std::function<void(const VolumeInfo&)>;

    // Initialize connection to receiver
    Result initialize(const std::string& ip_address, uint16_t port = 60128);
    void shutdown();

    // Check if connected
    bool isConnected() const { return m_connected; }

    // Get receiver info
    ReceiverInfo getReceiverInfo() const { return m_receiver_info; }

    // Volume operations
    Result queryVolume();                    // Request current volume
    Result setVolume(int level);             // Set volume (0-100)
    Result setVolumeRaw(int raw_level);      // Set volume (receiver units)
    Result volumeUp();                       // Increase volume
    Result volumeDown();                     // Decrease volume
    Result toggleMute();                     // Toggle mute
    Result setMute(bool muted);              // Set mute state

    // Get current volume info
    VolumeInfo getVolumeInfo() const;

    // Register callback for volume changes
    void setVolumeCallback(VolumeCallback callback) { m_volume_callback = callback; }

    // Enable/disable automatic volume monitoring
    void setMonitoringEnabled(bool enabled);
    bool isMonitoringEnabled() const { return m_monitoring_enabled; }

private:
    // EISCP protocol implementation
    struct EISCPMessage {
        std::string command;     // e.g., "MVL" (Master Volume Level)
        std::string parameter;   // e.g., "50" or "UP"
    };

    // Build EISCP packet
    std::vector<uint8_t> buildEISCPPacket(const std::string& command,
                                          const std::string& parameter = "");

    // Parse EISCP response
    bool parseEISCPPacket(const std::vector<uint8_t>& packet, EISCPMessage& message);

    // Send command to receiver
    Result sendCommand(const std::string& command, const std::string& parameter = "");

    // Receive response from receiver
    Result receiveResponse(EISCPMessage& message, int timeout_ms = 1000);

    // Connection thread for monitoring updates
    void monitoringThread();

    // Parse volume response
    void parseVolumeResponse(const std::string& parameter);

    // Socket for TCP/IP connection
    int m_socket = -1;

    // Receiver info
    ReceiverInfo m_receiver_info;

    // Volume state
    VolumeInfo m_volume_info;
    mutable std::mutex m_volume_mutex;

    // Connection state
    std::atomic<bool> m_connected{false};

    // Monitoring thread
    std::thread m_monitor_thread;
    std::atomic<bool> m_monitoring_enabled{false};
    std::atomic<bool> m_monitor_running{false};

    // Callback
    VolumeCallback m_volume_callback;
};

} // namespace input
} // namespace ares
