#include "vkh_internal.h"

namespace
{
    bool EqualsInsensitiveA(const char* lhs, const char* rhs)
    {
        return lhs && rhs && lstrcmpiA(lhs, rhs) == 0;
    }
}

namespace VkhHookInternal
{
    PFN_vkVoidFunction InterceptInstanceProcName(const char* procName)
    {
        if (!procName)
        {
            return nullptr;
        }

        if (EqualsInsensitiveA(procName, "vkGetInstanceProcAddr"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkGetInstanceProcAddr);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceProcAddr"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkGetDeviceProcAddr);
        }

        if (EqualsInsensitiveA(procName, "vkCreateWin32SurfaceKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkCreateWin32SurfaceKHR);
        }

        if (EqualsInsensitiveA(procName, "vkCreateDevice"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkCreateDevice);
        }

        if (EqualsInsensitiveA(procName, "vkDestroySurfaceKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkDestroySurfaceKHR);
        }

        return InterceptDeviceProcName(procName);
    }

    PFN_vkVoidFunction InterceptDeviceProcName(const char* procName)
    {
        if (!procName)
        {
            return nullptr;
        }

        if (EqualsInsensitiveA(procName, "vkCreateSwapchainKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkCreateSwapchainKHR);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceQueue"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkGetDeviceQueue);
        }

        if (EqualsInsensitiveA(procName, "vkGetDeviceQueue2"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkGetDeviceQueue2);
        }

        if (EqualsInsensitiveA(procName, "vkDestroySwapchainKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkDestroySwapchainKHR);
        }

        if (EqualsInsensitiveA(procName, "vkGetSwapchainImagesKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkGetSwapchainImagesKHR);
        }

        if (EqualsInsensitiveA(procName, "vkAcquireNextImageKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkAcquireNextImageKHR);
        }

        if (EqualsInsensitiveA(procName, "vkAcquireNextImage2KHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkAcquireNextImage2KHR);
        }

        if (EqualsInsensitiveA(procName, "vkQueuePresentKHR"))
        {
            return reinterpret_cast<PFN_vkVoidFunction>(&HookVkQueuePresentKHR);
        }

        return nullptr;
    }
}
