#include "receiver_control.h"
#include "core/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

namespace ares {
namespace input {

ReceiverControl::~ReceiverControl() {
    shutdown();
}

Result ReceiverControl::initialize(const std::string& ip_address, uint16_t port) {
    if (m_connected) {
        LOG_WARN("Receiver", "Already connected, shutting down first");
        shutdown();
    }

    LOG_INFO("Receiver", "Connecting to receiver at %s:%d", ip_address.c_str(), port);

    // Create TCP socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        LOG_ERROR("Receiver", "Failed to create socket: %s", strerror(errno));
        return Result::ERROR_INITIALIZATION_FAILED;
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Connect to receiver
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address.c_str(), &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Receiver", "Invalid IP address: %s", ip_address.c_str());
        close(m_socket);
        m_socket = -1;
        return Result::ERROR_INVALID_PARAMETER;
    }

    if (connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Receiver", "Failed to connect to receiver: %s", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return Result::ERROR_CONNECTION_FAILED;
    }

    m_connected = true;
    m_receiver_info.ip_address = ip_address;
    m_receiver_info.port = port;
    m_receiver_info.connected = true;

    LOG_INFO("Receiver", "Connected to receiver successfully");

    // Query initial volume
    queryVolume();

    // Start monitoring thread if enabled
    if (m_monitoring_enabled) {
        setMonitoringEnabled(true);
    }

    return Result::SUCCESS;
}

void ReceiverControl::shutdown() {
    // Stop monitoring thread
    if (m_monitor_running) {
        m_monitor_running = false;
        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }
    }

    // Close socket
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    m_connected = false;
    m_receiver_info.connected = false;

    LOG_INFO("Receiver", "Disconnected from receiver");
}

std::vector<uint8_t> ReceiverControl::buildEISCPPacket(const std::string& command,
                                                        const std::string& parameter) {
    // EISCP packet format:
    // Header: "ISCP" (4 bytes)
    // Header Size: 16 (4 bytes, big-endian)
    // Data Size: variable (4 bytes, big-endian)
    // Version: 0x01 (1 byte)
    // Reserved: 0x00 0x00 0x00 (3 bytes)
    // Data: "!1" + command + parameter + "\r\n" (ASCII)

    std::string data = "!1" + command + parameter + "\r\n";
    uint32_t data_size = data.size();
    uint32_t header_size = 16;

    std::vector<uint8_t> packet;
    packet.reserve(header_size + data_size);

    // Header: "ISCP"
    packet.push_back('I');
    packet.push_back('S');
    packet.push_back('C');
    packet.push_back('P');

    // Header size (16, big-endian)
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x10);

    // Data size (big-endian)
    packet.push_back((data_size >> 24) & 0xFF);
    packet.push_back((data_size >> 16) & 0xFF);
    packet.push_back((data_size >> 8) & 0xFF);
    packet.push_back(data_size & 0xFF);

    // Version
    packet.push_back(0x01);

    // Reserved
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);

    // Data
    packet.insert(packet.end(), data.begin(), data.end());

    return packet;
}

bool ReceiverControl::parseEISCPPacket(const std::vector<uint8_t>& packet,
                                       EISCPMessage& message) {
    // Minimum packet size: 16 (header) + 5 ("!1XXX\r\n")
    if (packet.size() < 21) {
        LOG_ERROR("Receiver", "Packet too small: %zu bytes", packet.size());
        return false;
    }

    // Verify header
    if (packet[0] != 'I' || packet[1] != 'S' || packet[2] != 'C' || packet[3] != 'P') {
        LOG_ERROR("Receiver", "Invalid ISCP header");
        return false;
    }

    // Get data size
    uint32_t data_size = (packet[8] << 24) | (packet[9] << 16) |
                         (packet[10] << 8) | packet[11];

    // Verify packet size
    if (packet.size() < 16 + data_size) {
        LOG_ERROR("Receiver", "Incomplete packet: expected %u bytes, got %zu",
                 16 + data_size, packet.size());
        return false;
    }

    // Extract data (skip "!1" prefix and "\r\n" suffix)
    std::string data(packet.begin() + 18, packet.begin() + 16 + data_size - 2);

    // Parse command (first 3 characters)
    if (data.size() < 3) {
        LOG_ERROR("Receiver", "Data too short: %zu bytes", data.size());
        return false;
    }

    message.command = data.substr(0, 3);
    message.parameter = data.substr(3);

    return true;
}

Result ReceiverControl::sendCommand(const std::string& command,
                                    const std::string& parameter) {
    if (!m_connected || m_socket < 0) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    auto packet = buildEISCPPacket(command, parameter);

    ssize_t sent = send(m_socket, packet.data(), packet.size(), 0);
    if (sent < 0) {
        LOG_ERROR("Receiver", "Failed to send command: %s", strerror(errno));
        return Result::ERROR_CONNECTION_FAILED;
    }

    if ((size_t)sent != packet.size()) {
        LOG_WARN("Receiver", "Partial send: %zd/%zu bytes", sent, packet.size());
    }

    return Result::SUCCESS;
}

Result ReceiverControl::receiveResponse(EISCPMessage& message, int timeout_ms) {
    if (!m_connected || m_socket < 0) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Receive header first (16 bytes)
    std::vector<uint8_t> packet(16);
    ssize_t received = recv(m_socket, packet.data(), 16, 0);

    if (received <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return Result::ERROR_TIMEOUT;
        }
        LOG_ERROR("Receiver", "Failed to receive header: %s", strerror(errno));
        return Result::ERROR_CONNECTION_FAILED;
    }

    if (received < 16) {
        LOG_ERROR("Receiver", "Incomplete header: %zd bytes", received);
        return Result::ERROR_CONNECTION_FAILED;
    }

    // Get data size
    uint32_t data_size = (packet[8] << 24) | (packet[9] << 16) |
                         (packet[10] << 8) | packet[11];

    // Receive data
    packet.resize(16 + data_size);
    received = recv(m_socket, packet.data() + 16, data_size, 0);

    if (received < (ssize_t)data_size) {
        LOG_ERROR("Receiver", "Incomplete data: %zd/%u bytes", received, data_size);
        return Result::ERROR_CONNECTION_FAILED;
    }

    // Parse packet
    if (!parseEISCPPacket(packet, message)) {
        return Result::ERROR_INVALID_DATA;
    }

    return Result::SUCCESS;
}

Result ReceiverControl::queryVolume() {
    // MVL = Master Volume Level
    // QSTN = Query
    Result result = sendCommand("MVL", "QSTN");
    if (result != Result::SUCCESS) {
        return result;
    }

    // Wait for response
    EISCPMessage response;
    result = receiveResponse(response, 1000);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Parse response
    if (response.command == "MVL") {
        parseVolumeResponse(response.parameter);
    }

    return Result::SUCCESS;
}

Result ReceiverControl::setVolume(int level) {
    // Convert 0-100 to receiver range (typically 0-80 for Integra)
    int raw_level = (level * m_volume_info.max_volume) / 100;
    return setVolumeRaw(raw_level);
}

Result ReceiverControl::setVolumeRaw(int raw_level) {
    // Clamp to valid range
    if (raw_level < 0) raw_level = 0;
    if (raw_level > m_volume_info.max_volume) raw_level = m_volume_info.max_volume;

    // Format as hex (e.g., "50" becomes "32" in hex)
    char param[3];
    snprintf(param, sizeof(param), "%02X", raw_level);

    return sendCommand("MVL", param);
}

Result ReceiverControl::volumeUp() {
    return sendCommand("MVL", "UP");
}

Result ReceiverControl::volumeDown() {
    return sendCommand("MVL", "DOWN");
}

Result ReceiverControl::toggleMute() {
    return sendCommand("AMT", "TG");
}

Result ReceiverControl::setMute(bool muted) {
    return sendCommand("AMT", muted ? "01" : "00");
}

ReceiverControl::VolumeInfo ReceiverControl::getVolumeInfo() const {
    std::lock_guard<std::mutex> lock(m_volume_mutex);
    return m_volume_info;
}

void ReceiverControl::setMaxVolume(int max_volume) {
    std::lock_guard<std::mutex> lock(m_volume_mutex);
    m_volume_info.max_volume = max_volume;
    // Recalculate level based on new max
    if (m_volume_info.raw_level > 0) {
        m_volume_info.level = (m_volume_info.raw_level * 100) / max_volume;
    }
    LOG_INFO("Receiver", "Max volume set to %d (for 0-100 scaling)", max_volume);
}

void ReceiverControl::setMonitoringEnabled(bool enabled) {
    if (enabled == m_monitoring_enabled) {
        return;
    }

    m_monitoring_enabled = enabled;

    if (enabled && m_connected) {
        // Start monitoring thread
        m_monitor_running = true;
        m_monitor_thread = std::thread(&ReceiverControl::monitoringThread, this);
        LOG_INFO("Receiver", "Volume monitoring enabled");
    } else if (!enabled && m_monitor_running) {
        // Stop monitoring thread
        m_monitor_running = false;
        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }
        LOG_INFO("Receiver", "Volume monitoring disabled");
    }
}

void ReceiverControl::monitoringThread() {
    LOG_INFO("Receiver", "Volume monitoring thread started");

    while (m_monitor_running && m_connected) {
        EISCPMessage message;
        Result result = receiveResponse(message, 500);

        if (result == Result::SUCCESS) {
            // Handle volume updates
            if (message.command == "MVL") {
                parseVolumeResponse(message.parameter);
            } else if (message.command == "AMT") {
                // Mute status: "00" = unmuted, "01" = muted
                std::lock_guard<std::mutex> lock(m_volume_mutex);
                m_volume_info.muted = (message.parameter == "01");
                m_volume_info.changed = true;
                m_volume_info.last_change_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();

                if (m_volume_callback) {
                    m_volume_callback(m_volume_info);
                }
            }
        } else if (result != Result::ERROR_TIMEOUT) {
            LOG_WARN("Receiver", "Error receiving update: %d", static_cast<int>(result));
        }

        // Query volume periodically (every 5 seconds)
        static auto last_query = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_query).count() >= 5) {
            queryVolume();
            last_query = now;
        }
    }

    LOG_INFO("Receiver", "Volume monitoring thread stopped");
}

void ReceiverControl::parseVolumeResponse(const std::string& parameter) {
    // Parameter is hex value (e.g., "32" = 50 decimal)
    // Or "N/A" if not available
    if (parameter == "N/A" || parameter.empty()) {
        return;
    }

    try {
        int raw_level = std::stoi(parameter, nullptr, 16);
        int level = (raw_level * 100) / m_volume_info.max_volume;

        std::lock_guard<std::mutex> lock(m_volume_mutex);

        // Check if volume changed
        bool changed = (m_volume_info.raw_level != raw_level);

        m_volume_info.raw_level = raw_level;
        m_volume_info.level = level;
        m_volume_info.changed = changed;

        if (changed) {
            m_volume_info.last_change_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            LOG_DEBUG("Receiver", "Volume changed: %d (raw: %d)", level, raw_level);

            // Trigger callback
            if (m_volume_callback) {
                m_volume_callback(m_volume_info);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Receiver", "Failed to parse volume parameter '%s': %s",
                 parameter.c_str(), e.what());
    }
}

} // namespace input
} // namespace ares
