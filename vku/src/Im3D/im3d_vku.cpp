#include "Im3D/im3d_vku.h"
#include "Im3D/im3d_math.h"
#include "Im3D/shaders/im3d_lines.frag.spv.h"
#include "Im3D/shaders/im3d_lines.vert.spv.h"
#include "Im3D/shaders/im3d_points.frag.spv.h"
#include "Im3D/shaders/im3d_points.vert.spv.h"
#include "Im3D/shaders/im3d_tris.frag.spv.h"
#include "Im3D/shaders/im3d_tris.vert.spv.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "vku/Buffer.h"
#include "vku/Debug.h"
#include "vku/Defaults.h"
#include "vku/Log.h"
#include "vku/Mesh.h"
#include "vku/Pipeline.h"
#include "volk.h"

namespace vku {
Vector<unsigned char> ToVector(IAllocator &alloc, const unsigned char *src,
                               uint32_t count) {
  Vector<unsigned char> chars(alloc);
  for (uint32_t i = 0; i < count; i++) {
    chars.push_back(src[i]);
  }
  return chars;
}

VkuIm3dState LoadIm3D(VkState &vk) {

  ShaderProgram im3d_tris_33 =
      ShaderProgram::CreateShaderSlang(vk, "shaders/im3d_tris", {"v", "f"})
          .value();

  Vector<unsigned char> tris_vert_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_tris_vert_spv_bin[0],
               (uint32_t)im3d_tris_vert_spv_bin_SIZE);
  ShaderStage tris_vert =
      ShaderStage::CreateFromBinary(vk, tris_vert_bin, "Im3dTrisVert");
  Vector<unsigned char> tris_frag_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_tris_frag_spv_bin[0],
               (uint32_t)im3d_tris_frag_spv_bin_SIZE);
  ShaderStage tris_frag =
      ShaderStage::CreateFromBinary(vk, tris_frag_bin, "Im3dTrisFrag");
  ShaderProgram tris_prog =
      ShaderProgram::CreateGraphics(vk, tris_vert, tris_frag);

  Vector<unsigned char> lines_vert_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_lines_vert_spv_bin[0],
               (uint32_t)im3d_lines_vert_spv_bin_SIZE);
  ShaderStage lines_vert =
      ShaderStage::CreateFromBinary(vk, lines_vert_bin, "Im3dLinesVert");
  Vector<unsigned char> lines_frag_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_lines_frag_spv_bin[0],
               (uint32_t)im3d_lines_frag_spv_bin_SIZE);
  ShaderStage lines_frag =
      ShaderStage::CreateFromBinary(vk, lines_frag_bin, "Im3dLinesFrag");
  ShaderProgram lines_prog =
      ShaderProgram::CreateGraphics(vk, lines_vert, lines_frag);

  Vector<unsigned char> points_vert_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_points_vert_spv_bin[0],
               (uint32_t)im3d_points_vert_spv_bin_SIZE);
  ShaderStage points_vert =
      ShaderStage::CreateFromBinary(vk, points_vert_bin, "Im3dPointsVert");
  Vector<unsigned char> points_frag_bin =
      ToVector(*vk.m_CPUAllocator, &im3d_points_frag_spv_bin[0],
               (uint32_t)im3d_points_frag_spv_bin_SIZE);
  ShaderStage points_frag =
      ShaderStage::CreateFromBinary(vk, points_frag_bin, "Im3dPointsFrag");
  ShaderProgram points_prog =
      ShaderProgram::CreateGraphics(vk, points_vert, points_frag);

  Vector<VertexDataPos4> vertexData(*vk.m_CPUAllocator);
  vertexData.push_back(VertexDataPos4{{-1.0f, -1.0f, 0.0f, 1.0f}});
  vertexData.push_back(VertexDataPos4{{1.0f, -1.0f, 0.0f, 1.0f}});
  vertexData.push_back(VertexDataPos4{{1.0f, 1.0f, 0.0f, 1.0f}});
  vertexData.push_back(VertexDataPos4{{-1.0f, 1.0f, 0.0f, 1.0f}});

  Buffer vertexBuffer =
      buffers::CreateVertexBuffer<VertexDataPos4>(vk, vertexData);

  return {tris_prog, points_prog, lines_prog, vertexBuffer};
}

VkuIm3dViewState AddIm3dForViewport(VkState &vk, VkuIm3dState &state,
                                    VkRenderPass renderPass, bool enableMSAA,
                                    bool enableDynamicRendering) {
  auto vertexDescription =
      VertexDataPos4::GetVertexDescription(*vk.m_CPUAllocator);

  RasterizationState tris_raster_state{VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
                                       enableMSAA, VK_COMPARE_OP_LESS,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  Vector<VkFormat> formats(*vk.m_CPUAllocator);
  formats.push_back(VK_FORMAT_R8G8B8A8_UNORM);

  VkPipelineData tris_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_TriProg, vertexDescription, tris_raster_state,
                renderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_TriProg, vertexDescription, tris_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material tris_material = Material::Create(vk, state.m_TriProg);
  tris_material.CreateBuffer(vk, 0, 0);
  tris_material.CreateBuffer(vk, 0, 1);

  RasterizationState points_raster_state{
      VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE, enableMSAA, VK_COMPARE_OP_LESS,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineData points_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_PointsProg, vertexDescription, points_raster_state,
                renderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_PointsProg, vertexDescription, points_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material points_material = Material::Create(vk, state.m_PointsProg);
  points_material.CreateBuffer(vk, 0, 0);
  points_material.CreateBuffer(vk, 0, 1);

  RasterizationState lines_raster_state{VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE,
                                        enableMSAA, VK_COMPARE_OP_LESS,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineData lines_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_LinesProg, vertexDescription, lines_raster_state,
                renderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_LinesProg, vertexDescription, lines_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material lines_material = Material::Create(vk, state.m_LinesProg);
  lines_material.CreateBuffer(vk, 0, 0);
  lines_material.CreateBuffer(vk, 0, 1);

  return {tris_material, points_material, lines_material,
          tris_pipeline, points_pipeline, lines_pipeline};
}

VkuIm3dViewState AddIm3dForGBuffer(VkState &vk, VkuIm3dState &state) {
  auto vertexDescription =
      VertexDataPos4::GetVertexDescription(*vk.m_CPUAllocator);

  RasterizationState tris_raster_state{VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
                                       false, VK_COMPARE_OP_LESS,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  Vector<VkFormat> singleFormat(*vk.m_CPUAllocator);
  singleFormat.push_back(vk.m_SwapChainImageFormat);

  VkPipelineData tris_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_TriProg, vertexDescription, tris_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material tris_material = Material::Create(vk, state.m_TriProg);
  tris_material.CreateBuffer(vk, 0, 0);
  tris_material.CreateBuffer(vk, 0, 1);

  RasterizationState points_raster_state{
      VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE, false, VK_COMPARE_OP_LESS,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineData points_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_PointsProg, vertexDescription, points_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material points_material = Material::Create(vk, state.m_PointsProg);
  points_material.CreateBuffer(vk, 0, 0);
  points_material.CreateBuffer(vk, 0, 1);

  RasterizationState lines_raster_state{VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE,
                                        false, VK_COMPARE_OP_LESS,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineData lines_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_LinesProg, vertexDescription, lines_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material lines_material = Material::Create(vk, state.m_LinesProg);
  lines_material.CreateBuffer(vk, 0, 0);
  lines_material.CreateBuffer(vk, 0, 1);

  return {tris_material, points_material, lines_material,
          tris_pipeline, points_pipeline, lines_pipeline};
}

VkuIm3dViewState AddIm3dForDeferredLightPass(VkState &vk, VkuIm3dState &state) {
  auto vertexDescription =
      VertexDataPos4::GetVertexDescription(*vk.m_CPUAllocator);

  RasterizationState tris_raster_state{VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
                                       false, VK_COMPARE_OP_LESS,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  Vector<VkFormat> singleFormat(*vk.m_CPUAllocator);
  singleFormat.push_back(vk.m_SwapChainImageFormat);

  VkPipelineData tris_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_TriProg, vertexDescription, tris_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material tris_material = Material::Create(vk, state.m_TriProg);
  tris_material.CreateBuffer(vk, 0, 0);
  tris_material.CreateBuffer(vk, 0, 1);

  RasterizationState points_raster_state{
      VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE, false, VK_COMPARE_OP_LESS,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineData points_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_PointsProg, vertexDescription, points_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material points_material = Material::Create(vk, state.m_PointsProg);
  points_material.CreateBuffer(vk, 0, 0);
  points_material.CreateBuffer(vk, 0, 1);

  RasterizationState lines_raster_state{VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE,
                                        false, VK_COMPARE_OP_LESS,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineData lines_pipeline = pipelines::CreateDynamicRasterPipeline(
      vk, state.m_LinesProg, vertexDescription, lines_raster_state,
      vk.m_SwapChainImageExtent, singleFormat);

  Material lines_material = Material::Create(vk, state.m_LinesProg);
  lines_material.CreateBuffer(vk, 0, 0);
  lines_material.CreateBuffer(vk, 0, 1);

  return {tris_material, points_material, lines_material,
          tris_pipeline, points_pipeline, lines_pipeline};
}

VkuIm3dViewState AddIm3dForGBufferRenderPass(VkState &vk, VkuIm3dState &state,
                                             VkRenderPass gbufferRenderPass) {
  auto vertexDescription =
      VertexDataPos4::GetVertexDescription(*vk.m_CPUAllocator);

  RasterizationState tris_raster_state{VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
                                       false, VK_COMPARE_OP_LESS,
                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  Vector<VkFormat> singleFormat(*vk.m_CPUAllocator);
  singleFormat.push_back(vk.m_SwapChainImageFormat);

  VkPipelineData tris_pipeline = pipelines::CreateRasterPipeline(
      vk, state.m_TriProg, vertexDescription, tris_raster_state,
      gbufferRenderPass, vk.m_SwapChainImageExtent, 1);

  Material tris_material = Material::Create(vk, state.m_TriProg);
  tris_material.CreateBuffer(vk, 0, 0);
  tris_material.CreateBuffer(vk, 0, 1);

  RasterizationState points_raster_state{
      VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE, false, VK_COMPARE_OP_LESS,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineData points_pipeline = pipelines::CreateRasterPipeline(
      vk, state.m_PointsProg, vertexDescription, points_raster_state,
      gbufferRenderPass, vk.m_SwapChainImageExtent, 1);

  Material points_material = Material::Create(vk, state.m_PointsProg);
  points_material.CreateBuffer(vk, 0, 0);
  points_material.CreateBuffer(vk, 0, 1);

  RasterizationState lines_raster_state{VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE,
                                        false, VK_COMPARE_OP_LESS,
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineData lines_pipeline = pipelines::CreateRasterPipeline(
      vk, state.m_LinesProg, vertexDescription, lines_raster_state,
      gbufferRenderPass, vk.m_SwapChainImageExtent, 1);

  Material lines_material = Material::Create(vk, state.m_LinesProg);
  lines_material.CreateBuffer(vk, 0, 0);
  lines_material.CreateBuffer(vk, 0, 1);

  return {tris_material, points_material, lines_material,
          tris_pipeline, points_pipeline, lines_pipeline};
}

VkuIm3dViewState AddIm3dForForwardPass(VkState &vk, VkuIm3dState &state,
                                       bool enableDynamicRendering) {
  auto vertexDescription =
      VertexDataPos4::GetVertexDescription(*vk.m_CPUAllocator);

  RasterizationState tris_raster_state{
      VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, vk.m_UseSwapchainMsaa,
      VK_COMPARE_OP_LESS, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  Vector<VkFormat> formats(*vk.m_CPUAllocator);
  formats.push_back(vk.m_SwapChainImageFormat);

  VkPipelineData tris_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_TriProg, vertexDescription, tris_raster_state,
                vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_TriProg, vertexDescription, tris_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material tris_material = Material::Create(vk, state.m_TriProg);
  tris_material.CreateBuffer(vk, 0, 0);
  tris_material.CreateBuffer(vk, 0, 1);

  RasterizationState points_raster_state{
      VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE, vk.m_UseSwapchainMsaa,
      VK_COMPARE_OP_LESS, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineData points_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_PointsProg, vertexDescription, points_raster_state,
                vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_PointsProg, vertexDescription, points_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material points_material = Material::Create(vk, state.m_PointsProg);
  points_material.CreateBuffer(vk, 0, 0);
  points_material.CreateBuffer(vk, 0, 1);

  RasterizationState lines_raster_state{
      VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, vk.m_UseSwapchainMsaa,
      VK_COMPARE_OP_LESS, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineData lines_pipeline =
      !enableDynamicRendering
          ? pipelines::CreateRasterPipeline(
                vk, state.m_LinesProg, vertexDescription, lines_raster_state,
                vk.m_SwapchainImageRenderPass, vk.m_SwapChainImageExtent)
          :

          pipelines::CreateDynamicRasterPipeline(
              vk, state.m_LinesProg, vertexDescription, lines_raster_state,
              vk.m_SwapChainImageExtent, formats);

  Material lines_material = Material::Create(vk, state.m_LinesProg);
  lines_material.CreateBuffer(vk, 0, 0);
  lines_material.CreateBuffer(vk, 0, 1);

  return {tris_material, points_material, lines_material,
          tris_pipeline, points_pipeline, lines_pipeline};
}

void FreeIm3dViewport(VkState &vk, VkuIm3dViewState &viewState) {
  viewState.m_TrisMaterial.Free(vk);
  viewState.m_LinesMaterial.Free(vk);
  viewState.m_PointsMaterial.Free(vk);

  viewState.m_TrisPipeline.Free(vk);
  viewState.m_LinesPipeline.Free(vk);
  viewState.m_PointsPipeline.Free(vk);
}

Im3d::Mat4 ToIm3D(const glm::mat4 &_m) {
  Im3d::Mat4 m(1.0);
  for (int i = 0; i < 16; ++i) {
    m[i] = *(&(_m[0][0]) + i);
  }
  return m;
}

void FreeIm3d(VkState &vk, VkuIm3dState &state) {
  state.m_TriProg.Free(vk);
  state.m_LinesProg.Free(vk);
  state.m_PointsProg.Free(vk);
  state.m_ScreenQuadBuffer.Free(vk);
}

void DrawIm3d(VkState &vk, VkCommandBuffer &buffer, uint32_t frameIndex,
              VkuIm3dState &state, VkuIm3dViewState &viewState,
              glm::mat4 _viewProj, uint32_t width, uint32_t height,
              bool drawText) {
  auto &context = Im3d::GetContext();
  debug::BeginDebugMarker(buffer, "Im3D Pass");
  for (uint32_t i = 0; i < context.getDrawListCount(); i++) {
    auto drawList = &context.getDrawLists()[i];
    int primVertexCount;

    ShaderProgram *shader = nullptr;
    Material *mat = nullptr;
    VkPipelineLayout *pipelineLayout = nullptr;
    VkPipeline *pipeline = nullptr;
    switch (drawList->m_primType) {
    case Im3d::DrawPrimitiveType::DrawPrimitive_Triangles:
      shader = &state.m_TriProg;
      pipelineLayout = &viewState.m_TrisPipeline.m_PipelineLayout;
      pipeline = &viewState.m_TrisPipeline.m_Pipeline;
      mat = &viewState.m_TrisMaterial;
      primVertexCount = 3;
      break;
    case Im3d::DrawPrimitiveType::DrawPrimitive_Lines:
      shader = &state.m_LinesProg;
      pipelineLayout = &viewState.m_LinesPipeline.m_PipelineLayout;
      pipeline = &viewState.m_LinesPipeline.m_Pipeline;
      mat = &viewState.m_LinesMaterial;
      primVertexCount = 2;
      break;
    case Im3d::DrawPrimitiveType::DrawPrimitive_Points:
      shader = &state.m_PointsProg;
      pipelineLayout = &viewState.m_PointsPipeline.m_PipelineLayout;
      pipeline = &viewState.m_PointsPipeline.m_Pipeline;
      mat = &viewState.m_PointsMaterial;
      primVertexCount = 1;
      break;
    default:
      VKU_LOG_ERR("Im3d: unknown primitive type");
      return;
    }

    const int kMaxBufferSize =
        64 * 1024; // assuming 64kb here but the application should check the
                   // implementation limit
    const int kPrimsPerPass =
        kMaxBufferSize / (sizeof(Im3d::VertexData) * primVertexCount);

    int remainingPrimCount = drawList->m_vertexCount / primVertexCount;
    const Im3d::VertexData *vertexData = drawList->m_vertexData;

    struct CameraUniformData {
      glm::mat4 ViewProj;
      glm::vec2 ViewPort;
    };

    CameraUniformData camData{_viewProj, {width, height}};

    while (remainingPrimCount > 0) {

      int passPrimCount = remainingPrimCount < kPrimsPerPass
                              ? remainingPrimCount
                              : kPrimsPerPass;
      int passVertexCount = passPrimCount * primVertexCount;

      // access violation
      mat->SetBuffer(frameIndex, 0, 0, vertexData,
                     passVertexCount * sizeof(Im3d::VertexData));
      mat->SetBuffer(frameIndex, 0, 1, camData);

      VkViewport viewport{};
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.width = static_cast<float>(width);
      viewport.height = static_cast<float>(height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor{};
      scissor.offset = {0, 0};
      scissor.extent = VkExtent2D{static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height)};
      VkDeviceSize sizes[] = {0};
      vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
      vkCmdSetViewport(buffer, 0, 1, &viewport);
      vkCmdSetScissor(buffer, 0, 1, &scissor);
      vkCmdBindVertexBuffers(buffer, 0, 1,
                             &state.m_ScreenQuadBuffer.m_GpuBuffer, sizes);
      vkCmdBindDescriptorSets(
          buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1,
          &mat->m_DescriptorSets[0].m_Sets[frameIndex], 0, nullptr);

      vkCmdDraw(buffer, primVertexCount == 3 ? 3 : 4, passPrimCount, 0, 0);
      vertexData += passVertexCount;
      remainingPrimCount -= passPrimCount;
    }
  }
  debug::EndDebugMarker(buffer);
  if (!drawText) {
    return;
  }

  DrawIm3dTextListsImGui(vk.m_SwapChainImageExtent.width,
                         vk.m_SwapChainImageExtent.height, _viewProj);
}

void DrawIm3dTextListsImGui(uint32_t width, uint32_t height,
                            glm::mat4 _viewProj) {
  // Using ImGui here as a simple means of rendering text draw lists, however as
  // with primitives the application is free to draw text in any conceivable
  // manner.

  // Invisible ImGui window which covers the screen.
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
  ImGui::Begin("Invisible", nullptr,
               0 | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  DrawIm3dTextListsImGuiAsChild(_viewProj);

  ImGui::End();
  ImGui::PopStyleColor(1);
}

void DrawIm3dTextListsImGuiAsChild(glm::mat4 _viewProj) {
  ImDrawList *imDrawList = ImGui::GetWindowDrawList();
  uint32_t _count = Im3d::GetTextDrawListCount();
  const Im3d::Mat4 viewProj = ToIm3D(_viewProj);
  for (uint32_t i = 0; i < _count; ++i) {
    const Im3d::TextDrawList &textDrawList = Im3d::GetTextDrawLists()[i];

    if (textDrawList.m_layerId == Im3d::MakeId("NamedLayer")) {
      // The application may group primitives into layers, which can be used to
      // change the draw state (e.g. enable depth testing, use a different
      // shader)
    }

    for (uint32_t j = 0; j < textDrawList.m_textDataCount; ++j) {
      const Im3d::TextData &textData = textDrawList.m_textData[j];
      if (textData.m_positionSize.w == 0.0f ||
          textData.m_color.getA() == 0.0f) {
        continue;
      }

      // Project world -> screen space.
      Im3d::Vec4 clip = viewProj * Im3d::Vec4(textData.m_positionSize.x,
                                              textData.m_positionSize.y,
                                              textData.m_positionSize.z, 1.0f);
      Im3d::Vec2 screen = Im3d::Vec2(clip.x / clip.w, clip.y / clip.w);

      // Cull text which falls offscreen. Note that this doesn't take into
      // account text size but works well enough in practice.
      if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f) {
        continue;
      }

      // Pixel coordinates for the ImGuiWindow ImGui.
      screen = screen * Im3d::Vec2(0.5f) + Im3d::Vec2(0.5f);
      auto windowSize = ImGui::GetWindowSize();
      screen = screen * Im3d::Vec2{windowSize.x, windowSize.y};

      // All text data is stored in a single buffer; each textData instance has
      // an offset into this buffer.
      const char *text =
          textDrawList.m_textBuffer + textData.m_textBufferOffset;

      // Calculate the final text size in pixels to apply alignment flags
      // correctly.
      ImGui::SetWindowFontScale(
          textData.m_positionSize
              .w); // NB no CalcTextSize API which takes a font/size directly...
      auto textSize = ImGui::CalcTextSize(text, text + textData.m_textLength);
      ImGui::SetWindowFontScale(1.0f);

      // Generate a pixel offset based on text flags.
      Im3d::Vec2 textOffset = Im3d::Vec2(
          -textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
      if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0) {
        textOffset.x = -textSize.x;
      } else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0) {
        textOffset.x = 0.0f;
      }

      if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0) {
        textOffset.y = -textSize.y;
      } else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0) {
        textOffset.y = 0.0f;
      }
      ImFont *font = nullptr;
      // Add text to the window draw list.
      screen = screen + textOffset;
      ImVec2 windowPos = ImGui::GetWindowPos();
      ImVec2 imguiScreen{windowPos.x + screen.x, windowPos.y + screen.y};
      imDrawList->AddText(
          font, (float)(textData.m_positionSize.w * ImGui::GetFontSize()),
          imguiScreen, textData.m_color.getABGR(), text,
          text + textData.m_textLength);
    }
  }
}
} // namespace vku