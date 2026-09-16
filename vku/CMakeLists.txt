cmake_minimum_required(VERSION 3.14)
project(vku)
set(CMAKE_CXX_STANDARD 23)
option(VKU_RT_SHADER_COMPILATION "Enable Runtime Shader Compilation" TRUE)

set(VKU_SRCS
        src/vku/Texture.cpp
        src/vku/Framebuffer.cpp
        src/vku/Mesh.cpp
        src/vku/Material.cpp
        src/vku/Shader.cpp
        src/vku/Structs.cpp
        src/vku/DescriptorSetAllocator.cpp
        src/vku/Pipeline.cpp
        src/vku/Buffer.cpp
        src/vku/Commands.cpp
        src/vku/Utils.cpp
        src/vku/Init.cpp
        src/vku/Descriptor.cpp
        src/vku/RenderPass.cpp
        src/vku/Submission.cpp
        src/vku/Debug.cpp
        src/ThirdParty/spirv_reflect.c
        src/ThirdParty/nanovg.c
        src/ImGui/imgui_impl_vulkan.cpp
        src/ImGui/imgui_draw.cpp
        src/ImGui/imgui_tables.cpp
        src/ImGui/imgui_widgets.cpp
        src/ImGui/imgui.cpp
        src/Im3D/im3d.cpp
        src/Im3D/im3d_vku.cpp
        include/vku/vku.h
        include/vku/Texture.h
        include/vku/Framebuffer.h
        include/vku/Mesh.h
        include/vku/Material.h
        include/vku/Shader.h
        include/vku/Macros.h
        include/vku/Log.h
        include/vku/DescriptorSetAllocator.h
        include/vku/Pipeline.h
        include/vku/Structs.h
        include/vku/Buffer.h
        include/vku/Commands.h
        include/vku/Utils.h
        include/vku/Init.h
        include/vku/Descriptor.h
        include/vku/RenderPass.h
        include/vku/Submission.h
        include/vku/Defaults.h
        include/vku/Debug.h
        include/vku/Alias.h
        include/vku/Allocator.h
        include/ThirdParty/FunnelSansTTF.h
        include/ThirdParty/spirv_reflect.h
        include/ThirdParty/VulkanMemoryAllocator.h
        include/ThirdParty/stb_image.h
        include/Im3D/im3d.h
        include/Im3D/im3d_math.h
        include/Im3D/im3d_config.h
        include/Im3D/im3d_vku.h
        include/ImGui/imgui_impl_vulkan.h
        include/ImGui/vku_extensions.h
        include/ImGui/imconfig.h
        include/ImGui/imgui_internal.h
        include/ImGui/imgui.h
        include/ImGui/imstb_rectpack.h
        include/ImGui/imstb_textedit.h
        include/ImGui/imstb_truetype.h
)

set(VKU_COMPILE_DEFS "/DIMGUI_IMPL_VULKAN_USE_VOLK")

if (VKU_RT_SHADER_COMPILATION)
    set(VKU_COMPILE_DEFS ${VKU_COMPILE_DEFS} "/DVKU_RT_SHADER_COMPILATION")
endif ()

if (VKU_ENABLE_SLANG)
    set(VKU_COMPILE_DEFS ${VKU_COMPILE_DEFS} "/DVKU_ENABLE_SLANG")
endif ()

add_library(${PROJECT_NAME}
        ${VKU_SRCS}
)

set(VKU_INCLUDES ${Vulkan_INCLUDE_DIRS}
        ${GLM_INCLUDES}
        ${SHADERC_INCLUDES}
        ${SHADERC_UTIL_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        CACHE INTERNAL "")

target_include_directories(${PROJECT_NAME} PUBLIC ${VKU_INCLUDES})

if (MSVC)
    # temporarily remove MP to make it easier to diagnose where unique copies are coming from
    # set(${VKU_COMPILE_DEFS} ${VKU_COMPILE_DEFS} "/bigobj" "/MP" "/Zi" "/fsanitize=address")
    set(${VKU_COMPILE_DEFS} ${VKU_COMPILE_DEFS} "/bigobj" "/Zi" "/fsanitize=address")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,Release>:ProgramDatabase>")
endif ()

target_compile_options(${PROJECT_NAME} INTERFACE ${VKU_COMPILE_DEFS})
target_link_options(${PROJECT_NAME} PUBLIC "/fsanitize=address")

target_link_libraries(${PROJECT_NAME} volk shaderc shaderc_util)

if (VKU_ENABLE_SLANG)
    target_link_libraries(${PROJECT_NAME} slang)
endif ()
