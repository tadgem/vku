#pragma once
#include "vku/Structs.h"

namespace vku::sync
{
void ImageBarrier(VkCommandBuffer         cmd,
                  VkImage                 img,
                  VkImageLayout           newLayout,
                  VkAccessFlags           srcAccessMask,
                  VkAccessFlags           dstAccessMask,
                  VkPipelineStageFlagBits srcPipelineStage,
                  VkPipelineStageFlagBits dstPipelineStage,
                  VkImageAspectFlagBits   imageAspect = VK_IMAGE_ASPECT_COLOR_BIT);
}
