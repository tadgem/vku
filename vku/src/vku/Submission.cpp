#include "vku/Submission.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "vku/Commands.h"
#include "vku/Debug.h"
#include "vku/Init.h"
#include "vku/Log.h"
#include "vku/Macros.h"

void vku::submission::SubmitFrame(VkState &vk) {
  if (vk.m_RunComputeCommands) {
    // Compute
    vkWaitForFences(vk.m_LogicalDevice, 1,
                    &vk.m_ComputeInFlightFences[vk.m_CurrentFrameIndex],
                    VK_TRUE, UINT64_MAX);
    vkResetFences(vk.m_LogicalDevice, 1,
                  &vk.m_ComputeInFlightFences[vk.m_CurrentFrameIndex]);

    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers =
        &vk.m_ComputeCommandBuffers[vk.m_CurrentFrameIndex];
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores =
        &vk.m_ComputeFinishedSemaphores[vk.m_CurrentFrameIndex];

    VK_CHECK(vkQueueSubmit(vk.m_ComputeQueue, 1, &computeSubmitInfo,
                           vk.m_ComputeInFlightFences[vk.m_CurrentFrameIndex]));
  }
  // Graphics
  vkWaitForFences(vk.m_LogicalDevice, 1,
                  &vk.m_FrameInFlightFences[vk.m_CurrentFrameIndex], VK_TRUE,
                  UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      vk.m_LogicalDevice, vk.m_SwapChain, UINT64_MAX,
      vk.m_ImageAvailableSemaphores[vk.m_CurrentFrameIndex], VK_NULL_HANDLE,
      &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    init::RecreateSwapChain(vk);
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    VKU_LOG_ERR("VulkanAPI : Failed to acquire swap chain image!");
    return;
  }

  vkResetFences(vk.m_LogicalDevice, 1,
                &vk.m_FrameInFlightFences[vk.m_CurrentFrameIndex]);

  vk.m_ImagesInFlightFences[imageIndex] =
      vk.m_FrameInFlightFences[vk.m_CurrentFrameIndex];

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  STLAllocator<VkSemaphore> sa(*vk.m_CPUAllocator);
  STLAllocator<VkPipelineStageFlags> psfa(*vk.m_CPUAllocator);
  Vector<VkSemaphore> waitSemaphores(sa);
  Vector<VkPipelineStageFlags> waitStages(psfa);

  waitSemaphores.push_back(
      vk.m_ImageAvailableSemaphores[vk.m_CurrentFrameIndex]);
  waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  if (vk.m_RunComputeCommands) {
    waitSemaphores.push_back(
        vk.m_ComputeFinishedSemaphores[vk.m_CurrentFrameIndex]);
    waitStages.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }

  VkSemaphore signalSemaphores[] = {
      vk.m_RenderFinishedSemaphores[vk.m_CurrentFrameIndex]};
  submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
  submitInfo.pWaitSemaphores = waitSemaphores.data();
  submitInfo.pWaitDstStageMask = waitStages.data();
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &vk.m_GraphicsCommandBuffers[imageIndex];
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkResetFences(vk.m_LogicalDevice, 1,
                &vk.m_FrameInFlightFences[vk.m_CurrentFrameIndex]);

  if (vkQueueSubmit(vk.m_GraphicsQueue, 1, &submitInfo,
                    vk.m_FrameInFlightFences[vk.m_CurrentFrameIndex]) !=
      VK_SUCCESS) {
    VKU_LOG_ERR("VulkanAPI : Failed to submit draw command buffer!");
  }

  if (vk.m_UseImGui) {
    RenderImGui(vk, vk.m_CurrentFrameIndex);
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  presentInfo.swapchainCount = 1;
  VkSwapchainKHR swapchains[] = {vk.m_SwapChain};
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pResults = nullptr;

  result = vkQueuePresentKHR(vk.m_GraphicsQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    init::RecreateSwapChain(vk);
    return;
  } else if (result != VK_SUCCESS) {
    VKU_LOG_ERR("VulkanAPI : Error presenting swapchain image");
  }

  vk.m_CurrentFrameIndex = (vk.m_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vku::submission::RenderImGui(VkState &vk, uint32_t frameIndex) {
  ImGui::Render();
  VkCommandBuffer imguiCommandBuffer = commands::BeginSingleTimeCommands(vk);
  debug::BeginDebugMarker(imguiCommandBuffer, "ImGui",
                          {0.0f, 0.0f, 1.0f, 1.0f});

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = vk.m_ImGuiRenderPass;
  renderPassInfo.framebuffer = vk.m_SwapChainFramebuffers[frameIndex];
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