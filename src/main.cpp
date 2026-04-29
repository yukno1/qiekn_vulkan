#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>

import vulkan; // provides the functions, structures and enumerations
import std;


// clang-format off
// #include <algorithm> // Necessary for std::clamp, if not import std;
#include <assert.h>
#include <cstdlib>  // provides the EXIT_SUCCESS and EXIT_FAILURE macros
#include <cstdint> // Necessary for uint32_t
// #include <cstring>
// #include <fstream>
// #include <limits> // Necessary for std::numeric_limits, if not import std;
// #include <iostream>
// #include <limits>
// #include <memory>
// #include <stdexcept>
// #include <vector>
// clang-format on

// It’s a good idea to use constants because we’ll be referring to these values
// a couple of times in the future.
constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {{{.location = 0,
                  .binding  = 0,
                  .format   = vk::Format::eR32G32Sfloat,
                  .offset   = offsetof(Vertex, pos)},
                 {.location = 1,
                  .binding  = 0,
                  .format   = vk::Format::eR32G32B32Sfloat,
                  .offset   = offsetof(Vertex, color)}}};
    }
};

const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    // store the Vulkan objects as private class members and initiate each of
    // them
private:
    // store a reference to the actual window
    GLFWwindow* window = nullptr;

    // the handle to the instance and the raii context:
    vk::raii::Context                context;
    vk::raii::Instance               instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    // window surface
    vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device         device         = nullptr;

    uint32_t        queueIndex = ~0;
    vk::raii::Queue queue      = nullptr;

    vk::raii::SwapchainKHR           swapChain = nullptr;
    std::vector<vk::Image>           swapChainImages;
    vk::SurfaceFormatKHR             swapChainSurfaceFormat;
    vk::Extent2D                     swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    vk::raii::PipelineLayout pipelineLayout   = nullptr;
    vk::raii::Pipeline       graphicsPipeline = nullptr;

    vk::raii::CommandPool                commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    vk::raii::Buffer       vertexBuffer       = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     inFlightFences;
    uint32_t                         frameIndex = 0;

    bool framebufferResized = false;

    std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};

    void initWindow()
    {
        glfwInit();

        // Because GLFW was originally designed to create an OpenGL context, we need
        // to tell it to not create an OpenGL context with a later call:
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Because handling resized windows takes special care that we’ll look into
        // later, disable it for now
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // initialize the window
        // The 4th parameter allows you to optionally specify a monitor to open the
        // window on.
        // The 5th parameter is only relevant to OpenGL.
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan()
    {
        // The very first thing you need to do is initialize the Vulkan library by
        // creating an instance.
        // The instance is the connection between your application and the Vulkan
        // library, and creating it involves specifying some details about your
        // application to the driver.
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createVertexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    /**
     * start rendering frames
     */
    void mainLoop()
    {
        // To keep the application running until either an error occurs or the
        // window is closed, we need to add an event loop.
        // It loops and checks for events like pressing the X button until the user
        // has closed the window.
        // This is also the loop where we’ll later call a function to render a
        // single frame.
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
    }

    /**
     * deallocate the resources
     */
    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
        std::cout << "Creating Instance" << std::endl;
        // This data is technically optional, but it may provide some useful
        // information to the driver to optimize our specific application
        constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
                                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName        = "No Engine",
                                              .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                                              .apiVersion         = vk::ApiVersion14};

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        if (std::ranges::any_of(
                requiredLayers,
                [&layerProperties](auto const& requiredLayer)
                {
                    return std::ranges::none_of(
                        layerProperties,
                        [requiredLayer](auto const& layerProperty)
                        { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                }))
        {
            throw std::runtime_error("One or more required layers are not supported!");
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredInstanceExtensions();

        // Check if the required extensions are supported by the Vulkan
        // implementation.
        auto extensionProperties   = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(
            requiredExtensions,
            [&extensionProperties](auto const& requiredExtension)
            {
                return std::ranges::none_of(
                    extensionProperties,
                    [requiredExtension](auto const& extensionProperty)
                    { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
            });
        if (unsupportedPropertyIt != requiredExtensions.end())
        {
            throw std::runtime_error("Required extension not supported: " +
                                     std::string(*unsupportedPropertyIt));
        }

        // A lot of information in Vulkan is passed through structs
        // not optional and tells the Vulkan driver which global extensions and
        // validation layers we want to use.
        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames     = requiredLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data()};

        instance = vk::raii::Instance(context, createInfo);
    }

    void setupDebugMessenger()
    {
        std::cout << "Setting Up DebugMessenger" << std::endl;
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType     = messageTypeFlags,
            .pfnUserCallback = &debugCallback};
        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void createSurface()
    {
        std::cout << "Create Surface" << std::endl;
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }



    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
    {
        // Check if the physicalDevice supports the Vulkan 1.3 API version
        bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

        // Check if any of the queue families support graphics operations
        auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(
            queueFamilies,
            [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

        // Check if all required physicalDevice extensions are available
        auto availableDeviceExtensions     = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(
            requiredDeviceExtension,
            [&availableDeviceExtensions](auto const& requiredDeviceExtension)
            {
                return std::ranges::any_of(
                    availableDeviceExtensions,
                    [requiredDeviceExtension](auto const& availableDeviceExtension)
                    {
                        return strcmp(availableDeviceExtension.extensionName,
                                      requiredDeviceExtension) == 0;
                    });
            });

        // Check if the physicalDevice supports the required features
        auto features =
            physicalDevice
                .template getFeatures2<vk::PhysicalDeviceFeatures2,
                                       vk::PhysicalDeviceVulkan11Features,
                                       vk::PhysicalDeviceVulkan13Features,
                                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures =
            features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
                .extendedDynamicState;

        // Return true if the physicalDevice meets all the criteria
        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions &&
               supportsRequiredFeatures;
    }

    void pickPhysicalDevice()
    {
        std::cout << "  Pick Physical Device" << std::endl;
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        auto const devIter = std::ranges::find_if(physicalDevices,
                                                  [&](auto const& physicalDevice)
                                                  { return isDeviceSuitable(physicalDevice); });
        if (devIter == physicalDevices.end())
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        physicalDevice = *devIter;
    }

    void createLogicalDevice()
    {
        std::cout << "Create Logical Device" << std::endl;
        // find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
            physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both
        // graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0)
        {
            throw std::runtime_error(
                "Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                                  // vk::PhysicalDeviceFeatures2
                {.shaderDrawParameters = VK_TRUE},   // vk::PhysicalDeviceVulkan11Features
                {.synchronization2 = true,
                 .dynamicRendering = true},   // vk::PhysicalDeviceVulkan13Features
                {.extendedDynamicState =
                     true}   // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            };

        // create a Device
        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &deviceQueueCreateInfo,
            .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    void createSwapChain()
    {
        std::cout << "Create Swap Chain" << std::endl;
        vk::SurfaceCapabilitiesKHR surfaceCapabilities =
            physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent        = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats =
            physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

        std::vector<vk::PresentModeKHR> availablePresentModes =
            physicalDevice.getSurfacePresentModesKHR(*surface);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface          = *surface,
            .minImageCount    = minImageCount,
            .imageFormat      = swapChainSurfaceFormat.format,
            .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
            .imageExtent      = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform     = surfaceCapabilities.currentTransform,
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode      = presentMode,
            .clipped          = true};

        swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    void cleanupSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
    }

    void createImageViews()
    {
        std::cout << "Create ImageViews" << std::endl;
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType         = vk::ImageViewType::e2D,
            .format           = swapChainSurfaceFormat.format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        for (auto& image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    void createGraphicsPipeline()
    {
        std::cout << "Create GraphicsPipeline" << std::endl;
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName  = "vertMain"   // function to invoke, known as the entrypoint
            // .pSpecializationInfo is optional, it allows you to specify values for shader
            // constants
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName  = "fragMain"};


        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                            fragShaderStageInfo};

        // format of the vertex data that will be passed to the vertex shader
        auto bindingDescription    = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = attributeDescriptions.data()};
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

        std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                       vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()};

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,   // vk::True: fragments that are beyond the near and far
                                             // planes are clamped to them.
            .rasterizerDiscardEnable =
                vk::False,   // vk::True: geometry never passes through the rasterizer stage
            .polygonMode     = vk::PolygonMode::eFill,
            .cullMode        = vk::CullModeFlagBits::eBack,
            .frontFace       = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth       = 1.0f};

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

        // color blending
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable    = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

        vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable   = vk::False,
                                                            .logicOp         = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments = &colorBlendAttachment};

        // pipeline layout
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount         = 0,
                                                        .pushConstantRangeCount = 0};

        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
            pipelineCreateInfoChain = {{.stageCount          = 2,
                                        .pStages             = shaderStages,
                                        .pVertexInputState   = &vertexInputInfo,
                                        .pInputAssemblyState = &inputAssembly,
                                        .pViewportState      = &viewportState,
                                        .pRasterizationState = &rasterizer,
                                        .pMultisampleState   = &multisampling,
                                        .pColorBlendState    = &colorBlending,
                                        .pDynamicState       = &dynamicState,
                                        .layout              = pipelineLayout,
                                        .renderPass          = nullptr},
                                       {.colorAttachmentCount    = 1,
                                        .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

        graphicsPipeline = vk::raii::Pipeline(
            device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

    void createCommandPool()
    {
        std::cout << "Creating Command Pool" << std::endl;
        vk::CommandPoolCreateInfo poolInfo{.flags =
                                               vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                           .queueFamilyIndex = queueIndex};
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createVertexBuffer()
    {
        vk::BufferCreateInfo bufferInfo{.size        = sizeof(vertices[0]) * vertices.size(),
                                        .usage       = vk::BufferUsageFlagBits::eVertexBuffer,
                                        .sharingMode = vk::SharingMode::eExclusive};
        vertexBuffer = vk::raii::Buffer(device, bufferInfo);

        vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                              vk::MemoryPropertyFlagBits::eHostVisible |
                                                  vk::MemoryPropertyFlagBits::eHostCoherent)};
        vertexBufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);

        vertexBuffer.bindMemory(*vertexBufferMemory, 0);

        void* data = vertexBufferMemory.mapMemory(0, bufferInfo.size);
        memcpy(data, vertices.data(), bufferInfo.size);
        vertexBufferMemory.unmapMemory();
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createCommandBuffers()
    {
        std::cout << "Creating Command Buffer" << std::endl;
        vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                                .level       = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto& commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});

        // Before starting rendering, transition the swapchain image to
        // vk::ImageLayout::eColorAttachmentOptimal
        transition_image_layout(imageIndex,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                {},   // srcAccessMask (no need to wait for previous operations)
                                vk::AccessFlagBits2::eColorAttachmentWrite,   // dstAccessMask
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,   // srcStage
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput    // dstStage
        );

        // Set up the color attachment
        vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {.imageView = swapChainImageViews[imageIndex],
                                                      .imageLayout =
                                                          vk::ImageLayout::eColorAttachmentOptimal,
                                                      .loadOp     = vk::AttachmentLoadOp::eClear,
                                                      .storeOp    = vk::AttachmentStoreOp::eStore,
                                                      .clearValue = clearColor};

        // set up the rendering info
        vk::RenderingInfo renderingInfo = {
            .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo};

        commandBuffer.beginRendering(renderingInfo);

        // bind the graphics pipeline
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);


        commandBuffer.setViewport(0,
                                  vk::Viewport(0.0f,
                                               0.0f,
                                               static_cast<float>(swapChainExtent.width),
                                               static_cast<float>(swapChainExtent.height),
                                               0.0f,
                                               1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
        commandBuffer.draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);
        commandBuffer.endRendering();

        // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        transition_image_layout(imageIndex,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eColorAttachmentWrite,   // srcAccessMask
                                {},                                           // dstAccessMask
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,   // srcStage
                                vk::PipelineStageFlagBits2::eBottomOfPipe             // dstStage
        );
        commandBuffer.end();
    }

    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                                 vk::AccessFlags2        dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask)
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask        = src_stage_mask,
            .srcAccessMask       = src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = old_layout,
            .newLayout           = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = swapChainImages[imageIndex],
            .subresourceRange    = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1}};
        vk::DependencyInfo dependency_info = {
            .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};
        commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
    }

    void createSyncObjects()
    {
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() &&
               inFlightFences.empty());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            inFlightFences.emplace_back(
                device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
    }

    void drawFrame()
    {
        // Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by
        // frameIndex, while renderFinishedSemaphores is indexed by imageIndex
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }
        device.resetFences(*inFlightFences[frameIndex]);

        auto [result, imageIndex] =
            swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR
        // can be checked as a result here and does not need to be caught by an exception.
        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        // On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
        // On any error code, aquireNextImage already threw an exception.
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Only reset the fence if we are submitting work
        device.resetFences(*inFlightFences[frameIndex]);

        commandBuffers[frameIndex].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(
            vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                                        .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
                                        .pWaitDstStageMask    = &waitDestinationStageMask,
                                        .commandBufferCount   = 1,
                                        .pCommandBuffers      = &*commandBuffers[frameIndex],
                                        .signalSemaphoreCount = 1,
                                        .pSignalSemaphores =
                                            &*renderFinishedSemaphores[imageIndex]};
        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                                .pWaitSemaphores =
                                                    &*renderFinishedSemaphores[imageIndex],
                                                .swapchainCount = 1,
                                                .pSwapchains    = &*swapChain,
                                                .pImageIndices  = &imageIndex};
        result = queue.presentKHR(presentInfoKHR);
        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR
        // can be checked as a result here and does not need to be caught by an exception.
        if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) ||
            framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
        }
        else
        {
            // There are no other success codes than eSuccess; on any error code, presentKHR already
            // threw an exception.
            assert(result == vk::Result::eSuccess);
        }
        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                              .pCode =
                                                  reinterpret_cast<const uint32_t*>(code.data())};
        vk::raii::ShaderModule     shaderModule{device, createInfo};
        return shaderModule;
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) &&
            (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        std::vector<vk::SurfaceFormatKHR> const& availableFormats)
    {
        const auto formatIt =
            std::ranges::find_if(availableFormats,
                                 [](const auto& format)
                                 {
                                     return format.format == vk::Format::eB8G8R8A8Srgb &&
                                            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                                 });
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(
        std::vector<vk::PresentModeKHR> const& availablePresentModes)
    {
        assert(std::ranges::any_of(availablePresentModes,
                                   [](auto presentMode)
                                   { return presentMode == vk::PresentModeKHR::eFifo; }));
        // check if vk::PresentModeKHR::eMailbox is avaliable
        return std::ranges::any_of(availablePresentModes,
                                   [](const vk::PresentModeKHR value)
                                   { return vk::PresentModeKHR::eMailbox == value; })
                   ? vk::PresentModeKHR::eMailbox
                   : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {
            std::clamp<uint32_t>(
                width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(
                height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
    }

    // return a list of the required instance extensions
    std::vector<const char*> getRequiredInstanceExtensions()
    {
        // Check if the required GLFW extensions are supported by the Vulkan
        // implementation.
        uint32_t glfwExtensionCount = 0;
        auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            // To set up a callback in the program to handle messages and the
            // associated details, we have to set up a debug messenger with a callback
            // using the VK_EXT_debug_utils extension.
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::cerr << "validation layer: type " << debugUtilsMessageTypeFlagsToString(type)
                  << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }

    // 辅助函数
    // vulkan.cppm 编译为模块时，ErrorCategoryImpl::message() 内部调用了 to_string
    // → toHexString → std::format → __allocating_buffer。
    // 模块 BMI 中这个析构函数没有被内联，变成了需要外部链接的符号，但 libc++.a
    // 里没有它（因为本该内联）。
    static std::string debugUtilsMessageTypeFlagsToString(vk::DebugUtilsMessageTypeFlagsEXT flags)
    {
        std::string result;
        if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) result += "General|";
        if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) result += "Validation|";
        if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) result += "Performance|";
        if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding)
            result += "DeviceAddressBinding|";
        if (!result.empty()) result.pop_back();   // 去掉末尾的 '|'
        return result;
    }

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }
        std::vector<char> buffer(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return buffer;
    }
};

int main()
{
    // If any kind of fatal error occurs during execution, then we’ll throw a
    // std::runtime_error exception with a descriptive message, which will
    // propagate back to the main function and be printed to the command prompt
    try
    {
        HelloTriangleApplication app;
        app.run();
        // To handle a variety of standard exception types, as
        // well, we catch the more general std::exception.
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}