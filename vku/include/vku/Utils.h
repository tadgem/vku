#pragma once
#include "vku/Structs.h"
namespace vku::utils {
  uint32_t                            FindMemoryType(VkState& vk,uint32_t typeFilter, VkMemoryPropertyFlags properties);
  VkFormat                            FindSupportedFormat(VkState& vk, const Vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
  VkFormat                            FindDepthFormat(VkState& vk);
  bool                                HasStencilComponent(VkFormat& format);
}