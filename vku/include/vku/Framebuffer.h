#pragma once
#include "vku/Macros.h"
#include "vku/RenderPass.h"
#include "Utils.h"
#include "vku/Texture.h"

namespace vku
{
    class Attachment
    {
    public:

        Vector<Texture>         m_AttachmentSwapchainImages;
        VkFormat                m_Format;
        VkSampleCountFlagBits   m_SampleCount;

        Attachment(VkState& vk)
            : m_AttachmentSwapchainImages(STLAllocator<Texture>(*vk.m_CPUAllocator))
        {

        }

        void Free(VkState & vk)
        {
            for (auto& t : m_AttachmentSwapchainImages)
            {
                t.Free(vk);
            }
        }

        static Attachment CreateColourAttachment(VkState & vk, VkExtent2D resolution,
            uint32_t numMips, VkSampleCountFlagBits sampleCount,
            VkFormat format, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkImageAspectFlagBits imageAspect,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            Attachment a(vk);
            a.m_Format = format;
            a.m_SampleCount = sampleCount;

            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                a.m_AttachmentSwapchainImages.push_back(Texture::CreateAttachment(vk, resolution.width, resolution.height, numMips, sampleCount, format, tiling, usageFlags, memoryFlags, imageAspect, samplerFilter, samplerAddressMode));
            }
            return a;
        }

        static Attachment CreateDepthAttachment(VkState & vk, VkExtent2D resolution,
            uint32_t numMips, VkSampleCountFlagBits sampleCount,
            VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkImageAspectFlagBits imageAspect,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            Attachment a(vk);
            a.m_Format = utils::FindDepthFormat(vk);
            a.m_SampleCount = sampleCount;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                a.m_AttachmentSwapchainImages.push_back(Texture::CreateAttachment(vk, resolution.width, resolution.height, numMips, sampleCount, a.m_Format, tiling, usageFlags, memoryFlags, imageAspect, samplerFilter, samplerAddressMode));
            }
            return a;
        }

    };

    class Framebuffer
    {
    public:
        Vector<Attachment>  m_ColourAttachments;
        Vector<Attachment>  m_DepthAttachments;
        Vector<Attachment>  m_ResolveAttachments;

        // Render pass
        RenderPassInfo        m_RenderPassInfo;

        Vector <VkClearValue>       m_ClearValues;
        VkAttachmentLoadOp          m_AttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkExtent2D                  m_Resolution;

        Framebuffer(IAllocator& alloc) : 
            m_ColourAttachments(alloc),
            m_DepthAttachments(alloc),
            m_ResolveAttachments(alloc),
            m_RenderPassInfo(alloc),
            m_ClearValues(alloc)
        {  }

        void AddColourAttachment(VkState& vk, VkExtent2D resolution,
            uint32_t numMips,  VkFormat format, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkSampleCountFlagBits sampleCount,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            m_ColourAttachments.push_back(Attachment::CreateColourAttachment(vk,
                resolution, numMips, sampleCount, format, usageFlags, memoryFlags, VK_IMAGE_ASPECT_COLOR_BIT, samplerFilter, samplerAddressMode));
            for (auto& img : m_ColourAttachments.back().m_AttachmentSwapchainImages)
            {
                textures::TransitionImageLayout(vk, img.m_Image, format, numMips, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
            }
            m_Resolution = resolution;
        }
        
        void AddColourAttachment(VkState & vk, ResolutionScale scale,
            uint32_t numMips, VkFormat format, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkSampleCountFlagBits sampleCount,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            VkExtent2D resolution = vk.m_MaxFramebufferExtent;
            ResolveResolutionScale(scale, resolution.width, resolution.height, resolution.width, resolution.height);
            AddColourAttachment(vk, resolution, numMips, format, usageFlags, memoryFlags, sampleCount, samplerFilter, samplerAddressMode);
        }

        void AddDepthAttachment(VkState& vk, VkExtent2D resolution,
            uint32_t numMips, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkSampleCountFlagBits sampleCount,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            m_DepthAttachments.push_back(Attachment::CreateDepthAttachment(vk,
                resolution, numMips, sampleCount, usageFlags, memoryFlags, VK_IMAGE_ASPECT_DEPTH_BIT, samplerFilter, samplerAddressMode));
            for (auto& img : m_DepthAttachments.back().m_AttachmentSwapchainImages)
            {
                textures::TransitionImageLayout(vk, img.m_Image, utils::FindDepthFormat(vk), numMips, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
            m_Resolution = resolution;

        }

        void AddDepthAttachment(VkState & vk, ResolutionScale scale,
            uint32_t numMips, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkSampleCountFlagBits sampleCount,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            VkExtent2D resolution = vk.m_MaxFramebufferExtent;
            ResolveResolutionScale(scale, resolution.width, resolution.height, resolution.width, resolution.height);
            AddDepthAttachment(vk, resolution, numMips, usageFlags, memoryFlags, sampleCount, samplerFilter, samplerAddressMode);
        }

        void AddResolveAttachment(VkState& vk, VkExtent2D resolution,
            uint32_t numMips, VkFormat format, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkImageAspectFlagBits imageAspect,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            m_ResolveAttachments.push_back(Attachment::CreateColourAttachment(vk,
                resolution, numMips, vk.m_MaxMsaaSamples, format, usageFlags, memoryFlags, imageAspect, samplerFilter, samplerAddressMode));
            for (auto& img : m_ResolveAttachments.back().m_AttachmentSwapchainImages)
            {
                textures::TransitionImageLayout(vk, img.m_Image, utils::FindDepthFormat(vk), numMips, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
            }
            m_Resolution = resolution;
        }

        void AddResolveAttachment(VkState & vk, ResolutionScale scale,
            uint32_t numMips, VkFormat format, VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlagBits memoryFlags, VkImageAspectFlagBits imageAspect,
            VkFilter samplerFilter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
        {
            VkExtent2D resolution = vk.m_MaxFramebufferExtent;
            ResolveResolutionScale(scale, resolution.width, resolution.height, resolution.width, resolution.height);
            AddResolveAttachment(vk, resolution, numMips, format, usageFlags, memoryFlags, imageAspect, samplerFilter, samplerAddressMode);
        }

        void Build(VkState & vk);
        void BeginDynamicRendering(VkState& vk, VkCommandBuffer& cmd, uint32_t frameIndex);

        void Free(VkState & vk);
      private:

        void BuildRenderPasses(VkState& vk);
    };
}