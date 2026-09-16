#include "vku/Pipeline.h"
#include "vku/Log.h"

namespace vku::pipelines {

VkPipelineShaderStageCreateInfo
CreateShaderStageInfo(VkShaderStageFlagBits shaderStage,
                      VkShaderModule &module) {
  VkPipelineShaderStageCreateInfo shaderStageInfo{};
  shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo.stage = shaderStage;
  shaderStageInfo.module = module;
  shaderStageInfo.pName = "main";
  return shaderStageInfo;
}

VkPipelineVertexInputStateCreateInfo
CreatePipelineVertexInputState(vku::VertexDescription &vertexDescription) {
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertexDescription.m_BindingDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.m_BindingDescriptions.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertexDescription.m_AttributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.m_AttributeDescriptions.data();

  return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo
CreatePipelineInputAssemblyState(vku::RasterizationState &pipelineState) {
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
  inputAssemblyInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyInfo.topology = pipelineState.m_InputAssemblyTopology;
  inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
  return inputAssemblyInfo;
}

vku::VkViewportData CreateViewportData(VkExtent2D resolution,
                                       vku::RasterizationState rasterState) {
  using namespace vku;
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(resolution.width);
  viewport.height = static_cast<float>(resolution.height);
  if (rasterState.m_DepthCompareOp != VK_COMPARE_OP_NEVER) {
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
  }
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D{resolution.width, resolution.height};

  auto data = VkViewportData{viewport, scissor, {}};

  VkPipelineViewportStateCreateInfo viewportInfo{};
  viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportInfo.viewportCount = 1;
  viewportInfo.pViewports = &data.m_Viewport;
  viewportInfo.scissorCount = 1;
  viewportInfo.pScissors = &data.m_Scissor;

  data.m_CreateInfo = viewportInfo;
  return data;
}

VkPipelineRasterizationStateCreateInfo
CreateRasterizationState(vku::RasterizationState rasterState) {
  VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
  rasterizerInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizerInfo.depthClampEnable = VK_FALSE;

  rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
  // using anything other than FILL requires enableing a gpu feature.
  rasterizerInfo.polygonMode = rasterState.m_PolygonMode;
  // thickness of lines in terms of pixels. > 1.0f requires wide lines gpu
  // feature.
  rasterizerInfo.lineWidth = rasterState.m_LineWidth;

  rasterizerInfo.cullMode = rasterState.m_CullMode;
  rasterizerInfo.frontFace = rasterState.m_FrontFace;

  rasterizerInfo.depthBiasEnable = VK_FALSE;
  rasterizerInfo.depthBiasConstantFactor = 0.0f;
  rasterizerInfo.depthBiasClamp = 0.0f;
  rasterizerInfo.depthBiasSlopeFactor = 0.0f;
  return rasterizerInfo;
}

VkPipelineMultisampleStateCreateInfo
CreateMultiSampleInfo(vku::VkState &vk, vku::RasterizationState rasterState) {
  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

  if (rasterState.m_EnableMSAA) {
    sampleCount = vk.m_MaxMsaaSamples;
  }

  // ToDo : Do something with enableMultisampling here
  VkPipelineMultisampleStateCreateInfo multisampleInfo{};
  multisampleInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleInfo.sampleShadingEnable =
      static_cast<VkBool32>(rasterState.m_EnableMSAA);
  multisampleInfo.rasterizationSamples = sampleCount;
  multisampleInfo.minSampleShading = .2f;
  multisampleInfo.pSampleMask = nullptr;
  multisampleInfo.alphaToCoverageEnable = VK_FALSE;
  multisampleInfo.alphaToOneEnable = VK_FALSE;
  return multisampleInfo;
}

vku::PipelineAttachmentState
CreateAttachmentState(vku::IAllocator &alloc, uint32_t colourAttachmentCount) {
  using namespace vku;
  PipelineAttachmentState state(alloc);
  for (uint32_t i = 0; i < colourAttachmentCount; i++) {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // TODO: We only want to blend on the first attachment for our use-cases atm
    // Might need to support blending multiple attachments in future.
    colorBlendAttachment.blendEnable = i == 0 ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    state.m_ColourAttachmentStates.push_back(colorBlendAttachment);
  }

  state.m_BlendStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  state.m_BlendStateInfo.logicOpEnable = VK_FALSE;
  state.m_BlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
  state.m_BlendStateInfo.attachmentCount =
      static_cast<uint32_t>(state.m_ColourAttachmentStates.size());
  state.m_BlendStateInfo.pAttachments = state.m_ColourAttachmentStates.data();
  return state;
}

vku::PipelineAttachmentState
CreateGBufferAttachmentState(vku::IAllocator &alloc,
                             uint32_t colourAttachmentCount,
                             uint32_t writeAttachmentIndex) {
  using namespace vku;
  PipelineAttachmentState state(alloc);
  for (uint32_t i = 0; i < colourAttachmentCount; i++) {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    if (i == writeAttachmentIndex) {
      colorBlendAttachment.colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      colorBlendAttachment.blendEnable = VK_TRUE;
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      colorBlendAttachment.dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
      colorBlendAttachment.colorWriteMask = 0;
      colorBlendAttachment.blendEnable = VK_FALSE;
    }

    state.m_ColourAttachmentStates.push_back(colorBlendAttachment);
  }

  state.m_BlendStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  state.m_BlendStateInfo.logicOpEnable = VK_FALSE;
  state.m_BlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
  state.m_BlendStateInfo.attachmentCount =
      static_cast<uint32_t>(state.m_ColourAttachmentStates.size());
  state.m_BlendStateInfo.pAttachments = state.m_ColourAttachmentStates.data();
  return state;
}

vku::PipelineDynamicState CreateDynamicStateInfo(vku::IAllocator &alloc) {
  using namespace vku;
  PipelineDynamicState dynamicState(alloc);
  dynamicState.m_DynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicState.m_DynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

  dynamicState.m_DynamicStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.m_DynamicStateInfo.dynamicStateCount =
      static_cast<uint32_t>(dynamicState.m_DynamicStates.size());
  dynamicState.m_DynamicStateInfo.pDynamicStates =
      dynamicState.m_DynamicStates.data();

  return dynamicState;
}

VkPipelineDepthStencilStateCreateInfo
CreateDepthStencilState(vku::RasterizationState &rasterState) {
  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = rasterState.m_DepthCompareOp;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // Optional
  depthStencil.maxDepthBounds = 1.0f; // Optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // Optional
  depthStencil.back = {};  // Optional

  return depthStencil;
}

VkPipelineData CreateRasterPipeline(VkState &vk, ShaderProgram &shader,
                                    VertexDescription &vertexDescription,
                                    RasterizationState &rasterState,
                                    VkRenderPass &pipelineRenderPass,
                                    VkExtent2D resolution,
                                    uint32_t colorAttachmentCount) {

  VkShaderModule vertShaderModule = shader.m_Stages[0].m_Module;
  VkShaderModule fragShaderModule = shader.m_Stages[1].m_Module;

  VkPipelineShaderStageCreateInfo vertexShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);

  Array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = {
      vertexShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
      CreatePipelineVertexInputState(vertexDescription);

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
      CreatePipelineInputAssemblyState(rasterState);

  VkViewportData viewport = CreateViewportData(resolution, rasterState);

  VkPipelineRasterizationStateCreateInfo rasterizerInfo =
      CreateRasterizationState(rasterState);

  VkPipelineMultisampleStateCreateInfo multisampleInfo =
      CreateMultiSampleInfo(vk, rasterState);

  PipelineAttachmentState attachmentState =
      CreateAttachmentState(*vk.m_CPUAllocator, colorAttachmentCount);

  PipelineDynamicState dynamicState =
      CreateDynamicStateInfo(*vk.m_CPUAllocator);

  VkPipelineDepthStencilStateCreateInfo depthStencil =
      CreateDepthStencilState(rasterState);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      shader.GetPipelineLayoutCreateInfo();

  VkPipelineLayout pipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(vk.m_LogicalDevice, &pipelineLayoutInfo,
                                  nullptr, &pipelineLayout))

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStageCreateInfos.data();

  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineCreateInfo.pViewportState = &viewport.m_CreateInfo;
  pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
  pipelineCreateInfo.pMultisampleState = &multisampleInfo;
  pipelineCreateInfo.pColorBlendState = &attachmentState.m_BlendStateInfo;
  pipelineCreateInfo.pDynamicState = &dynamicState.m_DynamicStateInfo;

  rasterState.m_DepthCompareOp != VK_COMPARE_OP_NEVER
      ? pipelineCreateInfo.pDepthStencilState = &depthStencil
      : pipelineCreateInfo.pDepthStencilState = nullptr;

  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = pipelineRenderPass;
  pipelineCreateInfo.subpass = 0;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1,
                                     &pipelineCreateInfo, nullptr, &pipeline))

  vkDestroyShaderModule(vk.m_LogicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(vk.m_LogicalDevice, fragShaderModule, nullptr);

  return {pipeline, pipelineLayout};
}

VkPipelineData CreateRasterPipeline(VkState &vk, ShaderProgram &shader,
                                    VertexDescription &vertexDescription,
                                    RasterizationState &rasterState,
                                    VkRenderPass &pipelineRenderPass,
                                    VkExtent2D resolution,
                                    PipelineAttachmentState &attachmentState) {

  VkShaderModule vertShaderModule = shader.m_Stages[0].m_Module;
  VkShaderModule fragShaderModule = shader.m_Stages[1].m_Module;

  VkPipelineShaderStageCreateInfo vertexShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);

  Array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = {
      vertexShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
      CreatePipelineVertexInputState(vertexDescription);

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
      CreatePipelineInputAssemblyState(rasterState);

  VkViewportData viewport = CreateViewportData(resolution, rasterState);

  VkPipelineRasterizationStateCreateInfo rasterizerInfo =
      CreateRasterizationState(rasterState);

  VkPipelineMultisampleStateCreateInfo multisampleInfo =
      CreateMultiSampleInfo(vk, rasterState);

  PipelineDynamicState dynamicState =
      CreateDynamicStateInfo(*vk.m_CPUAllocator);

  VkPipelineDepthStencilStateCreateInfo depthStencil =
      CreateDepthStencilState(rasterState);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      shader.GetPipelineLayoutCreateInfo();

  VkPipelineLayout pipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(vk.m_LogicalDevice, &pipelineLayoutInfo,
                                  nullptr, &pipelineLayout))

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStageCreateInfos.data();

  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineCreateInfo.pViewportState = &viewport.m_CreateInfo;
  pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
  pipelineCreateInfo.pMultisampleState = &multisampleInfo;
  pipelineCreateInfo.pColorBlendState = &attachmentState.m_BlendStateInfo;
  pipelineCreateInfo.pDynamicState = &dynamicState.m_DynamicStateInfo;

  rasterState.m_DepthCompareOp != VK_COMPARE_OP_NEVER
      ? pipelineCreateInfo.pDepthStencilState = &depthStencil
      : pipelineCreateInfo.pDepthStencilState = nullptr;

  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = pipelineRenderPass;
  pipelineCreateInfo.subpass = 0;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1,
                                     &pipelineCreateInfo, nullptr, &pipeline))

  vkDestroyShaderModule(vk.m_LogicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(vk.m_LogicalDevice, fragShaderModule, nullptr);

  return {pipeline, pipelineLayout};
}

VkPipelineData
CreateComputePipeline(VkState &vk, StageBinary &comp,
                      VkDescriptorSetLayout &descriptorSetLayout) {

  auto compStage = CreateShaderModule(vk, comp);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

  VkPipelineLayout pipelineLayout;
  if (vkCreatePipelineLayout(vk.m_LogicalDevice, &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    VKU_LOG_ERR("failed to create compute pipeline layout!");
    vkDestroyShaderModule(vk.m_LogicalDevice, compStage, nullptr);
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  VkPipelineShaderStageCreateInfo compShaderStageInfo{};
  compShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  compShaderStageInfo.module = compStage;
  compShaderStageInfo.pName = "main";

  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.stage = compShaderStageInfo;

  VkPipeline pipeline;

  if (vkCreateComputePipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1,
                               &pipelineInfo, nullptr,
                               &pipeline) != VK_SUCCESS) {
    VKU_LOG_ERR("failed to create compute pipeline!");
    vkDestroyShaderModule(vk.m_LogicalDevice, compStage, nullptr);
    vkDestroyPipelineLayout(vk.m_LogicalDevice, pipelineLayout, nullptr);
    return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  }

  vkDestroyShaderModule(vk.m_LogicalDevice, compStage, nullptr);
  return {pipeline, pipelineLayout};
}

VkPipelineData CreateDynamicRasterPipeline(VkState &vk, ShaderProgram &shader,
                                           VertexDescription &vertexDescription,
                                           RasterizationState &rasterState,
                                           VkExtent2D resolution,
                                           Vector<VkFormat> colourAttachments) {
  VkShaderModule vertShaderModule = shader.m_Stages[0].m_Module;
  VkShaderModule fragShaderModule = shader.m_Stages[1].m_Module;

  VkPipelineShaderStageCreateInfo vertexShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);

  Array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = {
      vertexShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
      CreatePipelineVertexInputState(vertexDescription);

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
      CreatePipelineInputAssemblyState(rasterState);

  VkViewportData viewport = CreateViewportData(resolution, rasterState);

  VkPipelineRasterizationStateCreateInfo rasterizerInfo =
      CreateRasterizationState(rasterState);

  VkPipelineMultisampleStateCreateInfo multisampleInfo =
      CreateMultiSampleInfo(vk, rasterState);

  PipelineAttachmentState attachmentState = CreateAttachmentState(
      *vk.m_CPUAllocator, static_cast<uint32_t>(colourAttachments.size()));

  PipelineDynamicState dynamicState =
      CreateDynamicStateInfo(*vk.m_CPUAllocator);

  VkPipelineDepthStencilStateCreateInfo depthStencil =
      CreateDepthStencilState(rasterState);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      shader.GetPipelineLayoutCreateInfo();

  VkPipelineLayout pipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(vk.m_LogicalDevice, &pipelineLayoutInfo,
                                  nullptr, &pipelineLayout))

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStageCreateInfos.data();

  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineCreateInfo.pViewportState = &viewport.m_CreateInfo;
  pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
  pipelineCreateInfo.pMultisampleState = &multisampleInfo;
  pipelineCreateInfo.pColorBlendState = &attachmentState.m_BlendStateInfo;
  pipelineCreateInfo.pDynamicState = &dynamicState.m_DynamicStateInfo;

  rasterState.m_DepthCompareOp != VK_COMPARE_OP_NEVER
      ? pipelineCreateInfo.pDepthStencilState = &depthStencil
      : pipelineCreateInfo.pDepthStencilState = nullptr;

  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = nullptr;
  pipelineCreateInfo.subpass = 0;

  VkPipelineRenderingCreateInfoKHR dynRenderingCreateInfo{};
  dynRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  dynRenderingCreateInfo.colorAttachmentCount =
      static_cast<uint32_t>(colourAttachments.size());
  dynRenderingCreateInfo.pColorAttachmentFormats = colourAttachments.data();
  // Todo: Support other depth formats
  VkFormat depthStencilFormat =
      rasterState.m_DepthCompareOp == VK_COMPARE_OP_NEVER
          ? VK_FORMAT_UNDEFINED
          : utils::FindDepthFormat(vk);
  dynRenderingCreateInfo.depthAttachmentFormat = depthStencilFormat;
  if (utils::HasStencilComponent(depthStencilFormat)) {
    dynRenderingCreateInfo.stencilAttachmentFormat = depthStencilFormat;
  }

  pipelineCreateInfo.pNext = &dynRenderingCreateInfo;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1,
                                     &pipelineCreateInfo, nullptr, &pipeline))

  vkDestroyShaderModule(vk.m_LogicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(vk.m_LogicalDevice, fragShaderModule, nullptr);

  return {pipeline, pipelineLayout};
}

VkPipelineData CreateDynamicRasterPipeline(
    VkState &vk, ShaderProgram &shader, VertexDescription &vertexDescription,
    RasterizationState &rasterState, VkExtent2D resolution,
    Vector<VkFormat> colourAttachments,
    PipelineAttachmentState &attachmentState) {

  VkShaderModule vertShaderModule = shader.m_Stages[0].m_Module;
  VkShaderModule fragShaderModule = shader.m_Stages[1].m_Module;

  VkPipelineShaderStageCreateInfo vertexShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
      CreateShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);

  Array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = {
      vertexShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
      CreatePipelineVertexInputState(vertexDescription);

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
      CreatePipelineInputAssemblyState(rasterState);

  VkViewportData viewport = CreateViewportData(resolution, rasterState);

  VkPipelineRasterizationStateCreateInfo rasterizerInfo =
      CreateRasterizationState(rasterState);

  VkPipelineMultisampleStateCreateInfo multisampleInfo =
      CreateMultiSampleInfo(vk, rasterState);

  PipelineDynamicState dynamicState =
      CreateDynamicStateInfo(*vk.m_CPUAllocator);

  VkPipelineDepthStencilStateCreateInfo depthStencil =
      CreateDepthStencilState(rasterState);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      shader.GetPipelineLayoutCreateInfo();

  VkPipelineLayout pipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(vk.m_LogicalDevice, &pipelineLayoutInfo,
                                  nullptr, &pipelineLayout))

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStageCreateInfos.data();

  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineCreateInfo.pViewportState = &viewport.m_CreateInfo;
  pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
  pipelineCreateInfo.pMultisampleState = &multisampleInfo;
  pipelineCreateInfo.pColorBlendState = &attachmentState.m_BlendStateInfo;
  pipelineCreateInfo.pDynamicState = &dynamicState.m_DynamicStateInfo;

  rasterState.m_DepthCompareOp != VK_COMPARE_OP_NEVER
      ? pipelineCreateInfo.pDepthStencilState = &depthStencil
      : pipelineCreateInfo.pDepthStencilState = nullptr;

  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = nullptr;
  pipelineCreateInfo.subpass = 0;

  VkPipelineRenderingCreateInfoKHR dynRenderingCreateInfo{};
  dynRenderingCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  dynRenderingCreateInfo.colorAttachmentCount =
      static_cast<uint32_t>(colourAttachments.size());
  dynRenderingCreateInfo.pColorAttachmentFormats = colourAttachments.data();
  VkFormat depthStencilFormat =
      rasterState.m_DepthCompareOp == VK_COMPARE_OP_NEVER
          ? VK_FORMAT_UNDEFINED
          : utils::FindDepthFormat(vk);
  dynRenderingCreateInfo.depthAttachmentFormat = depthStencilFormat;
  if (utils::HasStencilComponent(depthStencilFormat)) {
    dynRenderingCreateInfo.stencilAttachmentFormat = depthStencilFormat;
  }

  pipelineCreateInfo.pNext = &dynRenderingCreateInfo;

  VkPipeline pipeline;
  VK_CHECK(vkCreateGraphicsPipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1,
                                     &pipelineCreateInfo, nullptr, &pipeline))

  vkDestroyShaderModule(vk.m_LogicalDevice, vertShaderModule, nullptr);
  vkDestroyShaderModule(vk.m_LogicalDevice, fragShaderModule, nullptr);

  return {pipeline, pipelineLayout};
}
} // namespace vku::pipelines
