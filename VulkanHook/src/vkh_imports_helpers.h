#pragma once

#include "vkh_internal.h"

#include <string>

namespace VkhImportHelpers
{
    inline HMODULE GetCurrentModuleHandle()
    {
        HMODULE moduleHandle = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetCurrentModuleHandle),
            &moduleHandle);
        return moduleHandle;
    }

    inline bool EqualsInsensitiveA(const char* lhs, const char* rhs)
    {
        return lhs && rhs && lstrcmpiA(lhs, rhs) == 0;
    }

    inline bool ContainsInsensitiveA(const char* value, const char* token)
    {
        if (!value || !token)
        {
            return false;
        }

        std::string lowerValue = value;
        std::string lowerToken = token;
        CharLowerBuffA(lowerValue.data(), static_cast<DWORD>(lowerValue.size()));
        CharLowerBuffA(lowerToken.data(), static_cast<DWORD>(lowerToken.size()));
        return lowerValue.find(lowerToken) != std::string::npos;
    }

    inline bool IsKernelLoaderImportName(const char* importDllName)
    {
        return EqualsInsensitiveA(importDllName, "kernel32.dll") ||
            EqualsInsensitiveA(importDllName, "kernelbase.dll") ||
            ContainsInsensitiveA(importDllName, "api-ms-win-core-libraryloader");
    }

    inline bool IsVulkanImportName(const char* importDllName)
    {
        return EqualsInsensitiveA(importDllName, "vulkan-1.dll");
    }

    inline void* ResolveReplacementByName(const char* procName)
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

    inline bool PatchThunkSlotLocked(HMODULE moduleHandle, void** slot, void* replacement)
    {
        using namespace VkhHookInternal;

        if (!slot || !replacement)
        {
            return false;
        }

        for (const auto& patch : g_state.iatPatches)
        {
            if (patch.slot == slot)
            {
                if (*slot == replacement)
                {
                    return true;
                }

                DWORD oldProtect = 0;
                if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    return false;
                }

                *slot = replacement;
                VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
                return true;
            }
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        IatPatchRecord record = {};
        record.moduleHandle = moduleHandle;
        record.slot = slot;
        record.originalValue = *slot;
        *slot = replacement;
        VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
        g_state.iatPatches.push_back(record);
        return true;
    }
}
