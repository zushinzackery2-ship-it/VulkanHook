#pragma once

#include <Windows.h>

struct VkhHookRuntime;

using VkhHookSetupCallback = void (*)(
    const VkhHookRuntime* runtime,
    void* userData);

using VkhHookRenderCallback = void (*)(
    const VkhHookRuntime* runtime,
    void* userData);

using VkhHookVisibleCallback = bool (*)(
    void* userData);

using VkhHookShutdownCallback = void (*)(
    void* userData);

struct VkhHookRuntime
{
    void* hwnd;
    void* instance;
    void* physicalDevice;
    void* device;
    void* queue;
    void* swapchain;
    UINT queueFamilyIndex;
    UINT swapchainFormat;
    UINT memoryTypeCount;
    UINT memoryTypeFlags[32];
    UINT imageCount;
    UINT imageIndex;
    float width;
    float height;
    UINT frameCount;
};

struct VkhHookDesc
{
    VkhHookSetupCallback onSetup;
    VkhHookRenderCallback onRender;
    VkhHookVisibleCallback isVisible;
    VkhHookShutdownCallback onShutdown;
    void* userData;

    bool autoCreateContext;
    bool hookWndProc;
    bool blockInputWhenVisible;
    bool enableDefaultDebugWindow;

    UINT warmupFrames;
    UINT shutdownWaitTimeoutMs;
    UINT toggleVirtualKey;
    bool startVisible;
};
