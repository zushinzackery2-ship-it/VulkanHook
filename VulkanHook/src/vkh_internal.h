#pragma once

#include <Windows.h>
#include <Shlwapi.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../vkh_hook.h"
#include "vkh_internal_logger.h"
#include "vkh_vk_minimal.h"
#define VKH_LOG(...) PvrcInternalLogger::Log(__VA_ARGS__)

namespace VkhHookInternal
{
    typedef FARPROC(WINAPI* GetProcAddressFn)(HMODULE moduleHandle, LPCSTR procName);
    typedef void (CALLBACK* DllNotificationCallbackFn)(ULONG notificationReason, const void* notificationData, void* context);
    typedef LONG (NTAPI* LdrRegisterDllNotificationFn)(ULONG flags, DllNotificationCallbackFn callback, void* context, void** cookie);
    typedef LONG (NTAPI* LdrUnregisterDllNotificationFn)(void* cookie);

    struct SurfaceInfo
    {
        VkInstance instance;
        HWND hwnd;
    };

    struct SwapchainInfo
    {
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        VkSurfaceKHR surface;
        VkQueue lastQueue;
        UINT queueFamilyIndex;
        VkFormat format;
        HWND hwnd;
        UINT imageCount;
        UINT lastImageIndex;
        UINT frameCount;
        float width;
        float height;
        bool lateAttached;
    };

    struct IatPatchRecord
    {
        HMODULE moduleHandle;
        void** slot;
        void* originalValue;
    };

    struct DispatchTablePatchRecord
    {
        void** slot;
        void* originalValue;
    };

    struct DispatchHookFailureRecord
    {
        std::uint64_t queueKey;
        void* target;
    };

    struct DirectPointerPatchRecord
    {
        void** slot;
        void* originalValue;
    };

    struct QueueInfo
    {
        VkDevice device;
        UINT queueFamilyIndex;
    };

    struct DeviceInfo
    {
        VkPhysicalDevice physicalDevice;
        UINT memoryTypeCount;
        VkMemoryPropertyFlags memoryTypeFlags[32];
    };

    struct ModuleState
    {
        VkhHookDesc desc;
        VkhHookRuntime runtime;
        std::mutex mutex;
        std::unordered_map<std::uint64_t, SurfaceInfo> surfaces;
        std::unordered_map<std::uint64_t, SwapchainInfo> swapchains;
        std::unordered_map<std::uint64_t, DeviceInfo> devices;
        std::unordered_map<std::uint64_t, QueueInfo> queues;
        std::vector<IatPatchRecord> iatPatches;
        std::vector<DispatchTablePatchRecord> queuePresentDispatchPatches;
        std::vector<DispatchHookFailureRecord> queuePresentDispatchFailures;
        std::vector<DirectPointerPatchRecord> queuePresentDirectPatches;
        HMODULE vulkanModule;
        GetProcAddressFn realGetProcAddress;
        PFN_vkGetInstanceProcAddr realVkGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr realVkGetDeviceProcAddr;
        LdrRegisterDllNotificationFn ldrRegisterDllNotification;
        LdrUnregisterDllNotificationFn ldrUnregisterDllNotification;
        void* dllNotificationCookie;
        SIZE_T queuePresentSlotIndex;
        PFN_vkQueuePresentKHR queuePresentCommand;
        std::condition_variable runtimeProbeCondition;
        std::thread runtimeProbeThread;
        std::atomic<DWORD> runtimeProbeThreadId;
        bool runtimeProbeRequested;
        bool runtimeProbeStopRequested;
        bool runtimeProbeCompleted;
        bool layerModeEnabled;
        bool backendRecognized;
        bool installed;
        bool ready;
        bool setupCalled;
    };

    extern ModuleState g_state;

    inline std::uint64_t HandleKey(void* handle)
    {
        return static_cast<std::uint64_t>(reinterpret_cast<UINT_PTR>(handle));
    }

    void FillDefaultDesc(VkhHookDesc& desc);
    void ResetRuntime(VkhHookRuntime& runtime);
    void ResetTransientStateLocked();
    void MarkBackendRecognizedLocked(const char* reason);
    bool EnsureHookingLocked();
    void RemoveHooksLocked();
    void EnsureRuntimeProbeThreadLocked();
    void RequestRuntimeProbeLocked(const char* reason);
    void StopRuntimeProbeThreadLocked(std::thread& threadToJoin);
    bool PatchModuleImportsLocked(HMODULE moduleHandle);
    bool PatchLoadedModulesLocked();
    void ProcessDllNotificationLoaded(HMODULE moduleHandle);
    void ResolveLoaderFunctionsLocked();
    HMODULE FindVulkanModuleLocked();
    void MarkLayerModeActiveLocked();
    bool FindQueuePresentDispatchSlotByTargetLocked(VkQueue queue, void* queuePresentTarget, SIZE_T& slotIndex);
    bool DiscoverQueuePresentSlotIndexLocked(
        SIZE_T& slotIndex,
        PFN_vkQueuePresentKHR* resolvedCommand = nullptr);
    bool InstallLiveQueuePresentHooksLocked();
    bool PatchQueuePresentDispatchLocked(VkQueue queue);
    bool RegisterDllNotificationsLocked();
    void UnregisterDllNotificationsLocked();
    PFN_vkQueuePresentKHR ResolvePatchedQueuePresentCommandLocked(VkQueue queue);
    PFN_vkQueuePresentKHR ResolveQueuePresentCommand(VkQueue queue, const VkPresentInfoKHR* presentInfo);
    PFN_vkVoidFunction InterceptInstanceProcName(const char* procName);
    PFN_vkVoidFunction InterceptDeviceProcName(const char* procName);
    void DispatchPresent(VkQueue queue, VkSwapchainKHR swapchain, UINT imageIndex);
    void ObservePresentHit(VkQueue queue, const VkPresentInfoKHR* presentInfo);
    void TrackSurfaceCreated(VkInstance instance, VkSurfaceKHR surface, HWND hwnd);
    void TrackSurfaceDestroyed(VkSurfaceKHR surface);
    void TrackSwapchainCreated(
        VkDevice device,
        VkFormat format,
        VkSwapchainKHR swapchain,
        VkSurfaceKHR surface,
        UINT imageCount,
        float width,
        float height);
    void TrackSwapchainDestroyed(VkSwapchainKHR swapchain);
    void TrackSwapchainImageCount(VkSwapchainKHR swapchain, UINT imageCount);
    void TrackAcquireImage(VkSwapchainKHR swapchain, UINT imageIndex, VkDevice device = nullptr);
    void TrackDeviceCreated(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr);
    void TrackQueueResolved(VkQueue queue, VkDevice device, UINT queueFamilyIndex);
    void TrackLateSwapchainDevice(VkDevice device, VkSwapchainKHR swapchain);
    bool TryBootstrapLateSwapchainLocked(VkQueue queue, VkSwapchainKHR swapchain);

    template <typename T>
    inline T ResolveLoaderExport(const char* procName)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (!g_state.vulkanModule)
        {
            g_state.vulkanModule = FindVulkanModuleLocked();
        }

        if (!g_state.realGetProcAddress || !g_state.vulkanModule)
        {
            return nullptr;
        }

        return reinterpret_cast<T>(g_state.realGetProcAddress(g_state.vulkanModule, procName));
    }

    template <typename T>
    inline T ResolveInstanceCommand(VkInstance instance, const char* procName)
    {
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

        if (realVkGetInstanceProcAddr)
        {
            return reinterpret_cast<T>(realVkGetInstanceProcAddr(instance, procName));
        }

        return ResolveLoaderExport<T>(procName);
    }

    template <typename T>
    inline T ResolveDeviceCommand(VkDevice device, const char* procName)
    {
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

        if (realVkGetDeviceProcAddr)
        {
            return reinterpret_cast<T>(realVkGetDeviceProcAddr(device, procName));
        }

        return ResolveLoaderExport<T>(procName);
    }

    inline bool IsSuccessfulResult(VkResult result)
    {
        return result >= 0;
    }

    VkResult VKAPI_CALL HookVkCreateWin32SurfaceKHR(
        VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface);

    VkResult VKAPI_CALL HookVkCreateDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkDevice* device);

    void VKAPI_CALL HookVkDestroySurfaceKHR(
        VkInstance instance,
        VkSurfaceKHR surface,
        const VkAllocationCallbacks* allocator);

    void VKAPI_CALL HookVkGetDeviceQueue(
        VkDevice device,
        std::uint32_t queueFamilyIndex,
        std::uint32_t queueIndex,
        VkQueue* queue);

    void VKAPI_CALL HookVkGetDeviceQueue2(
        VkDevice device,
        const VkDeviceQueueInfo2* queueInfo,
        VkQueue* queue);

    VkResult VKAPI_CALL HookVkCreateSwapchainKHR(
        VkDevice device,
        const VkSwapchainCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSwapchainKHR* swapchain);

    void VKAPI_CALL HookVkDestroySwapchainKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        const VkAllocationCallbacks* allocator);

    VkResult VKAPI_CALL HookVkGetSwapchainImagesKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint32_t* swapchainImageCount,
        void* swapchainImages);

    VkResult VKAPI_CALL HookVkAcquireNextImageKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint64_t timeout,
        VkSemaphore semaphore,
        VkFence fence,
        std::uint32_t* imageIndex);

    VkResult VKAPI_CALL HookVkAcquireNextImage2KHR(
        VkDevice device,
        const VkAcquireNextImageInfoKHR* acquireInfo,
        std::uint32_t* imageIndex);

    VkResult VKAPI_CALL HookVkQueuePresentKHR(
        VkQueue queue,
        const VkPresentInfoKHR* presentInfo);

    PFN_vkVoidFunction VKAPI_CALL HookVkGetInstanceProcAddr(
        VkInstance instance,
        const char* procName);

    PFN_vkVoidFunction VKAPI_CALL HookVkGetDeviceProcAddr(
        VkDevice device,
        const char* procName);

    FARPROC WINAPI HookGetProcAddress(HMODULE moduleHandle, LPCSTR procName);
}
