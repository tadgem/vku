#define VMA_IMPLEMENTATION
#include "ThirdParty/stb_image.h"
#include "vku/Init.h"
#include "vku/Macros.h"
#include "vku/RenderPass.h"
#include "vku/Texture.h"
#include "vku/Utils.h"
#include "vku/Log.h"
#include "ThirdParty/FunnelSansTTF.h"
#define NANOVG_VULKAN_IMPLEMENTATION
#include "ThirdParty/nanovg_vk.h"
#include "vku/Commands.h"


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
  pUserData;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    VKU_LOG_WARN("VL: %ull : %s", messageType, pCallbackData->pMessage);
  }
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
  {
    VKU_LOG_INFO("VL: %ull : %s",  messageType, pCallbackData->pMessage);
  }
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
  {
    VKU_LOG_ERR("VL: %ull :  %s",  messageType, pCallbackData->pMessage);
  }
  return VK_FALSE;
}

void vku::init::Cleanup(VkState& vk)
{
  Texture::FreeDefaultTexture(vk);
  Mesh::FreeBuiltInMeshes(vk);
  CleanupImGui(vk);
  CleanupNanoVG(vk);
  vk.m_Backend->CleanupWindow(vk);
  CleanupVulkan(vk);
}

void vku::init::CleanupImGui(VkState& vk)
{
  if (vk.m_UseImGui)
  {
    // TODO: destroy with other built in render passes
    // vkDestroyRenderPass(m_LogicalDevice, m_ImGuiRenderPass, nullptr);
    ImGui_ImplVulkan_Shutdown();
    vk.m_Backend->CleanupImGuiBackend(vk);
  }
}

void vku::init::CleanupNanoVG(VkState& vk)
{
    nvgDeleteVk(vk.m_NanoVG);
}

bool vku::init::CheckValidationLayerSupport(VkState& vk)
{
  uint32_t layerCount;
  if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
  {
    VKU_LOG_ERR("Failed to enumerate supported validation layers!");
  }

  STLAllocator<VkLayerProperties>alloc(*vk.m_CPUAllocator);
  Vector<VkLayerProperties> availableLayers(alloc);
  availableLayers.resize(layerCount);
  if (vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()) != VK_SUCCESS)
  {
      VKU_LOG_ERR("Failed to enumerate supported validation layers!");
  }

  for (const char* layerName : s_ValidationLayers) {
    bool layerFound = false;

    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
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

bool vku::init::CheckDeviceExtensionSupport(VkState& vk, VkPhysicalDevice device)
{
  Vector<VkExtensionProperties> availableExtensions =
      GetDeviceAvailableExtensions(*vk.m_CPUAllocator, device);

  uint32_t requiredExtensionsFound = 0;


  for (auto const& extension : availableExtensions)
  {
    for (auto const& requiredExtensionName : s_RequiredDeviceExtensions)
    {
      if (strcmp(requiredExtensionName, extension.extensionName) == 0)
      {
        requiredExtensionsFound++;
      }
    }

    for(auto const& desiredExtensionName : vk.m_DesiredDeviceExtensions)
    {
      if(strcmp(desiredExtensionName, extension.extensionName) == 0)
      {
        requiredExtensionsFound++;
      }
    }
  }

  auto requiredCount =
      s_RequiredDeviceExtensions.size() + vk.m_DesiredDeviceExtensions.size();

  return requiredExtensionsFound >= requiredCount;

}

void vku::init::SetupDebugOutput(VkState& vk)
{
  if (!vk.m_UseValidation) return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  PopulateDebugMessengerCreateInfo(createInfo);

  PFN_vkVoidFunction rawFunction = vkGetInstanceProcAddr(vk.m_Instance, "vkCreateDebugUtilsMessengerEXT");
  PFN_vkCreateDebugUtilsMessengerEXT function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(rawFunction);

  if (!function)
  {
    VKU_LOG_ERR("Could not find vkCreateDebugUtilsMessengerEXT function");
    return;
  }

  if (function(vk.m_Instance, &createInfo, nullptr, &vk.m_DebugMessenger))
  {
    VKU_LOG_ERR("Failed to create debug messenger");
  }

}

void vku::init::CleanupDebugOutput(VkState& vk)
{
  PFN_vkVoidFunction rawFunction = vkGetInstanceProcAddr(vk.m_Instance, "vkDestroyDebugUtilsMessengerEXT");
  auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(rawFunction);

  if (function != nullptr) {
    function(vk.m_Instance, vk.m_DebugMessenger, nullptr);
  }
  else
  {
    VKU_LOG_ERR("Could not find vkDestroyDebugUtilsMessengerEXT function");
  }
}

void vku::init::ListDeviceExtensions(VkState& vk, VkPhysicalDevice physicalDevice)
{
  Vector<VkExtensionProperties> extensions =
      GetDeviceAvailableExtensions(*vk.m_CPUAllocator, physicalDevice);

  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  VKU_LOG_INFO("Selected device %s supports the following extensions:", &deviceProperties.deviceName[0]);
  for (int i = 0; i < extensions.size(); i++)
  {
    VKU_LOG_INFO("    - %s", &extensions[i].extensionName[0]);
  }
}

void vku::init::PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  createInfo.pfnUserCallback = debugCallback;
  // TODO: Find a way to quit on a debug error
}

void vku::init::InitVulkan(VkState& vk, bool enableSwapchainMsaa)
{
  vk.m_CurrentFrameIndex = 0;
  vk.m_UseSwapchainMsaa = enableSwapchainMsaa;
  VK_CHECK(volkInitialize());
  CreateInstance(vk);
  SetupDebugOutput(vk);
  vk.m_Backend->CreateSurface(vk);
  PickPhysicalDevice(vk);
  CreateLogicalDevice(vk);
  GetMaxUsableSampleCount(vk);
  GetQueueHandles(vk);
  CreateCommandPool(vk);
  CreateSwapChain(vk);
  CreateSwapChainImageViews(vk);
  CreateSwapChainDepthTexture(vk, vk.m_UseSwapchainMsaa);
  CreateSwapChainColourTexture(vk, vk.m_UseSwapchainMsaa);
  CreateBuiltInRenderPasses(vk);
  CreateSwapChainFramebuffers(vk);
  CreateDescriptorSetAllocator(vk);
  CreateSemaphores(vk);
  CreateFences(vk);
  CreateCommandBuffers(vk);
  CreateVmaAllocator(vk);
}

void SetImGuiStyle() {
    auto& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button] = ImVec4(0.29f, 0.29f, 0.29f, 0.62f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.29f, 0.29f, 0.72f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.29f, 0.29f, 0.23f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 2;
    style.ChildBorderSize = 2;
    style.PopupBorderSize = 2;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 1;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}


void vku::init::InitImGui(VkState& vk)
{
  if (!vk.m_UseImGui)
  {
    return;
  }
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  auto style = ImGui::GetStyle();
  style.Colors[ImGuiCol_FrameBg].w = 1.0f;
  style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  style.Colors[ImGuiCol_ChildBg].w = 1.0f;

  // TODO : Reinit imgui backend on backend refactor
  vk.m_Backend->InitImGuiBackend(vk);
  ImGui_ImplVulkan_InitInfo init_info = {};

  init_info.Instance = vk.m_Instance;
  init_info.PhysicalDevice = vk.m_PhysicalDevice;
  init_info.Device = vk.m_LogicalDevice;
  init_info.QueueFamily = vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute];
  init_info.Queue = vk.m_GraphicsQueue;
  init_info.PipelineCache = VK_NULL_HANDLE;
  // TODO: this is a bit shit, need to clean this pool some wheere
  init_info.DescriptorPool = vk.m_DescriptorSetAllocator.CreatePool(*vk.m_CPUAllocator, vk.m_LogicalDevice, 4096);
  init_info.Allocator = nullptr;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 2;
  init_info.RenderPass = vk.m_ImGuiRenderPass;
  if (vk.m_UseSwapchainMsaa)
  {
    init_info.MSAASamples = vk.m_MaxMsaaSamples;
  }
  ImGui_ImplVulkan_Init(&init_info);

  ImGui_ImplVulkan_CreateFontsTexture();

  SetImGuiStyle();
}

void vku::init::InitNanoVG(VkState& vk)
{
    VKNVGCreateInfo nvgCreateInfo = { 0 };
    nvgCreateInfo.device = vk.m_LogicalDevice;
    nvgCreateInfo.gpu = vk.m_PhysicalDevice;
    nvgCreateInfo.renderpass = vk.m_SwapchainImageRenderPass;
    nvgCreateInfo.cmdBuffer = vk.m_GraphicsCommandBuffers.data();
    nvgCreateInfo.swapchainImageCount = static_cast<uint32_t>(vk.m_SwapChainImages.size());
    nvgCreateInfo.currentFrame = &vk.m_CurrentFrameIndex;

    nvgCreateInfo.ext.colorBlendEquation = true;
    nvgCreateInfo.ext.colorWriteMask = true;
    nvgCreateInfo.ext.dynamicState = true;
    nvgCreateInfo.ext.sampleCount = vk.m_UseSwapchainMsaa ? vk.m_MaxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    int flags = 0;
    flags |= NVG_ANTIALIAS;
    flags |= NVG_STENCIL_STROKES;

    vk.m_NanoVG = nvgCreateVk(nvgCreateInfo, flags, vk.m_GraphicsQueue);
    

    
}

VkApplicationInfo vku::init::CreateAppInfo() {
  VkApplicationInfo appInfo{};
  // each struct needs to explicitly be told its type
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan Example SDL";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "None";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  return appInfo;
}

void vku::init::CreateInstance(VkState& vk)
{
  if (vk.m_UseValidation && !CheckValidationLayerSupport(vk))
  {
    VKU_LOG_ERR("Validation layers requested but not available.");
  }

  VkApplicationInfo appInfo = CreateAppInfo();

  Vector<const char*> extensionNames = vk.m_Backend->GetRequiredInstanceExtensions(vk);

  if(vk.m_UseValidation) {
    for (const auto &extension: extensionNames) {
      VKU_LOG_INFO("Backend Extension : %s", extension);
    }
  }
  uint32_t extensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  STLAllocator<VkExtensionProperties> alloc(*vk.m_CPUAllocator);
  Vector<VkExtensionProperties> extensions(alloc);
  extensions.resize(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

  if(vk.m_UseValidation) {
    VKU_LOG_INFO("Supported m_Instance extensions: ");
  }
  for (const auto& extension : extensions) {
    if (vk.m_UseValidation) {
      VKU_LOG_INFO("   - %s", &extension.extensionName[0]);
    }
    bool shouldAdd = true;
    for (int i = 0; i < extensionNames.size(); i++)
    {
      if (extensionNames[i] == extension.extensionName)
      {
        shouldAdd = false;
      }
    }

    if (shouldAdd && extension.extensionName != NULL)
    {
      extensionNames.push_back(extension.extensionName);
    }

  }

  // setup an m_Instance create info with our extensions & app info to create a vulkan m_Instance
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
  createInfo.ppEnabledExtensionNames = extensionNames.data();

  if (vk.m_UseValidation)
  {
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
    createInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
    createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

    PopulateDebugMessengerCreateInfo(debugMessengerCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugMessengerCreateInfo;
  }
  else
  {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &vk.m_Instance) != VK_SUCCESS)
  {
    VKU_LOG_ERR("Failed to create vulkan m_Instance");
  }

  volkLoadInstance(vk.m_Instance);

}

void vku::init::CleanupVulkan(VkState& vk)
{
  vmaDestroyAllocator(vk.m_Allocator);
  CleanupSwapChain(vk);

  vk.m_DescriptorSetAllocator.Free(vk.m_LogicalDevice);

  if (vk.m_UseValidation)
  {
    CleanupDebugOutput(vk);
  }
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(vk.m_LogicalDevice, vk.m_ImageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(vk.m_LogicalDevice, vk.m_RenderFinishedSemaphores[i], nullptr);
    vkDestroyFence(vk.m_LogicalDevice, vk.m_FrameInFlightFences[i], nullptr);
  }

  vkDestroyCommandPool(vk.m_LogicalDevice, vk.m_GraphicsComputeQueueCommandPool, nullptr);
  vkDestroyRenderPass(vk.m_LogicalDevice, vk.m_SwapchainImageRenderPass, nullptr);


  vkDestroySurfaceKHR(vk.m_Instance, vk.m_Surface, nullptr);
  vkDestroyDevice(vk.m_LogicalDevice, nullptr);
  vkDestroyInstance(vk.m_Instance, nullptr);
}

vku::QueueFamilyIndices vku::init::FindQueueFamilies(VkState& vk, VkPhysicalDevice m_PhysicalDevice)
{
  QueueFamilyIndices indices (*vk.m_CPUAllocator);

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

  STLAllocator<VkQueueFamilyProperties> alloc(*vk.m_CPUAllocator);
  Vector<VkQueueFamilyProperties> queueFamilyProperties(alloc);
  queueFamilyProperties.resize(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilyProperties.data());

  for (int i = 0; i < queueFamilyProperties.size(); i++)
  {
    if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
      indices.m_QueueFamilies.emplace(QueueFamilyType::GraphicsAndCompute, i);
    }

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, vk.m_Surface, &presentSupport);

    if (presentSupport == VK_TRUE) {
      indices.m_QueueFamilies[QueueFamilyType::Present] = i;
    }
  }
  return indices;
}

vku::SwapChainSupportDetais vku::init::GetSwapChainSupportDetails(VkState& vk, VkPhysicalDevice physicalDevice)
{
  SwapChainSupportDetais details(*vk.m_CPUAllocator);

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, vk.m_Surface, &details.m_Capabilities);

  uint32_t supportedNumberFormats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vk.m_Surface, &supportedNumberFormats, nullptr);

  if (supportedNumberFormats > 0)
  {
    details.m_SupportedFormats.resize(supportedNumberFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vk.m_Surface, &supportedNumberFormats, details.m_SupportedFormats.data());
  }

  uint32_t supportedPresentModes = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vk.m_Surface, &supportedPresentModes, nullptr);

  if (supportedNumberFormats > 0)
  {
    details.m_SupportedPresentModes.resize(supportedPresentModes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vk.m_Surface, &supportedPresentModes, details.m_SupportedPresentModes.data());
  }

  return details;
}

bool vku::init::IsDeviceSuitable(VkState& vk,VkPhysicalDevice physicalDevice)
{
  QueueFamilyIndices indices = FindQueueFamilies(vk, physicalDevice);
  bool extensionsSupported = CheckDeviceExtensionSupport(vk, physicalDevice);
  bool swapChainSupport = false;
  if (extensionsSupported)
  {
    SwapChainSupportDetais swapChainDetails = GetSwapChainSupportDetails(vk, physicalDevice);
    swapChainSupport = swapChainDetails.m_SupportedFormats.size() > 0 && swapChainDetails.m_SupportedPresentModes.size() > 0;
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

  return indices.IsComplete() &&
          extensionsSupported &&
          swapChainSupport &&
          supportedFeatures.samplerAnisotropy &&
          supportedFeatures.wideLines &&
          supportedFeatures.independentBlend;
}

uint32_t vku::init::AssessDeviceSuitability(VkPhysicalDevice m_PhysicalDevice) {
  uint32_t score = 0;

  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);

  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &deviceFeatures);

  if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
  {
    score += 1000;
  }

  score += deviceFeatures.geometryShader;
  score += deviceFeatures.shaderStorageImageMultisample;
  score += deviceFeatures.multiViewport;
  score += deviceFeatures.wideLines;
  score += deviceFeatures.independentBlend;

  return score;
}

void vku::init::PickPhysicalDevice(VkState& vk)
{
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(vk.m_Instance, &deviceCount, nullptr);

  if (deviceCount == 0)
  {
    VKU_LOG_ERR("Failed to find any physical devices.");
  }

  STLAllocator<VkPhysicalDevice>alloc(*vk.m_CPUAllocator);

  Vector<VkPhysicalDevice> physicalDevices(alloc);
  physicalDevices.resize(deviceCount);
  vkEnumeratePhysicalDevices(vk.m_Instance, &deviceCount, physicalDevices.data());
  VkPhysicalDevice physicalDeviceCandidate = VK_NULL_HANDLE;
  uint32_t bestScore = 0;

  for (int i = 0; i < physicalDevices.size(); i++)
  {
    uint32_t score = AssessDeviceSuitability(physicalDevices[i]);
    if (IsDeviceSuitable(vk, physicalDevices[i]) && score > bestScore)
    {
      physicalDeviceCandidate = physicalDevices[i];
      bestScore = score;
    }
  }

  if (bestScore == 0)
  {
    VKU_LOG_ERR("failed to find a suitable device.");
    return;
  }

  vk.m_PhysicalDevice = physicalDeviceCandidate;

  if (vk.m_PhysicalDevice == VK_NULL_HANDLE)
  {
    VKU_LOG_ERR("Failed to find a suitable physical device.");
    return;
  }

  VkPhysicalDeviceProperties deviceProperties{};
  vkGetPhysicalDeviceProperties(vk.m_PhysicalDevice, &deviceProperties);

  if(vk.m_UseValidation) {
    VKU_LOG_INFO("Chose GPU : %s", &deviceProperties.deviceName[0]);
    ListDeviceExtensions(vk, vk.m_PhysicalDevice);
  }
}

void vku::init::CreateLogicalDevice(VkState& vk)
{
  vk.m_QueueFamilyIndices = FindQueueFamilies(vk, vk.m_PhysicalDevice);

  STLAllocator< VkDeviceQueueCreateInfo> alloc(*vk.m_CPUAllocator);
  Vector< VkDeviceQueueCreateInfo> queueCreateInfos(alloc);
  float priority = 1.0f;
  for (auto const& [type, index] : vk.m_QueueFamilyIndices.m_QueueFamilies)
  {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.queueFamilyIndex = index;
    queueCreateInfo.pQueuePriorities = &priority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures physicalDeviceFeatures{};
  physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
  physicalDeviceFeatures.sampleRateShading = VK_TRUE;
  physicalDeviceFeatures.fillModeNonSolid = VK_TRUE;
  physicalDeviceFeatures.wideLines = VK_TRUE;
  physicalDeviceFeatures.independentBlend = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos        = queueCreateInfos.data();
  createInfo.queueCreateInfoCount     = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pEnabledFeatures         = &physicalDeviceFeatures;

  auto extensions = s_RequiredDeviceExtensions;
  extensions.push_back("VK_KHR_dynamic_rendering");
  extensions.push_back("VK_KHR_synchronization2");
  extensions.push_back("VK_EXT_extended_dynamic_state");
  extensions.push_back("VK_EXT_extended_dynamic_state3");

  createInfo.enabledExtensionCount    = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames  = extensions.data();

  if (vk.m_UseValidation)
  {
    createInfo.enabledLayerCount    = static_cast<uint32_t>(s_ValidationLayers.size());
    createInfo.ppEnabledLayerNames  = s_ValidationLayers.data();
  }
  else {
    createInfo.enabledLayerCount = 0;
  }
  VkPhysicalDeviceSynchronization2FeaturesKHR sync2Extension {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
      nullptr,
      VK_TRUE,
  };

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingExtension {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
      static_cast<void*>(&sync2Extension),
      VK_TRUE,
  };

  createInfo.pNext = &dynamicRenderingExtension;

  if (vkCreateDevice(vk.m_PhysicalDevice, &createInfo, nullptr, &vk.m_LogicalDevice))
  {
    VKU_LOG_ERR("Failed to create logical device.");
    return;
  }

  volkLoadDevice(vk.m_LogicalDevice);
}

void vku::init::GetQueueHandles(VkState& vk)
{
  vkGetDeviceQueue(vk.m_LogicalDevice, vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute],    0, &vk.m_GraphicsQueue);
  vkGetDeviceQueue(vk.m_LogicalDevice, vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute],    0, &vk.m_ComputeQueue);
  vkGetDeviceQueue(vk.m_LogicalDevice, vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::Present],               0, &vk.m_PresentQueue);
}

VkSurfaceFormatKHR vku::init::ChooseSwapChainSurfaceFormat(
    Vector<VkSurfaceFormatKHR> availableFormats) {
  if (availableFormats.size() == 0)
  {
    VKU_LOG_ERR("Could not find any suitable Swapchain Surface Format in provided collection!");
  }

  for (auto const& format : availableFormats)
  {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM )
    {
      return format;
    }
  }

  VKU_LOG_ERR("Could not find suitable Swapchain Surface Format in provided collection!");
  return availableFormats[0];
}

VkPresentModeKHR vku::init::ChooseSwapChainPresentMode(VkState& vk, Vector<VkPresentModeKHR> availableModes)
{
  if(vk.m_WaitForVerticalSync)
  {
    return VK_PRESENT_MODE_FIFO_KHR;
  }
  for (auto const& presentMode : availableModes)
  {
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      return presentMode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

void vku::init::CreateSwapChain(VkState& vk)
{
  SwapChainSupportDetais swapChainDetails = GetSwapChainSupportDetails(vk, vk.m_PhysicalDevice);

  VkSurfaceFormatKHR format       =
      ChooseSwapChainSurfaceFormat(swapChainDetails.m_SupportedFormats);
  VkPresentModeKHR presentMode    = ChooseSwapChainPresentMode(vk, swapChainDetails.m_SupportedPresentModes);
  VkExtent2D surfaceExtent        = ChooseSwapExtent(vk, swapChainDetails.m_Capabilities);
  // request one more than minimum supported number of images in swap chain
  // from vk-tutorial : "we may sometimes have to wait on the driver to complete
  // internal operations before we can acquire another image to render to.
  // Therefore it is recommended to request at least one more image than the minimum"
  uint32_t swapChainImageCount = swapChainDetails.m_Capabilities.minImageCount;
  if (swapChainImageCount > swapChainDetails.m_Capabilities.maxImageCount)
  {
    swapChainImageCount = swapChainDetails.m_Capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface          = vk.m_Surface;
  createInfo.minImageCount    = swapChainImageCount;
  createInfo.clipped          = VK_TRUE;
  createInfo.imageFormat      = format.format;
  createInfo.imageColorSpace  = format.colorSpace;
  createInfo.presentMode      = presentMode;
  createInfo.imageExtent      = surfaceExtent;
  // This is always 1 unless you are developing a stereoscopic 3D application
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queueFamilyIndices[] = { vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute], vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::Present] };

  if (vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute] != vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::Present])
  {
    createInfo.imageSharingMode         = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount    = 2;
    createInfo.pQueueFamilyIndices      = queueFamilyIndices;
  }
  else
  {
    createInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount    = 0;
    createInfo.pQueueFamilyIndices      = nullptr;
  }
  // flip, rotate, if specified possible in capabilities.
  createInfo.preTransform     = swapChainDetails.m_Capabilities.currentTransform;
  // specifies if the alpha channel should be used for blending with other windows in the window system.
  // You'll almost always want to simply ignore the alpha channel, hence OPAQUE_BIT_KHR.
  createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.oldSwapchain     = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_SwapChain) != VK_SUCCESS)
  {
    VKU_LOG_ERR("Failed to create swapchain.");
  }

  vkGetSwapchainImagesKHR(vk.m_LogicalDevice, vk.m_SwapChain, &swapChainImageCount, nullptr);
  vk.m_SwapChainImages.resize(swapChainImageCount);
  vkGetSwapchainImagesKHR(vk.m_LogicalDevice, vk.m_SwapChain, &swapChainImageCount, vk.m_SwapChainImages.data());

  vk.m_SwapChainImageFormat = format.format;
  vk.m_SwapChainImageExtent = surfaceExtent;
}

void vku::init::CreateSwapChainFramebuffers(VkState& vk)
{
  vk.m_SwapChainFramebuffers.resize(vk.m_SwapChainImageViews.size());

  for (uint32_t i = 0; i < vk.m_SwapChainImageViews.size(); i++)
  {
    Vector<VkImageView> attachments(*vk.m_CPUAllocator);

    if (vk.m_UseSwapchainMsaa)
    {
      attachments.resize(3);
      attachments[0] = vk.m_SwapChainColourImageView;
      attachments[1] = vk.m_SwapChainDepthImageView;
      attachments[2] = vk.m_SwapChainImageViews[i];
    }
    else
    {
      attachments.resize(2);
      attachments[0] = vk.m_SwapChainImageViews[i];
      attachments[1] = vk.m_SwapChainDepthImageView;

    }

    textures::CreateFramebuffer(vk, attachments, vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent, vk.m_SwapChainFramebuffers[i]);
  }
}

void vku::init::CreateSwapChainImageViews(VkState& vk)
{
  vk.m_SwapChainImageViews.resize(vk.m_SwapChainImages.size());

  for (uint32_t i = 0; i < vk.m_SwapChainImages.size(); i++)
  {
    textures::CreateImageView(vk, vk.m_SwapChainImages[i], vk.m_SwapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT,  vk.m_SwapChainImageViews[i]);
  }
}

void vku::init::CleanupSwapChain(VkState& vk)
{
  for (int i = 0; i < vk.m_SwapChainFramebuffers.size(); i++)
  {
    vkDestroyFramebuffer(vk.m_LogicalDevice, vk.m_SwapChainFramebuffers[i], nullptr);
  }
  for (int i = 0; i < vk.m_SwapChainImageViews.size(); i++)
  {
    vkDestroyImageView(vk.m_LogicalDevice, vk.m_SwapChainImageViews[i], nullptr);
  }

  vkDestroyImageView(vk.m_LogicalDevice, vk.m_SwapChainColourImageView, nullptr);
  vkDestroyImage(vk.m_LogicalDevice, vk.m_SwapChainColourImage, nullptr);
  vkFreeMemory(vk.m_LogicalDevice, vk.m_SwapChainColourImageMemory, nullptr);

  vkDestroyImageView(vk.m_LogicalDevice, vk.m_SwapChainDepthImageView, nullptr);
  vkDestroyImage(vk.m_LogicalDevice, vk.m_SwapChainDepthImage, nullptr);
  vkFreeMemory(vk.m_LogicalDevice, vk.m_SwapChainDepthImageMemory, nullptr);

  vkDestroySwapchainKHR(vk.m_LogicalDevice, vk.m_SwapChain, nullptr);
}

void vku::init::RecreateSwapChain(VkState& vk)
{
  VKU_LOG_INFO("VulkanAPI : Recreating Swapchain");
  while (vkDeviceWaitIdle(vk.m_LogicalDevice) != VK_SUCCESS);

  CleanupSwapChain(vk);
  CleanupImGui(vk);
  CleanupNanoVG(vk);

  CreateSwapChain(vk);
  CreateSwapChainImageViews(vk);
  CreateSwapChainColourTexture(vk, vk.m_UseSwapchainMsaa);
  CreateSwapChainDepthTexture(vk, vk.m_UseSwapchainMsaa);
  CreateSwapChainFramebuffers(vk);

  InitImGui(vk);
  InitNanoVG(vk);
}

void vku::init::CreateSwapChainColourTexture(VkState& vk, bool enableMsaa)
{
  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

  if (enableMsaa)
  {
    sampleCount = vk.m_MaxMsaaSamples;
  }

  textures::CreateImage(vk, vk.m_SwapChainImageExtent.width, vk.m_SwapChainImageExtent.height, 1, sampleCount,
              vk.m_SwapChainImageFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
              vk.m_SwapChainColourImage,
              vk.m_SwapChainColourImageMemory);

  textures::CreateImageView(vk, vk.m_SwapChainColourImage, vk.m_SwapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT, vk.m_SwapChainColourImageView);
}

void vku::init::CreateSwapChainDepthTexture(VkState& vk, bool enableMsaa )
{
  VkFormat depthFormat = utils::FindDepthFormat(vk);

  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

  if (enableMsaa)
  {
    sampleCount = vk.m_MaxMsaaSamples;
  }

  textures::CreateImage(vk, vk.m_SwapChainImageExtent.width, vk.m_SwapChainImageExtent.height, 1, sampleCount,
              depthFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
              vk.m_SwapChainDepthImage,
              vk.m_SwapChainDepthImageMemory);

  textures::CreateImageView(vk, vk.m_SwapChainDepthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT, vk.m_SwapChainDepthImageView);
  textures::TransitionImageLayout(vk, vk.m_SwapChainDepthImage, depthFormat, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkExtent2D vku::init::ChooseSwapExtent(VkState& vk, VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
  if(surfaceCapabilities.currentExtent.width != UINT32_MAX)
  {
    return surfaceCapabilities.currentExtent;
  }
  else
  {
    return vk.m_Backend->GetSurfaceExtent(vk, surfaceCapabilities);
  }
}

void vku::init::CreateCommandPool(VkState& vk)
{
  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType                = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex     = vk.m_QueueFamilyIndices.m_QueueFamilies[QueueFamilyType::GraphicsAndCompute];
  createInfo.flags                = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if(vkCreateCommandPool(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_GraphicsComputeQueueCommandPool) != VK_SUCCESS)
  {
    VKU_LOG_ERR("Failed to create Command Pool!");
  }
}

void vku::init::CreateDescriptorSetAllocator(VkState& vk)
{
  STLAllocator<DescriptorSetAllocator::PoolSizeRatio> ratioAlloc(*vk.m_CPUAllocator);
  Vector<DescriptorSetAllocator::PoolSizeRatio> ratios(ratioAlloc);
  ratios.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.33f });
  ratios.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.33f });
  ratios.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0.33f });
  vk.m_DescriptorSetAllocator.Init(*vk.m_CPUAllocator,vk.m_LogicalDevice, MAX_FRAMES_IN_FLIGHT * 128, ratios);
}

void vku::init::CreateSemaphores(VkState& vk)
{
  vk.m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  vk.m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  vk.m_ComputeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    if (vkCreateSemaphore(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_ComputeFinishedSemaphores[i]) != VK_SUCCESS)
    {
      VKU_LOG_ERR("Failed to create semaphores!");
    }
  }

}

void vku::init::CreateFences(VkState& vk)
{
  vk.m_FrameInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  vk.m_ImagesInFlightFences.resize(vk.m_SwapChainImages.size(), VK_NULL_HANDLE);
  vk.m_ComputeInFlightFences.resize(vk.m_SwapChainImages.size());

  VkFenceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    if (vkCreateFence(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_FrameInFlightFences[i]) != VK_SUCCESS ||
        vkCreateFence(vk.m_LogicalDevice, &createInfo, nullptr, &vk.m_ComputeInFlightFences[i]) != VK_SUCCESS )
    {
      VKU_LOG_ERR("Failed to create Fences!");
    }
  }
}

void vku::init::CreateCommandBuffers(VkState& vk)
{
  vk.m_GraphicsCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  vk.m_ComputeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.commandPool = vk.m_GraphicsComputeQueueCommandPool;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VK_CHECK(vkAllocateCommandBuffers(vk.m_LogicalDevice, &allocateInfo, vk.m_GraphicsCommandBuffers.data()))
  VK_CHECK(vkAllocateCommandBuffers(vk.m_LogicalDevice, &allocateInfo, vk.m_ComputeCommandBuffers.data()))

}

void vku::init::ClearCommandBuffers(VkState& vk)
{
  for (uint32_t i = 0; i < vk.m_GraphicsCommandBuffers.size(); i++)
  {
    vkResetCommandBuffer(vk.m_GraphicsCommandBuffers[i], 0);
  }

  for (uint32_t i = 0; i < vk.m_ComputeCommandBuffers.size(); i++)
  {
    vkResetCommandBuffer(vk.m_ComputeCommandBuffers[i], 0);
  }
}

void vku::init::CreateVmaAllocator(VkState& vk)
{
  VmaVulkanFunctions vulkanFunctions = {};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorCreateInfo = {};
  allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorCreateInfo.physicalDevice = vk.m_PhysicalDevice;
  allocatorCreateInfo.device = vk.m_LogicalDevice;
  allocatorCreateInfo.instance = vk.m_Instance;
  allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

  VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &vk.m_Allocator));
}

void vku::init::GetMaxUsableSampleCount(VkState& vk)
{
  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(vk.m_PhysicalDevice, &physicalDeviceProperties);

  auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & VK_SAMPLE_COUNT_64_BIT) {  vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_64_BIT;  return ;}
  if (counts & VK_SAMPLE_COUNT_32_BIT) {  vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_32_BIT;  return ;}
  if (counts & VK_SAMPLE_COUNT_16_BIT) {  vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_16_BIT;  return ;}
  if (counts & VK_SAMPLE_COUNT_8_BIT) {   vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_8_BIT;   return ;}
  if (counts & VK_SAMPLE_COUNT_4_BIT) {   vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_4_BIT;   return ;}
  if (counts & VK_SAMPLE_COUNT_2_BIT) {   vk.m_MaxMsaaSamples = VK_SAMPLE_COUNT_2_BIT;   return ;}

  vk.m_SelectedMsaaSamples = vk.m_MaxMsaaSamples;
}

void vku::init::CreateBuiltInRenderPasses(vku::VkState &vk) {
  STLAllocator< VkAttachmentDescription> ada(*vk.m_CPUAllocator);
  {

    Vector<VkAttachmentDescription> colourAttachmentDescriptions(ada);
    Vector<VkAttachmentDescription> resolveAttachmentDescriptions(ada);
    VkAttachmentDescription depthAttachmentDescription{};

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vk.m_SwapChainImageFormat;
    colorAttachment.samples = vk.m_UseSwapchainMsaa ? vk.m_MaxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk.m_UseSwapchainMsaa)
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    colourAttachmentDescriptions.push_back(colorAttachment);

    depthAttachmentDescription.format = utils::FindDepthFormat(vk);
    depthAttachmentDescription.samples = vk.m_UseSwapchainMsaa ? vk.m_MaxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    if (vk.m_UseSwapchainMsaa)
    {
      colorAttachmentResolve.format = vk.m_SwapChainImageFormat;
      colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      resolveAttachmentDescriptions.push_back(colorAttachmentResolve);
    }

    render_passes::CreateRenderPass(vk, vk.m_SwapchainImageRenderPass, colourAttachmentDescriptions, resolveAttachmentDescriptions, true, depthAttachmentDescription, VK_ATTACHMENT_LOAD_OP_CLEAR);
  }
  {
    Vector<VkAttachmentDescription> colourAttachmentDescriptions(ada);
    Vector<VkAttachmentDescription> resolveAttachmentDescriptions(ada);
    VkAttachmentDescription depthAttachmentDescription{};

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = vk.m_SwapChainImageFormat;
    colorAttachment.samples = vk.m_UseSwapchainMsaa ? vk.m_MaxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk.m_UseSwapchainMsaa)
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
      colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    colourAttachmentDescriptions.push_back(colorAttachment);

    depthAttachmentDescription.format = utils::FindDepthFormat(vk);
    depthAttachmentDescription.samples = vk.m_UseSwapchainMsaa ? vk.m_MaxMsaaSamples : VK_SAMPLE_COUNT_1_BIT;;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    if (vk.m_UseSwapchainMsaa)
    {
      colorAttachmentResolve.format = vk.m_SwapChainImageFormat;
      colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      resolveAttachmentDescriptions.push_back(colorAttachmentResolve);
    }
    render_passes::CreateRenderPass(vk, vk.m_ImGuiRenderPass, colourAttachmentDescriptions, resolveAttachmentDescriptions, true, depthAttachmentDescription, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  }
}

vku::Vector<VkExtensionProperties>
vku::init::GetDeviceAvailableExtensions(IAllocator& alloc, VkPhysicalDevice physicalDevice) {
  using namespace vku;
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

  STLAllocator<VkExtensionProperties> epa(alloc);
  Vector<VkExtensionProperties> availableExtensions(epa);
  availableExtensions.resize(extensionCount);
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

  return availableExtensions;


}
void vku::init::Quit(vku::VkState &vk) {
  vk.m_ShouldRun = false;
}
