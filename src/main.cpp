#include "vulkan.h"
#include "glfw.h"
#include "vk.hpp"
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>

class VulkanApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
private:
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::SurfaceKHR surface;
    vk::DebugUtilsMessengerEXT debugMessenger;
    GLFWwindow* window;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "VulkanApp", nullptr, nullptr);
    }

    void initVulkan() {
        vk::ApplicationInfo appInfo("VulkanApp", 1, "LearningVulkan", 1, VK_API_VERSION_1_2);

        if (vklearn::enableValidationLayers) {
            auto debugCreateInfo = vklearn::boilerplate::debugUtilsMessengerCreateInfoEXT(&vklearn::debugCallback);
            instance = vklearn::boilerplate::instance(&appInfo, reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo));
            std::cout << "debug messenger for instance is enabled" << std::endl;
        } else {
            instance = vklearn::boilerplate::instance(&appInfo, nullptr);
        }

        std::cout << "created instance successfully" << std::endl;

        setupDebugMessenger();
        std::cout << "debug messenger have been set up successfully" << std::endl;
    
        showInstanceInfo();

        createSurface();

        pickPhysicalDevice();

        createLogicalDevice();
    }

    void showInstanceInfo() {
        std::vector<vk::LayerProperties> layerProperties = vk::enumerateInstanceLayerProperties();
        std::cout << layerProperties.size() << " layers supported:\n";
        for (const auto& layer : layerProperties) {
            std::cout << '\t' << layer.layerName << std::endl;
        }

        std::vector<vk::ExtensionProperties> extProperties = vk::enumerateInstanceExtensionProperties();
        std::cout << extProperties.size() << " available extensions:\n";
        for (const auto& ext : extProperties) {
            std::cout << '\t' << ext.extensionName << std::endl;
        }
    }

    void createSurface() {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::SurfaceKHR(_surface);
    }

    void pickPhysicalDevice() {
        std::vector<vk::PhysicalDevice> pDevices = instance.enumeratePhysicalDevices();
        std::multimap<int, VkPhysicalDevice> candidates;

        if (pDevices.empty()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::cout << pDevices.size() << " devices found:\n";
        for (const auto& dev : pDevices) {
            std::cout << '\t' << dev.getProperties().deviceName << std::endl;
        }

        for (const auto& dev : pDevices) {
            int score = vklearn::rateDeviceSuitability(dev, surface);
            candidates.insert(std::make_pair(score, dev));
        }

        if (candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        std::cout << "Use device: " << physicalDevice.getProperties().deviceName << std::endl;
    }

    void createLogicalDevice() {
        auto indices = vklearn::findQueueFamilies(physicalDevice, surface);
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value(),
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCreateInfo(
                {},
                queueFamily,
                1,
                &queuePriority
            );
            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::PhysicalDeviceFeatures deviceFeatures{};
        vk::DeviceCreateInfo createInfo(
            {},
            static_cast<uint32_t>(queueCreateInfos.size()),
            queueCreateInfos.data(),
            0, // no effect on latest implementation
            {}, // same as above
            vklearn::requiredDeviceExtensions.size(),
            vklearn::requiredDeviceExtensions.data(),
            &deviceFeatures
        );
        if (vklearn::enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(vklearn::validationLayers.size());
            createInfo.ppEnabledLayerNames = vklearn::validationLayers.data();
        }
        // for compatibility with older implementations

        device = physicalDevice.createDevice(createInfo);
        graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
        presentQueue = device.getQueue(indices.presentFamily.value(), 0);
    }

    void setupDebugMessenger() {
        if (!vklearn::enableValidationLayers) return;

        if (!vklearn::setDebugMessageFunc(instance)) {
            throw std::runtime_error("cannot set debug message func");
        }

        debugMessenger = instance.createDebugUtilsMessengerEXT(
            vklearn::boilerplate::debugUtilsMessengerCreateInfoEXT(&vklearn::debugCallback)
        );
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    
    void cleanup() {
        device.destroy();
        if (vklearn::enableValidationLayers) {
            instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
        }
        instance.destroySurfaceKHR(surface);
        instance.destroy();

        glfwDestroyWindow(window);

        glfwTerminate();
    }
};

int main() {
    VulkanApp app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}