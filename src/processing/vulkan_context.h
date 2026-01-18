#pragma once

#include <ares/types.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace ares {
namespace processing {

// Vulkan context for headless GPU processing
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    // Initialize Vulkan for headless rendering
    Result initialize(bool enable_validation = false);

    // Cleanup resources
    void cleanup();

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Get Vulkan objects
    VkInstance getInstance() const { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physical_device; }
    VkDevice getDevice() const { return m_device; }
    VkQueue getGraphicsQueue() const { return m_graphics_queue; }
    VkQueue getComputeQueue() const { return m_compute_queue; }
    VkQueue getTransferQueue() const { return m_transfer_queue; }

    uint32_t getGraphicsQueueFamily() const { return m_graphics_queue_family; }
    uint32_t getComputeQueueFamily() const { return m_compute_queue_family; }
    uint32_t getTransferQueueFamily() const { return m_transfer_queue_family; }

    // Device properties
    const VkPhysicalDeviceProperties& getDeviceProperties() const { return m_device_properties; }
    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return m_memory_properties; }

    // Check for extension support
    bool hasExtension(const std::string& extension) const;

    // Memory allocation helpers
    uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    // Command pool management
    VkCommandPool createCommandPool(uint32_t queue_family, VkCommandPoolCreateFlags flags = 0);
    void destroyCommandPool(VkCommandPool pool);

    // Single-time command helpers
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
    void endSingleTimeCommands(VkCommandBuffer command_buffer, VkCommandPool pool, VkQueue queue);

    // Statistics
    struct Stats {
        std::string device_name;
        std::string driver_version;
        uint32_t api_version;
        uint64_t total_memory_mb;
        uint64_t available_memory_mb;
        bool has_compute_queue;
        bool has_dedicated_transfer_queue;
    };

    Stats getStats() const;

private:
    // Initialization helpers
    Result createInstance(bool enable_validation);
    Result selectPhysicalDevice();
    Result createLogicalDevice();
    Result findQueueFamilies();

    // Validation layers
    bool checkValidationLayerSupport();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);

    // Vulkan objects
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    // Queues
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;
    VkQueue m_transfer_queue = VK_NULL_HANDLE;

    uint32_t m_graphics_queue_family = UINT32_MAX;
    uint32_t m_compute_queue_family = UINT32_MAX;
    uint32_t m_transfer_queue_family = UINT32_MAX;

    // Device properties
    VkPhysicalDeviceProperties m_device_properties = {};
    VkPhysicalDeviceMemoryProperties m_memory_properties = {};
    VkPhysicalDeviceFeatures m_device_features = {};

    // Extensions
    std::vector<std::string> m_enabled_extensions;

    // State
    bool m_initialized = false;
    bool m_validation_enabled = false;
};

} // namespace processing
} // namespace ares
