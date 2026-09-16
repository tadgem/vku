#pragma once
#include "SDL3/SDL.h"
#include "vku/Structs.h"
#undef main // why is this a thing SDL?!
namespace vku {
	class VulkanAPIWindowHandle_SDL : public VulkanAPIWindowHandle
	{
	public:
		VulkanAPIWindowHandle_SDL(SDL_Window* sdlWindow);
		SDL_Window* m_SdlWindow;
	};

	class VkSDL : public VkBackend {
	public:
          VkSDL(bool enableDebugValidation = true);
          virtual ~VkSDL();
          void 							HandleSDLEvent(VkState& vk, SDL_Event& sdl_event);
          // Inherited via VulkanAPI
          virtual Vector<const char*>
          GetRequiredInstanceExtensions(VkState& vk) override;
          virtual void 			CreateSurface(VkState& vk) override;
          virtual void 			CreateWindowVKU(VkState& vk, uint32_t width, uint32_t height) override
          {
                  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
                  SDL_Window* window = SDL_CreateWindow(vk.m_AppName.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
                  m_SdlHandle = new VulkanAPIWindowHandle_SDL(window);
                  vk.m_WindowHandle = m_SdlHandle;
          }
          virtual void 			CleanupWindow(VkState& vk) override;
          virtual void 			Run(VkState& vk, std::function<void()> callback) override;
          virtual VkExtent2D	GetSurfaceExtent(VkState& vk, VkSurfaceCapabilitiesKHR surface) override;
          virtual VkExtent2D    GetMaxFramebufferResolution(VkState& vk) override;
          virtual bool			ShouldRun(VkState& vk) override;
          virtual void 			PreFrame(VkState& vk) override;
          virtual void 			PostFrame(VkState& vk) override;
          virtual void          InitImGuiBackend(VkState& vk) override;
          virtual void          CleanupImGuiBackend(VkState& vk) override;
          virtual StageBinary   LoadBinaryFromPath(VkState& vk, const char* path) override;
          virtual String        LoadStringFromPath(VkState& vk, const char* path) override;

          VulkanAPIWindowHandle_SDL* m_SdlHandle;
	};
}