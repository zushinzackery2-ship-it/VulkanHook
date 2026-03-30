#pragma once

#include "types.h"

namespace VkhHook
{
    void FillDefaultDesc(VkhHookDesc* desc);
    bool Init(const VkhHookDesc* desc);
    void Shutdown();
    bool IsInstalled();
    bool IsLayerModeEnabled();
    bool HasTrackedActivity();
    bool HasRecognizedBackend();
    bool IsReady();
    const VkhHookRuntime* GetRuntime();
}
