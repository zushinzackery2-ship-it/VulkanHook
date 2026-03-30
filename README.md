<div align="center">

# VulkanHook

**Vulkan Layer 运行时跟踪与回调注入库**

*Implicit Layer 链 | vkQueuePresentKHR 拦截 | Swapchain / Queue / Device 全链路跟踪*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![API](https://img.shields.io/badge/API-Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!IMPORTANT]
> **仓库定位**  
> 本仓库只负责 Vulkan 底层切入、Layer 链接和 runtime 跟踪。  
> 不负责 GUI、录屏、控制器，也不反向依赖上层仓库。

> [!NOTE]
> **Layer 模式说明**  
> 本库通过 Vulkan Implicit Layer 机制注入目标进程，无需修改目标程序代码。  
> Layer 在 `vkCreateInstance` / `vkCreateDevice` 时自动激活，拦截关键渲染路径。

---

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **Implicit Layer 注入** | 通过 `vkNegotiateLoaderLayerInterfaceVersion` 协商接口版本，向 Loader 注册 `VK_LAYER_PVRC_capture`，在 Instance / Device 创建时自动插入调用链 |
| **Dispatch Table 拦截** | Layer 创建时从 `VkLayerInstanceCreateInfo` / `VkLayerDeviceCreateInfo` 获取下层 `GetInstanceProcAddr` / `GetDeviceProcAddr`，构建独立的 `InstanceDispatch` / `DeviceDispatch` 分发表 |
| **Surface 跟踪** | 拦截 `vkCreateWin32SurfaceKHR` / `vkDestroySurfaceKHR`，记录 `VkSurfaceKHR → HWND → VkInstance` 映射，为后续 Swapchain 关联窗口句柄 |
| **Swapchain 跟踪** | 拦截 `vkCreateSwapchainKHR` / `vkDestroySwapchainKHR`，记录 Format / ImageCount / Extent，支持 Late Attach（中途注入时自动补全缺失信息） |
| **Queue 跟踪** | 拦截 `vkGetDeviceQueue` / `vkGetDeviceQueue2`，记录 `VkQueue → VkDevice → QueueFamilyIndex` 映射 |
| **Device 跟踪** | 拦截 `vkCreateDevice`，读取 `VkPhysicalDeviceMemoryProperties`，缓存 MemoryType 标志位供上层分配 Staging Buffer |
| **Present 回调** | 拦截 `vkQueuePresentKHR`，在每帧提交前调用用户回调，传递完整的 `VkhHookRuntime` 快照（Device / Queue / Swapchain / ImageIndex / Extent） |
| **Late Attach** | 即使 Layer 在 Swapchain 创建后才激活，也能通过 `vkAcquireNextImageKHR` 和窗口枚举自动补全 Swapchain 信息 |
| **Runtime Probe** | 后台线程轮询 `vulkan-1.dll` 加载状态，检测后端是否已就绪 |
| **线程安全** | 全局状态使用 `std::mutex` 保护，多线程环境下安全调用 |

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `VHK::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `VHK::Init(desc)` | 注册回调并激活 Layer 跟踪 |
|  | `VHK::Shutdown()` | 卸载 Hook，清理状态 |
| **状态查询** | `VHK::IsInstalled()` | 是否已调用 Init |
|  | `VHK::IsLayerModeEnabled()` | 是否作为 Implicit Layer 加载 |
|  | `VHK::HasTrackedActivity()` | 是否检测到 Vulkan 调用 |
|  | `VHK::HasRecognizedBackend()` | 是否识别到有效后端 |
|  | `VHK::IsReady()` | Runtime 是否完整可用 |
| **运行时** | `VHK::GetRuntime()` | 获取当前 `VkhHookRuntime` 快照 |

---

## 回调类型

```cpp
using VkhHookSetupCallback    = void (*)(const VkhHookRuntime* runtime, void* userData);
using VkhHookRenderCallback   = void (*)(const VkhHookRuntime* runtime, void* userData);
using VkhHookVisibleCallback  = bool (*)(void* userData);
using VkhHookShutdownCallback = void (*)(void* userData);
```

**VkhHookRuntime 结构**：

| 字段 | 类型 | 说明 |
|:-----|:-----|:-----|
| `hwnd` | `void*` | 渲染窗口句柄 |
| `instance` | `void*` | `VkInstance` |
| `physicalDevice` | `void*` | `VkPhysicalDevice` |
| `device` | `void*` | `VkDevice` |
| `queue` | `void*` | 当前帧使用的 `VkQueue` |
| `swapchain` | `void*` | `VkSwapchainKHR` |
| `queueFamilyIndex` | `UINT` | Queue Family 索引 |
| `swapchainFormat` | `UINT` | `VkFormat` |
| `memoryTypeCount` | `UINT` | 可用内存类型数量 |
| `memoryTypeFlags[32]` | `UINT[]` | 各内存类型的 PropertyFlags |
| `imageCount` | `UINT` | Swapchain Image 数量 |
| `imageIndex` | `UINT` | 当前帧 Image 索引 |
| `width` / `height` | `float` | 渲染分辨率 |
| `frameCount` | `UINT` | 累计帧数 |

---

## 架构

```
应用程序
  vkCreateInstance / vkCreateDevice / vkQueuePresentKHR ...
    |
    v
Vulkan Loader
  枚举 Implicit Layer → 加载 VK_LAYER_PVRC_capture
    |
    | vkNegotiateLoaderLayerInterfaceVersion
    v
VulkanHook Layer
  +-- InstanceDispatch (gipa / destroyInstance / ...)
  +-- DeviceDispatch (gdpa / destroyDevice / ...)
  +-- Tracking (Surface / Swapchain / Queue)
        |
        | vkQueuePresentKHR 拦截
        v
      DispatchPresent() → 用户回调 onSetup / onRender
    |
    v
ICD (驱动)
```

---

## 跟踪流程

```
vkCreateInstance
    │
    ├─ Layer 获取下层 GetInstanceProcAddr
    ├─ 构建 InstanceDispatch
    └─ 注册 Surface / PhysicalDevice 拦截

vkCreateDevice
    │
    ├─ Layer 获取下层 GetDeviceProcAddr
    ├─ 构建 DeviceDispatch
    ├─ 读取 PhysicalDeviceMemoryProperties
    └─ 注册 Queue / Swapchain 拦截

vkCreateWin32SurfaceKHR
    └─ 记录 Surface → HWND → Instance 映射

vkCreateSwapchainKHR
    │
    ├─ 记录 Swapchain → Surface → Device 映射
    ├─ 缓存 Format / ImageCount / Extent
    └─ 刷新 Runtime 快照

vkGetDeviceQueue / vkGetDeviceQueue2
    └─ 记录 Queue → Device → QueueFamilyIndex 映射

vkAcquireNextImageKHR
    │
    ├─ Late Attach: 若 Swapchain 未跟踪，自动创建条目
    └─ 更新 lastImageIndex

vkQueuePresentKHR
    │
    ├─ 从 Tracking 表组装完整 Runtime
    ├─ warmupFrames 后调用 onSetup（仅一次）
    └─ 每帧调用 onRender
```

---

## 目录结构

```
VulkanHook/
└── VulkanHook/
    ├── include/vkh/
    │   ├── vkh.h                    # 主入口，VHK 命名空间封装
    │   ├── hook.h                   # VkhHook 静态类声明
    │   ├── types.h                  # VkhHookDesc / VkhHookRuntime 定义
    │   └── vkh_minimal.h            # 最小前置声明
    └── src/
        ├── vkh_bootstrap.cpp        # Init / Shutdown / Probe 线程
        ├── vkh_tracking.cpp         # Surface / Swapchain / Queue / Device 跟踪
        ├── vkh_runtime_probe.cpp    # vulkan-1.dll 加载检测
        ├── vkh_layer_exports.cpp    # Layer 导出函数 (vkGetInstanceProcAddr 等)
        ├── vkh_layer_instance.cpp   # vkCreateInstance / vkDestroyInstance 拦截
        ├── vkh_layer_runtime.cpp    # Layer 运行时状态
        ├── vkh_layer_state.cpp      # Layer 全局状态管理
        ├── vkh_layer_internal.h     # Layer 内部结构 (InstanceDispatch / DeviceDispatch)
        ├── vkh_internal.h           # 模块内部状态 (ModuleState / SurfaceInfo / SwapchainInfo)
        ├── vkh_internal_logger.h    # 内部日志
        └── vkh_vk_minimal.h         # Vulkan 类型最小定义（避免依赖 SDK 头）
```

---

## 快速开始

```cpp
#include <vkh/vkh.h>

void OnSetup(const VkhHookRuntime* runtime, void* userData)
{
    // 首次就绪时调用，可在此初始化渲染资源
}

void OnRender(const VkhHookRuntime* runtime, void* userData)
{
    // 每帧 Present 前调用
    // runtime->device / runtime->queue / runtime->swapchain 可用于提交命令
}

int main()
{
    VkhHookDesc desc;
    VHK::FillDefaultDesc(&desc);
    desc.onSetup = OnSetup;
    desc.onRender = OnRender;
    desc.warmupFrames = 3;

    if (!VHK::Init(&desc))
    {
        return 1;
    }

    // ... 等待目标程序渲染 ...

    VHK::Shutdown();
    return 0;
}
```

---

## 依赖方向

```
VulkanHook (本仓库)
    ↑
Universal-Render-Hook (可选上层)
    ↑
RainGui / InterRec (可选上层)
```

`VulkanHook` 自身不依赖上层 GUI 封装、录制业务或 `Universal-Render-Hook`。

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
