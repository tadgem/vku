#include "ThirdParty/nanovg.h"
#include "ImGui/imgui_impl_sdl3.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "SDL3/SDL_vulkan.h"
#include "VkSDL.h"
#include "vku/Init.h"
#include "vku/Submission.h"
#include "vku/Log.h"
#include "volk.h"
#include <filesystem>

vku::VkSDL::~VkSDL()
{
}

void vku::VkSDL::HandleSDLEvent(VkState& vk, SDL_Event& sdl_event)
{
    if (sdl_event.type == SDL_EVENT_QUIT)
    {
        vk.m_ShouldRun = false;
    }

    if (sdl_event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        init::RecreateSwapChain(vk);
    }
    if (sdl_event.type == SDL_EVENT_WINDOW_MAXIMIZED) {
        init::RecreateSwapChain(vk);
    }

}

vku::Vector<const char*> vku::VkSDL::GetRequiredInstanceExtensions(VkState& vk)
{
    uint32_t extensionCount = 0;

    const char* const * extensionNamesC =
        SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if(extensionCount == 0)
    {
        VKU_LOG_ERR("Failed to enumerate required SDL device extensions");
        return Vector<const char*>(*vk.m_CPUAllocator);
    }
    STLAllocator<const char*> alloc(*vk.m_CPUAllocator);
    Vector<const char*> extensionNames(alloc);

    for(uint32_t i = 0; i < extensionCount; i++)
    {
        extensionNames.push_back(extensionNamesC[i]);
    }
    if (vk.m_UseValidation)
    {
        extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensionNames;
}

void vku::VkSDL::CreateSurface(VkState& vk)
{
    // todo: do we want to provide an alloc callback to SDL?
    const VkAllocationCallbacks* alloc_callback = nullptr;
    if (!SDL_Vulkan_CreateSurface(
            m_SdlHandle->m_SdlWindow, vk.m_Instance,alloc_callback, &vk.m_Surface))
    {
        VKU_LOG_ERR("Failed to create SDL Vulkan surface");
        std::cerr << "Failed to create SDL Vulkan surface";
    }
}

void vku::VkSDL::CleanupWindow(VkState& vk)
{
    VulkanAPIWindowHandle_SDL* derived = static_cast<VulkanAPIWindowHandle_SDL*>(vk.m_WindowHandle);

    if (derived == nullptr)
    {
        VKU_LOG_ERR("Failed to cast Window Handle to SDL WindowHandle");
        return;
    }
    SDL_DestroyWindow(m_SdlHandle->m_SdlWindow);
    delete m_SdlHandle;
    m_SdlHandle = nullptr;
    SDL_Quit();
}

bool vku::VkSDL::ShouldRun(VkState& vk)
{
    return vk.m_ShouldRun;
}

void vku::VkSDL::PreFrame(VkState& vk)
{
    uint64_t currentFrame = SDL_GetPerformanceCounter();
    vk.m_DeltaTime = (currentFrame - vk.m_LastFrameTime) / (double)SDL_GetPerformanceFrequency();
    vk.m_LastFrameTime = currentFrame;
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        HandleSDLEvent(vk, sdl_event);
        ImGui_ImplSDL3_ProcessEvent(&sdl_event);
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void vku::VkSDL::PostFrame(VkState& vk)
{
    ImGui::EndFrame();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    submission::SubmitFrame(vk);

    if (vkDeviceWaitIdle(vk.m_LogicalDevice) != VK_SUCCESS)
    {
        VKU_LOG_ERR("Failed to wait for device idle");
        std::cerr << "Failed to wait for device idle" << std::endl;
    }

    init::ClearCommandBuffers(vk);
}

void vku::VkSDL::InitImGuiBackend(VkState& vk)
{
    vk;
    ImGui_ImplSDL3_InitForVulkan(m_SdlHandle->m_SdlWindow);
}

void vku::VkSDL::CleanupImGuiBackend(VkState& vk)
{
    vk;
    ImGui_ImplSDL3_Shutdown();
}

void vku::VkSDL::Run(VkState& vk, std::function<void()> callback)
{
    vk.m_ShouldRun = true;
    while (vk.m_ShouldRun)
    {
        uint64_t currentFrame = SDL_GetPerformanceCounter();
        vk.m_DeltaTime = (currentFrame - vk.m_LastFrameTime) / (double)SDL_GetPerformanceFrequency();
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event))
        {
            HandleSDLEvent(vk, sdl_event);
        }

        callback();

        submission::SubmitFrame(vk);
    }
    if (vkDeviceWaitIdle(vk.m_LogicalDevice) != VK_SUCCESS)
    {
        VKU_LOG_ERR("Failed to wait for device idle");
        std::cerr << "Failed to wait for device idle" << std::endl;
    }
}

VkExtent2D vku::VkSDL::GetSurfaceExtent(VkState& vk, VkSurfaceCapabilitiesKHR surface)
{
    vk;
    surface;
    SDL_DisplayID id = SDL_GetPrimaryDisplay();
    SDL_DisplayMode displayMode = *SDL_GetCurrentDisplayMode(id);

    return VkExtent2D{};
}

    VkExtent2D vku::VkSDL::GetMaxFramebufferResolution(VkState& vk)
{
    vk;
    int numDisplays = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&numDisplays);
    VkExtent2D res{};
    for (auto i = 0; i < numDisplays; i++)
    {
        int mode_count = 0;
        auto* displayModes = SDL_GetFullscreenDisplayModes(ids[i], &mode_count);
        for (int j = 0; j < mode_count; j++)
        {
            SDL_DisplayMode displayMode = *displayModes[j];

            if (res.width < static_cast<uint32_t>(displayMode.w))
            {
                res.width = displayMode.w;
            }

            if (res.height < static_cast<uint32_t>(displayMode.h))
            {
                res.height = displayMode.h;
            }
        }
        SDL_free(displayModes);
    }
    SDL_free(ids);
    return res;
}

vku::StageBinary vku::VkSDL::LoadBinaryFromPath(VkState& vk, const char* path)
{
    size_t numBytes;
    void* addr = SDL_LoadFile(path, &numBytes);
    StageBinary binary(*vk.m_CPUAllocator);
    binary.resize(numBytes);
    memcpy(binary.data(), addr, numBytes);
    SDL_free(addr);
    return binary;
}

vku::String vku::VkSDL::LoadStringFromPath(VkState& vk, const char* path)
{
    size_t numBytes;
    void* addr = SDL_LoadFile(path, &numBytes);
    String str(*vk.m_CPUAllocator);
    str.resize(numBytes);
    memcpy(str.data(), addr, numBytes);
    SDL_free(addr);
    return str;
}

vku::VkSDL::VkSDL(bool enableDebugValidation)
{
    VKU_LOG_INFO("VKU : current working directory : %s : enable validation? %d",
        std::filesystem::current_path().string().c_str(),
        enableDebugValidation ? 1 : 0);
}

vku::VulkanAPIWindowHandle_SDL::VulkanAPIWindowHandle_SDL(SDL_Window* sdlWindow) : m_SdlWindow(sdlWindow)
{
    
}

