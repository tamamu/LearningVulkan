#include "vulkan.h"
#include "glfw.h"
#include "vk.hpp"
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "mmd.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <filesystem>

#define WIDTH 1200
#define HEIGHT 600

const std::string MODEL_VERTEX_SHADER_PATH = "spir-v/toon_model.vert.spv";
const std::string EDGE_VERTEX_SHADER_PATH = "spir-v/toon_edge.vert.spv";
const std::string FRAGMENT_SHADER_PATH = "spir-v/toon_tex.frag.spv";
const std::string PMX_PATH = "ying/ying.pmx";
//const std::string PMX_PATH = "paimeng/paimeng.pmx";

const int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    int texID;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions{};

        // float: eR32Sfloat
        // vec2: R32G32Sfloat
        // vec3: R32G32B32Sfloat
        // vec4: R32G32B32A32Sfloat
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = vk::Format::eR32Uint;
        attributeDescriptions[3].offset = offsetof(Vertex, texID);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invModel;
};

class Renderer {
private:
    vk::Device& deviceRef;
    vklearn::SwapChainDetails& swapChainDetails;
    vk::RenderPass& renderPassRef;

public:
    vk::DescriptorSetLayout descriptorSetLayout;
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline graphicsPipeline;
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    vk::CullModeFlags cullModeFlags;

    Renderer(vk::Device& dr, vklearn::SwapChainDetails& scd, vk::RenderPass& rpr, std::string vsp, std::string fsp, vk::CullModeFlags cmf)
    : deviceRef(dr), swapChainDetails(scd), renderPassRef(rpr), vertexShaderPath(vsp), fragmentShaderPath(fsp), cullModeFlags(cmf) {
        recreate();
    }

    ~Renderer() {
        destroy();
    }

    void recreate() {
        createDescriptorSetLayout();
        createGraphicsPipeline();
    }

    void destroy() {
        deviceRef.destroyDescriptorSetLayout(descriptorSetLayout);
        deviceRef.destroyPipeline(graphicsPipeline);
        deviceRef.destroyPipelineLayout(pipelineLayout);
    }

    // シェーダに渡るデータのレイアウトを設定
    void createDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(
            0, // binding
            vk::DescriptorType::eUniformBuffer,
            1, // number of values in the array (1 ubo, in here)
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, // only referencing from vertex shader
            nullptr // only relevant for image sampling
            );
        vk::DescriptorSetLayoutBinding samplerLayoutBinding(
            1,
            vk::DescriptorType::eCombinedImageSampler,
            8,
            vk::ShaderStageFlagBits::eFragment,
            nullptr
        );

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

        vk::DescriptorSetLayoutCreateInfo layoutInfo(
            {},
            static_cast<uint32_t>(bindings.size()),
            bindings.data()
        );

        if (deviceRef.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // 
    void createGraphicsPipeline() {
        vk::ShaderModule vertShaderModule = vklearn::createShaderModuleFromFile(deviceRef, vertexShaderPath);
        vk::ShaderModule fragShaderModule = vklearn::createShaderModuleFromFile(deviceRef, fragmentShaderPath);

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
            cullModeFlags,
            vk::FrontFace::eCounterClockwise,
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

        vk::PipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.depthTestEnable = true;
        depthStencil.depthWriteEnable = true;
        depthStencil.depthCompareOp = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = false;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = false;

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
            1, // descriptorSetLayoutCount
            &descriptorSetLayout, // descriptorSetLayouts
            0,
            nullptr
        );
        
        if (deviceRef.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout) != vk::Result::eSuccess) {
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
            &depthStencil,
            &colorBlending,
            nullptr,
            pipelineLayout,
            renderPassRef,
            0,
            nullptr,
            -1
        );
        if (deviceRef.createGraphicsPipelines(nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        deviceRef.destroyShaderModule(fragShaderModule);
        deviceRef.destroyShaderModule(vertShaderModule);
        // TODO: https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules#page_Shader-stage-creation
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

    vk::RenderPass renderPass;
    // vk::DescriptorSetLayout descriptorSetLayout;
    // vk::PipelineLayout pipelineLayout;
    // vk::Pipeline graphicsPipeline;
    vk::DescriptorPool descriptorPool;
    Renderer* modelRenderer;
    Renderer* edgeRenderer;
    std::vector<vk::DescriptorSet> descriptorSets;

    std::vector<uint32_t> vertexCounts;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexBufferMemory;
    vk::Buffer indexBuffer;
    vk::DeviceMemory indexBufferMemory;
    std::array<vk::Image, 8> textureImage;
    std::array<uint32_t, 8> mipLevels;
    std::array<vk::DeviceMemory, 8> textureImageMemory;
    std::array<vk::ImageView, 8> textureImageView;
    std::array<vk::Sampler, 8> textureSampler;
    std::vector<vk::Buffer> uniformBuffers;
    std::vector<vk::DeviceMemory> uniformBuffersMemory;

    vk::Image depthImage;
    vk::DeviceMemory depthImageMemory;
    vk::ImageView depthImageView;

    std::vector<std::filesystem::path> texturePaths;

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

        modelRenderer = new Renderer(device, swapChainDetails, renderPass, MODEL_VERTEX_SHADER_PATH, FRAGMENT_SHADER_PATH, vk::CullModeFlagBits::eBack);
        edgeRenderer = new Renderer(device, swapChainDetails, renderPass, EDGE_VERTEX_SHADER_PATH, FRAGMENT_SHADER_PATH, vk::CullModeFlagBits::eFront);

        // createDescriptorSetLayout();

        // createGraphicsPipeline();


        createCommandPool();

        createDepthResources();

        createFramebuffers();

        loadModel();

        createTextureImage();

        createTextureSampler();

        createVertexBuffer();

        createIndexBuffer();

        createUniformBuffers();

        createDescriptorPool();

        createDescriptorSets();

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
        deviceFeatures.setSamplerAnisotropy(true);

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
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t idx = 0; idx < swapChainImages.size(); ++idx) {
            swapChainImageViews[idx] = vklearn::boilerplate::createImageView(device, swapChainImages[idx], swapChainDetails.format.format, vk::ImageAspectFlagBits::eColor, 1);
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

        vk::AttachmentDescription depthAttachment(
            {},
            findDepthFormat(),
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eDontCare,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal
        );
        vk::AttachmentReference depthAttachmentRef(
            1,
            vk::ImageLayout::eDepthStencilAttachmentOptimal
        );

        vk::SubpassDescription subpass(
            {},
            vk::PipelineBindPoint::eGraphics,
            0,
            nullptr,
            1,
            &colorAttachmentRef
        );
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        vk::SubpassDependency dependency(
            VK_SUBPASS_EXTERNAL,
            0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            {},
            vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
        );

        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo(
            {},
            static_cast<uint32_t>(attachments.size()),
            attachments.data(),
            1,
            &subpass,
            1,
            &dependency
        );
        if (device.createRenderPass(&renderPassInfo, nullptr, &renderPass) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t idx = 0; idx < swapChainImageViews.size(); idx++) {
            std::array<vk::ImageView, 2> attachments = {
                swapChainImageViews[idx],
                depthImageView
            };

            vk::FramebufferCreateInfo framebufferInfo(
                {},
                renderPass,
                static_cast<uint32_t>(attachments.size()),
                attachments.data(),
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

    vk::Format findDepthFormat() {
        return vklearn::findSupportedFormat(physicalDevice,
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    // color attachmentのようにdepth attachmentを作る
    // これはdraw operationの時に使われるため、1つだけで良い(同時に複数走らない)
    void createDepthResources() {
        vk::Format depthFormat = findDepthFormat();
        createImage(
            swapChainDetails.extent.width,
            swapChainDetails.extent.height,
            1,
            depthFormat,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            depthImage,
            depthImageMemory);
        depthImageView = vklearn::boilerplate::createImageView(device, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
        vklearn::transitionImageLayout(device, commandPool, graphicsQueue, depthImage, depthFormat,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            1);
    }

    void generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
        vk::CommandBuffer commandBuffer = vklearn::beginSingleTimeCommands(device, commandPool);

        // check if image format supports linear blitting
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
        if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        vk::ImageMemoryBarrier barrier;
        barrier
            .setImage(image)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setSubresourceRange(
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
            );
        //vklearn::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t j=1; j < mipLevels; ++j) {
            barrier.subresourceRange.baseMipLevel = j - 1;
            barrier
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                0, nullptr,
                0, nullptr,
                1, &barrier
                );

            vk::ImageBlit blit{};
            blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
            blit.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
            blit.setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j-1, 0, 1));
            blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
            blit.dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            blit.setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, j, 0, 1));

            commandBuffer.blitImage(
                image, vk::ImageLayout::eTransferSrcOptimal,
                image, vk::ImageLayout::eTransferDstOptimal,
                {blit},
                vk::Filter::eLinear);

            barrier
                .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                {},
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        vklearn::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
    }

    void createTextureImage() {
        for (int j=0; j < texturePaths.size(); ++j) {
            int texWidth, texHeight, texChannels;
            stbi_uc* pixels = stbi_load(texturePaths[j].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            vk::DeviceSize imageSize = texWidth * texHeight * 4;
            mipLevels[j] = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

            if (!pixels) {
                throw std::runtime_error("failed to load texture image!");
            }

            vk::Buffer stagingBuffer;
            vk::DeviceMemory stagingBufferMemory;
            std::tie(stagingBuffer, stagingBufferMemory) = vklearn::createBuffer(
                physicalDevice, device,
                imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

            void* data;
            device.mapMemory(stagingBufferMemory, 0, imageSize, {}, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
            device.unmapMemory(stagingBufferMemory);

            stbi_image_free(pixels);

            createImage(
                texWidth, texHeight, mipLevels[j],
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                textureImage[j],
                textureImageMemory[j]
                );
            
            vklearn::transitionImageLayout(
                device, commandPool, graphicsQueue,
                textureImage[j],
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                mipLevels[j]);
            vklearn::copyBufferToImage(
                device, commandPool, graphicsQueue,
                stagingBuffer,
                textureImage[j],
                static_cast<uint32_t>(texWidth),
                static_cast<uint32_t>(texHeight)
            );

            device.destroyBuffer(stagingBuffer);
            device.freeMemory(stagingBufferMemory);

            generateMipmaps(textureImage[j], vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels[j]);

            vklearn::transitionImageLayout(
                device, commandPool, graphicsQueue,
                textureImage[j],
                vk::Format::eR8G8B8A8Srgb,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                mipLevels[j]
            );



            textureImageView[j] = vklearn::boilerplate::createImageView(device, textureImage[j], vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels[j]);
        }
    }

    void createTextureSampler() {
        for (int j=0; j < texturePaths.size(); ++j) {
            vk::SamplerCreateInfo samplerInfo{};
            samplerInfo
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(16.0f)
                .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
                .setCompareOp(vk::CompareOp::eAlways)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMipLodBias(0.0f)
                .setMinLod(0.0f)
                .setMaxLod(static_cast<float>(mipLevels[j]));

            if (device.createSampler(&samplerInfo, nullptr, &textureSampler[j]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create texture sampler!");
            }
        }
    }

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& imageMemory) {
        vk::ImageCreateInfo imageInfo(
            {},
            // e1D - gradient, e2D - 2D image, e3D - voxel volumes
            vk::ImageType::e2D,
            format,
            vk::Extent3D(
                width,
                height,
                1   // depth
            ),
            mipLevels,  // mipLevels
            1,  // arrayLayers
            vk::SampleCountFlagBits::e1,
            tiling, // vk::ImageTiling::eLinear
            usage,
            vk::SharingMode::eExclusive
        );

        // ePreinitialized - first transition will preserve the texels
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;

        if (device.createImage(&imageInfo, nullptr, &image) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create image!");
        }

        vk::MemoryRequirements memRequirements;
        device.getImageMemoryRequirements(image, &memRequirements);

        vk::MemoryAllocateInfo allocInfo(
            memRequirements.size,
            vklearn::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
        );

        if (device.allocateMemory(&allocInfo, nullptr, &imageMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        device.bindImageMemory(image, imageMemory, 0);
    }

    void loadModel() {
        std::vector<PMXLoader::Vertex> _vertices;
        std::vector<int> _planes;
        std::vector<PMXLoader::Material> _materials;
        std::tie(_vertices, _planes, texturePaths, _materials) = PMXLoader::read_pmx(PMX_PATH);

        vertices = std::vector<Vertex>(_vertices.size());
        for (int j=0; j < _vertices.size(); ++j) {
            auto _pos = _vertices[j].position;
            auto _norm = _vertices[j].normal;
            auto _uv = _vertices[j].uv;
            vertices[j] = {
                glm::vec3(_pos.x, _pos.y, _pos.z),
                glm::vec3(_norm.x, _norm.y, _norm.z),
                glm::vec2(_uv.x, _uv.y),
                0
            };
        }

        std::cout << _vertices.size() << "(" << _planes.size() << ")" << std::endl;
        int cur = 0;
        for (int j=0; j < _materials.size(); ++j) {
            vertexCounts.push_back(_materials[j].number_of_plane);
            std::cout << _materials[j].name << " " << _materials[j].number_of_plane / 3 << std::endl;
            for (int k=0; k < _materials[j].number_of_plane / 3; ++k) {
                vertices[cur].texID = _materials[j].normal_texture;
                cur++;
            }
        }
        std::cout << cur << " ok" << std::endl;

        indices = std::vector<uint16_t>(_planes.size());
        for (int j=0; j < _planes.size(); ++j) {
            indices[j] = static_cast<uint16_t>(_planes[j]);
        }

        // vertices = {
        //     {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        //     {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        //     {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        //     {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
        // };
        // indices = {
        //     0, 1, 2, 2, 3, 0
        // };
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        std::tie(stagingBuffer, stagingBufferMemory) = vklearn::createBuffer(
            physicalDevice, device, bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );

        void* data;
        device.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data);
        memcpy(data, vertices.data(), (size_t) bufferSize);
        device.unmapMemory(stagingBufferMemory);

        std::tie(vertexBuffer, vertexBufferMemory) = vklearn::createBuffer(
            physicalDevice, device, bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal
            );

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        device.destroyBuffer(stagingBuffer);
        device.freeMemory(stagingBufferMemory);
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        std::tie(stagingBuffer, stagingBufferMemory) = vklearn::createBuffer(
            physicalDevice, device, bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );
        
        void* data;
        device.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        device.unmapMemory(stagingBufferMemory);

        std::tie(indexBuffer, indexBufferMemory) = vklearn::createBuffer(
            physicalDevice, device, bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        device.destroyBuffer(stagingBuffer);
        device.freeMemory(stagingBufferMemory);
    }

    void createUniformBuffers() {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(swapChainImages.size());
        uniformBuffersMemory.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); ++i) {
            std::tie(uniformBuffers[i], uniformBuffersMemory[i]) =
                vklearn::createBuffer(
                    physicalDevice, device, bufferSize,
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                );
        }
    }

    void createDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 2> poolSizes{};
        poolSizes[0]
            .setType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(static_cast<uint32_t>(swapChainImages.size()));
        poolSizes[1]
            .setType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(static_cast<uint32_t>(swapChainImages.size()));
        
        vk::DescriptorPoolCreateInfo poolInfo(
            {},
            static_cast<uint32_t>(swapChainImages.size()), // max num of descriptor sets
            static_cast<uint32_t>(poolSizes.size()),
            poolSizes.data()
        );
        
        if (device.createDescriptorPool(&poolInfo, nullptr, &descriptorPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(swapChainImages.size(), modelRenderer->descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo(
            descriptorPool,
            static_cast<uint32_t>(swapChainImages.size()),
            layouts.data()
        );
        descriptorSets.resize(swapChainImages.size());

        if (device.allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t idx = 0; idx < swapChainImages.size(); ++idx) {
            vk::DescriptorBufferInfo bufferInfo(
                uniformBuffers[idx],
                0,
                sizeof(UniformBufferObject)
            );
            std::array<vk::DescriptorImageInfo, 8> imageInfos{};
            for (int j=0; j < 8; ++j) {
                imageInfos[j] = vk::DescriptorImageInfo(
                    textureSampler[j],
                    textureImageView[j],
                    vk::ImageLayout::eShaderReadOnlyOptimal
                );
            }

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0]
                .setDstSet(descriptorSets[idx])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setPBufferInfo(&bufferInfo);
            descriptorWrites[1]
                .setDstSet(descriptorSets[idx])
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(texturePaths.size())
                .setPImageInfo(imageInfos.data());

            device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
        vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

        vk::CommandBuffer commandBuffer = vklearn::beginSingleTimeCommands(device, commandPool);

        vk::BufferCopy copyRegion(0, 0, size);
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

        vklearn::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
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
            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{1.0, 1.0, 1.0, 1.0});
            clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
            vk::RenderPassBeginInfo renderPassInfo(
                renderPass,
                swapChainFramebuffers[idx],
                vk::Rect2D({0, 0}, swapChainDetails.extent),
                static_cast<uint32_t>(clearValues.size()),
                clearValues.data()
            );
            commandBuffers[idx].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);
            commandBuffers[idx].bindPipeline(vk::PipelineBindPoint::eGraphics, modelRenderer->graphicsPipeline);
            // commandBuffers[idx].draw(3, 1, 0, 0);
            // commandBuffers[idx].endRenderPass();
            vk::Buffer vertexBuffers[] = {vertexBuffer};
            vk::DeviceSize offsets[] = {0};
            commandBuffers[idx].bindVertexBuffers(0, 1, vertexBuffers, offsets);
            commandBuffers[idx].bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);
            commandBuffers[idx].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, modelRenderer->pipelineLayout, 0, 1, &descriptorSets[idx], 0, nullptr);
            uint32_t firstIndex = 0;
            for (int j=0; j < vertexCounts.size(); ++j) {
                commandBuffers[idx].drawIndexed(vertexCounts[j], 1, firstIndex, 0, 0);
                firstIndex += vertexCounts[j];
            }

            
            commandBuffers[idx].bindPipeline(vk::PipelineBindPoint::eGraphics, edgeRenderer->graphicsPipeline);
            commandBuffers[idx].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, edgeRenderer->pipelineLayout, 0, 1, &descriptorSets[idx], 0, nullptr);
            firstIndex = 0;
            for (int j=0; j < vertexCounts.size(); ++j) {
                commandBuffers[idx].drawIndexed(vertexCounts[j], 1, firstIndex, 0, 0);
                firstIndex += vertexCounts[j];
            }
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
        device.destroyImageView(depthImageView);
        device.destroyImage(depthImage);
        device.freeMemory(depthImageMemory);

        for (size_t idx = 0; idx < swapChainFramebuffers.size(); idx++) {
            device.destroyFramebuffer(swapChainFramebuffers[idx]);
        }

        for (size_t idx = 0; idx < swapChainFramebuffers.size(); idx++) {
            device.destroyBuffer(uniformBuffers[idx]);
            device.freeMemory(uniformBuffersMemory[idx]);
        }

        modelRenderer->destroy();
        edgeRenderer->destroy();
        device.destroyDescriptorPool(descriptorPool);
        device.freeCommandBuffers(commandPool, commandBuffers);
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
        modelRenderer->recreate();
        edgeRenderer->recreate();
        createDepthResources();
        createFramebuffers();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
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
        updateUniformBuffer(imageIndex);
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

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(
            glm::mat4(1.0f),
            time * glm::radians(90.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        ubo.view = glm::lookAt(
            glm::vec3(0.0f, 18.0f, 20.0f),
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        );
        ubo.proj = glm::perspective(
            glm::radians(45.0f),
            swapChainDetails.extent.width / (float) swapChainDetails.extent.height,
            0.1f,
            40.0f
        );
        ubo.proj[1][1] *= -1; // Y coordinate upside down
        ubo.invModel = glm::inverse(ubo.model);

        void* data;
        device.mapMemory(uniformBuffersMemory[currentImage], 0, sizeof(ubo), {}, &data);
        memcpy(data, &ubo, sizeof(ubo));
        device.unmapMemory(uniformBuffersMemory[currentImage]);
    }
    
    void cleanup() {
        cleanupSwapChain();

        for (int j=0; j < texturePaths.size(); ++j) {
            device.destroySampler(textureSampler[j]);
            device.destroyImageView(textureImageView[j]);
            device.destroyImage(textureImage[j]);
            device.freeMemory(textureImageMemory[j]);
        }

        device.destroyBuffer(indexBuffer);
        device.freeMemory(indexBufferMemory);
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