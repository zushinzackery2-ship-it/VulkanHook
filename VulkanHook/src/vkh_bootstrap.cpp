#include "vkh_internal.h"

namespace VkhHookInternal
{
    ModuleState g_state = {};

    void ResetRuntimeStateLocked(bool preserveTrackedObjects)
    {
        ResetRuntime(g_state.runtime);
        if (!preserveTrackedObjects)
        {
            g_state.surfaces.clear();
            g_state.swapchains.clear();
            g_state.devices.clear();
            g_state.queues.clear();
        }

        g_state.queuePresentDispatchPatches.clear();
        g_state.queuePresentDispatchFailures.clear();
        g_state.queuePresentDirectPatches.clear();
        g_state.queuePresentSlotIndex = static_cast<SIZE_T>(-1);
        g_state.queuePresentCommand = nullptr;
        g_state.runtimeProbeThreadId.store(0);
        g_state.runtimeProbeRequested = false;
        g_state.runtimeProbeStopRequested = false;
        g_state.runtimeProbeCompleted = false;
        g_state.backendRecognized = preserveTrackedObjects;
        g_state.ready = false;
        g_state.setupCalled = false;
    }

    void MarkBackendRecognizedLocked(const char* reason)
    {
        if (g_state.backendRecognized)
        {
            return;
        }

        g_state.backendRecognized = true;
        VKH_LOG(
            "Backend recognized: reason=%s",
            reason ? reason : "<unknown>");
    }

    void FillDefaultDesc(VkhHookDesc& desc)
    {
        ZeroMemory(&desc, sizeof(desc));
        desc.autoCreateContext = true;
        desc.hookWndProc = true;
        desc.blockInputWhenVisible = true;
        desc.enableDefaultDebugWindow = false;
        desc.warmupFrames = 5;
        desc.shutdownWaitTimeoutMs = 5000;
        desc.toggleVirtualKey = VK_INSERT;
        desc.startVisible = true;
    }

    void ResetRuntime(VkhHookRuntime& runtime)
    {
        ZeroMemory(&runtime, sizeof(runtime));
    }

    void ResetTransientStateLocked()
    {
        ResetRuntimeStateLocked(false);
    }

    void ResolveLoaderFunctionsLocked()
    {
        if (!g_state.realGetProcAddress)
        {
            g_state.realGetProcAddress = reinterpret_cast<GetProcAddressFn>(
                ::GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetProcAddress"));
        }

        HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");
        if (ntdllModule && g_state.realGetProcAddress)
        {
            if (!g_state.ldrRegisterDllNotification)
            {
                g_state.ldrRegisterDllNotification = reinterpret_cast<LdrRegisterDllNotificationFn>(
                    g_state.realGetProcAddress(ntdllModule, "LdrRegisterDllNotification"));
            }

            if (!g_state.ldrUnregisterDllNotification)
            {
                g_state.ldrUnregisterDllNotification = reinterpret_cast<LdrUnregisterDllNotificationFn>(
                    g_state.realGetProcAddress(ntdllModule, "LdrUnregisterDllNotification"));
            }
        }

        g_state.vulkanModule = FindVulkanModuleLocked();
        if (g_state.vulkanModule && g_state.realGetProcAddress)
        {
            g_state.realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                g_state.realGetProcAddress(g_state.vulkanModule, "vkGetInstanceProcAddr"));
            g_state.realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                g_state.realGetProcAddress(g_state.vulkanModule, "vkGetDeviceProcAddr"));
        }
    }

    HMODULE FindVulkanModuleLocked()
    {
        return GetModuleHandleW(L"vulkan-1.dll");
    }

    bool EnsureHookingLocked()
    {
        ResolveLoaderFunctionsLocked();
        if (!g_state.realGetProcAddress)
        {
            return false;
        }

        RegisterDllNotificationsLocked();
        if (!PatchLoadedModulesLocked())
        {
            return false;
        }

        return true;
    }

    void MarkLayerModeActiveLocked()
    {
        g_state.layerModeEnabled = true;
    }

    void RemoveHooksLocked()
    {
        for (auto it = g_state.queuePresentDirectPatches.rbegin();
            it != g_state.queuePresentDirectPatches.rend();
            ++it)
        {
            if (!it->slot)
            {
                continue;
            }

            __try
            {
                DWORD oldProtect = 0;
                if (VirtualProtect(it->slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    *it->slot = it->originalValue;
                    VirtualProtect(it->slot, sizeof(void*), oldProtect, &oldProtect);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        for (auto it = g_state.queuePresentDispatchPatches.rbegin();
            it != g_state.queuePresentDispatchPatches.rend();
            ++it)
        {
            if (!it->slot)
            {
                continue;
            }

            __try
            {
                DWORD oldProtect = 0;
                if (VirtualProtect(it->slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    *it->slot = it->originalValue;
                    VirtualProtect(it->slot, sizeof(void*), oldProtect, &oldProtect);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        for (auto it = g_state.iatPatches.rbegin(); it != g_state.iatPatches.rend(); ++it)
        {
            if (!it->slot)
            {
                continue;
            }

            __try
            {
                DWORD oldProtect = 0;
                if (VirtualProtect(it->slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    *it->slot = it->originalValue;
                    VirtualProtect(it->slot, sizeof(void*), oldProtect, &oldProtect);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        g_state.queuePresentDirectPatches.clear();
        g_state.queuePresentDispatchPatches.clear();
        g_state.queuePresentDispatchFailures.clear();
        g_state.queuePresentSlotIndex = static_cast<SIZE_T>(-1);
        g_state.queuePresentCommand = nullptr;
        g_state.iatPatches.clear();
        UnregisterDllNotificationsLocked();
    }
}

namespace VkhHook
{
    void FillDefaultDesc(VkhHookDesc* desc)
    {
        if (!desc)
        {
            return;
        }

        VkhHookInternal::FillDefaultDesc(*desc);
    }

    bool Init(const VkhHookDesc* desc)
    {
        using namespace VkhHookInternal;

        VKH_LOG("Init entered");
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.installed)
            {
                VKH_LOG("Init called while already installed");
                return true;
            }

            VkhHookInternal::FillDefaultDesc(g_state.desc);
            if (desc)
            {
                g_state.desc = *desc;
            }

            const bool preserveTrackedObjects =
                g_state.layerModeEnabled ||
                !g_state.surfaces.empty() ||
                !g_state.swapchains.empty() ||
                !g_state.devices.empty() ||
                !g_state.queues.empty();

            ResetRuntimeStateLocked(preserveTrackedObjects);
            ResolveLoaderFunctionsLocked();
            g_state.layerModeEnabled = true;
            g_state.installed = true;
        }

        VKH_LOG("Init success: layer-mode only.");
        return true;
    }

    void Shutdown()
    {
        using namespace VkhHookInternal;

        std::thread runtimeProbeThread;
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            StopRuntimeProbeThreadLocked(runtimeProbeThread);
        }

        if (runtimeProbeThread.joinable())
        {
            runtimeProbeThread.join();
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            RemoveHooksLocked();
            ResetTransientStateLocked();
            ZeroMemory(&g_state.desc, sizeof(g_state.desc));
            g_state.vulkanModule = nullptr;
            g_state.realGetProcAddress = nullptr;
            g_state.realVkGetInstanceProcAddr = nullptr;
            g_state.realVkGetDeviceProcAddr = nullptr;
            g_state.ldrRegisterDllNotification = nullptr;
            g_state.ldrUnregisterDllNotification = nullptr;
            g_state.dllNotificationCookie = nullptr;
            g_state.layerModeEnabled = false;
            g_state.installed = false;
        }
    }

    bool IsInstalled()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        return VkhHookInternal::g_state.installed;
    }

    bool IsLayerModeEnabled()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        return VkhHookInternal::g_state.layerModeEnabled;
    }

    bool HasTrackedActivity()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        return VkhHookInternal::g_state.ready ||
            VkhHookInternal::g_state.backendRecognized ||
            VkhHookInternal::g_state.layerModeEnabled ||
            !VkhHookInternal::g_state.surfaces.empty() ||
            !VkhHookInternal::g_state.swapchains.empty() ||
            !VkhHookInternal::g_state.devices.empty() ||
            !VkhHookInternal::g_state.queues.empty();
    }

    bool HasRecognizedBackend()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        return VkhHookInternal::g_state.installed &&
            (VkhHookInternal::g_state.backendRecognized ||
             VkhHookInternal::g_state.ready);
    }

    bool IsReady()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        return VkhHookInternal::g_state.installed && VkhHookInternal::g_state.ready;
    }

    const VkhHookRuntime* GetRuntime()
    {
        std::lock_guard<std::mutex> lock(VkhHookInternal::g_state.mutex);
        if (!VkhHookInternal::g_state.installed)
        {
            return nullptr;
        }

        return &VkhHookInternal::g_state.runtime;
    }
}
