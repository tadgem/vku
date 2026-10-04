cmake_minimum_required(VERSION 3.22)
project(vku-all)

set(CMAKE_CXX_STANDARD 23)

set(SDL_STATIC TRUE CACHE INTERNAL TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)
set(SDL3_DISABLE_SDL3MAIN ON CACHE BOOL "Disable SDL Entrypoint")
set(SDL3_DISABLE_INSTALL ON CACHE BOOL "Disable SDL Install")
set(SDL3_DISABLE_UNINSTALL ON CACHE BOOL "Disable SDL Uninstall")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable ASSIMP dynamic linking")

option(VKU_ENABLE_SLANG "Pull in the Slang shader compiler runtime" OFF)
option(VKU_RT_SHADER_COMPILATION "Enable Runtime Shader Compilation" TRUE)
option(VKU_BUILD_EXAMPLES "Build vku examples" ON)

find_package(Vulkan REQUIRED)

add_subdirectory(submodules/glm)
add_subdirectory(submodules/assimp)
add_subdirectory(submodules/sdl)
add_subdirectory(submodules/volk)

if (VKU_RT_SHADER_COMPILATION)
    set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(SHADERC_ENABLE_SHARED_CRT ON CACHE BOOL "" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)

    # Add the vku-managed copies of the shaderc dependencies first. shaderc's
    # third_party/CMakeLists.txt skips adding its own copy when the targets
    # (SPIRV-Tools / glslang) already exist.
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
    add_subdirectory(submodules/SPIRV-headers)
    add_subdirectory(submodules/SPIRV-tools)
    add_subdirectory(submodules/glslang)
    add_subdirectory(submodules/shaderc)
endif ()

if (VKU_ENABLE_SLANG)
    # Embed only the Slang compiler runtime, not its tools, tests or examples.
    set(SLANG_LIB_TYPE STATIC CACHE STRING "" FORCE)
    set(SLANG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_GFX OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANGC OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_REPLAYER OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANG_RHI OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_SLANG_GLSLANG OFF CACHE BOOL "" FORCE)
    set(SLANG_ENABLE_DXIL OFF CACHE BOOL "" FORCE)
    set(SLANG_SLANG_LLVM_FLAVOR DISABLE CACHE STRING "" FORCE)

    add_subdirectory(submodules/slang)
endif ()

set(ASSIMP_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/assimp/include CACHE INTERNAL "")
set(GLM_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/glm CACHE INTERNAL "")
set(SDL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/sdl/include CACHE INTERNAL "")
set(SHADERC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/shaderc/libshaderc/include CACHE INTERNAL "")
set(SHADERC_UTIL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/shaderc/libshaderc_util/include CACHE INTERNAL "")

set_target_properties(glm PROPERTIES FOLDER "ThirdParty/glm")
set_target_properties(SDL3_test PROPERTIES FOLDER "ThirdParty/SDL")
set_target_properties(SDL3-static PROPERTIES FOLDER "ThirdParty/SDL")

add_subdirectory(vku)
add_subdirectory(backends/sdl)

if (VKU_BUILD_EXAMPLES)
    add_subdirectory(examples/msaa)
    add_subdirectory(examples/nanovg)
    add_subdirectory(examples/lights-forward)
    add_subdirectory(examples/im3d-multiview)
    add_subdirectory(examples/compute)
    add_subdirectory(examples/dynamic-rendering)
endif ()
