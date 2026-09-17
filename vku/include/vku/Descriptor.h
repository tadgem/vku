#pragma once
#include "ThirdParty/spirv_reflect.h"
#include "vku/Structs.h"

namespace vku {
namespace descriptor {

Vector<VkDescriptorSetLayoutBinding>
GetDescriptorSetLayoutBindings(VkState &vk,
                               Vector<DescriptorSetLayoutData> &layoutDatas);

void CreateDescriptorSetLayout(VkState &vk,
                               Vector<DescriptorSetLayoutData> &layoutData,
                               VkDescriptorSetLayout &descriptorSetLayout);

Vector<DescriptorSetLayoutData>
ReflectDescriptorSetLayouts(VkState &vk, StageBinary &stageBin);
Vector<DescriptorSetLayoutData>
ReflectDescriptorSetLayoutsRaw(VkState &vk, const unsigned char *stage_bin,
                               size_t stage_size);

Vector<PushConstantBlock> ReflectPushConstants(VkState &vk,
                                               StageBinary &stageBin);
Vector<PushConstantBlock>
ReflectPushConstantsRaw(VkState &vk, const unsigned char *stage_bin,
                        size_t stage_size);

VkDescriptorSet CreateDescriptorSet(VkState &vk,
                                    DescriptorSetLayoutData &layoutData);
ShaderBufferMemberType
GetTypeFromSpvReflect(SpvReflectTypeDescription *typeDescription);
} // namespace descriptor
} // namespace vku