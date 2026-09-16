vkCreateGraphicsPipelines(): pCreateInfos[0].pColorBlendState->pAttachments[1].blendEnable is VK_TRUE.
The Vulkan spec states: If the pipeline requires fragment output interface state and renderPass is VK_NULL_HANDLE,

for each color attachment format defined by the pColorAttachmentFormats member of VkPipelineRenderingCreateInfo,

if its potential format features do not contain VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, 

then the blendEnable member of the corresponding element of the pAttachments member of pColorBlendState must be VK_FALSE

(https://vulkan.lunarg.com/doc/view/1.3.296.0/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-renderPass-06062)