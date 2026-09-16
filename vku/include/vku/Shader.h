#pragma once
#include "vku/Descriptor.h"
#include "vku/Structs.h"
#include "vku/Utils.h"
#include VKU_FILESYSTEM_ALIAS
#include <initializer_list>
namespace vku {
VkShaderModule CreateShaderModule(VkState &vk, const StageBinary &data);
VkShaderModule CreateShaderModuleRaw(VkState &vk, const char *data,
                                     size_t length);

struct ShaderStage {
  String m_Name;
  VkShaderModule m_Module;
  Vector<PushConstantBlock> m_PushConstants;
  Vector<DescriptorSetLayoutData> m_LayoutDatas;

  ShaderStage(IAllocator &alloc);

  static String LoadShaderSource(VkState &vk, const char *path);

  static ShaderStage CreateFromBinary(VkState &vk,
                                      Vector<unsigned char> &binary,
                                      const char *name) {
    auto stageLayoutDatas = descriptor::ReflectDescriptorSetLayouts(vk, binary);
    auto pushConstants = descriptor::ReflectPushConstants(vk, binary);
    auto module = CreateShaderModule(vk, binary);

    ShaderStage stage(*vk.m_CPUAllocator);
    stage.m_Name = name;
    stage.m_Module = module;
    stage.m_PushConstants = pushConstants;
    stage.m_LayoutDatas = stageLayoutDatas;

    return stage;
  }

  static ShaderStage CreateFromBinaryPath(VkState &vk, const char *stagePath,
                                          const ShaderStageType &stageType) {
    String name(
        VKU_FILESYSTEM_NS::filesystem::path(stagePath).filename().string(),
        *vk.m_CPUAllocator);

    auto stageBin = vk.m_Backend->LoadBinaryFromPath(vk, stagePath);
    return CreateFromBinary(vk, stageBin,
                            VKU_FILESYSTEM_NS::filesystem::path(stagePath)
                                .filename()
                                .string()
                                .c_str());
  }
};

struct ShaderProgram {
  ShaderProgram(IAllocator &alloc, Vector<ShaderStage> shaderStages,
                VkDescriptorSetLayout layout);

  Vector<ShaderStage> m_Stages;

  VkDescriptorSetLayout m_DescriptorSetLayout;
  Vector<VkPushConstantRange> m_PushConstantRanges;

  void Free(VkState &vk);
  void BuildPushConstantRanges();
  uint32_t GetPushConstantRangeCount();
  VkPipelineLayoutCreateInfo GetPipelineLayoutCreateInfo();

  static ShaderProgram CreateGraphics(VkState &vk, ShaderStage &vert,
                                      ShaderStage &frag) {
    VkDescriptorSetLayout layout;
    auto layoutDatas = utils::Combine(*vk.m_CPUAllocator, vert.m_LayoutDatas,
                                      frag.m_LayoutDatas);
    descriptor::CreateDescriptorSetLayout(vk, layoutDatas, layout);
    STLAllocator<ShaderStage> alloc(*vk.m_CPUAllocator);
    Vector<ShaderStage> stages(alloc);
    stages.push_back(vert);
    stages.push_back(frag);
    return ShaderProgram(*vk.m_CPUAllocator, stages, layout);
  }

  static ShaderProgram CreateGraphicsFromBinaryPath(VkState &vk,
                                                    const char *vertPath,
                                                    const char *fragPath) {
    ShaderStage vert = ShaderStage::CreateFromBinaryPath(
        vk, vertPath, ShaderStageType::Vertex);
    ShaderStage frag = ShaderStage::CreateFromBinaryPath(
        vk, fragPath, ShaderStageType::Fragment);
    return CreateGraphics(vk, vert, frag);
  }

  static ShaderProgram CreateCompute(VkState &vk, ShaderStage &compute);

  static ShaderProgram CreateComputeFromBinaryPath(VkState &vk,
                                                   const char *comp_path) {
    ShaderStage comp = ShaderStage::CreateFromBinaryPath(
        vk, comp_path, ShaderStageType::Compute);
    return CreateCompute(vk, comp);
  }
  static Optional<ShaderProgram>
  CreateShaderSlang(VkState &vk, const String &name,
                    std::initializer_list<const char *> entries);
};

} // namespace vku