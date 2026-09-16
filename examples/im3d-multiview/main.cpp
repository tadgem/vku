#include "example-common.h"
#include "vku/Shader.h"
#include "vku/Material.h"
#include <algorithm>
#include "Im3D/im3d_vku.h"
#include "ImGui/vku_extensions.h"

using namespace vku;

struct ViewData
{
    // most of this can be encapsulated in a view pipeline
    Framebuffer m_GBuffer;
    Framebuffer m_LightPassFB;
    Material    m_LightPassMaterial;

    VkPipelineData    m_GBufferPipeline, m_LightPassPipeline;
    VkuIm3dViewState  m_Im3dState;
    VkuIm3dViewState  m_DeferredIm3dState;
    VkRenderPass      m_Im3dRenderPass = VK_NULL_HANDLE;
    Vector<VkFramebuffer> m_Im3dFramebuffers;
    VkExtent2D        m_CurrentResolution{ 1920, 1080 };

    Camera      m_Camera;
    Mesh        m_ViewQuad;
};

static Transform g_Transform;

ViewData CreateView(VkState & vk, VkuIm3dState im3dState, ShaderProgram gbufferProg, ShaderProgram lightPassProg)
{
    Framebuffer gbuffer(*vk.m_CPUAllocator);
    gbuffer.AddColourAttachment(vk,
        ResolutionScale::Full, 1,  VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT);
    gbuffer.AddColourAttachment(vk, ResolutionScale::Full, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT);
    gbuffer.AddColourAttachment(vk, ResolutionScale::Full, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT);
    gbuffer.AddDepthAttachment(vk, ResolutionScale::Full, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT);
    gbuffer.Build(vk);

    Framebuffer finalImage(*vk.m_CPUAllocator);
    finalImage.AddColourAttachment(vk, ResolutionScale::Full, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT);
    finalImage.Build(vk);

    Material lightPassMat = Material::Create(vk, lightPassProg);
    lightPassMat.CreateBuffer(vk, 0, 3);

    lightPassMat.SetColourAttachment(vk, String("positionBufferSampler", *vk.m_CPUAllocator), gbuffer, 1);
    lightPassMat.SetColourAttachment(vk, String("normalBufferSampler", *vk.m_CPUAllocator), gbuffer, 2);
    lightPassMat.SetColourAttachment(vk, String("colourBufferSampler", *vk.m_CPUAllocator), gbuffer, 0);

    // create gbuffer pipeline
    auto vertexDescription = VertexDataPosNormalUv::GetVertexDescription(*vk.m_CPUAllocator);
    VkPipelineData gbufferPipeline = vku::pipelines::CreateRasterPipeline(vk,
        gbufferProg,vertexDescription, defaults::DefaultRasterState,
        gbuffer.m_RenderPassInfo.m_RenderPass, vk.m_SwapChainImageExtent, 3);

    // create present graphics pipeline
    // Pipeline stage?
    auto presentVertexDescription = VertexDataPosUv::GetVertexDescription(*vk.m_CPUAllocator);
    VkPipelineData pipeline = vku::pipelines::CreateRasterPipeline(vk,
        lightPassProg, presentVertexDescription, defaults::CullNoneRasterState,
        finalImage.m_RenderPassInfo.m_RenderPass, vk.m_SwapChainImageExtent);

    auto im3dViewState = AddIm3dForViewport(vk, im3dState, finalImage.m_RenderPassInfo.m_RenderPass, false);

    Vector<VkAttachmentDescription> im3dColourAttachments(*vk.m_CPUAllocator);
    VkAttachmentDescription im3dColourDesc{};
    im3dColourDesc.format = finalImage.m_ColourAttachments[0].m_Format;
    im3dColourDesc.samples = finalImage.m_ColourAttachments[0].m_SampleCount;
    im3dColourDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    im3dColourDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    im3dColourDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    im3dColourDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    im3dColourDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    im3dColourDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    im3dColourAttachments.push_back(im3dColourDesc);

    VkAttachmentDescription im3dDepthDesc{};
    im3dDepthDesc.format = gbuffer.m_DepthAttachments[0].m_Format;
    im3dDepthDesc.samples = gbuffer.m_DepthAttachments[0].m_SampleCount;
    im3dDepthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    im3dDepthDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    im3dDepthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    im3dDepthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    im3dDepthDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    im3dDepthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkRenderPass im3dRenderPass = VK_NULL_HANDLE;
    render_passes::CreateRenderPass(vk, im3dRenderPass, im3dColourAttachments, Vector<VkAttachmentDescription>(*vk.m_CPUAllocator), true, im3dDepthDesc, VK_ATTACHMENT_LOAD_OP_LOAD);

    Vector<VkFramebuffer> im3dFramebuffers(*vk.m_CPUAllocator);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        Vector<VkImageView> fbAttachments(*vk.m_CPUAllocator);
        fbAttachments.push_back(finalImage.m_ColourAttachments[0].m_AttachmentSwapchainImages[i].m_ImageView);
        fbAttachments.push_back(gbuffer.m_DepthAttachments[0].m_AttachmentSwapchainImages[i].m_ImageView);
        VkFramebuffer fb;
        textures::CreateFramebuffer(vk, fbAttachments, im3dRenderPass, vk.m_MaxFramebufferExtent, fb);
        im3dFramebuffers.push_back(fb);
    }

    auto deferredIm3dViewState = AddIm3dForDeferredLightPass(vk, im3dState);

    static StaticVector<VertexDataPosUv> screenQuadVerts = {
                    { { -1.0f, -1.0f , 0.0f}, { 0.0f, 0.0f } },
                    { {1.0f, -1.0f, 0.0f}, {1.0, 0.0f} },
                    { {1.0f, 1.0f, 0.0f}, {1.0, 1.0} },
                    { {-1.0f, 1.0f, 0.0f}, {0.0f, 1.0} }
    };

    static StaticVector<uint32_t> screenQuadIndices = {
    0, 1, 2, 2, 3, 0
    };

    Buffer vertexBuffer = buffers::CreateVertexBuffer<VertexDataPosUv>(vk, screenQuadVerts.data(), screenQuadVerts.size());
    Buffer indexBuffer = buffers::CreateIndexBuffer(vk, screenQuadIndices.data(), screenQuadIndices.size());

    Mesh screenQuad{ vertexBuffer, indexBuffer, 6 };

    return { gbuffer, finalImage, lightPassMat, gbufferPipeline, pipeline,  im3dViewState, deferredIm3dViewState, im3dRenderPass, im3dFramebuffers, {1920, 1080}, {},  screenQuad };
}

void FreeView(VkState & vk, ViewData& view)
{
    FreeIm3dViewport(vk, view.m_Im3dState);
    FreeIm3dViewport(vk, view.m_DeferredIm3dState);
    for (auto& fb : view.m_Im3dFramebuffers)
    {
        vkDestroyFramebuffer(vk.m_LogicalDevice, fb, nullptr);
    }
    if (view.m_Im3dRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(vk.m_LogicalDevice, view.m_Im3dRenderPass, nullptr);
    }
}

void UpdateRenderItemUniformBuffer(VkState & vk, Material& renderItemMaterial)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    MvpData2 ubo{};
    ubo.Model = g_Transform.to_mat4();

    renderItemMaterial.SetBuffer(vk.m_CurrentFrameIndex, 0, 0, ubo);
}

void UpdateViewData(VkState & vk, ViewData* view, DeferredLightData& lightData)
{
    glm::quat qPitch = glm::angleAxis(glm::radians(-view->m_Camera.Rotation.x), glm::vec3(1, 0, 0));
    glm::quat qYaw = glm::angleAxis(glm::radians(view->m_Camera.Rotation.y), glm::vec3(0, 1, 0));
    // omit roll
    glm::quat Rotation = qPitch * qYaw;
    Rotation = glm::normalize(Rotation);
    glm::mat4 rotate = glm::mat4_cast(Rotation);
    glm::mat4 translate = glm::mat4(1.0f);
    translate = glm::translate(translate, -view->m_Camera.Position);

    view->m_Camera.View = rotate * translate;
    if (vk.m_SwapChainImageExtent.width > 0 || vk.m_SwapChainImageExtent.height)
    {
        view->m_Camera.Proj = glm::perspective(glm::radians(45.0f),(float) view->m_CurrentResolution.width / (float)view->m_CurrentResolution.height, 0.1f, 10000.0f);
        view->m_Camera.Proj[1][1] *= -1;
    }

    view->m_LightPassMaterial.SetBuffer(vk.m_CurrentFrameIndex, 0, 3, lightData);
}

void RecordCommandBuffersV2(VkState & vk, Vector<ViewData*> views, RenderModel& model, Mesh& screenQuad, VkuIm3dState& im3dState, DeferredLightData& lightData)
{
    static StaticVector<VertexDataPosUv> originalScreenQuadData = {
                    { { -1.0f, -1.0f , 0.0f}, { 0.0f, 0.0f } },
                    { {1.0f, -1.0f, 0.0f}, {1.0, 0.0f} },
                    { {1.0f, 1.0f, 0.0f}, {1.0, 1.0} },
                    { {-1.0f, 1.0f, 0.0f}, {0.0f, 1.0} }
    };
    vku::commands::RecordGraphicsCommands(vk, [&](VkCommandBuffer& commandBuffer, uint32_t frameIndex) {
        {
            Array<VkClearValue, 4> clearValues{};
            clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[3].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = vk.m_SwapchainImageRenderPass;
            renderPassInfo.framebuffer = vk.m_SwapChainFramebuffers[frameIndex];
            renderPassInfo.renderArea.offset = { 0,0 };
            renderPassInfo.renderArea.extent = vk.m_SwapChainImageExtent;

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(vk.m_SwapChainImageExtent.width);
            viewport.height = static_cast<float>(vk.m_SwapChainImageExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = { 0,0 };
            scissor.extent = VkExtent2D{
                static_cast<uint32_t>(vk.m_SwapChainImageExtent.width) ,
                static_cast<uint32_t>(vk.m_SwapChainImageExtent.height)
            };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdEndRenderPass(commandBuffer);
        }

        for (auto& view : views)
        {
            struct PCViewData
            {
                glm::mat4 view;
                glm::mat4 proj;
            };


            PCViewData pcData{ view->m_Camera.View, view->m_Camera.Proj };
            VkExtent2D viewExtent = view->m_CurrentResolution;

            // update screen quad
            {
                auto max = vk.m_MaxFramebufferExtent;
                float w = static_cast<float>((float) viewExtent.width / (float) max.width);
                float h = static_cast<float>((float) viewExtent.height / (float) max.height);

                Vector<VertexDataPosUv> newScreenQuadData(*vk.m_CPUAllocator); 
                newScreenQuadData.push_back({ { -1.0f, -1.0f , 0.0f}, { 0.0f, 0.0f } });
                newScreenQuadData.push_back({ {1.0f, -1.0f, 0.0f}, {w, 0.0f} });
                newScreenQuadData.push_back({ {1.0f, 1.0f, 0.0f}, {w, h} });
                newScreenQuadData.push_back({ {-1.0f, 1.0f, 0.0f}, {0.0f, h} });
                

                vkCmdUpdateBuffer(commandBuffer, view->m_ViewQuad.m_VertexBuffer.m_GpuBuffer, 0, 4 * sizeof(VertexDataPosUv), &newScreenQuadData[0]);
            }

            {
                Array<VkClearValue, 4> clearValues{};
                clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
                clearValues[1].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
                clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
                clearValues[3].depthStencil = { 1.0f, 0 };

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = view->m_GBuffer.m_RenderPassInfo.m_RenderPass;
                renderPassInfo.framebuffer = view->m_GBuffer.m_RenderPassInfo.m_SwapchainFramebuffers[frameIndex];
                renderPassInfo.renderArea.offset = { 0,0 };
                renderPassInfo.renderArea.extent = viewExtent;

                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();

                vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, view->m_GBufferPipeline.m_Pipeline);
                VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
                viewport.width = static_cast<float>(viewExtent.width);
                viewport.height = static_cast<float>(viewExtent.height);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                VkRect2D scissor{};
                scissor.offset = { 0,0 };
                scissor.extent = VkExtent2D{
                    static_cast<uint32_t>(viewExtent.width) ,
                    static_cast<uint32_t>(viewExtent.height)
                };

                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
                vkCmdPushConstants(commandBuffer, view->m_GBufferPipeline.m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PCViewData), &pcData);

                for (int i = 0; i < model.m_RenderItems.size(); i++)
                {
                    MeshEx& mesh = model.m_RenderItems[i].m_Mesh;

                    VkBuffer vertexBuffers[]{ mesh.m_Mesh.m_VertexBuffer.m_GpuBuffer };
                    VkDeviceSize sizes[] = { 0 };

                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, sizes);
                    vkCmdBindIndexBuffer(commandBuffer, mesh.m_Mesh.m_IndexBuffer.m_GpuBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, view->m_GBufferPipeline.m_PipelineLayout, 0, 1, &model.m_RenderItems[i].m_Material.m_DescriptorSets[0].m_Sets[frameIndex], 0, nullptr);
                    vkCmdDrawIndexed(commandBuffer, mesh.m_IndexCount, 1, 0, 0, 0);
                }
                vkCmdEndRenderPass(commandBuffer);
            }

            Array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = view->m_LightPassFB.m_RenderPassInfo.m_RenderPass;
            renderPassInfo.framebuffer = view->m_LightPassFB.m_RenderPassInfo.m_SwapchainFramebuffers[frameIndex];
            renderPassInfo.renderArea.offset = { 0,0 };
            renderPassInfo.renderArea.extent = viewExtent;

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, view->m_LightPassPipeline.m_Pipeline);
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(viewExtent.width);
            viewport.height = static_cast<float>(viewExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = { 0,0 };
            scissor.extent = VkExtent2D{
                static_cast<uint32_t>(viewExtent.width) ,
                static_cast<uint32_t>(viewExtent.height)
            };

            // issue with lighting pass is that uvs are just 0,0 -> 1,1
            // meaning the entire buffer will be resampled
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdPushConstants(commandBuffer, view->m_LightPassPipeline.m_PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PCViewData), &pcData);
            VkDeviceSize sizes[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &view->m_ViewQuad.m_VertexBuffer.m_GpuBuffer, sizes);
            vkCmdBindIndexBuffer(commandBuffer, view->m_ViewQuad.m_IndexBuffer.m_GpuBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, view->m_LightPassPipeline.m_PipelineLayout, 0, 1, &view->m_LightPassMaterial.m_DescriptorSets[0].m_Sets[frameIndex], 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, view->m_ViewQuad.m_IndexCount, 1, 0, 0, 0);

            vkCmdEndRenderPass(commandBuffer);

            {
                VkRenderPassBeginInfo im3dRenderPassInfo{};
                im3dRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                im3dRenderPassInfo.renderPass = view->m_Im3dRenderPass;
                im3dRenderPassInfo.framebuffer = view->m_Im3dFramebuffers[frameIndex];
                im3dRenderPassInfo.renderArea.offset = { 0,0 };
                im3dRenderPassInfo.renderArea.extent = viewExtent;
                im3dRenderPassInfo.clearValueCount = 0;
                im3dRenderPassInfo.pClearValues = nullptr;

                vkCmdBeginRenderPass(commandBuffer, &im3dRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, view->m_DeferredIm3dState.m_TrisPipeline.m_Pipeline);
                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(viewExtent.width);
                viewport.height = static_cast<float>(viewExtent.height);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                VkRect2D scissor{};
                scissor.offset = { 0,0 };
                scissor.extent = VkExtent2D{
                    static_cast<uint32_t>(viewExtent.width) ,
                    static_cast<uint32_t>(viewExtent.height)
                };

                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                DrawIm3d(vk, commandBuffer, frameIndex, im3dState, view->m_DeferredIm3dState, view->m_Camera.Proj * view->m_Camera.View, viewExtent.width, viewExtent.height);
                vkCmdEndRenderPass(commandBuffer);
            }
        }
        }
    );
}

RenderModel CreateRenderModelGbuffer(VkState & vk, const char* modelPath, ShaderProgram& shader)
{
    Model model(*vk.m_CPUAllocator);
    LoadModelAssimp(vk, model, modelPath, true);

    RenderModel renderModel(*vk.m_CPUAllocator);
    renderModel.m_Original = model;
    for (auto& mesh : model.m_Meshes)
    {
        RenderItem item(*vk.m_CPUAllocator);
        int materialIndex = std::min(mesh.m_MaterialIndex, (uint32_t)model.m_Materials.size() - 1);
        item.m_Mesh = mesh;
        item.m_Material = Material::Create(vk, shader);
        item.m_Material.CreateBuffer(vk, 0, 0);
        MaterialEx& material = model.m_Materials[mesh.m_MaterialIndex];
        item.m_Material.SetSampler(vk, "texSampler", material.m_Diffuse.m_ImageView, material.m_Diffuse.m_Sampler);
        renderModel.m_RenderItems.push_back(item);
    }

    return renderModel;
}

void OnImGui(VkState & vk, DeferredLightData& lightDataCpu, Vector<ViewData*> views)
{
    if (ImGui::Begin("View 1"))
    {
        // size needs to be the current resolution
        // uv0 will likely always be 0,0
        // uv1 needs to be MaxResolution / CurrentResolution;
        auto extent = ImGui::GetContentRegionAvail();
        auto max = vk.m_MaxFramebufferExtent;
        ImVec2 uv1 = { extent.x / max.width, extent.y / max.height };
        auto& image = views[0]->m_LightPassFB.m_ColourAttachments[0].m_AttachmentSwapchainImages[vk.m_CurrentFrameIndex];

        ImGuiX::Image(image, extent, { 0,0 }, uv1);
        DrawIm3dTextListsImGuiAsChild(views[0]->m_Camera.Proj * views[0]->m_Camera.View);
        views[0]->m_CurrentResolution = { (uint32_t)extent.x, (uint32_t)extent.y };

    }
    ImGui::End();

    if (ImGui::Begin("View 2"))
    {
        // size needs to be the current resolution
        // uv0 will likely always be 0,0
        // uv1 needs to be MaxResolution / CurrentResolution;
        auto extent = ImGui::GetContentRegionAvail();
        auto max = vk.m_MaxFramebufferExtent;
        ImVec2 uv1 = { extent.x / max.width, extent.y / max.height };
        auto& image = views[1]->m_LightPassFB.m_ColourAttachments[0].m_AttachmentSwapchainImages[vk.m_CurrentFrameIndex];

        ImGuiX::Image(image, extent, { 0,0 }, uv1);
        DrawIm3dTextListsImGuiAsChild(views[1]->m_Camera.Proj * views[1]->m_Camera.View);
        views[1]->m_CurrentResolution = { (uint32_t)extent.x, (uint32_t)extent.y };

    }
    ImGui::End();

    if (ImGui::Begin("ECS Debug"))
    {
        ImGui::Text("Frametime: %f", (1.0 / vk.m_DeltaTime));
        ImGui::Separator();
        ImGui::DragFloat3("Position", &g_Transform.m_Position[0]);
        ImGui::DragFloat3("Euler Rotation", &g_Transform.m_Rotation[0]);
        ImGui::DragFloat3("Scale", &g_Transform.m_Scale[0]);

        ImGui::Separator();

        ImGui::DragFloat3("Cam 1 Position", &views[0]->m_Camera.Position[0]);
        ImGui::DragFloat3("Cam 1 Euler Rotation", &views[0]->m_Camera.Rotation[0]);

        ImGui::Separator();

        ImGui::DragFloat3("Cam 2 Position", &views[1]->m_Camera.Position[0]);
        ImGui::DragFloat3("Cam 2 Euler Rotation", &views[1]->m_Camera.Rotation[0]);
    }
    ImGui::End();

    if (ImGui::Begin("Lights Menu"))
    {
        ImGui::Text("Frametime: %f", (1.0 / vk.m_DeltaTime));
        ImGui::DragFloat3("Directional Light Dir", &lightDataCpu.m_DirectionalLight.Direction[0]);
        ImGui::DragFloat4("Directional Light Colour", &lightDataCpu.m_DirectionalLight.Colour[0]);
        ImGui::DragFloat4("Directional Light Ambient Colour", &lightDataCpu.m_DirectionalLight.Ambient[0]);

        if (ImGui::TreeNode("Point Lights"))
        {
            for (int i = 0; i < NUM_LIGHTS; i++)
            {
                ImGui::PushID(i);
                if (ImGui::TreeNode("Point Light"))
                {
                    ImGui::DragFloat3("Position", &lightDataCpu.m_PointLights[i].PositionRadius[0]);
                    ImGui::DragFloat("Radius", &lightDataCpu.m_PointLights[i].PositionRadius[3]);
                    ImGui::DragFloat4("Colour", &lightDataCpu.m_PointLights[i].Colour[0]);
                    ImGui::DragFloat4("Ambient Colour", &lightDataCpu.m_PointLights[i].Ambient[0]);

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spot Lights"))
        {
            for (int i = 0; i < NUM_LIGHTS; i++)
            {
                ImGui::PushID(NUM_LIGHTS + i);
                if (ImGui::TreeNode("Spot Light"))
                {
                    ImGui::DragFloat3("Position", &lightDataCpu.m_SpotLights[i].PositionRadius[0]);
                    ImGui::DragFloat("Radius", &lightDataCpu.m_SpotLights[i].PositionRadius[3]);
                    ImGui::DragFloat3("Direction", &lightDataCpu.m_SpotLights[i].DirectionAngle[0]);
                    ImGui::DragFloat("Angle", &lightDataCpu.m_SpotLights[i].DirectionAngle[3]);
                    ImGui::DragFloat4("Colour", &lightDataCpu.m_SpotLights[i].Colour[0]);
                    ImGui::DragFloat4("Ambient Colour", &lightDataCpu.m_SpotLights[i].Ambient[0]);

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void OnIm3D()
{
    Im3d::PushAlpha(1.0f);
    Im3d::PushColor(Im3d::Color_Red);
    Im3d::SetAlpha(1.0f);
    Im3d::DrawCircle({ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 20.5f, 32);
    Im3d::DrawCone({ 40.0f, 0.0f, 0.0f }, { 0.0, 1.0, 0.0 }, 30.0f, 30.0f, 32);
    Im3d::DrawPrism({ 80.0f, 0.0f, 0.0f }, { 80.0f, 80.0f, 0.0f }, 10.0f, 32);
    Im3d::PopColor();
    Im3d::PushColor(Im3d::Color_Green);
    Im3d::DrawArrow({ 120.0f, 0.0f, 0.0f }, { 3.0f, 23.0f, 0.0f }, 2.0f, 2.0f);
    Im3d::DrawXyzAxes();
    Im3d::DrawCylinder({ 160.0f, 0.0f, 0.0f }, { 160.0f,20.0f, 0.0f }, 20.0f, 32);
    Im3d::PopColor();
    Im3d::PushColor(Im3d::Color_White);
    Im3d::Text({ 0.0, 20.0f, 0.0f }, 0, "Hello from you fuck you bloody");
    Im3d::PopColor();
    Im3d::PopAlpha();

}

int main() {
    constexpr bool enableMSAA = false;
    VkState vk = init::Create<VkSDL>("Im3D Multiview", 1920, 1080, enableMSAA);
    auto im3dState = LoadIm3D(vk);

    DeferredLightData lightDataCpu{};
    FillExampleLightData(lightDataCpu);

    ShaderProgram gbufferProg = ShaderProgram::CreateGraphicsFromSourcePath(
        vk, "shaders/gbuffer.vert", "shaders/gbuffer.frag");
    ShaderProgram lightPassProg = ShaderProgram::CreateGraphicsFromSourcePath(
        vk, "shaders/lights.vert", "shaders/lights.frag");

    ViewData viewA = CreateView(vk, im3dState, gbufferProg, lightPassProg);
    viewA.m_Camera.Position = { -40.0, 10.0f, 30.0f };
    ViewData viewB = CreateView(vk, im3dState, gbufferProg, lightPassProg);
    viewB.m_Camera.Position = { 30.0, 0.0f, -20.0f };

    Vector<ViewData*> views(*vk.m_CPUAllocator);
    views.push_back(&viewA);
    views.push_back(&viewB);

    // create vertex and index buffer
    // allocate materials instead of raw buffers etc.
    RenderModel m = CreateRenderModelGbuffer(vk, "assets/sponza/sponza.gltf", gbufferProg);

    while (vk.m_Backend->ShouldRun(vk))
    {
        vk.m_Backend->PreFrame(vk);

        Im3d::NewFrame();

        for (int i = 0; i < m.m_RenderItems.size(); i++)
        {
            UpdateRenderItemUniformBuffer(vk, m.m_RenderItems[i].m_Material);
        }

        for (int i = 0; i < views.size(); i++)
        {
            UpdateViewData(vk, views[i], lightDataCpu);
        }


        OnIm3D();

        Im3d::EndFrame();

        RecordCommandBuffersV2(vk, views, m, *Mesh::g_ScreenSpaceQuad, im3dState, lightDataCpu);

        OnImGui(vk, lightDataCpu, views);

        vk.m_Backend->PostFrame(vk);
    }
    gbufferProg.Free(vk);
    lightPassProg.Free(vk);

    FreeView(vk, viewA);
    FreeView(vk, viewB);

    m.Free(vk);

    FreeIm3d(vk, im3dState);

    return 0;
}
