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
)

add_library(${PROJECT_NAME}
        ${VKU_SRCS}
)

set(VKU_INCLUDES
        ${Vulkan_INCLUDE_DIRS}
        ${GLM_INCLUDES}
        ${SHADERC_INCLUDES}
        ${SHADERC_UTIL_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ImGui
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Im3D
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ThirdParty
        CACHE INTERNAL "")

target_include_directories(${PROJECT_NAME} PUBLIC ${VKU_INCLUDES})

target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

if (VKU_RT_SHADER_COMPILATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VKU_RT_SHADER_COMPILATION)
endif ()

if (MSVC)
    # temporarily remove MP to make it easier to diagnose where unique copies are coming from
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /Zi)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,Release>:ProgramDatabase>")
endif ()

target_link_libraries(${PROJECT_NAME} PUBLIC volk)

if (VKU_RT_SHADER_COMPILATION)
    target_link_libraries(${PROJECT_NAME} PUBLIC shaderc shaderc_util)
endif ()
