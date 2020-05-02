#include "vulkan.h"
#include "glfw.h"

#include <stdexcept>
#include <iostream>
#include <vector>

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

    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
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