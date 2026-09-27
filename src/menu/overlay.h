#pragma once
#include "../util/common.h"
#include <cstring>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "../util/Settings.h"
#include "../game/Gameloop.h"
#include "../driver/communication.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")

inline std::string OverlayExitReason;

static void OverlayLogLine(const std::string& line, bool error = true) {
    OverlayExitReason = line;
    std::cout << (error ? xorstr_("[-] ") : xorstr_("[!] ")) << line << '\n';
    std::cout.flush();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool g_overlayAllowTearing = false;
static RECT g_menuHitRects[32];
static int g_menuHitRectCount = 0;

static bool PointInMenuHitRects(POINT clientPt)
{
    for (int i = 0; i < g_menuHitRectCount; ++i) {
        if (PtInRect(&g_menuHitRects[i], clientPt))
            return true;
    }
    return false;
}

// ImGui stores the Begin() label pointer; must be stable (not xorstr stack).
static constexpr const char kOverlayMenuWindowId[] = "RetracMenu";

static bool g_overlayUiReady = false;
static bool g_overlayLastFrameMenuRendered = false;
static bool g_insertKeyPrevDown = false;
static bool g_insertHotkeyRegistered = false;
static bool g_menuOpenPersistExpected = false;
static unsigned long long g_lastInsertMenuToggleMs = 0;
static HWND g_overlayHwnd = nullptr;

static constexpr int kInsertMenuHotkeyId = 1;
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

static bool OverlaySampleInsertDown()
{
    const SHORT insertAsync = static_cast<SHORT>(GetAsyncKeyState(VK_INSERT));
    if ((insertAsync & 0x8000) != 0)
        return true;
    BYTE keyboardState[256]{};
    if (GetKeyboardState(keyboardState) && (keyboardState[VK_INSERT] & 0x80) != 0)
        return true;
    return (GetKeyState(VK_INSERT) & 0x8000) != 0;
}

static void OverlaySyncInsertKeyStateFromHardware()
{
    g_insertKeyPrevDown = OverlaySampleInsertDown();
}

static void OverlaySyncPassthroughStyle(HWND hwnd);

static void OverlayCommitInsertMenuToggle(const char* via)
{
    const unsigned long long nowMs = GetTickCount64();
    if (g_lastInsertMenuToggleMs != 0 && nowMs - g_lastInsertMenuToggleMs < 200)
        return;
    g_lastInsertMenuToggleMs = nowMs;

    const SHORT insertAsync = static_cast<SHORT>(GetAsyncKeyState(VK_INSERT));
    const bool keyState = OverlaySampleInsertDown();
    const bool previous = g_insertKeyPrevDown;
    g_insertKeyPrevDown = keyState;

    const bool persistedRead = Settings::Menu;
    if (persistedRead != g_menuOpenPersistExpected) {
        std::cout << xorstr_("[menu] state_overwrite read=") << (persistedRead ? 1 : 0)
                  << xorstr_(" expected=") << (g_menuOpenPersistExpected ? 1 : 0) << '\n';
    }
    const bool toggleBefore = Settings::Menu;
    Settings::Menu = !toggleBefore;
    const bool toggleAfter = Settings::Menu;
    g_menuOpenPersistExpected = toggleAfter;
    OverlaySyncPassthroughStyle(g_overlayHwnd);

    std::cout << xorstr_("[menu] key_state=") << (keyState ? 1 : 0) << xorstr_(" previous=")
              << (previous ? 1 : 0) << xorstr_(" pressed=1 via=") << via << xorstr_(" async_lsb=")
              << ((insertAsync & 1) ? 1 : 0) << '\n';
    std::cout << xorstr_("[menu] toggle before=") << (toggleBefore ? 1 : 0) << xorstr_(" after=")
              << (toggleAfter ? 1 : 0) << xorstr_(" menu_open=") << (toggleAfter ? 1 : 0)
              << xorstr_(" persisted_read=") << (persistedRead ? 1 : 0) << xorstr_(" ui_same_var=1")
              << '\n';
    std::cout.flush();
}

static void OverlayPollInsertMenuToggle()
{
    const SHORT insertAsync = static_cast<SHORT>(GetAsyncKeyState(VK_INSERT));
    const bool keyState = OverlaySampleInsertDown();
    const bool transitionBit = (insertAsync & 1) != 0;
    const bool rising = keyState && !g_insertKeyPrevDown;
    if (!rising && !transitionBit)
        return;
    OverlayCommitInsertMenuToggle("poll");
}

static void OverlayHandleInsertHotkeyMessage()
{
    OverlayCommitInsertMenuToggle("hotkey");
}

static void OverlaySyncPassthroughStyle(HWND hwnd)
{
    if (!hwnd)
        return;
    const LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    const bool menu = Settings::Menu;
    LONG_PTR next = ex;
    if (menu)
        next &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    else
        next |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    if (next == ex)
        return;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, next);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    if (menu) {
        ClipCursor(nullptr);
        SetForegroundWindow(hwnd);
    }
}

static void OverlayPumpThreadMessages(MSG& msg)
{
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_HOTKEY && msg.wParam == static_cast<WPARAM>(kInsertMenuHotkeyId)) {
            OverlayHandleInsertHotkeyMessage();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void OverlayAddWindowHitRect(ImGuiWindow* w, float vpX, float vpY)
{
    if (!w || w->Hidden || w->IsFallbackWindow)
        return;
    if (w->Name && strcmp(w->Name, "RadarInteract") == 0)
        return;
    if (!w->Active && !w->WasActive)
        return;
    if (w->Size.x < 1.f || w->Size.y < 1.f)
        return;
    if (g_menuHitRectCount >= 32)
        return;
    RECT& r = g_menuHitRects[g_menuHitRectCount++];
    const ImVec2 mn = w->OuterRectClipped.Min;
    const ImVec2 mx = w->OuterRectClipped.Max;
    r.left = (LONG)(mn.x - vpX);
    r.top = (LONG)(mn.y - vpY);
    r.right = (LONG)(mx.x - vpX);
    r.bottom = (LONG)(mx.y - vpY);
}

static void OverlayAddFallbackMenuHitRect()
{
    if (g_menuHitRectCount >= 32)
        return;
    RECT& r = g_menuHitRects[g_menuHitRectCount++];
    r.left = 80;
    r.top = 80;
    r.right = 820;
    r.bottom = 600;
}

static void CaptureMenuHitRects()
{
    g_menuHitRectCount = 0;
    if (!Settings::Menu)
        return;
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx)
        return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float vpX = vp ? vp->Pos.x : 0.f;
    const float vpY = vp ? vp->Pos.y : 0.f;
    ImGuiWindow* menuRoot = ImGui::FindWindowByName(kOverlayMenuWindowId);
    if (!menuRoot) {
        OverlayAddFallbackMenuHitRect();
        return;
    }
    const ImGuiWindow* menuTree = menuRoot->RootWindow;
    for (int i = ctx->Windows.Size - 1; i >= 0 && g_menuHitRectCount < 32; --i) {
        ImGuiWindow* w = ctx->Windows[i];
        if (!w)
            continue;
        if (w->Name && strcmp(w->Name, "RadarInteract") == 0)
            continue;
        if (!w->Active && !w->WasActive)
            continue;
        const bool popupOrTip =
            (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0;
        const bool menuTreeWin = menuTree && w->RootWindow == menuTree;
        if (popupOrTip || menuTreeWin)
            OverlayAddWindowHitRect(w, vpX, vpY);
    }
    if (g_menuHitRectCount == 0)
        OverlayAddFallbackMenuHitRect();
}

static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY && wParam == static_cast<WPARAM>(kInsertMenuHotkeyId)) {
        OverlayHandleInsertHotkeyMessage();
        return 0;
    }
    if (msg == WM_MOUSEACTIVATE)
        return Settings::Menu ? MA_ACTIVATE : MA_NOACTIVATE;
    if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) &&
        (wParam == VK_SNAPSHOT || wParam == VK_F12))
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    if (msg == WM_NCHITTEST) {
        if (!Settings::Menu)
            return HTTRANSPARENT;
        POINT pt;
        pt.x = (LONG)(short)LOWORD(lParam);
        pt.y = (LONG)(short)HIWORD(lParam);
        ScreenToClient(hWnd, &pt);
        if (PointInMenuHitRects(pt))
            return HTCLIENT;
        return HTTRANSPARENT;
    }
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

#include "TextureLoader.h"

void Menu();
const char* GetKeyName(int key);

DWORD GetProcessIdByName(const std::wstring& exeName) {
    std::wstring target = exeName;
    std::transform(target.begin(), target.end(), target.begin(), ::towlower);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring current = pe.szExeFile;
            std::transform(current.begin(), current.end(), current.begin(), ::towlower);
            if (current == target) {
                DWORD pid = pe.th32ProcessID;
                CloseHandle(snap);
                return pid;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

HWND FindVisibleWindowByPid(DWORD pid)
{
    HWND found = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        DWORD winPid = 0;
        GetWindowThreadProcessId(hwnd, &winPid);
        if (winPid != static_cast<DWORD>(lParam)) return TRUE;

        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        if (style & WS_VISIBLE) {
            if (GetWindow(hwnd, GW_OWNER) == NULL) {
                SetProp(hwnd, L"__found_tmp__", (HANDLE)1);
                return FALSE;
            }
        }
        return TRUE;
        }, (LPARAM)pid);

    EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
        if (GetProp(hwnd, L"__found_tmp__") != NULL) {
            RemoveProp(hwnd, L"__found_tmp__");
            SetProp(hwnd, L"__return_tmp__", (HANDLE)hwnd);
            return FALSE;
        }
        return TRUE;
        }, 0);

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        HANDLE h = GetProp(hwnd, L"__return_tmp__");
        if (h != NULL) {
            RemoveProp(hwnd, L"__return_tmp__");
            HWND* out = (HWND*)lParam;
            *out = (HWND)h;
            return FALSE;
        }
        return TRUE;
        }, (LPARAM)&found);

    return found;
}

// Game keeps foreground (WS_EX_NOACTIVATE); ImGui Win32 backend skips mouse unless hwnd is focused.
static void OverlayFeedMenuInput(HWND hwnd, ImGuiIO& io)
{
    if (!Settings::Menu || !hwnd)
        return;
    POINT screenPt{};
    if (!GetCursorPos(&screenPt))
        return;
    POINT clientPt = screenPt;
    if (!ScreenToClient(hwnd, &clientPt))
        return;
    io.AddMousePosEvent(static_cast<float>(clientPt.x), static_cast<float>(clientPt.y));

    if (GetForegroundWindow() == hwnd)
        return;

    if (!PointInMenuHitRects(clientPt))
        return;

    static bool prevDown[3]{};
    const int vk[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
    for (int i = 0; i < 3; ++i) {
        const bool down = (GetAsyncKeyState(vk[i]) & 0x8000) != 0;
        if (down != prevDown[i]) {
            io.AddMouseButtonEvent(i, down);
            prevDown[i] = down;
        }
    }
}

void FeedInput() {
    auto& io = ImGui::GetIO();
    for (int i = 0x41; i <= 0x5A; i++) {
        if (GetAsyncKeyState(i) & 1) {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000);
            char c = (char)i;
            if (!shift) c += 32;
            io.AddInputCharacter(c);
        }
    }
    for (int i = 0x30; i <= 0x39; i++) {
        if (GetAsyncKeyState(i) & 1) {
            io.AddInputCharacter((char)i);
        }
    }
    if (GetAsyncKeyState(VK_SPACE) & 1) io.AddInputCharacter(' ');   
    if (GetAsyncKeyState(VK_BACK) & 1) {
        io.AddKeyEvent(ImGuiKey_Backspace, true);
        io.AddKeyEvent(ImGuiKey_Backspace, false);
    }
    if (GetAsyncKeyState(VK_RETURN) & 1) {
        io.AddKeyEvent(ImGuiKey_Enter, true);
        io.AddKeyEvent(ImGuiKey_Enter, false);
    }
}

bool create_overlay()
{
    OverlayExitReason.clear();
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* render_target_view = nullptr;
    MSG msg{};

    DWORD gamePid = static_cast<DWORD>(Memory::process_id);
    if (!gamePid)
        gamePid = GetProcessIdByName(L"FortniteClient-Win64-Shipping.exe");
    if (!gamePid) {
        OverlayLogLine(xorstr_("Overlay: FortniteClient-Win64-Shipping.exe not found (pid 0)"));
        return false;
    }

    HWND gameHwnd = nullptr;
    for (int attempts = 0; !gameHwnd && attempts < 100; ++attempts) {
        gameHwnd = FindVisibleWindowByPid(gamePid);
        if (!gameHwnd)
            Sleep(100);
    }
    if (!gameHwnd) {
        OverlayLogLine(
            std::string(xorstr_("Overlay: no visible HWND for Fortnite pid "))
            + std::to_string(gamePid));
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Discord Inc.";
    if (!RegisterClassExW(&wc)) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            OverlayLogLine(
                std::string(xorstr_("Overlay: RegisterClassExW failed GetLastError="))
                + std::to_string(err));
            return false;
        }
    }

    RECT gameRect{};
    GetWindowRect(gameHwnd, &gameRect);
    int overlayW = gameRect.right - gameRect.left;
    int overlayH = gameRect.bottom - gameRect.top;
    if (overlayW <= 0) overlayW = Settings::Width;
    if (overlayH <= 0) overlayH = Settings::Height;
    UpdateViewportSize(overlayW, overlayH);

    constexpr DWORD kOverlayExStyle =
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
        WS_EX_NOREDIRECTIONBITMAP;
    HWND hwnd = CreateWindowExW(
        kOverlayExStyle,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        gameRect.left,
        gameRect.top,
        overlayW,
        overlayH,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);
    if (!hwnd) {
        OverlayLogLine(
            std::string(xorstr_("Overlay: CreateWindowExW failed GetLastError="))
            + std::to_string(GetLastError()));
        return false;
    }
    g_overlayHwnd = hwnd;

    SetWindowDisplayAffinity(hwnd, Settings::StreamProof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);

    // Premultiplied flip chain (below). Color-key blit was locking Present to ~32 Hz.
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Thread-queue hotkey (hwnd=null): WM_HOTKEY is delivered to this overlay thread even when the game is focused.
    if (RegisterHotKey(nullptr, kInsertMenuHotkeyId, MOD_NOREPEAT, VK_INSERT)) {
        g_insertHotkeyRegistered = true;
    } else if (RegisterHotKey(hwnd, kInsertMenuHotkeyId, MOD_NOREPEAT, VK_INSERT)) {
        g_insertHotkeyRegistered = true;
    } else {
        g_insertHotkeyRegistered = false;
        std::cout << xorstr_("[menu] hotkey_register_fail err=") << GetLastError()
                  << xorstr_(" (poll-only fallback)\n");
        std::cout.flush();
    }

    const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL selected_feature_level;
    const HRESULT devHr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
        _countof(feature_levels), D3D11_SDK_VERSION, &device, &selected_feature_level, &device_context);
    if (FAILED(devHr)) {
        std::ostringstream oss;
        oss << xorstr_("Overlay: D3D11CreateDevice failed HRESULT=0x") << std::hex << std::uppercase
            << static_cast<unsigned>(devHr);
        OverlayLogLine(oss.str());
        DestroyWindow(hwnd);
        return false;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* dxgiAdapter = nullptr;
    IDXGIFactory2* dxgiFactory = nullptr;
    IDCompositionDevice* dcompDevice = nullptr;
    IDCompositionTarget* dcompTarget = nullptr;
    IDCompositionVisual* dcompVisual = nullptr;
    HRESULT d3dHr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (SUCCEEDED(d3dHr))
        d3dHr = dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));
    if (SUCCEEDED(d3dHr))
        d3dHr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = static_cast<UINT>(overlayW);
    swapDesc.Height = static_cast<UINT>(overlayH);
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    IDXGISwapChain1* swap1 = nullptr;
    g_overlayAllowTearing = false;
    if (SUCCEEDED(d3dHr))
        d3dHr = dxgiFactory->CreateSwapChainForComposition(device, &swapDesc, nullptr, &swap1);
    if (SUCCEEDED(d3dHr))
        d3dHr = DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&dcompDevice));
    if (SUCCEEDED(d3dHr))
        d3dHr = dcompDevice->CreateTargetForHwnd(hwnd, TRUE, &dcompTarget);
    if (SUCCEEDED(d3dHr))
        d3dHr = dcompDevice->CreateVisual(&dcompVisual);
    if (SUCCEEDED(d3dHr))
        d3dHr = dcompVisual->SetContent(swap1);
    if (SUCCEEDED(d3dHr))
        d3dHr = dcompTarget->SetRoot(dcompVisual);
    if (SUCCEEDED(d3dHr))
        d3dHr = dcompDevice->Commit();
    if (SUCCEEDED(d3dHr) && swap1)
        d3dHr = swap1->QueryInterface(IID_PPV_ARGS(&swap_chain));
    if (dxgiFactory)
        dxgiFactory->Release();
    if (dxgiAdapter)
        dxgiAdapter->Release();
    if (dxgiDevice)
        dxgiDevice->Release();
    if (swap1)
        swap1->Release();
    if (FAILED(d3dHr) || !swap_chain) {
        std::ostringstream oss;
        oss << xorstr_("Overlay: composition swap chain failed HRESULT=0x") << std::hex
            << std::uppercase << static_cast<unsigned>(d3dHr);
        OverlayLogLine(oss.str());
        if (dcompVisual)
            dcompVisual->Release();
        if (dcompTarget)
            dcompTarget->Release();
        if (dcompDevice)
            dcompDevice->Release();
        DestroyWindow(hwnd);
        return false;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))) || !back_buffer) {
        OverlayLogLine(xorstr_("Overlay: swap chain GetBuffer failed"));
        DestroyWindow(hwnd);
        return false;
    }

    if (FAILED(device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view))) {
        OverlayLogLine(xorstr_("Overlay: CreateRenderTargetView failed"));
        DestroyWindow(hwnd);
        return false;
    }

    back_buffer->Release();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle* style = &ImGui::GetStyle();
    style->WindowTitleAlign.x = 0.50f;
    style->WindowRounding = 0;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, device_context);
    g_overlayUiReady = true;
    g_overlayLastFrameMenuRendered = false;
    g_menuOpenPersistExpected = Settings::Menu;
    OverlaySyncInsertKeyStateFromHardware();
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 16.0f);
    ImGui::GetIO().Fonts->AddFontFromFileTTF(xorstr_("C:\\Windows\\Fonts\\fa-solid-900.ttf"), 16.0f, &icons_config, icons_ranges);
    LocalPtrs::GameFont = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 30.0f);

    constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    unsigned long long overlayFrames = 0;
    unsigned long long nextFindMs = 0;
    unsigned long long nextTopMs = 0;
    RECT lastRect = gameRect;
    int cursorMode = 0;
    LARGE_INTEGER qpf{};
    LARGE_INTEGER qpcFrame{};
    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&qpcFrame);
    while (msg.message != WM_QUIT)
    {
        ++overlayFrames;
        if (overlayFrames == 1) {
            std::cout << xorstr_("[menu] overlay_loop=1 ui_ready=") << (g_overlayUiReady ? 1 : 0)
                      << xorstr_(" hotkey_registered=") << (g_insertHotkeyRegistered ? 1 : 0)
                      << xorstr_(" menu_open=") << (Settings::Menu ? 1 : 0) << '\n';
            std::cout.flush();
        }
        OverlayPumpThreadMessages(msg);
        (void)MsgWaitForMultipleObjects(0, nullptr, FALSE, 0, QS_ALLINPUT);

        const unsigned long long nowMs = GetTickCount64();
        if (!IsWindow(gameHwnd) || !IsWindowVisible(gameHwnd) || nowMs >= nextFindMs) {
            HWND found = FindVisibleWindowByPid(gamePid);
            if (found)
                gameHwnd = found;
            nextFindMs = nowMs + 500;
        }

        RECT gr{};
        if (GetWindowRect(gameHwnd, &gr)) {
            const int w = gr.right - gr.left;
            const int h = gr.bottom - gr.top;
            const bool moved = gr.left != lastRect.left || gr.top != lastRect.top ||
                               gr.right != lastRect.right || gr.bottom != lastRect.bottom;
            if (w > 0 && h > 0 && (moved || nowMs >= nextTopMs)) {
                UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
                if (!moved)
                    flags |= SWP_NOMOVE | SWP_NOSIZE;
                SetWindowPos(hwnd, HWND_TOPMOST, gr.left, gr.top, w, h, flags);
                nextTopMs = nowMs + 200;
                if (moved) {
                    lastRect = gr;
                    overlayW = w;
                    overlayH = h;
                    UpdateViewportSize(w, h);
                }
            }
        }

        LARGE_INTEGER qpcNow{};
        QueryPerformanceCounter(&qpcNow);
        float dt = 1.f / 144.f;
        if (qpf.QuadPart)
            dt = static_cast<float>(static_cast<double>(qpcNow.QuadPart - qpcFrame.QuadPart) /
                                    static_cast<double>(qpf.QuadPart));
        qpcFrame = qpcNow;
        if (!(dt > 0.f && dt < 0.1f))
            dt = 1.f / 144.f;

        auto& io = ImGui::GetIO();
        io.DeltaTime = dt;
        OverlayPumpThreadMessages(msg);
        OverlayPollInsertMenuToggle();
        OverlaySyncPassthroughStyle(hwnd);
        const bool shotKeyDown = (GetAsyncKeyState(VK_SNAPSHOT) & 0x8000) != 0 ||
                                 (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                                 (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
        const bool drawCursor = Settings::Menu && !shotKeyDown;
        io.MouseDrawCursor = drawCursor;
        if ((drawCursor ? 1 : 0) != cursorMode) {
            ShowCursor(drawCursor ? FALSE : TRUE);
            cursorMode = drawCursor ? 1 : 0;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (Settings::Menu)
            OverlayFeedMenuInput(hwnd, io);
        ImGui::NewFrame();
        {
            try {
                ActorLoop();
            } catch (...) {
            }
            OverlayPumpThreadMessages(msg);
            OverlayPollInsertMenuToggle();
            OverlaySyncPassthroughStyle(hwnd);
            g_overlayLastFrameMenuRendered = false;
            if (Settings::Menu) {
                FeedInput();
                Menu();
                g_overlayLastFrameMenuRendered = true;
            }
            CaptureMenuHitRects();
        }
        ImGui::Render();
        device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
        device_context->ClearRenderTargetView(render_target_view, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        UINT presentFlags = 0;
        if (!Settings::VSync) {
            presentFlags = DXGI_PRESENT_DO_NOT_WAIT;
            if (g_overlayAllowTearing)
                presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }
        const HRESULT presentHr = swap_chain->Present(Settings::VSync ? 1 : 0, presentFlags);
        if (presentHr == DXGI_ERROR_WAS_STILL_DRAWING)
            Sleep(1);
    }

    {
        std::ostringstream oss;
        if (overlayFrames <= 1) {
            oss << xorstr_("Overlay message loop exited immediately (WM_QUIT after ")
                << overlayFrames << xorstr_(" frame(s); msg=0x")
                << std::hex << std::uppercase << msg.message << std::dec << ')';
        } else {
            oss << xorstr_("Overlay closed (WM_QUIT after ") << overlayFrames
                << xorstr_(" frames)");
        }
        OverlayLogLine(oss.str(), overlayFrames <= 1);
    }

    if (g_insertHotkeyRegistered) {
        UnregisterHotKey(nullptr, kInsertMenuHotkeyId);
        UnregisterHotKey(hwnd, kInsertMenuHotkeyId);
        g_insertHotkeyRegistered = false;
    }
    g_overlayHwnd = nullptr;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (render_target_view) render_target_view->Release();
    if (swap_chain) swap_chain->Release();
    if (device_context) device_context->Release();
    if (device) device->Release();

    DestroyWindow(hwnd);
    return true;
}
