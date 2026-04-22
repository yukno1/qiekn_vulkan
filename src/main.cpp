import vulkan; // provides the functions, structures and enumerations
import std;
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// clang-format off
#include <assert.h>
#include <cstdlib>  // provides the EXIT_SUCCESS and EXIT_FAILURE macros
// clang-format on

// It’s a good idea to use constants because we’ll be referring to these values
// a couple of times in the future.
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

  // store the Vulkan objects as private class members and initiate each of
  // them
private:
  // store a reference to the actual window
  GLFWwindow *window;

  // the handle to the instance and the raii context:
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  // window surface
  vk::raii::SurfaceKHR surface = nullptr;

  vk::raii::Queue graphicsQueue = nullptr;

  std::vector<const char *> requiredDeviceExtension = {
      vk::KHRSwapchainExtensionName};

  void initWindow() {
    glfwInit();

    // Because GLFW was originally designed to create an OpenGL context, we need
    // to tell it to not create an OpenGL context with a later call:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Because handling resized windows takes special care that we’ll look into
    // later, disable it for now
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // initialize the window
    // The 4th parameter allows you to optionally specify a monitor to open the
    // window on.
    // The 5th parameter is only relevant to OpenGL.
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }
  void initVulkan() {
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
  }

  /**
   * start rendering frames
   */
  void mainLoop() {
    // To keep the application running until either an error occurs or the
    // window is closed, we need to add an event loop.
    // It loops and checks for events like pressing the X button until the user
    // has closed the window.
    // This is also the loop where we’ll later call a function to render a
    // single frame.
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  /**
   * deallocate the resources
   */
  void cleanup() {
    glfwDestroyWindow(window);

    glfwTerminate();
  }

  void createInstance() {
    // This data is technically optional, but it may provide some useful
    // information to the driver to optimize our specific application
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14};

    // Get the required layers
    std::vector<char const *> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(
            requiredLayers, [&layerProperties](auto const &requiredLayer) {
              return std::ranges::none_of(
                  layerProperties, [requiredLayer](auto const &layerProperty) {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                  });
            })) {
      throw std::runtime_error(
          "One or more required layers are not supported!");
    }

    // Get the required extensions.
    auto requiredExtensions = getRequiredInstanceExtensions();

    // Check if the required extensions are supported by the Vulkan
    // implementation.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt = std::ranges::find_if(
        requiredExtensions,
        [&extensionProperties](auto const &requiredExtension) {
          return std::ranges::none_of(
              extensionProperties,
              [requiredExtension](auto const &extensionProperty) {
                return strcmp(extensionProperty.extensionName,
                              requiredExtension) == 0;
              });
        });
    if (unsupportedPropertyIt != requiredExtensions.end()) {
      throw std::runtime_error("Required extension not supported: " +
                               std::string(*unsupportedPropertyIt));
    }

    // A lot of information in Vulkan is passed through structs
    // not optional and tells the Vulkan driver which global extensions and
    // validation layers we want to use.
    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()};

    instance = vk::raii::Instance(context, createInfo);
  }

  // return a list of the required instance extensions
  std::vector<const char *> getRequiredInstanceExtensions() {
    // Check if the required GLFW extensions are supported by the Vulkan
    // implementation.
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      // To set up a callback in the program to handle messages and the
      // associated details, we have to set up a debug messenger with a callback
      // using the VK_EXT_debug_utils extension.
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  void pickPhysicalDevice() {
    std::vector<vk::raii::PhysicalDevice> physicalDevices =
        instance.enumeratePhysicalDevices();
    auto const devIter =
        std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) {
          return isDeviceSuitable(physicalDevice);
        });
    if (devIter == physicalDevices.end()) {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
    physicalDevice = *devIter;
  }

  void createLogicalDevice() {
    // find the index of the first queue family that supports graphics
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports graphics
    auto graphicsQueueFamilyProperty =
        std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
          return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                 static_cast<vk::QueueFlags>(0);
        });
    assert(graphicsQueueFamilyProperty != queueFamilyProperties.end() &&
           "No graphics queue family found!");

    auto graphicsIndex = static_cast<uint32_t>(std::distance(
        queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

    // query for Vulkan 1.3 features
    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain = {
            {}, // vk::PhysicalDeviceFeatures2
            {.dynamicRendering =
                 true}, // Enable dynamic rendering from Vulkan 1.3
            {.extendedDynamicState =
                 true} // Enable extended dynamic state from the extension
        };

    // create a Device
    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority};

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredDeviceExtension.size()),
        .ppEnabledExtensionNames = requiredDeviceExtension.data()};

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers)
      return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback};
    debugMessenger =
        instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
    // Check if the physicalDevice supports the Vulkan 1.3 API version
    bool supportsVulkan1_3 =
        physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

    // Check if any of the queue families support graphics operations
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics =
        std::ranges::any_of(queueFamilies, [](auto const &qfp) {
          return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
        });

    // Check if all required physicalDevice extensions are available
    auto availableDeviceExtensions =
        physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = std::ranges::all_of(
        requiredDeviceExtension,
        [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
          return std::ranges::any_of(
              availableDeviceExtensions,
              [requiredDeviceExtension](auto const &availableDeviceExtension) {
                return strcmp(availableDeviceExtension.extensionName,
                              requiredDeviceExtension) == 0;
              });
        });

    // Check if the physicalDevice supports the required features
    auto features = physicalDevice.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures =
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .dynamicRendering &&
        features
            .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;

    // Return true if the physicalDevice meets all the criteria
    return supportsVulkan1_3 && supportsGraphics &&
           supportsAllRequiredExtensions && supportsRequiredFeatures;
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL
  debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                vk::DebugUtilsMessageTypeFlagsEXT type,
                const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData) {
    std::cerr << "validation layer: type "
              << debugUtilsMessageTypeFlagsToString(type)
              << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  }

  // 辅助函数
  // vulkan.cppm 编译为模块时，ErrorCategoryImpl::message() 内部调用了 to_string
  // → toHexString → std::format → __allocating_buffer。
  // 模块 BMI 中这个析构函数没有被内联，变成了需要外部链接的符号，但 libc++.a
  // 里没有它（因为本该内联）。
  static std::string
  debugUtilsMessageTypeFlagsToString(vk::DebugUtilsMessageTypeFlagsEXT flags) {
    std::string result;
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
      result += "General|";
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
      result += "Validation|";
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
      result += "Performance|";
    if (flags & vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding)
      result += "DeviceAddressBinding|";
    if (!result.empty())
      result.pop_back(); // 去掉末尾的 '|'
    return result;
  }
};

int main() {
  // If any kind of fatal error occurs during execution, then we’ll throw a
  // std::runtime_error exception with a descriptive message, which will
  // propagate back to the main function and be printed to the command prompt
  try {
    HelloTriangleApplication app;
    app.run();
    // To handle a variety of standard exception types, as
    // well, we catch the more general std::exception.
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}