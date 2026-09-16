#include "example-common.h"
#include "vku/Shader.h"
#include <random>

using namespace vku;

#define NUM_LIGHTS 16
using ForwardLightData = FrameLightDataT<NUM_LIGHTS>;
struct Particle
{
  glm::vec2 position;
  glm::vec2 velocity;
  glm::vec4 colour;

  static vku::Vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(IAllocator& alloc)
  {
    Vector<VkVertexInputAttributeDescription> attrs (alloc);
    attrs.resize(2);
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].offset = offsetof(Particle, position);
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].offset = offsetof(Particle, colour);
    attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;

    return attrs;
  }

  static VertexDescription GetVertexDescription(IAllocator& alloc)
  {
    VertexDescription desc (alloc);
    VkVertexInputBindingDescription binding {};
    binding.binding = 0;
    binding.stride = sizeof(Particle);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    desc.m_BindingDescriptions = {binding};
    desc.m_AttributeDescriptions = GetAttributeDescriptions(alloc);
    return desc;
  }


};

struct ParticlesUBOData
{
  float delta;
};

constexpr size_t PARTICLE_COUNT = 262144;
constexpr VkDeviceSize buffer_size = PARTICLE_COUNT * sizeof(Particle);

static ShaderBufferFrameData mvpUniformData;
static ShaderBufferFrameData lightsUniformData;
static ShaderBufferFrameData particleDeltaUniformData;

static ForwardLightData lightDataCpu {};
static std::vector<VkDescriptorSet> forwardPassDescriptorSets;
static std::vector<VkDescriptorSet> computePassDescriptorSets;

void CreateGraphicsDescriptorSets(VkState & vk, VkDescriptorSetLayout& descriptorSetLayout, VkImageView& textureImageView, VkSampler& textureSampler)
{
  std::vector<VkDescriptorSetLayout> forward_layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = vk.m_DescriptorSetAllocator.GetPool(*vk.m_CPUAllocator, vk.m_LogicalDevice);
  allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  allocInfo.pSetLayouts = forward_layouts.data();

  forwardPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  VK_CHECK(vkAllocateDescriptorSets(vk.m_LogicalDevice, &allocInfo,
                                    forwardPassDescriptorSets.data()));



  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo mvpBufferInfo{};
    mvpBufferInfo.buffer = mvpUniformData.m_UniformBuffers[i]->m_GpuBuffer;
    mvpBufferInfo.offset = 0;
    mvpBufferInfo.range = sizeof(MvpData);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView;
    imageInfo.sampler = textureSampler;

    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = lightsUniformData.m_UniformBuffers[i]->m_GpuBuffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = sizeof(ForwardLightData);

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = forwardPassDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &mvpBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = forwardPassDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = forwardPassDescriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &lightBufferInfo;


    vkUpdateDescriptorSets(vk.m_LogicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }

}


void RecordComputeCommandBuffers(VkState& vk, VkPipeline& p, VkPipelineLayout& layout, Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>& computeDescriptors)
{
  vku::commands::RecordComputeCommands(vk, [&](VkCommandBuffer& cmd, uint32_t frameIndex)
  {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &computeDescriptors[frameIndex], 0,0);
      vkCmdDispatch(cmd, PARTICLE_COUNT / 256, 1, 1);
  });
}

void RecordGraphicsCommandBuffers(VkState & vk,
                                  VkPipelineData& particlePipeline,
                                  std::vector<Buffer>& particleBuffers)
{
    vku::commands::RecordGraphicsCommands(vk, [&](VkCommandBuffer& commandBuffer, uint32_t frameIndex) {
        // push to example
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = vk.m_SwapchainImageRenderPass;
        renderPassInfo.framebuffer = vk.m_SwapChainFramebuffers[frameIndex];
        renderPassInfo.renderArea.offset = { 0,0 };
        renderPassInfo.renderArea.extent = vk.m_SwapChainImageExtent;

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        VkDeviceSize sizes[] = { 0 };
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
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline.m_Pipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &particleBuffers[frameIndex].m_GpuBuffer, sizes);
        vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0,0);
        vkCmdEndRenderPass(commandBuffer);
    });
}


void UpdateUniformBuffer(VkState & vk)
{
    particleDeltaUniformData.Set(vk.m_CurrentFrameIndex,
                                 ParticlesUBOData{static_cast<float>(std::clamp(vk.m_DeltaTime, 0.0, 0.5))});

}

static void CreateComputeBuffers(VkState& vk, std::vector<Buffer>& uniformBuffers,
                                 std::vector<Buffer>& shaderStorageBuffers)
{
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    shaderStorageBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    // Initial particle positions on a circle
    std::vector<Particle> particles(PARTICLE_COUNT);
    for (auto& particle : particles) {
        float r = 0.25f * sqrt(rndDist(rndEngine));
        float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
        float x = r * cos(theta) * vk.m_SwapChainImageExtent.height / vk.m_SwapChainImageExtent.width;
        float y = r * sin(theta);
        particle.position = glm::vec2(x, y);
        particle.velocity = glm::normalize(glm::vec2(x,y)) * 0.25f;
        particle.colour = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
    }

    MappedBuffer stagingBuffer = buffers::CreateMappedBuffer(vk, buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);


    memcpy(stagingBuffer.m_MappedAddr, particles.data(), buffer_size);

    for(auto f = 0; f < MAX_FRAMES_IN_FLIGHT; f++)
    {
        uniformBuffers[f] = buffers::CreateBuffer(vk, sizeof(ParticlesUBOData),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        shaderStorageBuffers[f] = buffers::CreateBuffer(vk, buffer_size,
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        buffers::CopyBuffer(vk, stagingBuffer.m_GpuBuffer, shaderStorageBuffers[f].m_GpuBuffer, buffer_size);


    }
}

VkDescriptorSetLayout CreateComputeDescriptorLayouts(VkState& vk)
{
    VkDescriptorSetLayout layout {};
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = 2;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT ;
    bindings[2].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo create {};
    create.bindingCount = 3;
    create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(vk.m_LogicalDevice, &create, nullptr, &layout));
    return layout;
}

Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> CreateComputeDescriptorSets(VkState& vk, VkDescriptorSetLayout layout,
                                 std::vector<Buffer>& uniformBuffers,
                                 std::vector<Buffer>& shaderStorageBuffers)
{
    Array<VkDescriptorSetLayout, 2> layouts{ layout, layout };
    

    VkDescriptorSetAllocateInfo alloc {};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = vk.m_DescriptorSetAllocator.GetPool(*vk.m_CPUAllocator, vk.m_LogicalDevice);
    alloc.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    alloc.pSetLayouts= layouts.data();
    

    Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets ;
    

    VK_CHECK(vkAllocateDescriptorSets(vk.m_LogicalDevice, &alloc, sets.data()));

    for(uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; f++)
    {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = particleDeltaUniformData.m_UniformBuffers[f]->m_GpuBuffer;
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(ParticlesUBOData);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = sets[f];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

        auto last_index = (f - 1) % MAX_FRAMES_IN_FLIGHT;
        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = shaderStorageBuffers[last_index].m_GpuBuffer;
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = sets[f];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[f].m_GpuBuffer;
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = sets[f];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

        vkUpdateDescriptorSets(vk.m_LogicalDevice, 3, descriptorWrites.data(), 0, nullptr);
    }

    return sets;

}

void OnImGui(VkState& vk)
{
    if (ImGui::Begin("Lights")) {
        ImGui::Text("FPS : %f", 1.0 / vk.m_DeltaTime);
    }
    ImGui::End();
}

VkPipeline CreateComputePipeline(VkState& vk, VkDescriptorSetLayout& layout, ShaderProgram& computeProg, VkPipelineLayout& computeLayout)
{
    VkPipelineLayoutCreateInfo computeLayoutInfo {};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &layout;

    VK_CHECK(vkCreatePipelineLayout(vk.m_LogicalDevice, &computeLayoutInfo, nullptr, & computeLayout));

    VkPipelineShaderStageCreateInfo computeStageInfo {};
    computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStageInfo.module = computeProg.m_Stages.front().m_Module;
    computeStageInfo.pName = "main";


    VkComputePipelineCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create.layout = computeLayout;
    create.stage = computeStageInfo;

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(vk.m_LogicalDevice, VK_NULL_HANDLE, 1, &create, nullptr, &pipeline));

    return pipeline;
}

int main()
{
    bool enableMSAA = false;
    VkState vk = init::Create<VkSDL>("Forward Lights", 1920, 1080, enableMSAA);

    // shader abstraction
    ShaderProgram particles_prog = ShaderProgram::CreateComputeFromSourcePath(vk, "shaders/particles.comp");
    ShaderProgram draw_particles = ShaderProgram::CreateGraphicsFromSourcePath(
        vk, "shaders/draw_particle.vert", "shaders/draw_particle.frag");
    Material computeMaterial = Material::Create(vk, particles_prog);

    std::vector<Buffer> uniformBuffers;
    std::vector<Buffer> shaderStorageBuffers;
    CreateComputeBuffers(vk, uniformBuffers, shaderStorageBuffers);


    particleDeltaUniformData = buffers::CreateUniformBuffersT<ParticlesUBOData>(vk);

    VkDescriptorSetLayout layout = CreateComputeDescriptorLayouts(vk);
    Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> computeDescriptors = CreateComputeDescriptorSets(vk, layout,  uniformBuffers, shaderStorageBuffers);
    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline = CreateComputePipeline(vk, layout, particles_prog, computePipelineLayout);


    RasterizationState rasterState = {VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false, VK_COMPARE_OP_ALWAYS,VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

    auto particleVertexDescription = Particle::GetVertexDescription(*vk.m_CPUAllocator);
    VkPipelineData particlePipeline = vku::pipelines::CreateRasterPipeline(
        vk, draw_particles, particleVertexDescription, rasterState,
        vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent);


    while (vk.m_ShouldRun)
    {    
        vk.m_Backend->PreFrame(vk);
        
        UpdateUniformBuffer(vk);

        OnImGui(vk);

        RecordComputeCommandBuffers(vk, computePipeline, computePipelineLayout, computeDescriptors);
        RecordGraphicsCommandBuffers(vk,
          particlePipeline, shaderStorageBuffers);

        vk.m_Backend->PostFrame(vk);
    }

    particleDeltaUniformData.Free(vk);

    return 0;
}
