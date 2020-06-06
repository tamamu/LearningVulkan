#include "vulkan.h"
#include "glfw.h"

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <optional>
#include <algorithm>
#include <vulkan/vulkan.hpp>

/// NOTE: PFN/pfn denotes that a type is a function pointer, or that a variable is of a pointer type.
PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT;

/// the belows should be placed in global scope, because occurs linker error if they are in vklearn namespace...
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger) {
    return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const *pAllocator) {
    return pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

namespace vklearn {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;

        SwapChainSupportDetails(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
            capabilities = device.getSurfaceCapabilitiesKHR(surface);
            formats = device.getSurfaceFormatsKHR(surface);
            presentModes = device.getSurfacePresentModesKHR(surface);
        }

        bool isComplete() {
            return !formats.empty() && !presentModes.empty();
        }
    };

    struct SwapChainDetails {
        vk::SurfaceFormatKHR format;
        vk::PresentModeKHR presentMode;
        vk::Extent2D extent;

        SwapChainDetails() {}

        SwapChainDetails(vk::SurfaceFormatKHR format,
            vk::PresentModeKHR presentMode,
            vk::Extent2D extent)
            : format(format), presentMode(presentMode), extent(extent) {}
    };

    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    std::vector<const char*> getRequiredExtensions() {
        /// returns required extensions for GLFW and validation layers when enabled.

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // NOTE: initialize with first iterator and last iterator.
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        /// returns whether the validation layer is available by the physical device or not.

        auto availableLayers = vk::enumerateInstanceLayerProperties();

        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            for (const auto& layer: availableLayers) {
                if (strcmp(layerName, layer.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    bool setDebugMessageFunc(vk::Instance instance) {
        /// associates the instance functions to create and destroy debug messenger with function pointers of library-side,
        /// then returns whether the association is successful or not.

        pfnVkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                instance.getProcAddr("vkCreateDebugUtilsMessengerEXT")
            );
        if (!pfnVkCreateDebugUtilsMessengerEXT) {
            std::cerr << "GetInstanceProcAddr: Unable to find pfnVkCreateDebugUtilsMessengerEXT function." << std::endl;
            return false;
        }

        pfnVkDestroyDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT")
            );
        if (!pfnVkDestroyDebugUtilsMessengerEXT) {
            std::cerr << "GetInstanceProcAddr: Unable to find pfnVkDestroyDebugUtilsMessengerEXT function." << std::endl;
            return false;
        }

        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
        // TODO: take required queue flags parameter
        QueueFamilyIndices indices;
        std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
        int idx = 0;
        for (const auto& queueFamily : queueFamilies) {
            /// check transferable data type (graphics, compute, transfer, sparse, protected)
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                indices.graphicsFamily = idx;
            }

            if (device.getSurfaceSupportKHR(idx, surface)) {
                indices.presentFamily = idx;
            }

            if (indices.isComplete()) {
                break;
            }

            idx++;
        }
        return indices;
    }

    bool checkDeviceExtensionSupport(vk::PhysicalDevice device, std::vector<const char*> deviceExtensions) {
        std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties(nullptr);
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(static_cast<const char*>(extension.extensionName));
        }

        return requiredExtensions.empty();
    }

    int rateDeviceSuitability(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
        // TODO: take rating function
        int score = 0;
        vk::PhysicalDeviceProperties props = device.getProperties();
        vk::PhysicalDeviceFeatures features = device.getFeatures();
        auto indices = findQueueFamilies(device, surface);

        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 1000;
        }

        score += props.limits.maxImageDimension2D;

        if (!features.geometryShader) {
            std::cerr << "Not supported: Geometry shader" << std::endl;
            return 0;
        }

        if (!indices.isComplete()) {
            std::cerr << "Not found: Required queue families" << std::endl;
            return 0;
        }

        if (!checkDeviceExtensionSupport(device, requiredDeviceExtensions)) {
            std::cerr << "Not supported: Required device extensions" << std::endl;
            for (auto extension : requiredDeviceExtensions) {
                std::cerr << "\t" << extension << std::endl;
            }
            return 0;
        }

        SwapChainSupportDetails swapChainSupport(device, surface);
        if (!swapChainSupport.isComplete()) {
            std::cerr << "Not supported: Swap chain" << std::endl;
            return 0;
        }

        return score;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        // B8G8R8A8_SRGB is most common one
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb
                && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        // MAILBOX is used to implement triple buffering,
        // which allows you to avoid tearing with significantly less latency
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const uint32_t width, const uint32_t height) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            vk::Extent2D actualExtent(width, height);
            actualExtent.width = std::clamp(
                actualExtent.width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            );
            actualExtent.height = std::clamp(
                actualExtent.height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            );
            return actualExtent;
        }
    }

    vk::ShaderModule createShaderModuleFromFile(vk::Device device, const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        vk::ShaderModuleCreateInfo createInfo(
            {},
            buffer.size(),
            reinterpret_cast<const uint32_t*>(buffer.data()));
        vk::ShaderModule shaderModule;
        if (device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    namespace boilerplate
    {
        vk::Instance instance(vk::ApplicationInfo* pAppInfo, const void *pNext) {
            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("validation layers requested, but not available!");
            }
            
            vk::Instance instance;
            vk::InstanceCreateInfo createInfo({}, pAppInfo);

            auto extensions = getRequiredExtensions();
            createInfo.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()));
            createInfo.setPpEnabledExtensionNames(extensions.data());

            if (enableValidationLayers) {
                createInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()));
                createInfo.setPpEnabledLayerNames(validationLayers.data());
                std::cout << "validation layers are enabled" << std::endl;
            } else {
                createInfo.setEnabledLayerCount(0);
            }
            createInfo.setPNext(pNext);

            vk::Result result = vk::createInstance(&createInfo, nullptr, &instance);

            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create instance");
            }
            return instance;
        }

        std::tuple<vk::SwapchainKHR, SwapChainDetails> SwapchainKHR(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface, uint32_t width, uint32_t height) {
            vklearn::SwapChainSupportDetails swapChainSupport(physicalDevice, surface);
            vk::SurfaceFormatKHR surfaceFormat = vklearn::chooseSwapSurfaceFormat(swapChainSupport.formats);
            vk::PresentModeKHR presentMode = vklearn::chooseSwapPresentMode(swapChainSupport.presentModes);
            vk::Extent2D extent = vklearn::chooseSwapExtent(swapChainSupport.capabilities, width, height);
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
            if (swapChainSupport.capabilities.maxImageCount > 0 &&
                imageCount > swapChainSupport.capabilities.maxImageCount) {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            vk::SwapchainCreateInfoKHR createInfo(
                {},
                surface,
                imageCount,
                surfaceFormat.format,
                surfaceFormat.colorSpace,
                extent,
                1, // the amount of layers is always 1 unless you are developing a stereoscopic 3D application
                vk::ImageUsageFlagBits::eColorAttachment);
            
            vklearn::QueueFamilyIndices indices = vklearn::findQueueFamilies(physicalDevice, surface);
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            if (indices.graphicsFamily != indices.presentFamily) {
                // CONCURRENT means images can be used across multiple queue families without explicit ownership transfers
                createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
                std::cerr << "Image Sharing Mode = CONCURRENT" << std::endl;
            } else {
                // EXCLUSIVE means an image is owned by one queue family at a time and ownership must be explicitly transfered before using it in another queue family
                createInfo.imageSharingMode = vk::SharingMode::eExclusive;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices = nullptr;
                std::cerr << "Image Sharing Mode = EXCLUSIVE" << std::endl;
            }
            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            // compositeAlpha specifies if the alpha channel should be used for blending with other windows in the window system
            // OPAQUE means ignore the alpha channel
            createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            createInfo.presentMode = presentMode;
            // you'll get the best performance by enabling clipping
            createInfo.clipped = VK_TRUE;
            // TODO: https://vulkan-tutorial.com/en/Drawing_a_triangle/Swap_chain_recreation
            createInfo.oldSwapchain = nullptr;
            vk::SwapchainKHR swapChain;
            if(device.createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swap chain!");
            }

            SwapChainDetails details(surfaceFormat, presentMode, extent);

            return std::make_tuple(swapChain, details);
        }

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT(PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback) {
            vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            );

            vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            );
            return vk::DebugUtilsMessengerCreateInfoEXT(
                {},
                severityFlags,
                messageTypeFlags,
                pfnUserCallback
            );
        }
    }

}