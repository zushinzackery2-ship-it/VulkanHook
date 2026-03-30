<div align="center">

# VulkanHook

**Vulkan 底层 Hook 与 Layer 运行时跟踪库**

*Loader / Layer / Runtime Tracking | Queue / Swapchain Introspection*

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
> **历史路径说明**  
> 旧的 `VEH / late bootstrap / PageGuard` 主业务路径已经移除。  
> 当前正式路径以 Layer / runtime tracking 为主。

## 目录

```text
VulkanHook/
└── VulkanHook/
    ├── include/vkh/
    ├── src/
    ├── vkh_hook.h
    └── vkh_types.h
```

## 对外接口

头文件入口：

```cpp
#include <vkh/vkh.h>
```

主要能力：

- `VHK::FillDefaultDesc`
- `VHK::Init`
- `VHK::Shutdown`
- `VHK::IsInstalled`
- `VHK::IsLayerModeEnabled`
- `VHK::HasTrackedActivity`
- `VHK::HasRecognizedBackend`
- `VHK::IsReady`
- `VHK::GetRuntime`

原生公开类型统一使用 `Vkh*` 前缀：

- `VkhHookDesc`
- `VkhHookRuntime`

## 依赖方向

```text
VulkanHook
    ↑
Universal-Render-Hook
    ↑
RainGui / InterRec
```

`VulkanHook` 自身不依赖上层 GUI 封装、录制业务或 `Universal-Render-Hook`。

## 许可

MIT，见根目录 `LICENSE`
