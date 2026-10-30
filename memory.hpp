#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

class Memory {
public:
    HANDLE    handle   = NULL;
    DWORD     pID      = 0;
    uintptr_t gameBase = 0;
    uintptr_t monoBase = 0; // Adobe AIR — pas de Mono, gardé pour compatibilité main.cpp

    ~Memory() { if (handle) CloseHandle(handle); }

    bool Attach(const wchar_t* /*procName*/) {
        HWND hwnd = FindWindowW(nullptr, L"Brawlhalla");
        if (!hwnd) return false;
        GetWindowThreadProcessId(hwnd, &pID);
        if (!pID) return false;
        handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
        if (!handle) return false;
        gameBase = GetModuleBase("Brawlhalla.exe");
        monoBase = 0;
        return true;
    }

    uintptr_t GetModuleBase(const char* modName) {
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pID);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32 me{ sizeof(me) };
        if (Module32First(snap, &me)) {
            do {
                std::wstring wm(me.szModule);
                std::string  sm(wm.begin(), wm.end());
                if (!_stricmp(sm.c_str(), modName)) {
                    CloseHandle(snap);
                    return (uintptr_t)me.modBaseAddr;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
        return 0;
    }

    template<typename T>
    T Read(uintptr_t addr) {
        T buf{};
        ReadProcessMemory(handle, (LPCVOID)addr, &buf, sizeof(T), NULL);
        return buf;
    }

    template<typename T>
    void Write(uintptr_t addr, T val) {
        WriteProcessMemory(handle, (LPVOID)addr, &val, sizeof(T), NULL);
    }

    bool WriteBytes(uintptr_t addr, const std::vector<uint8_t>& bytes) {
        return WriteProcessMemory(handle, (LPVOID)addr,
            bytes.data(), bytes.size(), NULL);
    }

    // Adobe AIR string — offset 0x14 = char data (expérimental, FILL si noms faux)
    std::string ReadMonoStr(uintptr_t addr) {
        if (!addr) return "?";
        char buf[128]{};
        ReadProcessMemory(handle, (LPCVOID)(addr + 0x14), buf, 127, nullptr);
        buf[127] = '\0';
        return std::string(buf);
    }

    // AOB scan sur toutes les régions exécutables du process
    uintptr_t FindPatternSafe(const char* pattern, const char* mask) {
        SYSTEM_INFO si; GetSystemInfo(&si);
        uintptr_t cur  = (uintptr_t)si.lpMinimumApplicationAddress;
        uintptr_t end  = (uintptr_t)si.lpMaximumApplicationAddress;
        size_t    plen = strlen(mask);

        MEMORY_BASIC_INFORMATION mbi;
        std::vector<uint8_t> buf;

        while (cur < end) {
            if (!VirtualQueryEx(handle, (LPCVOID)cur, &mbi, sizeof(mbi)))
                { cur += 0x1000; continue; }
            if (mbi.State == MEM_COMMIT &&
               (mbi.Protect == PAGE_EXECUTE_READ ||
                mbi.Protect == PAGE_EXECUTE_READWRITE)) {
                buf.resize(mbi.RegionSize);
                if (ReadProcessMemory(handle, mbi.BaseAddress,
                        buf.data(), mbi.RegionSize, nullptr)) {
                    for (size_t i = 0; i + plen < mbi.RegionSize; i++) {
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

    // Alloue une page exécutable dans la plage jmp rel32 (+/-2GB)
    uintptr_t AllocNear(uintptr_t target, size_t size = 0x1000) {
        for (uintptr_t i = target - 0x70000000;
             i < target + 0x70000000; i += 0x10000) {
            LPVOID p = VirtualAllocEx(handle, (LPVOID)i, size,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (p) return (uintptr_t)p;
        }
        return 0;
    }
};
