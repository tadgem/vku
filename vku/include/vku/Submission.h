
#pragma once
#include "vku/Structs.h"

namespace vku {
namespace submission {
void SubmitFrame(VkState &vk);
void RenderImGui(VkState &vk, uint32_t frameIndex);

} // namespace submission

} // namespace vku