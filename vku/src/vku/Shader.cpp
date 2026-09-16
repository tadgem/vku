#include "vku/Shader.h"
#include "vku/Macros.h"
#include "vku/Log.h"
#include "volk.h"
#include "shaderc/shaderc.h"
#include VKU_FILESYSTEM_ALIAS
#include VKU_STRING_STREAM_ALIAS
#include VKU_REGEX_ALIAS

namespace vku {

ShaderStage::ShaderStage(IAllocator& alloc) :
    m_PushConstants(alloc),
    m_LayoutDatas(alloc),
    m_StageBinary(alloc),
    m_Name(alloc)
{
}

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
  auto shader =  ShaderProgram(*vk.m_CPUAllocator, stages, layout);
  return shader;
}

ShaderProgram::ShaderProgram(
    IAllocator& alloc, 
    Vector<ShaderStage> shaderStages,
    VkDescriptorSetLayout layout) :
    
    m_DescriptorSetLayout(layout), 
    m_Stages(shaderStages),
    m_PushConstantRanges(alloc)
{
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
      VKU_LOG_ERR(
          "VulkanAPI : CreateRasterizationPipeline : Supplied stage has more than 1 push constant block, this is not allowed.");
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
  pipelineLayoutInfo.pPushConstantRanges = !m_PushConstantRanges.empty() ?
                                            m_PushConstantRanges.data() :
                                            nullptr;

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

shaderc_shader_kind GetShadercShaderKind(vku::ShaderStageType type)
{
  switch(type)
  {
    case vku::ShaderStageType::Vertex:
      return shaderc_shader_kind ::shaderc_glsl_vertex_shader;
    case vku::ShaderStageType::Fragment:
      return shaderc_shader_kind ::shaderc_glsl_fragment_shader;
    case vku::ShaderStageType::Compute:
      return shaderc_shader_kind ::shaderc_glsl_compute_shader;
    default:
      return shaderc_shader_kind ::shaderc_compute_shader;
  }
}

StageBinary CreateStageBinaryFromSource(VkState &vk, ShaderStageType type,
                                    const String &source, const char* shaderName) {
  shaderc_compiler* c = shaderc_compiler_initialize();
  shaderc_compile_options_t opt {};

  auto result = shaderc_compile_into_spv(c,
                          source.c_str(),
                          source.size(),
                          GetShadercShaderKind(type),
                          shaderName,
                          "main",
                           opt);

  const char* spirv_bytes = shaderc_result_get_bytes(result);
  size_t      spirv_size  = shaderc_result_get_length(result);
  StageBinary bin (*vk.m_CPUAllocator);
  if(shaderc_result_get_num_errors(result) != 0)
  {
      VKU_LOG_ERR("Failed to compile shader : %s",
                    shaderc_result_get_error_message(result));
      return bin;
  }

  bin.resize(spirv_size);
  for(auto i = 0; i < spirv_size; i++)
  {
      bin[i] = spirv_bytes[i];
  }

  shaderc_result_release(result);
  shaderc_compiler_release(c);

  return bin;
}

void RecurseStringInclude(VkState& vk, String inputDir, String& output, const String& path)
{
  String input(*vk.m_CPUAllocator);
  String dir(*vk.m_CPUAllocator);
  String finalDir = inputDir + "/" + path;
  input = vk.m_Backend->LoadStringFromPath(vk, finalDir.c_str());
  dir = std::filesystem::path(path).parent_path().string();
  IStringStream iss(input);
  std::regex include_dir_regex("\\\"(.*)\\\"");
  for (std::string line; std::getline(iss, line); )
  {
      if(line.find("#include") != String::npos)
      {
        auto words_begin =
            std::sregex_iterator(line.begin(), line.end(), include_dir_regex);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
          const std::smatch& match = *i;
          // remove first and last ""
          String include_dir(*vk.m_CPUAllocator);
          include_dir = match.str().substr(1, match.str().size() - 2);
          RecurseStringInclude(vk, inputDir, output, include_dir);
        }

        RecurseStringInclude(vk, dir, output, input);
      }
      else
      {
        output += line + "\n";
      }
  }
}

String ShaderStage::LoadShaderSource(VkState& vk, const char* path) {
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
}
