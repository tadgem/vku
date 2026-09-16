#pragma once

#include "vku/Material.h"
#include "vku/Shader.h"
#pragma warning(push)
#pragma warning (disable: 4324)
#include "Im3D/im3d.h"
#pragma warning(pop)
#include "glm/glm.hpp"

namespace vku
{
    struct VkuIm3dState
    {
        ShaderProgram m_TriProg;
        ShaderProgram m_PointsProg;
        ShaderProgram m_LinesProg;
        // also ss quad mesh
        Buffer        m_ScreenQuadBuffer;
    };

    struct VkuIm3dViewState
    {
        Material m_TrisMaterial;
        Material m_PointsMaterial;
        Material m_LinesMaterial;

        VkPipelineData m_TrisPipeline;
        VkPipelineData m_PointsPipeline;
        VkPipelineData m_LinesPipeline;
    };

    VkuIm3dState LoadIm3D(VkState & vk);
    VkuIm3dViewState AddIm3dForViewport(VkState & vk, VkuIm3dState& state, VkRenderPass renderPass, bool enableMSAA, bool enableDynamicRendering = false);
    VkuIm3dViewState AddIm3dForGBuffer(VkState & vk, VkuIm3dState& state);
    VkuIm3dViewState AddIm3dForDeferredLightPass(VkState & vk, VkuIm3dState& state);
    VkuIm3dViewState AddIm3dForGBufferRenderPass(VkState & vk, VkuIm3dState& state, VkRenderPass gbufferRenderPass, uint32_t colourAttachmentCount, uint32_t colourWriteAttachment);
    VkuIm3dViewState AddIm3dForForwardPass(VkState & vk, VkuIm3dState& state, bool enableDynamicRendering);
    void FreeIm3dViewport(VkState & vk, VkuIm3dViewState& viewState);
    void FreeIm3d(VkState & vk, VkuIm3dState& state);
    void DrawIm3d(VkState & vk, VkCommandBuffer& buffer, uint32_t frameIndex, VkuIm3dState& state, VkuIm3dViewState& viewState, glm::mat4 _viewProj, uint32_t width, uint32_t height, bool drawText = false);
    void DrawIm3dTextListsImGui(uint32_t width, uint32_t height, glm::mat4 _viewProj);
    void DrawIm3dTextListsImGuiAsChild(glm::mat4 _viewProj);

}