cmake_minimum_required(VERSION 3.22)
project(vku-all)

set(CMAKE_CXX_STANDARD 23)

set(SDL_STATIC TRUE CACHE INTERNAL TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)
set(SDL3_DISABLE_SDL3MAIN ON CACHE BOOL "Disable SDL Entrypoint")
set(SDL3_DISABLE_INSTALL ON CACHE BOOL "Disable SDL Install")
set(SDL3_DISABLE_UNINSTALL ON CACHE BOOL "Disable SDL Uninstall")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable ASSIMP dynamic linking")

add_subdirectory(submodules/glm)
add_subdirectory(submodules/assimp)
add_subdirectory(submodules/sdl)
add_subdirectory(submodules/volk)

option(VKU_ENABLE_SLANG "Pull in the Slang shader compiler runtime" ON)

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
    # shaderc already provides the glslang and SPIRV-Tools targets, so skip Slang's copies.
    set(SLANG_ENABLE_SLANG_GLSLANG OFF CACHE BOOL "" FORCE)
    # Vulkan only needs Slang's native SPIR-V backend, so skip the DXIL and
    # LLVM toolchains (which would otherwise be downloaded at configure time).
    set(SLANG_ENABLE_DXIL OFF CACHE BOOL "" FORCE)
    set(SLANG_SLANG_LLVM_FLAVOR DISABLE CACHE STRING "" FORCE)

    add_subdirectory(submodules/slang)
endif ()

set(ASSIMP_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/assimp/include CACHE INTERNAL "")
set(GLM_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/glm CACHE INTERNAL "")
set(SPDLOG_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/spdlog/include CACHE INTERNAL "")
set(SDL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sdl/include CACHE INTERNAL "")
set(CPPTRACE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/cpptrace/include CACHE INTERNAL "")
set(SHADERC_UTIL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/submodules/shaderc/libshaderc_util/include CACHE INTERNAL "")

set_target_properties(glm PROPERTIES FOLDER "ThirdParty/glm")
set_target_properties(SDL3_test PROPERTIES FOLDER "ThirdParty/SDL")
set_target_properties(SDL3-static PROPERTIES FOLDER "ThirdParty/SDL")

add_subdirectory(vku)
add_subdirectory(backends/sdl)
add_subdirectory(examples/msaa)
add_subdirectory(examples/nanovg)
add_subdirectory(examples/lights-forward)
add_subdirectory(examples/im3d-multiview)
add_subdirectory(examples/compute)
add_subdirectory(examples/dynamic-rendering)
