#include "vkh_internal.h"

namespace VkhHookInternal
{
    namespace
    {
        LONG g_queuePresentHookLogCount = 0;
        LONG g_queueResolvedLogCount = 0;
        LONG g_getDeviceProcAddrLogCount = 0;

        bool EqualsInsensitiveA(const char* lhs, const char* rhs)
        {
            return lhs && rhs && lstrcmpiA(lhs, rhs) == 0;
        }
    }

    VkResult VKAPI_CALL HookVkCreateWin32SurfaceKHR(
        VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface)
    {
        PFN_vkCreateWin32SurfaceKHR original = ResolveInstanceCommand<PFN_vkCreateWin32SurfaceKHR>(
            instance,
            "vkCreateWin32SurfaceKHR");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(instance, createInfo, allocator, surface);
        if (IsSuccessfulResult(result) && createInfo && surface)
        {
            TrackSurfaceCreated(instance, *surface, createInfo->hwnd);
        }

        return result;
    }

    VkResult VKAPI_CALL HookVkCreateDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkDevice* device)
    {
        PFN_vkCreateDevice original = ResolveLoaderExport<PFN_vkCreateDevice>("vkCreateDevice");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(physicalDevice, createInfo, allocator, device);
        if (IsSuccessfulResult(result) && device && *device)
        {
            TrackDeviceCreated(
                *device,
                physicalDevice,
                ResolveLoaderExport<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties"));
            std::lock_guard<std::mutex> lock(g_state.mutex);
            RequestRuntimeProbeLocked("vkCreateDevice");
        }

        return result;
    }

    void VKAPI_CALL HookVkDestroySurfaceKHR(
        VkInstance instance,
        VkSurfaceKHR surface,
        const VkAllocationCallbacks* allocator)
    {
        PFN_vkDestroySurfaceKHR original = ResolveInstanceCommand<PFN_vkDestroySurfaceKHR>(
            instance,
            "vkDestroySurfaceKHR");
        if (original)
        {
            original(instance, surface, allocator);
        }

        TrackSurfaceDestroyed(surface);
    }

    void VKAPI_CALL HookVkGetDeviceQueue(
        VkDevice device,
        std::uint32_t queueFamilyIndex,
        std::uint32_t queueIndex,
        VkQueue* queue)
    {
        PFN_vkGetDeviceQueue original = ResolveDeviceCommand<PFN_vkGetDeviceQueue>(device, "vkGetDeviceQueue");
        if (!original)
        {
            return;
        }

        original(device, queueFamilyIndex, queueIndex, queue);
        if (queue && *queue)
        {
            TrackQueueResolved(*queue, device, queueFamilyIndex);
            std::lock_guard<std::mutex> lock(g_state.mutex);
            RequestRuntimeProbeLocked("vkGetDeviceQueue");
            if (InterlockedIncrement(&g_queueResolvedLogCount) <= 16)
            {
                VKH_LOG(
                    "HookVkGetDeviceQueue queue=%p device=%p family=%u index=%u",
                    *queue,
                    device,
                    queueFamilyIndex,
                    queueIndex);
            }
        }
    }

    void VKAPI_CALL HookVkGetDeviceQueue2(
        VkDevice device,
        const VkDeviceQueueInfo2* queueInfo,
        VkQueue* queue)
    {
        PFN_vkGetDeviceQueue2 original = ResolveDeviceCommand<PFN_vkGetDeviceQueue2>(device, "vkGetDeviceQueue2");
        if (!original)
        {
            return;
        }

        original(device, queueInfo, queue);
        if (queue && *queue && queueInfo)
        {
            TrackQueueResolved(*queue, device, queueInfo->queueFamilyIndex);
            std::lock_guard<std::mutex> lock(g_state.mutex);
            RequestRuntimeProbeLocked("vkGetDeviceQueue2");
            if (InterlockedIncrement(&g_queueResolvedLogCount) <= 16)
            {
                VKH_LOG(
                    "HookVkGetDeviceQueue2 queue=%p device=%p family=%u index=%u",
                    *queue,
                    device,
                    queueInfo->queueFamilyIndex,
                    queueInfo->queueIndex);
            }
        }
    }

    VkResult VKAPI_CALL HookVkCreateSwapchainKHR(
        VkDevice device,
        const VkSwapchainCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSwapchainKHR* swapchain)
    {
        PFN_vkCreateSwapchainKHR original = ResolveDeviceCommand<PFN_vkCreateSwapchainKHR>(
            device,
            "vkCreateSwapchainKHR");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(device, createInfo, allocator, swapchain);
        if (IsSuccessfulResult(result) && createInfo && swapchain)
        {
            TrackSwapchainCreated(
                device,
                createInfo->imageFormat,
                *swapchain,
                createInfo->surface,
                createInfo->minImageCount,
                static_cast<float>(createInfo->imageExtent.width),
                static_cast<float>(createInfo->imageExtent.height));
        }

        return result;
    }

    void VKAPI_CALL HookVkDestroySwapchainKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        const VkAllocationCallbacks* allocator)
    {
        PFN_vkDestroySwapchainKHR original = ResolveDeviceCommand<PFN_vkDestroySwapchainKHR>(
            device,
            "vkDestroySwapchainKHR");
        if (original)
        {
            original(device, swapchain, allocator);
        }

        TrackSwapchainDestroyed(swapchain);
    }

    VkResult VKAPI_CALL HookVkGetSwapchainImagesKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint32_t* swapchainImageCount,
        void* swapchainImages)
    {
        PFN_vkGetSwapchainImagesKHR original = ResolveDeviceCommand<PFN_vkGetSwapchainImagesKHR>(
            device,
            "vkGetSwapchainImagesKHR");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(device, swapchain, swapchainImageCount, swapchainImages);
        if (IsSuccessfulResult(result) && swapchainImageCount)
        {
            TrackSwapchainImageCount(swapchain, *swapchainImageCount);
        }

        return result;
    }

    VkResult VKAPI_CALL HookVkAcquireNextImageKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint64_t timeout,
        VkSemaphore semaphore,
        VkFence fence,
        std::uint32_t* imageIndex)
    {
        PFN_vkAcquireNextImageKHR original = ResolveDeviceCommand<PFN_vkAcquireNextImageKHR>(
            device,
            "vkAcquireNextImageKHR");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(device, swapchain, timeout, semaphore, fence, imageIndex);
        if (IsSuccessfulResult(result) && imageIndex)
        {
            TrackAcquireImage(swapchain, *imageIndex, device);
        }

        return result;
    }

    VkResult VKAPI_CALL HookVkAcquireNextImage2KHR(
        VkDevice device,
        const VkAcquireNextImageInfoKHR* acquireInfo,
        std::uint32_t* imageIndex)
    {
        PFN_vkAcquireNextImage2KHR original = ResolveDeviceCommand<PFN_vkAcquireNextImage2KHR>(
            device,
            "vkAcquireNextImage2KHR");
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = original(device, acquireInfo, imageIndex);
        if (IsSuccessfulResult(result) && acquireInfo && imageIndex)
        {
            TrackAcquireImage(acquireInfo->swapchain, *imageIndex, device);
        }

        return result;
    }

    VkResult VKAPI_CALL HookVkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* presentInfo)
    {
        PFN_vkQueuePresentKHR original = ResolveQueuePresentCommand(queue, presentInfo);
        if (!original)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (presentInfo && presentInfo->pSwapchains)
        {
            if (InterlockedIncrement(&g_queuePresentHookLogCount) <= 12)
            {
                VKH_LOG(
                    "HookVkQueuePresentKHR queue=%p swapchainCount=%u imageIndex0=%u",
                    queue,
                    presentInfo->swapchainCount,
                    presentInfo->pImageIndices ? presentInfo->pImageIndices[0] : 0);
            }

            ObservePresentHit(queue, presentInfo);
        }

        return original(queue, presentInfo);
    }

    PFN_vkVoidFunction VKAPI_CALL HookVkGetInstanceProcAddr(VkInstance instance, const char* procName)
    {
        if (!procName)
        {
            return nullptr;
        }

        if (PFN_vkVoidFunction replacement = InterceptInstanceProcName(procName))
        {
            return replacement;
        }

        PFN_vkGetInstanceProcAddr realVkGetInstanceProcAddr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.realVkGetInstanceProcAddr && g_state.vulkanModule && g_state.realGetProcAddress)
            {
                g_state.realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    g_state.realGetProcAddress(g_state.vulkanModule, "vkGetInstanceProcAddr"));
            }

            realVkGetInstanceProcAddr = g_state.realVkGetInstanceProcAddr;
        }

        PFN_vkVoidFunction result = realVkGetInstanceProcAddr ? realVkGetInstanceProcAddr(instance, procName) : nullptr;
        return result;
    }

    PFN_vkVoidFunction VKAPI_CALL HookVkGetDeviceProcAddr(VkDevice device, const char* procName)
    {
        if (!procName)
        {
            return nullptr;
        }

        if (PFN_vkVoidFunction replacement = InterceptDeviceProcName(procName))
        {
            if (EqualsInsensitiveA(procName, "vkQueuePresentKHR") &&
                InterlockedIncrement(&g_getDeviceProcAddrLogCount) <= 12)
            {
                VKH_LOG(
                    "HookVkGetDeviceProcAddr intercepted vkQueuePresentKHR device=%p replacement=%p",
                    device,
                    replacement);
            }
            return replacement;
        }

        PFN_vkGetDeviceProcAddr realVkGetDeviceProcAddr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.realVkGetDeviceProcAddr && g_state.vulkanModule && g_state.realGetProcAddress)
            {
                g_state.realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    g_state.realGetProcAddress(g_state.vulkanModule, "vkGetDeviceProcAddr"));
            }

            realVkGetDeviceProcAddr = g_state.realVkGetDeviceProcAddr;
        }

        PFN_vkVoidFunction result = realVkGetDeviceProcAddr ? realVkGetDeviceProcAddr(device, procName) : nullptr;
        if (EqualsInsensitiveA(procName, "vkQueuePresentKHR") &&
            InterlockedIncrement(&g_getDeviceProcAddrLogCount) <= 12)
        {
            VKH_LOG(
                "HookVkGetDeviceProcAddr pass-through vkQueuePresentKHR device=%p result=%p",
                device,
                result);
        }

        return result;
    }
}
