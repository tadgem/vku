#include "example-common.h"
#include "vku/Shader.h"
using namespace vku;


void RecordGraphicsCommandBuffers(VkState & vk, VkPipelineData& pipeline,  Model& model, Vector<VkDescriptorSet>& descriptorSets)
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

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_Pipeline);

        for (int i = 0; i < model.m_Meshes.size(); i++)
        {
            MeshEx& mesh = model.m_Meshes[i];
            VkBuffer vertexBuffers[]{ mesh.m_Mesh.m_VertexBuffer.m_GpuBuffer};
            VkDeviceSize sizes[] = { 0 };

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
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, sizes);
            vkCmdBindIndexBuffer(commandBuffer, mesh.m_Mesh.m_IndexBuffer.m_GpuBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.m_PipelineLayout, 0, 1, &descriptorSets[frameIndex], 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, mesh.m_IndexCount, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(commandBuffer);
    });
}

void UpdateUniformBuffer(VkState & vk, Vector<MappedBuffer>& uniformBuffers)
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

    memcpy(uniformBuffers[vk.m_CurrentFrameIndex].m_MappedAddr, &ubo, sizeof(ubo));
}

void CreateGraphicsDescriptorSets(VkState & vk, VkDescriptorSetLayout& descriptorSetLayout, VkImageView& textureImageView, VkSampler& textureSampler, 
    Vector<VkDescriptorSet>& descriptorSets, Vector<MappedBuffer>& uniformBuffers)
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vk.m_DescriptorSetAllocator.GetPool(*vk.m_CPUAllocator, vk.m_LogicalDevice);
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(vk.m_LogicalDevice, &allocInfo, descriptorSets.data()));

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i].m_GpuBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(MvpData);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vk.m_LogicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

}

int main()
{
    bool enableMSAA = true;
    VkState vk = init::Create<VkSDL>("MSAA Example", 1920, 1080, enableMSAA);


    Vector<MappedBuffer>           uniformBuffers(*vk.m_CPUAllocator);
    Vector<VkDescriptorSet>        descriptorSets(*vk.m_CPUAllocator);

    ShaderProgram prog = ShaderProgram::CreateGraphicsFromSourcePath(
        vk, "shaders/texture.vert","shaders/texture.frag");

    uint32_t mipLevels;
    VkImage textureImage;
    VkImageView imageView;
    VkDeviceMemory textureMemory;
    VkSampler imageSampler;
    textures::CreateTexture(vk, "assets/viking_room.png", VK_FORMAT_R8G8B8A8_UNORM, textureImage, imageView, textureMemory, &mipLevels);
    textures::CreateImageSampler(vk, mipLevels, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, imageSampler);

    auto vertexDescription = VertexDataPosUv::GetVertexDescription(*vk.m_CPUAllocator);
    VkPipelineData pipeline = vku::pipelines::CreateRasterPipeline(vk,
        prog, vertexDescription, defaults::CullNoneRasterStateMSAA,
        vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent);

    // create vertex and index buffer
    Model model(*vk.m_CPUAllocator);
    LoadModelAssimp(vk, model, "assets/viking_room.obj");

    uniformBuffers = buffers::CreateUniformBuffers<MvpData>(vk);

    CreateGraphicsDescriptorSets(vk, prog.m_DescriptorSetLayout, imageView,
                                 imageSampler, descriptorSets, uniformBuffers);

    while (vk.m_ShouldRun)
    {    
        vk.m_Backend->PreFrame(vk);
        
        UpdateUniformBuffer(vk, uniformBuffers);

        if (ImGui::Begin("Help"))
        {

        }
        ImGui::End();
        RecordGraphicsCommandBuffers(vk, pipeline, model, descriptorSets);

        vk.m_Backend->PostFrame(vk);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        uniformBuffers[i].Free(vk);
    }
    FreeModel(vk, model);

    vkDestroySampler(vk.m_LogicalDevice, imageSampler, nullptr);
    vkDestroyImageView(vk.m_LogicalDevice, imageView, nullptr);
    vkDestroyImage(vk.m_LogicalDevice, textureImage, nullptr);
    vkFreeMemory(vk.m_LogicalDevice, textureMemory, nullptr);
    pipeline.Free(vk);

    return 0;
}
