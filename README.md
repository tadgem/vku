# vku

## Vulkan helper library

### Features
- Much higher level API than raw vulkan calls, making experimenting with vulkan more pleasant
- Automated descriptor set creation 
- Material system that allows generation of buffers or assigning existing buffers to descriptor slots per instanced draw / compute invocation
- Backend API to allow multiple windowing systems (currently an SDL impl)
- Custom allocator support (Used internally, optional in dependencies)
- Runtime GLSL shader compilation to SPIR-V (Optional, on by default via
  `VKU_RT_SHADER_COMPILATION`, through the shaderc/glslang submodules). Shaders
  are written in GLSL and can be reloaded from source at runtime.

### To get started, run the following command in the root dir of this repo:
```
- git submod update --init --recursive
- mkdir build & cd build
- cmake ..
```