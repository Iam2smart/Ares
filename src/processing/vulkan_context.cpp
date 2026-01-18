#include "vulkan_context.h"
#include "core/logger.h"

#include <cstring>
#include <vector>
#include <set>

namespace ares {
namespace processing {

// Validation layers to enable in debug builds
static const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

VulkanContext::VulkanContext() {
    LOG_INFO("Processing", "VulkanContext created");
}

VulkanContext::~VulkanContext() {
    cleanup();
}

Result VulkanContext::initialize(bool enable_validation) {
    if (m_initialized) {
        LOG_WARN("Processing", "VulkanContext already initialized");
        return Result::SUCCESS;
    }

    m_validation_enabled = enable_validation;

    LOG_INFO("Processing", "Initializing Vulkan context (validation: %s)",
             enable_validation ? "enabled" : "disabled");

    // Create Vulkan instance
    Result result = createInstance(enable_validation);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to create Vulkan instance");
        return result;
    }

    // Select physical device
    result = selectPhysicalDevice();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to select physical device");
        cleanup();
        return result;
    }

    // Find queue families
    result = findQueueFamilies();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to find suitable queue families");
        cleanup();
        return result;
    }

    // Create logical device
    result = createLogicalDevice();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to create logical device");
        cleanup();
        return result;
    }

    m_initialized = true;

    // Print device info
    LOG_INFO("Processing", "Vulkan initialized successfully");
    LOG_INFO("Processing", "Device: %s", m_device_properties.deviceName);
    LOG_INFO("Processing", "API Version: %d.%d.%d",
             VK_VERSION_MAJOR(m_device_properties.apiVersion),
             VK_VERSION_MINOR(m_device_properties.apiVersion),
             VK_VERSION_PATCH(m_device_properties.apiVersion));

    return Result::SUCCESS;
}

void VulkanContext::cleanup() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Processing", "Cleaning up Vulkan context");

    // Wait for device to be idle
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    // Destroy logical device
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (m_debug_messenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_instance, m_debug_messenger, nullptr);
        }
        m_debug_messenger = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_initialized = false;
    LOG_INFO("Processing", "Vulkan cleanup complete");
}

Result VulkanContext::createInstance(bool enable_validation) {
    // Check validation layer support
    if (enable_validation && !checkValidationLayerSupport()) {
        LOG_WARN("Processing", "Validation layers requested but not available");
        enable_validation = false;
    }

    // Application info
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Ares HDR Video Processor";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Ares";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    // Instance create info
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // No extensions needed for headless rendering
    create_info.enabledExtensionCount = 0;

    // Enable validation layers if requested
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    if (enable_validation) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();

        // Setup debug messenger
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debugCallback;

        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;
        create_info.pNext = nullptr;
    }

    // Create instance
    VkResult vk_result = vkCreateInstance(&create_info, nullptr, &m_instance);
    if (vk_result != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create Vulkan instance: %d", vk_result);
        return Result::ERROR_GENERIC;
    }

    // Create debug messenger
    if (enable_validation) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_instance, &debug_create_info, nullptr, &m_debug_messenger);
        }
    }

    return Result::SUCCESS;
}

Result VulkanContext::selectPhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    if (device_count == 0) {
        LOG_ERROR("Processing", "No Vulkan-capable GPUs found");
        return Result::ERROR_NOT_FOUND;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    // Score devices and pick the best one
    int best_score = -1;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);

        int score = 0;

        // Prefer discrete GPUs
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Add score based on max image dimension
        score += properties.limits.maxImageDimension2D;

        LOG_DEBUG("Processing", "Found GPU: %s (score: %d)", properties.deviceName, score);

        if (score > best_score) {
            best_score = score;
            best_device = device;
        }
    }

    if (best_device == VK_NULL_HANDLE) {
        LOG_ERROR("Processing", "No suitable GPU found");
        return Result::ERROR_NOT_FOUND;
    }

    m_physical_device = best_device;

    // Get device properties
    vkGetPhysicalDeviceProperties(m_physical_device, &m_device_properties);
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
    vkGetPhysicalDeviceFeatures(m_physical_device, &m_device_features);

    return Result::SUCCESS;
}

Result VulkanContext::findQueueFamilies() {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count,
                                            queue_families.data());

    // Find graphics, compute, and transfer queues
    bool found_graphics = false;
    bool found_compute = false;
    bool found_transfer = false;

    for (uint32_t i = 0; i < queue_family_count; i++) {
        const auto& family = queue_families[i];

        // Graphics queue
        if (!found_graphics && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            m_graphics_queue_family = i;
            found_graphics = true;
            LOG_DEBUG("Processing", "Graphics queue family: %d", i);
        }

        // Compute queue (prefer dedicated)
        if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!found_compute || !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                m_compute_queue_family = i;
                found_compute = true;
                LOG_DEBUG("Processing", "Compute queue family: %d", i);
            }
        }

        // Transfer queue (prefer dedicated)
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!found_transfer ||
                !(family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
                m_transfer_queue_family = i;
                found_transfer = true;
                LOG_DEBUG("Processing", "Transfer queue family: %d", i);
            }
        }
    }

    if (!found_graphics) {
        LOG_ERROR("Processing", "No graphics queue family found");
        return Result::ERROR_NOT_FOUND;
    }

    // Use graphics queue for compute/transfer if dedicated queues not found
    if (!found_compute) {
        m_compute_queue_family = m_graphics_queue_family;
    }
    if (!found_transfer) {
        m_transfer_queue_family = m_graphics_queue_family;
    }

    return Result::SUCCESS;
}

Result VulkanContext::createLogicalDevice() {
    // Collect unique queue families
    std::set<uint32_t> unique_queue_families = {
        m_graphics_queue_family,
        m_compute_queue_family,
        m_transfer_queue_family
    };

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Device features
    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.shaderInt64 = VK_TRUE;

    // Device create info
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 0;

    // Enable validation layers for device (deprecated but still supported)
    if (m_validation_enabled) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    // Create logical device
    VkResult result = vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create logical device: %d", result);
        return Result::ERROR_GENERIC;
    }

    // Get queue handles
    vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_compute_queue_family, 0, &m_compute_queue);
    vkGetDeviceQueue(m_device, m_transfer_queue_family, 0, &m_transfer_queue);

    return Result::SUCCESS;
}

bool VulkanContext::checkValidationLayerSupport() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : validation_layers) {
        bool found = false;

        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("Vulkan", "%s", callback_data->pMessage);
    } else {
        LOG_DEBUG("Vulkan", "%s", callback_data->pMessage);
    }

    return VK_FALSE;
}

bool VulkanContext::hasExtension(const std::string& extension) const {
    for (const auto& ext : m_enabled_extensions) {
        if (ext == extension) {
            return true;
        }
    }
    return false;
}

uint32_t VulkanContext::findMemoryType(uint32_t type_filter,
                                       VkMemoryPropertyFlags properties) const {
    for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (m_memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR("Processing", "Failed to find suitable memory type");
    return UINT32_MAX;
}

VkCommandPool VulkanContext::createCommandPool(uint32_t queue_family,
                                               VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    pool_info.flags = flags;

    VkCommandPool command_pool;
    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create command pool");
        return VK_NULL_HANDLE;
    }

    return command_pool;
}

void VulkanContext::destroyCommandPool(VkCommandPool pool) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, pool, nullptr);
    }
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands(VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer command_buffer,
                                         VkCommandPool pool,
                                         VkQueue queue) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(m_device, pool, 1, &command_buffer);
}

VulkanContext::Stats VulkanContext::getStats() const {
    Stats stats;

    stats.device_name = m_device_properties.deviceName;

    // Driver version
    uint32_t driver_version = m_device_properties.driverVersion;
    char driver_str[64];
    snprintf(driver_str, sizeof(driver_str), "%d.%d.%d",
             VK_VERSION_MAJOR(driver_version),
             VK_VERSION_MINOR(driver_version),
             VK_VERSION_PATCH(driver_version));
    stats.driver_version = driver_str;

    stats.api_version = m_device_properties.apiVersion;

    // Calculate total memory
    stats.total_memory_mb = 0;
    for (uint32_t i = 0; i < m_memory_properties.memoryHeapCount; i++) {
        if (m_memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            stats.total_memory_mb += m_memory_properties.memoryHeaps[i].size / (1024 * 1024);
        }
    }

    // Estimate available memory (simplified)
    stats.available_memory_mb = stats.total_memory_mb;

    stats.has_compute_queue = (m_compute_queue_family != UINT32_MAX);
    stats.has_dedicated_transfer_queue = (m_transfer_queue_family != m_graphics_queue_family);

    return stats;
}

} // namespace processing
} // namespace ares
