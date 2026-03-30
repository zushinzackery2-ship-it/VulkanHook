#include "vkh_internal.h"

namespace
{
    struct UnicodeString
    {
        USHORT length;
        USHORT maximumLength;
        PWSTR buffer;
    };

    struct LdrLoadedNotificationData
    {
        ULONG flags;
        const UnicodeString* fullDllName;
        const UnicodeString* baseDllName;
        void* dllBase;
        ULONG sizeOfImage;
    };

    struct LdrUnloadedNotificationData
    {
        ULONG flags;
        const UnicodeString* fullDllName;
        const UnicodeString* baseDllName;
        void* dllBase;
        ULONG sizeOfImage;
    };

    union LdrDllNotificationData
    {
        LdrLoadedNotificationData loaded;
        LdrUnloadedNotificationData unloaded;
    };

    bool EqualsInsensitiveA(const char* lhs, const char* rhs)
    {
        return lhs && rhs && lstrcmpiA(lhs, rhs) == 0;
    }

    void* ResolveReplacementByName(const char* procName)
    {
        using namespace VkhHookInternal;

        if (EqualsInsensitiveA(procName, "vkGetInstanceProcAddr"))
        {
            return reinterpret_cast<void*>(&HookVkGetInstanceProcAddr);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceProcAddr"))
        {
            return reinterpret_cast<void*>(&HookVkGetDeviceProcAddr);
        }

        if (EqualsInsensitiveA(procName, "vkCreateWin32SurfaceKHR"))
        {
            return reinterpret_cast<void*>(&HookVkCreateWin32SurfaceKHR);
        }

        if (EqualsInsensitiveA(procName, "vkCreateDevice"))
        {
            return reinterpret_cast<void*>(&HookVkCreateDevice);
        }

        if (EqualsInsensitiveA(procName, "vkDestroySurfaceKHR"))
        {
            return reinterpret_cast<void*>(&HookVkDestroySurfaceKHR);
        }

        if (EqualsInsensitiveA(procName, "vkCreateSwapchainKHR"))
        {
            return reinterpret_cast<void*>(&HookVkCreateSwapchainKHR);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceQueue"))
        {
            return reinterpret_cast<void*>(&HookVkGetDeviceQueue);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceQueue2"))
        {
            return reinterpret_cast<void*>(&HookVkGetDeviceQueue2);
        }

        if (EqualsInsensitiveA(procName, "vkDestroySwapchainKHR"))
        {
            return reinterpret_cast<void*>(&HookVkDestroySwapchainKHR);
        }

        if (EqualsInsensitiveA(procName, "vkGetSwapchainImagesKHR"))
        {
            return reinterpret_cast<void*>(&HookVkGetSwapchainImagesKHR);
        }

        if (EqualsInsensitiveA(procName, "vkAcquireNextImageKHR"))
        {
            return reinterpret_cast<void*>(&HookVkAcquireNextImageKHR);
        }

        if (EqualsInsensitiveA(procName, "vkAcquireNextImage2KHR"))
        {
            return reinterpret_cast<void*>(&HookVkAcquireNextImage2KHR);
        }

        if (EqualsInsensitiveA(procName, "vkQueuePresentKHR"))
        {
            return reinterpret_cast<void*>(&HookVkQueuePresentKHR);
        }

        return nullptr;
    }

    void CALLBACK OnDllNotification(ULONG notificationReason, const void* notificationData, void*)
    {
        if (notificationReason != 1 || !notificationData)
        {
            return;
        }

        const auto* data = static_cast<const LdrDllNotificationData*>(notificationData);
        HMODULE moduleHandle = static_cast<HMODULE>(data->loaded.dllBase);
        if (!moduleHandle)
        {
            return;
        }

        VkhHookInternal::ProcessDllNotificationLoaded(moduleHandle);
    }
}

namespace VkhHookInternal
{
    void ProcessDllNotificationLoaded(HMODULE moduleHandle)
    {
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            if (GetModuleHandleW(L"vulkan-1.dll") == moduleHandle)
            {
                g_state.vulkanModule = moduleHandle;
                if (g_state.realGetProcAddress)
                {
                    g_state.realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        g_state.realGetProcAddress(moduleHandle, "vkGetInstanceProcAddr"));
                    g_state.realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                        g_state.realGetProcAddress(moduleHandle, "vkGetDeviceProcAddr"));
                }
            }

            PatchModuleImportsLocked(moduleHandle);
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.vulkanModule && !g_state.layerModeEnabled)
            {
                RequestRuntimeProbeLocked("dll-load");
            }
        }
    }
    bool RegisterDllNotificationsLocked()
    {
        if (!g_state.ldrRegisterDllNotification || g_state.dllNotificationCookie)
        {
            return true;
        }

        return g_state.ldrRegisterDllNotification(0, OnDllNotification, nullptr, &g_state.dllNotificationCookie) >= 0;
    }

    void UnregisterDllNotificationsLocked()
    {
        if (!g_state.ldrUnregisterDllNotification || !g_state.dllNotificationCookie)
        {
            return;
        }

        g_state.ldrUnregisterDllNotification(g_state.dllNotificationCookie);
        g_state.dllNotificationCookie = nullptr;
    }

    FARPROC WINAPI HookGetProcAddress(HMODULE moduleHandle, LPCSTR procName)
    {
        GetProcAddressFn realGetProcAddress = nullptr;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            realGetProcAddress = g_state.realGetProcAddress;
        }

        if (!realGetProcAddress || !moduleHandle || !procName || reinterpret_cast<ULONG_PTR>(procName) <= 0xFFFF)
        {
            return realGetProcAddress ? realGetProcAddress(moduleHandle, procName) : nullptr;
        }

        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH) > 0)
        {
            const wchar_t* moduleName = PathFindFileNameW(modulePath);
            if (moduleName && _wcsicmp(moduleName, L"vulkan-1.dll") == 0)
            {
                if (EqualsInsensitiveA(procName, "vkGetInstanceProcAddr"))
                {
                    return reinterpret_cast<FARPROC>(&HookVkGetInstanceProcAddr);
                }

                if (EqualsInsensitiveA(procName, "vkGetDeviceProcAddr"))
                {
                    return reinterpret_cast<FARPROC>(&HookVkGetDeviceProcAddr);
                }

                void* replacement = ResolveReplacementByName(procName);
                if (replacement)
                {
                    return reinterpret_cast<FARPROC>(replacement);
                }
            }
        }

        return realGetProcAddress(moduleHandle, procName);
    }
}
