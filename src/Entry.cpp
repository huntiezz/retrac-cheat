#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define CURL_STATICLIB

#include "util/common.h"

#include <iomanip>
#include <random>
#include <regex>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <conio.h>
#include <sstream>
#include <unordered_set>

#include <Uxtheme.h>
#include <dxgi.h>
#include <urlmon.h>
#include <Wininet.h>
#include <ShlObj.h>
#include <lmcons.h>
#include <wincrypt.h>
#include <sddl.h>

#include "menu/menu.h"
#include "Game/Gameloop.h"
#include "driver/communication.h"
#include "driver/DriverConfig.h"
#include "sdk-offsets/sdk.h"
#include "util/obfuscate.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "urlmon.lib")

#include "util/JunkCode.h"
#include "menu/Font_bytes.h"

#define RESET   xorstr_("\033[0m")
#define RED     xorstr_("\033[31m")
#define GREEN   xorstr_("\033[32m")
#define YELLOW  xorstr_("\033[33m")
#define PURPLE  xorstr_("\033[35m")
#define CYAN    xorstr_("\033[36m")

#define SILENT(x) do{freopen("NUL","w",stdout);freopen("NUL","w",stderr);x;freopen("CON","w",stdout);freopen("CON","w",stderr);}while(0)

// for anyone that see's this, the esp is for sure broken, and hella errors in this source to fix. this is a proj you just put through ai and hope for the best. LOVE FROM QRSIS3LETTER


void SetColor(WORD color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

namespace Log {
    inline void Info(const std::string& text) {
        std::cout << CYAN << xorstr_("[*] ") << RESET << text << "\n";
    }
    inline void Success(const std::string& text) {
        std::cout << GREEN << xorstr_("[+] ") << RESET << text << "\n";
    }
    inline void Error(const std::string& text) {
        std::cout << RED << xorstr_("[-] ") << RESET << text << "\n";
    }
    inline void Warn(const std::string& text) {
        std::cout << YELLOW << xorstr_("[!] ") << RESET << text << "\n";
    }
    inline void Input(const std::string& text) {
        std::cout << PURPLE << xorstr_("[>] ") << RESET << text;
    }
}

namespace Utils {
    inline void SleepMs(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    inline void EnableANSI() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    inline void Spinner(const std::string& text, int durationMs) {
        const char frames[] = { '|', '/', '-', '\\' };
        int frame = 0;
        int loops = durationMs / 80;

        for (int i = 0; i < loops; i++) {
            std::cout << xorstr_("\r ") << CYAN << frames[frame++] << RESET << xorstr_(" ") << text << xorstr_("   ");
            frame %= 4;
            SleepMs(80);
        }
        std::cout << "\n";
    }

    inline uint32_t GetPIDByName(const std::wstring& proc_name) {
        PROCESSENTRY32W proc_info{};
        proc_info.dwSize = sizeof(proc_info);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;
        std::uint32_t best_pid = 0;
        std::uint64_t newest_time = 0;
        if (Process32FirstW(snapshot, &proc_info)) {
            do {
                if (!_wcsicmp(proc_info.szExeFile, proc_name.c_str())) {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_info.th32ProcessID);
                    if (hProc) {
                        FILETIME create_time{}, exit_time{}, kernel_time{}, user_time{};
                        if (GetProcessTimes(hProc, &create_time, &exit_time, &kernel_time, &user_time)) {
                            ULARGE_INTEGER t;
                            t.LowPart = create_time.dwLowDateTime;
                            t.HighPart = create_time.dwHighDateTime;
                            if (t.QuadPart > newest_time) {
                                newest_time = t.QuadPart;
                                best_pid = proc_info.th32ProcessID;
                            }
                        }
                        CloseHandle(hProc);
                    }
                }
            } while (Process32NextW(snapshot, &proc_info));
        }
        CloseHandle(snapshot);
        return best_pid;
    }

    inline int WaitForProcess() {
        Log::Info(xorstr_("Waiting For Retrac...."));
        while (true) {
            Memory::process_id = GetPIDByName((L"FortniteClient-Win64-Shipping.exe"));
            if (Memory::process_id) {
                Log::Success(xorstr_("Retrac Found!"));
                Beep(180, 800);
                return true;
            }
            Sleep(500);
        }
    }
    
    inline void WaitForKey(int key) {
        std::cout << "\n\n";
        Log::Input(xorstr_("Press Insert in Lobby...\n"));
        while (true) {
            if (GetAsyncKeyState(key) & 1) {
                Beep(180, 700);
                break;
            }
            Sleep(10);
        }
        while ((GetAsyncKeyState(key) & 0x8000) != 0)
            Sleep(10);
        (void)GetAsyncKeyState(key);
    }
    inline void RandomTitleLoop() {
        while (true) {
            std::string title = xorstr_("");
            static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            for (int i = 0; i < 16; ++i) {
                title += charset[rand() % (sizeof(charset) - 1)];
            }
            SetConsoleTitleA(title.c_str());
            SleepMs(100);
        }
    }

    inline void PrintBanner() {
        std::cout << PURPLE << xorstr_("========== Discord Inc ==========") << RESET << "\n\n";
    }
}

void SetConsoleTransparency(int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    HWND hWnd = GetConsoleWindow();
    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, 0, alpha, LWA_ALPHA);
}

static std::string ResolveDriverSysPath() {
    char modulePath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    const std::filesystem::path besideExe =
        std::filesystem::path(modulePath).parent_path() / "driver.sys";
    if (std::filesystem::exists(besideExe)) {
        return besideExe.string();
    }
    if (std::filesystem::exists(RETRAC_DRIVER_SYS_PATH)) {
        return RETRAC_DRIVER_SYS_PATH;
    }
    return RETRAC_DRIVER_SYS_PATH;
}

void LoadDriver() {
    JUNK_CODE_HEAVY;
    (void)ResolveDriverSysPath;
    Log::Info(xorstr_("process memory"));
}

static void LogOffsetStep(const char* name, uintptr_t addr) {
    const bool valid = addr && Memory::IsValid(addr);
    std::ostringstream line;
    line << name << " 0x" << std::hex << std::uppercase << addr << ' '
         << (valid ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(line.str());
}

static const char* MemoryStateName(DWORD state) {
    switch (state) {
    case MEM_COMMIT:
        return "MEM_COMMIT";
    case MEM_FREE:
        return "MEM_FREE";
    case MEM_RESERVE:
        return "MEM_RESERVE";
    default:
        return "unknown";
    }
}

static void LogGObjectsSlotVirtualQueryOnce(uintptr_t gobjectsSlot) {
    static std::atomic<bool> logged{false};
    if (logged.exchange(true) || !gobjectsSlot || !Memory::Process.Handle)
        return;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQueryEx(Memory::Process.Handle, reinterpret_cast<LPCVOID>(gobjectsSlot), &mbi,
                        sizeof(mbi)))
        return;
    std::ostringstream line;
    line << xorstr_("GObjects slot VirtualQuery State=") << MemoryStateName(mbi.State)
         << xorstr_(" Protect=0x") << std::hex << std::uppercase << mbi.Protect << std::dec
         << xorstr_(" Type=0x") << std::hex << mbi.Type << std::dec;
    Log::Info(line.str());
}

static void LogMainModuleAtBaseOnce() {
    static std::atomic<bool> logged{false};
    if (logged.exchange(true) || !Baseadress || !Memory::process_id)
        return;
    const HANDLE snap =
        CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, Memory::process_id);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Module32FirstW(snap, &entry)) {
        do {
            if (reinterpret_cast<uintptr_t>(entry.modBaseAddr) == Baseadress) {
                found = true;
                char narrow[MAX_PATH]{};
                WideCharToMultiByte(CP_UTF8, 0, entry.szModule, -1, narrow, MAX_PATH, nullptr,
                                    nullptr);
                std::ostringstream line;
                line << xorstr_("Process module @ Baseadress: ") << narrow << xorstr_(" size 0x")
                     << std::hex << std::uppercase << entry.modBaseSize << std::dec;
                Log::Info(line.str());
                break;
            }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    if (!found)
        Log::Warn(xorstr_("Baseadress does not match any loaded module (launcher stub?)"));
}

static void LogGObjectsReadProbeOnce() {
    static std::atomic<bool> logged{false};
    if (logged.exchange(true) || !Baseadress)
        return;

    RunRpmHeaderVsDataProbeOnce(Baseadress);

    auto logProbe = [](const char* label, uintptr_t addr, DWORD size) {
        std::ostringstream line;
        line << xorstr_("Read probe ") << label << xorstr_(" @ 0x") << std::hex << std::uppercase
             << addr << std::dec;
        if (!addr) {
            line << xorstr_(" skip (null)");
            Log::Info(line.str());
            return;
        }
        if (size == 2) {
            uint16_t strict{};
            uint16_t loose{};
            const bool strictOk = Memory::Process.ReadRequestOk(addr, strict);
            const bool looseOk =
                Memory::Process.ReadUnchecked(addr, &loose, sizeof(strict));
            line << xorstr_(" strict=") << (strictOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" loose=") << (looseOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" val=0x") << std::hex << std::uppercase << loose << std::dec;
        } else if (size == 4) {
            int32_t strict{};
            int32_t loose{};
            const bool strictOk = Memory::Process.ReadRequestOk(addr, strict);
            const bool looseOk =
                Memory::Process.ReadUnchecked(addr, &loose, sizeof(loose));
            line << xorstr_(" strict=") << (strictOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" loose=") << (looseOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" val=") << std::dec << loose;
        } else {
            uint64_t strict{};
            uint64_t loose{};
            const bool strictOk = Memory::Process.ReadRequestOk(addr, strict);
            const bool looseOk =
                Memory::Process.ReadUnchecked(addr, &loose, sizeof(loose));
            line << xorstr_(" strict=") << (strictOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" loose=") << (looseOk ? xorstr_("ok") : xorstr_("fail"))
                 << xorstr_(" val=0x") << std::hex << std::uppercase << loose << std::dec;
        }
        Log::Info(line.str());
    };

    logProbe(xorstr_("PE e_magic"), Baseadress, 2);
    const uintptr_t gobjectsSlot = Baseadress + Offsets::GObjects;
    logProbe(xorstr_("GObjects slot"), gobjectsSlot, 8);
    {
        uint64_t strictProbe{};
        if (!Memory::Process.ReadRequestOk(gobjectsSlot, strictProbe))
            LogGObjectsSlotVirtualQueryOnce(gobjectsSlot);
    }
    logProbe(xorstr_("NumElements@slot+0x14"),
             gobjectsSlot + TUObjectArrayLayout::NumElements, 4);
    logProbe(xorstr_("NumElements@slot+0x24"),
             gobjectsSlot + 0x10 + TUObjectArrayLayout::NumElements, 4);
    LogRpmHeaderVsDataProbeSummaryOnce();
}

static void PauseBeforeExit() {
    Log::Warn(xorstr_("Press any key to close"));
    std::cout.flush();
    (void)_getch();
}

static void LogPeField(const char* name, uint32_t value, bool valid) {
    std::ostringstream line;
    line << name << xorstr_(" 0x") << std::hex << std::uppercase << value << ' '
         << (valid ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(line.str());
}

static void LogMeshPoseTArrays(uintptr_t mesh) {
    // xorstr_ hands back a pointer into a temporary that dies with the full-expression, so a
    // label kept in a static array dangles and prints whatever string landed on that stack slot
    // next ("Mesh+0xMesh+0x"). The offset is the label anyway.
    static const uint32_t slots[] = {
        static_cast<uint32_t>(Offsets::BoneCache),
        static_cast<uint32_t>(Offsets::BonePosePad),
        static_cast<uint32_t>(Offsets::BoneArray),
    };
    for (uint32_t slotOffset : slots) {
        FUeTArrayHeader hdr{};
        const bool readOk = ReadUeTArrayHeader(mesh + slotOffset, hdr);
        std::ostringstream line;
        line << xorstr_("Mesh+0x") << std::hex << std::uppercase << slotOffset << std::dec
             << xorstr_(" TArray read ") << (readOk ? xorstr_("ok") : xorstr_("fail"));
        if (readOk) {
            line << xorstr_(" Num ") << std::dec << hdr.Num << xorstr_(" Data 0x") << std::hex
                 << std::uppercase << hdr.Data << std::dec << xorstr_(" Max ") << hdr.Max;
        }
        Log::Info(line.str());
    }
}

// Fail-fast attach check: every field must come back from ReadProcessMemory, not zero-filled failure.
static bool RequireGameModuleReadable() {
    if (!Baseadress) {
        Log::Error(xorstr_("Game module base is null."));
        std::cout.flush();
        return false;
    }

    uint16_t eMagic{};
    if (!Memory::Process.ReadRequestOk(Baseadress, eMagic)) {
        Log::Error(
            xorstr_("Cannot read game module PE header at driver base — process reads failed (driver/RPM dead)."));
        std::cout.flush();
        return false;
    }
    if (eMagic != 0x5A4D) {
        std::ostringstream line;
        line << xorstr_("Game module e_magic invalid (0x") << std::hex << std::uppercase << eMagic
             << std::dec << xorstr_(") — wrong base or unreadable image.");
        Log::Error(line.str());
        std::cout.flush();
        return false;
    }

    int32_t eLfanew{};
    if (!Memory::Process.ReadRequestOk(Baseadress + 0x3C, eLfanew) || eLfanew <= 0 || eLfanew >= 0x1000) {
        Log::Error(xorstr_("Game module PE e_lfanew unreadable or invalid — process reads failed."));
        std::cout.flush();
        return false;
    }

    const uintptr_t nt = Baseadress + static_cast<uintptr_t>(eLfanew);
    uint32_t peSig{};
    if (!Memory::Process.ReadRequestOk(nt, peSig) || peSig != 0x00004550) {
        Log::Error(xorstr_("Game module PE signature unreadable or invalid — process reads failed."));
        std::cout.flush();
        return false;
    }

    uint16_t optMagic{};
    if (!Memory::Process.ReadRequestOk(nt + 0x18, optMagic) || optMagic != 0x20B) {
        Log::Error(xorstr_("Game module optional header unreadable or not PE32+ — process reads failed."));
        std::cout.flush();
        return false;
    }

    Log::Success(xorstr_("Game PE header readable"));
    std::cout.flush();
    return true;
}

// Verbose MZ/PE probe for offset debug (after RequireGameModuleReadable passed).
static bool PeHeaderReadableAt(uintptr_t base, const char* label) {
    if (!base) {
        Log::Warn(std::string(label) + xorstr_(" base is null"));
        return false;
    }
    const uint16_t eMagic = Read<uint16_t>(base);
    const bool magicOk = eMagic == 0x5A4D;
    std::ostringstream magicLine;
    magicLine << label << xorstr_(" PE e_magic 0x") << std::hex << std::uppercase << eMagic
              << std::dec << ' ' << (magicOk ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(magicLine.str());

    const int32_t eLfanew = Read<int32_t>(base + 0x3C);
    const bool lfanewOk = eLfanew > 0 && eLfanew < 0x1000;
    std::ostringstream lfanewLine;
    lfanewLine << label << xorstr_(" PE e_lfanew 0x") << std::hex << std::uppercase << eLfanew
               << std::dec << ' ' << (lfanewOk ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(lfanewLine.str());

    uint32_t peSig = 0;
    uint16_t optMagic = 0;
    if (lfanewOk) {
        const uintptr_t nt = base + static_cast<uintptr_t>(eLfanew);
        peSig = Read<uint32_t>(nt);
        optMagic = Read<uint16_t>(nt + 0x18);
    }
    const bool sigOk = lfanewOk && peSig == 0x00004550;
    const bool optOk = lfanewOk && optMagic == 0x20B;
    std::ostringstream sigLine;
    sigLine << label << xorstr_(" PE signature 0x") << std::hex << std::uppercase << peSig
            << std::dec << ' ' << (sigOk ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(sigLine.str());
    std::ostringstream optLine;
    optLine << label << xorstr_(" PE optional magic 0x") << std::hex << std::uppercase << optMagic
            << std::dec << ' ' << (optOk ? xorstr_("VALID") : xorstr_("INVALID"));
    Log::Info(optLine.str());

    return magicOk && lfanewOk && sigOk && optOk;
}

static void DebugOffsets() {
    (void)PeHeaderReadableAt(Baseadress, xorstr_("module"));

    const char* selfCheckFail = nullptr;
    if (!ProjectionFailsClosedSelfCheck(&selfCheckFail)) {
        std::string msg = xorstr_("W2S fail-closed self-check failed");
        if (selfCheckFail && selfCheckFail[0]) {
            msg += xorstr_(": ");
            msg += selfCheckFail;
        }
        Log::Error(msg);
    }
    const char* frustumTestFail = nullptr;
    if (!RunCameraProjectionFrustumSelfTest(&frustumTestFail)) {
        std::string msg = xorstr_("Camera projection frustum self-test failed");
        if (frustumTestFail && frustumTestFail[0]) {
            msg += xorstr_(": ");
            msg += frustumTestFail;
        }
        Log::Error(msg);
    }
    if (!CameraPovScoringSelfCheck())
        Log::Error(xorstr_("Camera POV scoring self-check failed"));
    if (!CameraFovValidationSelfCheck())
        Log::Error(xorstr_("Camera FOV validation self-check failed"));
    if (!ControlRotationSelfCheck())
        Log::Error(xorstr_("Camera rotation comparison self-check failed"));
    if (!ControlRotationReferenceSelfCheck())
        Log::Error(xorstr_("ControlRotation reference trust self-check failed"));
    if (!CameraDiagnostics::CandidateClassificationSelfCheck())
        Log::Error(xorstr_("Camera candidate classification self-check failed"));
    if (!SyntheticInMatchCameraSourceSelfCheck())
        Log::Error(xorstr_("Synthetic in-match camera source self-check failed"));
    if (!QuatToRotatorSelfCheck())
        Log::Error(xorstr_("Quat rotator self-check failed"));

    Log::Info(xorstr_("--- Offset debug ---"));
    LogOffsetStep(xorstr_("Packed module base"), Baseadress);

    if (!PreferEngineDiscoveryPath())
        (void)ResolveSdkImageBase();

    {
        const int32_t numElements = ReadGObjectsNumElements(GlobalImageBase());
        std::ostringstream line;
        line << xorstr_("GObjects count ");
        if (numElements < 0)
            line << xorstr_("RPM failed");
        else
            line << std::dec << numElements;
        Log::Info(line.str());
    }

    LogOffsetStep(xorstr_("Unpacked image base"), UnpackedBase);
    if (UnpackedBase && UnpackedBase != Baseadress) {
        if (!PeHeaderReadableAt(UnpackedBase, xorstr_("SDK image")))
            Log::Warn(xorstr_("SDK image PE unreadable; UWorld RVA may not match this mapping"));
    }
    if (SdkUnpackedRegionsChecked > 0) {
        std::ostringstream line;
        line << xorstr_("Unpacked VA regions checked ") << std::dec << SdkUnpackedRegionsChecked;
        Log::Info(line.str());
    }
    if (!UnpackedBase)
        Log::Warn(xorstr_("SDK image base unset (using packed module)"));
    else if (UnpackedBase == Baseadress)
        Log::Info(xorstr_("Packed and unpacked bases coincide"));
    else
        Log::Info(xorstr_("Using separate unpacked SDK mapping"));

    {
        std::ostringstream peLine;
        peLine << xorstr_("GObjects RVA 0x") << std::hex << std::uppercase << SdkPeDiscovery::GObjectsRva()
               << xorstr_(" UWorld RVA 0x") << SdkPeDiscovery::UWorldRva() << std::dec;
        if (SdkPeDiscovery::scanFoundLive.load())
            peLine << xorstr_(" (PE scan)");
        else if (SdkPeDiscovery::staticRvasValidated.load())
            peLine << xorstr_(" (PE static)");
        else
            peLine << xorstr_(" (offsets.hpp)");
        Log::Info(peLine.str());
    }

    const ResolvedGObjects gobjects = ResolveGObjectsArray();
    LogOffsetStep(xorstr_("GObjects slot"), gobjects.arrayBase);
    {
        const auto layout = static_cast<GObjectsResolveLayout>(
            LastGObjectsResolveLayout.load(std::memory_order_relaxed));
        std::ostringstream layoutLine;
        layoutLine << xorstr_("GObjects layout ") << GObjectsResolveLayoutName(layout);
        Log::Info(layoutLine.str());
    }
    if (gobjects.viaDereference)
        Log::Info(xorstr_("GObjects via slot dereference"));
    LogOffsetStep(xorstr_("GObjects chunks"), gobjects.chunkTable);
    {
        std::ostringstream countLine;
        countLine << xorstr_("GObjects NumElements ");
        if (!gobjects.numElementsReadOk)
            countLine << xorstr_("RPM failed");
        else
            countLine << std::dec << gobjects.numElements;
        countLine << xorstr_(" NumChunks ");
        if (!gobjects.chunkTableReadOk && gobjects.numChunks == 0)
            countLine << xorstr_("RPM failed");
        else
            countLine << std::dec << gobjects.numChunks;
        Log::Info(countLine.str());
    }

    const uint64_t gworldDirectEarly = ReadUWorldFromImage(GlobalImageBase());
    if (gworldDirectEarly && Memory::IsValid(gworldDirectEarly))
        GEngineDiscovery::DisableDeferredStringScan.store(true, std::memory_order_relaxed);
    GEngineDiscovery::SetCandidateRejectLogging(false);
    const bool allowGEngineStringScan =
        !GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed);
    const uint64_t gengineXref =
        ResolveEngineViaCommandNotRecognizedXref(allowGEngineStringScan);
    GEngineDiscovery::SetCandidateRejectLogging(true);
    LogOffsetStep(xorstr_("GEngine (Command xref)"), gengineXref);
    const uint64_t gengine = gengineXref ? gengineXref : ResolveEngineViaGObjects();
    LogOffsetStep(xorstr_("GEngine (resolved)"), gengine);
    const bool gengineValid = gengine && Memory::IsValid(gengine);
    if (!gengineValid)
        Log::Warn(xorstr_("GEngine invalid (chain may fail)"));

    const uint64_t gameViewport =
        gengineValid ? Read<uint64_t>(gengine + Offsets::GameViewport) : 0;
    LogOffsetStep(xorstr_("GameViewport"), gameViewport);

    const uint64_t gworldChain =
        (gameViewport && Memory::IsValid(gameViewport))
            ? Read<uint64_t>(gameViewport + Offsets::ViewportWorld)
            : 0;
    LogOffsetStep(xorstr_("UWorld (GEngine chain)"), gworldChain);

    const uint64_t gworldDirect = ReadUWorldFromImage(GlobalImageBase());
    LogOffsetStep(xorstr_("UWorld direct"), gworldDirect);

    if (SdkPeDiscovery::peDataScanEnabled.load())
        SdkPeDiscovery::EnsureDiscoveredOffsets(Baseadress);

    const uint64_t gworld = ResolveUWorld();
    LogOffsetStep(xorstr_("UWorld (resolved)"), gworld);

    if (!gworld || !Memory::IsValid(gworld)) {
        Log::Warn(xorstr_("Offset chain stopped (no valid UWorld)"));
        if (SdkPeDiscovery::GObjectsRva() && !SdkPeDiscovery::UWorldRva())
            Log::Warn(xorstr_("GObjects resolved but UWorld global still missing (PE scan)"));
        return;
    }

    DisableGEngineStringScanForLiveSdk();
    LocalPtrs::Gworld = static_cast<uintptr_t>(gworld);
    SyncLocalPtrsFromWorld(static_cast<uintptr_t>(gworld));
    AdvanceRuntimeInitStage(RuntimeInitStage::SdkReady);
    SdkReady.store(true, std::memory_order_relaxed);

    UWorldFieldSnapshot uworldSnap{};
    (void)ReadUWorldFieldSnapshot(gworld, uworldSnap);
    {
        int uworldScore = ScoreUWorldSnapshot(uworldSnap, true);
        std::ostringstream snapLine;
        snapLine << xorstr_("UWorld fields score ") << std::dec << uworldScore
                 << xorstr_(" GameState 0x") << std::hex << std::uppercase << uworldSnap.gameState
                 << xorstr_(" LocalPlayer 0x") << uworldSnap.localPlayer << xorstr_(" PC 0x")
                 << uworldSnap.playerController << std::dec;
        Log::Info(snapLine.str());
    }

    const uint64_t gameState = Read<uintptr_t>(gworld + Offsets::GameState);
    LogOffsetStep(xorstr_("GameState"), gameState);

    const uint64_t gameInstance = Read<uint64_t>(gworld + Offsets::OwningGameInstance);
    LogOffsetStep(xorstr_("OwningGameInstance"), gameInstance);
    if (!gameInstance || !Memory::IsValid(gameInstance)) {
        Log::Warn(xorstr_("Offset chain stopped (OwningGameInstance invalid)"));
        return;
    }

    const uint64_t localPlayersArray = Read<uint64_t>(gameInstance + Offsets::LocalPlayers);
    LogOffsetStep(xorstr_("LocalPlayers array"), localPlayersArray);

    const uint64_t localPlayer = Read<uint64_t>(localPlayersArray);
    LogOffsetStep(xorstr_("LocalPlayer"), localPlayer);
    if (!localPlayer || !Memory::IsValid(localPlayer)) {
        Log::Warn(xorstr_("Offset chain stopped (LocalPlayer invalid)"));
        return;
    }

    const uint64_t playerController = Read<uint64_t>(localPlayer + Offsets::PlayerController);
    LogOffsetStep(xorstr_("PlayerController"), playerController);

    const uint64_t acknowledgedPawn = Read<uint64_t>(playerController + Offsets::AcknowledgedPawn);
    LogOffsetStep(xorstr_("AcknowledgedPawn"), acknowledgedPawn);

    const uint64_t playerState = Read<uint64_t>(playerController + Offsets::PlayerState);
    LogOffsetStep(xorstr_("PlayerState (PC)"), playerState);

    const uint64_t localPawn = ResolveLocalPawn(static_cast<uintptr_t>(playerController),
                                                static_cast<uintptr_t>(playerState));
    LogOffsetStep(xorstr_("Local pawn (resolved)"), localPawn);

    const uint64_t mesh =
        (localPawn && Memory::IsValid(localPawn))
            ? Read<uint64_t>(localPawn + Offsets::Mesh)
            : 0;
    LogOffsetStep(xorstr_("Mesh"), mesh);

    const uint64_t rootComponent =
        (localPawn && Memory::IsValid(localPawn))
            ? Read<uint64_t>(localPawn + Offsets::RootComponent)
            : 0;
    LogOffsetStep(xorstr_("RootComponent"), rootComponent);

    const uint64_t playerCameraManager = Read<uint64_t>(playerController + Offsets::playercameramanager);
    LogOffsetStep(xorstr_("PlayerCameraManager"), playerCameraManager);

    const UWorldCameraSample cam = ReadUWorldCamera(static_cast<uintptr_t>(gworld));
    if (!cam.resolved) {
        Log::Info(xorstr_("Camera skipped (PlayerCameraManager unread)"));
    } else {
        std::ostringstream povLine;
        povLine << xorstr_("Camera (") << cam.name << xorstr_(" 0x") << std::hex << std::uppercase
                << cam.fieldOffset << std::dec << xorstr_(" POV ") << cam.location.x << ", "
                << cam.location.y << ", " << cam.location.z << xorstr_(") FOV ") << cam.fov;
        if (!cam.usable)
            povLine << xorstr_(" REJECTED");
        Log::Info(povLine.str());

        // GetCamera scores the manager's POV blocks against this pawn, so hand it the pawn the
        // debug pass already resolved: UpdateLocalPlayerThread has not started yet.
        LocalPtrs::Player = static_cast<uintptr_t>(localPawn);
        LocalPtrs::PlayerState = static_cast<uintptr_t>(playerState);
        LocalPtrs::PlayerController = static_cast<uintptr_t>(playerController);
        if (localPawn && Memory::IsValid(localPawn))
            LocalPtrs::PlayerMesh = Read<uintptr_t>(localPawn + Offsets::Mesh);
        GetCamera();
        CommitCameraFrameForW2S();
        std::ostringstream resolved;
        if (Camera::Valid) {
            resolved << xorstr_("Camera resolved [v23] ") << Camera::Source << xorstr_(" +0x")
                     << std::hex << std::uppercase << Camera::SourceOffset << std::dec
                     << xorstr_(" loc ") << Camera::Location.x << ", " << Camera::Location.y << ", "
                     << Camera::Location.z << xorstr_(" rot ") << Camera::Rotation.x << ", "
                     << Camera::Rotation.y << ", " << Camera::Rotation.z
                     << xorstr_(" fov ") << Camera::FOV;
        } else {
            resolved << xorstr_("Camera unresolved stage=") << RenderPipeline::g_CameraLifecycle.stage
                     << xorstr_(" source=") << RenderPipeline::g_CameraLifecycle.source
                     << xorstr_(" reason=") << RenderPipeline::g_CameraLifecycle.reason
                     << xorstr_(" - fail closed, nothing projects");
        }
        Log::Info(resolved.str());

        if (Camera::ViewProjectionReady) {
            const char* rendererTestFail = nullptr;
            if (!RunOfflineRendererPipelineSelfTest(&rendererTestFail)) {
                std::ostringstream rt;
                rt << xorstr_("Offline renderer pipeline self-test failed");
                if (rendererTestFail && rendererTestFail[0]) {
                    rt << xorstr_(": ");
                    rt << rendererTestFail;
                }
                Log::Warn(rt.str());
            }
        } else {
            Log::Info(xorstr_("Renderer self-test deferred until CAMERA_READY"));
        }
    }

    const uintptr_t playerArray = (gameState && Memory::IsValid(gameState))
        ? Read<uintptr_t>(gameState + Offsets::PlayerArray) : 0;
    int playerCount = (gameState && Memory::IsValid(gameState))
        ? Read<int>(gameState + Offsets::PlayerArray + sizeof(uintptr_t)) : 0;
    if (playerCount > 32) playerCount = 32;
    LocalPtrs::PlayerArray = playerArray;
    LocalPtrs::PlayerArrayCount = playerCount;
    const bool teamFilterActive =
        localPawn && Memory::IsValid(localPawn) && playerState && Memory::IsValid(playerState);
    const uint8_t localTeam =
        teamFilterActive ? Read<uint8_t>(playerState + Offsets::TeamIndex) : static_cast<uint8_t>(0);
    uintptr_t meshPtr = 0;
    uintptr_t meshPawn = 0;
    float meshPlace = -1.f;
    int espCacheEligible = 0;
    int espProjectable = 0;
    int espLevelAdded = 0;
    std::vector<LocalPtrs::CachedPlayer> espProbeCache;
    espProbeCache.reserve(64);
    std::unordered_set<uintptr_t> espSeenPawns;
    for (int i = 0; playerArray && i < playerCount; ++i) {
        const uintptr_t ps = Read<uintptr_t>(playerArray + i * sizeof(uintptr_t));
        if (!ps) continue;
        const uintptr_t pawn = Read<uintptr_t>(ps + Offsets::PawnPrivate);
        if (pawn && espSeenPawns.count(pawn)) continue;
        if (pawn) espSeenPawns.insert(pawn);
        PushCachedPlayerFromPlayerState(ps, espProbeCache);
    }
    const size_t beforeLevels = espProbeCache.size();
    AppendPlayersFromWorldLevels(static_cast<uintptr_t>(gworld), espProbeCache, espSeenPawns);
    espLevelAdded = static_cast<int>(espProbeCache.size() - beforeLevels);
    PushCachedLobbyLocalPreview(espProbeCache);
    espCacheEligible = static_cast<int>(espProbeCache.size());
    int espLobbyPreview = 0;
    for (const auto& cp : espProbeCache) {
        if (cp.LobbyPreview) {
            espLobbyPreview = 1;
            break;
        }
    }
    for (const auto& cp : espProbeCache) {
        const uintptr_t mesh = cp.Mesh ? cp.Mesh : Read<uintptr_t>(cp.Pawn + Offsets::Mesh);
        if (!mesh || !Memory::IsValid(mesh)) continue;
        if (Camera::Valid) {
            const Vector3 head3d = GetBoneWithRotation(mesh, EBoneIndex::Head);
            const Vector3 bottom3d = GetBoneWithRotation(mesh, EBoneIndex::Root);
            if (!BoneWorldMissing(head3d) && !BoneWorldMissing(bottom3d)) {
                Vector3 head2d{}, bottom2d{};
                if (ProjectWorldToScreen({ head3d.x, head3d.y, head3d.z + 20.f }, &head2d) &&
                    ProjectWorldToScreen({ bottom3d.x, bottom3d.y, bottom3d.z - 10.f }, &bottom2d)) {
                    ++espProjectable;
                }
            }
        }
        const Vector3 place = ReadFTransformTranslation(mesh + Offsets::ComponentToWorld);
        const float span = CameraLocationMaxAbs(place);
        if (!meshPtr || span > meshPlace) {
            meshPtr = mesh;
            meshPawn = cp.Pawn;
            meshPlace = span;
        }
    }
    {
        std::ostringstream espLine;
        espLine << xorstr_("ESP PlayerArray ") << playerCount << xorstr_(" cache-eligible ")
                << espCacheEligible << xorstr_(" level-added ") << espLevelAdded
                << xorstr_(" lobby-preview ") << espLobbyPreview << xorstr_(" local team ")
                << static_cast<int>(localTeam);
        if (Camera::Valid)
            espLine << xorstr_(" on-screen ") << espProjectable;
        Log::Info(espLine.str());
    }
    if (localPawn && Memory::IsValid(localPawn)) {
        meshPtr = ResolveEspMeshForPawn(static_cast<uintptr_t>(localPawn));
        meshPawn = static_cast<uintptr_t>(localPawn);
    }
    if (meshPtr) {
        std::ostringstream meshLine;
        meshLine << std::hex << std::uppercase << xorstr_("Bone mesh 0x") << meshPtr << std::dec
                 << xorstr_(" (local pawn mesh)");
        Log::Info(meshLine.str());
        LogMeshPoseTArrays(meshPtr);

        ResolvedBoneArray pose{};
        const bool poseFound = ResolveBonePoseArray(meshPtr, pose);
        std::ostringstream used;
        used << xorstr_("Bone pose ");
        if (poseFound) {
            used << xorstr_("found at mesh+0x") << std::hex << std::uppercase << pose.MeshOffset
                 << xorstr_(" Data 0x") << pose.Data << std::dec << xorstr_(" Num ") << pose.Num;
        } else {
            used << xorstr_("NOT FOUND (no TArray<FTransform> on the mesh)");
        }
        Log::Info(used.str());

        {
            const MeshTransformSnapshot c2wSnap =
                ResolveMeshTransformSnapshot(meshPtr, static_cast<uintptr_t>(meshPawn));
            std::ostringstream c2wLine;
            c2wLine << xorstr_("bone_transform source=") << (c2wSnap.source ? c2wSnap.source : "?")
                    << xorstr_(" valid=") << (c2wSnap.valid ? 1 : 0)
                    << xorstr_(" component_to_world location=") << c2wSnap.translation.x << ", "
                    << c2wSnap.translation.y << ", " << c2wSnap.translation.z;
            if (c2wSnap.rejectionReason)
                c2wLine << xorstr_(" world_transform rejection_reason=") << c2wSnap.rejectionReason;
            Log::Info(c2wLine.str());
        }

        if (poseFound) {
            const int bones[] = { EBoneIndex::Root, EBoneIndex::Pelvis, EBoneIndex::Spine_05,
                                  EBoneIndex::Hand_L, EBoneIndex::Neck_01, EBoneIndex::Head,
                                  EBoneIndex::Foot_L };
            for (int bone : bones) {
                FTransform cs{};
                std::ostringstream line;
                line << xorstr_("Bone[") << bone << xorstr_("] ");
                if (!ResolveBoneComponentSpaceTransform(meshPtr, bone, cs)) {
                    line << xorstr_("unresolved");
                } else {
                    line << xorstr_("cs ") << cs.translation.x << ", " << cs.translation.y << ", "
                         << cs.translation.z;
                }
                Log::Info(line.str());
            }
        } else {
            int poseNum = 0;
            uint32_t foundPoseOffset = 0;
            const uint32_t meshOffset = FindPawnMeshOffset(meshPawn, foundPoseOffset, poseNum);
            std::ostringstream probe;
            probe << xorstr_("Pawn mesh probe ");
            if (meshOffset) {
                probe << xorstr_("pawn+0x") << std::hex << std::uppercase << meshOffset
                      << xorstr_(" holds a pose at +0x") << foundPoseOffset << std::dec
                      << xorstr_(" Num ") << poseNum
                      << xorstr_(" (Offsets::Mesh is 0x") << std::hex << Offsets::Mesh << std::dec << ')';
            } else {
                probe << xorstr_("no component with a pose found on the pawn");
            }
            Log::Info(probe.str());
        }

        const Vector3 headW = GetBoneWithRotation(meshPtr, EBoneIndex::Head);
        const Vector3 pelvisW = GetBoneWithRotation(meshPtr, EBoneIndex::Pelvis);
        std::ostringstream worldLine;
        worldLine << xorstr_("Bone world head ") << headW.x << ", " << headW.y << ", " << headW.z
                  << xorstr_(" pelvis ") << pelvisW.x << ", " << pelvisW.y << ", " << pelvisW.z;
        Log::Info(worldLine.str());

        if (!BoneMultiplyKeepsOffset())
            Log::Error(xorstr_("Bone multiply collapsed to origin"));
        if (!BoneMatrixMultiplyOrderSelfCheck())
            Log::Error(xorstr_("Bone matrix multiply order self-check failed"));
        if (!BoneIdentityUsesC2wTranslationSelfCheck())
            Log::Error(xorstr_("Identity bone C2W invariant self-check failed"));
        if (!PoseSpreadSelfCheck())
            Log::Error(xorstr_("Pose-buffer classifier self-check failed"));
        if (!RetracComponentPoseSlotSelfCheck())
            Log::Error(xorstr_("Retrac bone pose slot self-check failed"));
        if (!PoseSpaceClassifySelfCheck())
            Log::Error(xorstr_("Pose space classifier self-check failed"));

        if (Camera::Valid) {
            std::ostringstream camRaw;
            camRaw << xorstr_("Camera in use loc ") << Camera::Location.x << ", "
                   << Camera::Location.y << ", " << Camera::Location.z << xorstr_(" rot ")
                   << Camera::Rotation.x << ", " << Camera::Rotation.y << ", "
                   << Camera::Rotation.z << xorstr_(" fov ") << Camera::FOV
                   << xorstr_(" cam-to-target ") << Dist3(Camera::Location, GetMeshWorldLocation(meshPtr));
            Log::Info(camRaw.str());
        }

        const Vector3 c2w = GetMeshWorldLocation(meshPtr);
        const Vector3 w0 = GetBoneWithRotation(meshPtr, EBoneIndex::Root);
        const Vector3 w11 = GetBoneWithRotation(meshPtr, EBoneIndex::Hand_L);
        Vector3 s0{};
        Vector3 s11{};
        const char* reason0 = nullptr;
        const char* reason11 = nullptr;
        const bool on0 = ProjectWorldToScreenWithReason(w0, &s0, &reason0);
        const bool on11 = ProjectWorldToScreenWithReason(w11, &s11, &reason11);
        const int segments = CountSubmittedSkeleton(meshPtr);
        std::ostringstream drawLine;
        drawLine << xorstr_("Draw mesh 0x") << std::hex << std::uppercase << meshPtr << std::dec
                 << xorstr_(" C2W ") << c2w.x << ", " << c2w.y << ", " << c2w.z
                 << xorstr_(" b0 ") << w0.x << ", " << w0.y << ", " << w0.z
                 << xorstr_(" W2S ") << (on0 ? xorstr_("pass") : xorstr_("fail"))
                 << (reason0 ? reason0 : "?");
        if (on0)
            drawLine << ' ' << s0.x << ", " << s0.y;
        drawLine << xorstr_(" b11 ") << w11.x << ", " << w11.y << ", " << w11.z
                 << xorstr_(" W2S ") << (on11 ? xorstr_("pass") : xorstr_("fail"))
                 << (reason11 ? reason11 : "?");
        if (on11)
            drawLine << ' ' << s11.x << ", " << s11.y;
        drawLine << xorstr_(" segments ") << segments;
        Log::Info(drawLine.str());
    }
}


int main(int argc, char* argv[]) {
    JUNK_CODE_ONE;
    Utils::EnableANSI();
    SetConsoleTransparency(240);
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);

    char tempBuffer[MAX_PATH];
    GetTempPathA(MAX_PATH, tempBuffer);
    std::filesystem::path bakPath = std::filesystem::path(tempBuffer) / exePath.filename();
    bakPath += ".bak";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sizeDist(16, 256);
    std::uniform_int_distribution<> byteDist(0, 255);
    int size = sizeDist(gen);
    std::vector<unsigned char> randomBytes(size);
    for (int i = 0; i < size; ++i)
    randomBytes[i] = static_cast<unsigned char>(byteDist(gen));
    std::error_code ec;

    if (std::filesystem::exists(bakPath)) {
        SetFileAttributesA(bakPath.string().c_str(), FILE_ATTRIBUTE_NORMAL);
        std::filesystem::remove(bakPath, ec);
    }

    std::filesystem::rename(exePath, bakPath, ec);

    if (!ec) {
        std::filesystem::copy(bakPath, exePath, ec);
        if (!ec) {
            std::ofstream exe(exePath, std::ios::binary | std::ios::app);
            if (exe) {
                exe.write(reinterpret_cast<char*>(randomBytes.data()), randomBytes.size());
                exe.close();

                Log::Success(xorstr_("Custom Build Initialized! (to avoid ban wave)"));
                Sleep(2000);
            }
            else {
                Log::Error(xorstr_("Error 0x001 While Creating Custom Build"));
                Sleep(2000);
            }
        }
        else {
            Log::Error(xorstr_("Error 0x002 While Creating Custom Build: ") + ec.message());
            std::filesystem::rename(bakPath, exePath, ec);
            Sleep(2000);
        }
    }
    else {
        Log::Error(xorstr_("Error 0x003 While Creating Custom Build: ") + ec.message());
        Sleep(2000);
    }

    system(xorstr_("cls"));
    JUNK_CODE_TWO;
    std::thread(Utils::RandomTitleLoop).detach();
    Utils::PrintBanner();
    Utils::Spinner(xorstr_("Initializing"), 1200);
    {
        std::string font_path = "C:\\Windows\\Fonts\\fa-solid-900.ttf";
        if (!std::filesystem::exists(font_path)) {
             std::ofstream font_file(font_path, std::ios::binary);
             if (font_file.is_open()) {
                 font_file.write(reinterpret_cast<const char*>(Font), sizeof(Font));
                 font_file.close();
             }
        }
    }

    system(xorstr_("cls"));
    Utils::PrintBanner();
    LoadDriver();
    Utils::WaitForProcess();

    if (!Memory::Attach()) {
        Log::Error(
            xorstr_("OpenProcess failed (GetLastError=")
            + std::to_string(Memory::Process.AttachLastError)
            + xorstr_("; run as admin?)")
        );
        PauseBeforeExit();
        return 1;
    }
    Log::Success(
        Memory::Process.ReadOnlyAttach
            ? xorstr_("Process memory attached (read-only)")
            : xorstr_("Process memory attached")
    );

    Utils::WaitForKey(VK_INSERT);

    Baseadress = Memory::ModuleBase();

    if (!Baseadress) {
        Log::Error(xorstr_("Base Address not Found!"));
        PauseBeforeExit();
        return 1;
    }

    if (!RequireGameModuleReadable()) {
        PauseBeforeExit();
        return 1;
    }

    LogMainModuleAtBaseOnce();
    (void)TryConnectKernelForBlockedData(Baseadress);
    LogGObjectsReadProbeOnce();

    SdkReady.store(false, std::memory_order_relaxed);
    Log::Info(xorstr_("SDK resolve — overlay poll (direct UWorld, no GEngine scan when valid)"));

    std::thread([] { DebugOffsets(); }).detach();

    StartExploitThreads();
    std::thread(TriggerbotThread).detach();
    std::thread(World_esp_thread).detach();
    std::thread(PlayerCacheThread).detach();
    std::thread(UpdateLocalPlayerThread).detach();
    Log::Success(xorstr_("Successfully Loaded"));
    JUNK_CODE_THREE;
    if (!setup()) {
        if (!OverlayExitReason.empty())
            Log::Error(OverlayExitReason);
        else
            Log::Error(xorstr_("Overlay failed to start"));
        PauseBeforeExit();
        return 1;
    }
    if (!OverlayExitReason.empty())
        Log::Warn(OverlayExitReason);
    PauseBeforeExit();
    return 0;
}
