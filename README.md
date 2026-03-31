<div align="center">

# VulkanHook

**Vulkan Implicit Layer 运行时跟踪与回调注入库**

*Layer 导出 | Dispatch 桥接 | Swapchain / Queue / Device 跟踪 | Present 回调*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![API](https://img.shields.io/badge/API-Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!NOTE]
> **仓库边界**  
> 本仓库只负责 Vulkan Layer 导出、Dispatch 桥接、运行时跟踪和回调派发，不包含 GUI、录屏或控制器。

> [!IMPORTANT]
> **Layer 模式**  
> 当前唯一正式路径为 Implicit Layer。`HasTrackedActivity()` / `HasRecognizedBackend()` 仅反映当前进程是否存在 Vulkan 活动，不代表额外注入。

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **Implicit Layer** | 通过 `vkNegotiateLoaderLayerInterfaceVersion` 协商版本，向 Loader 暴露本层 |
| **Dispatch 桥接** | `vkCreateInstance` / `vkCreateDevice` 期间保存下层 gipa/gdpa 并构建 Dispatch 表 |
| **Surface 跟踪** | 记录 `VkSurfaceKHR → HWND → VkInstance` |
| **Swapchain 跟踪** | 记录 format / imageCount / extent / 关联 Surface 与 Device |
| **Queue 跟踪** | 记录 `VkQueue → VkDevice → QueueFamilyIndex` |
| **Device 跟踪** | 记录 `VkPhysicalDeviceMemoryProperties` |
| **Present 回调** | `vkQueuePresentKHR` 时组装 `VkhHookRuntime` 并触发 `onSetup` / `onRender` |
| **Late Attach** | Layer 已生效但早期元数据未完整时，在 acquire/present 路径补建记录 |
| **Runtime Probe** | 后台观察 vulkan-1.dll，输出 tracked / recognized / ready 状态 |

---

## 架构

```
应用程序
  vkCreateInstance / vkCreateDevice / vkQueuePresentKHR / ...
    │
    ▼
Vulkan Loader
  枚举 Implicit Layer
    │
    ▼
VK_LAYER_PVRC_capture
  ├─ vkNegotiateLoaderLayerInterfaceVersion
  ├─ vkGetInstanceProcAddr / vkGetDeviceProcAddr
  ├─ InstanceDispatch / DeviceDispatch
  └─ Tracking
       ├─ Surface / Queue / Device / Swapchain
       └─ Present → VkhHookRuntime → onSetup / onRender
```

---

## 跟踪流程

```
vkCreateInstance
    ├─ 获取下层 GetInstanceProcAddr
    ├─ 构建 InstanceDispatch
    └─ 注册 Surface/Instance 拦截

vkCreateDevice
    ├─ 获取下层 GetDeviceProcAddr
    ├─ 构建 DeviceDispatch
    ├─ 读取 PhysicalDeviceMemoryProperties
    └─ 注册 Queue/Swapchain 拦截

vkCreateWin32SurfaceKHR    → 记录 Surface → HWND → Instance
vkCreateSwapchainKHR       → 记录 Swapchain 元数据
vkGetDeviceQueue[2]        → 记录 Queue → Device → QueueFamilyIndex
vkAcquireNextImageKHR      → 补建 Swapchain 条目，更新 imageIndex
vkQueuePresentKHR          → 组装 Runtime，warmup 后触发回调
```

---

## Late Attach 语义

当前支持：
- Layer 已在调用链中，但早期 Swapchain 元数据未完整跟踪
- 在 `vkAcquireNextImageKHR` / `vkQueuePresentKHR` 路径补建缺失条目

不支持：
- Layer 未加载后再晚注入同一 Vulkan 调用链

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `VHK::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `VHK::Init(desc)` | 注册回调，开始跟踪 |
|  | `VHK::Shutdown()` | 清理状态 |
| **状态** | `VHK::IsInstalled()` | 是否已安装 |
|  | `VHK::IsLayerModeEnabled()` | 是否以 Layer 方式生效 |
|  | `VHK::HasTrackedActivity()` | 是否观察到 Vulkan 活动 |
|  | `VHK::HasRecognizedBackend()` | 是否识别到有效后端 |
|  | `VHK::IsReady()` | Runtime 是否可用 |
| **运行时** | `VHK::GetRuntime()` | 获取 `VkhHookRuntime` 快照 |

---

## 目录结构

```
VulkanHook/
└── VulkanHook/
    ├── include/vkh/
    │   ├── vkh.h            # 入口
    │   ├── hook.h           # Hook API
    │   ├── types.h          # 类型定义
    │   └── vkh_minimal.h    # 最小 Vulkan 类型
    └── src/
        ├── vkh_bootstrap.cpp
        ├── vkh_runtime_probe.cpp
        ├── vkh_tracking.cpp
        ├── vkh_layer_*.cpp   # Layer 导出与状态
        └── vkh_internal.h
```

---

## 集成

源码仓库，不含 GUI / 录屏器 / Controller / 测试载荷。

- 直接接入 `VkhHook` 作为 Vulkan 运行时跟踪库
- 或通过 `Universal-Render-Hook` 纳入统一后端仲裁

## 依赖方向

```
VulkanHook  (无外部依赖)
```

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
