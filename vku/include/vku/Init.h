#pragma once
#include "Mesh.h"
#include "Texture.h"
#include "vku/Structs.h"

namespace vku {

namespace init {
const StaticVector<const char *> s_ValidationLayers = {
    "VK_LAYER_KHRONOS_validation"};
const StaticVector<const char *> s_RequiredDeviceExtensions = {
    "VK_KHR_swapchain"};

void CleanupImGui(VkState &vk);
void CleanupNanoVG(VkState &vk);
void Cleanup(VkState &vk);

bool CheckValidationLayerSupport(VkState &vk);
bool CheckDeviceExtensionSupport(VkState &vk, VkPhysicalDevice device);
void SetupDebugOutput(VkState &vk);
void CleanupDebugOutput(VkState &vk);
void ListDeviceExtensions(VkState &vk, VkPhysicalDevice physicalDevice);
void PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo);

void InitVulkan(VkState &vk, bool enableSwapchainMsaa = false);
void InitImGui(VkState &vk);
void InitNanoVG(VkState &vk);

VkApplicationInfo CreateAppInfo();
void CreateInstance(VkState &vk);
void CleanupVulkan(VkState &vk);
QueueFamilyIndices FindQueueFamilies(VkState &vk,
                                     VkPhysicalDevice physicalDevice);
SwapChainSupportDetais
GetSwapChainSupportDetails(VkState &vk, VkPhysicalDevice physicalDevice);
bool IsDeviceSuitable(VkState &vk, VkPhysicalDevice physicalDevice);
uint32_t AssessDeviceSuitability(VkPhysicalDevice physicalDevice);
void PickPhysicalDevice(VkState &vk);
void CreateLogicalDevice(VkState &vk);
void GetQueueHandles(VkState &vk);
VkSurfaceFormatKHR
ChooseSwapChainSurfaceFormat(Vector<VkSurfaceFormatKHR> availableFormats);
VkPresentModeKHR
ChooseSwapChainPresentMode(VkState &vk,
                           Vector<VkPresentModeKHR> availableModes);
void CreateSwapChain(VkState &vk);
void CreateSwapChainFramebuffers(VkState &vk);
void CreateSwapChainImageViews(VkState &vk);
void CleanupSwapChain(VkState &vk);
void RecreateSwapChain(VkState &vk);
void CreateSwapChainColourTexture(VkState &vk, bool enableMsaa = false);
void CreateSwapChainDepthTexture(VkState &vk, bool enableMsaa = false);
VkExtent2D ChooseSwapExtent(VkState &vk,
                            VkSurfaceCapabilitiesKHR &surfaceCapabilities);
void CreateCommandPool(VkState &vk);
void CreateDescriptorSetAllocator(VkState &vk);
void CreateSemaphores(VkState &vk);
void CreateFences(VkState &vk);

void CreateCommandBuffers(VkState &vk);
void ClearCommandBuffers(VkState &vk);
void CreateVmaAllocator(VkState &vk);
void GetMaxUsableSampleCount(VkState &vk);

Vector<VkExtensionProperties>
GetDeviceAvailableExtensions(IAllocator &alloc,
                             VkPhysicalDevice physicalDevice);
void EnableCommonExtensions(VkState &vk);

void CreateBuiltInRenderPasses(VkState &vk);
void Quit(VkState &vk);

template <typename _BackendTy, typename _AllocatorType = MallocAllocator>
VkState Create(const char *appName, uint32_t width, uint32_t height,
               bool enableSwapchainMsaa, bool enableValidation = true) {
  // always going to be in STD impl :(
  static_assert(std::is_base_of<VkBackend, _BackendTy>::value,
                "Backend must inherit from VkBackend");
  static_assert(std::is_base_of<IAllocator, _AllocatorType>::value,
                "Allocator must inherit from IAllocator");
  auto alloc = VKU_MEMORY_NS::make_unique<_AllocatorType>();
  VkState vk(*alloc);
  vk.m_UseValidation = enableValidation;
  vk.m_CPUAllocator = VKU_MEMORY_NS::move(alloc);
  vk.m_AppName = String(appName, *vk.m_CPUAllocator);
  vk.m_Backend = VKU_MEMORY_NS::make_unique<_BackendTy>();
  vk.m_Backend->CreateWindowVKU(vk, width, height);
  InitVulkan(vk, enableSwapchainMsaa);
  vk.m_MaxFramebufferExtent = vk.m_Backend->GetMaxFramebufferResolution(vk);
  vk.m_RunComputeCommands = false;
  InitImGui(vk);
  InitNanoVG(vk);
  Texture::InitDefaultTexture(vk);
  Mesh::InitBuiltInMeshes(vk);

  return vk;
}
} // namespace init

} // namespace vku