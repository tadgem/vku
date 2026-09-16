#pragma once
#include "ImGui/imgui.h"
#include "vku/Texture.h"

namespace ImGuiX
{
    static void Image(vku::Texture& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4 tint = ImVec4(1, 1, 1, 1), const ImVec4 borderCol = ImVec4(0, 0, 0, 0))
    {
        ImGui::Image((ImTextureID)texture.m_ImGuiHandle, size, uv0, uv1, tint, borderCol);
    }

}