#pragma once
#include "vku/Log.h"
#include "vku/Macros.h"
#include "vku/Structs.h"

namespace vku {
class buffers {
public:
  static Buffer CreateBuffer(VkState &vk, VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties);

  static void CopyBuffer(VkState &vk, VkBuffer &src, VkBuffer &dst,
                         VkDeviceSize size);

  static Buffer CreateIndexBuffer(VkState &vk, uint32_t *indices, size_t count);
  static Buffer CreateIndexBuffer(VkState &vk, Vector<uint32_t> &indices) {
    return CreateIndexBuffer(vk, indices.data(), indices.size());
  }

  template <typename _Ty>
  static Vector<MappedBuffer> CreateUniformBuffers(VkState &vk) {
    VkDeviceSize bufferSize = sizeof(_Ty);
    Vector<MappedBuffer> uniformBuffers(*vk.m_CPUAllocator);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      uniformBuffers[i] =
          CreateMappedBuffer(vk, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    return uniformBuffers;
  }

  static MappedBuffer
  CreateMappedBuffer(VkState &vk, VkDeviceSize size,
                     VkBufferUsageFlags bufferUsage,
                     VkMemoryPropertyFlags memoryProperties);

  static ShaderBufferFrameData CreateUniformBuffers(VkState &vk,
                                                    VkDeviceSize bufferSize);
  static ShaderBufferFrameData CreateShaderStorageBuffers(
      VkState &vk, VkDeviceSize bufferSize,
      VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM);

  template <typename _Ty>
  static ShaderBufferFrameData CreateUniformBuffersT(VkState &vk) {
    constexpr VkDeviceSize bufferSize = sizeof(_Ty);
    return CreateUniformBuffers(vk, bufferSize);
  }

  template <typename _Ty>
  static Buffer CreateVertexBuffer(VkState &vk, _Ty *verts, size_t count) {
    VkDeviceSize bufferSize = sizeof(_Ty) * count;

    // create a CPU side buffer to dump vertex data into

    MappedBuffer stagingBuffer =
        CreateMappedBuffer(vk, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // dump vert data
    memcpy(stagingBuffer.m_MappedAddr, verts, bufferSize);

    // create GPU side buffer
    Buffer vb = CreateBuffer(vk, bufferSize,
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CopyBuffer(vk, stagingBuffer.m_GpuBuffer, vb.m_GpuBuffer, bufferSize);

    stagingBuffer.Free(vk);
    return vb;
  }

  template <typename _Ty>
  static Buffer CreateVertexBuffer(VkState &vk, Vector<_Ty> &verts) {
    return CreateVertexBuffer<_Ty>(vk, verts.data(), verts.size());
  }
};
}