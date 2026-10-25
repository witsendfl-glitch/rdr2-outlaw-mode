// Minimal bridge to Script Hook RDR2.
//
// We resolve Script Hook's exports at runtime instead of linking its import
// library, so the mod builds with any compiler (including MinGW on Linux).
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <type_traits>

typedef int Entity, Ped, Player, Object, Vehicle, Blip;
typedef uint32_t Hash;

#pragma pack(push, 1)
struct Vector3 {
    float x; uint32_t _px;
    float y; uint32_t _py;
    float z; uint32_t _pz;
};
#pragma pack(pop)
static_assert(sizeof(Vector3) == 24, "Vector3 must match the game's layout");

inline Vector3 V3(float x, float y, float z) { return Vector3{x, 0, y, 0, z, 0}; }

namespace sh {

using ScriptMainFn = void (*)();
extern void (*scriptRegister)(HMODULE, ScriptMainFn);
extern void (*scriptUnregister)(HMODULE);
extern void (*nativeInit)(uint64_t);
extern void (*nativePush64)(uint64_t);
extern uint64_t* (*nativeCall)();
extern void (*scriptWait)(DWORD);

// Finds Script Hook's exports. Returns false if Script Hook isn't loaded.
bool Resolve();

template <typename T>
inline void Push(T value) {
    uint64_t raw = 0;
    static_assert(sizeof(T) <= sizeof(raw), "native argument too large");
    std::memcpy(&raw, &value, sizeof(T));
    nativePush64(raw);
}

}  // namespace sh

template <typename R, typename... Args>
inline R invoke(uint64_t hash, Args... args) {
    sh::nativeInit(hash);
    (sh::Push(args), ...);
    uint64_t* result = sh::nativeCall();
    if constexpr (!std::is_void_v<R>) {
        R out;
        std::memcpy(&out, result, sizeof(R));
        return out;
    }
}

inline void WAIT(DWORD ms) { sh::scriptWait(ms); }

// MISC::VAR_STRING is variadic, so it is wrapped by hand.
inline const char* LiteralString(const char* text) {
    return invoke<const char*>(0xFA925AC00EB830B9ULL, 10, "LITERAL_STRING", text);
}
