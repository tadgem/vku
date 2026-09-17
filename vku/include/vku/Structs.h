#pragma once

#define VK_NO_PROTOTYPES
#include "volk.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4189)
#pragma warning(disable : 4324)
#pragma warning(disable : 4505)
#include "ThirdParty/VulkanMemoryAllocator.h"
#pragma warning(pop)

#include "glm/glm.hpp"
#include "vku/Alias.h"
#include "vku/Allocator.h"
#include "vku/DescriptorSetAllocator.h"
#include "vku/Macros.h"

struct NVGcontext;

namespace vku {

struct VkState;

enum class ShaderBindingType {
  UniformBuffer,
  ShaderStorageBuffer,
  PushConstants,
  Sampler,
  SampledImage
};

enum class ShaderBufferMemberType {
  UNKNOWN,
  _vec2,
  _vec3,
  _vec4,
  _mat2,
  _mat3,
  _mat4,
  _float,
  _double,
  _int,
  _uint,
  _array,
  _sampler
};

class VulkanAPIWindowHandle {};

using StageBinary = Vector<unsigned char>;

struct PushConstantBlock {
  uint32_t m_Size;
  uint32_t m_Offset;
  String m_Name;
  VkShaderStageFlags m_Stage;

  PushConstantBlock(IAllocator &alloc) : m_Name(alloc) {}
};

struct ShaderBufferMember {
  uint32_t m_Size;
  uint32_t m_Offset;
  uint32_t m_Stride;
  String m_Name;
  ShaderBufferMemberType m_Type;

  ShaderBufferMember(IAllocator &alloc) : m_Name(alloc) {}
};

struct DescriptorSetLayoutBindingData {
  String m_BindingName;
  uint32_t m_BindingIndex;
  uint32_t m_ExpectedBufferSizeOrDivisor;
  ShaderBindingType m_BufferType;
  Vector<ShaderBufferMember> m_Members;

  DescriptorSetLayoutBindingData(IAllocator &alloc)
      : m_BindingName(alloc), m_BindingIndex(0),
        m_ExpectedBufferSizeOrDivisor(0), m_Members(alloc) {}
};

struct DescriptorSetLayoutData {
  uint32_t m_SetNumber;
  VkDescriptorSetLayoutCreateInfo m_CreateInfo;
  VkDescriptorSetLayout m_Layout;
  Vector<VkDescriptorSetLayoutBinding> m_Bindings;
  Vector<DescriptorSetLayoutBindingData> m_BindingDatas;

  explicit DescriptorSetLayoutData(IAllocator &alloc)
      : m_SetNumber(0), m_Bindings(alloc), m_BindingDatas(alloc) {}

  ~DescriptorSetLayoutData() = default;

  DescriptorSetLayoutData(const DescriptorSetLayoutData &other) = default;

  DescriptorSetLayoutData(DescriptorSetLayoutData &&other) = default;

  DescriptorSetLayoutData &
  operator=(const DescriptorSetLayoutData &other) // copy assignment
  {
    // implemented as move-assignment from a temporary copy for brevity
    // note that this prevents potential storage reuse
    return *this = DescriptorSetLayoutData(other);
  }

  DescriptorSetLayoutData &
  operator=(DescriptorSetLayoutData &&other) noexcept // move assignment
  {
    std::swap(m_SetNumber, other.m_SetNumber);
    std::swap(m_CreateInfo, other.m_CreateInfo);
    std::swap(m_Layout, other.m_Layout);
    std::swap(m_Bindings, other.m_Bindings);
    std::swap(m_BindingDatas, other.m_BindingDatas);
    return *this;
  }
};

class Buffer {
public:
  enum BufferStorageType { GPUOnly, Mapped };

  enum BufferType { Uniform, ShaderStorage };

  VkBuffer m_GpuBuffer = VK_NULL_HANDLE;
  VmaAllocation m_GpuMemory = VK_NULL_HANDLE;
  VkDeviceSize m_Size = 0;
  BufferStorageType m_Type;

  Buffer(const BufferStorageType &bufferType, VkBuffer buf, VmaAllocation alloc,
         VkDeviceSize size);
  Buffer() = default;
  virtual ~Buffer() = default;
  virtual void Free(VkState &vk);
};

// reuse this for generic cpu dynamic buffer
class MappedBuffer : public Buffer {
public:
  MappedBuffer(Buffer &b);
  MappedBuffer() = default;

  void *m_MappedAddr = nullptr;

  void Map(VkState &vk);
  void Free(VkState &vk) override;
};

struct ShaderBufferFrameData {
  Array<RefCntPtr<Buffer>, MAX_FRAMES_IN_FLIGHT> m_UniformBuffers;
  ShaderBufferFrameData() = default;

  bool Ready();
  bool CanSet(uint32_t frameIndex);

  template <typename _Ty>
  void SetMemory(uint32_t frameIndex, const _Ty *start, uint64_t count) {
    if (!CanSet(frameIndex)) {
      return;
    }
    MappedBuffer *mb =
        static_cast<MappedBuffer *>(m_UniformBuffers[frameIndex].get());
    void *addr = mb->m_MappedAddr;
    memcpy(addr, start, count);
  }

  template <typename _Ty>
  void Set(uint32_t frameIndex, const _Ty &data, uint32_t offset = 0) {
    constexpr size_t _ty_size = sizeof(_Ty);
    if (!CanSet(frameIndex)) {
      return;
    }
    MappedBuffer *mb =
        static_cast<MappedBuffer *>(m_UniformBuffers[frameIndex].get());
    uint64_t base_addr = (uint64_t)mb->m_MappedAddr;
    void *addr = (void *)(base_addr + static_cast<uint64_t>(offset));
    memcpy(addr, &data, _ty_size);
  }

  void Free(VkState &vk);
};

struct DescriptorSetBinding {
  union {
    uint64_t m_Data;
    struct SetBinding {
      uint32_t m_Set;
      uint32_t m_Binding;
    } m_SetBinding;
  };
  VkDeviceSize m_BindingSize;

  DescriptorSetBinding(uint32_t set, uint32_t binding, VkDeviceSize size)
      : m_SetBinding({set, binding}), m_BindingSize(size) {}
  DescriptorSetBinding() = default;

  bool operator==(const DescriptorSetBinding &other) const {
    return (this->m_Data == other.m_Data);
    // TODO: This size should account for alignment diffs between GPU & CPU
    // && (this->m_BindingSize== other.m_BindingSize);
  }
};

struct VertexDescription {
  Vector<VkVertexInputBindingDescription> m_BindingDescriptions;
  Vector<VkVertexInputAttributeDescription> m_AttributeDescriptions;

  VertexDescription(IAllocator &allocator)
      : m_BindingDescriptions(
            STLAllocator<VkVertexInputBindingDescription>(allocator)),
        m_AttributeDescriptions(
            STLAllocator<VkVertexInputAttributeDescription>(allocator)) {}
};

struct RasterizationState {
  VkPolygonMode m_PolygonMode;
  VkCullModeFlags m_CullMode;
  bool m_EnableMSAA;
  VkCompareOp m_DepthCompareOp;
  VkPrimitiveTopology m_InputAssemblyTopology;
  VkFrontFace m_FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  float m_LineWidth = 1.0f;
};

struct PipelineAttachmentState {
  Vector<VkPipelineColorBlendAttachmentState> m_ColourAttachmentStates;
  VkPipelineColorBlendStateCreateInfo m_BlendStateInfo;

  PipelineAttachmentState(IAllocator &alloc)
      : m_ColourAttachmentStates(alloc), m_BlendStateInfo({}) {};
};

struct PipelineDynamicState {
  Vector<VkDynamicState> m_DynamicStates;
  VkPipelineDynamicStateCreateInfo m_DynamicStateInfo;

  PipelineDynamicState(IAllocator &alloc)
      : m_DynamicStates(alloc), m_DynamicStateInfo({}) {};
};

struct RenderPassInfo {
  Vector<VkFramebuffer> m_SwapchainFramebuffers;
  VkRenderPass m_RenderPass;
  Vector<VkRenderPassBeginInfo> m_RenderPassInfos;

  RenderPassInfo(IAllocator &alloc)
      : m_SwapchainFramebuffers(alloc), m_RenderPassInfos(alloc) {};
};

struct DynamicRenderingInfo {
  Array<Vector<VkRenderingAttachmentInfoKHR>, MAX_FRAMES_IN_FLIGHT>
      m_ColourAttachmentInfos;
  Array<Vector<VkRenderingAttachmentInfoKHR>, MAX_FRAMES_IN_FLIGHT>
      m_DepthAttachmentInfos;
  Array<Vector<VkRenderingAttachmentInfoKHR>, MAX_FRAMES_IN_FLIGHT>
      m_ResolveAttachmentInfos;

  Vector<VkRenderingInfoKHR> m_RenderingInfos;
};

enum class ShaderStageType { Vertex, Fragment, Compute };

enum QueueFamilyType {
  GraphicsAndCompute = VK_QUEUE_GRAPHICS_BIT,
  Transfer = VK_QUEUE_TRANSFER_BIT,
  Present = 8

};

struct QueueFamilyIndices {
  HashMap<QueueFamilyType, uint32_t> m_QueueFamilies;

  QueueFamilyIndices(IAllocator &alloc);

  bool IsComplete();
};

struct SwapChainSupportDetais {
  VkSurfaceCapabilitiesKHR m_Capabilities;
  Vector<VkSurfaceFormatKHR> m_SupportedFormats;
  Vector<VkPresentModeKHR> m_SupportedPresentModes;

  SwapChainSupportDetais(IAllocator &alloc)
      : m_SupportedFormats(alloc), m_SupportedPresentModes(alloc) {}
};

struct VkState;

struct VkPipelineData {
  VkPipelineData(VkPipeline pipeline, VkPipelineLayout layout) {
    m_Pipeline = pipeline;
    m_PipelineLayout = layout;
  }

  VkPipelineData() = default;

  void Free(VkState &vk) const;

  VkPipeline m_Pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};

class VkBackend {
public:
  virtual ~VkBackend() = default;

  virtual Vector<const char *> GetRequiredInstanceExtensions(VkState &vk) = 0;
  virtual void CreateSurface(VkState &vk) = 0;
  virtual void CreateWindowVKU(VkState &vk, uint32_t width,
                               uint32_t height) = 0;
  virtual void CleanupWindow(VkState &vk) = 0;
  virtual VkExtent2D GetSurfaceExtent(VkState &vk,
                                      VkSurfaceCapabilitiesKHR surface) = 0;
  virtual VkExtent2D GetMaxFramebufferResolution(VkState &vk) = 0;
  virtual bool ShouldRun(VkState &vk) = 0;
  virtual void PreFrame(VkState &vk) = 0;
  virtual void PostFrame(VkState &vk) = 0;
  virtual void Run(VkState &vk,
                   VKU_FUNCTIONAL_NS::function<void()> callback) = 0;
  virtual void InitImGuiBackend(VkState &vk) = 0;
  virtual void CleanupImGuiBackend(VkState &vk) = 0;
  virtual StageBinary LoadBinaryFromPath(VkState &vk, const char *path) = 0;
  virtual String LoadStringFromPath(VkState &vk, const char *path) = 0;
};

struct VkState {
  Unique<VkBackend> m_Backend;
  Unique<IAllocator> m_CPUAllocator;

  VkInstance m_Instance;
  VkSurfaceKHR m_Surface;
  VkSwapchainKHR m_SwapChain;
  VkDebugUtilsMessengerEXT m_DebugMessenger;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_LogicalDevice = VK_NULL_HANDLE;
  VkRenderPass m_SwapchainImageRenderPass;
  VkRenderPass m_ImGuiRenderPass;
  VkCommandPool m_GraphicsComputeQueueCommandPool;
  VmaAllocator m_Allocator;
  DescriptorSetAllocator m_DescriptorSetAllocator;

  Vector<VkSemaphore> m_ImageAvailableSemaphores;
  Vector<VkSemaphore> m_RenderFinishedSemaphores;
  Vector<VkSemaphore> m_ComputeFinishedSemaphores;
  Vector<VkFence> m_FrameInFlightFences;
  Vector<VkFence> m_ImagesInFlightFences;
  Vector<VkFence> m_ComputeInFlightFences;
  QueueFamilyIndices m_QueueFamilyIndices;

  VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
  VkQueue m_ComputeQueue = VK_NULL_HANDLE;
  VkQueue m_PresentQueue = VK_NULL_HANDLE;

  VulkanAPIWindowHandle *m_WindowHandle = nullptr;

  Vector<VkImage> m_SwapChainImages;
  Vector<VkImageView> m_SwapChainImageViews;
  Vector<VkFramebuffer> m_SwapChainFramebuffers;
  Vector<VkCommandBuffer> m_GraphicsCommandBuffers;
  Vector<VkCommandBuffer> m_ComputeCommandBuffers;

  VkFormat m_SwapChainImageFormat;
  VkExtent2D m_SwapChainImageExtent;

  VkImage m_SwapChainColourImage;
  VkDeviceMemory m_SwapChainColourImageMemory;
  VkImageView m_SwapChainColourImageView;

  VkImage m_SwapChainDepthImage;
  VkDeviceMemory m_SwapChainDepthImageMemory;
  VkImageView m_SwapChainDepthImageView;

  VkSampleCountFlagBits m_MaxMsaaSamples;
  VkSampleCountFlags m_SelectedMsaaSamples = VK_SAMPLE_COUNT_1_BIT;

  Vector<const char *> m_DesiredDeviceExtensions;

  double m_DeltaTime;
  bool m_ShouldRun = true;
  bool m_RunComputeCommands = false;
  bool m_UseSwapchainMsaa = false;
  bool m_WaitForVerticalSync = false;
  bool m_UseDynamicRendering = false;
  bool m_UseValidation = true;
  const bool m_UseImGui = true;
  uint64_t m_LastFrameTime;
  uint32_t m_CurrentFrameIndex;
  VkExtent2D m_MaxFramebufferExtent;
  String m_AppName;

  NVGcontext *m_NanoVG;

  VkState(IAllocator &alloc)
      : m_DescriptorSetAllocator(alloc), m_ImageAvailableSemaphores(alloc),
        m_RenderFinishedSemaphores(alloc), m_ComputeFinishedSemaphores(alloc),
        m_FrameInFlightFences(alloc), m_ImagesInFlightFences(alloc),
        m_ComputeInFlightFences(alloc), m_SwapChainImages(alloc),
        m_SwapChainImageViews(alloc), m_SwapChainFramebuffers(alloc),
        m_GraphicsCommandBuffers(alloc), m_ComputeCommandBuffers(alloc),
        m_DesiredDeviceExtensions(alloc), m_QueueFamilyIndices(alloc),
        m_AppName(alloc) {}
};

struct VkViewportData {
  VkViewport m_Viewport;
  VkRect2D m_Scissor;
  VkPipelineViewportStateCreateInfo m_CreateInfo;
};

} // namespace vku

template <> struct VKU_UNORDERED_MAP_NS::hash<vku::DescriptorSetBinding> {
  VKU_UNORDERED_MAP_NS::size_t
  operator()(const vku::DescriptorSetBinding &sb) const {
    using VKU_UNORDERED_MAP_NS::hash;

    return ((hash<uint64_t>()(sb.m_Data)));

    // TODO: This size should account for alignment diffs between GPU & CPU
    // ^ (hash<uint64_t>()(sb.m_BindingSize))));
  }
};
