#include <algorithm> // std::{clamp, ranges::find_if}
#include <chrono>    // std::chrono::{duration_cast, seconds, steady_clock, time_point}

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#include <DracUtils/Error.hpp>
#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace {
  fn cleanupSwapChain(const vk::Device device, Vec<vk::ImageView>& swapChainImageViews, const vk::CommandPool commandPool, Vec<vk::CommandBuffer>& commandBuffers) -> void {
    if (!commandBuffers.empty()) {
      device.freeCommandBuffers(commandPool, commandBuffers);
      commandBuffers.clear();
    }

    for (vk::ImageView imageView : swapChainImageViews)
      if (imageView)
        device.destroyImageView(imageView);

    swapChainImageViews.clear();
  }

  fn recreateSwapChain(GLFWwindow* window, vk::Device device, vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, vk::SwapchainKHR& swapChain, Vec<vk::Image>& swapChainImages, vk::SurfaceFormatKHR& surfaceFormat, vk::Extent2D& swapChainExtent, Vec<vk::ImageView>& swapChainImageViews, vk::CommandPool commandPool, Vec<vk::CommandBuffer>& commandBuffers, vk::PresentModeKHR& presentMode) -> Result<> {
    i32 width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    info_log("Recreating swapchain with dimensions: {}x{}", width, height);

    if (device.waitIdle() != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to wait for device idle before recreation!"));

    vk::SwapchainKHR oldSwapChain = swapChain;
    swapChain                     = VK_NULL_HANDLE;

    if (oldSwapChain)
      cleanupSwapChain(device, swapChainImageViews, commandPool, commandBuffers);

    swapChainImages.clear();

    vk::ResultValue<vk::SurfaceCapabilitiesKHR> capabilitiesResult = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    if (capabilitiesResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to get surface capabilities"));

    vk::SurfaceCapabilitiesKHR capabilities = capabilitiesResult.value;

    info_log("Surface capabilities - min: {}x{}, max: {}x{}, current: {}x{}", capabilities.minImageExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.width, capabilities.maxImageExtent.height, capabilities.currentExtent.width, capabilities.currentExtent.height);

    {
      using matchit::match, matchit::is, matchit::_;

      // clang-format off
      swapChainExtent = match(capabilities.currentExtent.width)(
        is | std::numeric_limits<u32>::max() = vk::Extent2D {
          std::clamp(static_cast<u32>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
          std::clamp(static_cast<u32>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        },
        is | _ = capabilities.currentExtent
      );
      // clang-format on
    }

    info_log("Using swapchain extent: {}x{}", swapChainExtent.width, swapChainExtent.height);

    vk::ResultValue<Vec<vk::SurfaceFormatKHR>> formatsResult = physicalDevice.getSurfaceFormatsKHR(surface);
    if (formatsResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to get surface formats"));

    surfaceFormat = formatsResult.value[0];

    vk::ResultValue<Vec<vk::PresentModeKHR>> presentModesResult = physicalDevice.getSurfacePresentModesKHR(surface);
    if (presentModesResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to get surface present modes"));

    presentMode = vk::PresentModeKHR::eFifo;
    for (const vk::PresentModeKHR& availablePresentMode : presentModesResult.value) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        presentMode = availablePresentMode;
        break;
      }
    }

    u32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
      imageCount = capabilities.maxImageCount;

    info_log("Using {} swapchain images", imageCount);

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.sType            = vk::StructureType::eSwapchainCreateInfoKHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform     = capabilities.currentTransform;
    createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = oldSwapChain;

    vk::ResultValue<vk::SwapchainKHR> swapChainResult = device.createSwapchainKHR(createInfo);
    if (swapChainResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to create swapchain!"));

    swapChain = swapChainResult.value;

    vk::ResultValue<Vec<vk::Image>> imagesResult = device.getSwapchainImagesKHR(swapChain);
    if (imagesResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to get swapchain images!"));

    swapChainImages = imagesResult.value;

    info_log("Created {} swapchain images", swapChainImages.size());

    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
      vk::ImageViewCreateInfo imageViewCreateInfo;
      imageViewCreateInfo.sType                           = vk::StructureType::eImageViewCreateInfo;
      imageViewCreateInfo.image                           = swapChainImages[i];
      imageViewCreateInfo.viewType                        = vk::ImageViewType::e2D;
      imageViewCreateInfo.format                          = surfaceFormat.format;
      imageViewCreateInfo.components.r                    = vk::ComponentSwizzle::eIdentity;
      imageViewCreateInfo.components.g                    = vk::ComponentSwizzle::eIdentity;
      imageViewCreateInfo.components.b                    = vk::ComponentSwizzle::eIdentity;
      imageViewCreateInfo.components.a                    = vk::ComponentSwizzle::eIdentity;
      imageViewCreateInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
      imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
      imageViewCreateInfo.subresourceRange.levelCount     = 1;
      imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
      imageViewCreateInfo.subresourceRange.layerCount     = 1;

      vk::ResultValue<vk::ImageView> imageViewResult = device.createImageView(imageViewCreateInfo);
      if (imageViewResult.result != vk::Result::eSuccess)
        return Err(DracError(Other, "failed to create image views!"));

      swapChainImageViews[i] = imageViewResult.value;
    }

    vk::ResultValue<Vec<vk::CommandBuffer>> buffersResult = device.allocateCommandBuffers({ commandPool, vk::CommandBufferLevel::ePrimary, static_cast<u32>(swapChainImageViews.size()) });
    if (buffersResult.result != vk::Result::eSuccess)
      return Err(DracError(Other, "failed to allocate command buffers!"));

    commandBuffers = buffersResult.value;

    info_log("Successfully recreated swapchain");

    return {};
  }
} // namespace

fn main() -> i32 {
  static vk::detail::DynamicLoader Loader;

  VULKAN_HPP_DEFAULT_DISPATCHER.init(Loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

  if (!glfwInit()) {
    error_log("Failed to initialize GLFW");
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan Example", nullptr, nullptr);

  if (!window) {
    error_log("Failed to create GLFW window");
    glfwTerminate();
    return EXIT_FAILURE;
  }

  bool framebufferWasResized = false;

  glfwSetWindowUserPointer(window, &framebufferWasResized);

  glfwSetFramebufferSizeCallback(window, [](GLFWwindow* windowInner, i32, i32) {
    bool* framebufferWasResizedInner = static_cast<bool*>(glfwGetWindowUserPointer(windowInner));
    *framebufferWasResizedInner      = true;
  });

  vk::ApplicationInfo appInfo("Vulkan Example", 1, "Draconis++ Example", 1, VK_API_VERSION_1_3);

  u32   glfwExtensionCount = 0;
  CStr* glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  Vec<CStr> extensions;
  extensions.reserve(glfwExtensionCount);
  for (Span<CStr> glfwExts(glfwExtensions, glfwExtensionCount); CStr ext : glfwExts)
    extensions.push_back(ext);

#ifdef __APPLE__
  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

  vk::InstanceCreateInfo createInfo;
  createInfo.pApplicationInfo = &appInfo;
#ifdef __APPLE__
  createInfo.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif
  createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  vk::ResultValue<vk::Instance> instanceResult = vk::createInstance(createInfo);

  if (instanceResult.result != vk::Result::eSuccess) {
    error_log("Failed to create Vulkan instance: {}", vk::to_string(instanceResult.result));
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::Instance instance = instanceResult.value;

  VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

  info_log("Vulkan instance created.");

  vk::SurfaceKHR surface;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  if (glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
    error_log("Failed to create window surface!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::ResultValue<Vec<vk::PhysicalDevice>> physicalDevicesResult = instance.enumeratePhysicalDevices();
  if (physicalDevicesResult.result != vk::Result::eSuccess) {
    error_log("Failed to find GPUs with Vulkan support!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::PhysicalDevice physicalDevice = physicalDevicesResult.value.front();

  Vec<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

  const auto graphicsQueueFamilyIndex = std::ranges::distance(queueFamilyProperties.begin(), std::ranges::find_if(queueFamilyProperties, [](const vk::QueueFamilyProperties& qfp) {
                                                                return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
                                                              }));

  f32 queuePriority = 1.0F;

  vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), static_cast<u32>(graphicsQueueFamilyIndex), 1, &queuePriority);

  const Vec<CStr> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
  };

  vk::DeviceCreateInfo deviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo);
  deviceCreateInfo.enabledExtensionCount   = static_cast<u32>(deviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

  vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature;
  dynamicRenderingFeature.dynamicRendering = VK_TRUE;
  deviceCreateInfo.pNext                   = &dynamicRenderingFeature;

  vk::ResultValue<vk::Device> deviceResult = physicalDevice.createDevice(deviceCreateInfo);

  if (deviceResult.result != vk::Result::eSuccess) {
    error_log("Failed to create logical device!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::Device device = deviceResult.value;
  VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

  vk::SwapchainKHR     swapChain;
  Vec<vk::Image>       swapChainImages;
  vk::SurfaceFormatKHR surfaceFormat;
  vk::Extent2D         swapChainExtent;
  Vec<vk::ImageView>   swapChainImageViews;
  vk::PresentModeKHR   presentMode = vk::PresentModeKHR::eFifo;

  vk::ResultValue<vk::CommandPool> poolResult = device.createCommandPool({ {}, static_cast<u32>(graphicsQueueFamilyIndex) });

  if (poolResult.result != vk::Result::eSuccess) {
    error_log("Failed to create command pool!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::CommandPool commandPool = poolResult.value;

  Vec<vk::CommandBuffer> commandBuffers;

  if (Result<> result = recreateSwapChain(window, device, physicalDevice, surface, swapChain, swapChainImages, surfaceFormat, swapChainExtent, swapChainImageViews, commandPool, commandBuffers, presentMode); !result) {
    error_log("Failed to recreate swap chain! {}", result.error().message);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  vk::Queue graphicsQueue = device.getQueue(static_cast<u32>(graphicsQueueFamilyIndex), 0);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& imguiIO = ImGui::GetIO();
  (void)imguiIO;
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(window, true);
  ImGui_ImplVulkan_InitInfo initInfo = {};
  initInfo.Instance                  = static_cast<VkInstance>(instance);
  initInfo.PhysicalDevice            = static_cast<VkPhysicalDevice>(physicalDevice);
  initInfo.Device                    = static_cast<VkDevice>(device);
  initInfo.QueueFamily               = static_cast<u32>(graphicsQueueFamilyIndex);
  initInfo.Queue                     = static_cast<VkQueue>(graphicsQueue);
  initInfo.PipelineCache             = VK_NULL_HANDLE;
  initInfo.UseDynamicRendering       = VK_TRUE;
  initInfo.Allocator                 = nullptr;
  initInfo.MinImageCount             = 2;
  initInfo.ImageCount                = static_cast<u32>(swapChainImages.size());
  initInfo.CheckVkResultFn           = nullptr;

  vk::PipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {};
  pipelineRenderingCreateInfo.sType                              = vk::StructureType::ePipelineRenderingCreateInfoKHR;
  pipelineRenderingCreateInfo.colorAttachmentCount               = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats            = &surfaceFormat.format;
  initInfo.PipelineRenderingCreateInfo                           = pipelineRenderingCreateInfo;

  // clang-format off
  Array<vk::DescriptorPoolSize, 11> poolSizes = {{
    { vk::DescriptorType::eCombinedImageSampler, 1000 },
    { vk::DescriptorType::eInputAttachment, 1000 },
    { vk::DescriptorType::eSampledImage, 1000 },
    { vk::DescriptorType::eSampler, 1000 },
    { vk::DescriptorType::eStorageBuffer, 1000 },
    { vk::DescriptorType::eStorageBufferDynamic, 1000 },
    { vk::DescriptorType::eStorageImage, 1000 },
    { vk::DescriptorType::eStorageTexelBuffer, 1000 },
    { vk::DescriptorType::eUniformBuffer, 1000 },
    { vk::DescriptorType::eUniformBufferDynamic, 1000 },
    { vk::DescriptorType::eUniformTexelBuffer, 1000 },
  }};
  // clang-format on

  vk::DescriptorPoolCreateInfo poolInfo = {};
  poolInfo.flags                        = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
  poolInfo.maxSets                      = 1000 * poolSizes.size();
  poolInfo.poolSizeCount                = static_cast<u32>(poolSizes.size());
  poolInfo.pPoolSizes                   = poolSizes.data();

  vk::ResultValue<vk::DescriptorPool> imguiPoolResult = device.createDescriptorPool(poolInfo);
  if (imguiPoolResult.result != vk::Result::eSuccess) {
    error_log("Failed to create imgui descriptor pool!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  initInfo.DescriptorPool = imguiPoolResult.value;

  ImGui_ImplVulkan_LoadFunctions([](const CStr function_name, void* vulkan_instance) {
    return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(static_cast<VkInstance>(vulkan_instance), function_name);
  });

  ImGui_ImplVulkan_Init(&initInfo);
  ImGui_ImplVulkan_CreateFontsTexture();

  std::chrono::time_point lastUpdateTime = std::chrono::steady_clock::now();
  Result<String>          host;
  Result<String>          kernelVersion;
  Result<String>          osVersion;
  Result<String>          cpuModel;
  Result<String>          gpuModel;
  Result<ResourceUsage>   memInfo;
  Result<String>          desktopEnv;
  Result<String>          windowMgr;
  Result<ResourceUsage>   diskUsage;
  Result<String>          shell;
#if DRAC_ENABLE_PACKAGECOUNT
  Result<u64> packageCount;
#endif
#if DRAC_ENABLE_NOWPLAYING
  Result<MediaInfo> nowPlaying;
#endif

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if (const std::chrono::time_point now = std::chrono::steady_clock::now();
        std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdateTime).count() >= 1) {
      using draconis::core::system::System;
      using namespace draconis::services::packages;

      host          = System::getHost();
      kernelVersion = System::getKernelVersion();
      osVersion     = System::getOSVersion();
      cpuModel      = System::getCPUModel();
      gpuModel      = System::getGPUModel();
      memInfo       = System::getMemInfo();
      desktopEnv    = System::getDesktopEnvironment();
      windowMgr     = System::getWindowManager();
      diskUsage     = System::getDiskUsage();
      shell         = System::getShell();
#if DRAC_ENABLE_PACKAGECOUNT
      packageCount = GetTotalCount(Manager::CARGO);
#endif
#if DRAC_ENABLE_NOWPLAYING
      nowPlaying = System::getNowPlaying();
#endif
      lastUpdateTime = now;
    }

    if (framebufferWasResized) {
      if (Result<> result = recreateSwapChain(
            window,
            device,
            physicalDevice,
            surface,
            swapChain,
            swapChainImages,
            surfaceFormat,
            swapChainExtent,
            swapChainImageViews,
            commandPool,
            commandBuffers,
            presentMode
          );
          !result) {
        error_log("Failed to recreate swap chain! {}", result.error().message);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
      }
      framebufferWasResized = false;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Draconis++");
    {
      ImGui::TextUnformatted(std::format("Host: {}", host.value_or("N/A")).c_str());
      ImGui::TextUnformatted(std::format("Kernel: {}", kernelVersion.value_or("N/A")).c_str());
      ImGui::TextUnformatted(std::format("OS: {}", osVersion.value_or("N/A")).c_str());
      ImGui::TextUnformatted(std::format("CPU: {}", cpuModel.value_or("N/A")).c_str());
      ImGui::TextUnformatted(std::format("GPU: {}", gpuModel.value_or("N/A")).c_str());

      if (memInfo.has_value())
        ImGui::TextUnformatted(std::format("Memory: {} / {}", BytesToGiB(memInfo->usedBytes), BytesToGiB(memInfo->totalBytes)).c_str());
      else
        ImGui::TextUnformatted("Memory: N/A");

      ImGui::TextUnformatted(std::format("DE: {}", desktopEnv.value_or("N/A")).c_str());
      ImGui::TextUnformatted(std::format("WM: {}", windowMgr.value_or("N/A")).c_str());

      if (diskUsage.has_value())
        ImGui::TextUnformatted(std::format("Disk: {} / {}", BytesToGiB(diskUsage->usedBytes), BytesToGiB(diskUsage->totalBytes)).c_str());
      else
        ImGui::TextUnformatted("Disk: N/A");

      ImGui::TextUnformatted(std::format("Shell: {}", shell.value_or("N/A")).c_str());

#if DRAC_ENABLE_PACKAGECOUNT
      ImGui::TextUnformatted(std::format("Packages: {}", packageCount.value_or(0)).c_str());
#endif

#if DRAC_ENABLE_NOWPLAYING
      if (nowPlaying) {
        const MediaInfo& nowPlayingInfo = *nowPlaying;
        ImGui::TextUnformatted(std::format("Now Playing: {} - {}", nowPlayingInfo.artist.value_or("N/A"), nowPlayingInfo.title.value_or("N/A")).c_str());
      } else
        ImGui::TextUnformatted("Now Playing: N/A");
#endif
    }
    ImGui::End();

    ImGui::Begin("Vulkan & GLFW Info");
    {
      ImGui::TextUnformatted(std::format("FPS: {:.1f}", ImGui::GetIO().Framerate).c_str());
      ImGui::Separator();
      const vk::PhysicalDeviceProperties props = physicalDevice.getProperties();
      ImGui::TextUnformatted(std::format("GLFW Version: {}", glfwGetVersionString()).c_str());
      ImGui::Separator();
      ImGui::TextUnformatted(std::format("Vulkan API Version: {}.{}.{}", VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion), VK_API_VERSION_PATCH(props.apiVersion)).c_str());
      ImGui::TextUnformatted(std::format("Device: {}", props.deviceName.data()).c_str());
      ImGui::TextUnformatted(std::format("Driver Version: {}", props.driverVersion).c_str());
      ImGui::Separator();
      ImGui::TextUnformatted(std::format("Swapchain Extent: {}x{}", swapChainExtent.width, swapChainExtent.height).c_str());
      ImGui::TextUnformatted(std::format("Swapchain Images: {}", swapChainImages.size()).c_str());
      ImGui::TextUnformatted(std::format("Surface Format: {}", vk::to_string(surfaceFormat.format)).c_str());
      ImGui::TextUnformatted(std::format("Color Space: {}", vk::to_string(surfaceFormat.colorSpace)).c_str());
      ImGui::TextUnformatted(std::format("Present Mode: {}", vk::to_string(presentMode)).c_str());
    }
    ImGui::End();

    ImGui::Render();

    vk::ResultValue<u32> acquireResult = device.acquireNextImageKHR(swapChain, std::numeric_limits<u64>::max(), VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (acquireResult.result == vk::Result::eErrorOutOfDateKHR) {
      if (Result<> result = recreateSwapChain(
            window,
            device,
            physicalDevice,
            surface,
            swapChain,
            swapChainImages,
            surfaceFormat,
            swapChainExtent,
            swapChainImageViews,
            commandPool,
            commandBuffers,
            presentMode
          );
          !result) {
        error_log("Failed to recreate swap chain! {}", result.error().message);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
      }
      continue;
    }

    if (acquireResult.result != vk::Result::eSuccess && acquireResult.result != vk::Result::eSuboptimalKHR) {
      error_log("Failed to acquire swap chain image!");
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }

    u32 imageIndex = acquireResult.value;

    if (commandBuffers[imageIndex].begin(vk::CommandBufferBeginInfo()) != vk::Result::eSuccess) {
      error_log("Failed to begin command buffer!");
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }

    vk::ClearValue clearColor(std::array<float, 4> { 0.1F, 0.1F, 0.1F, 1.0F });

    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.imageView   = swapChainImageViews[imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue  = clearColor;

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea           = vk::Rect2D({ 0, 0 }, swapChainExtent);
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;

    commandBuffers[imageIndex].beginRendering(renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[imageIndex]);
    commandBuffers[imageIndex].endRendering();

    if (commandBuffers[imageIndex].end() != vk::Result::eSuccess) {
      error_log("Failed to end command buffer!");
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }

    vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffers[imageIndex]);
    if (vk::Result submitResult = graphicsQueue.submit(submitInfo, nullptr); submitResult != vk::Result::eSuccess) {
      error_log("Failed to submit draw command buffer!");
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }

    vk::PresentInfoKHR presentInfo;
    presentInfo.sType          = vk::StructureType::ePresentInfoKHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains    = &swapChain;
    presentInfo.pImageIndices  = &imageIndex;

    if (vk::Result presentResult = graphicsQueue.presentKHR(presentInfo); presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
      if (Result<> result = recreateSwapChain(
            window,
            device,
            physicalDevice,
            surface,
            swapChain,
            swapChainImages,
            surfaceFormat,
            swapChainExtent,
            swapChainImageViews,
            commandPool,
            commandBuffers,
            presentMode
          );
          !result) {
        error_log("Failed to recreate swap chain! {}", result.error().message);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
      }
    } else if (presentResult != vk::Result::eSuccess) {
      error_log("Unexpected present result: {}", vk::to_string(presentResult));
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }

    if (graphicsQueue.waitIdle() != vk::Result::eSuccess) {
      error_log("Failed to wait for graphics queue idle!");
      glfwDestroyWindow(window);
      glfwTerminate();
      return EXIT_FAILURE;
    }
  }

  if (device.waitIdle() != vk::Result::eSuccess) {
    error_log("Failed to wait for device idle!");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  cleanupSwapChain(device, swapChainImageViews, commandPool, commandBuffers);
  device.destroyDescriptorPool(imguiPoolResult.value);
  device.freeCommandBuffers(commandPool, commandBuffers);
  device.destroyCommandPool(commandPool);
  device.destroy();
  instance.destroySurfaceKHR(surface);
  instance.destroy();

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
