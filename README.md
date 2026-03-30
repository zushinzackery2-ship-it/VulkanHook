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
> 本仓库只负责 Vulkan Layer 导出、dispatch 搭桥、runtime 跟踪和回调派发。  
> 它不负责 GUI、录屏、控制器，也不再包含非 Layer Vulkan 回退路径。

> [!NOTE]
> **Layer 说明**  
> 当前正式路径只有 `Implicit Layer`。  
> `Runtime Probe`、`HasTrackedActivity()`、`HasRecognizedBackend()` 这些状态只反映“当前进程里有没有观察到 Vulkan 活动”，不代表本库在做额外注入。

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **Implicit Layer 注入** | 通过 `vkNegotiateLoaderLayerInterfaceVersion` 协商接口版本，并向 Loader 暴露本层导出 |
| **Dispatch Table 拦截** | 在 `vkCreateInstance` / `vkCreateDevice` 期间保存下层 `gipa / gdpa` 并构建 dispatch 表 |
| **Surface 跟踪** | 记录 `VkSurfaceKHR -> HWND -> VkInstance` 映射 |
| **Swapchain 跟踪** | 记录 `VkSwapchainKHR` 的 format、imageCount、extent、关联 surface / device |
| **Queue 跟踪** | 记录 `VkQueue -> VkDevice -> QueueFamilyIndex` |
| **Device 跟踪** | 记录 `VkPhysicalDeviceMemoryProperties`，供上层挑选 readback memory type |
| **Present 回调** | 在 `vkQueuePresentKHR` 路径组装 `VkhHookRuntime` 并触发 `onSetup / onRender` |
| **Late Attach 补全** | 当 Layer 已生效但早期 swapchain 元数据未完整记录时，可在 acquire / present 路径补建记录 |
| **Runtime Probe** | 后台观察 `vulkan-1.dll` 与关键过程，输出 tracked / recognized / ready 状态 |

---

## 架构

```text
应用程序
  vkCreateInstance / vkCreateDevice / vkQueuePresentKHR / ...
    |
    v
Vulkan Loader
  枚举 Implicit Layer
    |
    v
VK_LAYER_PVRC_capture
  ├─ vkNegotiateLoaderLayerInterfaceVersion
  ├─ vkGetInstanceProcAddr / vkGetDeviceProcAddr
  ├─ InstanceDispatch
  ├─ DeviceDispatch
  └─ Tracking
       ├─ Surface
       ├─ Queue
       ├─ Device
       ├─ Swapchain
       └─ Present runtime snapshot
             |
             v
         用户回调 onSetup / onRender
```

---

## 跟踪流程

```text
vkCreateInstance
    │
    ├─ 获取下层 GetInstanceProcAddr
    ├─ 构建 InstanceDispatch
    └─ 注册 surface / instance 相关拦截

vkCreateDevice
    │
    ├─ 获取下层 GetDeviceProcAddr
    ├─ 构建 DeviceDispatch
    ├─ 读取 physical device memory properties
    └─ 注册 queue / swapchain 相关拦截

vkCreateWin32SurfaceKHR
    └─ 记录 Surface -> HWND -> Instance

vkCreateSwapchainKHR
    └─ 记录 Swapchain -> Surface -> Device + format / extent / imageCount

vkGetDeviceQueue / vkGetDeviceQueue2
    └─ 记录 Queue -> Device -> QueueFamilyIndex

vkAcquireNextImageKHR
    └─ 在必要时补建 swapchain 条目并更新 imageIndex

vkQueuePresentKHR
    │
    ├─ 组合完整 runtime snapshot
    ├─ warmupFrames 后首次调用 onSetup
    └─ 每帧调用 onRender
```

---

## 当前 Late Attach 语义

当前代码支持的是：

- Layer 已经在调用链里
- 但 swapchain 早期元数据没有完整跟踪到
- 之后在 `vkAcquireNextImageKHR` / `vkQueuePresentKHR` 路径里补建缺失条目

这不等于：

- Layer 完全没加载
- 之后还能把自己晚注入进同一条 Vulkan 调用链

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `VHK::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `VHK::Init(desc)` | 注册回调并开始 runtime 跟踪 |
|  | `VHK::Shutdown()` | 清理状态 |
| **状态查询** | `VHK::IsInstalled()` | 是否已安装 |
|  | `VHK::IsLayerModeEnabled()` | 当前是否以 Layer 方式生效 |
|  | `VHK::HasTrackedActivity()` | 是否观察到 Vulkan 活动 |
|  | `VHK::HasRecognizedBackend()` | 是否识别到有效后端 |
|  | `VHK::IsReady()` | runtime 是否完整可用于上层 |
| **运行时** | `VHK::GetRuntime()` | 获取当前 `VkhHookRuntime` 快照 |

---

## 目录结构

```text
VulkanHook/
└── VulkanHook/
    ├── include/vkh/
    │   ├── vkh.h
    │   ├── hook.h
    │   ├── types.h
    │   └── vkh_minimal.h
    └── src/
        ├── vkh_bootstrap.cpp
        ├── vkh_runtime_probe.cpp
        ├── vkh_tracking.cpp
        ├── vkh_layer_exports.cpp
        ├── vkh_layer_instance.cpp
        ├── vkh_layer_runtime.cpp
        ├── vkh_layer_state.cpp
        ├── vkh_layer_internal.h
        ├── vkh_internal.h
        └── vkh_vk_minimal.h
```

---

## 集成

这是源码仓库，不跟踪仓库内 GUI、录屏器、controller、打包脚本或测试载荷。

- 可以直接把 `VkhHook` 作为 Vulkan runtime 跟踪库接入
- 也可以通过 `Universal-Render-Hook` 把 Vulkan 候选纳入统一后端仲裁

---

## 依赖方向

```text
VulkanHook
```

`VulkanHook` 自身不依赖 `Universal-Render-Hook`、`RainGui` 或 `InterRec`。

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
