#include "vku/Shader.h"
#include "vku/Log.h"
#include "vku/Macros.h"
#include "volk.h"

#include VKU_FILESYSTEM_ALIAS
#include VKU_STRING_STREAM_ALIAS
#include VKU_REGEX_ALIAS

#if defined(VKU_ENABLE_SLANG)
#include "slang.h"
#endif

#if defined(VKU_RT_SHADER_COMPILATION)
#include "shaderc/shaderc.h"
#endif

#include <cstring>
#include <string>

namespace vku {

#if defined(VKU_RT_SHADER_COMPILATION)
static shaderc_shader_kind GetShadercShaderKind(ShaderStageType type) {
  switch (type) {
  case ShaderStageType::Vertex:
    return shaderc_glsl_vertex_shader;
  case ShaderStageType::Fragment:
    return shaderc_glsl_fragment_shader;
  case ShaderStageType::Compute:
    return shaderc_glsl_compute_shader;
  default:
    return shaderc_glsl_vertex_shader;
  }
}

StageBinary CreateStageBinaryFromSource(VkState &vk, ShaderStageType type,
                                        const String &source,
                                        const char *shaderName) {
  StageBinary bin(*vk.m_CPUAllocator);

  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  if (compiler == nullptr) {
    VKU_LOG_ERR("Failed to initialize shaderc compiler");
    return bin;
  }

  shaderc_compile_options_t options = shaderc_compile_options_initialize();
  shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan,
                                         shaderc_env_version_vulkan_1_2);
  shaderc_compile_options_set_optimization_level(
      options, shaderc_optimization_level_performance);
  // Preserve OpName debug info so spirv-reflect can recover descriptor /
  // member names (required by vku's name-based Material binding lookup).
  shaderc_compile_options_set_generate_debug_info(options);

  shaderc_compilation_result_t result = shaderc_compile_into_spv(
      compiler, source.c_str(), source.size(), GetShadercShaderKind(type),
      shaderName, "main", options);

  if (result == nullptr) {
    VKU_LOG_ERR("Failed to compile shader '%s' : shaderc returned null",
                shaderName);
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);
    return bin;
  }

  if (shaderc_result_get_compilation_status(result) !=
      shaderc_compilation_status_success) {
    VKU_LOG_ERR("Failed to compile shader '%s' : %s", shaderName,
                shaderc_result_get_error_message(result));
    shaderc_result_release(result);
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);
    return bin;
  }

  VKU_LOG_INFO("shaderc: compilation successful for '%s'", shaderName);

  const char *bytes = shaderc_result_get_bytes(result);
  const size_t length = shaderc_result_get_length(result);

  if (bytes == nullptr || length == 0) {
    VKU_LOG_ERR("Failed to compile shader '%s' : no SPIR-V produced",
                shaderName);
    shaderc_result_release(result);
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);
    return bin;
  }

  bin.resize(length);
  std::memcpy(bin.data(), bytes, length);

  shaderc_result_release(result);
  shaderc_compile_options_release(options);
  shaderc_compiler_release(compiler);

  return bin;
}
#endif

#if defined(VKU_ENABLE_SLANG)
static slang::IGlobalSession *g_Slang = nullptr;

static void InitializeSlang() {
  if (SLANG_FAILED(slang::createGlobalSession(&g_Slang))) {
    VKU_LOG_ERR("Failed to create slang::IGlobalSession");
  }
}

static slang::ISession *GetSlangSession() {
  if (g_Slang == nullptr) {
    InitializeSlang();
  }
  slang::TargetDesc targetDesc = {};
  targetDesc.format = SLANG_SPIRV;
  targetDesc.profile = g_Slang->findProfile("spirv_1_5");

  slang::SessionDesc sessionDesc = {};
  sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
  sessionDesc.targets = &targetDesc;
  sessionDesc.targetCount = 1;

  slang::ISession *session = nullptr;
  g_Slang->createSession(sessionDesc, &session);

  return session;
}

static bool ReportSlangError(slang::IBlob *diagnostics) {
  if (!diagnostics)
    return true;

  const char *msg = static_cast<const char *>(diagnostics->getBufferPointer());
  VKU_LOG_ERR("Slang Error : %s", msg);
  diagnostics->release();
  return false;
}

static Optional<ShaderStage> CompileSlangEntry(VkState &vk,
                                               slang::ISession *session,
                                               slang::IModule *module,
                                               const char *entryName) {

  slang::IBlob *diagnostics = nullptr;
  slang::IEntryPoint *entry = nullptr;

  if (SLANG_FAILED(module->findEntryPointByName(entryName, &entry)) || !entry) {
    VKU_LOG_ERR("Failed to find entry point {} in shader", entryName);
    return {};
  }

  // One composite per stage: { module, singleEntryPoint }
  slang::IComponentType *components[] = {module, entry};
  slang::IComponentType *program = nullptr;
  session->createCompositeComponentType(components, 2, &program, &diagnostics);
  if (!ReportSlangError(diagnostics) || !program) {
    VKU_LOG_ERR("Failed to create stage composite {}", entryName);
    entry->release();
    return {};
  }

  slang::IComponentType *linked = nullptr;
  program->link(&linked, &diagnostics);
  program->release();
  if (!ReportSlangError(diagnostics) || !linked) {
    VKU_LOG_ERR("Failed to link stage named {}", entryName);
    entry->release();
    return {};
  }

  slang::IBlob *code = nullptr;
  linked->getEntryPointCode(
      0, 0, &code, &diagnostics); // this program has exactly one entry point
  linked->release();
  entry->release();
  if (!ReportSlangError(diagnostics) || !code) {
    VKU_LOG_ERR("Failed to get entry point named {} post link", entryName);
    return {};
  }

  Vector<uint8_t> spirv(*vk.m_CPUAllocator);
  spirv.resize(code->getBufferSize());

  std::memcpy(spirv.data(), code->getBufferPointer(), code->getBufferSize());
  code->release();

  return ShaderStage::CreateFromBinary(vk, spirv, entryName);
}

Optional<ShaderProgram>
ShaderProgram::CreateShaderSlang(VkState &vk, const String &name,
                                 std::initializer_list<const char *> entries) {

  slang::IBlob *diagnostics = nullptr;

  auto *session = GetSlangSession();
  auto *module = session->loadModule(name.c_str(), &diagnostics);

  if (!module) {
    VKU_LOG_ERR("Failed to load shader %s", name.c_str());
    ReportSlangError(diagnostics);
    return {};
  }

  Vector<ShaderStage> shaderStages(*vk.m_CPUAllocator);
  shaderStages.reserve(entries.size());

  for (const char *entry : entries) {
    auto result = CompileSlangEntry(vk, session, module, entry);
    if (result.has_value()) {
      shaderStages.push_back(result.value());
    } else {
      VKU_LOG_ERR("Failed to compile slang entry {}", entry);
    }
  }

  Vector<DescriptorSetLayoutData> datas(*vk.m_CPUAllocator);

  for (const auto &stage : shaderStages) {
    datas = utils::Combine(*vk.m_CPUAllocator, datas, stage.m_LayoutDatas);
  }

  VkDescriptorSetLayout descriptorSetLayout = {};
  descriptor::CreateDescriptorSetLayout(vk, datas, descriptorSetLayout);

  return ShaderProgram(*vk.m_CPUAllocator, shaderStages, descriptorSetLayout);
}
#endif // VKU_ENABLE_SLANG

ShaderStage::ShaderStage(IAllocator &alloc)
    : m_Name(alloc), m_StageBinary(alloc), m_PushConstants(alloc),
      m_LayoutDatas(alloc) {}

void ShaderProgram::Free(VkState &vk) {
  vkDestroyDescriptorSetLayout(vk.m_LogicalDevice, m_DescriptorSetLayout,
                               nullptr);
}

ShaderProgram ShaderProgram::CreateCompute(VkState &vk, ShaderStage &compute) {
  VkDescriptorSetLayout layout;

  Vector<VkDescriptorSetLayoutBinding> bindings(*vk.m_CPUAllocator);
  for (auto &layoutData : compute.m_LayoutDatas) {
    for (auto &binding : layoutData.m_Bindings) {
      bindings.push_back(binding);
    }
  }
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  VK_CHECK(vkCreateDescriptorSetLayout(vk.m_LogicalDevice, &layoutInfo, nullptr,
                                       &layout))

  Vector<ShaderStage> stages(*vk.m_CPUAllocator);
  stages.push_back(compute);
  auto shader = ShaderProgram(*vk.m_CPUAllocator, stages, layout);
  return shader;
}

ShaderProgram::ShaderProgram(IAllocator &alloc,
                             Vector<ShaderStage> shaderStages,
                             VkDescriptorSetLayout layout)
    : m_Stages(shaderStages), m_DescriptorSetLayout(layout),
      m_PushConstantRanges(alloc) {
  BuildPushConstantRanges();
}

void ShaderProgram::BuildPushConstantRanges() {
  // update
  // valid combos:
  // 1 stage has 1 push constant block
  // both stages share the same push constant block
  // each stage has a separate push constant block
  // e.g. only ever 1 block per stage
  for (auto &stage : m_Stages) {
    VKU_LOG_INFO("DBGPUSH stage=%s pushConstantCount=%d", stage.m_Name.c_str(),
                 (int)stage.m_PushConstants.size());
    for (auto &pc : stage.m_PushConstants) {
      VKU_LOG_INFO("DBGPUSH   name=%s offset=%u size=%u stageFlags=%u",
                   pc.m_Name.c_str(), pc.m_Offset, pc.m_Size,
                   (uint32_t)pc.m_Stage);
    }
    if (stage.m_PushConstants.empty()) {
      continue;
    }

    if (stage.m_PushConstants.size() > 1) {
      VKU_LOG_ERR("VulkanAPI : CreateRasterizationPipeline : Supplied stage "
                  "has more than 1 push constant block, this is not allowed.");
      continue;
    }

    PushConstantBlock &block = stage.m_PushConstants[0];

    bool skip = false;

    for (auto &range : m_PushConstantRanges) {
      if (block.m_Offset == range.offset && block.m_Size == range.size) {
        range.stageFlags |= block.m_Stage;
      }
    }

    if (skip) {
      continue;
    }

    VkPushConstantRange range{};
    range.offset = block.m_Offset;
    range.size = block.m_Size;
    range.stageFlags = block.m_Stage;

    m_PushConstantRanges.push_back(range);
  }
}

uint32_t ShaderProgram::GetPushConstantRangeCount() {
  return static_cast<uint32_t>(m_PushConstantRanges.size());
}

VkPipelineLayoutCreateInfo ShaderProgram::GetPipelineLayoutCreateInfo() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = GetPushConstantRangeCount();
  pipelineLayoutInfo.pPushConstantRanges =
      !m_PushConstantRanges.empty() ? m_PushConstantRanges.data() : nullptr;

  return pipelineLayoutInfo;
}

VkShaderModule CreateShaderModule(VkState &vk, const StageBinary &data) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = static_cast<uint32_t>(data.size());
  createInfo.pCode = reinterpret_cast<const uint32_t *>(data.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(vk.m_LogicalDevice, &createInfo, nullptr,
                           &shaderModule) != VK_SUCCESS) {
    VKU_LOG_ERR("Failed to create shader module!");
  }
  return shaderModule;
}

VkShaderModule CreateShaderModuleRaw(VkState &vk, const unsigned char *data,
                                     size_t length) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = static_cast<uint32_t>(length);
  createInfo.pCode = reinterpret_cast<const uint32_t *>(data);

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(vk.m_LogicalDevice, &createInfo, nullptr,
                           &shaderModule) != VK_SUCCESS) {
    VKU_LOG_ERR("Failed to create shader module!");
  }
  return shaderModule;
}

// Recursively resolves #include "..." directives relative to the including
// file's directory, producing a single self contained GLSL source string.
static void ResolveShaderIncludes(VkState &vk, const std::string &filePath,
                                  String &output, int depth) {
  if (depth > 32) {
    VKU_LOG_ERR("Shader include depth exceeded while resolving '%s'",
                filePath.c_str());
    return;
  }

  String loaded = vk.m_Backend->LoadStringFromPath(vk, filePath.c_str());
  std::string source(loaded.c_str());

  const std::string dir =
      VKU_FILESYSTEM_NS::filesystem::path(filePath).parent_path().string();

  IStringStream iss(loaded.c_str());
  for (std::string line; std::getline(iss, line);) {
    const size_t includePos = line.find("#include");
    if (includePos != std::string::npos) {
      const size_t firstQuote = line.find('"');
      const size_t lastQuote = line.rfind('"');
      if (firstQuote != std::string::npos && lastQuote != std::string::npos &&
          lastQuote > firstQuote) {
        const std::string includeName =
            line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
        const std::string includePath =
            dir.empty() ? includeName : dir + "/" + includeName;
        ResolveShaderIncludes(vk, includePath, output, depth + 1);
        continue;
      }
    }
    output += line + "\n";
  }
}

String ShaderStage::LoadShaderSource(VkState &vk, const char *path) {
  String output(*vk.m_CPUAllocator);
  ResolveShaderIncludes(vk, std::string(path), output, 0);
  return output;
}
} // namespace vku
