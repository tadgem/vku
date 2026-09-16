#pragma once
#include "vku/Structs.h"
namespace vku {
namespace commands {
  VkCommandBuffer BeginSingleTimeCommands(VkState &vk);

  void EndSingleTimeCommands(VkState &vk, VkCommandBuffer &commandBuffer);

  void RecordGraphicsCommands(
      VkState &vk,
      VKU_FUNCTIONAL_NS::function<void(VkCommandBuffer &, uint32_t)> graphicsCommandsCallback);

  void RecordComputeCommands(
      VkState &vk,
      VKU_FUNCTIONAL_NS::function<void(VkCommandBuffer &, uint32_t)> computeCommandsCallback);
}
} // namespace turas