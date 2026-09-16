#pragma once
#include "VkSDL.h"
#include "assimp/Importer.hpp"
#include "assimp/cimport.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "vku/vku.h"
#include "ThirdParty/nanovg.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "volk.h"

void vku_internal_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

glm::vec3 CalculateVec3Radians(glm::vec3 eulerDegrees) {
    return glm::vec3(glm::radians(eulerDegrees.x), glm::radians(eulerDegrees.y), glm::radians(eulerDegrees.z));
}

glm::vec3 CalculateVec3Degrees(glm::vec3 eulerRadians) {
    return glm::vec3(glm::degrees(eulerRadians.x), glm::degrees(eulerRadians.y), glm::degrees(eulerRadians.z));
}

glm::quat CalculateRotationQuat(glm::vec3 eulerDegrees) {
    glm::vec3 eulerRadians = CalculateVec3Radians(eulerDegrees);
    glm::quat xRotation = glm::angleAxis(eulerRadians.x, glm::vec3(1, 0, 0));
    glm::quat yRotation = glm::angleAxis(eulerRadians.y, glm::vec3(0, 1, 0));
    glm::quat zRotation = glm::angleAxis(eulerRadians.z, glm::vec3(0, 0, 1));

    return zRotation * yRotation * xRotation;
}


struct MvpData {
    glm::mat4 Model;
    glm::mat4 View;
    glm::mat4 Proj;
};

struct MvpData2 {
    glm::mat4 Model;
};


struct AABB
{
    glm::vec3 m_Min;
    glm::vec3 m_Max;
};



struct MeshEx {
    vku::Mesh m_Mesh;
    AABB m_AABB;
    glm::mat4 m_OBB;

    uint32_t m_IndexCount;
    uint32_t m_MaterialIndex;

    void Bind(VkCommandBuffer &cmd)
    {
      const VkDeviceSize sizes[] = { 0 };
      vkCmdBindVertexBuffers(cmd, 0, 1, &m_Mesh.m_VertexBuffer.m_GpuBuffer, sizes);
      vkCmdBindIndexBuffer(cmd, m_Mesh.m_IndexBuffer.m_GpuBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
};

struct MaterialEx
{
    vku::Texture m_Diffuse;
};

struct Model
{
    vku::Vector<MeshEx>        m_Meshes;
    vku::Vector<MaterialEx>  m_Materials;

    Model(vku::IAllocator& alloc) :
        m_Meshes(alloc), m_Materials(alloc) {
    }
};

struct Transform {
    glm::vec3 m_Position = glm::vec3(0);
    glm::vec3 m_Rotation = glm::vec3(0);
    glm::vec3 m_Scale = glm::vec3(1);
    glm::mat4 to_mat4() {
        glm::mat4 m = glm::translate(glm::mat4(1), m_Position);
        glm::vec3 radians = CalculateVec3Radians(m_Rotation);
        m *= glm::mat4_cast(glm::quat(radians));
        m = glm::scale(m, m_Scale);
        return m;
    };
};


struct DirectionalLight
{
    glm::vec4 Direction;
    glm::vec4 Ambient;
    glm::vec4 Colour;
};

struct PointLight
{
    glm::vec4 PositionRadius;
    glm::vec4 Ambient;
    glm::vec4 Colour;
};

struct SpotLight
{
    glm::vec4 PositionRadius;
    glm::vec4 DirectionAngle;
    glm::vec4 Ambient;
    glm::vec4 Colour;

};

template<size_t _Size>
struct FrameLightDataT
{
    DirectionalLight                m_DirectionalLight;
    std::array<PointLight, _Size>   m_PointLights;
    std::array<SpotLight, _Size>    m_SpotLights;

    uint32_t            m_DirectionalLightActive;
    uint32_t            m_PointLightsActive;
    uint32_t            m_SpotLightsActive;
    uint32_t            _padding;
};

float Random(float max = 1.0f)
{
    return static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / max));
}

template<size_t _Size>
void FillExampleLightData(FrameLightDataT<_Size>& lightData)
{
    lightData.m_DirectionalLight.Colour = { 0.66f, 0.66f, 0.66f, 0.0f };
    lightData.m_DirectionalLight.Direction = { -13.0f, -2.33f, -10.0f, 0.0f };

    glm::vec4 centerPos = { -0.2f, -0.2f, 0.1f, 0.0f };
    float areaSpread = 1.0f;
    float directionSpread = 12.0f;
    float colourSpread = 100.0f;
    float radiusSpread = 0.02f;

    for (size_t i = 0; i < _Size; i++)
    {
        lightData.m_PointLights[i].PositionRadius = { centerPos.x + Random(areaSpread), centerPos.y + Random(areaSpread), centerPos.z + Random(areaSpread), Random(radiusSpread) };
        lightData.m_PointLights[i].Colour = { Random(colourSpread), Random(colourSpread), Random(colourSpread), 0.0f };

        lightData.m_SpotLights[i].PositionRadius = { centerPos.x + Random(areaSpread), centerPos.y + Random(areaSpread), centerPos.z + Random(areaSpread), Random(radiusSpread) };
        lightData.m_SpotLights[i].DirectionAngle = { Random(directionSpread), Random(directionSpread), -Random(directionSpread), Random(areaSpread) };
        lightData.m_SpotLights[i].Colour = { Random(colourSpread), Random(colourSpread), Random(colourSpread), 0.0f };
    }
}

static glm::vec2 AssimpToGLM(aiVector2D& aiVec)
{
    return { aiVec.x, aiVec.y};
}

static glm::vec3 AssimpToGLM(aiVector3D& aiVec)
{
    return { aiVec.x, aiVec.y, aiVec.z };
}

static vku::String AssimpToSTD(vku::IAllocator& alloc, aiString str) {
    return vku::String(str.C_Str(), alloc);
}

void FreeMesh(vku::VkState & vk, MeshEx& m)
{
    m.m_Mesh.Free(vk);
}

void FreeModel(vku::VkState & vk, Model& model)
{
    for (MeshEx& m : model.m_Meshes)
    {
        FreeMesh(vk, m);
    }
}

void ProcessMesh(vku::VkState & vk, Model& model, aiMesh* mesh, aiNode* node, const aiScene* scene) {
    using namespace vku;
    bool hasPositions = mesh->HasPositions();
    bool hasUVs = mesh->HasTextureCoords(0);
    bool hasIndices = mesh->HasFaces();

    Vector<VertexDataPosUv> verts(*vk.m_CPUAllocator);
    if (hasPositions && hasUVs) {
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            VertexDataPosUv vert {};
            vert.Position = AssimpToGLM(mesh->mVertices[i]);
            vert.UV = glm::vec2 { mesh->mTextureCoords[0][i].x, 1.0f - mesh->mTextureCoords[0][i].y };
            verts.push_back(vert);
        }

    }
    Vector<uint32_t> indices(*vk.m_CPUAllocator);
    if (hasIndices) {
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace currentFace = mesh->mFaces[i];
            if (currentFace.mNumIndices != 3) {
                VKU_LOG_ERR("Attempting to import a mesh with non triangular face structure! cannot load this mesh.");
                return;
            }
            for (unsigned int index = 0; index < mesh->mFaces[i].mNumIndices; index++) {
                indices.push_back(static_cast<uint32_t>(mesh->mFaces[i].mIndices[index]));
            }
        }
    }
    AABB aabb = { {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z},
                    {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z} };
    MeshEx m{};
    m.m_Mesh.m_VertexBuffer = buffers::CreateVertexBuffer<VertexDataPosUv>(vk, verts);
    m.m_Mesh.m_IndexBuffer  = buffers::CreateIndexBuffer(vk, indices);
    m.m_IndexCount = static_cast<uint32_t>(indices.size());
    m.m_AABB = aabb;
    model.m_Meshes.push_back(m);
}

AABB TransformAABB(AABB& in, glm::mat4& m)
{
    float scalingFactor = m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2];
    float f = 1.0f / scalingFactor;
    glm::mat3 rotationMatrix =
    {
        {m[0][0] * f, m[0][1] * f, m[0][2] * f},
        {m[1][0] * f, m[1][1] * f, m[1][2] * f},
        {m[2][0] * f, m[2][1] * f, m[2][2] * f}
    };

    glm::vec3 translation = { m[0][3],m[1][3], m[2][3] };
    AABB ret;
    ret.m_Min = glm::vec3(0.0f);
    ret.m_Max = glm::vec3(0.0f);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            auto a = rotationMatrix[i][j] * in.m_Min[j];
            auto b = rotationMatrix[i][j] * in.m_Max[j];

            ret.m_Min[i] += a < b ? a : b;
            ret.m_Max[i] += a < b ? b : a;
        }
    }

    return ret;
}

void ProcessMeshWithNormals(vku::VkState & vk, Model& model, aiMesh* mesh, aiNode* node, const aiScene* scene) {
    using namespace vku;
    bool hasPositions = mesh->HasPositions();
    bool hasUVs = mesh->HasTextureCoords(0);
    bool hasNormals = mesh->HasNormals();
    bool hasIndices = mesh->HasFaces();

    Vector<aiMaterialProperty*> properties(*vk.m_CPUAllocator);
    aiMaterial* meshMaterial = scene->mMaterials[mesh->mMaterialIndex];

    for (unsigned int i = 0; i < meshMaterial->mNumProperties; i++)
    {
        aiMaterialProperty* prop = meshMaterial->mProperties[i];
        properties.push_back(prop);
    }
    Vector<VertexDataPosNormalUv> verts(*vk.m_CPUAllocator);
    if (hasPositions && hasUVs && hasNormals) {
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            VertexDataPosNormalUv vert{};
            vert.Position = AssimpToGLM(mesh->mVertices[i]);
            vert.UV = { mesh->mTextureCoords[0][i].x, 1.0f - mesh->mTextureCoords[0][i].y };
            vert.Normal = AssimpToGLM(mesh->mNormals[i]);
            verts.push_back(vert);
        }

    }
    Vector<uint32_t> indices(*vk.m_CPUAllocator);
    if (hasIndices) {
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace currentFace = mesh->mFaces[i];
            if (currentFace.mNumIndices != 3) {
                VKU_LOG_ERR("Attempting to import a mesh with non triangular face structure! cannot load this mesh.");
                return;
            }
            for (unsigned int index = 0; index < mesh->mFaces[i].mNumIndices; index++) {
                indices.push_back(static_cast<uint32_t>(mesh->mFaces[i].mIndices[index]));
            }
        }
    }
    AABB aabb = { {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z},
                {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z} };

    MeshEx m{};
    m.m_Mesh.m_VertexBuffer = buffers::CreateVertexBuffer<VertexDataPosNormalUv>(vk, verts);
    m.m_Mesh.m_IndexBuffer = buffers::CreateIndexBuffer(vk, indices);
    m.m_IndexCount = static_cast<uint32_t>(indices.size());
    m.m_MaterialIndex = mesh->mMaterialIndex;
    m.m_AABB = aabb;
    model.m_Meshes.push_back(m);
}

void ProcessNode(vku::VkState & vk, Model& model, aiNode* node, const aiScene* scene, bool withNormals = false) {

    if (node->mNumMeshes > 0) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            unsigned int sceneIndex = node->mMeshes[i];
            aiMesh* mesh = scene->mMeshes[sceneIndex];
            if (withNormals)
            {
                ProcessMeshWithNormals(vk, model, mesh, node, scene);
            }
            else
            {
                ProcessMesh(vk, model, mesh, node, scene);
            }
        }
    }

    if (node->mNumChildren == 0) {
        return;
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(vk, model, node->mChildren[i], scene);
    }
}

void LoadModelAssimp(vku::VkState & vk, Model& model, const char* path, bool withNormals = false)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_OptimizeMeshes |
        aiProcess_GenSmoothNormals |
        aiProcess_OptimizeGraph |
        aiProcess_FixInfacingNormals |
        aiProcess_FindInvalidData | 
        aiProcess_GenBoundingBoxes
    );
    //
    if (scene == nullptr) {
        VKU_LOG_ERR("AssimpModelAssetFactory : Failed to load asset at path : %s", path);
        return;
    }
    ProcessNode(vk, model, scene->mRootNode, scene, withNormals);

    // TODO: STL substr does not accept an allocator as param and so returned string 
    // MUST be default allocated :( EASTL does not do this, uses allocator 
    // provided on the string be substr'd
    std::string path_stl(path);
    auto lastDelim = path_stl.find_last_of('/') + 1;
    std::string dir_stl = path_stl.substr(0, lastDelim);
    vku::String directory = vku::String(dir_stl, *vk.m_CPUAllocator);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        aiMaterial* meshMaterial = scene->mMaterials[i];

        uint32_t diffuseCount = aiGetMaterialTextureCount(meshMaterial, aiTextureType_DIFFUSE);
        if(diffuseCount == 0)
        {
            diffuseCount = aiGetMaterialTextureCount(meshMaterial, aiTextureType_BASE_COLOR);
        }

        if(diffuseCount == 0)
        {
            diffuseCount = aiGetMaterialTextureCount(meshMaterial, aiTextureType_UNKNOWN);
        }

        if (diffuseCount > 0)
        {
            aiString resultPath;
            aiGetMaterialTexture(meshMaterial, aiTextureType_DIFFUSE, 0, &resultPath);
            vku::String finalPath = directory + vku::String(resultPath.C_Str(), *vk.m_CPUAllocator);
            vku::Texture texture = vku::Texture::CreateTexture(vk, finalPath.c_str(), VK_FORMAT_R8G8B8A8_UNORM);
            model.m_Materials.push_back({ texture });
        }
    }
}

MeshEx BuildScreenSpaceQuad(vku::VkState & vk, vku::Vector <vku::VertexDataPosUv > & verts, vku::Vector<uint32_t>& indices)
{
    MeshEx m{};
    m.m_Mesh.m_VertexBuffer = vku::buffers::CreateVertexBuffer<vku::VertexDataPosUv>(vk, verts);
    m.m_Mesh.m_IndexBuffer  = vku::buffers::CreateIndexBuffer(vk, indices);
    m.m_IndexCount = 6;
    return m;
}


#define NUM_LIGHTS 512
using DeferredLightData = FrameLightDataT<NUM_LIGHTS>;

struct RenderItem
{
    MeshEx              m_Mesh;
    vku::Material       m_Material;

    RenderItem(vku::IAllocator& alloc) : m_Material(alloc) {}
};

struct RenderModel
{
    Model m_Original;
    vku::Vector<RenderItem> m_RenderItems;

    RenderModel(vku::IAllocator& alloc) : m_RenderItems(alloc), m_Original(alloc)
    {

    }

    void Free(vku::VkState & vk)
    {
        for (auto& item : m_RenderItems)
        {
            item.m_Material.Free(vk);
            FreeMesh(vk, item.m_Mesh);
        }

        for (auto& mat : m_Original.m_Materials)
        {
            mat.m_Diffuse.Free(vk);
        }
        m_RenderItems.clear();
    }
};

struct Camera
{
    glm::vec3 Position;
    glm::vec3 Rotation;
    float FOV = 90.0f;
    float Near = 0.001f, Far = 300.0f;

    glm::mat4 View;
    glm::mat4 Proj;
};