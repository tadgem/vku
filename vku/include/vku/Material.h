#pragma once
#include "vku/Framebuffer.h"
#include "vku/Texture.h"
#include VKU_UTILITY_ALIAS
namespace vku
{
    struct ShaderProgram;
    class Material
    {
    public:
        // create a material from a shader
        // material is the interface to an instance of a shader
        // contains descriptor set and associated buffers.
        // set mat4, mat3, vec4, vec3, sampler etc.
        // reflect the size of each bound thing in each set (one set for now)

        class ShaderBufferBindingData
        {
        public:
            DescriptorSetBinding    m_Binding;
            ShaderBufferFrameData   m_Buffer;
            Buffer::BufferType      m_BufferType;

            ShaderBufferBindingData(uint32_t set, uint32_t binding, VkDeviceSize size, Buffer::BufferType bufferType, ShaderBufferFrameData& buffer);
            ShaderBufferBindingData() = default;

            bool    Ready();

        };

        struct SamplerBindingData
        {
            uint32_t        m_SetNumber;
            uint32_t        m_BindingNumber;
            VkImageView     m_ImageView;
            VkSampler       m_Sampler;
        };

        struct ShaderAccessorData
        {
            uint32_t                m_ExpectedSize;
            uint32_t                m_Offset;
            uint32_t                m_Stride;
            uint32_t                m_ArraySize;
            uint32_t                m_BufferIndex;
            Buffer::BufferType      m_BufferType;

        };

        struct FrameDescriptorSets
        {
            Array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_Sets;
        };

        String                                                      m_ShaderName;
        Vector<FrameDescriptorSets>                                 m_DescriptorSets;
        Vector<PushConstantBlock>                                   m_PushConstants;
        HashMap<DescriptorSetBinding, ShaderBufferBindingData>      m_ShaderBuffers;
        HashMap<String, ShaderAccessorData>                         m_UniformBufferAccessors;
        HashMap<String, SamplerBindingData>                         m_Samplers;

        Material(IAllocator& alloc);

        static Material Create(VkState & vk, ShaderProgram& shader);

        void AttachBuffer(VkState& vk, uint32_t frameIndex, uint32_t set, uint32_t binding, ShaderBufferFrameData& buffer);
        void CreateBuffer(VkState& vk, uint32_t set, uint32_t binding);

        void UpdateDescriptors(VkState& vk);

        template<typename _Ty>
        bool SetBuffer(uint32_t frameIndex, uint32_t set, uint32_t binding, const _Ty& value)
        {
            static constexpr size_t _type_size = sizeof(_Ty);
            DescriptorSetBinding sb (set, binding, _type_size);

            if (m_ShaderBuffers.find(sb) == m_ShaderBuffers.end())
            {
                return false;
            }

            if (!m_ShaderBuffers[sb].Ready())
            {
                return false;
            }
            m_ShaderBuffers[sb].m_Buffer.Set(frameIndex, value);

            return true;
        }

        template<typename _Ty>
        bool SetBuffer(uint32_t frameIndex, uint32_t set, uint32_t binding, _Ty* start, uint32_t count)
        {
            static constexpr size_t _type_size = sizeof(_Ty);
            DescriptorSetBinding sb(set, binding, _type_size);

            if (m_ShaderBuffers.find(sb) == m_ShaderBuffers.end())
            {
                return false;
            }

            if (!m_ShaderBuffers[sb].Ready())
            {
                return false;
            }

            m_ShaderBuffers[sb].m_Buffer.SetMemory(frameIndex, start, count);

            return true;
        }


        template<typename _Ty>
        bool SetBufferArrayElement(uint32_t frameIndex, uint32_t set, uint32_t binding, uint32_t index, const _Ty& value, uint32_t innerElementOffset = 0)
        {
            // size of each element
            static constexpr size_t _type_size = sizeof(_Ty);
            DescriptorSetBinding sb(set, binding, _type_size);

            if (m_ShaderBuffers.find(sb) == m_ShaderBuffers.end())
            {
                return false;
            }

            if (!m_ShaderBuffers[sb].Ready())
            {
                return false;
            }
            uint64_t offset = (_type_size * index) + innerElementOffset;
            m_ShaderBuffers[sb].m_Buffer.Set(frameIndex, value, offset);
        }

        bool SetSampler(VkState & vk, const char* name, const VkImageView& imageView, const VkSampler& sampler, bool isAttachment = false);
        bool SetSampler(VkState & vk, const char* name, Texture& texture);
        bool SetColourAttachment(VkState& vk, const String& name, Framebuffer& framebuffer, uint32_t colourAttachmentIndex);
        bool SetColourAttachment(VkState & vk, const char* name, Framebuffer& framebuffer, uint32_t colourAttachmentIndex);
        bool SetDepthAttachment(VkState & vk, const String& name, Framebuffer& framebuffer);
        bool SetDepthAttachment(VkState& vk, const char* name, Framebuffer& framebuffer);


        
        void Free(VkState & vk);
    };

}