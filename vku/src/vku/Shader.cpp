#include "vku/Shader.h"
#include "slang.h"
#include "vku/Log.h"
#include "vku/Macros.h"
#include "volk.h"

#include VKU_FILESYSTEM_ALIAS
#include VKU_STRING_STREAM_ALIAS
#include VKU_REGEX_ALIAS

namespace vku {

// #ifdef VKU_RT_SHADER_COMPILATION
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
  sessionDesc.targets = &targetDesc;
  sessionDesc.targetCount = 1;

  slang::ISession *session = nullptr;
  g_Slang->createSession(sessionDesc, &session);

  return session;
}

static bool ReportSlangError(slang::IBlob *diagnostics) {
  if (!diagnostics)
    return true;
  printf("%s\n", (const char *)diagnostics->getBufferPointer());
  diagnostics->release();
  return false;
}

static Optional<ShaderStage> CompileSlangEntry(VkState &vk,
                                               slang::ISession *session,
                                               slang::IModule *module,
                                               const char *entryName) {

  slang::IBlob *diagnostics = nullptr;
  slang::IEntryPoint *entry = nullptr;

  if (SLANG_FAILED(module->findEntryPointByName(entryName, &entry)) ||
      !entry) {
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
  auto *module = session->loadModule(name.c_str());

  if (!module) {
    VKU_LOG_ERR("Failed to load shader {}", name.c_str());
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
    utils::Combine(*vk.m_CPUAllocator, datas, stage.m_LayoutDatas);
  }

  VkDescriptorSetLayout descriptorSetLayout = {};
  descriptor::CreateDescriptorSetLayout(vk, datas, descriptorSetLayout);

  return ShaderProgram(*vk.m_CPUAllocator, shaderStages, descriptorSetLayout);
}

// #endif

ShaderStage::ShaderStage(IAllocator &alloc)
    : m_PushConstants(alloc), m_LayoutDatas(alloc), m_Name(alloc) {}

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
    : m_DescriptorSetLayout(layout), m_Stages(shaderStages),
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

VkShaderModule CreateShaderModuleRaw(VkState &vk, const char *data,
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

void RecurseStringInclude(VkState &vk, String inputDir, String &output,
                          const String &path) {
  String input(*vk.m_CPUAllocator);
  String dir(*vk.m_CPUAllocator);
  String finalDir = inputDir + "/" + path;
  input = vk.m_Backend->LoadStringFromPath(vk, finalDir.c_str());
  dir = std::filesystem::path(path).parent_path().string();
  IStringStream iss(input);
  std::regex include_dir_regex("\\\"(.*)\\\"");
  for (std::string line; std::getline(iss, line);) {
    if (line.find("#include") != String::npos) {
      auto words_begin =
          std::sregex_iterator(line.begin(), line.end(), include_dir_regex);
      auto words_end = std::sregex_iterator();
      for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        const std::smatch &match = *i;
        // remove first and last ""
        String include_dir(*vk.m_CPUAllocator);
        include_dir = match.str().substr(1, match.str().size() - 2);
        RecurseStringInclude(vk, inputDir, output, include_dir);
      }

      RecurseStringInclude(vk, dir, output, input);
    } else {
      output += line + "\n";
    }
  }
}

String ShaderStage::LoadShaderSource(VkState &vk, const char *path) {
  String final_shader_src(*vk.m_CPUAllocator);
  String parent_path(*vk.m_CPUAllocator);
  String filename(*vk.m_CPUAllocator);
  std::filesystem::path inputPath(path);
  parent_path = inputPath.parent_path().string();
  filename = inputPath.filename().string();

  RecurseStringInclude(vk, parent_path, final_shader_src, filename);
  // do includes
  return final_shader_src;
}
} // namespace vku
