#include "vku/Framebuffer.h"
#include "volk.h"

void vku::Framebuffer::Free(VkState & vk)
{
    for (auto& t : m_ColourAttachments)
    {
        t.Free(vk);
    }
    for (auto& t : m_DepthAttachments)
    {
        t.Free(vk);
    }
    for (auto& t : m_ResolveAttachments)
    {
        t.Free(vk);
    }

    for (auto& fb : m_RenderPassInfo.m_SwapchainFramebuffers)
    {
        vkDestroyFramebuffer(vk.m_LogicalDevice, fb, nullptr);
    }
}

void vku::Framebuffer::Build(vku::VkState &vk)
{
    BuildRenderPasses(vk);
}

void vku::Framebuffer::BuildRenderPasses(vku::VkState &vk) {
    // build renderpass & dynamic rendering state
    STLAllocator<VkAttachmentDescription> attachmentDescriptionAlloc(*vk.m_CPUAllocator);
    Vector<VkAttachmentDescription> colourAttachmentDescriptions(attachmentDescriptionAlloc);
    for (auto& col : m_ColourAttachments)
    {
        VkAttachmentDescription colourAttachment{};
        colourAttachment.format = col.m_Format;
        colourAttachment.samples = col.m_SampleCount;
        colourAttachment.loadOp = m_AttachmentLoadOp;
        colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colourAttachmentDescriptions.push_back(colourAttachment);
    }

    Vector<VkAttachmentDescription> resolveAttachmentDescriptions(attachmentDescriptionAlloc);

    for (auto& col : m_ResolveAttachments)
    {
        VkAttachmentDescription resolveAttachment{};
        resolveAttachment.format = col.m_Format;
        resolveAttachment.samples = col.m_SampleCount;
        resolveAttachment.loadOp = m_AttachmentLoadOp;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        resolveAttachmentDescriptions.push_back(resolveAttachment);
    }

    VkAttachmentDescription depthAttachmentDescription{};
    bool hasDepth = !m_DepthAttachments.empty();
    if (hasDepth)
    {
        Attachment& depth = m_DepthAttachments[0];
        depthAttachmentDescription.format = depth.m_Format;
        depthAttachmentDescription.samples = depth.m_SampleCount;
        depthAttachmentDescription.loadOp = m_AttachmentLoadOp;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    render_passes::CreateRenderPass(vk, m_RenderPassInfo.m_RenderPass,
                                    colourAttachmentDescriptions, resolveAttachmentDescriptions, hasDepth,
                                    depthAttachmentDescription, m_AttachmentLoadOp);

    STLAllocator<VkImageView> iva(*vk.m_CPUAllocator);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        Vector<VkImageView> framebufferAttachments(iva);
        for (auto& colour : m_ColourAttachments)
        {
          framebufferAttachments.push_back(colour.m_AttachmentSwapchainImages[i].m_ImageView);
        }
        if (hasDepth)
        {
          framebufferAttachments.push_back(m_DepthAttachments[0].m_AttachmentSwapchainImages[i].m_ImageView);
        }
        for (auto& resolve : m_ResolveAttachments)
        {
          framebufferAttachments.push_back(resolve.m_AttachmentSwapchainImages[i].m_ImageView);
        }
        VkFramebuffer fb;
        textures::CreateFramebuffer(vk, framebufferAttachments, m_RenderPassInfo.m_RenderPass, m_Resolution, fb);
        m_RenderPassInfo.m_SwapchainFramebuffers.push_back(fb);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_RenderPassInfo.m_RenderPass;
        renderPassInfo.framebuffer = m_RenderPassInfo.m_SwapchainFramebuffers[i];
        renderPassInfo.renderArea.offset = { 0,0 };
        renderPassInfo.renderArea.extent = m_Resolution;

        renderPassInfo.clearValueCount = static_cast<uint32_t>(m_ClearValues.size());
        renderPassInfo.pClearValues = m_ClearValues.data();
        m_RenderPassInfo.m_RenderPassInfos.push_back(renderPassInfo);

    }
}

void vku::Framebuffer::BeginDynamicRendering(vku::VkState& vk, VkCommandBuffer &cmd, uint32_t frameIndex) {
    STLAllocator<VkRenderingAttachmentInfoKHR> raia(*vk.m_CPUAllocator);
    Vector<VkRenderingAttachmentInfoKHR> colourInfos (raia);
    for (auto &col : m_ColourAttachments) {
          VkRenderingAttachmentInfoKHR curr = {};

          curr.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          curr.imageView = col.m_AttachmentSwapchainImages[frameIndex].m_ImageView;
          curr.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
          curr.loadOp = m_AttachmentLoadOp;
          curr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
          curr.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
          colourInfos.push_back(curr);
    }
    Vector<VkRenderingAttachmentInfoKHR> depthInfos(raia);

    if(!m_DepthAttachments.empty()) {
        for (auto &depth : m_DepthAttachments) {
            VkRenderingAttachmentInfoKHR curr = {};
            curr.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            curr.imageView = depth.m_AttachmentSwapchainImages[frameIndex].m_ImageView;
            curr.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            curr.loadOp = m_AttachmentLoadOp;
            curr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            curr.clearValue.depthStencil = {1.0f, 0};
            depthInfos.push_back(curr);
        }
    }
    Vector<VkRenderingAttachmentInfoKHR> resolveInfos (raia);

    for (auto &resolve : m_ResolveAttachments) {
          VkRenderingAttachmentInfoKHR curr {};

          curr.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          curr.resolveImageView = resolve.m_AttachmentSwapchainImages[frameIndex].m_ImageView;
          curr.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
          curr.loadOp = m_AttachmentLoadOp;
          curr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
          curr.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
          resolveInfos.push_back(curr);
    }

    VkRenderingInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.colorAttachmentCount = static_cast<uint32_t>(colourInfos.size());
    info.pColorAttachments = colourInfos.data();
    info.pDepthAttachment = depthInfos.empty() ? nullptr : depthInfos.data();
    // todo: support render area offset;
    info.renderArea = {{0,0}, m_Resolution};

    VkFormat depthFormat = utils::FindDepthFormat(vk);
    if(utils::HasStencilComponent(depthFormat))
    {
      info.pStencilAttachment = depthInfos.empty() ? nullptr : depthInfos.data();
    }

    info.layerCount = 1;

    vkCmdBeginRenderingKHR(cmd, &info);

}

