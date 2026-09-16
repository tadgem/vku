#pragma once
#include "Im3D/im3d_vku.h"
#include "vku/Material.h"
#include "vku/Shader.h"

namespace vku {
namespace pipelines {

VkPipelineData
CreateComputePipeline(VkState &vk, StageBinary &comp,
                      VkDescriptorSetLayout &descriptorSetLayout);

VkPipelineData CreateDynamicRasterPipeline(VkState &vk, ShaderProgram &shader,
                                           VertexDescription &vertexDescription,
                                           RasterizationState &rasterState,
                                           VkExtent2D resolution,
                                           Vector<VkFormat> colourAttachments);

VkPipelineData CreateDynamicRasterPipeline(
    VkState &vk, ShaderProgram &shader, VertexDescription &vertexDescription,
    RasterizationState &rasterState, VkExtent2D resolution,
    Vector<VkFormat> colourAttachments,
    PipelineAttachmentState &attachmentState);

VkPipelineData CreateRasterPipeline(VkState &vk, ShaderProgram &shader,
                                    VertexDescription &vertexDescription,
                                    RasterizationState &rasterState,
                                    VkRenderPass &pipelineRenderPass,
                                    VkExtent2D resolution,
                                    uint32_t colorAttachmentCount = 1);

VkPipelineData CreateRasterPipeline(VkState &vk, ShaderProgram &shader,
                                    VertexDescription &vertexDescription,
                                    RasterizationState &rasterState,
                                    VkRenderPass &pipelineRenderPass,
                                    VkExtent2D resolution,
                                    PipelineAttachmentState &attachmentState);

PipelineAttachmentState
CreateGBufferAttachmentState(IAllocator &alloc, uint32_t colourAttachmentCount,
                             uint32_t writeAttachmentIndex);

class Pipeline {
public:
  Vector<Framebuffer *> m_FBs;
  Vector<Material *> m_PipelineMaterials;
  Vector<VkPipelineData *> m_PipelineDatas;

  Optional<Framebuffer *> m_OutputFramebuffer;
  VkuIm3dViewState *m_Im3dState = nullptr;

  Framebuffer *AddFramebuffer(VkState &vk) {
    m_FBs.push_back(new Framebuffer(*vk.m_CPUAllocator));
    return m_FBs[m_FBs.size() - 1];
  }

  void SetOutputFramebuffer(Framebuffer *fb) { m_OutputFramebuffer = fb; }

  Framebuffer *GetOutputFramebuffer() {
    if (m_OutputFramebuffer.has_value()) {
      return m_OutputFramebuffer.value();
    }
    return nullptr;
  }

  Material *AddMaterial(VkState &vk, ShaderProgram &prog) {
    m_PipelineMaterials.push_back(new Material(Material::Create(vk, prog)));
    return m_PipelineMaterials.back();
  }

  VkPipelineData *AddPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    m_PipelineDatas.emplace_back(new VkPipelineData(pipeline, layout));
    return m_PipelineDatas.back();
  }

  VkuIm3dViewState *AddIm3d(VkState &vk, VkuIm3dState im3dState) {
    m_Im3dState = new VkuIm3dViewState(AddIm3dForViewport(
        vk, im3dState,
        m_OutputFramebuffer.value()->m_RenderPassInfo.m_RenderPass, false));
    return m_Im3dState;
  }

  void Free(VkState &vk) {
    if (m_Im3dState != nullptr) {
      vku::FreeIm3dViewport(vk, *m_Im3dState);
    }
    for (auto *fb : m_FBs) {
      fb->Free(vk);
    }

    for (auto *material : m_PipelineMaterials) {
      material->Free(vk);
    }

    for (auto *pipeline_data : m_PipelineDatas) {
      pipeline_data->Free(vk);
    }
  }
};

template <typename _Ty> class PipelineT {
public:
  Vector<Framebuffer *> m_FBs;
  Vector<Material *> m_PipelineMaterials;
  Vector<VkPipelineData *> m_PipelineDatas;

  Optional<Framebuffer *> m_OutputFramebuffer;
  VkuIm3dViewState *m_Im3dState = nullptr;

  Optional<_Ty> m_CommandCallback;

  Framebuffer *AddFramebuffer(VkState &vk) {
    m_FBs.push_back(new Framebuffer(*vk.m_CPUAllocator));
    return m_FBs[m_FBs.size() - 1];
  }

  void SetOutputFramebuffer(Framebuffer *fb) { m_OutputFramebuffer = fb; }

  Framebuffer *GetOutputFramebuffer() {
    if (m_OutputFramebuffer.has_value()) {
      return m_OutputFramebuffer.value();
    }
    return nullptr;
  }

  Material *AddMaterial(VkState &vk, ShaderProgram &prog) {
    m_PipelineMaterials.push_back(new Material(Material::Create(vk, prog)));
    return m_PipelineMaterials.back();
  }

  VkPipelineData *AddPipeline(VkState &vk, VkPipeline pipeline,
                              VkPipelineLayout layout) {
    m_PipelineDatas.emplace_back(new VkPipelineData(pipeline, layout));
    return m_PipelineDatas.back();
  }

  VkuIm3dViewState *AddIm3d(VkState &vk, VkuIm3dState im3dState) {
    m_Im3dState = new VkuIm3dViewState(AddIm3dForViewport(
        vk, im3dState,
        m_OutputFramebuffer.value()->m_RenderPassInfo.m_RenderPass, false));
    return m_Im3dState;
  }

  void RecordCommands(_Ty callback) { m_CommandCallback = callback; }
};
} // namespace pipelines
}