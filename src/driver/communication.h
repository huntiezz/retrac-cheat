#pragma once
#include "KernelInterface.h"

inline uintptr_t Baseadress;
inline uintptr_t UnpackedBase = 0;

inline uintptr_t GlobalImageBase() {
	return UnpackedBase ? UnpackedBase : Baseadress;
}

namespace Memory {
    inline int& process_id = Process.ProcessId;

    inline bool IsValid(uintptr_t Addr) {
        return Process.IsValid(Addr);
    }
    
    inline bool ReadVirtual(void* Addr, void* Buffer, DWORD Size) {
        return Process.Read((uintptr_t)Addr, Buffer, Size);
    }

    inline bool Attach() {
        return Process.Initialize();
    }

    inline uintptr_t ModuleBase() {
        uintptr_t base = Process.GetBase(process_id);
        Process.Base = base;
        return base;
    }
}

template <typename T>
T Read(uintptr_t address) {
    if (!address) return T{};
    T tmp{};
    (void)Memory::Process.ReadUnchecked(address, &tmp, static_cast<DWORD>(sizeof(T)));
    return tmp;
}

template <typename T>
bool ReadOk(uintptr_t address, T& out) {
    if (!address) {
        out = T{};
        return false;
    }
    if (Memory::Process.ReadRequestOk(address, out))
        return true;
    if (Memory::Process.ReadRequestOk(address, out))
        return true;
    return Memory::Process.ReadUnchecked(address, &out, static_cast<DWORD>(sizeof(T)));
}

template <typename T>
bool Write(uintptr_t address, T value) {
    if (!address) return false;
    return Memory::Process.WriteRequest<T>(address, value);
}

template<typename T>
T read_chain(uintptr_t address, std::vector<uintptr_t> chain) {
    uintptr_t current = address;
    for (int i = 0; i < chain.size() - 1; i++) {
        current = Read<uintptr_t>(current + chain[i]);
    }
    return Read<T>(current + chain[chain.size() - 1]);
}

std::string read_wstr(uintptr_t address) {
    wchar_t buffer[64] = { 0 };
    if (!address) return "";
    Memory::Process.Read(address, buffer, 64 * sizeof(wchar_t));
    std::wstring ws(buffer);
    return std::string(ws.begin(), ws.end());
}
