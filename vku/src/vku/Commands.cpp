#include "vku/Commands.h"
#include "vku/Macros.h"
#include "vku/Log.h"
#include "ThirdParty/nanovg.h"

namespace vku {
namespace commands {
VkCommandBuffer BeginSingleTimeCommands(VkState &vk) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = vk.m_GraphicsComputeQueueCommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(vk.m_LogicalDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void EndSingleTimeCommands(VkState &vk, VkCommandBuffer &commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(vk.m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk.m_GraphicsQueue);

  vkFreeCommandBuffers(vk.m_LogicalDevice, vk.m_GraphicsComputeQueueCommandPool, 1,
                       &commandBuffer);
}

void RecordGraphicsCommands(
    VkState &vk,
    std::function<void(VkCommandBuffer &, uint32_t)> graphicsCommandsCallback) {
    for (uint32_t i = 0; i < vk.m_GraphicsCommandBuffers.size(); i++) {
      VkCommandBufferBeginInfo commandBufferBeginInfo{};
      commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      commandBufferBeginInfo.flags = 0;
      commandBufferBeginInfo.pInheritanceInfo = nullptr;

      VK_CHECK(
          vkBeginCommandBuffer(vk.m_GraphicsCommandBuffers[i], &commandBufferBeginInfo))

      uint32_t prevFrameIndex = vk.m_CurrentFrameIndex;
      vk.m_CurrentFrameIndex = i;

      nvgBeginFrame(vk.m_NanoVG, 1920, 1080, 1.0);

      // Callback (nvgEndFrame is called inside, which uses m_CurrentFrameIndex)
      graphicsCommandsCallback(vk.m_GraphicsCommandBuffers[i], i);

      vk.m_CurrentFrameIndex = prevFrameIndex;


      VK_CHECK(vkEndCommandBuffer(vk.m_GraphicsCommandBuffers[i]));
  }
}

void RecordComputeCommands(
    VkState &vk,
    std::function<void(VkCommandBuffer &, uint32_t)> computeCommandsCallback) {
  vk.m_RunComputeCommands = true;
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      VkCommandBufferBeginInfo commandBufferBeginInfo{};
      commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      commandBufferBeginInfo.flags = 0;
      commandBufferBeginInfo.pInheritanceInfo = nullptr;

      VK_CHECK(
          vkBeginCommandBuffer(vk.m_ComputeCommandBuffers[i], &commandBufferBeginInfo))

      // Callback
      computeCommandsCallback(vk.m_ComputeCommandBuffers[i], i);

      VK_CHECK(vkEndCommandBuffer(vk.m_ComputeCommandBuffers[i]));
  }
}
}
}