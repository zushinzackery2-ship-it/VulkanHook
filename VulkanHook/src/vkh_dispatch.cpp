#include "vkh_internal.h"

namespace
{
    constexpr SIZE_T InvalidDispatchSlotIndex = static_cast<SIZE_T>(-1);
    constexpr SIZE_T MaxProbeDispatchSlots = 128;

    template <typename T>
    T ResolveGlobalCommandLocked(const char* procName)
    {
        using namespace VkhHookInternal;

        if (!g_state.vulkanModule || !g_state.realGetProcAddress)
        {
            return nullptr;
        }

        if (g_state.realVkGetInstanceProcAddr)
        {
            PFN_vkVoidFunction function = g_state.realVkGetInstanceProcAddr(nullptr, procName);
            if (function)
            {
                return reinterpret_cast<T>(function);
            }
        }

        return reinterpret_cast<T>(g_state.realGetProcAddress(g_state.vulkanModule, procName));
    }

    template <typename T>
    T ResolveInstanceCommandLocked(VkInstance instance, const char* procName)
    {
        using namespace VkhHookInternal;

        if (g_state.realVkGetInstanceProcAddr)
        {
            PFN_vkVoidFunction function = g_state.realVkGetInstanceProcAddr(instance, procName);
            if (function)
            {
                return reinterpret_cast<T>(function);
            }
        }

        return reinterpret_cast<T>(ResolveGlobalCommandLocked<PFN_vkVoidFunction>(procName));
    }

    template <typename T>
    T ResolveDeviceCommandLocked(VkDevice device, const char* procName)
    {
        using namespace VkhHookInternal;

        if (g_state.realVkGetDeviceProcAddr)
        {
            PFN_vkVoidFunction function = g_state.realVkGetDeviceProcAddr(device, procName);
            if (function)
            {
                return reinterpret_cast<T>(function);
            }
        }

        return reinterpret_cast<T>(ResolveGlobalCommandLocked<PFN_vkVoidFunction>(procName));
    }

    void** GetDispatchTablePointer(void* dispatchableHandle)
    {
        if (!dispatchableHandle)
        {
            return nullptr;
        }

        __try
        {
            return *reinterpret_cast<void***>(dispatchableHandle);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    bool PatchDispatchSlot(void** slot, void* replacement, void** originalValue)
    {
        if (!slot || !replacement)
        {
            return false;
        }

        if (originalValue)
        {
            *originalValue = *slot;
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

    void RestoreDispatchSlot(void** slot, void* originalValue)
    {
        if (!slot)
        {
            return;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return;
        }

        *slot = originalValue;
        VirtualProtect(slot, sizeof(void*), oldProtect, &oldProtect);
    }

    bool FindProbeQueueFamilyIndex(
        VkPhysicalDevice physicalDevice,
        PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProperties,
        std::uint32_t& queueFamilyIndex)
    {
        if (!physicalDevice || !getQueueFamilyProperties)
        {
            return false;
        }

        std::uint32_t queueFamilyCount = 0;
        getQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0)
        {
            return false;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        getQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            if (queueFamilies[index].queueCount == 0)
            {
                continue;
            }

            if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                queueFamilyIndex = index;
                return true;
            }
        }

        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            if (queueFamilies[index].queueCount == 0)
            {
                continue;
            }

            queueFamilyIndex = index;
            return true;
        }

        return false;
    }

    bool SafeCreateInstance(
        PFN_vkCreateInstance createInstance,
        const VkInstanceCreateInfo* createInfo,
        VkInstance* instance,
        VkResult& result,
        DWORD& exceptionCode)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
        exceptionCode = 0;
        __try
        {
            result = createInstance(createInfo, nullptr, instance);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = GetExceptionCode();
        }

        return false;
    }

    bool SafeCreateDevice(
        PFN_vkCreateDevice createDevice,
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        VkDevice* device,
        VkResult& result,
        DWORD& exceptionCode)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
        exceptionCode = 0;
        __try
        {
            result = createDevice(physicalDevice, createInfo, nullptr, device);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = GetExceptionCode();
        }

        return false;
    }

    bool DiscoverQueuePresentSlotIndexLockedImpl(
        SIZE_T& slotIndex,
        PFN_vkQueuePresentKHR* resolvedCommand)
    {
        using namespace VkhHookInternal;

        VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl begin");
        if (resolvedCommand)
        {
            *resolvedCommand = nullptr;
        }

        PFN_vkCreateInstance createInstance = reinterpret_cast<PFN_vkCreateInstance>(
            g_state.realGetProcAddress(g_state.vulkanModule, "vkCreateInstance"));
        if (!createInstance)
        {
            VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: vkCreateInstance unresolved");
            return false;
        }

        auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            g_state.realGetProcAddress(g_state.vulkanModule, "vkGetInstanceProcAddr"));
        if (!getInstanceProcAddr)
        {
            VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: vkGetInstanceProcAddr unresolved");
            return false;
        }

        const char* deviceExtensions[] =
        {
            "VK_KHR_swapchain"
        };

        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "VkhProbe";
        applicationInfo.pEngineName = "VkhProbe";

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = nullptr;
        VkDevice device = nullptr;
        PFN_vkDestroyInstance destroyInstance = nullptr;
        PFN_vkDestroyDevice destroyDevice = nullptr;
        bool discovered = false;

        do
        {
            VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl calling vkCreateInstance");
            DWORD createInstanceExceptionCode = 0;
            VkResult instanceResult = VK_ERROR_INITIALIZATION_FAILED;
            if (!SafeCreateInstance(
                    createInstance,
                    &instanceCreateInfo,
                    &instance,
                    instanceResult,
                    createInstanceExceptionCode))
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl exception: vkCreateInstance code=0x%08X",
                    createInstanceExceptionCode);
                break;
            }

            if (!IsSuccessfulResult(instanceResult) || !instance)
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl failed: createInstance result=%d instance=%p",
                    instanceResult,
                    instance);
                break;
            }

            destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(getInstanceProcAddr(instance, "vkDestroyInstance"));
            PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices =
                reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(getInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
            PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProperties =
                reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                    getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
            PFN_vkCreateDevice createDevice =
                reinterpret_cast<PFN_vkCreateDevice>(getInstanceProcAddr(instance, "vkCreateDevice"));
            if (!enumeratePhysicalDevices || !getQueueFamilyProperties || !createDevice)
            {
                VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: instance commands unresolved");
                break;
            }

            std::uint32_t physicalDeviceCount = 0;
            VkResult enumerateResult = enumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
            if (!IsSuccessfulResult(enumerateResult) || physicalDeviceCount == 0)
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl failed: enumeratePhysicalDevices result=%d count=%u",
                    enumerateResult,
                    physicalDeviceCount);
                break;
            }

            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            enumerateResult = enumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
            if (!IsSuccessfulResult(enumerateResult) || physicalDevices.empty())
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl failed: enumeratePhysicalDevices(list) result=%d count=%u",
                    enumerateResult,
                    physicalDeviceCount);
                break;
            }

            std::uint32_t queueFamilyIndex = 0;
            if (!FindProbeQueueFamilyIndex(physicalDevices[0], getQueueFamilyProperties, queueFamilyIndex))
            {
                VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: FindProbeQueueFamilyIndex");
                break;
            }

            const float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkDeviceCreateInfo deviceCreateInfo = {};
            deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreateInfo.queueCreateInfoCount = 1;
            deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
            deviceCreateInfo.enabledExtensionCount = 1;
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

            VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl calling vkCreateDevice");
            DWORD createDeviceExceptionCode = 0;
            VkResult deviceResult = VK_ERROR_INITIALIZATION_FAILED;
            if (!SafeCreateDevice(
                    createDevice,
                    physicalDevices[0],
                    &deviceCreateInfo,
                    &device,
                    deviceResult,
                    createDeviceExceptionCode))
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl exception: vkCreateDevice code=0x%08X",
                    createDeviceExceptionCode);
                break;
            }

            if (!IsSuccessfulResult(deviceResult) || !device)
            {
                VKH_LOG(
                    "DiscoverQueuePresentSlotIndexLockedImpl failed: createDevice result=%d device=%p",
                    deviceResult,
                    device);
                break;
            }

            auto getDeviceProcAddr =
                reinterpret_cast<PFN_vkGetDeviceProcAddr>(getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
            destroyDevice = getDeviceProcAddr
                ? reinterpret_cast<PFN_vkDestroyDevice>(getDeviceProcAddr(device, "vkDestroyDevice"))
                : nullptr;
            if (!getDeviceProcAddr)
            {
                VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: vkGetDeviceProcAddr unresolved");
                break;
            }

            auto queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(getDeviceProcAddr(device, "vkQueuePresentKHR"));
            if (!queuePresent)
            {
                VKH_LOG("DiscoverQueuePresentSlotIndexLockedImpl failed: vkQueuePresentKHR unresolved");
                break;
            }

            if (resolvedCommand)
            {
                *resolvedCommand = queuePresent;
            }

            discovered = true;
        }
        while (false);

        if (device && destroyDevice)
        {
            destroyDevice(device, nullptr);
        }

        if (instance && destroyInstance)
        {
            destroyInstance(instance, nullptr);
        }

        VKH_LOG(
            "DiscoverQueuePresentSlotIndexLockedImpl end: discovered=%d slotIndex=%zu",
            discovered ? 1 : 0,
            slotIndex);
        return discovered;
    }

    bool FindQueuePresentDispatchSlotByTargetLockedImpl(VkQueue queue, void* queuePresentTarget, SIZE_T& slotIndex)
    {
        if (!queue || !queuePresentTarget)
        {
            return false;
        }

        void** dispatchTable = GetDispatchTablePointer(reinterpret_cast<void*>(queue));
        if (!dispatchTable)
        {
            return false;
        }

        for (SIZE_T index = 0; index < MaxProbeDispatchSlots; ++index)
        {
            void* entry = dispatchTable[index];
            if (!entry)
            {
                continue;
            }

            if (entry == queuePresentTarget)
            {
                slotIndex = index;
                return true;
            }
        }

        return false;
    }

    bool PatchQueuePresentDispatchLockedImpl(VkQueue queue)
    {
        using namespace VkhHookInternal;

        if (!queue)
        {
            return false;
        }

        PFN_vkQueuePresentKHR queuePresentTarget = g_state.queuePresentCommand
            ? g_state.queuePresentCommand
            : ResolveLoaderExport<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
        if (!queuePresentTarget)
        {
            return false;
        }

        const std::uint64_t queueKey = HandleKey(queue);
        for (const DispatchHookFailureRecord& failureRecord : g_state.queuePresentDispatchFailures)
        {
            if (failureRecord.queueKey == queueKey &&
                failureRecord.target == reinterpret_cast<void*>(queuePresentTarget))
            {
                return false;
            }
        }

        if (g_state.queuePresentSlotIndex == InvalidDispatchSlotIndex)
        {
            SIZE_T slotIndex = InvalidDispatchSlotIndex;
            if (!FindQueuePresentDispatchSlotByTargetLockedImpl(
                    queue,
                    reinterpret_cast<void*>(queuePresentTarget),
                    slotIndex))
            {
                VKH_LOG(
                    "PatchQueuePresentDispatchLocked failed: queue=%p target=%p",
                    queue,
                    queuePresentTarget);
                DispatchHookFailureRecord failureRecord = {};
                failureRecord.queueKey = queueKey;
                failureRecord.target = reinterpret_cast<void*>(queuePresentTarget);
                g_state.queuePresentDispatchFailures.push_back(failureRecord);
                return false;
            }

            g_state.queuePresentSlotIndex = slotIndex;
            VKH_LOG(
                "PatchQueuePresentDispatchLocked resolved slot from live queue: queue=%p slotIndex=%zu target=%p",
                queue,
                slotIndex,
                queuePresentTarget);
        }

        void** dispatchTable = GetDispatchTablePointer(reinterpret_cast<void*>(queue));
        if (!dispatchTable)
        {
            return false;
        }

        void** slot = &dispatchTable[g_state.queuePresentSlotIndex];
        for (const DispatchTablePatchRecord& patchRecord : g_state.queuePresentDispatchPatches)
        {
            if (patchRecord.slot == slot)
            {
                return true;
            }
        }

        DispatchTablePatchRecord patchRecord = {};
        patchRecord.slot = slot;
        if (!PatchDispatchSlot(slot, reinterpret_cast<void*>(&HookVkQueuePresentKHR), &patchRecord.originalValue))
        {
            DispatchHookFailureRecord failureRecord = {};
            failureRecord.queueKey = queueKey;
            failureRecord.target = reinterpret_cast<void*>(queuePresentTarget);
            g_state.queuePresentDispatchFailures.push_back(failureRecord);
            return false;
        }

        g_state.queuePresentDispatchPatches.push_back(patchRecord);
        return true;
    }
}

namespace VkhHookInternal
{
    bool FindQueuePresentDispatchSlotByTargetLocked(VkQueue queue, void* queuePresentTarget, SIZE_T& slotIndex)
    {
        return FindQueuePresentDispatchSlotByTargetLockedImpl(queue, queuePresentTarget, slotIndex);
    }

    bool DiscoverQueuePresentSlotIndexLocked(
        SIZE_T& slotIndex,
        PFN_vkQueuePresentKHR* resolvedCommand)
    {
        return DiscoverQueuePresentSlotIndexLockedImpl(slotIndex, resolvedCommand);
    }

    bool InstallLiveQueuePresentHooksLocked()
    {
        bool patchedAny = false;
        for (const auto& entry : g_state.swapchains)
        {
            const SwapchainInfo& swapchainInfo = entry.second;
            if (!swapchainInfo.lastQueue)
            {
                continue;
            }

            if (PatchQueuePresentDispatchLockedImpl(swapchainInfo.lastQueue))
            {
                patchedAny = true;
            }
        }

        return patchedAny;
    }

    PFN_vkQueuePresentKHR ResolvePatchedQueuePresentCommandLocked(VkQueue queue)
    {
        if (!queue || g_state.queuePresentSlotIndex == InvalidDispatchSlotIndex)
        {
            return nullptr;
        }

        void** dispatchTable = GetDispatchTablePointer(reinterpret_cast<void*>(queue));
        if (!dispatchTable)
        {
            return nullptr;
        }

        void** slot = &dispatchTable[g_state.queuePresentSlotIndex];
        for (const DispatchTablePatchRecord& patchRecord : g_state.queuePresentDispatchPatches)
        {
            if (patchRecord.slot == slot)
            {
                return reinterpret_cast<PFN_vkQueuePresentKHR>(patchRecord.originalValue);
            }
        }

        return nullptr;
    }

    bool PatchQueuePresentDispatchLocked(VkQueue queue)
    {
        return PatchQueuePresentDispatchLockedImpl(queue);
    }

    PFN_vkQueuePresentKHR ResolveQueuePresentCommand(VkQueue queue, const VkPresentInfoKHR*)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);

        PFN_vkQueuePresentKHR command = ResolvePatchedQueuePresentCommandLocked(queue);
        if (!command && !g_state.queuePresentDirectPatches.empty() && g_state.queuePresentCommand)
        {
            return g_state.queuePresentCommand;
        }

        if (!command && queue)
        {
            PatchQueuePresentDispatchLockedImpl(queue);
            command = ResolvePatchedQueuePresentCommandLocked(queue);
        }

        if (command)
        {
            return command;
        }

        if (g_state.queuePresentCommand)
        {
            return g_state.queuePresentCommand;
        }

        if (!g_state.vulkanModule)
        {
            g_state.vulkanModule = FindVulkanModuleLocked();
        }

        if (!g_state.vulkanModule || !g_state.realGetProcAddress)
        {
            return nullptr;
        }

        return reinterpret_cast<PFN_vkQueuePresentKHR>(
            g_state.realGetProcAddress(g_state.vulkanModule, "vkQueuePresentKHR"));
    }
}
