import vulkan; // provides the functions, structures and enumerations
import std;
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// clang-format off
#include <cstdlib>  // provides the EXIT_SUCCESS and EXIT_FAILURE macros
// clang-format on

// It’s a good idea to use constants because we’ll be referring to these values
// a couple of times in the future.
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

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
  }

  void createInstance() {
    // Get the required instance extensions from GLFW.
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Check if the required GLFW extensions are supported by the Vulkan
    // implementation.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      if (std::ranges::none_of(extensionProperties,
                               [glfwExtension = glfwExtensions[i]](
                                   auto const &extensionProperty) {
                                 return strcmp(extensionProperty.extensionName,
                                               glfwExtension) == 0;
                               })) {
        throw std::runtime_error("Required GLFW extension not supported: " +
                                 std::string(glfwExtensions[i]));
      }
    }

    // This data is technically optional, but it may provide some useful
    // information to the driver to optimize our specific application
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14};

    // A lot of information in Vulkan is passed through structs
    // not optional and tells the Vulkan driver which global extensions and
    // validation layers we want to use.
    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions};

    instance = vk::raii::Instance(context, createInfo);
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