#include "vku/Structs.h"
#include "vku/Macros.h"
#include "vku/Log.h"

bool vku::QueueFamilyIndices::IsComplete() {
  bool foundGraphicsQueue = m_QueueFamilies.find(QueueFamilyType::GraphicsAndCompute) != m_QueueFamilies.end();
  bool foundPresentQueue  = m_QueueFamilies.find(QueueFamilyType::Present) != m_QueueFamilies.end();
  return foundGraphicsQueue && foundPresentQueue;
}
void vku::MappedBuffer::Free(vku::VkState &vk) {
  vmaUnmapMemory(vk.m_Allocator, m_GpuMemory);
  Buffer::Free(vk);
}

vku::QueueFamilyIndices::QueueFamilyIndices(IAllocator& alloc) : m_QueueFamilies(alloc)
{

}

void vku::ShaderBufferFrameData::Free(vku::VkState& vk) {
    if (!Ready())
    {
        return;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_UniformBuffers[i]->Free(vk);
    }
}

void vku::VkPipelineData::Free(vku::VkState &vk) const
{
  vkDestroyPipelineLayout (vk.m_LogicalDevice, m_PipelineLayout, nullptr);
  vkDestroyPipeline(vk.m_LogicalDevice, m_Pipeline, nullptr);
}
void vku::Buffer::Free(vku::VkState &vk) {
  vkDestroyBuffer(vk.m_LogicalDevice, m_GpuBuffer, nullptr);
  vmaFreeMemory(vk.m_Allocator, m_GpuMemory);
}

vku::Buffer::Buffer(const BufferStorageType& bufferType, VkBuffer buf, VmaAllocation alloc, VkDeviceSize size)
    : m_Type(bufferType), m_GpuBuffer(buf), m_GpuMemory(alloc), m_Size(size)
{}

void vku::MappedBuffer::Map(VkState& vk)
{
    vmaMapMemory(vk.m_Allocator, m_GpuMemory, &m_MappedAddr);
}

vku::MappedBuffer::MappedBuffer(Buffer& buf)
    : Buffer(Buffer::BufferStorageType::Mapped, buf.m_GpuBuffer, buf.m_GpuMemory, buf.m_Size), m_MappedAddr(nullptr)
{
}

bool vku::ShaderBufferFrameData::CanSet(uint32_t frameIndex)
{
    if (!Ready())
    {
        VKU_LOG_ERR("Buffer has not beed set or allocated");
        return false;
    }
    if (m_UniformBuffers[frameIndex]->m_Type != Buffer::BufferStorageType::Mapped)
    {
        VKU_LOG_ERR("Attempting to set data for non mapped buffer");
        return false;
    }
    return true;
}

bool vku::ShaderBufferFrameData::Ready()
{
    bool ready = true;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (m_UniformBuffers[i].get() == nullptr)
        {
            ready = false;
            break;
        }
    }
    return ready;
}
