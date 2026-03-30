#include "vkh_internal.h"
#include "vkh_imports_helpers.h"

#include <TlHelp32.h>

namespace VkhHookInternal
{
    bool PatchModuleImportsLocked(HMODULE moduleHandle)
    {
        if (!moduleHandle)
        {
            return false;
        }

        auto* baseAddress = reinterpret_cast<std::uint8_t*>(moduleHandle);
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(baseAddress);
        if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return false;
        }

        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(baseAddress + dosHeader->e_lfanew);
        if (!ntHeaders || ntHeaders->Signature != IMAGE_NT_SIGNATURE)
        {
            return false;
        }

        const auto& importDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!importDirectory.VirtualAddress || !importDirectory.Size)
        {
            return true;
        }

        auto* importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(baseAddress + importDirectory.VirtualAddress);
        for (; importDescriptor->Name; ++importDescriptor)
        {
            const char* importDllName = reinterpret_cast<const char*>(baseAddress + importDescriptor->Name);
            const bool patchGetProcAddress = VkhImportHelpers::IsKernelLoaderImportName(importDllName);
            const bool patchVulkanImports = VkhImportHelpers::IsVulkanImportName(importDllName);
            if (!patchGetProcAddress && !patchVulkanImports)
            {
                continue;
            }

            if (!importDescriptor->OriginalFirstThunk || !importDescriptor->FirstThunk)
            {
                continue;
            }

            auto* originalThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(baseAddress + importDescriptor->OriginalFirstThunk);
            auto* firstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(baseAddress + importDescriptor->FirstThunk);
            for (; originalThunk->u1.AddressOfData && firstThunk->u1.Function; ++originalThunk, ++firstThunk)
            {
                if (IMAGE_SNAP_BY_ORDINAL(originalThunk->u1.Ordinal))
                {
                    continue;
                }

                auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(baseAddress + originalThunk->u1.AddressOfData);
                const char* procName = reinterpret_cast<const char*>(importByName->Name);
                void* replacement = nullptr;
                if (patchGetProcAddress &&
                    VkhImportHelpers::EqualsInsensitiveA(procName, "GetProcAddress"))
                {
                    replacement = reinterpret_cast<void*>(&HookGetProcAddress);
                }
                else if (patchVulkanImports)
                {
                    replacement = VkhImportHelpers::ResolveReplacementByName(procName);
                }

                if (!replacement)
                {
                    continue;
                }

                VkhImportHelpers::PatchThunkSlotLocked(
                    moduleHandle,
                    reinterpret_cast<void**>(&firstThunk->u1.Function),
                    replacement);
            }
        }

        return true;
    }

    bool PatchLoadedModulesLocked()
    {
        const HMODULE currentModule = VkhImportHelpers::GetCurrentModuleHandle();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        MODULEENTRY32W moduleEntry = {};
        moduleEntry.dwSize = sizeof(moduleEntry);
        BOOL hasModule = Module32FirstW(snapshot, &moduleEntry);
        while (hasModule)
        {
            if (moduleEntry.hModule != currentModule && moduleEntry.hModule != g_state.vulkanModule)
            {
                PatchModuleImportsLocked(moduleEntry.hModule);
            }

            hasModule = Module32NextW(snapshot, &moduleEntry);
        }

        CloseHandle(snapshot);
        return true;
    }
}
