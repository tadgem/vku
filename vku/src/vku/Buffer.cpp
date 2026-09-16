#include "vku/Buffer.h"
#include "vku/Macros.h"
#include "vku/Commands.h"
#include "vku/Log.h"

namespace vku
{
Buffer buffers::CreateBuffer(VkState& vk, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
  VkBuffer buffer; 
  VmaAllocation allocation;
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo = {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocInfo.requiredFlags = properties;

  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
  {
    // if we access members of the array non sequentially,
    // we may need to request random access instead of sequential
    // VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  }

  VK_CHECK(vmaCreateBuffer(vk.m_Allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr));

  return Buffer(Buffer::BufferStorageType::GPUOnly, buffer, allocation, size);
}

void buffers::CopyBuffer(VkState& vk, VkBuffer& src, VkBuffer& dst, VkDeviceSize size)
{
  // create a new command buffer to record the buffer copy
  VkCommandBuffer commandBuffer = commands::BeginSingleTimeCommands(vk);

  // record copy command
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0; // Optional
  copyRegion.dstOffset = 0; // Optional
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
  commands::EndSingleTimeCommands(vk, commandBuffer);
}

Buffer buffers::CreateIndexBuffer(VkState& vk, uint32_t* indices, size_t count)
{
  VkDeviceSize bufferSize = sizeof(uint32_t) * count;

  // create a CPU side buffer to dump vertex data into
  MappedBuffer stagingBuf = buffers::CreateMappedBuffer(vk, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  memcpy(stagingBuf.m_MappedAddr, indices, bufferSize);
  
  // create GPU side buffer
  Buffer buf = CreateBuffer(vk, bufferSize,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CopyBuffer(vk, stagingBuf.m_GpuBuffer, buf.m_GpuBuffer, bufferSize);
  stagingBuf.Free(vk);

  return buf;
}

MappedBuffer buffers::CreateMappedBuffer(VkState& vk, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryProperties)
{
  Buffer b = CreateBuffer(vk,VkDeviceSize{ size }, bufferUsage, memoryProperties);
  MappedBuffer mb (b);
  mb.Map(vk);
  return mb;
}

ShaderBufferFrameData buffers::CreateUniformBuffers (VkState& vk, VkDeviceSize bufferSize)
{
  ShaderBufferFrameData uniformData {};
  
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    uniformData.m_UniformBuffers[i] = std::move(std::make_unique<MappedBuffer>(CreateMappedBuffer(vk,
        static_cast<uint32_t>(bufferSize),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
  }
  return uniformData;
}

ShaderBufferFrameData buffers::CreateShaderStorageBuffers(VkState& vk, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage )
{
    ShaderBufferFrameData uniformData{};
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bufferUsage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM)
    {
        usageFlags |= bufferUsage;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformData.m_UniformBuffers[i] = std::move(std::make_unique<MappedBuffer>(CreateMappedBuffer(vk,
            static_cast<uint32_t>(bufferSize),
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
    }
    return uniformData;
}
}