#pragma once
#include "vku/Descriptor.h"
#include "vku/Structs.h"
#include "vku/Utils.h"
#include VKU_FILESYSTEM_ALIAS
#include <initializer_list>
namespace vku {
VkShaderModule CreateShaderModule(VkState &vk, const StageBinary &data);
VkShaderModule CreateShaderModuleRaw(VkState &vk, const unsigned char *data,
                                     size_t length);

#if defined(VKU_RT_SHADER_COMPILATION)
// Compile GLSL source to SPIR-V at runtime using shaderc.
StageBinary CreateStageBinaryFromSource(VkState &vk, ShaderStageType type,
                                        const String &source,
                                        const char *shaderName);
#endif

struct ShaderStage {
  String m_Name;
  StageBinary m_StageBinary;
  VkShaderModule m_Module;
  ShaderStageType m_Type = ShaderStageType::Vertex;
  Vector<PushConstantBlock> m_PushConstants;
  Vector<DescriptorSetLayoutData> m_LayoutDatas;

  ShaderStage(IAllocator &alloc);

  static String LoadShaderSource(VkState &vk, const char *path);

  static ShaderStage CreateFromBinary(VkState &vk,
                                      Vector<unsigned char> &binary,
                                      const char *name,
                                      ShaderStageType type =
                                          ShaderStageType::Vertex) {
    auto stageLayoutDatas = descriptor::ReflectDescriptorSetLayouts(vk, binary);
    auto pushConstants = descriptor::ReflectPushConstants(vk, binary);
    auto module = CreateShaderModule(vk, binary);

    ShaderStage stage(*vk.m_CPUAllocator);
    stage.m_Name = name;
    stage.m_StageBinary = binary;
    stage.m_Module = module;
    stage.m_Type = type;
    stage.m_PushConstants = pushConstants;
    stage.m_LayoutDatas = stageLayoutDatas;

    return stage;
  }

  static ShaderStage CreateFromBinaryRaw(VkState &vk, unsigned char *binary,
                                         size_t length, const char *name,
                                         ShaderStageType type =
                                             ShaderStageType::Vertex) {
    auto stageLayoutDatas =
        descriptor::ReflectDescriptorSetLayoutsRaw(vk, binary, length);
    auto pushConstants =
        descriptor::ReflectPushConstantsRaw(vk, binary, length);
    auto module = CreateShaderModuleRaw(vk, binary, length);

    ShaderStage stage(*vk.m_CPUAllocator);
    stage.m_Name = name;
    stage.m_Module = module;
    stage.m_Type = type;
    stage.m_PushConstants = pushConstants;
    stage.m_LayoutDatas = stageLayoutDatas;

    return stage;
  }

  static ShaderStage CreateFromBinaryPath(VkState &vk, const char *stagePath,
                                          const ShaderStageType &stageType) {
    auto stageBin = vk.m_Backend->LoadBinaryFromPath(vk, stagePath);
    return CreateFromBinary(
        vk, stageBin,
        VKU_FILESYSTEM_NS::filesystem::path(stagePath).filename().string().c_str(),
        stageType);
  }

#if defined(VKU_RT_SHADER_COMPILATION)
  static ShaderStage CreateFromSource(VkState &vk, const String &source,
                                      const ShaderStageType &type,
                                      const char *name, const char *path = "") {
    auto bin = CreateStageBinaryFromSource(vk, type, source, path);
    if (bin.empty()) {
      return ShaderStage(*vk.m_CPUAllocator);
    }
    return CreateFromBinary(vk, bin, name, type);
  }

  static ShaderStage CreateFromSourcePath(VkState &vk, const char *path,
                                          const ShaderStageType &type) {
    auto source = LoadShaderSource(vk, path);
    return CreateFromSource(
        vk, source, type,
        VKU_FILESYSTEM_NS::filesystem::path(path).filename().string().c_str(),
        path);
  }
#endif
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

#if defined(VKU_RT_SHADER_COMPILATION)
  static ShaderProgram CreateGraphicsFromSourcePath(VkState &vk,
                                                    const char *vertPath,
                                                    const char *fragPath) {
    ShaderStage vert = ShaderStage::CreateFromSourcePath(
        vk, vertPath, ShaderStageType::Vertex);
    ShaderStage frag = ShaderStage::CreateFromSourcePath(
        vk, fragPath, ShaderStageType::Fragment);
    return CreateGraphics(vk, vert, frag);
  }
#endif

  static ShaderProgram CreateCompute(VkState &vk, ShaderStage &compute);

  static ShaderProgram CreateComputeFromBinaryPath(VkState &vk,
                                                   const char *comp_path) {
    ShaderStage comp = ShaderStage::CreateFromBinaryPath(
        vk, comp_path, ShaderStageType::Compute);
    return CreateCompute(vk, comp);
  }

#if defined(VKU_RT_SHADER_COMPILATION)
  static ShaderProgram CreateComputeFromSourcePath(VkState &vk,
                                                   const char *compPath) {
    ShaderStage comp = ShaderStage::CreateFromSourcePath(
        vk, compPath, ShaderStageType::Compute);
    return CreateCompute(vk, comp);
  }
#endif

#if defined(VKU_ENABLE_SLANG)
  static Optional<ShaderProgram>
  CreateShaderSlang(VkState &vk, const String &name,
                    std::initializer_list<const char *> entries);
#endif
};

} // namespace vku
