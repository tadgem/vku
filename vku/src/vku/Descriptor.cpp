#include "vku/Descriptor.h"
#include "vku/Log.h"
#include "vku/Macros.h"

namespace vku {
namespace descriptor {

Vector<VkDescriptorSetLayoutBinding>
CleanDescriptorSetLayout(IAllocator &alloc,
                         Vector<VkDescriptorSetLayoutBinding> &arr) {
  STLAllocator<VkDescriptorSetLayoutBinding> a(alloc);
  Vector<VkDescriptorSetLayoutBinding> clean(a);

  for (int k = 0; k < arr.size(); k++) {
    auto &layoutSetData = arr[k];
    if (clean.empty()) {
      clean.push_back(layoutSetData);
      continue;
    }

    int start = static_cast<int>(clean.size() - 1);
    bool updated = false;
    for (int i = start; i >= 0; i--) {
      auto &newLayoutSet = clean[i];
      if (newLayoutSet.binding == layoutSetData.binding) {
        if (newLayoutSet.descriptorCount == layoutSetData.descriptorCount &&
            newLayoutSet.descriptorType == layoutSetData.descriptorType) {
          clean[i].stageFlags =
              layoutSetData.stageFlags + newLayoutSet.stageFlags;
          updated = true;
          break;
        }
      }
    }

    if (!updated) {
      clean.push_back(layoutSetData);
    }
  }

  return clean;
}

ShaderBindingType GetBindingType(const SpvReflectDescriptorBinding &binding) {
  if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
    return ShaderBindingType::UniformBuffer;
  }
  if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
    return ShaderBindingType::ShaderStorageBuffer;
  }
  if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER ||
      binding.descriptor_type ==
          SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
    return ShaderBindingType::Sampler;
  }

  if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
    return ShaderBindingType::SampledImage;
  }

  return ShaderBindingType::UniformBuffer;
}

ShaderBufferMemberType
GetTypeFromSpvReflect(SpvReflectTypeDescription *typeDescription) {

  if (typeDescription->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
    if (typeDescription->traits.numeric.matrix.column_count == 4 &&
        typeDescription->traits.numeric.matrix.row_count == 4) {
      return ShaderBufferMemberType::_mat4;
    }

    if (typeDescription->traits.numeric.matrix.column_count == 3 &&
        typeDescription->traits.numeric.matrix.row_count == 3) {
      return ShaderBufferMemberType::_mat3;
    }

    return ShaderBufferMemberType::UNKNOWN;
  }

  if (typeDescription->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) {
    if (typeDescription->traits.numeric.vector.component_count == 2) {
      return ShaderBufferMemberType::_vec2;
    }

    if (typeDescription->traits.numeric.vector.component_count == 3) {
      return ShaderBufferMemberType::_vec3;
    }

    if (typeDescription->traits.numeric.vector.component_count == 4) {
      return ShaderBufferMemberType::_vec4;
    }
    return ShaderBufferMemberType::UNKNOWN;
  }

  if (typeDescription->type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY) {
    return ShaderBufferMemberType::_array;
  }

  if (typeDescription->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
    return ShaderBufferMemberType::_float;
  }

  if (typeDescription->type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
    // signedness: unsigned = 0, signed = 1(?)
    // might need to come back to this if we need 64 bit ints.
    if (typeDescription->traits.numeric.scalar.signedness == 0) {
      return ShaderBufferMemberType::_uint;
    } else {
      return ShaderBufferMemberType::_int;
    }
  }

  return ShaderBufferMemberType::UNKNOWN;
}

Vector<VkDescriptorSetLayoutBinding>
GetDescriptorSetLayoutBindings(VkState &vk,
                               Vector<DescriptorSetLayoutData> &layoutDatas) {
  Vector<VkDescriptorSetLayoutBinding> bindings(*vk.m_CPUAllocator);
  uint8_t count = 0;

  for (auto &layoutData : layoutDatas) {
    count += static_cast<uint8_t>(layoutData.m_Bindings.size());
  }

  bindings.resize(count);

  count = 0;
  // .. do the things
  for (auto &layoutData : layoutDatas) {
    for (auto &binding : layoutData.m_Bindings) {
      bindings[count] = binding;
      count++;
    }
  }

  return CleanDescriptorSetLayout(*vk.m_CPUAllocator, bindings);
}

void CreateDescriptorSetLayout(VkState &vk,
                               Vector<DescriptorSetLayoutData> &layoutDatas,
                               VkDescriptorSetLayout &descriptorSetLayout) {
  STLAllocator<VkDescriptorSetLayoutBinding> a(*vk.m_CPUAllocator);
  Vector<VkDescriptorSetLayoutBinding> bindings(a);
  uint8_t count = 0;

  for (auto &fragLayoutData : layoutDatas) {
    count += static_cast<uint8_t>(fragLayoutData.m_Bindings.size());
  }

  bindings.resize(count);

  count = 0;
  // .. do the things
  for (auto &vertLayoutData : layoutDatas) {
    for (auto &binding : vertLayoutData.m_Bindings) {
      bindings[count] = binding;
      count++;
    }
  }

  Vector<VkDescriptorSetLayoutBinding> cleanBindings =
      CleanDescriptorSetLayout(*vk.m_CPUAllocator, bindings);

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(cleanBindings.size());
  layoutInfo.pBindings = cleanBindings.data();
  layoutInfo.bindingCount = cleanBindings.size();

  VK_CHECK(vkCreateDescriptorSetLayout(vk.m_LogicalDevice, &layoutInfo, nullptr,
                                       &descriptorSetLayout))
}

Vector<DescriptorSetLayoutData>
ReflectDescriptorSetLayouts(VkState &vk, StageBinary &stageBin) {
  return ReflectDescriptorSetLayoutsRaw(vk, stageBin.data(), stageBin.size());
}

Vector<PushConstantBlock> ReflectPushConstants(VkState &vk,
                                               StageBinary &stageBin) {
  return ReflectPushConstantsRaw(vk, stageBin.data(), stageBin.size());
}

VkDescriptorSet CreateDescriptorSet(VkState &vk,
                                    DescriptorSetLayoutData &layoutData) {
  return vk.m_DescriptorSetAllocator.Allocate(
      *vk.m_CPUAllocator, vk.m_LogicalDevice, layoutData.m_Layout, nullptr);
}

Vector<DescriptorSetLayoutData>
ReflectDescriptorSetLayoutsRaw(VkState &vk, const unsigned char *stage_bin,
                               size_t stage_size) {
  STLAllocator<DescriptorSetLayoutData> dsld_alloc(*vk.m_CPUAllocator.get());
  SpvReflectShaderModule shaderReflectModule;
  SpvReflectResult result =
      spvReflectCreateShaderModule(stage_size, stage_bin, &shaderReflectModule);

  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    VKU_LOG_ERR("Descriptor.cpp : Failed to reflect descriptor set layouts.");
    return Vector(dsld_alloc);
  }
  uint32_t descriptorSetCount = 0;
  spvReflectEnumerateDescriptorSets(&shaderReflectModule, &descriptorSetCount,
                                    nullptr);

  if (descriptorSetCount == 0) {
    return Vector(dsld_alloc);
  }

  Vector<SpvReflectDescriptorSet *> reflectedDescriptorSets(*vk.m_CPUAllocator);
  reflectedDescriptorSets.resize(descriptorSetCount);
  spvReflectEnumerateDescriptorSets(&shaderReflectModule, &descriptorSetCount,
                                    &reflectedDescriptorSets[0]);

  Vector<DescriptorSetLayoutData> layoutDatas(*vk.m_CPUAllocator);

  for (int idx = 0; idx < reflectedDescriptorSets.size(); idx++) {
    const SpvReflectDescriptorSet &reflectedSet = *reflectedDescriptorSets[idx];
    DescriptorSetLayoutData layoutData =
        DescriptorSetLayoutData(*vk.m_CPUAllocator);

    layoutData.m_Bindings.resize(reflectedSet.binding_count);
    for (uint32_t bc = 0; bc < reflectedSet.binding_count; bc++) {
      const SpvReflectDescriptorBinding &reflectedBinding =
          *reflectedSet.bindings[bc];
      VkDescriptorSetLayoutBinding &layoutBinding = layoutData.m_Bindings[bc];
      layoutBinding.binding = reflectedBinding.binding;
      layoutBinding.descriptorType =
          static_cast<VkDescriptorType>(reflectedBinding.descriptor_type);
      layoutBinding.descriptorCount = 1; // sus
      for (uint32_t i_dim = 0; i_dim < reflectedBinding.array.dims_count;
           ++i_dim) {
        layoutBinding.descriptorCount *= reflectedBinding.array.dims[i_dim];
      }
      layoutBinding.stageFlags =
          static_cast<VkShaderStageFlagBits>(shaderReflectModule.shader_stage);

      ShaderBindingType bufferType = GetBindingType(reflectedBinding);
      STLAllocator<ShaderBufferMember> sbma(*vk.m_CPUAllocator);
      DescriptorSetLayoutBindingData binding(*vk.m_CPUAllocator);
      binding.m_BindingName = String(reflectedBinding.name, *vk.m_CPUAllocator);
      binding.m_BindingIndex = reflectedBinding.binding;
      binding.m_ExpectedBufferSizeOrDivisor = reflectedBinding.block.size;
      binding.m_BufferType = bufferType;
      binding.m_Members = Vector<ShaderBufferMember>(sbma);

      for (uint32_t blockIdx = 0;
           blockIdx < reflectedBinding.block.member_count; blockIdx++) {
        auto member = reflectedBinding.block.members[blockIdx];
        ShaderBufferMember reflectedMember(*vk.m_CPUAllocator);
        reflectedMember.m_Name = String(member.name, *vk.m_CPUAllocator);
        reflectedMember.m_Offset =
            member.absolute_offset; // this might be an issue with padded types?
        reflectedMember.m_Size = member.padded_size;
        reflectedMember.m_Type = GetTypeFromSpvReflect(member.type_description);

        if (member.array.dims_count > 0) {
          reflectedMember.m_Stride = member.array.stride;
        } else {
          reflectedMember.m_Stride = 0;
        }

        binding.m_Members.push_back(reflectedMember);
      }
      if (reflectedBinding.resource_type & SPV_REFLECT_RESOURCE_FLAG_SAMPLER) {
        ShaderBufferMember reflectedMember(*vk.m_CPUAllocator);
        reflectedMember.m_Name =
            String(reflectedBinding.name, *vk.m_CPUAllocator);
        reflectedMember.m_Type = ShaderBufferMemberType::_sampler;
        binding.m_Members.push_back(reflectedMember);
      }
      layoutData.m_BindingDatas.push_back(binding);
    }

    layoutData.m_SetNumber = reflectedSet.set;
    layoutData.m_CreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutData.m_CreateInfo.bindingCount = reflectedSet.binding_count;
    layoutData.m_CreateInfo.pBindings = layoutData.m_Bindings.data();

    layoutDatas.push_back(layoutData);
  }

  return layoutDatas;
}

Vector<PushConstantBlock>
ReflectPushConstantsRaw(VkState &vk, const unsigned char *stage_bin,
                        size_t stage_size) {
  STLAllocator<PushConstantBlock> pcba(*vk.m_CPUAllocator);
  Vector<PushConstantBlock> pushConstants(pcba);
  SpvReflectShaderModule shaderReflectModule;
  SpvReflectResult result =
      spvReflectCreateShaderModule(stage_size, stage_bin, &shaderReflectModule);

  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    VKU_LOG_ERR("Descriptor.cpp : Failed to reflect push constants from shader "
                "binary.");
    return Vector(pcba);
  }
  uint32_t pushConstantBlockCount = 0;
  spvReflectEnumeratePushConstantBlocks(&shaderReflectModule,
                                        &pushConstantBlockCount, nullptr);
  STLAllocator<SpvReflectBlockVariable *> alloc(*vk.m_CPUAllocator);
  Vector<SpvReflectBlockVariable *> reflectedPushConstantBlocks(alloc);
  reflectedPushConstantBlocks.resize(pushConstantBlockCount);
  spvReflectEnumeratePushConstantBlocks(&shaderReflectModule,
                                        &pushConstantBlockCount,
                                        reflectedPushConstantBlocks.data());

  for (uint32_t i = 0; i < pushConstantBlockCount; i++) {
    const SpvReflectBlockVariable &pcBlock = *reflectedPushConstantBlocks[i];
    PushConstantBlock pcb(*vk.m_CPUAllocator);
    pcb.m_Size = pcBlock.size;
    pcb.m_Offset = pcBlock.offset;
    pcb.m_Name = pcBlock.name;
    pcb.m_Stage = (VkShaderStageFlags)shaderReflectModule.shader_stage;
    pushConstants.push_back(pcb);
  }

  return pushConstants;
}
} // namespace descriptor
} // namespace vku
