#include "vulkan.h"
#include "glfw.h"
#include "vk.hpp"
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <thread>

#define WIDTH 1200
#define HEIGHT 600

const std::string VERTEX_SHADER_PATH = "spir-v/pos_col.vert.spv";
const std::string FRAGMENT_SHADER_PATH = "spir-v/static_in_color.frag.spv";

const int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        // float: eR32Sfloat
        // vec2: R32G32Sfloat
        // vec3: R32G32B32Sfloat
        // vec4: R32G32B32A32Sfloat
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

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
    vk::SwapchainKHR swapChain;
    vklearn::SwapChainDetails swapChainDetails;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    vk::RenderPass renderPass;
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline graphicsPipeline;
    std::vector<vk::Framebuffer> swapChainFramebuffers;
    vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> commandBuffers;
    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;
    vk::DebugUtilsMessengerEXT debugMessenger;
    GLFWwindow* window;
    size_t currentFrame = 0;
    bool framebufferResized = false;

    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };
    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexBufferMemory;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "VulkanApp", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
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

        createSwapChain();

        createImageViews();

        createRenderPass();

        createGraphicsPipeline();

        createFramebuffers();

        createCommandPool();

        createVertexBuffer();

        createCommandBuffers();

        createSyncObjects();
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

    void createSwapChain() {
        std::tie(swapChain, swapChainDetails) = vklearn::boilerplate::SwapchainKHR(physicalDevice, device, surface, window, &swapChain);
        swapChainImages = device.getSwapchainImagesKHR(swapChain);
    }

    void createImageViews() {
        swapChainImageViews = std::vector<vk::ImageView>(swapChainImages.size());
        for (size_t idx = 0; idx < swapChainImages.size(); ++idx) {
            vk::ImageViewCreateInfo createInfo(
                {},
                swapChainImages[idx],
                vk::ImageViewType::e2D,
                swapChainDetails.format.format,
                vk::ComponentMapping(
                    vk::ComponentSwizzle::eIdentity,
                    vk::ComponentSwizzle::eIdentity,
                    vk::ComponentSwizzle::eIdentity,
                    vk::ComponentSwizzle::eIdentity),
                vk::ImageSubresourceRange(
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    1,
                    0,
                    1)
                );
            if (device.createImageView(&createInfo, nullptr, &swapChainImageViews[idx]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void createRenderPass() {
        vk::AttachmentDescription colorAttachment(
            {},
            swapChainDetails.format.format,
            vk::SampleCountFlagBits::e1,

            // eLoad - preserve the existing contents of the attachment
            // eClear - clear the values to a constant at the start
            // eDontCare - existing contents are undefined; we don't care about them
            vk::AttachmentLoadOp::eClear,

            // eStore - rendered contents will be stored in memory and can be read later
            // eDontCare - contents of the framebuffer will be undefined after the rendering operation
            vk::AttachmentStoreOp::eStore,

            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,

            // eColorAttachmentOptimal - images used as color attachment
            // ePresentSrcKHR - images to be presented in the swap chain
            // eTransferDstOptimal - images to be used as destination for a memory copy operation
            vk::ImageLayout::eUndefined,

            vk::ImageLayout::ePresentSrcKHR
            );
        vk::AttachmentReference colorAttachmentRef(
            0, // index of attachment description
            vk::ImageLayout::eColorAttachmentOptimal);
        vk::SubpassDescription subpass(
            {},
            vk::PipelineBindPoint::eGraphics,
            0,
            nullptr,
            1,
            &colorAttachmentRef
        );
        vk::SubpassDependency dependency(
            VK_SUBPASS_EXTERNAL,
            0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {},
            vk::AccessFlagBits::eColorAttachmentWrite
        );
        vk::RenderPassCreateInfo renderPassInfo(
            {},
            1,
            &colorAttachment,
            1,
            &subpass,
            1,
            &dependency
        );
        if (device.createRenderPass(&renderPassInfo, nullptr, &renderPass) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createGraphicsPipeline() {
        vk::ShaderModule vertShaderModule = vklearn::createShaderModuleFromFile(device, VERTEX_SHADER_PATH);
        vk::ShaderModule fragShaderModule = vklearn::createShaderModuleFromFile(device, FRAGMENT_SHADER_PATH);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
            {},
            vk::ShaderStageFlagBits::eVertex,
            vertShaderModule,
            "main");
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
            {},
            vk::ShaderStageFlagBits::eFragment,
            fragShaderModule,
            "main");
        
        vk::PipelineShaderStageCreateInfo shaderStages[2] = {vertShaderStageInfo, fragShaderStageInfo};
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
            {},
            1,
            &bindingDescription,
            static_cast<uint32_t>(attributeDescriptions.size()),
            attributeDescriptions.data()
        );
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
            {},
            vk::PrimitiveTopology::eTriangleList,
            false);
        vk::Viewport viewport(
            0.0,
            0.0,
            static_cast<float>(swapChainDetails.extent.width),
            static_cast<float>(swapChainDetails.extent.height),
            0.0,
            1.0);
        vk::Rect2D scissor({0, 0}, swapChainDetails.extent);
        vk::PipelineViewportStateCreateInfo viewportState(
            {},
            1,
            &viewport,
            1,
            &scissor);
        vk::PipelineRasterizationStateCreateInfo rasterizer(
            {},
            false,
            false,
            vk::PolygonMode::eFill,
            vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise,
            false,
            0.0,
            0.0,
            0.0,
            1.0
        );
        vk::PipelineMultisampleStateCreateInfo multisampling(
            {},
            vk::SampleCountFlagBits::e1,
            false,
            1.0,
            nullptr,
            false,
            false
        );
        vk::PipelineColorBlendAttachmentState colorBlendAttachment(
            false,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA
        );

        //// enable color blending
        // vk::PipelineColorBlendAttachmentState colorBlendAttachment(
        //     true,
        //     vk::BlendFactor::eSrc1Alpha,
        //     vk::BlendFactor::eOneMinusSrc1Alpha,
        //     vk::BlendOp::eAdd,
        //     vk::BlendFactor::eOne,
        //     vk::BlendFactor::eZero,
        //     vk::BlendOp::eAdd,
        //     vk::ColorComponentFlagBits::eR |
        //     vk::ColorComponentFlagBits::eG |
        //     vk::ColorComponentFlagBits::eB |
        //     vk::ColorComponentFlagBits::eA
        // );
        vk::PipelineColorBlendStateCreateInfo colorBlending(
            {},
            false,
            vk::LogicOp::eCopy,
            1,
            &colorBlendAttachment,
            {0.0, 0.0, 0.0, 0.0}
        );

        vk::DynamicState dynamicStates[2] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eLineWidth
        };

        vk::PipelineDynamicStateCreateInfo dynamicState(
            {},
            2,
            dynamicStates
        );

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
            {},
            0,
            nullptr,
            0,
            nullptr
        );
        
        if (device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            {},
            2,
            shaderStages,
            &vertexInputInfo,
            &inputAssembly,
            nullptr,
            &viewportState,
            &rasterizer,
            &multisampling,
            nullptr,
            &colorBlending,
            nullptr,
            pipelineLayout,
            renderPass,
            0,
            nullptr,
            -1
        );
        if (device.createGraphicsPipelines(nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        device.destroyShaderModule(fragShaderModule);
        device.destroyShaderModule(vertShaderModule);
        // TODO: https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules#page_Shader-stage-creation
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t idx = 0; idx < swapChainImageViews.size(); idx++) {
            vk::ImageView attachments[] = {
                swapChainImageViews[idx]
            };

            vk::FramebufferCreateInfo framebufferInfo(
                {},
                renderPass,
                1,
                attachments,
                swapChainDetails.extent.width,
                swapChainDetails.extent.height,
                1);
            if (device.createFramebuffer(&framebufferInfo, nullptr, &swapChainFramebuffers[idx]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create framebuffers!");
            }
        }
    }

    void createCommandPool() {
        vklearn::QueueFamilyIndices queueFamilyIndices = vklearn::findQueueFamilies(physicalDevice, surface);
        vk::CommandPoolCreateInfo poolInfo(
            {},
            queueFamilyIndices.graphicsFamily.value());
        if (device.createCommandPool(&poolInfo, nullptr, &commandPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createVertexBuffer() {
        vk::BufferCreateInfo bufferInfo(
            {},
            sizeof(vertices[0]) * vertices.size(),
            vk::BufferUsageFlagBits::eVertexBuffer,
            vk::SharingMode::eExclusive);

        if (device.createBuffer(&bufferInfo, nullptr, &vertexBuffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(vertexBuffer);
        vk::MemoryAllocateInfo allocInfo(
            memRequirements.size,
            vklearn::findMemoryType(
                physicalDevice,
                memRequirements.memoryTypeBits,
                static_cast<vk::MemoryPropertyFlagBits>(
                    static_cast<uint32_t>(vk::MemoryPropertyFlagBits::eHostVisible) | static_cast<uint32_t>(vk::MemoryPropertyFlagBits::eHostCoherent)
                )
                ));

        if (device.allocateMemory(&allocInfo, nullptr, &vertexBufferMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        device.bindBufferMemory(vertexBuffer, vertexBufferMemory, 0);

        void* data;
        device.mapMemory(vertexBufferMemory, 0, bufferInfo.size, {}, &data);
        memcpy(data, vertices.data(), (size_t) bufferInfo.size);
        device.unmapMemory(vertexBufferMemory);
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());
        vk::CommandBufferAllocateInfo allocInfo(
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            static_cast<uint32_t>(commandBuffers.size())
        );

        if (device.allocateCommandBuffers(&allocInfo, commandBuffers.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t idx = 0; idx < commandBuffers.size(); idx++) {
            vk::CommandBufferBeginInfo beginInfo({}, nullptr);
            if (commandBuffers[idx].begin(&beginInfo) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }
            vk::ClearValue clearColor(vk::ClearColorValue(std::array<float, 4>({{0.0, 0.0, 0.0,1.0}})));
            vk::RenderPassBeginInfo renderPassInfo(
                renderPass,
                swapChainFramebuffers[idx],
                vk::Rect2D({0, 0}, swapChainDetails.extent),
                1,
                &clearColor
            );
            commandBuffers[idx].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);
            commandBuffers[idx].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
            // commandBuffers[idx].draw(3, 1, 0, 0);
            // commandBuffers[idx].endRenderPass();
            vk::Buffer vertexBuffers[] = {vertexBuffer};
            vk::DeviceSize offsets[] = {0};
            commandBuffers[idx].bindVertexBuffers(0, 1, vertexBuffers, offsets);
            commandBuffers[idx].draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            commandBuffers[idx].end();
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapChainImages.size(), nullptr);

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

        for (size_t idx = 0; idx < MAX_FRAMES_IN_FLIGHT; idx++) {
            if (device.createSemaphore(&semaphoreInfo, nullptr, &imageAvailableSemaphores[idx]) != vk::Result::eSuccess
            || device.createSemaphore(&semaphoreInfo, nullptr, &renderFinishedSemaphores[idx]) != vk::Result::eSuccess
            || device.createFence(&fenceInfo, nullptr, &inFlightFences[idx]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create semaphore!");
            }
        }
    }

    void cleanupSwapChain() {
        for (size_t idx = 0; idx < swapChainFramebuffers.size(); idx++) {
            device.destroyFramebuffer(swapChainFramebuffers[idx]);
        }

        device.freeCommandBuffers(commandPool, commandBuffers);
        device.destroyPipeline(graphicsPipeline);
        device.destroyPipelineLayout(pipelineLayout);
        device.destroyRenderPass(renderPass);

        for (size_t idx = 0; idx < swapChainImageViews.size(); idx++) {
            device.destroyImageView(swapChainImageViews[idx]);
        }

        device.destroySwapchainKHR(swapChain);
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandBuffers();
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
            drawFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        device.waitIdle();
    }

    void drawFrame() {
        device.waitForFences({inFlightFences[currentFrame]}, true, UINT64_MAX);
        
        uint32_t imageIndex;
        vk::Result result = device.acquireNextImageKHR(swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr, &imageIndex);

        if (result == vk::Result::eErrorOutOfDateKHR) {
            // stop drawing current frame
            device.resetFences({inFlightFences[currentFrame]});
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // check if the next image is used in previous frame
        if (imagesInFlight[imageIndex]) {
            device.waitForFences({imagesInFlight[imageIndex]}, true, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        vk::Semaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        vk::Semaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::SubmitInfo submitInfo(
            1,
            waitSemaphores,
            waitStages,
            1,
            &commandBuffers[imageIndex],
            1,
            signalSemaphores);
        device.resetFences({inFlightFences[currentFrame]});
        graphicsQueue.submit({submitInfo}, inFlightFences[currentFrame]);
        vk::SwapchainKHR swapChains[] = {swapChain};
        vk::PresentInfoKHR presentInfo(
            1,
            signalSemaphores,
            1,
            swapChains,
            &imageIndex,
            nullptr
        );
        result = presentQueue.presentKHR(&presentInfo);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    
    void cleanup() {
        cleanupSwapChain();

        device.destroyBuffer(vertexBuffer);
        device.freeMemory(vertexBufferMemory);

        for (size_t idx = 0; idx < MAX_FRAMES_IN_FLIGHT; idx++) {
            device.destroySemaphore(renderFinishedSemaphores[idx]);
            device.destroySemaphore(imageAvailableSemaphores[idx]);
            device.destroyFence(inFlightFences[idx]);
        }
        device.destroyCommandPool(commandPool);
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