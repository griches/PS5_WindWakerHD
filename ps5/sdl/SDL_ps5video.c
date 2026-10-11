/*
 * Wind Waker HD for the PlayStation 5 - SDL3's video driver for the console.
 *
 * The console has one output and no windows: this gives SDL one display and
 * lets it keep its window bookkeeping (size, flags, events) for a window that
 * is the whole screen. Nothing is drawn through SDL. Its Vulkan calls answer
 * for the console: the "library" is RADV, linked into the title, and a window's
 * surface is VK_KHR_display's first display in its first mode.
 *
 * SDL_PS5_DISPLAY=WIDTHxHEIGHT in the environment sets the size (default
 * 1920x1080).
 *
 * SPDX-License-Identifier: Zlib
 */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PS5

#include "video/SDL_sysvideo.h"
#include "events/SDL_events_c.h"

#ifdef SDL_VIDEO_VULKAN
#include "video/SDL_vulkan_internal.h"

/* RADV's entry point: there is no Vulkan loader, the driver is in the title */
extern PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);

static bool PS5_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    _this->vulkan_config.vkGetInstanceProcAddr = (void *)vk_icdGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (void *)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!_this->vulkan_config.vkEnumerateInstanceExtensionProperties) {
        return SDL_SetError("RADV has no vkEnumerateInstanceExtensionProperties");
    }
    SDL_strlcpy(_this->vulkan_config.loader_path, "radv", SDL_arraysize(_this->vulkan_config.loader_path));
    return true;
}

static void PS5_Vulkan_UnloadLibrary(SDL_VideoDevice *_this)
{
}

static char const *const *PS5_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count)
{
    static const char *const extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME };
    if (count) {
        *count = SDL_arraysize(extensions);
    }
    return extensions;
}

static bool PS5_Vulkan_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, VkInstance instance,
                                     const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    return SDL_Vulkan_Display_CreateSurface(_this->vulkan_config.vkGetInstanceProcAddr, instance, allocator, surface);
}

static void PS5_Vulkan_DestroySurface(SDL_VideoDevice *_this, VkInstance instance, VkSurfaceKHR surface,
                                      const struct VkAllocationCallbacks *allocator)
{
    SDL_Vulkan_DestroySurface_Internal(_this->vulkan_config.vkGetInstanceProcAddr, instance, surface, allocator);
}
#endif // SDL_VIDEO_VULKAN

static bool PS5_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    int w = 1920, h = 1080;
    const char *size = SDL_getenv("SDL_PS5_DISPLAY");

    if (size && SDL_sscanf(size, "%dx%d", &w, &h) != 2) {
        w = 1920;
        h = 1080;
    }
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.w = w;
    mode.h = h;
    mode.refresh_rate = 60.0f;
    return SDL_AddBasicVideoDisplay(&mode) != 0;
}

static void PS5_VideoQuit(SDL_VideoDevice *_this)
{
}

static void PS5_PumpEvents(SDL_VideoDevice *_this)
{
}

static bool PS5_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    /* The window is the screen, and always has the pad's focus */
    SDL_SetKeyboardFocus(window);
    return true;
}

static bool PS5_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    return true;
}

static void PS5_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* The size stays the display's */
}

static SDL_FullscreenResult PS5_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen)
{
    return SDL_FULLSCREEN_SUCCEEDED;
}

static void PS5_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *PS5_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    device->PumpEvents = PS5_PumpEvents;
    device->CreateSDLWindow = PS5_CreateWindow;
    device->SetWindowPosition = PS5_SetWindowPosition;
    device->SetWindowSize = PS5_SetWindowSize;
    device->SetWindowFullscreen = PS5_SetWindowFullscreen;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = PS5_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = PS5_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = PS5_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = PS5_Vulkan_CreateSurface;
    device->Vulkan_DestroySurface = PS5_Vulkan_DestroySurface;
#endif
    device->free = PS5_DeleteDevice;
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;
    return device;
}

VideoBootStrap PS5_bootstrap = {
    "ps5", "PlayStation 5 video driver",
    PS5_CreateDevice,
    NULL, // no ShowMessageBox implementation
    false
};

#endif // SDL_VIDEO_DRIVER_PS5
