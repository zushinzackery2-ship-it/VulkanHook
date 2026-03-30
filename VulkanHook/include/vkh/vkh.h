#pragma once

#include "hook.h"
#include "types.h"
#include "vkh_minimal.h"

namespace VHK
{
    using Runtime = VkhHookRuntime;
    using Desc = VkhHookDesc;
    using SetupCallback = VkhHookSetupCallback;
    using RenderCallback = VkhHookRenderCallback;
    using VisibleCallback = VkhHookVisibleCallback;
    using ShutdownCallback = VkhHookShutdownCallback;

    inline void FillDefaultDesc(Desc* desc)
    {
        VkhHook::FillDefaultDesc(desc);
    }

    inline bool Init(const Desc* desc)
    {
        return VkhHook::Init(desc);
    }

    inline void Shutdown()
    {
        VkhHook::Shutdown();
    }

    inline bool IsInstalled()
    {
        return VkhHook::IsInstalled();
    }

    inline bool IsLayerModeEnabled()
    {
        return VkhHook::IsLayerModeEnabled();
    }

    inline bool HasTrackedActivity()
    {
        return VkhHook::HasTrackedActivity();
    }

    inline bool HasRecognizedBackend()
    {
        return VkhHook::HasRecognizedBackend();
    }

    inline bool IsReady()
    {
        return VkhHook::IsReady();
    }

    inline const Runtime* GetRuntime()
    {
        return VkhHook::GetRuntime();
    }
}
