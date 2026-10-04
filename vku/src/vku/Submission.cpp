#include "vku/Submission.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "vku/Commands.h"
#include "vku/Debug.h"
#include "vku/Init.h"
#include "vku/Log.h"
#include "vku/Macros.h"

void vku::submission::SubmitFrame(VkState &vk) {
  // Nothing was recorded this frame (minimised surface / out of date swapchain).
  if (!vk.m_FrameValid) {
    return;
  }

  const uint32_t frameIndex = vk.m_CurrentFrameIndex;
  const uint32_t imageIndex = vk.m_CurrentImageIndex;

  if (vk.m_RunComputeCommands) {
    vkWaitForFences(vk.m_LogicalDevice, 1,
                    &vk.m_ComputeInFlightFences[frameIndex], VK_TRUE,
                    UINT64_MAX);
    vkResetFences(vk.m_LogicalDevice, 1,
                  &vk.m_ComputeInFlightFences[frameIndex]);

    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &vk.m_ComputeCommandBuffers[frameIndex];
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores =
        &vk.m_ComputeFinishedSemaphores[frameIndex];

    VK_CHECK(vkQueueSubmit(vk.m_ComputeQueue, 1, &computeSubmitInfo,
                           vk.m_ComputeInFlightFences[frameIndex]));
  }

  vkResetFences(vk.m_LogicalDevice, 1, &vk.m_FrameInFlightFences[frameIndex]);

  // Wait on the image-available semaphore signalled by init::BeginFrame's
  // acquire, and signal the render-finished semaphore owned by this image.
  VkSemaphore waitSemaphores[] = {
      vk.m_ImageAvailableSemaphores[frameIndex]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSemaphore waitSemaphoresMulti[2];
  VkPipelineStageFlags waitStagesMulti[2];
  uint32_t waitCount = 1;
  waitSemaphoresMulti[0] = vk.m_ImageAvailableSemaphores[frameIndex];
  waitStagesMulti[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  if (vk.m_RunComputeCommands) {
    waitSemaphoresMulti[1] = vk.m_ComputeFinishedSemaphores[frameIndex];
    waitStagesMulti[1] = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    waitCount = 2;
  }

  VkSemaphore signalSemaphore =
      vk.m_RenderFinishedSemaphores[imageIndex];

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = waitCount;
  submitInfo.pWaitSemaphores =
      waitCount == 1 ? waitSemaphores : waitSemaphoresMulti;
  submitInfo.pWaitDstStageMask =
      waitCount == 1 ? waitStages : waitStagesMulti;
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &vk.m_GraphicsCommandBuffers[frameIndex];
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = &signalSemaphore;

  if (vkQueueSubmit(vk.m_GraphicsQueue, 1, &submitInfo,
                    vk.m_FrameInFlightFences[frameIndex]) != VK_SUCCESS) {
    VKU_LOG_ERR("VulkanAPI : Failed to submit draw command buffer!");
  }


  if (vk.m_UseImGui) {
    RenderImGui(vk, imageIndex);
  }

  VkSwapchainKHR swapchains[] = {vk.m_SwapChain};

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &signalSemaphore;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pResults = nullptr;

  VkResult result = vkQueuePresentKHR(vk.m_GraphicsQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    init::RecreateSwapChain(vk);
  } else if (result != VK_SUCCESS) {
    VKU_LOG_ERR("VulkanAPI : Error presenting swapchain image");
  }

  vk.m_FrameValid = false;
  vk.m_CurrentFrameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vku::submission::RenderImGui(VkState &vk, uint32_t imageIndex) {
  ImGui::Render();
  VkCommandBuffer imguiCommandBuffer = commands::BeginSingleTimeCommands(vk);
  debug::BeginDebugMarker(imguiCommandBuffer, "ImGui",
                          {0.0f, 0.0f, 1.0f, 1.0f});

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = vk.m_ImGuiRenderPass;
  renderPassInfo.framebuffer = vk.m_SwapChainFramebuffers[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = vk.m_SwapChainImageExtent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(imguiCommandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguiCommandBuffer);
  vkCmdEndRenderPass(imguiCommandBuffer);
  debug::EndDebugMarker(imguiCommandBuffer);
  commands::EndSingleTimeCommands(vk, imguiCommandBuffer);
}
