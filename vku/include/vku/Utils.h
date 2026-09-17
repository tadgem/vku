#pragma once
#include "vku/Structs.h"
namespace vku::utils {

uint32_t FindMemoryType(VkState &vk, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

VkFormat FindSupportedFormat(VkState &vk, const Vector<VkFormat> &candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features);

VkFormat FindDepthFormat(VkState &vk);

bool HasStencilComponent(VkFormat &format);

template <typename _Ty>
Vector<_Ty> Combine(IAllocator &alloc, const Vector<_Ty> &a,
                    const Vector<_Ty> &b) {
  Vector<_Ty> v(alloc);

  for (const auto &val : a) {
    v.push_back(val);
  }
  for (const auto &val : b) {
    v.push_back(val);
  }

  return v;
}
} // namespace vku::utils