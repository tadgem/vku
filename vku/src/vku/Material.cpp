#include "vku/Material.h"
#include "vku/Shader.h"
#include "vku/Texture.h"
#include "vku/Buffer.h"
#include "volk.h"

static auto reflect_descriptor_info = [](vku::ShaderStage& stage, vku::Material &mat)
    {
        using namespace vku;

        for (auto& pushConstant : stage.m_PushConstants)
        {
            mat.m_PushConstants.push_back(pushConstant);
        }

        for (auto& descriptorSetInfo : stage.m_LayoutDatas)
        {
            for (auto& bindingInfo : descriptorSetInfo.m_BindingDatas)
            {
                if (bindingInfo.m_ExpectedBufferSizeOrDivisor == 0 && bindingInfo.m_BufferType == ShaderBindingType::Sampler)
                {
                    Material::SamplerBindingData sbd{
                        descriptorSetInfo.m_SetNumber,
                        bindingInfo.m_BindingIndex,
                        Texture::g_DefaultTexture->m_ImageView,
                        Texture::g_DefaultTexture->m_Sampler
                    };
                    mat.m_Samplers.emplace(bindingInfo.m_BindingName, sbd);
                    continue;
                }

                if (bindingInfo.m_BufferType == ShaderBindingType::UniformBuffer ||
                    bindingInfo.m_BufferType == ShaderBindingType::ShaderStorageBuffer)
                {
                    // if a uniform buffer
                    //ShaderBufferFrameData uniform = buffers::CreateUniformBuffers(vk, VkDeviceSize{ bindingInfo.m_ExpectedBufferSizeOrDivisor});
                    // build accessors
                    for (auto& member : bindingInfo.m_Members)
                    {
                        String accessorName = bindingInfo.m_BindingName + "." + member.m_Name;
                        auto arraySize = member.m_Stride > 0 ? (member.m_Size / member.m_Stride) : 0;
                        Material::ShaderAccessorData data{ 
                            member.m_Size , 
                            member.m_Offset, 
                            member.m_Stride, 
                            static_cast<uint16_t> (arraySize),
                            static_cast<uint32_t>(mat.m_ShaderBuffers.size()), 
                            Buffer::BufferType::Uniform 
                        };
                        mat.m_UniformBufferAccessors.emplace(accessorName, data);
                    }

                    DescriptorSetBinding binding (
                        descriptorSetInfo.m_SetNumber, 
                        bindingInfo.m_BindingIndex, 
                        bindingInfo.m_ExpectedBufferSizeOrDivisor);
                    
                    Buffer::BufferType bufferType = bindingInfo.m_BufferType == ShaderBindingType::UniformBuffer ?
                        Buffer::BufferType::Uniform : Buffer::BufferType::ShaderStorage;

                    auto gpuBuffer = ShaderBufferFrameData();
                    mat.m_ShaderBuffers.emplace(binding,
                        Material::ShaderBufferBindingData (descriptorSetInfo.m_SetNumber,
                            bindingInfo.m_BindingIndex,
                            bindingInfo.m_ExpectedBufferSizeOrDivisor,
                            bufferType,
                            // Deliberately empty, other code can create buffers and assign
                            gpuBuffer
                        ));
                }
            }
        }
    };

vku::Material::Material(IAllocator& alloc) :
    m_DescriptorSets(alloc),
    m_PushConstants(alloc),
    m_ShaderBuffers(alloc),
    m_Samplers(alloc),
    m_UniformBufferAccessors(alloc),
    m_ShaderName(alloc)
{
}


vku::Material::ShaderBufferBindingData::ShaderBufferBindingData(uint32_t set, uint32_t binding, VkDeviceSize size, Buffer::BufferType bufferType, vku::ShaderBufferFrameData& buffer)
    : m_Binding(set, binding, size), m_BufferType(bufferType), m_Buffer(buffer)
{
}

bool vku::Material::ShaderBufferBindingData::Ready() {
    bool ready = false;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ready = m_Buffer.m_UniformBuffers[i].get() != nullptr;
        if (!ready)
        {
            break;
        }
        ready = m_Buffer.m_UniformBuffers[i]->m_GpuBuffer != VK_NULL_HANDLE;
        if (!ready)
        {
            break;
        }
    }
    return ready;
}

void vku::Material::UpdateDescriptors(VkState& vk)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        // write buffers to descriptor set + default texture for any samplers
        Vector<VkDescriptorBufferInfo>  bufferWriteInfos(*vk.m_CPUAllocator);
        for (auto&& [setBinding, bufferInfo] : m_ShaderBuffers)
        {
            if (!bufferInfo.Ready())
            {
                VKU_LOG_WARN("Material with name %s : set %d, binding %d has no associated buffer", m_ShaderName.c_str(), setBinding.m_SetBinding.m_Set, setBinding.m_SetBinding.m_Binding);
                continue;
            }
            VkDescriptorBufferInfo bufferWriteInfo{};
            bufferWriteInfo.buffer = bufferInfo.m_Buffer.m_UniformBuffers[0]->m_GpuBuffer;
            bufferWriteInfo.offset = 0;
            bufferWriteInfo.range = bufferInfo.m_Binding.m_BindingSize;
            bufferWriteInfos.push_back(bufferWriteInfo);
        }
        Vector<VkDescriptorImageInfo>   imageWriteInfos(*vk.m_CPUAllocator);;
        Vector<uint32_t> bindings(*vk.m_CPUAllocator);;
        for (auto [name, sampler] : m_Samplers)
        {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = sampler.m_ImageView;
            imageInfo.sampler = sampler.m_Sampler;
            imageWriteInfos.push_back(imageInfo);
            bindings.push_back(sampler.m_BindingNumber);
        }

        Vector<VkWriteDescriptorSet> descriptorWrites(*vk.m_CPUAllocator);

        int k = 0;
        for (auto&& [setBinding, ubo] : m_ShaderBuffers)
        {
            if (! ubo.Ready())
            {
                continue;
            }
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_DescriptorSets.front().m_Sets[i];
            write.dstBinding = ubo.m_Binding.m_SetBinding.m_Binding;
            write.dstArrayElement = 0; // todo
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferWriteInfos[k];
            descriptorWrites.push_back(write);
            k++;
        }
        for (int j = 0; j < imageWriteInfos.size(); j++)
        {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_DescriptorSets.front().m_Sets[i];
            write.dstBinding = bindings[j];
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageWriteInfos[j];
            descriptorWrites.push_back(write);
        }

        vkUpdateDescriptorSets(vk.m_LogicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    }
}

// todo: add ability to add existing buffers when creating the material
vku::Material vku::Material::Create(VkState & vk, ShaderProgram& shader)
{
    Material mat(*vk.m_CPUAllocator);

    String materialShaderName(*vk.m_CPUAllocator);

    for (auto& stage : shader.m_Stages)
    {
        materialShaderName += stage.m_Name + " ";
    }
    
    mat.m_ShaderName = materialShaderName;

    // Create Descriptors
    mat.m_DescriptorSets.push_back(FrameDescriptorSets{});
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        mat.m_DescriptorSets.front().m_Sets[i] = vk.m_DescriptorSetAllocator.Allocate(*vk.m_CPUAllocator, vk.m_LogicalDevice, shader.m_DescriptorSetLayout, nullptr);
    }

    // Collect
    for (auto& stage : shader.m_Stages)
    {
        reflect_descriptor_info(stage, mat);
    }

    return mat;
}

void vku::Material::AttachBuffer(VkState& vk, uint32_t frameIndex, uint32_t set, uint32_t binding, ShaderBufferFrameData& buffer)
{
    VkDeviceSize size = buffer.m_UniformBuffers[frameIndex]->m_Size;
    DescriptorSetBinding b (set, binding, size);
    
    if (m_ShaderBuffers.find(b) == m_ShaderBuffers.end())
    {
        VKU_LOG_ERR("Material with shader %s : No binding at set %s, binding %s", m_ShaderName.c_str(), set, binding);
        return;
    }

    ShaderBufferBindingData bindingData(set, binding, size, m_ShaderBuffers[b].m_BufferType, buffer);
    
    m_ShaderBuffers[b] = bindingData;

    UpdateDescriptors(vk);
}

void vku::Material::CreateBuffer(VkState& vk, uint32_t set, uint32_t binding)
{
    DescriptorSetBinding bind_handle{};
    for (auto& [b, _] : m_ShaderBuffers)
    {
        if (b.m_SetBinding.m_Set == set && b.m_SetBinding.m_Binding == binding)
        {
            bind_handle = b;
            break;
        }
    }

    if (m_ShaderBuffers.find(bind_handle) == m_ShaderBuffers.end())
    {
        VKU_LOG_ERR("Material with shader %s : No binding at set %s, binding %s", m_ShaderName.c_str(), set, binding);
        return;
    }

    ShaderBufferFrameData newBuffer = m_ShaderBuffers[bind_handle].m_BufferType == Buffer::BufferType::Uniform ?
        buffers::CreateUniformBuffers(vk, bind_handle.m_BindingSize) :
        buffers::CreateShaderStorageBuffers(vk, bind_handle.m_BindingSize);

    m_ShaderBuffers[bind_handle].m_Buffer = newBuffer;
    UpdateDescriptors(vk);
}

bool vku::Material::SetSampler(VkState & vk, const char* name, const VkImageView& imageView, const VkSampler& sampler, bool isAttachment)
{
    auto nameStr = String(name, *vk.m_CPUAllocator);
    if (m_Samplers.find(nameStr) == m_Samplers.end())
    {
        VKU_LOG_ERR("Material with shader %s: No associated sampler with name %s", m_ShaderName.c_str(), name);
        return false;
    }

    VkImageLayout imageLayout = isAttachment ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    SamplerBindingData& samplerBinding = m_Samplers.at(nameStr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = imageLayout;
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSets[samplerBinding.m_SetNumber].m_Sets[i];
        write.dstBinding = samplerBinding.m_BindingNumber;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vk.m_LogicalDevice, 1, &write, 0, nullptr);
    }
    return true;
}

bool vku::Material::SetColourAttachment(VkState & vk, const String& name, Framebuffer& framebuffer, uint32_t colourAttachmentIndex)
{
    if (m_Samplers.find(name) == m_Samplers.end())
    {
        VKU_LOG_ERR("Material with shader %s : No associated sampler with name %s", m_ShaderName.c_str(), name.c_str());
        return false;
    }

    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    SamplerBindingData& samplerBinding = m_Samplers.at(name);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = imageLayout;
        imageInfo.imageView = framebuffer.m_ColourAttachments[colourAttachmentIndex].m_AttachmentSwapchainImages[i].m_ImageView;
        imageInfo.sampler = framebuffer.m_ColourAttachments[colourAttachmentIndex].m_AttachmentSwapchainImages[i].m_Sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSets[samplerBinding.m_SetNumber].m_Sets[i];
        write.dstBinding = samplerBinding.m_BindingNumber;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vk.m_LogicalDevice, 1, &write, 0, nullptr);
    }

    return true;

}

bool vku::Material::SetColourAttachment(VkState& vk, const char* name, Framebuffer& framebuffer, uint32_t colourAttachmentIndex)
{
    String name_str(name, *vk.m_CPUAllocator);
    return SetColourAttachment(vk, name_str, framebuffer, colourAttachmentIndex);
}


bool vku::Material::SetDepthAttachment(VkState & vk, const String& name, Framebuffer& framebuffer)
{
    if (m_Samplers.find(name) == m_Samplers.end())
    {
        return false;
    }

    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    SamplerBindingData& samplerBinding = m_Samplers.at(name);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = imageLayout;
        imageInfo.imageView = framebuffer.m_DepthAttachments[0].m_AttachmentSwapchainImages[i].m_ImageView;
        imageInfo.sampler = framebuffer.m_DepthAttachments[0].m_AttachmentSwapchainImages[i].m_Sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSets[samplerBinding.m_SetNumber].m_Sets[i];
        write.dstBinding = samplerBinding.m_BindingNumber;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vk.m_LogicalDevice, 1, &write, 0, nullptr);
    }
    return true;
}

bool vku::Material::SetDepthAttachment(VkState& vk, const char* name, Framebuffer& framebuffer)
{
    String name_str(name, *vk.m_CPUAllocator);
    return SetDepthAttachment(vk, name_str, framebuffer);
}


void vku::Material::Free(VkState & vk)
{
    m_UniformBufferAccessors.clear();

    for (auto& [setBinding, buffer] : m_ShaderBuffers)
    {
        buffer.m_Buffer.Free(vk);
    }

    m_ShaderBuffers.clear();
    m_Samplers.clear();

    /*for (auto& frameDescriptorSets : m_DescriptorSets)
    {
        for (auto& set : frameDescriptorSets.m_Sets)
        {
            VK_CHECK(vkFreeDescriptorSets(vk.m_LogicalDevice, vk.m_DescriptorPool, 1, &set));
        }
    }*/


}

bool vku::Material::SetSampler(vku::VkState &vk, const char* name,
                               vku::Texture &texture) {
    return SetSampler(vk, name, texture.m_ImageView, texture.m_Sampler, false);
}
