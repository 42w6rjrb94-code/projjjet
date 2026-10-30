#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

class Memory {
public:
    HANDLE hProcess = NULL;
    DWORD  pID      = 0;

    ~Memory() { if (hProcess) CloseHandle(hProcess); }

    bool Init(const std::string& windowName, HWND& outHwnd) {
        outHwnd = FindWindowA(NULL, windowName.c_str());
        if (!outHwnd) return false;
        GetWindowThreadProcessId(outHwnd, &pID);
        if (!pID) return false;
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
        return hProcess != NULL;
    }

    template<typename T>
    T Read(uintptr_t addr) {
        T buf{};
        ReadProcessMemory(hProcess, (LPCVOID)addr, &buf, sizeof(T), NULL);
        return buf;
    }

    template<typename T>
    void Write(uintptr_t addr, T val) {
        WriteProcessMemory(hProcess, (LPVOID)addr, &val, sizeof(T), NULL);
    }

    bool WriteBytes(uintptr_t addr, const std::vector<uint8_t>& bytes) {
        return WriteProcessMemory(hProcess, (LPVOID)addr,
            bytes.data(), bytes.size(), NULL);
    }

    // AOB scan sur toutes les régions exécutables
    uintptr_t FindPatternSafe(const char* pattern, const char* mask) {
        SYSTEM_INFO si; GetSystemInfo(&si);
        uintptr_t cur  = (uintptr_t)si.lpMinimumApplicationAddress;
        uintptr_t max  = (uintptr_t)si.lpMaximumApplicationAddress;
        size_t    plen = strlen(mask);

        MEMORY_BASIC_INFORMATION mbi;
        std::vector<uint8_t> buf;

        while (cur < max) {
            if (!VirtualQueryEx(hProcess, (LPCVOID)cur, &mbi, sizeof(mbi)))
                { cur += 0x1000; continue; }

            if (mbi.State == MEM_COMMIT &&
               (mbi.Protect == PAGE_EXECUTE_READ ||
                mbi.Protect == PAGE_EXECUTE_READWRITE)) {
                buf.resize(mbi.RegionSize);
                if (ReadProcessMemory(hProcess, mbi.BaseAddress,
                    buf.data(), mbi.RegionSize, nullptr)) {
                    for (size_t i = 0; i < mbi.RegionSize - plen; i++) {
                        bool ok = true;
                        for (size_t j = 0; j < plen; j++) {
                            if (mask[j] == 'x' &&
                                buf[i+j] != (uint8_t)pattern[j])
                                { ok = false; break; }
                        }
                        if (ok) return (uintptr_t)mbi.BaseAddress + i;
                    }
                }
            }
            cur += mbi.RegionSize;
        }
        return 0;
    }

    // Alloue une page exécutable à portée de jmp relatif 32-bit
    uintptr_t AllocNear(uintptr_t target, size_t size = 0x1000) {
        for (uintptr_t i = target - 0x70000000;
             i < target + 0x70000000; i += 0x10000) {
            auto p = VirtualAllocEx(hProcess, (LPVOID)i, size,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (p) return (uintptr_t)p;
        }
        return 0;
    }
};
