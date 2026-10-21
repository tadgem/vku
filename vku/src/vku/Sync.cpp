#include "vku/Sync.h"

void vku::sync::ImageBarrier(VkCommandBuffer         cmd,
                             VkImage                 img,
                             VkImageLayout           newLayout,
                             VkAccessFlags           srcAccessMask,
                             VkAccessFlags           dstAccessMask,
                             VkPipelineStageFlagBits srcPipelineStage,
                             VkPipelineStageFlagBits dstPipelineStage,
                             VkImageAspectFlagBits   imageAspect)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout                       = newLayout;
	barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	barrier.image                           = img;
	barrier.subresourceRange.aspectMask     = imageAspect;
	barrier.subresourceRange.baseMipLevel   = 0;
	barrier.subresourceRange.levelCount     = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount     = 1;
	barrier.srcAccessMask                   = srcAccessMask;
	barrier.dstAccessMask                   = dstAccessMask;

	vkCmdPipelineBarrier(cmd, srcPipelineStage, dstPipelineStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
