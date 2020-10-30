#include "vulkan.h"
#include "glfw.h"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <optional>
#include <algorithm>
#include <tuple>
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

        if (!features.samplerAnisotropy) {
            std::cerr << "Not supported: Sampler Anisotropy" << std::endl;
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
        std::cout << "Available present modes:" << std::endl;
        for (const auto& availablePresentMode : availablePresentModes) {
            std::cout << "\t" << to_string(availablePresentMode) << std::endl;
        }
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                std::cout << "Use VK_PRESENT_MODE_MAILBOX_KHR" << std::endl;
                return availablePresentMode;
            }
        }
        std::cout << "Use VK_PRESENT_MODE_FIFO_KHR" << std::endl;
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            std::cout << width << "," << height << std::endl;
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

    vk::ShaderModule createShaderModuleFromFile(vk::Device device, const std::string filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filename);
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

    uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i=0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    vk::CommandBuffer beginSingleTimeCommands(vk::Device device, vk::CommandPool commandPool) {
        vk::CommandBufferAllocateInfo allocInfo(
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            1
        );

        vk::CommandBuffer commandBuffer;
        device.allocateCommandBuffers(&allocInfo, &commandBuffer);

        vk::CommandBufferBeginInfo beginInfo(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        );

        commandBuffer.begin(&beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(vk::Device device, vk::CommandPool commandPool, vk::CommandBuffer commandBuffer, vk::Queue queue) {
        commandBuffer.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        queue.submit(1, &submitInfo, nullptr);
        queue.waitIdle();

        device.freeCommandBuffers(commandPool, 1, &commandBuffer);
    }

    bool hasStencilComponent(vk::Format format) {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    void transitionImageLayout(vk::Device device, vk::CommandPool commandPool, vk::Queue graphicsQueue, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        vk::ImageMemoryBarrier barrier{};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = 0;
        barrier.dstQueueFamilyIndex = 0;
        barrier.image = image;
        barrier.subresourceRange = vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,  // baseMipLevel
            1,  // levelCount
            0,  // baseArrayLayer
            1   // layerCount
        );
        if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        } else {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = {};

        vk::PipelineStageFlags sourceStage, destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        commandBuffer.pipelineBarrier(
            sourceStage, destinationStage,
            {},
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
    }

    void copyBufferToImage(vk::Device device, vk::CommandPool commandPool, vk::Queue graphicsQueue, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset.x = 0;
        region.imageOffset.y = 0;
        region.imageOffset.z = 0;

        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;

        commandBuffer.copyBufferToImage(
            buffer,
            image,
            vk::ImageLayout::eTransferDstOptimal,
            1,
            &region
        );

        endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
    }

    std::tuple<vk::Buffer, vk::DeviceMemory> createBuffer(vk::PhysicalDevice physicalDevice, vk::Device device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo
            .setSize(size)
            .setUsage(usage)
            .setSharingMode(vk::SharingMode::eExclusive);
        vk::Buffer buffer;
        vk::DeviceMemory bufferMemory;

        if (device.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create buffer");
        }

        vk::MemoryRequirements memRequirements;
        device.getBufferMemoryRequirements(buffer, &memRequirements);

        vk::MemoryAllocateInfo allocInfo(memRequirements.size, findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));

        if (device.allocateMemory(&allocInfo, nullptr, &bufferMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        device.bindBufferMemory(buffer, bufferMemory, 0);

        return {buffer, bufferMemory};
    }

    vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
        for (vk::Format format : candidates) {
            vk::FormatProperties props = physicalDevice.getFormatProperties(format);
            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
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

        std::tuple<vk::SwapchainKHR, SwapChainDetails> SwapchainKHR(vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface, GLFWwindow* window, vk::SwapchainKHR* oldSwapchain) {
            vklearn::SwapChainSupportDetails swapChainSupport(physicalDevice, surface);
            vk::SurfaceFormatKHR surfaceFormat = vklearn::chooseSwapSurfaceFormat(swapChainSupport.formats);
            vk::PresentModeKHR presentMode = vklearn::chooseSwapPresentMode(swapChainSupport.presentModes);
            vk::Extent2D extent = vklearn::chooseSwapExtent(swapChainSupport.capabilities,window);
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

            if (oldSwapchain != nullptr) {
                createInfo.oldSwapchain = *oldSwapchain;
            }
            
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
            createInfo.oldSwapchain = nullptr;
            vk::SwapchainKHR swapChain;
            if(device.createSwapchainKHR(&createInfo, nullptr, &swapChain) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swap chain!");
            }

            SwapChainDetails details(surfaceFormat, presentMode, extent);

            return std::make_tuple(swapChain, details);
        }

        vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image = image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format = format;
            viewInfo.subresourceRange = vk::ImageSubresourceRange(
                    aspectFlags,
                    0,
                    1,
                    0,
                    1);
            
            vk::ImageView imageView;
            if (device.createImageView(&viewInfo, nullptr, &imageView) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create texture image view!");
            }

            return imageView;
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