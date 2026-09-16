#include "vku/RenderPass.h"
#include "vku/Log.h"

void vku::render_passes::CreateRenderPass(
    VkState &vk, VkRenderPass &renderPass,
    Vector<VkAttachmentDescription> &colourAttachments,
    Vector<VkAttachmentDescription> &resolveAttachments,
    bool hasDepthAttachment, VkAttachmentDescription depthAttachment,
    VkAttachmentLoadOp attachmentLoadOp) {
  // Layout: Colour attachments -> Depth attachments -> Resolve attachments
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  STLAllocator<VkAttachmentDescription> ada(*vk.m_CPUAllocator);
  STLAllocator<VkAttachmentReference> ara(*vk.m_CPUAllocator);

  Vector<VkAttachmentDescription> attachments(ada);

  Vector<VkAttachmentReference> colourAttachmentReferences(ara);
  Vector<VkAttachmentReference> resolveAttachmentReferences(ara);

  uint32_t attachmentCount = 0;

  VkAttachmentReference colorAttachmentReference{};
  for (auto i = 0; i < colourAttachments.size(); i++) {
    attachments.push_back(colourAttachments[i]);
    attachments.back().loadOp = attachmentLoadOp;

    colorAttachmentReference.attachment = attachmentCount++;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    colourAttachmentReferences.push_back(colorAttachmentReference);
  }

  VkAttachmentReference depthAttachmentReference{};
  if (hasDepthAttachment) {
    attachments.push_back(depthAttachment);
    depthAttachmentReference.attachment = attachmentCount++;
    depthAttachmentReference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;
  }

  VkAttachmentReference colorAttachmentResolveReference{};
  for (auto i = 0; i < resolveAttachments.size(); i++) {
    attachments.push_back(resolveAttachments[i]);
    colorAttachmentResolveReference.attachment = attachmentCount++;
    colorAttachmentResolveReference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    resolveAttachmentReferences.push_back(colorAttachmentResolveReference);
  }

  subpass.colorAttachmentCount =
      static_cast<uint32_t>(colourAttachmentReferences.size());
  subpass.pColorAttachments = colourAttachmentReferences.data();
  subpass.pResolveAttachments = resolveAttachmentReferences.data();

  VkPipelineStageFlags srcWaitFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkPipelineStageFlags dstWaitFlags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkAccessFlags accessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  if (hasDepthAttachment) {
    srcWaitFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dstWaitFlags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    accessFlags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }

  VkSubpassDependency subpassDependency{};
  subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpassDependency.dstSubpass = 0;
  subpassDependency.srcStageMask = srcWaitFlags;
  subpassDependency.srcAccessMask = accessFlags;
  subpassDependency.dstStageMask = dstWaitFlags;
  subpassDependency.dstAccessMask = accessFlags;

  VkRenderPassCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  createInfo.pAttachments = attachments.data();
  createInfo.subpassCount = 1;
  createInfo.pSubpasses = &subpass;
  createInfo.dependencyCount = 1;
  createInfo.pDependencies = &subpassDependency;

  if (vkCreateRenderPass(vk.m_LogicalDevice, &createInfo, nullptr,
                         &renderPass) != VK_SUCCESS) {
    VKU_LOG_ERR("Failed to create Render Pass!");
    std::cerr << "Failed to create Render Pass!" << std::endl;
  }
}
void vku::render_passes::BeginSwapchainRenderPass(vku::VkState &vk,
                                                  VkCommandBuffer &cmd) {
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = vk.m_SwapchainImageRenderPass;
  renderPassInfo.framebuffer =
      vk.m_SwapChainFramebuffers[vk.m_CurrentFrameIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = vk.m_SwapChainImageExtent;

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(vk.m_SwapChainImageExtent.width);
  viewport.height = static_cast<float>(vk.m_SwapChainImageExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent =
      VkExtent2D{static_cast<uint32_t>(vk.m_SwapChainImageExtent.width),
                 static_cast<uint32_t>(vk.m_SwapChainImageExtent.height)};

  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}
