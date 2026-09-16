#include "vku/Mesh.h"
#include "vku/Buffer.h"
#include "volk.h"

vku::Mesh* vku::Mesh::g_ScreenSpaceQuad = nullptr;

static vku::StaticVector<vku::VertexDataPosUv> g_ScreenSpaceQuadVertexData = {
    { { -1.0f, -1.0f , 0.0f}, { 0.0f, 0.0f } },
    { {1.0f, -1.0f, 0.0f}, {1.0f, 0.0f} },
    { {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
    { {-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f} }
};

static vku::StaticVector<uint32_t> g_ScreenSpaceQuadIndexData = {
    0, 1, 2, 2, 3, 0
};

void vku::Mesh::InitBuiltInMeshes(vku::VkState & vk)
{
    Buffer vertexBuffer = buffers::CreateVertexBuffer<VertexDataPosUv>(vk, g_ScreenSpaceQuadVertexData.data(), g_ScreenSpaceQuadVertexData.size());
    Buffer indexBuffer = buffers::CreateIndexBuffer(vk, g_ScreenSpaceQuadIndexData.data(), g_ScreenSpaceQuadIndexData.size());

    g_ScreenSpaceQuad = new Mesh { vertexBuffer, indexBuffer,  6 };
}

void vku::Mesh::FreeBuiltInMeshes(vku::VkState & vk)
{
    g_ScreenSpaceQuad->Free(vk);
    delete g_ScreenSpaceQuad;
    g_ScreenSpaceQuad = nullptr;
}

void vku::Mesh::Free (VkState & vk)
{
    m_VertexBuffer.Free(vk);
    m_IndexBuffer.Free(vk);
}

#define GET_VERTEX_DESCRIPTION_IMPL \
VertexDescription vd(alloc);\
vd.m_AttributeDescriptions = GetAttributeDescriptions(alloc);\
vd.m_BindingDescriptions = Vector<VkVertexInputBindingDescription>(alloc);\
vd.m_BindingDescriptions.push_back(GetBindingDescription());\
return vd;

vku::VertexDescription vku::VertexDataPosColUv::GetVertexDescription(IAllocator& alloc) {
    GET_VERTEX_DESCRIPTION_IMPL;
}
vku::VertexDescription vku::VertexDataPos4::GetVertexDescription(IAllocator& alloc) {
    GET_VERTEX_DESCRIPTION_IMPL;
}
vku::VertexDescription vku::VertexDataPosUv::GetVertexDescription(IAllocator& alloc) {
    GET_VERTEX_DESCRIPTION_IMPL;
}
vku::VertexDescription vku::VertexDataPosNormalUv::GetVertexDescription(IAllocator& alloc) {
    GET_VERTEX_DESCRIPTION_IMPL;
}
