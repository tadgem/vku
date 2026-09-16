#pragma once
#include "vku/Structs.h"

namespace vku::render_passes {

    void  CreateRenderPass(VkState& vk, VkRenderPass& renderPass, Vector<VkAttachmentDescription>& colourAttachments, Vector<VkAttachmentDescription>& resolveAttachments, bool hasDepthAttachment = true, VkAttachmentDescription depthAttachment = {}, VkAttachmentLoadOp attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);
    void  BeginSwapchainRenderPass(VkState& vk, VkCommandBuffer& cmd);

}