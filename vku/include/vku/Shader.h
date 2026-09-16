#pragma once
#include "vku/Descriptor.h"
#include "vku/Structs.h"
#include "vku/Utils.h"
#include VKU_FILESYSTEM_ALIAS
namespace vku {
VkShaderModule CreateShaderModule(VkState &vk, const StageBinary &data);
VkShaderModule CreateShaderModuleRaw(VkState &vk, const char *data,
                                     size_t length);

StageBinary CreateStageBinaryFromSource(VkState &vk, ShaderStageType type,
                                        const String &source,
                                        const char *shaderName);

struct ShaderStage {
  String m_Name;
  StageBinary m_StageBinary;
  VkShaderModule m_Module;
  Vector<PushConstantBlock> m_PushConstants;
  Vector<DescriptorSetLayoutData> m_LayoutDatas;
  ShaderStageType m_Type;

  ShaderStage(IAllocator &alloc);

  static String LoadShaderSource(VkState &vk, const char *path);

  static ShaderStage CreateFromBinary(VkState &vk,
                                      Vector<unsigned char> &binary,
                                      const ShaderStageType &type,
                                      const char *name) {
    auto stageLayoutDatas = descriptor::ReflectDescriptorSetLayouts(vk, binary);
    auto pushConstants = descriptor::ReflectPushConstants(vk, binary);
    auto module = CreateShaderModule(vk, binary);

    ShaderStage stage(*vk.m_CPUAllocator);
    stage.m_Name = name;
    stage.m_StageBinary = binary;
    stage.m_Module = module;
    stage.m_PushConstants = pushConstants;
    stage.m_LayoutDatas = stageLayoutDatas;
    stage.m_Type = type;

    return stage;
  }

  static ShaderStage CreateFromBinaryPath(VkState &vk, const char *stagePath,
                                          const ShaderStageType &stageType) {
    String name(
        VKU_FILESYSTEM_NS::filesystem::path(stagePath).filename().string(),
        *vk.m_CPUAllocator);

    auto stageBin = vk.m_Backend->LoadBinaryFromPath(vk, stagePath);
    return CreateFromBinary(vk, stageBin, stageType,
                            VKU_FILESYSTEM_NS::filesystem::path(stagePath)
                                .filename()
                                .string()
                                .c_str());
  }

  static ShaderStage CreateFromSource(VkState &vk, const String &source,
                                      const ShaderStageType &type,
                                      const char *name, const char *path = "") {
    auto bin = CreateStageBinaryFromSource(vk, type, source, path);
    if (bin.empty()) {
      return ShaderStage(*vk.m_CPUAllocator);
    }
    auto stageLayoutDatas = descriptor::ReflectDescriptorSetLayouts(vk, bin);
    auto pushConstants = descriptor::ReflectPushConstants(vk, bin);
    auto module = CreateShaderModule(vk, bin);

    ShaderStage stage(*vk.m_CPUAllocator);
    stage.m_Name = name;
    stage.m_StageBinary = bin;
    stage.m_Module = module;
    stage.m_PushConstants = pushConstants;
    stage.m_LayoutDatas = stageLayoutDatas;
    stage.m_Type = type;

    return VKU_MEMORY_NS::move(stage);
  }

  static ShaderStage CreateFromSourcePath(VkState &vk, const char *path,
                                          const ShaderStageType &type) {
    String name(*vk.m_CPUAllocator);
    auto source = LoadShaderSource(vk, path);
    return CreateFromSource(
        vk, source, type, path,
        VKU_FILESYSTEM_NS::filesystem::path(path).filename().string().c_str());
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
    descriptor::CreateDescriptorSetLayout(vk, vert.m_LayoutDatas,
                                          frag.m_LayoutDatas, layout);
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

  static ShaderProgram CreateGraphicsFromSourcePath(VkState &vk,
                                                    const char *vertPath,
                                                    const char *fragPath) {
    ShaderStage vert = ShaderStage::CreateFromSourcePath(
        vk, vertPath, ShaderStageType::Vertex);
    ShaderStage frag = ShaderStage::CreateFromSourcePath(
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

  static ShaderProgram
  CreateComputeFromSourcePath(VkState &vk, const char *compute_src_path) {
    ShaderStage comp = ShaderStage::CreateFromSourcePath(
        vk, compute_src_path, ShaderStageType::Compute);
    return CreateCompute(vk, comp);
  }

  static Optional<ShaderProgram> CreateShaderSlang(VkState &vk,
                                                   const String &name,
                                                   const Span<String> &entries);
};

} // namespace vku