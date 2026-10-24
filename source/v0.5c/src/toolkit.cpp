#include "toolkit.h"
#include "northlight_lights.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>

#include <atomic>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <cstdarg>
#include <cstring>
#include <cfloat>
#include <utility>
#include <sstream>
#include <algorithm>
#include <iomanip>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace ctl {

static HMODULE g_self{};
static std::atomic_bool g_running{false};
static std::atomic_bool g_uiVisible{true};
static uintptr_t g_controlBase{}, g_rendererBase{}, g_appBase{}, g_coreBase{}, g_anchor{};
// Filled directly by the native equivalent of the validated CE ControlPMWriter hook.
// Volatile is intentional here: the generated code cave writes these values directly.
alignas(8) static volatile uintptr_t g_cameraBase{};
alignas(8) static volatile uint64_t g_pmWriterCounter{};
static bool g_igcsEnabled{true};
static bool g_sessionActive{};
static uint64_t g_cameraSeq{};
static std::atomic_bool g_cameraPipeConnected{false};
static std::atomic_bool g_commandPipeConnected{false};
static HANDLE g_cameraPipeThread{};
static HANDLE g_commandPipeThread{};
static float g_dofScaleLR=0.007f, g_dofScaleUD=0.007f;

struct Camera { float x{},y{},z{},yaw{},pitch{},roll{},fov{37.0f}; };
static Camera g_savedCamera{};

// Live Photo Mode FOV pointer. Captured from UIOptionsModel::onFloatPropertySet.
// The pointer becomes available as soon as CONTROL writes the Photo Mode FOV once.
static std::atomic_uintptr_t g_fovDegPtr{};
static uintptr_t g_fovPtrHookTarget{};
static void* g_fovPtrHookCave{};
static uint8_t g_fovPtrHookOriginal[5]{};
static bool g_fovPtrHookInstalled{};

// -----------------------------------------------------------------------------
// Camera Path
// -----------------------------------------------------------------------------
struct CameraPathNode
{
    Camera cam{};
    float secondsToNext{2.0f};
    int framesToNext{120};
};

static std::vector<CameraPathNode> g_pathNodes{};
static int g_pathSelected{};
static std::atomic_bool g_pathPlaying{false};
static bool g_pathLoop{};
static bool g_pathFrameMode{};
static float g_pathEaseInPct{20.0f};
static float g_pathEaseOutPct{20.0f};
static int g_pathSegment{};
static uint64_t g_pathSegmentStartMs{};
static int g_pathFrameInSegment{};
static std::string g_pathStatus{"Idle"};


// -----------------------------------------------------------------------------
// Configurable hotkeys
// -----------------------------------------------------------------------------
enum class HotkeyAction : int
{
    ToggleUI = 0,
    PathAddNode,
    PathUpdateNode,
    PathGoToNode,
    PathDeleteNode,
    PathPreviousNode,
    PathNextNode,
    PathPlayStop,
    PathClear,
    PathSave,
    PathLoad,
    Count
};

struct HotkeyBinding
{
    HotkeyAction action{};
    const char* label{};
    const char* keyName{};
    int vk{};
};

static std::array<HotkeyBinding, static_cast<size_t>(HotkeyAction::Count)> g_hotkeys{{
    { HotkeyAction::ToggleUI,         "Show / Hide Toolkit", "ToggleUI",         VK_F10 },
    { HotkeyAction::PathAddNode,      "Add Node",            "PathAddNode",      VK_F5 },
    { HotkeyAction::PathUpdateNode,   "Update Node",         "PathUpdateNode",   0 },
    { HotkeyAction::PathGoToNode,     "Go To Node",          "PathGoToNode",     0 },
    { HotkeyAction::PathDeleteNode,   "Delete Node",         "PathDeleteNode",   VK_F6 },
    { HotkeyAction::PathPreviousNode, "Previous Node",       "PathPreviousNode", VK_F7 },
    { HotkeyAction::PathNextNode,     "Next Node",           "PathNextNode",     VK_F8 },
    { HotkeyAction::PathPlayStop,     "Play / Stop",         "PathPlayStop",     VK_F9 },
    { HotkeyAction::PathClear,        "Clear Path",          "PathClear",        0 },
    { HotkeyAction::PathSave,         "Save Path",           "PathSave",         VK_F11 },
    { HotkeyAction::PathLoad,         "Load Path",           "PathLoad",         VK_F12 },
}};

static int g_hotkeyCaptureIndex = -1;
static std::string g_hotkeyStatus{"Ready"};

static HotkeyBinding& Hotkey(HotkeyAction a)
{
    return g_hotkeys[static_cast<size_t>(a)];
}

static std::string HotkeyConfigFileName()
{
    return "ControlToolkit.ini";
}

static const char* VkName(int vk)
{
    switch (vk)
    {
    case 0: return "Unbound";
    case VK_ESCAPE: return "Escape";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SPACE: return "Space";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_ADD: return "Numpad +";
    case VK_SUBTRACT: return "Numpad -";
    case VK_MULTIPLY: return "Numpad *";
    case VK_DIVIDE: return "Numpad /";
    case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3"; case VK_F4: return "F4";
    case VK_F5: return "F5"; case VK_F6: return "F6"; case VK_F7: return "F7"; case VK_F8: return "F8";
    case VK_F9: return "F9"; case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
    default: break;
    }
    static thread_local char buf[24];
    if (vk >= 'A' && vk <= 'Z') { buf[0] = static_cast<char>(vk); buf[1] = 0; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = static_cast<char>(vk); buf[1] = 0; return buf; }
    std::snprintf(buf, sizeof(buf), "VK 0x%02X", vk & 0xFF);
    return buf;
}

static bool HotkeyConflict(size_t idx)
{
    if (idx >= g_hotkeys.size()) return false;
    const int vk = g_hotkeys[idx].vk;
    if (!vk) return false;
    for (size_t i = 0; i < g_hotkeys.size(); ++i)
        if (i != idx && g_hotkeys[i].vk == vk) return true;
    return false;
}

static void ResetHotkeysToDefaults()
{
    Hotkey(HotkeyAction::ToggleUI).vk = VK_F10;
    Hotkey(HotkeyAction::PathAddNode).vk = VK_F5;
    Hotkey(HotkeyAction::PathUpdateNode).vk = 0;
    Hotkey(HotkeyAction::PathGoToNode).vk = 0;
    Hotkey(HotkeyAction::PathDeleteNode).vk = VK_F6;
    Hotkey(HotkeyAction::PathPreviousNode).vk = VK_F7;
    Hotkey(HotkeyAction::PathNextNode).vk = VK_F8;
    Hotkey(HotkeyAction::PathPlayStop).vk = VK_F9;
    Hotkey(HotkeyAction::PathClear).vk = 0;
    Hotkey(HotkeyAction::PathSave).vk = VK_F11;
    Hotkey(HotkeyAction::PathLoad).vk = VK_F12;
    g_hotkeyStatus = "Defaults restored";
}

static void SaveHotkeys()
{
    std::ofstream f(HotkeyConfigFileName(), std::ios::trunc);
    if (!f) { g_hotkeyStatus = "Save failed"; return; }
    f << "[Keybinds]\n";
    for (const auto& h : g_hotkeys) f << h.keyName << '=' << h.vk << '\n';
    g_hotkeyStatus = "Saved ControlToolkit.ini";
}

static void LoadHotkeys()
{
    std::ifstream f(HotkeyConfigFileName());
    if (!f) { g_hotkeyStatus = "Using defaults (no ControlToolkit.ini)"; return; }
    std::string line;
    bool inSection = false;
    while (std::getline(f, line))
    {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']')
        {
            inSection = (line == "[Keybinds]");
            continue;
        }
        if (!inSection) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        int vk = 0;
        try { vk = std::stoi(line.substr(eq + 1)); } catch (...) { continue; }
        if (vk < 0 || vk > 255) continue;
        for (auto& h : g_hotkeys)
            if (key == h.keyName) { h.vk = vk; break; }
    }
    g_hotkeyStatus = "Loaded ControlToolkit.ini";
}

struct Param { const char* page; const char* name; intptr_t offset; bool integer; float minv,maxv,step; };
static const Param kParams[] = {
    {"Post Process","Global Exposure",-0xE52,false,-1,35,0.05f},
    {"Post Process","Radial Blur",0x1861CE,false,0,1,0.01f},
    {"Post Process","Vignette",0x184CDE,false,0,1,0.01f},
    {"Post Process","Bloom Scale Y",-0x36E,false,0,1,0.01f}, {"Post Process","Bloom Scale X",-0x36A,false,0,1,0.01f},
    {"Post Process","Bloom Intensity",-0x572,false,0,20,0.05f}, {"Post Process","Bloom Threshold",-0x672,false,0,1,0.01f},
    {"Post Process","Bloom Lens Dirt",-0x472,false,0,20,0.05f}, {"Post Process","Bloom Scatter",-0x772,false,0,1,0.01f},
};

struct RenderingToggle { const char* name; intptr_t offset; int offValue; int onValue; };
static const RenderingToggle kRenderingToggles[] = {
    {"Render SS Diffuse", -0x101B36, 0, 65536},
    {"Force Dynamic",     -0x101B1A, 0, 256},
    {"Cinematic Mode",    -0x101B3E, 1, 257},
};

// -----------------------------------------------------------------------------
// Per-Photo-Mode session snapshot / reset
// -----------------------------------------------------------------------------
static constexpr size_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);
static constexpr size_t kRenderingCount = sizeof(kRenderingToggles) / sizeof(kRenderingToggles[0]);

struct PatchSnapshot
{
    std::vector<uint8_t> bytes{};
    bool valid{};
};

struct SessionSnapshot
{
    std::array<uint32_t, kParamCount> params{};
    std::array<bool, kParamCount> paramValid{};
    std::array<int, kRenderingCount> rendering{};
    std::array<bool, kRenderingCount> renderingValid{};

    // Exact bytes as they existed when Photo Mode opened. This matters for
    // patches because the state at PM entry may not match the bytes captured
    // when the DLL itself was first injected.
    PatchSnapshot resizePatch{};
    PatchSnapshot photoRangePatch{};
    PatchSnapshot photoNopPatch{};
    PatchSnapshot hotPatch{};

    bool allowResize{};
    bool photo100{};
    bool hotsampling{};
    bool valid{};
};

static SessionSnapshot g_pmSnapshot{};
static std::mutex g_snapshotMutex;
static const char* g_snapshotSource = "none";
static bool g_autoRestoreOnPmExit = true;
static bool g_pmWasActive = false;
static uintptr_t g_photoModeStateAddr{};
static uint32_t g_photoModeStateValue{};


static void EnsureDebugConsole()
{
    if (GetConsoleWindow()) return;
    if (!AllocConsole()) return;

    FILE* fOut = nullptr;
    FILE* fErr = nullptr;
    freopen_s(&fOut, "CONOUT$", "w", stdout);
    freopen_s(&fErr, "CONOUT$", "w", stderr);
    SetConsoleTitleA("CONTROL Toolkit - Photo Mode Debug");
    printf("[ControlToolkit] debug console ready\n");
    fflush(stdout);
}

static void ConsolePMState(bool open)
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    printf("[%02u:%02u:%02u.%03u] [PHOTO MODE] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        open ? "OPEN" : "CLOSED");
    fflush(stdout);
}

static void Log(const char* fmt,...) {
    char b[1024]{}; va_list va; va_start(va,fmt); vsnprintf_s(b,sizeof(b),_TRUNCATE,fmt,va); va_end(va);
    wchar_t temp[MAX_PATH]{}; if(GetTempPathW(MAX_PATH,temp)){ std::wstring p=std::wstring(temp)+L"ControlToolkit.log"; FILE* f{}; _wfopen_s(&f,p.c_str(),L"at, ccs=UTF-8"); if(f){ SYSTEMTIME st{};GetLocalTime(&st);fwprintf(f,L"[%02u:%02u:%02u.%03u] %S\n",st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,b);fclose(f);} }
    OutputDebugStringA("[ControlToolkit] "); OutputDebugStringA(b); OutputDebugStringA("\n");
}
static uintptr_t ModuleBase(const wchar_t* n){return reinterpret_cast<uintptr_t>(GetModuleHandleW(n));}
static std::vector<uint8_t> Pattern(const char* s){std::vector<uint8_t>b;while(*s){while(*s==' ')++s;if(!*s)break;unsigned v{};sscanf_s(s,"%2x",&v);b.push_back((uint8_t)v);while(*s&&*s!=' ')++s;}return b;}
static uintptr_t FindPattern(HMODULE mod,const char* sig){if(!mod)return 0;auto*dos=(IMAGE_DOS_HEADER*)mod;auto*nt=(IMAGE_NT_HEADERS*)((uint8_t*)mod+dos->e_lfanew);size_t sz=nt->OptionalHeader.SizeOfImage;auto p=Pattern(sig);auto*base=(uint8_t*)mod;for(size_t i=0;i+p.size()<=sz;i++){bool ok=true;for(size_t j=0;j<p.size();j++)if(base[i+j]!=p[j]){ok=false;break;}if(ok)return(uintptr_t)(base+i);}return 0;}
static uintptr_t FindMasked(HMODULE mod,const uint8_t* bytes,const char* mask,size_t n){if(!mod)return 0;auto*dos=(IMAGE_DOS_HEADER*)mod;auto*nt=(IMAGE_NT_HEADERS*)((uint8_t*)mod+dos->e_lfanew);size_t sz=nt->OptionalHeader.SizeOfImage;auto*base=(uint8_t*)mod;for(size_t i=0;i+n<=sz;i++){bool ok=true;for(size_t j=0;j<n;j++)if(mask[j]=='x'&&base[i+j]!=bytes[j]){ok=false;break;}if(ok)return(uintptr_t)(base+i);}return 0;}
template<class T> static bool SafeRead(uintptr_t a,T& o){__try{o=*(T*)a;return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T> static bool SafeWrite(uintptr_t a,const T& v){DWORD old{};if(!VirtualProtect((void*)a,sizeof(T),PAGE_EXECUTE_READWRITE,&old))return false;__try{*(T*)a=v;}__except(EXCEPTION_EXECUTE_HANDLER){VirtualProtect((void*)a,sizeof(T),old,&old);return false;}VirtualProtect((void*)a,sizeof(T),old,&old);return true;}
static bool ReadBytes(uintptr_t a,std::vector<uint8_t>& o,size_t n){o.resize(n);__try{memcpy(o.data(),(void*)a,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool Patch(uintptr_t a,const void* data,size_t n){DWORD old{};if(!a||!VirtualProtect((void*)a,n,PAGE_EXECUTE_READWRITE,&old))return false;memcpy((void*)a,data,n);VirtualProtect((void*)a,n,old,&old);FlushInstructionCache(GetCurrentProcess(),(void*)a,n);return true;}

struct MemPatch { uintptr_t address{}; std::vector<uint8_t> original, patched; bool captured{}; bool enabled{}; };
static MemPatch g_resizePatch, g_photoRangePatch, g_photoNopPatch, g_hotPatch;
static bool g_allowResize=true,g_photo100=true,g_hotsampling=true;
static void InitPatch(MemPatch& p,uintptr_t a,std::initializer_list<uint8_t> bytes){p.address=a;p.patched.assign(bytes);if(a&&ReadBytes(a,p.original,p.patched.size()))p.captured=true;}
static void SetPatch(MemPatch& p,bool en){if(!p.captured)return;Patch(p.address,en?p.patched.data():p.original.data(),p.patched.size());p.enabled=en;}

static bool CapturePatchSnapshot(const MemPatch& patch, PatchSnapshot& out)
{
    out = {};
    if (!patch.captured || !patch.address || patch.original.empty()) return false;
    if (!ReadBytes(patch.address, out.bytes, patch.original.size())) return false;
    out.valid = true;
    return true;
}

static bool RestorePatchSnapshot(MemPatch& patch, const PatchSnapshot& snap)
{
    if (!snap.valid || !patch.address || snap.bytes.empty()) return true;
    const bool ok = Patch(patch.address, snap.bytes.data(), snap.bytes.size());
    if (ok)
        patch.enabled = (snap.bytes == patch.patched);
    return ok;
}

static bool CaptureSessionSnapshot(const char* source = "unknown")
{
    std::lock_guard<std::mutex> lock(g_snapshotMutex);

    SessionSnapshot snap{};
    bool capturedAnything = false;

    if (g_anchor)
    {
        for (size_t i = 0; i < kParamCount; ++i)
        {
            uint32_t raw{};
            if (SafeRead(g_anchor + kParams[i].offset, raw))
            {
                snap.params[i] = raw;
                snap.paramValid[i] = true;
                capturedAnything = true;
            }
            else
            {
                Log("PM snapshot: read failed at %s / %s", kParams[i].page, kParams[i].name);
            }
        }

        for (size_t i = 0; i < kRenderingCount; ++i)
        {
            int v{};
            if (SafeRead(g_anchor + kRenderingToggles[i].offset, v))
            {
                snap.rendering[i] = v;
                snap.renderingValid[i] = true;
                capturedAnything = true;
            }
            else
            {
                Log("PM snapshot: read failed at rendering %s", kRenderingToggles[i].name);
            }
        }
    }
    else
    {
        Log("PM snapshot: CONTROL parameter anchor unavailable");
    }

    capturedAnything = CapturePatchSnapshot(g_resizePatch, snap.resizePatch) || capturedAnything;
    capturedAnything = CapturePatchSnapshot(g_photoRangePatch, snap.photoRangePatch) || capturedAnything;
    capturedAnything = CapturePatchSnapshot(g_photoNopPatch, snap.photoNopPatch) || capturedAnything;
    capturedAnything = CapturePatchSnapshot(g_hotPatch, snap.hotPatch) || capturedAnything;

    // Keep the UI state too, but restore the exact bytes above rather than
    // reconstructing them from these booleans.
    snap.allowResize = g_allowResize;
    snap.photo100 = g_photo100;
    snap.hotsampling = g_hotsampling;
    snap.valid = capturedAnything;

    if (!snap.valid)
    {
        Log("PM session snapshot failed: nothing could be captured");
        return false;
    }

    g_pmSnapshot = std::move(snap);
    g_snapshotSource = source ? source : "unknown";
    Log("PM session FULL snapshot captured (%s)", g_snapshotSource);
    return true;
}

static bool RestoreSessionSnapshot(const char* reason, bool invalidateAfterRestore)
{
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    if (!g_pmSnapshot.valid) return false;

    bool ok = true;

    if (g_anchor)
    {
        for (size_t i = 0; i < kParamCount; ++i)
            if (g_pmSnapshot.paramValid[i])
                ok = SafeWrite(g_anchor + kParams[i].offset, g_pmSnapshot.params[i]) && ok;

        for (size_t i = 0; i < kRenderingCount; ++i)
            if (g_pmSnapshot.renderingValid[i])
                ok = SafeWrite(g_anchor + kRenderingToggles[i].offset, g_pmSnapshot.rendering[i]) && ok;
    }
    else
    {
        ok = false;
        Log("Reset All: CONTROL parameter anchor unavailable during restore");
    }

    // Restore exact entry-time patch bytes, not DLL-load defaults.
    ok = RestorePatchSnapshot(g_resizePatch, g_pmSnapshot.resizePatch) && ok;
    ok = RestorePatchSnapshot(g_photoRangePatch, g_pmSnapshot.photoRangePatch) && ok;
    ok = RestorePatchSnapshot(g_photoNopPatch, g_pmSnapshot.photoNopPatch) && ok;
    ok = RestorePatchSnapshot(g_hotPatch, g_pmSnapshot.hotPatch) && ok;

    g_allowResize = g_pmSnapshot.allowResize;
    g_photo100 = g_pmSnapshot.photo100;
    g_hotsampling = g_pmSnapshot.hotsampling;

    Log("Reset All: FULL PM session snapshot restored (%s)%s",
        reason ? reason : "manual", ok ? "" : " with one or more restore failures");

    if (invalidateAfterRestore)
    {
        g_pmSnapshot = {};
        g_snapshotSource = "none";
    }
    return ok;
}

static bool HasSessionSnapshot()
{
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    return g_pmSnapshot.valid;
}

static bool EnsureSessionSnapshot(const char* source)
{
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        if (g_pmSnapshot.valid) return true;
    }
    return CaptureSessionSnapshot(source);
}

static const char* SnapshotStatus()
{
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    if (!g_pmSnapshot.valid) return "Waiting for Photo Mode";
    return g_snapshotSource;
}

static bool ResolveNativePhotoModeState()
{
    g_photoModeStateAddr = 0;
    if (!g_controlBase) return false;

    // Native implementation reached through GameHelper::isPhotoModeActive():
    //   cmp dword ptr [rip+disp32],04
    //   sete al
    //   ret
    // This signature is unique in the supplied CONTROL DX12 executable.
    const uint8_t pat[] = { 0x83,0x3D,0,0,0,0,0x04,0x0F,0x94,0xC0,0xC3 };
    const char* mask = "xx????xxxxx";
    const uintptr_t fn = FindMasked((HMODULE)g_controlBase, pat, mask, sizeof(pat));
    if (!fn)
    {
        Log("Native Photo Mode state resolver FAILED");
        return false;
    }

    int32_t disp{};
    if (!SafeRead(fn + 2, disp))
    {
        Log("Native Photo Mode state resolver: RIP displacement read failed");
        return false;
    }

    g_photoModeStateAddr = fn + 7 + static_cast<intptr_t>(disp);
    uint32_t initial{};
    if (!SafeRead(g_photoModeStateAddr, initial))
    {
        Log("Native Photo Mode state resolver: state read failed at %p", (void*)g_photoModeStateAddr);
        g_photoModeStateAddr = 0;
        return false;
    }

    g_photoModeStateValue = initial;
    Log("Native Photo Mode state ready: fn=%p state=%p initial=%u",
        (void*)fn, (void*)g_photoModeStateAddr, initial);
    return true;
}

static void StopCameraPath(const char* reason);

static bool IsNativePhotoModeActive()
{
    if (!g_photoModeStateAddr) return false;
    uint32_t value{};
    if (!SafeRead(g_photoModeStateAddr, value)) return false;
    g_photoModeStateValue = value;
    return value == 4;
}

static void UpdatePhotoModeSessionState()
{
    const bool active = IsNativePhotoModeActive();

    if (active && !g_pmWasActive)
    {
        // Capture before any Toolkit write in this PM session.
        ConsolePMState(true);
        if (CaptureSessionSnapshot("native Photo Mode OPEN"))
            Log("Photo Mode entered (native state == 4)");
    }
    else if (!active && g_pmWasActive)
    {
        g_pathPlaying = false;
        ConsolePMState(false);
        Log("Photo Mode exited (native state != 4)");

        // Full reset on PM exit: all captured tweak/light/render values plus
        // exact entry-time bytes for the Toolkit patches.
        if (g_autoRestoreOnPmExit)
            RestoreSessionSnapshot("Photo Mode exit", true);
        else
        {
            std::lock_guard<std::mutex> lock(g_snapshotMutex);
            g_pmSnapshot = {};
            g_snapshotSource = "none";
        }

        // Camera capture belongs to IGCSDOF only; do not use it as PM state.
        g_cameraBase = 0;
    }

    g_pmWasActive = active;
}
static void Resolve(){g_controlBase=ModuleBase(L"Control_DX12.exe");g_rendererBase=ModuleBase(L"renderer_rmdwin10_f.dll");g_appBase=ModuleBase(L"app_rmdwin10_f.dll");g_coreBase=ModuleBase(L"coregame_rmdwin10_f.dll");if(g_rendererBase)g_anchor=FindPattern((HMODULE)g_rendererBase,"30 3A 49 6E 74 65 6E 73 69 74 79 00 00 00 00 00 00 00 00");ResolveNativePhotoModeState();}
static void InitCTPatches(){
    if(g_appBase)InitPatch(g_resizePatch,g_appBase+0x20D20,{1,0});
    if(g_controlBase){InitPatch(g_photoRangePatch,g_controlBase+0x12AC400,{0x00,0x00,0xC8,0x42,0x00,0x00});InitPatch(g_photoNopPatch,g_controlBase+0x3DC6FC,{0x90,0x90,0x90,0x90});}
    if(g_coreBase){const uint8_t pat[]={0x48,0x8B,0xC8,0xBA,0x03,0,0,0,0xFF,0x15,0,0,0,0,0x41,0xB4,0x01};const char*mask="xxxxxxxxxx????xxx";auto a=FindMasked((HMODULE)g_coreBase,pat,mask,sizeof(pat));if(a)InitPatch(g_hotPatch,a+8,{0x90,0x90,0x90,0x90,0x90,0x90});}
    // Important: only capture original bytes here. Do NOT alter the game at DLL load.
    // The first user change / PM-open snapshot must see the real starting values.
}

// Camera capture for IGCSDOF only: native equivalent of the validated CE script:
//   aobscanmodule(ControlPMWriter,Control_DX12.exe,0F 11 8E 80 00 00 00 0F 28)
//   pControlPM = RSI + 0x80
// No VEH / INT3 / debugger breakpoint is used here.
static uintptr_t g_camWriter{};
static void* g_camCave{};
static uint8_t g_camOrig[7]{};
static bool g_camHookInstalled{};
static const char* g_pmHookMode = "not installed";

static void* AllocNear(uintptr_t target, size_t size)
{
    const uintptr_t range = 0x70000000ull;
    const uintptr_t gran = 0x10000ull;
    const uintptr_t start = target & ~(gran - 1ull);

    for (uintptr_t off = gran; off < range; off += gran)
    {
        const uintptr_t down = start > off ? start - off : 0;
        if (down)
            if (void* p = VirtualAlloc(reinterpret_cast<void*>(down), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
                return p;

        const uintptr_t up = start + off;
        if (void* p = VirtualAlloc(reinterpret_cast<void*>(up), size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))
            return p;
    }
    return nullptr;
}

static bool EnableFovCapture()
{
    if (g_fovPtrHookInstalled) return true;
    if (!g_coreBase) return false;

    // UIOptionsModel::onFloatPropertySet: movss [rax+48],xmm2
    const uint8_t pat[] = { 0xF3,0x0F,0x11,0x50,0x48 };
    const char* mask = "xxxxx";
    const uintptr_t target = FindMasked((HMODULE)g_coreBase, pat, mask, sizeof(pat));
    if (!target)
    {
        Log("Photo Mode FOV pointer hook pattern not found");
        return false;
    }

    void* cave = AllocNear(target, 96);
    if (!cave)
    {
        Log("Photo Mode FOV pointer hook alloc-near failed");
        return false;
    }

    memcpy(g_fovPtrHookOriginal, reinterpret_cast<void*>(target), sizeof(g_fovPtrHookOriginal));

    auto* c = reinterpret_cast<uint8_t*>(cave);
    size_t i = 0;

    // Preserve RAX, capture RAX+0x48 into g_fovDegPtr, then execute original writer.
    c[i++] = 0x50; // push rax
    c[i++] = 0x48; c[i++] = 0x83; c[i++] = 0xC0; c[i++] = 0x48; // add rax,48
    c[i++] = 0x49; c[i++] = 0xBB;
    *reinterpret_cast<uint64_t*>(c + i) = reinterpret_cast<uint64_t>(&g_fovDegPtr); i += 8;
    c[i++] = 0x49; c[i++] = 0x89; c[i++] = 0x03; // mov [r11],rax
    c[i++] = 0x58; // pop rax
    c[i++] = 0xF3; c[i++] = 0x0F; c[i++] = 0x11; c[i++] = 0x50; c[i++] = 0x48;

    c[i++] = 0xFF; c[i++] = 0x25;
    c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00;
    *reinterpret_cast<uint64_t*>(c + i) = target + 5; i += 8;

    const intptr_t rel = reinterpret_cast<uintptr_t>(cave) - (target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX)
    {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("Photo Mode FOV pointer hook cave too far");
        return false;
    }

    uint8_t patch[5]{};
    patch[0] = 0xE9;
    *reinterpret_cast<int32_t*>(patch + 1) = static_cast<int32_t>(rel);
    if (!Patch(target, patch, sizeof(patch)))
    {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("Photo Mode FOV pointer hook patch failed");
        return false;
    }

    g_fovPtrHookTarget = target;
    g_fovPtrHookCave = cave;
    g_fovPtrHookInstalled = true;
    Log("Photo Mode FOV pointer hook installed target=%p cave=%p",
        reinterpret_cast<void*>(target), cave);
    return true;
}

static void DisableFovCapture()
{
    if (g_fovPtrHookInstalled && g_fovPtrHookTarget)
        Patch(g_fovPtrHookTarget, g_fovPtrHookOriginal, sizeof(g_fovPtrHookOriginal));

    g_fovPtrHookInstalled = false;
    g_fovPtrHookTarget = 0;
    g_fovDegPtr = 0;
    if (g_fovPtrHookCave)
    {
        VirtualFree(g_fovPtrHookCave, 0, MEM_RELEASE);
        g_fovPtrHookCave = nullptr;
    }
}

static bool EnableCameraCapture()
{
    if (g_camHookInstalled) return true;
    if (!g_controlBase) return false;

    // Preferred path: untouched game code, same AOB as the validated CE script.
    uintptr_t target = FindPattern((HMODULE)g_controlBase, "0F 11 8E 80 00 00 00 0F 28");
    uintptr_t chainTarget = 0;
    bool chainedExistingHook = false;

    if (!target)
    {
        // If the CE IGCSDOF entry was enabled before this DLL loaded, CE already
        // replaced the 7-byte writer with E9 rel32 + NOP NOP. In that case the
        // original AOB cannot be found. Chain in front of that existing hook.
        // RVA 0x185E5F is the validated writer RVA for this CONTROL build.
        const uintptr_t fallback = g_controlBase + 0x185E5F;
        uint8_t cur[7]{};
        __try { memcpy(cur, reinterpret_cast<void*>(fallback), sizeof(cur)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { memset(cur, 0, sizeof(cur)); }

        if (cur[0] == 0xE9)
        {
            const int32_t rel = *reinterpret_cast<int32_t*>(cur + 1);
            chainTarget = fallback + 5 + rel;
            target = fallback;
            chainedExistingHook = true;
            Log("ControlPMWriter original AOB already patched; chaining existing hook target=%p -> %p",
                reinterpret_cast<void*>(target), reinterpret_cast<void*>(chainTarget));
        }
        else
        {
            Log("ControlPMWriter AOB not found; fallback bytes at +185E5F = %02X %02X %02X %02X %02X %02X %02X",
                cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6]);
            g_pmHookMode = "unavailable";
            return false;
        }
    }

    void* cave = AllocNear(target, 128);
    if (!cave)
    {
        Log("ControlPMWriter alloc-near failed");
        g_pmHookMode = "alloc failed";
        return false;
    }

    memcpy(g_camOrig, reinterpret_cast<void*>(target), sizeof(g_camOrig));

    auto* c = reinterpret_cast<uint8_t*>(cave);
    size_t i = 0;

    // Preserve flags + RAX used by the instrumentation.
    c[i++] = 0x9C; // pushfq
    c[i++] = 0x50; // push rax

    // lea rax,[rsi+00000080] : exact pControlPM from the CE script.
    c[i++] = 0x48; c[i++] = 0x8D; c[i++] = 0x86;
    *reinterpret_cast<uint32_t*>(c + i) = 0x80; i += 4;

    // mov [absolute &g_cameraBase],rax
    c[i++] = 0x48; c[i++] = 0xA3;
    *reinterpret_cast<uint64_t*>(c + i) = reinterpret_cast<uint64_t>(&g_cameraBase); i += 8;

    // Heartbeat: lock inc qword ptr [&g_pmWriterCounter].
    c[i++] = 0x48; c[i++] = 0xB8;
    *reinterpret_cast<uint64_t*>(c + i) = reinterpret_cast<uint64_t>(&g_pmWriterCounter); i += 8;
    c[i++] = 0xF0; c[i++] = 0x48; c[i++] = 0xFF; c[i++] = 0x00;

    c[i++] = 0x58; // pop rax
    c[i++] = 0x9D; // popfq

    if (chainedExistingHook)
    {
        // Do NOT execute the patched E9 bytes as data. Continue into CE's existing
        // cave; that cave executes the original movups and returns to target+7.
        c[i++] = 0xFF; c[i++] = 0x25;
        c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00;
        *reinterpret_cast<uint64_t*>(c + i) = chainTarget; i += 8;
        g_pmHookMode = "chained CE hook";
    }
    else
    {
        // Untouched writer: execute original 7-byte instruction ourselves.
        memcpy(c + i, g_camOrig, sizeof(g_camOrig)); i += sizeof(g_camOrig);

        c[i++] = 0xFF; c[i++] = 0x25;
        c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00; c[i++] = 0x00;
        *reinterpret_cast<uint64_t*>(c + i) = target + 7; i += 8;
        g_pmHookMode = "native writer";
    }

    const intptr_t rel = reinterpret_cast<uintptr_t>(cave) - (target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX)
    {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("ControlPMWriter cave too far");
        g_pmHookMode = "cave too far";
        return false;
    }

    uint8_t patch[7]{};
    patch[0] = 0xE9;
    *reinterpret_cast<int32_t*>(patch + 1) = static_cast<int32_t>(rel);
    patch[5] = 0x90;
    patch[6] = 0x90;

    if (!Patch(target, patch, sizeof(patch)))
    {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("ControlPMWriter patch failed");
        g_pmHookMode = "patch failed";
        return false;
    }

    g_camWriter = target;
    g_camCave = cave;
    g_camHookInstalled = true;
    Log("ControlPMWriter hook installed mode=%s target=%p cave=%p",
        g_pmHookMode, reinterpret_cast<void*>(target), cave);
    return true;
}

static void DisableCameraCapture()
{
    if (g_camHookInstalled && g_camWriter)
        Patch(g_camWriter, g_camOrig, sizeof(g_camOrig));

    g_camHookInstalled = false;
    g_camWriter = 0;
    g_cameraBase = 0;
    g_pmWriterCounter = 0;
    g_pmHookMode = "not installed";

    if (g_camCave)
    {
        VirtualFree(g_camCave, 0, MEM_RELEASE);
        g_camCave = nullptr;
    }
}
static bool ReadCamera(Camera& c)
{
    const uintptr_t b = (uintptr_t)g_cameraBase;
    if (!b) return false;
    const bool ok = SafeRead(b+8,c.x) && SafeRead(b,c.y) && SafeRead(b+4,c.z) &&
                    SafeRead(b+0x10,c.yaw) && SafeRead(b+0x14,c.pitch) && SafeRead(b+0x18,c.roll);
    const uintptr_t fovPtr = g_fovDegPtr.load();
    if (fovPtr)
    {
        float f{};
        if (SafeRead(fovPtr, f) && std::isfinite(f) && f > 0.1f && f < 179.0f)
            c.fov = f;
    }
    return ok;
}

static bool WriteCameraRaw(const Camera& c)
{
    const uintptr_t b = (uintptr_t)g_cameraBase;
    if (!b) return false;
    bool ok = true;
    ok = SafeWrite(b+8, c.x) && ok;
    ok = SafeWrite(b+0, c.y) && ok;
    ok = SafeWrite(b+4, c.z) && ok;
    ok = SafeWrite(b+0x10, c.yaw) && ok;
    ok = SafeWrite(b+0x14, c.pitch) && ok;
    ok = SafeWrite(b+0x18, c.roll) && ok;
    const uintptr_t fovPtr = g_fovDegPtr.load();
    if (fovPtr && c.fov > 0.1f && c.fov < 179.0f)
        ok = SafeWrite(fovPtr, c.fov) && ok;
    return ok;
}

static bool WriteCameraPositionRaw(float x, float y, float z)
{
    const uintptr_t b = (uintptr_t)g_cameraBase;
    if (!b) return false;
    bool ok = true;
    ok = SafeWrite(b+8, x) && ok;
    ok = SafeWrite(b+0, y) && ok;
    ok = SafeWrite(b+4, z) && ok;
    return ok;
}

static float PathCatmull(float p0, float p1, float p2, float p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f*p0 - 5.0f*p1 + 4.0f*p2 - p3) * t2 +
        (-p0 + 3.0f*p1 - 3.0f*p2 + p3) * t3);
}

static float PathShortestAngle(float a, float b, float t)
{
    constexpr float kTwoPi = 6.2831853071795864769f;
    constexpr float kPi = 3.14159265358979323846f;
    float d = std::fmod(b - a, kTwoPi);
    if (d > kPi) d -= kTwoPi;
    if (d < -kPi) d += kTwoPi;
    return a + d * t;
}

static float PathEase(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float a = std::clamp(g_pathEaseInPct * 0.01f, 0.0f, 0.49f);
    float d = std::clamp(g_pathEaseOutPct * 0.01f, 0.0f, 0.49f);
    if (a + d > 0.95f)
    {
        const float scale = 0.95f / (a + d);
        a *= scale;
        d *= scale;
    }

    const float denom = 1.0f - 0.5f * (a + d);
    if (denom <= 0.0001f) return t;
    const float vmax = 1.0f / denom;

    if (a > 0.0001f && t < a)
        return 0.5f * (vmax / a) * t * t;

    if (d > 0.0001f && t > 1.0f - d)
    {
        const float u = 1.0f - t;
        return 1.0f - 0.5f * (vmax / d) * u * u;
    }

    return 0.5f * a * vmax + (t - a) * vmax;
}

static int PathIndex(int i)
{
    const int n = static_cast<int>(g_pathNodes.size());
    if (n <= 0) return 0;
    if (g_pathLoop)
    {
        i %= n;
        if (i < 0) i += n;
        return i;
    }
    return std::clamp(i, 0, n - 1);
}

static void StopCameraPath(const char* reason)
{
    if (g_pathPlaying.exchange(false))
        Log("Camera Path stopped: %s", reason ? reason : "stop");
    g_pathStatus = reason ? reason : "Stopped";
}

static bool ApplyCameraPathAt(int segment, float rawT)
{
    const int n = static_cast<int>(g_pathNodes.size());
    if (n < 2) return false;

    const int i1 = PathIndex(segment);
    const int i2 = PathIndex(segment + 1);
    const int i0 = PathIndex(segment - 1);
    const int i3 = PathIndex(segment + 2);

    const float t = PathEase(rawT);
    const Camera& p0 = g_pathNodes[i0].cam;
    const Camera& p1 = g_pathNodes[i1].cam;
    const Camera& p2 = g_pathNodes[i2].cam;
    const Camera& p3 = g_pathNodes[i3].cam;

    Camera out{};
    out.x = PathCatmull(p0.x, p1.x, p2.x, p3.x, t);
    out.y = PathCatmull(p0.y, p1.y, p2.y, p3.y, t);
    out.z = PathCatmull(p0.z, p1.z, p2.z, p3.z, t);
    out.yaw   = PathShortestAngle(p1.yaw,   p2.yaw,   t);
    out.pitch = PathShortestAngle(p1.pitch, p2.pitch, t);
    out.roll  = PathShortestAngle(p1.roll,  p2.roll,  t);
    out.fov = p1.fov + (p2.fov - p1.fov) * t;
    return WriteCameraRaw(out);
}

static void BeginCameraPath()
{
    if (g_pathNodes.size() < 2)
    {
        g_pathStatus = "Need at least 2 nodes";
        return;
    }
    if (!g_cameraBase)
    {
        g_pathStatus = "Photo Mode camera not ready";
        return;
    }

    g_pathSegment = 0;
    g_pathFrameInSegment = 0;
    g_pathSegmentStartMs = GetTickCount64();
    WriteCameraRaw(g_pathNodes[0].cam);
    g_pathPlaying = true;
    g_pathStatus = "Playing";
    Log("Camera Path PLAY nodes=%zu mode=%s", g_pathNodes.size(), g_pathFrameMode ? "frames" : "seconds");
}

static void UpdateCameraPath()
{
    if (!g_pathPlaying.load()) return;
    if (!IsNativePhotoModeActive() || !g_cameraBase)
    {
        StopCameraPath("Stopped: Photo Mode inactive");
        return;
    }

    const int n = static_cast<int>(g_pathNodes.size());
    if (n < 2)
    {
        StopCameraPath("Stopped: not enough nodes");
        return;
    }

    const int lastSegment = g_pathLoop ? n - 1 : n - 2;
    if (g_pathSegment > lastSegment)
    {
        if (g_pathLoop)
        {
            g_pathSegment = 0;
            g_pathFrameInSegment = 0;
            g_pathSegmentStartMs = GetTickCount64();
        }
        else
        {
            WriteCameraRaw(g_pathNodes.back().cam);
            StopCameraPath("Finished");
            return;
        }
    }

    CameraPathNode& node = g_pathNodes[PathIndex(g_pathSegment)];
    float t = 0.0f;
    if (g_pathFrameMode)
    {
        const int frames = std::max(1, node.framesToNext);
        t = static_cast<float>(g_pathFrameInSegment) / static_cast<float>(frames);
        ++g_pathFrameInSegment;
    }
    else
    {
        const float secs = std::max(0.01f, node.secondsToNext);
        const uint64_t elapsedMs = GetTickCount64() - g_pathSegmentStartMs;
        t = static_cast<float>(elapsedMs) / (secs * 1000.0f);
    }

    if (t >= 1.0f)
    {
        ApplyCameraPathAt(g_pathSegment, 1.0f);
        ++g_pathSegment;
        g_pathFrameInSegment = 0;
        g_pathSegmentStartMs = GetTickCount64();

        if (!g_pathLoop && g_pathSegment > n - 2)
        {
            WriteCameraRaw(g_pathNodes.back().cam);
            StopCameraPath("Finished");
        }
        return;
    }

    ApplyCameraPathAt(g_pathSegment, t);
}

static std::string CameraPathFileName()
{
    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(g_self, modulePath, MAX_PATH)) {
        std::string result(modulePath);
        const auto pos = result.find_last_of("\\/");
        if (pos != std::string::npos)
            return result.substr(0, pos + 1) + "ControlToolkit_CameraPath.ctlpath";
    }
    return "ControlToolkit_CameraPath.ctlpath";
}

static void SaveCameraPath()
{
    std::ofstream f(CameraPathFileName(), std::ios::trunc);
    if (!f)
    {
        g_pathStatus = "Save failed";
        return;
    }
    f << "CONTROL_CAMERA_PATH_V1\n";
    f << (g_pathLoop ? 1 : 0) << ' ' << (g_pathFrameMode ? 1 : 0) << ' '
      << g_pathEaseInPct << ' ' << g_pathEaseOutPct << '\n';
    for (const auto& n : g_pathNodes)
    {
        f << n.cam.x << ' ' << n.cam.y << ' ' << n.cam.z << ' '
          << n.cam.yaw << ' ' << n.cam.pitch << ' ' << n.cam.roll << ' ' << n.cam.fov << ' '
          << n.secondsToNext << ' ' << n.framesToNext << '\n';
    }
    g_pathStatus = "Saved: " + CameraPathFileName();
}

static void LoadCameraPath()
{
    std::ifstream f(CameraPathFileName());
    if (!f)
    {
        g_pathStatus = "File not found: " + CameraPathFileName();
        return;
    }
    std::string magic;
    std::getline(f, magic);
    if (magic != "CONTROL_CAMERA_PATH_V1")
    {
        g_pathStatus = "Invalid path file";
        return;
    }

    int loop{}, frame{};
    f >> loop >> frame >> g_pathEaseInPct >> g_pathEaseOutPct;
    std::vector<CameraPathNode> loaded;
    CameraPathNode n{};
    while (f >> n.cam.x >> n.cam.y >> n.cam.z
             >> n.cam.yaw >> n.cam.pitch >> n.cam.roll >> n.cam.fov
             >> n.secondsToNext >> n.framesToNext)
    {
        loaded.push_back(n);
        n = {};
    }
    if (loaded.empty())
    {
        g_pathStatus = "Path file contains no nodes";
        return;
    }
    StopCameraPath("Loaded");
    g_pathNodes = std::move(loaded);
    g_pathLoop = loop != 0;
    g_pathFrameMode = frame != 0;
    g_pathSelected = 0;
    g_pathStatus = "Loaded: " + CameraPathFileName();
}


static void PathActionAddNode()
{
    Camera c{};
    if (ReadCamera(c))
    {
        CameraPathNode node{};
        node.cam = c;
        if (!g_pathNodes.empty())
        {
            node.secondsToNext = g_pathNodes.back().secondsToNext;
            node.framesToNext = g_pathNodes.back().framesToNext;
        }
        g_pathNodes.push_back(node);
        g_pathSelected = static_cast<int>(g_pathNodes.size()) - 1;
        g_pathStatus = "Node added";
    }
    else g_pathStatus = "Camera not ready";
}

static void PathActionUpdateNode()
{
    if (g_pathNodes.empty()) return;
    Camera c{};
    if (ReadCamera(c))
    {
        g_pathNodes[g_pathSelected].cam = c;
        g_pathStatus = "Node updated from camera";
    }
}

static void PathActionGoToNode()
{
    if (g_pathNodes.empty()) return;
    StopCameraPath("Preview node");
    WriteCameraRaw(g_pathNodes[g_pathSelected].cam);
}

static void PathActionDeleteNode()
{
    if (g_pathNodes.empty()) return;
    StopCameraPath("Node deleted");
    g_pathNodes.erase(g_pathNodes.begin() + g_pathSelected);
    if (g_pathNodes.empty()) g_pathSelected = 0;
    else g_pathSelected = std::clamp(g_pathSelected, 0, static_cast<int>(g_pathNodes.size()) - 1);
}

static void PathActionPreviousNode()
{
    if (g_pathNodes.empty()) return;
    g_pathSelected = (g_pathSelected + static_cast<int>(g_pathNodes.size()) - 1) % static_cast<int>(g_pathNodes.size());
}

static void PathActionNextNode()
{
    if (g_pathNodes.empty()) return;
    g_pathSelected = (g_pathSelected + 1) % static_cast<int>(g_pathNodes.size());
}

static void PathActionPlayStop()
{
    if (g_pathPlaying.load()) StopCameraPath("Stopped by user");
    else BeginCameraPath();
}

static void PathActionClear()
{
    StopCameraPath("Cleared");
    g_pathNodes.clear();
    g_pathSelected = 0;
}

static void DispatchHotkey(HotkeyAction action)
{
    switch (action)
    {
    case HotkeyAction::ToggleUI:
    {
        const bool next = !g_uiVisible.load(std::memory_order_relaxed);
        g_uiVisible.store(next, std::memory_order_relaxed);
        Log("overlay %s", next ? "shown" : "hidden");
        break;
    }
    case HotkeyAction::PathAddNode:      PathActionAddNode(); break;
    case HotkeyAction::PathUpdateNode:   PathActionUpdateNode(); break;
    case HotkeyAction::PathGoToNode:     PathActionGoToNode(); break;
    case HotkeyAction::PathDeleteNode:   PathActionDeleteNode(); break;
    case HotkeyAction::PathPreviousNode: PathActionPreviousNode(); break;
    case HotkeyAction::PathNextNode:     PathActionNextNode(); break;
    case HotkeyAction::PathPlayStop:     PathActionPlayStop(); break;
    case HotkeyAction::PathClear:        PathActionClear(); break;
    case HotkeyAction::PathSave:         SaveCameraPath(); break;
    case HotkeyAction::PathLoad:         LoadCameraPath(); break;
    default: break;
    }
}

static bool DispatchVirtualKey(int vk)
{
    for (const auto& h : g_hotkeys)
    {
        if (h.vk != 0 && h.vk == vk)
        {
            DispatchHotkey(h.action);
            return true;
        }
    }
    return false;
}

static void DrawPatchToggle(const char*, bool&, MemPatch&, MemPatch*);
static void DrawConfiguration()
{
    DrawPatchToggle("Photo Mode - 8 to 100", g_photo100, g_photoRangePatch, &g_photoNopPatch);
    ImGui::Separator();
    ImGui::Text("Keyboard configuration");
    ImGui::TextDisabled("Click a binding, then press a key. Escape cancels. Unbound actions remain available from the UI.");
    ImGui::Separator();

    for (size_t i = 0; i < g_hotkeys.size(); ++i)
    {
        auto& h = g_hotkeys[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(h.label);
        ImGui::SameLine(230.0f);

        const bool conflict = HotkeyConflict(i);
        if (conflict)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.28f, 0.16f, 1.0f));
        }

        const char* text = (g_hotkeyCaptureIndex == static_cast<int>(i)) ? "Press a key..." : VkName(h.vk);
        if (ImGui::Button(text, ImVec2(150.0f, 0.0f)))
        {
            g_hotkeyCaptureIndex = static_cast<int>(i);
            g_hotkeyStatus = std::string("Waiting for key: ") + h.label;
        }

        if (conflict) ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            h.vk = 0;
            g_hotkeyCaptureIndex = -1;
            g_hotkeyStatus = std::string(h.label) + " unbound";
            SaveHotkeys();
        }
        if (conflict)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, .45f, .25f, 1.0f), "conflict");
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Save Keybinds")) SaveHotkeys();
    ImGui::SameLine();
    if (ImGui::Button("Reload Keybinds")) LoadHotkeys();
    ImGui::SameLine();
    if (ImGui::Button("Reset Keybinds")) { ResetHotkeysToDefaults(); SaveHotkeys(); }
    ImGui::TextDisabled("%s", g_hotkeyStatus.c_str());
    ImGui::TextDisabled("Config file: ControlToolkit.ini");
}

static void DrawCameraPath()
{
    Camera live{};
    const bool cameraReady = ReadCamera(live);
    ImGui::Text("Photo Mode camera: %s", cameraReady ? "Ready" : "Waiting");
    ImGui::SameLine();
    ImGui::TextDisabled("| FOV: %s", g_fovDegPtr.load() ? "Ready" : "Waiting - change FOV once");

    if (ImGui::Button("Add Node")) PathActionAddNode();
    ImGui::SameLine();
    if (ImGui::Button("Update Node") && !g_pathNodes.empty()) PathActionUpdateNode();
    ImGui::SameLine();
    if (ImGui::Button("Go To Node") && !g_pathNodes.empty()) PathActionGoToNode();
    ImGui::SameLine();
    if (ImGui::Button("Delete Node") && !g_pathNodes.empty()) PathActionDeleteNode();

    if (!g_pathNodes.empty())
    {
        if (ImGui::Button("< Prev")) PathActionPreviousNode();
        ImGui::SameLine();
        ImGui::Text("Node %d / %d", g_pathSelected + 1, static_cast<int>(g_pathNodes.size()));
        ImGui::SameLine();
        if (ImGui::Button("Next >")) PathActionNextNode();

        auto& node = g_pathNodes[g_pathSelected];
        ImGui::Text("X %.3f   Y %.3f   Z %.3f", node.cam.x, node.cam.y, node.cam.z);
        ImGui::Text("Pitch %.4f   Yaw %.4f   Roll %.4f", node.cam.pitch, node.cam.yaw, node.cam.roll);
        ImGui::DragFloat("Node FOV", &node.cam.fov, 0.1f, 1.0f, 170.0f, "%.2f");
        ImGui::DragFloat("Seconds to next", &node.secondsToNext, 0.05f, 0.05f, 120.0f, "%.2f s");
        ImGui::DragInt("Frames to next", &node.framesToNext, 1.0f, 1, 10000);
    }

    ImGui::Separator();
    ImGui::Checkbox("Loop", &g_pathLoop);
    ImGui::SameLine();
    ImGui::Checkbox("Frame timing", &g_pathFrameMode);
    ImGui::SliderFloat("Ease In", &g_pathEaseInPct, 0.0f, 49.0f, "%.0f%%");
    ImGui::SliderFloat("Ease Out", &g_pathEaseOutPct, 0.0f, 49.0f, "%.0f%%");
    ImGui::TextDisabled("Position: Catmull-Rom | Rotation: shortest-angle interpolation");

    if (!g_pathPlaying.load())
    {
        if (ImGui::Button("Play")) PathActionPlayStop();
    }
    else
    {
        if (ImGui::Button("Stop")) PathActionPlayStop();
        ImGui::SameLine();
        ImGui::Text("Segment %d", g_pathSegment + 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) PathActionClear();
    ImGui::SameLine();
    if (ImGui::Button("Save")) SaveCameraPath();
    ImGui::SameLine();
    if (ImGui::Button("Load")) LoadCameraPath();

    ImGui::TextDisabled("Status: %s", g_pathStatus.c_str());
    ImGui::TextDisabled("Path file: %s", CameraPathFileName().c_str());
}

static void StartIgcsSession()
{
    if (g_sessionActive) return;
    Camera c{};
    if (!ReadCamera(c)) return;
    g_savedCamera = c;
    g_sessionActive = true;
    Log("IGCSDOF SESSION_BEGIN saved camera");
}

static void EndIgcsSession()
{
    if (g_sessionActive)
        WriteCameraRaw(g_savedCamera);
    g_sessionActive = false;
    Log("IGCSDOF SESSION_END restored camera");
}

static std::vector<std::string> SplitPipeLine(const std::string& line)
{
    std::vector<std::string> out;
    size_t begin = 0;
    while (begin <= line.size())
    {
        const size_t pos = line.find('|', begin);
        if (pos == std::string::npos)
        {
            out.emplace_back(line.substr(begin));
            break;
        }
        out.emplace_back(line.substr(begin, pos - begin));
        begin = pos + 1;
    }
    return out;
}

static float ToFloat(const std::vector<std::string>& p, size_t i)
{
    if (i >= p.size()) return 0.0f;
    char* end = nullptr;
    const float v = std::strtof(p[i].c_str(), &end);
    return end == p[i].c_str() ? 0.0f : v;
}

static void HandlePipeCommand(const std::string& line)
{
    const auto p = SplitPipeLine(line);
    if (p.empty()) return;

    if (p[0] == "SESSION_BEGIN")
    {
        StartIgcsSession();
    }
    else if (p[0] == "SESSION_END")
    {
        EndIgcsSession();
    }
    else if (p[0] == "SET_POSITION_RAW")
    {
        // Universal bridge already performed the engine/camera-basis math.
        // CONTROL PM raw mapping is X=+08, Y=+00, Z=+04.
        WriteCameraPositionRaw(ToFloat(p,1), ToFloat(p,2), ToFloat(p,3));
    }
    else if (p[0] == "ROTATE_YAW_RAW")
    {
        const uintptr_t b = (uintptr_t)g_cameraBase;
        if (b)
        {
            float yaw{};
            if (SafeRead(b + 0x10, yaw))
            {
                yaw += ToFloat(p,1);
                SafeWrite(b + 0x10, yaw);
            }
        }
    }
}

static std::string MakeCameraRawLine()
{
    Camera c{};
    if (!ReadCamera(c)) return "CAMERA_INVALID\n";

    char line[512]{};
    sprintf_s(line, sizeof(line),
        "CAMERA_RAW|1|1|%d|%.9f|%.9f|%.9f|%.9f|%.9f|%.9f|%.9f\n",
        g_sessionActive ? 1 : 0,
        c.x, c.y, c.z,
        c.pitch, c.yaw, c.roll,
        c.fov);
    ++g_cameraSeq;
    return line;
}

static HANDLE OpenPipeClient(const wchar_t* name)
{
    if (!WaitNamedPipeW(name, 250))
        return INVALID_HANDLE_VALUE;

    HANDLE h = CreateFileW(name,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return h;

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
    return h;
}

static bool PipeWriteLine(HANDLE h, const std::string& line)
{
    if (!h || h == INVALID_HANDLE_VALUE) return false;
    DWORD written{};
    return WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) &&
           written == line.size();
}

static DWORD WINAPI CameraPipeWorker(void*)
{
    static constexpr const wchar_t* kCameraPipe = L"\\\\.\\pipe\\IGCSDOF_ProviderToBridge_v1";
    const std::string hello = "HELLO|Protocol=1|Transport=DualPipe|Provider=Control Toolkit|Engine=Northlight|CameraMode=RAW\n";

    while (g_running)
    {
        if (!g_igcsEnabled)
        {
            g_cameraPipeConnected = false;
            Sleep(100);
            continue;
        }

        HANDLE h = OpenPipeClient(kCameraPipe);
        if (h == INVALID_HANDLE_VALUE)
        {
            g_cameraPipeConnected = false;
            Sleep(500);
            continue;
        }

        if (!PipeWriteLine(h, hello))
        {
            CloseHandle(h);
            g_cameraPipeConnected = false;
            Sleep(500);
            continue;
        }

        g_cameraPipeConnected = true;
        Log("IGCSDOF camera pipe connected");

        while (g_running && g_igcsEnabled)
        {
            const std::string line = MakeCameraRawLine();
            if (!PipeWriteLine(h, line))
                break;
            Sleep(16);
        }

        CloseHandle(h);
        g_cameraPipeConnected = false;
        Log("IGCSDOF camera pipe disconnected");
        Sleep(250);
    }
    return 0;
}

static DWORD WINAPI CommandPipeWorker(void*)
{
    static constexpr const wchar_t* kCommandPipe = L"\\\\.\\pipe\\IGCSDOF_BridgeToProvider_v1";
    std::string pending;
    pending.reserve(1024);

    while (g_running)
    {
        if (!g_igcsEnabled)
        {
            g_commandPipeConnected = false;
            Sleep(100);
            continue;
        }

        HANDLE h = OpenPipeClient(kCommandPipe);
        if (h == INVALID_HANDLE_VALUE)
        {
            g_commandPipeConnected = false;
            Sleep(500);
            continue;
        }

        g_commandPipeConnected = true;
        pending.clear();
        Log("IGCSDOF command pipe connected");

        while (g_running && g_igcsEnabled)
        {
            DWORD available{};
            if (!PeekNamedPipe(h, nullptr, 0, nullptr, &available, nullptr))
                break;
            if (!available)
            {
                Sleep(5);
                continue;
            }

            char buf[512]{};
            DWORD got{};
            const DWORD want = (std::min<DWORD>)(available, static_cast<DWORD>(sizeof(buf)));
            if (!ReadFile(h, buf, want, &got, nullptr) || !got)
                break;

            pending.append(buf, buf + got);
            for (;;)
            {
                const size_t nl = pending.find('\n');
                if (nl == std::string::npos) break;
                std::string line = pending.substr(0, nl);
                pending.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty() || line.rfind("WELCOME", 0) == 0) continue;
                HandlePipeCommand(line);
            }
        }

        CloseHandle(h);
        g_commandPipeConnected = false;
        Log("IGCSDOF command pipe disconnected");
        Sleep(250);
    }
    return 0;
}

static void StartNamedPipeBridge()
{
    if (!g_cameraPipeThread)
        g_cameraPipeThread = CreateThread(nullptr, 0, CameraPipeWorker, nullptr, 0, nullptr);
    if (!g_commandPipeThread)
        g_commandPipeThread = CreateThread(nullptr, 0, CommandPipeWorker, nullptr, 0, nullptr);
}

static void StopNamedPipeBridge()
{
    if (g_cameraPipeThread)
    {
        WaitForSingleObject(g_cameraPipeThread, 1200);
        CloseHandle(g_cameraPipeThread);
        g_cameraPipeThread = nullptr;
    }
    if (g_commandPipeThread)
    {
        WaitForSingleObject(g_commandPipeThread, 1200);
        CloseHandle(g_commandPipeThread);
        g_commandPipeThread = nullptr;
    }
    g_cameraPipeConnected = false;
    g_commandPipeConnected = false;
}

// ---------------- In-game DX12 ImGui ----------------
using PresentFn=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT);
using ResizeFn=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using ExecuteFn=void(__stdcall*)(ID3D12CommandQueue*,UINT,ID3D12CommandList*const*);
static PresentFn g_origPresent{};static ResizeFn g_origResize{};static ExecuteFn g_origExecute{};
static ID3D12CommandQueue* g_queue{};static IDXGISwapChain3* g_swap{};static ID3D12Device* g_device{};
static ID3D12DescriptorHeap* g_rtvHeap{};static ID3D12DescriptorHeap* g_srvHeap{};static ID3D12GraphicsCommandList* g_cmdList{};static ID3D12Fence* g_fence{};static HANDLE g_fenceEvent{};static UINT64 g_nextFence=1;
static HWND g_gameHwnd{};static WNDPROC g_oldWndProc{};static bool g_imguiReady{};static DXGI_FORMAT g_rtvFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
struct FrameCtx{ID3D12CommandAllocator* allocator{};ID3D12Resource* target{};D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fenceValue{};};
static std::vector<FrameCtx> g_frames;
static UINT g_rtvInc{},g_srvInc{};static std::array<bool,64> g_srvUsed{};

static void WaitFrame(FrameCtx& f){if(!f.fenceValue||!g_fence)return;if(g_fence->GetCompletedValue()<f.fenceValue){g_fence->SetEventOnCompletion(f.fenceValue,g_fenceEvent);WaitForSingleObject(g_fenceEvent,2000);}f.fenceValue=0;}
static void WaitAll(){for(auto&f:g_frames)WaitFrame(f);}
static void SrvAlloc(ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE*outCpu,D3D12_GPU_DESCRIPTOR_HANDLE*outGpu){for(size_t i=0;i<g_srvUsed.size();++i)if(!g_srvUsed[i]){g_srvUsed[i]=true;auto c=g_srvHeap->GetCPUDescriptorHandleForHeapStart();auto g=g_srvHeap->GetGPUDescriptorHandleForHeapStart();c.ptr+=i*g_srvInc;g.ptr+=i*g_srvInc;*outCpu=c;*outGpu=g;return;}*outCpu={};*outGpu={};}
static void SrvFree(ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE cpu,D3D12_GPU_DESCRIPTOR_HANDLE){auto base=g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr;if(cpu.ptr>=base&&g_srvInc){size_t i=(cpu.ptr-base)/g_srvInc;if(i<g_srvUsed.size())g_srvUsed[i]=false;}}
static void DestroyTargets(){for(auto&f:g_frames){if(f.target){f.target->Release();f.target=nullptr;}if(f.allocator){f.allocator->Release();f.allocator=nullptr;}}g_frames.clear();if(g_rtvHeap){g_rtvHeap->Release();g_rtvHeap=nullptr;}}
static bool CreateTargets(){if(!g_swap||!g_device)return false;DXGI_SWAP_CHAIN_DESC d{};if(FAILED(g_swap->GetDesc(&d)))return false;g_rtvFormat=d.BufferDesc.Format==DXGI_FORMAT_UNKNOWN?DXGI_FORMAT_R8G8B8A8_UNORM:d.BufferDesc.Format;g_frames.resize(d.BufferCount);D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=d.BufferCount;if(FAILED(g_device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&g_rtvHeap))))return false;g_rtvInc=g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);auto h=g_rtvHeap->GetCPUDescriptorHandleForHeapStart();for(UINT i=0;i<d.BufferCount;i++){auto&f=g_frames[i];if(FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator))))return false;if(FAILED(g_swap->GetBuffer(i,IID_PPV_ARGS(&f.target))))return false;f.rtv=h;g_device->CreateRenderTargetView(f.target,nullptr,h);h.ptr+=g_rtvInc;}return true;}
static LRESULT CALLBACK HookWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    // All Toolkit hotkeys are edge-triggered on KEY UP.
    if (m == WM_KEYUP || m == WM_SYSKEYUP)
    {
        const int vk = static_cast<int>(w & 0xFF);

        // Rebind capture has priority over normal actions.
        if (g_hotkeyCaptureIndex >= 0)
        {
            if (vk == VK_ESCAPE)
            {
                g_hotkeyCaptureIndex = -1;
                g_hotkeyStatus = "Key capture cancelled";
            }
            else if (g_hotkeyCaptureIndex < static_cast<int>(g_hotkeys.size()))
            {
                auto& hkey = g_hotkeys[static_cast<size_t>(g_hotkeyCaptureIndex)];
                hkey.vk = vk;
                g_hotkeyStatus = std::string(hkey.label) + " = " + VkName(vk);
                g_hotkeyCaptureIndex = -1;
                SaveHotkeys();
            }
            return 0;
        }

        if (DispatchVirtualKey(vk))
            return 0; // Toolkit hotkeys do not leak into CONTROL.
    }

    if (g_uiVisible.load(std::memory_order_relaxed) && g_imguiReady)
    {
        ImGui_ImplWin32_WndProcHandler(h, m, w, l);
        ImGuiIO& io = ImGui::GetIO();

        const bool mouseMessage = (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST);
        const bool keyMessage = (m >= WM_KEYFIRST && m <= WM_KEYLAST);

        if ((io.WantCaptureMouse && mouseMessage) ||
            (io.WantCaptureKeyboard && keyMessage))
            return 1;
    }

    return CallWindowProcW(g_oldWndProc, h, m, w, l);
}
static void ShutdownImGui(){if(!g_imguiReady)return;WaitAll();if(g_oldWndProc&&g_gameHwnd)SetWindowLongPtrW(g_gameHwnd,GWLP_WNDPROC,(LONG_PTR)g_oldWndProc);g_oldWndProc=nullptr;ImGui_ImplDX12_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();g_imguiReady=false;if(g_cmdList){g_cmdList->Release();g_cmdList=nullptr;}DestroyTargets();if(g_srvHeap){g_srvHeap->Release();g_srvHeap=nullptr;}if(g_fence){g_fence->Release();g_fence=nullptr;}if(g_fenceEvent){CloseHandle(g_fenceEvent);g_fenceEvent=nullptr;}if(g_device){g_device->Release();g_device=nullptr;}if(g_swap){g_swap->Release();g_swap=nullptr;}g_srvUsed.fill(false);}
static bool InitImGui(IDXGISwapChain* sc){if(g_imguiReady)return true;if(!g_queue)return false;if(FAILED(sc->QueryInterface(IID_PPV_ARGS(&g_swap))))return false;if(FAILED(sc->GetDevice(IID_PPV_ARGS(&g_device))))return false;DXGI_SWAP_CHAIN_DESC sd{};g_swap->GetDesc(&sd);g_gameHwnd=sd.OutputWindow;if(!CreateTargets())return false;D3D12_DESCRIPTOR_HEAP_DESC sh{};sh.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;sh.NumDescriptors=(UINT)g_srvUsed.size();sh.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;if(FAILED(g_device->CreateDescriptorHeap(&sh,IID_PPV_ARGS(&g_srvHeap))))return false;g_srvInc=g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);if(FAILED(g_device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,g_frames[0].allocator,nullptr,IID_PPV_ARGS(&g_cmdList))))return false;g_cmdList->Close();if(FAILED(g_device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&g_fence))))return false;g_fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGuiIO&io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;ImGui::StyleColorsDark();ImGuiStyle&st=ImGui::GetStyle();st.WindowRounding=6;st.FrameRounding=3;st.ItemSpacing=ImVec2(9,7);if(!ImGui_ImplWin32_Init(g_gameHwnd))return false;ImGui_ImplDX12_InitInfo ii{};ii.Device=g_device;ii.CommandQueue=g_queue;ii.NumFramesInFlight=(int)g_frames.size();ii.RTVFormat=g_rtvFormat;ii.DSVFormat=DXGI_FORMAT_UNKNOWN;ii.SrvDescriptorHeap=g_srvHeap;ii.SrvDescriptorAllocFn=SrvAlloc;ii.SrvDescriptorFreeFn=SrvFree;if(!ImGui_ImplDX12_Init(&ii))return false;g_oldWndProc=(WNDPROC)SetWindowLongPtrW(g_gameHwnd,GWLP_WNDPROC,(LONG_PTR)HookWndProc);g_imguiReady=true;Log("in-game ImGui initialized HWND=%p buffers=%zu",g_gameHwnd,g_frames.size());return true;}

static bool ParamPage(const Param&p,const char*page){return strcmp(p.page,page)==0;}
static void DrawParams(const char* page)
{
    if (!g_anchor) { ImGui::TextDisabled("Parameter anchor unavailable."); return; }
    if (ImGui::BeginTable("##values",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch,0.42f);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,0.58f);
        int id=0;
        for (const auto& p : kParams)
        {
            if (!ParamPage(p,page)) { ++id; continue; }
            ImGui::PushID(id);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(p.name);
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
            if (p.integer)
            {
                int v{}; SafeRead(g_anchor+p.offset,v); int lo=(int)p.minv, hi=(int)p.maxv;
                if (ImGui::DragInt("##v",&v,p.step,lo,hi,"%d",ImGuiSliderFlags_AlwaysClamp))
                {
                            SafeWrite(g_anchor+p.offset,v);
                }
            }
            else
            {
                float v{}; SafeRead(g_anchor+p.offset,v);
                if (ImGui::DragFloat("##v",&v,p.step,p.minv,p.maxv,"%.6f",ImGuiSliderFlags_AlwaysClamp))
                {
                            SafeWrite(g_anchor+p.offset,v);
                }
            }
            ImGui::PopID(); ++id;
        }
        ImGui::EndTable();
    }
}
static void DrawRendering()
{
    if (!g_anchor) { ImGui::TextDisabled("Parameter anchor unavailable."); return; }
    ImGui::TextUnformatted("Rendering Flags");
    ImGui::Separator();
    for (int i=0; i<(int)(sizeof(kRenderingToggles)/sizeof(kRenderingToggles[0])); ++i)
    {
        const auto& t = kRenderingToggles[i];
        int value{};
        if (!SafeRead(g_anchor + t.offset, value))
        {
            ImGui::TextDisabled("%s: read failed", t.name);
            continue;
        }
        bool enabled = (value == t.onValue);
        ImGui::PushID(i);
        if (ImGui::Checkbox(t.name, &enabled))
        {
            const int out = enabled ? t.onValue : t.offValue;
            SafeWrite(g_anchor + t.offset, out);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("[%d]", value);
        ImGui::PopID();
    }
}

static void DrawPatchToggle(const char* label, bool& state, MemPatch& a, MemPatch* b = nullptr)
{
    bool v = state;
    if (ImGui::Checkbox(label, &v))
    {
        state = v;
        SetPatch(a, v);
        if (b && b->captured)
            SetPatch(*b, v);
    }

    if (!a.captured)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(unavailable)");
    }
}
// Window-size hotsampling (no SRWE required).
static int g_hotWidth=3840, g_hotHeight=2160;
static RECT g_hotOriginalRect{};
static bool g_hotHasOriginal=false;
static std::string g_hotStatus="Choose a resolution, then Apply";
static bool HotValid(int w,int h) { return w>=320 && h>=200 && w<=32768 && h<=32768; }
// Some CONTROL render paths commit the requested resolution only when the
// user finishes resizing. SetWindowPos alone does not generate WM_EXITSIZEMOVE.
// Send it explicitly after Apply and Restore, once the HWND has the new size.
static void HotCommitWindowResize()
{
    if (!g_gameHwnd || !IsWindow(g_gameHwnd)) return;
    DWORD_PTR ignored = 0;
    if (!SendMessageTimeoutW(g_gameHwnd, WM_EXITSIZEMOVE, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &ignored))
        g_hotStatus = "Window resized; WM_EXITSIZEMOVE could not be confirmed";
}
// CONTROL's GameWindow keeps output and internal render dimensions separately.
struct NativeRenderResolution { unsigned int width, height; };
static const char* g_nativeRenderReadStatus = "not checked";
static NativeRenderResolution g_nativeOutputResolution{};
static const char* g_renderResolutionHookStatus = "not installed";
static void* g_origSetUserRendererSettings{};
static std::atomic<double> g_hotRatioX{}, g_hotRatioY{};
static std::atomic_bool g_hotRatioReady{false};
using SetUserRendererSettingsFn = void(__fastcall*)(void*, const void*, int);
static bool ReadNativeRenderResolution(NativeRenderResolution& result)
{
    HMODULE module = GetModuleHandleW(L"coregame_rmdwin10_f.dll");
    if (!module) { g_nativeRenderReadStatus = "coregame module not loaded"; return false; }
    auto* singleton = reinterpret_cast<void**>(GetProcAddress(module,
        "?sm_pInstance@GameWindow@coregame@@0PEAV12@EA"));
    if (!singleton) { g_nativeRenderReadStatus = "GameWindow singleton export not resolved"; return false; }
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(singleton, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        g_nativeRenderReadStatus = "GameWindow singleton memory unavailable"; return false;
    }
    void* object = *singleton;
    if (!object || !VirtualQuery(object, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        g_nativeRenderReadStatus = "GameWindow instance unavailable"; return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(object);
    NativeRenderResolution output{}, value{};
    memcpy(&output.width, bytes + 0xF4, 4); memcpy(&output.height, bytes + 0xF8, 4);
    memcpy(&value.width, bytes + 0xFC, 4); memcpy(&value.height, bytes + 0x100, 4);
    if (value.width < 200 || value.height < 200 || value.width > 32768 || value.height > 32768 ||
        output.width < 200 || output.height < 200 || output.width > 32768 || output.height > 32768) {
        g_nativeRenderReadStatus = "GameWindow dimensions invalid/uninitialized"; return false;
    }
    result = value; g_nativeOutputResolution = output; g_nativeRenderReadStatus = "OK"; return true;
}
static void __fastcall HkSetUserRendererSettings(void* gameWindow, const void* settings, int matchClosest)
{
    auto original = reinterpret_cast<SetUserRendererSettingsFn>(g_origSetUserRendererSettings);
    if (!original || !settings) return;
    alignas(16) uint8_t copy[0xB0];
    memcpy(copy, settings, sizeof(copy));
    const double rx = g_hotRatioX.load(std::memory_order_relaxed);
    const double ry = g_hotRatioY.load(std::memory_order_relaxed);
    if (g_hotRatioReady.load(std::memory_order_acquire) && rx > 0.05 && ry > 0.05) {
        uint32_t outW{}, outH{};
        memcpy(&outW, copy + 0x70, 4); memcpy(&outH, copy + 0x74, 4);
        if (outW >= 200 && outH >= 200 && outW <= 32768 && outH <= 32768) {
            const auto rw = static_cast<uint32_t>(std::clamp(std::llround(outW * rx), 200LL, 32768LL));
            const auto rh = static_cast<uint32_t>(std::clamp(std::llround(outH * ry), 200LL, 32768LL));
            memcpy(copy + 0x78, &rw, 4); memcpy(copy + 0x7C, &rh, 4);
        }
    }
    original(gameWindow, copy, matchClosest);
}
static bool EnableRenderResolutionHook()
{
    HMODULE module = GetModuleHandleW(L"coregame_rmdwin10_f.dll");
    if (!module) { g_renderResolutionHookStatus = "coregame module unavailable"; return false; }
    FARPROC target = GetProcAddress(module,
        "?setUserRendererSettings@GameWindow@coregame@@QEAAXAEBUUserRendererSettings@rend@@W4MatchClosestResolution@12@@Z");
    if (!target) { g_renderResolutionHookStatus = "renderer settings export unresolved"; return false; }
    MH_STATUS st = MH_CreateHook(reinterpret_cast<void*>(target), HkSetUserRendererSettings, &g_origSetUserRendererSettings);
    if (st != MH_OK && st != MH_ERROR_ALREADY_CREATED) {
        g_renderResolutionHookStatus = "MinHook create failed"; Log("Renderer settings hook create failed: %d", (int)st); return false;
    }
    g_renderResolutionHookStatus = "active (GameWindow settings setter)";
    Log("Renderer settings hook active target=%p", target);
    return true;
}
static void DisableRenderResolutionHook() { }
// Capture one stable session baseline BEFORE applying hotsampling.
// g_vRenderResolution is a shared value, NOT a validated renderer resize API.
struct HotRenderRatioBaseline {
    bool captured{};
    double x{}, y{};
    int windowW{}, windowH{};
    unsigned int renderW{}, renderH{};
};
static HotRenderRatioBaseline g_hotRatioBaseline{};
static ULONGLONG g_hotRatioStableSince{};
static int g_hotRatioLastWindowW{}, g_hotRatioLastWindowH{};
static unsigned int g_hotRatioLastRenderW{}, g_hotRatioLastRenderH{};
static bool HotSampleResolutions(int& w, int& h, NativeRenderResolution& render)
{
    if (!g_gameHwnd || !IsWindow(g_gameHwnd) || !ReadNativeRenderResolution(render)) return false;
    w = static_cast<int>(g_nativeOutputResolution.width);
    h = static_cast<int>(g_nativeOutputResolution.height);
    return HotValid(w, h);
}
static bool HotCaptureRatio(bool requireStable)
{
    if (g_hotRatioBaseline.captured || g_hotHasOriginal) return g_hotRatioBaseline.captured;
    int w = 0, h = 0;
    NativeRenderResolution r{};
    if (!HotSampleResolutions(w, h, r)) return false;
    const ULONGLONG now = GetTickCount64();
    if (w != g_hotRatioLastWindowW || h != g_hotRatioLastWindowH ||
        r.width != g_hotRatioLastRenderW || r.height != g_hotRatioLastRenderH) {
        g_hotRatioLastWindowW = w; g_hotRatioLastWindowH = h;
        g_hotRatioLastRenderW = r.width; g_hotRatioLastRenderH = r.height;
        g_hotRatioStableSince = now;
        return false;
    }
    if (requireStable && (now - g_hotRatioStableSince < 2000)) return false;
    g_hotRatioBaseline = {true, double(r.width) / w, double(r.height) / h,
                          w, h, r.width, r.height};
    auto normalize = [](double ratio) { const double standard = std::round(ratio * 12.0) / 12.0; return std::fabs(ratio - standard) <= 0.0015 ? standard : ratio; };
    g_hotRatioX.store(normalize(g_hotRatioBaseline.x), std::memory_order_relaxed);
    g_hotRatioY.store(normalize(g_hotRatioBaseline.y), std::memory_order_relaxed);
    g_hotRatioReady.store(true, std::memory_order_release);
    g_hotRatioBaseline.x = g_hotRatioX.load(); g_hotRatioBaseline.y = g_hotRatioY.load();
    Log("Hotsampling baseline captured: window=%dx%d render=%ux%u ratios=%.6f %.6f",
        w, h, r.width, r.height, g_hotRatioBaseline.x, g_hotRatioBaseline.y);
    return true;
}
static void HotUpdateWindowStatus(const char* action)
{
    RECT rc{};
    if(g_gameHwnd) GetClientRect(g_gameHwnd,&rc);
    char text[256]{};
    snprintf(text,sizeof(text),"%s: window %ld x %ld", action, rc.right-rc.left, rc.bottom-rc.top);
    g_hotStatus=text;
}
static bool HotResizeWindow(int w,int h)
{
    if(!HotValid(w,h)) {g_hotStatus="Invalid size (320x200 to 32768x32768)";return false;}
    if(!g_gameHwnd || !IsWindow(g_gameHwnd)) {g_hotStatus="Game window unavailable";return false;}
    if(!g_allowResize || !g_hotsampling || !g_resizePatch.captured || !g_hotPatch.captured) {
        g_hotStatus="Enable Allow Window Resize and Allow Hotsampling first";return false;
    }
    // Capture the untouched game ratio before the first resize. If the engine
    // has not presented a stable pair yet, fail safely instead of guessing.
    if (!g_hotRatioBaseline.captured && !HotCaptureRatio(true)) {
        g_hotStatus = "Waiting for a stable Window/Render ratio before first Apply";
        return false;
    }
    if(!g_hotHasOriginal) {g_hotHasOriginal=!!GetWindowRect(g_gameHwnd,&g_hotOriginalRect);}
    RECT r{0,0,w,h};
    const DWORD style=(DWORD)GetWindowLongPtrW(g_gameHwnd,GWL_STYLE);
    const DWORD exstyle=(DWORD)GetWindowLongPtrW(g_gameHwnd,GWL_EXSTYLE);
    if(!AdjustWindowRectEx(&r,style,FALSE,exstyle)) {g_hotStatus="AdjustWindowRectEx failed";return false;}
    const int outerW=r.right-r.left,outerH=r.bottom-r.top;
    RECT old{};GetWindowRect(g_gameHwnd,&old);
    if(!SetWindowPos(g_gameHwnd,nullptr,old.left,old.top,outerW,outerH,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)) {
        g_hotStatus="SetWindowPos failed";return false;
    }
    g_hotWidth=w;g_hotHeight=h;
    HotCommitWindowResize();
    HotUpdateWindowStatus("Applied");
    return true;
}
static void DrawHotsampling()
{
    HotCaptureRatio(true);
    NativeRenderResolution native{};
    const bool hasNativeResolution = ReadNativeRenderResolution(native);
    if (hasNativeResolution && g_nativeOutputResolution.width && g_nativeOutputResolution.height) {
        ImGui::Text("Window res: %u x %u", g_nativeOutputResolution.width, g_nativeOutputResolution.height);
    } else {
        RECT client{};
        if (g_gameHwnd && GetClientRect(g_gameHwnd, &client))
            ImGui::Text("Window res: %ld x %ld", client.right - client.left, client.bottom - client.top);
        else
            ImGui::TextDisabled("Window res: unavailable");
    }
    if (hasNativeResolution)
        ImGui::Text("Internal res: %u x %u", native.width, native.height);
    else
        ImGui::TextDisabled("Internal res: unavailable");

    ImGui::Separator();
    ImGui::TextUnformatted("Target window resolution");
    ImGui::PushItemWidth(132.0f);
    ImGui::InputInt("Width",&g_hotWidth);
    ImGui::SameLine();
    ImGui::InputInt("Height",&g_hotHeight);
    ImGui::PopItemWidth();
    if(ImGui::Button("Apply")) HotResizeWindow(g_hotWidth,g_hotHeight);
    ImGui::SameLine();
    ImGui::BeginDisabled(!g_hotHasOriginal || !g_gameHwnd);
    if(ImGui::Button("Restore original") && g_hotHasOriginal && g_gameHwnd) {
        if(SetWindowPos(g_gameHwnd,nullptr,g_hotOriginalRect.left,g_hotOriginalRect.top,
            g_hotOriginalRect.right-g_hotOriginalRect.left,g_hotOriginalRect.bottom-g_hotOriginalRect.top,
            SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)) {
            g_hotHasOriginal=false;
            HotCommitWindowResize();
            HotUpdateWindowStatus("Restored");
        }
        else g_hotStatus="Window restore failed";
    }
    ImGui::EndDisabled();
    if (g_hotStatus != "Choose a resolution, then Apply")
        ImGui::TextWrapped("%s",g_hotStatus.c_str());
}
static void DrawIGCS()
{
    ImGui::Checkbox("IGCSDOF Named Pipe provider", &g_igcsEnabled);
    ImGui::Separator();

    Camera c{};
    const bool cam = ReadCamera(c);
    ImGui::Text("Camera capture: %s", cam ? "Ready" : "Waiting for Photo Mode camera");
    ImGui::Text("Provider -> Bridge: %s", g_cameraPipeConnected ? "CONNECTED" : "waiting");
    ImGui::Text("Bridge -> Provider: %s", g_commandPipeConnected ? "CONNECTED" : "waiting");
    ImGui::Text("DoF session: %s", g_sessionActive ? "Active" : "Idle");

    if (cam && ImGui::BeginTable("##cam", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
    {
        auto row=[](const char*n,double v){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(n);ImGui::TableSetColumnIndex(1);ImGui::Text("%.6f",v);};
        row("X",c.x); row("Y",c.y); row("Z",c.z);
        row("Pitch raw",c.pitch); row("Yaw raw",c.yaw); row("Roll raw",c.roll);
        row("FOV",c.fov);
        ImGui::EndTable();
    }

}
static void DrawUI()
{
    ImGui::SetNextWindowSize(ImVec2(720,620),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520,360),ImVec2(1400,1000));
    if(!ImGui::Begin("CONTROL Toolkit v0.5c + IGCSDOF",nullptr,ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if(g_anchor) ImGui::TextColored(ImVec4(.3f,1,.4f,1),"CONTROL detected");
    else ImGui::TextColored(ImVec4(1,.4f,.2f,1),"CONTROL data anchor not found");
    ImGui::SameLine(); ImGui::TextDisabled("%s = Show / Hide", VkName(Hotkey(HotkeyAction::ToggleUI).vk));
    ImGui::Separator();

    if (ImGui::Button("Reset All"))
        RestoreSessionSnapshot("manual", false);
    ImGui::SameLine();
    ImGui::Checkbox("Auto restore when Photo Mode closes", &g_autoRestoreOnPmExit);
    ImGui::SameLine();
    ImGui::TextDisabled("Snapshot: %s", HasSessionSnapshot() ? SnapshotStatus() : "Waiting for Photo Mode");
    ImGui::TextDisabled("PM native state: %s | value: %u | addr: 0x%llX",
        g_photoModeStateAddr ? (g_pmWasActive ? "ACTIVE" : "idle") : "unavailable",
        g_photoModeStateValue,
        (unsigned long long)g_photoModeStateAddr);
    ImGui::TextDisabled("IGCSDOF camera hook: %s | writer hits: %llu",
        g_pmHookMode,
        (unsigned long long)g_pmWriterCounter);

    ImGui::Separator();
    if(ImGui::BeginTabBar("MainTabs"))
    {
        if(ImGui::BeginTabItem("Hotsampling")) { DrawHotsampling(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Post Process")) { DrawParams("Post Process"); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Rendering")) { DrawRendering(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Lights")) { northlight_lights::DrawUI(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Camera Path")) { DrawCameraPath(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("IGCSDOF")) { DrawIGCS(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Configuration")) { DrawConfiguration(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
// Use one stable coordinate system for Win32 mouse input and ImGui layout.
// Only the framebuffer transform depends on the DX12 backbuffer dimensions.
// In particular, do NOT scale mouse events: the Win32 backend already provides
// client-area coordinates and hotsampling can change the swapchain independently.
static void SyncHotsamplingImGuiCoordinates(ID3D12Resource* backBuffer)
{
    if (!g_gameHwnd || !IsWindow(g_gameHwnd) || !backBuffer) return;
    RECT client{};
    if (!GetClientRect(g_gameHwnd, &client)) return;
    const float cw = float(client.right - client.left);
    const float ch = float(client.bottom - client.top);
    const auto desc = backBuffer->GetDesc();
    const float rw = float(desc.Width);
    const float rh = float(desc.Height);
    if (cw <= 0.0f || ch <= 0.0f || rw <= 0.0f || rh <= 0.0f) return;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(cw, ch);
    io.DisplayFramebufferScale = ImVec2(rw / cw, rh / ch);
    // Mouse positions, window geometry and hit testing remain in client pixels.
    // DX12's renderer uses DisplayFramebufferScale to target the physical buffer.
}

static void RenderOverlay(){if(!g_imguiReady||!g_uiVisible||!g_swap||!g_queue)return;UINT idx=g_swap->GetCurrentBackBufferIndex();if(idx>=g_frames.size())return;auto&f=g_frames[idx];WaitFrame(f);f.allocator->Reset();g_cmdList->Reset(f.allocator,nullptr);D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=f.target;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;g_cmdList->ResourceBarrier(1,&b);g_cmdList->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);ID3D12DescriptorHeap* heaps[]={g_srvHeap};g_cmdList->SetDescriptorHeaps(1,heaps);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();SyncHotsamplingImGuiCoordinates(f.target);ImGui::NewFrame();HotCaptureRatio(true);DrawUI();ImGui::Render();ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),g_cmdList);std::swap(b.Transition.StateBefore,b.Transition.StateAfter);g_cmdList->ResourceBarrier(1,&b);g_cmdList->Close();ID3D12CommandList* lists[]={g_cmdList};g_queue->ExecuteCommandLists(1,lists);UINT64 fv=g_nextFence++;g_queue->Signal(g_fence,fv);f.fenceValue=fv;}
static HRESULT __stdcall HkPresent(IDXGISwapChain* sc, UINT sync, UINT flags)
{
    if (!g_imguiReady)
        InitImGui(sc);

    UpdateCameraPath();
    RenderOverlay();
    return g_origPresent(sc, sync, flags);
}
static HRESULT __stdcall HkResize(IDXGISwapChain*sc,UINT count,UINT w,UINT h,DXGI_FORMAT fmt,UINT flags){bool same=g_swap&&(sc==(IDXGISwapChain*)g_swap);if(same&&g_imguiReady){WaitAll();ImGui_ImplDX12_InvalidateDeviceObjects();DestroyTargets();}HRESULT hr=g_origResize(sc,count,w,h,fmt,flags);if(same&&SUCCEEDED(hr)&&g_imguiReady){CreateTargets();ImGui_ImplDX12_CreateDeviceObjects();}return hr;}
static void __stdcall HkExecute(ID3D12CommandQueue*q,UINT n,ID3D12CommandList*const*lists){if(q&&q->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT)g_queue=q;g_origExecute(q,n,lists);}

static LRESULT CALLBACK DummyWndProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
static bool InstallHooks(){WNDCLASSW wc{};wc.lpfnWndProc=DummyWndProc;wc.hInstance=g_self;wc.lpszClassName=L"CTL_DX12_Dummy";RegisterClassW(&wc);HWND hw=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,g_self,nullptr);if(!hw)return false;IDXGIFactory4*fac{};ID3D12Device*dev{};ID3D12CommandQueue*q{};IDXGISwapChain1*sc1{};bool ok=false;if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&fac)))&&SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)))){D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;if(SUCCEEDED(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)))){DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=100;sd.Height=100;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;if(SUCCEEDED(fac->CreateSwapChainForHwnd(q,hw,&sd,nullptr,nullptr,&sc1))){void**sv=*(void***)sc1;void**qv=*(void***)q;MH_STATUS mh=MH_Initialize();if(mh==MH_OK||mh==MH_ERROR_ALREADY_INITIALIZED){EnableRenderResolutionHook();if(MH_CreateHook(sv[8],HkPresent,(void**)&g_origPresent)==MH_OK&&MH_CreateHook(sv[13],HkResize,(void**)&g_origResize)==MH_OK&&MH_CreateHook(qv[10],HkExecute,(void**)&g_origExecute)==MH_OK&&MH_EnableHook(MH_ALL_HOOKS)==MH_OK)ok=true;}}}}if(sc1)sc1->Release();if(q)q->Release();if(dev)dev->Release();if(fac)fac->Release();DestroyWindow(hw);UnregisterClassW(wc.lpszClassName,g_self);Log("DX12 hooks %s",ok?"installed":"FAILED");return ok;}
static void RemoveHooks(){MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();ShutdownImGui();}

static DWORD WINAPI MainThread(void*){EnsureDebugConsole();Log("v0.5c clean hotsampling UI + render ratio preservation + native PM + Northlight lights + Camera Path starting");LoadHotkeys();Resolve();Log("modules control=%p renderer=%p app=%p core=%p anchor=%p",(void*)g_controlBase,(void*)g_rendererBase,(void*)g_appBase,(void*)g_coreBase,(void*)g_anchor);InitCTPatches();SetPatch(g_resizePatch,true);SetPatch(g_hotPatch,true);g_allowResize=g_resizePatch.enabled;g_hotsampling=g_hotPatch.enabled;Log("Hotsampling patches: resize=%d hot=%d",(int)g_allowResize,(int)g_hotsampling);SetPatch(g_photoRangePatch,true);SetPatch(g_photoNopPatch,true);g_photo100=true;Log("Photo Mode 8->100 enabled by default");EnableCameraCapture();EnableFovCapture();StartNamedPipeBridge();if(!InstallHooks()){Log("hook installation failed");}while(g_running){UpdatePhotoModeSessionState();Sleep(10);}StopCameraPath("DLL unload");RestoreSessionSnapshot("DLL unload",true);EndIgcsSession();DisableRenderResolutionHook();DisableFovCapture();DisableCameraCapture();RemoveHooks();StopNamedPipeBridge();return 0;}
void Start(HMODULE self){g_self=self;if(g_running.exchange(true))return;CreateThread(nullptr,0,MainThread,nullptr,0,nullptr);}
void Stop(){g_running=false;}
}
