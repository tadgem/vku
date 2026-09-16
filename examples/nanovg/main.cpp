#include "example-common.h"
#include "vku/Shader.h"
using namespace vku;

#define NUM_LIGHTS 16
using ForwardLightData = FrameLightDataT<NUM_LIGHTS>;
static ForwardLightData lightDataCpu {};

void RecordGraphicsCommandBuffers(VkState & vk, VkPipelineData& pipeline, Model& model, Material& mat)
{
    vku::commands::RecordGraphicsCommands(vk, [&](VkCommandBuffer& commandBuffer, uint32_t frameIndex) {

        render_passes::BeginSwapchainRenderPass(vk, commandBuffer);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_Pipeline);

        for (MeshEx& mesh : model.m_Meshes)
        {
            mesh.Bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_PipelineLayout, 0, 1, &mat.m_DescriptorSets.front().m_Sets[frameIndex], 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, mesh.m_IndexCount, 1, 0, 0, 0);
        }

        auto vg = vk.m_NanoVG;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, 0, 600, 400, 3.0f);
        nvgFillColor(vg, nvgRGBA(255, 30, 34, 255));
        nvgFill(vg);

        nvgEndFrame(vk.m_NanoVG);

        vkCmdEndRenderPass(commandBuffer);
    });
}

void UpdateUniformBuffer(VkState & vk, Material& mat)
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    time = 0.0f;
    MvpData ubo{};
    ubo.Model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.View = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    if (vk.m_SwapChainImageExtent.width > 0 || vk.m_SwapChainImageExtent.height)
    {
        ubo.Proj = glm::perspective(glm::radians(45.0f), vk.m_SwapChainImageExtent.width / (float)vk.m_SwapChainImageExtent.height, 0.1f, 300.0f);
        ubo.Proj[1][1] *= -1;
    }

    mat.SetBuffer(vk.m_CurrentFrameIndex, 0, 0, ubo);
    mat.SetBuffer(vk.m_CurrentFrameIndex, 0, 2, lightDataCpu);
}

void OnLightsImGui(VkState& vk)
{
    if (ImGui::Begin("Lights"))
    {
        ImGui::Text("FPS : %f, Frametime : %f", 1.0 / vk.m_DeltaTime, vk.m_DeltaTime);
        ImGui::DragFloat3("Directional Light Dir", &lightDataCpu.m_DirectionalLight.Direction[0]);
        ImGui::DragFloat4("Directional Light Colour", &lightDataCpu.m_DirectionalLight.Colour[0]);
        ImGui::DragFloat4("Directional Light Ambient Colour", &lightDataCpu.m_DirectionalLight.Ambient[0]);

        if(ImGui::TreeNode("Point Lights"))
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

int main()
{
    bool enableMSAA = true;
    VkState vk = init::Create<VkSDL>("NanoVG", 1920, 1080, enableMSAA);
    FillExampleLightData(lightDataCpu);

    ShaderProgram lights_prog= ShaderProgram::CreateGraphicsFromSourcePath(
        vk, "shaders/lights.vert", "shaders/lights.frag");

    Material m = Material::Create(vk, lights_prog);
    m.CreateBuffer(vk, 0, 0);
    m.CreateBuffer(vk, 0, 2);

    // Pipeline stage?
    auto vertexDescription = VertexDataPosNormalUv::GetVertexDescription(*vk.m_CPUAllocator);
    VkPipelineData pipeline = vku::pipelines::CreateRasterPipeline(vk,
        lights_prog,vertexDescription, defaults::CullNoneRasterStateMSAA,
        vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent);

    // create vertex and index buffer
    Model model(*vk.m_CPUAllocator);
    LoadModelAssimp(vk, model, "assets/viking_room.obj", true);

    if(!m.SetSampler(vk, "texSampler", model.m_Materials.front().m_Diffuse))
    {
        VKU_LOG_ERR("Failed to set diffuse texture for forward lighting shader");
    }

    while (vk.m_ShouldRun)
    {    
        vk.m_Backend->PreFrame(vk);
        OnLightsImGui(vk);
        UpdateUniformBuffer(vk, m);
        RecordGraphicsCommandBuffers(vk, pipeline, model, m);
        
        vk.m_Backend->PostFrame(vk);
    }

    FreeModel(vk, model);
    m.Free(vk);
    pipeline.Free(vk);

    return 0;
}
