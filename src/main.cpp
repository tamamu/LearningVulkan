#include "vulkan.h"
#include "glfw.h"
#include "vk.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>

class VulkanApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
private:
    vk::Instance instance;
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
        if (vklearn::enableValidationLayers) {
            instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
        }
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