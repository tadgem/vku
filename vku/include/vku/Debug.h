#pragma once
#include "vku/Structs.h"

namespace vku {
namespace debug {
VkDebugUtilsLabelEXT CreateDebugLabel(const char *label,
                                      Array<float, 4> col = {1.0f, 1.0f, 1.0f,
                                                             1.0f});
void BeginDebugMarker(VkCommandBuffer &cmd, const char *label,
                      Array<float, 4> col = {1.0f, 1.0f, 1.0f, 1.0f});
void EndDebugMarker(VkCommandBuffer &cmd);

} // namespace debug
}