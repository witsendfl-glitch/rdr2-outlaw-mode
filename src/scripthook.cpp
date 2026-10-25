#include "scripthook.h"

namespace sh {

void (*scriptRegister)(HMODULE, ScriptMainFn) = nullptr;
void (*scriptUnregister)(HMODULE) = nullptr;
void (*nativeInit)(uint64_t) = nullptr;
void (*nativePush64)(uint64_t) = nullptr;
uint64_t* (*nativeCall)() = nullptr;
void (*scriptWait)(DWORD) = nullptr;

namespace {

// Script Hook exports C++ (MSVC-mangled) names such as
// "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z". We walk the export table
// and match on the "?name@@" prefix, falling back to a plain C name.
FARPROC FindExport(HMODULE module, const char* name) {
    if (FARPROC plain = GetProcAddress(module, name)) return plain;

    char prefix[128];
    wsprintfA(prefix, "?%s@@", name);
    size_t prefixLen = strlen(prefix);

    auto base = reinterpret_cast<const uint8_t*>(module);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.VirtualAddress == 0) return nullptr;

    auto exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
    auto names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
    auto ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
    auto functions = reinterpret_cast<const DWORD*>(base + exports->AddressOfFunctions);

    for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
        const char* exportName = reinterpret_cast<const char*>(base + names[i]);
        if (strncmp(exportName, prefix, prefixLen) == 0) {
            return reinterpret_cast<FARPROC>(const_cast<uint8_t*>(base + functions[ordinals[i]]));
        }
    }
    return nullptr;
}

template <typename T>
bool Bind(HMODULE module, T& target, const char* name) {
    target = reinterpret_cast<T>(reinterpret_cast<void*>(FindExport(module, name)));
    return target != nullptr;
}

}  // namespace

bool Resolve() {
    HMODULE module = GetModuleHandleA("ScriptHookRDR2.dll");
    // We don't import Script Hook, so it may not be loaded yet.
    if (!module) module = LoadLibraryA("ScriptHookRDR2.dll");
    if (!module) return false;
    return Bind(module, scriptRegister, "scriptRegister") &&
           Bind(module, scriptUnregister, "scriptUnregister") &&
           Bind(module, nativeInit, "nativeInit") &&
           Bind(module, nativePush64, "nativePush64") &&
           Bind(module, nativeCall, "nativeCall") &&
           Bind(module, scriptWait, "scriptWait");
}

}  // namespace sh
