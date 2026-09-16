#pragma once
#include "vku/Structs.h"

namespace vku {
namespace defaults {
inline static RasterizationState DefaultRasterState{
    VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, false, VK_COMPARE_OP_LESS,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

inline static RasterizationState DefaultRasterStateMSAA{
    VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, true, VK_COMPARE_OP_LESS,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

inline static RasterizationState CullNoneRasterState{
    VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false, VK_COMPARE_OP_LESS,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

inline static RasterizationState CullNoneRasterStateMSAA{
    VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, true, VK_COMPARE_OP_LESS,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

} // namespace defaults
}