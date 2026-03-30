# VulkanHook

Vulkan 底层 Hook 与 Layer 跟踪库。

## 定位

- 负责 `loader / layer / runtime tracking`
- 暴露 `instance / device / queue / swapchain` 等运行时快照
- 不负责 GUI、录屏、控制器
- 当前已经移除旧的 `VEH / late bootstrap / PageGuard` 路径

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

主要接口：

- `VHK::FillDefaultDesc`
- `VHK::Init`
- `VHK::Shutdown`
- `VHK::IsInstalled`
- `VHK::IsLayerModeEnabled`
- `VHK::HasTrackedActivity`
- `VHK::HasRecognizedBackend`
- `VHK::IsReady`
- `VHK::GetRuntime`

原生公开类型现在以 `Vkh*` 为规范名，例如：

- `VkhHookDesc`
- `VkhHookRuntime`

## 依赖方向

```text
VulkanHook
    ↑
Universal-Render-Hook
    ↑
InterRec
```

`VulkanHook` 自身不依赖上层 GUI 封装或 `Universal-Render-Hook`。

## 适用场景

- Vulkan 后端识别
- Layer 模式 runtime 跟踪
- 给上层录屏/调试/注入框架提供 Vulkan 底层能力

## 许可

MIT，见根目录 `LICENSE`
