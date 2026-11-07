// Delquen - aplicacao nativa (C++/Win32, sem Qt, sem Python)
//
//  - Janela principal tradicional (separadores, botoes, sliders, cores, atalhos)
//  - Overlay transparente com FPS grande, linhas compactas e editor com grelha
//  - Icone na area de notificacao
// Requer administrador (ETW em tempo real). Funciona com jogos em janela/borderless.
// Config: %APPDATA%\Delquen\config.ini

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <evntrace.h>
#include <evntcons.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <cwctype>
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ================================================================ Config
struct Cfg {
    int x = 40, y = 40;
    int scale = 100;      // % do tamanho
    int bg = 80;          // opacidade do fundo preto (0 = transparente)
    int opac = 100;       // opacidade global em %
    bool compact = false;
    bool fps = true, lows = true, ft = true, cpu = true, gpu = true, ram = true, vram = true, temps = true, graph = true;
    // cores: 0 FPS bom, 1 FPS medio, 2 FPS baixo, 3 CPU, 4 GPU, 5 RAM/VRAM
    int col[6] = {RGB(90, 255, 130), RGB(255, 215, 70), RGB(255, 90, 90), RGB(100, 175, 255), RGB(90, 255, 130), RGB(255, 215, 70)};
    int thrGood = 60, thrMid = 30;
    int rate = 500;       // ms
    int hkT = VK_F10, hkE = VK_F9;   // LOBYTE = tecla, HIBYTE = HOTKEYF_*
    bool trayClose = true, autostart = false;
    bool enabled = true, autoShow = false, asked = false, lock = false, record = true, windowed = false;   // lock = cliques atravessam o overlay
    int theme = 0;        // 0 sistema, 1 claro, 2 escuro
    int lang = -1;        // índice do idioma (-1 = detetar pelo Windows)
};
static Cfg C;
static const Cfg DEF{};

// ================================================================ Idiomas (22)
// Ordem: en, pt, pt-BR, fr, es, ar, pl, el, tr, sv, ru, uk, de, it, nl, ja, ko, zh, cs, ro, hu, id
static const int NLANG = 22;
enum StrId {
#define LSTR(n, ...) S_##n,
#include "lang.inc"
#undef LSTR
    S_COUNT_
};
static const wchar_t* const TXT[S_COUNT_][NLANG] = {
#define LSTR(n, ...) { __VA_ARGS__ },
#include "lang.inc"
#undef LSTR
};
static const wchar_t* const LANG_NAME[NLANG] = {
    L"English", L"Português (Portugal)", L"Português (Brasil)", L"Français", L"Español", L"العربية", L"Polski",
    L"Ελληνικά", L"Türkçe", L"Svenska", L"Русский", L"Українська", L"Deutsch", L"Italiano", L"Nederlands",
    L"日本語", L"한국어", L"简体中文", L"Čeština", L"Română", L"Magyar", L"Bahasa Indonesia"};
static const int LANG_AR = 5;
static int g_lang = 0;
static const wchar_t* TR(int id) { const wchar_t* s = TXT[id][g_lang]; return (s && *s) ? s : TXT[id][0]; }
static int DetectLang() {   // idioma do Windows -> índice
    LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)) {
    case LANG_ENGLISH: return 0;
    case LANG_PORTUGUESE: return SUBLANGID(id) == SUBLANG_PORTUGUESE_BRAZILIAN ? 2 : 1;
    case LANG_FRENCH: return 3; case LANG_SPANISH: return 4; case LANG_ARABIC: return 5; case LANG_POLISH: return 6;
    case LANG_GREEK: return 7; case LANG_TURKISH: return 8; case LANG_SWEDISH: return 9; case LANG_RUSSIAN: return 10;
    case LANG_UKRAINIAN: return 11; case LANG_GERMAN: return 12; case LANG_ITALIAN: return 13; case LANG_DUTCH: return 14;
    case LANG_JAPANESE: return 15; case LANG_KOREAN: return 16; case LANG_CHINESE: return 17; case LANG_CZECH: return 18;
    case LANG_ROMANIAN: return 19; case LANG_HUNGARIAN: return 20; case LANG_INDONESIAN: return 21;
    }
    return 0;
}

static std::wstring g_ini;
static std::wstring g_profName;    // exe do perfil de jogo ativo ("" = definições globais)

static int GS(const wchar_t* sec, const wchar_t* k, int d) { return GetPrivateProfileIntW(sec, k, d, g_ini.c_str()); }
static void SS(const wchar_t* sec, const wchar_t* k, int v) { wchar_t b[16]; swprintf_s(b, L"%d", v); WritePrivateProfileStringW(sec, k, b, g_ini.c_str()); }
static int GI(const wchar_t* k, int d) { return GS(L"overlay", k, d); }
static void SI(const wchar_t* k, int v) { SS(L"overlay", k, v); }
static std::wstring ProfSec(const std::wstring& exe) { return L"game:" + exe; }

// definições que podem ter perfil por jogo: posição, tamanho, fundo, opacidade, modo e sensores
static void CopySubset(Cfg& d, const Cfg& s) {
    d.x = s.x; d.y = s.y; d.scale = s.scale; d.bg = s.bg; d.opac = s.opac; d.compact = s.compact;
    d.fps = s.fps; d.lows = s.lows; d.ft = s.ft; d.cpu = s.cpu; d.gpu = s.gpu; d.ram = s.ram;
    d.vram = s.vram; d.temps = s.temps; d.graph = s.graph;
}
static void LoadSubset(const wchar_t* sec, const Cfg& def) {
    C.x = GS(sec, L"x", def.x); C.y = GS(sec, L"y", def.y);
    C.scale = std::clamp(GS(sec, L"scale", def.scale), 50, 250);
    C.bg = std::clamp(GS(sec, L"bg", def.bg), 0, 255);
    C.opac = std::clamp(GS(sec, L"opacity", def.opac), 20, 100);
    C.compact = GS(sec, L"compact", def.compact) != 0;
    C.fps = GS(sec, L"fps", def.fps) != 0; C.lows = GS(sec, L"lows", def.lows) != 0; C.ft = GS(sec, L"frametime", def.ft) != 0;
    C.cpu = GS(sec, L"cpu", def.cpu) != 0; C.gpu = GS(sec, L"gpu", def.gpu) != 0; C.ram = GS(sec, L"ram", def.ram) != 0;
    C.vram = GS(sec, L"vram", def.vram) != 0; C.temps = GS(sec, L"temps", def.temps) != 0; C.graph = GS(sec, L"graph", def.graph) != 0;
}
static void SaveSubset(const wchar_t* sec) {
    SS(sec, L"x", C.x); SS(sec, L"y", C.y); SS(sec, L"scale", C.scale); SS(sec, L"bg", C.bg); SS(sec, L"opacity", C.opac);
    SS(sec, L"compact", C.compact); SS(sec, L"fps", C.fps); SS(sec, L"lows", C.lows); SS(sec, L"frametime", C.ft);
    SS(sec, L"cpu", C.cpu); SS(sec, L"gpu", C.gpu); SS(sec, L"ram", C.ram); SS(sec, L"vram", C.vram);
    SS(sec, L"temps", C.temps); SS(sec, L"graph", C.graph);
}

static void LoadCfg() {
    wchar_t ap[MAX_PATH] = {};
    GetEnvironmentVariableW(L"APPDATA", ap, MAX_PATH);
    std::wstring dir = std::wstring(ap) + L"\\Delquen";
    {   // migração: o programa chamava-se GPAI (pasta %APPDATA%\\GPAI)
        std::wstring legacy = std::wstring(ap) + L"\\GPAI";
        if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES && GetFileAttributesW(legacy.c_str()) != INVALID_FILE_ATTRIBUTES)
            MoveFileW(legacy.c_str(), dir.c_str());
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    g_ini = dir + L"\\config.ini";
    LoadSubset(L"overlay", DEF);
    for (int i = 0; i < 6; ++i) { wchar_t k[8]; swprintf_s(k, L"c%d", i); C.col[i] = GI(k, DEF.col[i]); }
    C.thrGood = std::clamp(GI(L"thrGood", DEF.thrGood), 1, 1000);
    C.thrMid = std::clamp(GI(L"thrMid", DEF.thrMid), 1, 1000);
    C.rate = std::clamp(GI(L"rate", DEF.rate), 100, 2000);
    C.hkT = GI(L"hkToggle", DEF.hkT); C.hkE = GI(L"hkEdit", DEF.hkE);
    C.trayClose = GI(L"trayClose", 1) != 0;
    C.autostart = GI(L"autostart", 0) != 0;
    C.enabled = GI(L"enabled", 1) != 0; C.autoShow = GI(L"autoShow", 0) != 0;
    C.asked = GI(L"asked", 0) != 0; C.lock = GI(L"lock", 0) != 0; C.record = GI(L"record", 1) != 0; C.windowed = GI(L"windowed", 0) != 0;
    C.theme = std::clamp(GI(L"theme", 0), 0, 2);
    int lg = GI(L"language", -1);
    C.lang = (lg >= 0 && lg < NLANG) ? lg : DetectLang();
    g_lang = C.lang;
}
static void SaveCfg() {
    for (int i = 0; i < 6; ++i) { wchar_t k[8]; swprintf_s(k, L"c%d", i); SI(k, C.col[i]); }
    SI(L"thrGood", C.thrGood); SI(L"thrMid", C.thrMid); SI(L"rate", C.rate);
    SI(L"hkToggle", C.hkT); SI(L"hkEdit", C.hkE);
    SI(L"trayClose", C.trayClose); SI(L"autostart", C.autostart);
    SI(L"enabled", C.enabled); SI(L"autoShow", C.autoShow); SI(L"asked", C.asked); SI(L"lock", C.lock);
    SI(L"record", C.record); SI(L"windowed", C.windowed); SI(L"theme", C.theme); SI(L"language", C.lang);
    SaveSubset(g_profName.empty() ? L"overlay" : ProfSec(g_profName).c_str());   // com perfil ativo, grava no perfil
}

// ================================================================ FPS (ETW)
static const wchar_t kSession[] = L"DelquenFpsSession";
static const GUID kDxgi = {0xCA11C036, 0x0102, 0x4A2D, {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}};
static const GUID kKrnl = {0x802EC45A, 0x1E99, 0x4B83, {0x99, 0x20, 0x87, 0xC9, 0x82, 0x77, 0xBA, 0x9D}};  // Microsoft-Windows-DxgKrnl

// Fontes de frames: 0 = DXGI (DirectX 9-12); 1..5 = eventos do kernel (OpenGL, Vulkan e outros):
// PresentHistory_Start, Present_Info, Blit_Info, Flip_Info, FlipMultiPlaneOverlay_Info
static const int NSRC = 6;
static const USHORT KRNL_IDS[5] = {171, 184, 55, 168, 252};
static bool g_krnlOk = false;

struct Frame { double t; float ms; };
struct PidData { LONGLONG last[NSRC] = {}; double lastT = 0; std::deque<Frame> fr[NSRC]; };

static std::mutex g_mx;
static std::map<DWORD, PidData> g_pids;
static double g_qpcFreq = 1.0;

static double NowSec() { LARGE_INTEGER n; QueryPerformanceCounter(&n); return n.QuadPart / g_qpcFreq; }

// histograma de frame times da sessão (barras de 0,25 ms) - memória fixa
static const int HBINS = 800;
static DWORD g_histPid = 0;
static int g_histSrc = 0;
static unsigned long long g_hist[HBINS], g_histN = 0;
static double g_histSum = 0;
static float g_histMax = 0, g_histMin = 1e9f;
static void HistReset() { memset(g_hist, 0, sizeof(g_hist)); g_histN = 0; g_histSum = 0; g_histMax = 0; g_histMin = 1e9f; }
static void HistAdd(float ms) {   // chamar com g_mx bloqueado
    int b = std::min((int)(ms * 4.0f), HBINS - 1);
    g_hist[b]++; g_histN++; g_histSum += ms;
    if (ms > g_histMax) g_histMax = ms;
    if (ms < g_histMin) g_histMin = ms;
}
struct SessStats { double avg = 0, low1 = 0, low01 = 0, minFps = 0, maxFps = 0; unsigned long long n = 0; };
static double HistLow(double frac) {
    unsigned long long target = std::max<unsigned long long>(1, (unsigned long long)(g_histN * frac)), got = 0;
    double sum = 0;
    for (int b = HBINS - 1; b >= 0 && got < target; --b) {
        unsigned long long c = std::min(g_hist[b], target - got);
        sum += c * ((b + 0.5) / 4.0); got += c;
    }
    return got ? 1000.0 / (sum / got) : 0;
}
static SessStats HistStats() {    // chamar com g_mx bloqueado
    SessStats s; s.n = g_histN;
    if (!g_histN || g_histSum <= 0) return s;
    s.avg = 1000.0 * g_histN / g_histSum;
    s.low1 = HistLow(0.01); s.low01 = HistLow(0.001);
    s.minFps = g_histMax > 0 ? 1000.0 / g_histMax : 0;
    s.maxFps = g_histMin < 1e8f ? 1000.0 / g_histMin : 0;
    return s;
}

static void WINAPI OnEvent(PEVENT_RECORD ev) {
    USHORT id = ev->EventHeader.EventDescriptor.Id;
    int src = -1;
    if (id == 42 && IsEqualGUID(ev->EventHeader.ProviderId, kDxgi)) src = 0;
    else if (IsEqualGUID(ev->EventHeader.ProviderId, kKrnl)) { for (int i = 0; i < 5; ++i) if (id == KRNL_IDS[i]) src = i + 1; }
    if (src < 0) return;
    DWORD pid = ev->EventHeader.ProcessId;
    if (src > 0 && pid <= 4) return;                     // eventos do sistema
    LONGLONG ts = ev->EventHeader.TimeStamp.QuadPart;    // QPC (ClientContext = 1)
    double t = ts / g_qpcFreq;
    std::lock_guard<std::mutex> lk(g_mx);
    PidData& d = g_pids[pid];
    if (d.last[src]) {
        float ms = float((ts - d.last[src]) * 1000.0 / g_qpcFreq);
        if (ms > 0.1f && ms < 2000.f) {
            d.fr[src].push_back({t, ms});
            if (pid == g_histPid && src == g_histSrc) HistAdd(ms);
        }
        while (!d.fr[src].empty() && t - d.fr[src].front().t > 30.0) d.fr[src].pop_front();
    }
    d.last[src] = ts; d.lastT = t;
}

// escolhe a fonte de frames de um processo (chamar com g_mx bloqueado): DXGI se existir;
// senão a fonte do kernel com menos eventos (evita contar o mesmo frame duas vezes)
static int PickSrc(PidData& d, double now) {
    auto recent = [&](int s) {
        int n = 0;
        for (auto r = d.fr[s].rbegin(); r != d.fr[s].rend(); ++r) { if (now - r->t > 2.0) break; ++n; }
        return n;
    };
    if (recent(0) >= 2) return 0;
    int best = -1, bn = 0;
    for (int s = 1; s < NSRC; ++s) { int n = recent(s); if (n >= 6 && (best < 0 || n < bn)) { best = s; bn = n; } }
    return best;
}

static void FillProps(std::vector<BYTE>& buf) {
    buf.assign(sizeof(EVENT_TRACE_PROPERTIES) + sizeof(kSession), 0);
    auto* p = (EVENT_TRACE_PROPERTIES*)buf.data();
    p->Wnode.BufferSize = (ULONG)buf.size();
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1;
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
}

static bool EnableKrnl(TRACEHANDLE h) {   // só os eventos de present, filtrados por ID (volume baixo)
    const int n = 5;
    size_t sz = sizeof(EVENT_FILTER_EVENT_ID) + sizeof(USHORT) * (n - ANYSIZE_ARRAY);
    std::vector<BYTE> mem(sz, 0);
    auto* f = (EVENT_FILTER_EVENT_ID*)mem.data();
    f->FilterIn = TRUE; f->Count = n;
    for (int i = 0; i < n; ++i) f->Events[i] = KRNL_IDS[i];
    EVENT_FILTER_DESCRIPTOR d = {};
    d.Ptr = (ULONGLONG)(ULONG_PTR)f; d.Size = (ULONG)sz; d.Type = EVENT_FILTER_TYPE_EVENT_ID;
    ENABLE_TRACE_PARAMETERS p = {};
    p.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    p.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;
    p.EnableFilterDesc = &d; p.FilterDescCount = 1;
    return EnableTraceEx2(h, &kKrnl, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION,
                          0x7FFFFFFFFFFFFFFFULL, 0, 0, &p) == ERROR_SUCCESS;
}

static bool StartEtw() {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = (double)f.QuadPart;
    std::vector<BYTE> buf;
    FillProps(buf);
    ControlTraceW(0, kSession, (PEVENT_TRACE_PROPERTIES)buf.data(), EVENT_TRACE_CONTROL_STOP);
    FillProps(buf);
    TRACEHANDLE h = 0;
    if (StartTraceW(&h, kSession, (PEVENT_TRACE_PROPERTIES)buf.data()) != ERROR_SUCCESS) return false;
    EnableTraceEx2(h, &kDxgi, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0, 0, 0, nullptr);
    g_krnlOk = EnableKrnl(h);                              // OpenGL/Vulkan (experimental)
    EVENT_TRACE_LOGFILEW lf = {};
    lf.LoggerName = (LPWSTR)kSession;
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = OnEvent;
    TRACEHANDLE th = OpenTraceW(&lf);
    if (th == INVALID_PROCESSTRACE_HANDLE) return false;
    std::thread([th]() mutable { ProcessTrace(&th, 1, nullptr, nullptr); }).detach();
    return true;
}
static void StopEtw() {
    std::vector<BYTE> buf; FillProps(buf);
    ControlTraceW(0, kSession, (PEVENT_TRACE_PROPERTIES)buf.data(), EVENT_TRACE_CONTROL_STOP);
}

struct FpsStats { double cur = 0, avg = 0, low1 = 0, low01 = 0; float last = 0; int src = -1; std::vector<float> graph; };

static double LowFps(std::vector<float> ft, double frac) {
    if (ft.empty()) return 0;
    std::sort(ft.begin(), ft.end(), std::greater<float>());
    size_t n = std::max<size_t>(1, (size_t)(ft.size() * frac));
    double s = 0; for (size_t i = 0; i < n; ++i) s += ft[i];
    return 1000.0 / (s / n);
}

static FpsStats ComputeFps(DWORD pid) {
    FpsStats r;
    std::vector<Frame> fr;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = g_pids.find(pid);
        if (it != g_pids.end()) {
            int s = PickSrc(it->second, NowSec());
            if (s >= 0) { r.src = s; fr.assign(it->second.fr[s].begin(), it->second.fr[s].end()); }
        }
    }
    if (fr.size() < 2) { r.src = -1; return r; }
    if (NowSec() - fr.back().t > 2.0) { r.src = -1; return r; }
    std::vector<float> all; all.reserve(fr.size());
    double sum = 0; for (auto& f : fr) { all.push_back(f.ms); sum += f.ms; }
    r.avg = 1000.0 * all.size() / sum;
    r.low1 = LowFps(all, 0.01);
    r.low01 = LowFps(all, 0.001);
    int n1 = 0; double s1 = 0;
    for (size_t i = fr.size(); i-- > 0;) { if (fr.back().t - fr[i].t > 1.0) break; s1 += fr[i].ms; ++n1; }
    r.cur = n1 ? 1000.0 * n1 / s1 : 0;
    r.last = fr.back().ms;
    size_t g = std::min<size_t>(120, all.size());
    r.graph.assign(all.end() - g, all.end());
    return r;
}

static void Prune() {   // remove processos sem frames ha mais de 60 s
    double t = NowSec();
    std::lock_guard<std::mutex> lk(g_mx);
    for (auto it = g_pids.begin(); it != g_pids.end();) {
        if (t - it->second.lastT > 60.0) it = g_pids.erase(it); else ++it;
    }
}

// ================================================================ Hardware
struct NvUtil { unsigned gpu, mem; };
struct NvMem { unsigned long long total, free, used; };
typedef int (*FnInit)();
typedef int (*FnHandle)(unsigned, void**);
typedef int (*FnUtil)(void*, NvUtil*);
typedef int (*FnTemp)(void*, int, unsigned*);
typedef int (*FnMem)(void*, NvMem*);
static FnUtil nvUtil; static FnTemp nvTemp; static FnMem nvMem; static void* nvDev;

static void InitNvml() {
    HMODULE m = LoadLibraryW(L"nvml.dll");
    if (!m) m = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (!m) return;
    auto init = (FnInit)GetProcAddress(m, "nvmlInit_v2");
    auto hnd = (FnHandle)GetProcAddress(m, "nvmlDeviceGetHandleByIndex_v2");
    nvUtil = (FnUtil)GetProcAddress(m, "nvmlDeviceGetUtilizationRates");
    nvTemp = (FnTemp)GetProcAddress(m, "nvmlDeviceGetTemperature");
    nvMem = (FnMem)GetProcAddress(m, "nvmlDeviceGetMemoryInfo");
    if (!init || !hnd || !nvUtil || !nvTemp || !nvMem || init() != 0 || hnd(0, &nvDev) != 0) nvUtil = nullptr;
}

// GPU genérica (AMD/Intel/NVIDIA sem NVML): utilização 3D e VRAM via contadores do Windows + DXGI
static std::wstring g_luid;          // "luid_0x........_0x........" (minúsculas) do adaptador principal
static double g_vramTotalDxgi = 0;   // GB de memória dedicada do adaptador principal

static void InitDxgiAdapter() {
    IDXGIFactory1* fac = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&fac)) || !fac) return;
    SIZE_T best = 0;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; fac->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory >= best) {
            best = d.DedicatedVideoMemory;
            wchar_t b[64];
            swprintf_s(b, L"luid_0x%08x_0x%08x", (unsigned)d.AdapterLuid.HighPart, (unsigned)d.AdapterLuid.LowPart);
            g_luid = b;
            g_vramTotalDxgi = d.DedicatedVideoMemory / 1073741824.0;
        }
        a->Release();
    }
    fac->Release();
}

static PDH_HQUERY pq; static PDH_HCOUNTER pc, pcMem;
static void PdhReopen() {
    if (pq) { PdhCloseQuery(pq); pq = nullptr; }
    pc = pcMem = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &pq) != ERROR_SUCCESS) { pq = nullptr; return; }
    if (PdhAddEnglishCounterW(pq, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &pc) != ERROR_SUCCESS) {
        PdhCloseQuery(pq); pq = nullptr; pc = nullptr; return;
    }
    PdhAddEnglishCounterW(pq, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &pcMem);
    PdhCollectQueryData(pq);
}
static double PdhSum(PDH_HCOUNTER c) {   // soma as instâncias do adaptador principal
    if (!c) return -1;
    DWORD sz = 0, cnt = 0;
    if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &sz, &cnt, nullptr) != PDH_MORE_DATA || !sz) return -1;
    std::vector<BYTE> b(sz);
    auto* it = (PDH_FMT_COUNTERVALUE_ITEM_W*)b.data();
    if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE, &sz, &cnt, it) != ERROR_SUCCESS) return -1;
    double t = 0;
    for (DWORD i = 0; i < cnt; ++i) {
        if (!g_luid.empty()) {
            std::wstring n = it[i].szName;
            for (auto& ch : n) ch = (wchar_t)towlower(ch);
            if (n.find(g_luid) == std::wstring::npos) continue;
        }
        t += it[i].FmtValue.doubleValue;
    }
    return t;
}

struct Hw { double cpu = 0, ramUsed = 0, ramTotal = 0; int gpu = -1, gpuTemp = -1; double vramUsed = 0, vramTotal = 0; };
static ULONGLONG FT(const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }

static Hw PollHw() {
    static ULONGLONG pi = 0, pk = 0, pu = 0;
    static int k = 0;
    static int lastGpu = -1; static double lastVram = 0;
    Hw h;
    FILETIME i, kk, u;
    if (GetSystemTimes(&i, &kk, &u)) {
        ULONGLONG di = FT(i) - pi, dk = FT(kk) - pk, du = FT(u) - pu;
        ULONGLONG total = dk + du;
        if (pi && total) h.cpu = 100.0 * (total - di) / total;
        pi = FT(i); pk = FT(kk); pu = FT(u);
    }
    MEMORYSTATUSEX ms = {sizeof(ms)};
    GlobalMemoryStatusEx(&ms);
    h.ramTotal = ms.ullTotalPhys / 1073741824.0;
    h.ramUsed = (ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0;
    if (nvUtil) {
        NvUtil u2; unsigned t = 0; NvMem m2;
        if (nvUtil(nvDev, &u2) == 0) h.gpu = (int)u2.gpu;
        if (nvTemp(nvDev, 0, &t) == 0) h.gpuTemp = (int)t;
        if (nvMem(nvDev, &m2) == 0) { h.vramUsed = m2.used / 1073741824.0; h.vramTotal = m2.total / 1073741824.0; }
    } else if (C.gpu || C.vram) {
        if (k++ % 10 == 0) PdhReopen();                  // reabre de 10 em 10 leituras (instâncias mudam); esta leitura usa o valor anterior
        else if (pq) {
            PdhCollectQueryData(pq);
            double u3 = PdhSum(pc); if (u3 >= 0) lastGpu = (int)std::min(100.0, u3);
            double v = PdhSum(pcMem); if (v >= 0) lastVram = v / 1073741824.0;
        }
        h.gpu = lastGpu;
        if (g_vramTotalDxgi >= 0.5 && pcMem) { h.vramUsed = lastVram; h.vramTotal = g_vramTotalDxgi; }
    }
    return h;
}

// ================================================================ Estado global / prototipos
#define WM_TRAY (WM_APP + 1)
#define WM_SHOWMAIN (WM_APP + 2)
#define WM_SETLANG (WM_APP + 3)
#define TIMER_ID 1
#define COL_DIM  RGB(150, 165, 190)
#define COL_EDIT RGB(100, 175, 255)

static HWND g_hwnd;      // overlay
static HWND g_main;      // janela principal
static HINSTANCE g_hi;
static bool g_visible = false, g_edit = false, g_enabled = true, g_gameActive = false;
static DWORD g_gamePid = 0;
static FpsStats g_fps;
static Hw g_hw;
static DWORD g_pid;
static NOTIFYICONDATAW g_nid;
static std::wstring g_gameExe;     // exe do jogo detetado
static DWORD g_lastFg = 0;         // último programa em foco (que não é o Delquen)
static void RefreshGames();
static void RefreshDiag();
static void Log(const wchar_t* fmt, ...);   // %APPDATA%\Delquen\debug.log
static Cfg g_stash;                // definições globais guardadas enquanto um perfil de jogo está ativo
struct Session { bool active = false; FILE* f = nullptr; double t0 = 0, lastRow = 0; std::wstring exe, path; SYSTEMTIME st = {}; int rows = 0; };
static Session g_sess;
static void SessionTick();
static void SessionEnd();
static void SyncUi();
static void ShowMain();
static void UpdateShown();

// ================================================================ Render (alpha por pixel, sem cartoes)
static HFONT fBig, fMid, fSm;
static HDC g_mdc;
static HBITMAP g_dib; static HGDIOBJ g_old; static void* g_bits; static int dw, dh;

static int Sc(int v) { return (int)(v * C.scale / 100.0 + 0.5); }

// normaliza a cor para o canal maximo = 255 (necessario para o alpha por pixel)
static COLORREF Col(int i) {
    int c = C.col[i], r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    int m = std::max({r, g, b, 1});
    return RGB(r * 255 / m, g * 255 / m, b * 255 / m);
}

static void MakeFonts() {
    if (fBig) { DeleteObject(fBig); DeleteObject(fMid); DeleteObject(fSm); }
    auto mk = [](int h, int w) {
        return CreateFontW(-Sc(h), 0, 0, 0, w, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    };
    fBig = mk(50, FW_BOLD); fMid = mk(19, FW_SEMIBOLD); fSm = mk(14, FW_NORMAL);
}

static void EnsureDib(int W, int H) {
    if (g_dib && W == dw && H == dh) return;
    if (g_dib) { SelectObject(g_mdc, g_old); DeleteObject(g_dib); }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    g_dib = CreateDIBSection(g_mdc, &bi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
    g_old = SelectObject(g_mdc, g_dib);
    dw = W; dh = H;
}

static COLORREF HeatCol(int t) { return t < 70 ? Col(0) : t < 85 ? Col(1) : Col(2); }
static COLORREF LoadCol(int p, COLORREF base) { return p >= 95 ? Col(2) : base; }
static void T(HDC dc, int x, int y, const wchar_t* s) { TextOutW(dc, x, y, s, (int)wcslen(s)); }

static void Render() {
    if (!g_hwnd || !g_visible) return;
    HDC m = g_mdc;
    const bool full = !C.compact;
    const int pad = Sc(10), W = Sc(250), lineH = Sc(25);
    const bool showVram = C.vram && full && g_hw.vramTotal > 0;

    int H = pad;
    if (C.fps) H += Sc(60);
    if (C.fps && C.lows && full) H += Sc(20);
    if (C.compact) H += (C.cpu || C.gpu || C.ram) ? lineH : 0;
    else H += ((C.cpu ? 1 : 0) + (C.gpu ? 1 : 0) + (C.ram ? 1 : 0) + (showVram ? 1 : 0)) * lineH;
    if (C.graph && full) H += Sc(46);
    H += pad;
    if (H < pad * 2 + Sc(24)) H = pad * 2 + Sc(24);

    EnsureDib(W, H);
    memset(g_bits, 0, (size_t)W * H * 4);

    int y = pad;
    wchar_t s[96];

    // ---- FPS grande
    if (C.fps) {
        bool ok = g_fps.cur > 0;
        COLORREF col = !ok ? COL_DIM : g_fps.cur >= C.thrGood ? Col(0) : g_fps.cur >= C.thrMid ? Col(1) : Col(2);
        if (ok) swprintf_s(s, L"%.0f", g_fps.cur); else wcscpy_s(s, L"--");
        SelectObject(m, fBig); SetTextColor(m, col);
        T(m, pad, y - Sc(6), s);
        SIZE sz; GetTextExtentPoint32W(m, s, (int)wcslen(s), &sz);
        int tx = pad + sz.cx + Sc(8);
        SelectObject(m, fMid); SetTextColor(m, COL_DIM); T(m, tx, y + Sc(6), L"FPS");
        if (C.ft && ok) {
            swprintf_s(s, L"%.1f ms", g_fps.last);
            SelectObject(m, fSm); T(m, tx, y + Sc(30), s);
        }
        y += Sc(60);
        if (C.lows && full) {
            if (ok) swprintf_s(s, L"1%% %.0f    0.1%% %.0f    %ls %.0f", g_fps.low1, g_fps.low01, TR(S_AVG), g_fps.avg);
            else swprintf_s(s, L"1%% --    0.1%% --    %ls --", TR(S_AVG));
            SelectObject(m, fSm); SetTextColor(m, COL_DIM); T(m, pad, y - Sc(2), s);
            y += Sc(20);
        }
    }

    // ---- sensores
    wchar_t cpuS[16], gpuS[16], ramS[40], vramS[40], gtS[16];
    swprintf_s(cpuS, L"%.0f%%", g_hw.cpu);
    if (g_hw.gpu >= 0) swprintf_s(gpuS, L"%d%%", g_hw.gpu); else wcscpy_s(gpuS, L"n/d");
    swprintf_s(ramS, L"%.1f / %.1f GB", g_hw.ramUsed, g_hw.ramTotal);
    swprintf_s(vramS, L"%.1f / %.1f GB", g_hw.vramUsed, g_hw.vramTotal);
    if (g_hw.gpuTemp >= 0) swprintf_s(gtS, L"%d\u00B0C", g_hw.gpuTemp); else gtS[0] = 0;

    if (C.compact) {
        if (C.cpu || C.gpu || C.ram) {
            int cx = pad;
            auto put = [&](COLORREF c, const wchar_t* t) {
                SetTextColor(m, c); T(m, cx, y, t);
                SIZE z; GetTextExtentPoint32W(m, t, (int)wcslen(t), &z); cx += z.cx;
            };
            SelectObject(m, fSm);
            if (C.cpu) { put(COL_DIM, L"CPU "); put(LoadCol((int)g_hw.cpu, Col(3)), cpuS); put(COL_DIM, L"   "); }
            if (C.gpu) { put(COL_DIM, L"GPU "); put(LoadCol(g_hw.gpu, Col(4)), gpuS); put(COL_DIM, L"   "); }
            if (C.ram) { swprintf_s(s, L"%.1fG", g_hw.ramUsed); put(COL_DIM, L"RAM "); put(Col(5), s); }
            y += lineH;
        }
    } else {
        auto row = [&](const wchar_t* lab, COLORREF vc, const wchar_t* val, const wchar_t* tmp, COLORREF tc) {
            SelectObject(m, fMid);
            SetTextColor(m, COL_DIM); T(m, pad, y, lab);
            SetTextColor(m, vc); T(m, pad + Sc(58), y, val);
            if (tmp && *tmp) { SetTextColor(m, tc); T(m, pad + Sc(150), y, tmp); }
            y += lineH;
        };
        if (C.cpu) row(L"CPU", LoadCol((int)g_hw.cpu, Col(3)), cpuS, nullptr, 0);
        if (C.gpu) row(L"GPU", LoadCol(g_hw.gpu, Col(4)), gpuS, C.temps ? gtS : nullptr, HeatCol(g_hw.gpuTemp));
        if (C.ram) row(L"RAM", Col(5), ramS, nullptr, 0);
        if (showVram) row(L"VRAM", Col(5), vramS, nullptr, 0);
    }

    // ---- grafico de frame time (0-40 ms)
    if (C.graph && full) {
        RECT gr = {pad, y + Sc(4), W - pad, y + Sc(4) + Sc(36)};
        HBRUSH fb = CreateSolidBrush(RGB(70, 80, 100));
        FrameRect(m, &gr, fb); DeleteObject(fb);
        if (g_fps.graph.size() > 1) {
            HPEN pen = CreatePen(PS_SOLID, std::max(1, Sc(1)), Col(0));
            HGDIOBJ op = SelectObject(m, pen);
            int gw = gr.right - gr.left - 2, gh = gr.bottom - gr.top - 2;
            for (size_t i = 0; i < g_fps.graph.size(); ++i) {
                int x = gr.left + 1 + (int)(i * gw / (g_fps.graph.size() - 1));
                int yy = gr.bottom - 1 - (int)(std::min(g_fps.graph[i], 40.f) / 40.f * gh);
                if (i == 0) MoveToEx(m, x, yy, nullptr); else LineTo(m, x, yy);
            }
            // marcadores de stutter: frames com mais de 2x o frame time médio
            HPEN rp = CreatePen(PS_SOLID, std::max(1, Sc(1)), Col(2));
            SelectObject(m, rp);
            double lim = 2.0 * (g_fps.avg > 0 ? 1000.0 / g_fps.avg : 16.7);
            for (size_t i = 0; i < g_fps.graph.size(); ++i) {
                if (g_fps.graph[i] <= lim) continue;
                int x = gr.left + 1 + (int)(i * gw / (g_fps.graph.size() - 1));
                MoveToEx(m, x, gr.top + 1, nullptr); LineTo(m, x, gr.bottom - 1);
            }
            SelectObject(m, pen); DeleteObject(rp);
            SelectObject(m, op); DeleteObject(pen);
        }
        y += Sc(46);
    }

    // ---- modo de edicao: grelha + moldura
    if (g_edit) {
        HPEN gp = CreatePen(PS_SOLID, 1, RGB(60, 70, 95));
        HGDIOBJ op = SelectObject(m, gp);
        for (int x = 0; x < W; x += 20) { MoveToEx(m, x, 0, nullptr); LineTo(m, x, H); }
        for (int yy = 0; yy < H; yy += 20) { MoveToEx(m, 0, yy, nullptr); LineTo(m, W, yy); }
        SelectObject(m, op); DeleteObject(gp);
        RECT all = {0, 0, W, H};
        HBRUSH bb = CreateSolidBrush(COL_EDIT);
        FrameRect(m, &all, bb); DeleteObject(bb);
    }

    // ---- alpha por pixel: alpha = max(R,G,B) (texto sobre preto = pre-multiplicado)
    GdiFlush();
    int bgA = g_edit ? std::max(C.bg, 120) : C.bg;
    DWORD* px = (DWORD*)g_bits;
    for (int i = 0, n = W * H; i < n; ++i) {
        DWORD p = px[i];
        int a = std::max({(int)((p >> 16) & 255), (int)((p >> 8) & 255), (int)(p & 255)});
        int oa = a + bgA * (255 - a) / 255;
        px[i] = ((DWORD)oa << 24) | (p & 0x00FFFFFF);
    }

    POINT src = {0, 0};
    SIZE size = {W, H};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, (BYTE)(C.opac * 255 / 100), AC_SRC_ALPHA};
    UpdateLayeredWindow(g_hwnd, nullptr, nullptr, &size, g_mdc, &src, 0, &bf, ULW_ALPHA);
}

// ================================================================ Controlo do overlay
enum { ID_TOGGLE = 100, ID_EDIT, ID_COMPACT, ID_RESET, ID_QUIT, ID_OPEN, ID_AUTOSHOW, ID_SENS = 200, ID_SIZE = 300, ID_BG = 400, ID_OP = 500 };

static struct { const wchar_t* n; int sid; bool* f; } SENS[] = {
    {L"FPS", -1, &C.fps}, {L"1% / 0.1% Low", -1, &C.lows}, {L"Frame time (ms)", -1, &C.ft},
    {L"CPU", -1, &C.cpu}, {L"GPU", -1, &C.gpu}, {L"RAM", -1, &C.ram}, {L"VRAM", -1, &C.vram},
    {nullptr, S_SENS_TEMPS, &C.temps}, {nullptr, S_SENS_GRAPH, &C.graph}};
static const int NSENS = (int)(sizeof(SENS) / sizeof(SENS[0]));
static const wchar_t* SensName(int i) { return SENS[i].sid >= 0 ? TR(SENS[i].sid) : SENS[i].n; }
static const int SZ[] = {75, 100, 130, 170};
static const int SZN[] = {S_SZ_SMALL, S_SZ_NORMAL, S_SZ_LARGE, S_SZ_HUGE};
static const int BGV[] = {0, 80, 170};
static const int BGN[] = {S_BG_TRANSPARENT, S_BG_SOFT, S_TH_DARK};
static const int OPV[] = {100, 80, 60};

static HWND g_status;

// ---- deteção de jogos: processo em primeiro plano que apresenta frames (exclui browsers, leitores, shell)
static void OnGameChange(DWORD newPid);

static std::map<DWORD, std::wstring> g_exeCache;
static std::wstring ExeName(DWORD pid) {   // nome do .exe em minúsculas
    auto it = g_exeCache.find(pid);
    if (it != g_exeCache.end()) return it->second;
    std::wstring s;
    if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t p[MAX_PATH]; DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, p, &n)) {
            s.assign(p, n);
            size_t k = s.find_last_of(L'\\');
            if (k != std::wstring::npos) s = s.substr(k + 1);
            for (auto& ch : s) ch = (wchar_t)towlower(ch);
        }
        CloseHandle(h);
    }
    if (g_exeCache.size() > 256) g_exeCache.clear();
    g_exeCache[pid] = s;
    return s;
}

static std::set<std::wstring> g_ignore;    // programas que o utilizador marcou como "não é um jogo"
static std::set<std::wstring> g_include;   // programas que o utilizador marcou como "é um jogo"
static void LoadList(const wchar_t* sec, std::set<std::wstring>& out) {
    wchar_t buf[8192];
    DWORD n = GetPrivateProfileStringW(sec, nullptr, L"", buf, 8192, g_ini.c_str());
    for (wchar_t* q = buf; n && *q && q < buf + n; q += wcslen(q) + 1) out.insert(q);
}
static void LoadIgnore() { LoadList(L"ignore", g_ignore); LoadList(L"games", g_include); }

// janela em primeiro plano em ecrã inteiro / borderless (não maximizada): típico de jogos
static bool IsFullscreenLike(HWND w) {
    if (!w || IsZoomed(w)) return false;
    RECT r; MONITORINFO mi = {sizeof(mi)};
    if (!GetWindowRect(w, &r) || !GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

static bool IsExcluded(DWORD pid) {
    if (pid == GetCurrentProcessId()) return true;
    std::wstring s = ExeName(pid);
    if (g_include.count(s)) return false;
    if (g_ignore.count(s)) return true;
    static const wchar_t* skip[] = {
        L"dwm.exe", L"explorer.exe", L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe",
        L"opera.exe", L"vivaldi.exe", L"msedgewebview2.exe", L"steamwebhelper.exe", L"discord.exe",
        L"teams.exe", L"ms-teams.exe", L"vlc.exe", L"spotify.exe", L"delquen.exe", L"applicationframehost.exe",
        L"searchhost.exe", L"startmenuexperiencehost.exe", L"shellexperiencehost.exe", L"textinputhost.exe",
        L"lockapp.exe", L"mpc-hc64.exe", L"mpc-be64.exe", L"potplayermini64.exe", L"obs64.exe", L"mpv.exe",
        L"wmplayer.exe", L"microsoft.media.player.exe", L"code.exe", L"slack.exe", L"zoom.exe",
        L"claude.exe", L"notion.exe", L"whatsapp.exe", L"telegram.exe", L"figma.exe", L"obsidian.exe",
        L"steam.exe", L"epicgameslauncher.exe", L"battle.net.exe", L"galaxyclient.exe", L"eadesktop.exe",
        L"upc.exe", L"ubisoftconnect.exe", L"widgets.exe", L"searchapp.exe", L"nvcontainer.exe",
        L"nvidia overlay.exe", L"msteams.exe", L"onedrive.exe", L"photos.exe", L"video.ui.exe", L"filmstv.exe"};
    for (auto* e : skip) if (s == e) return true;
    return false;
}

static int RecentFrames(DWORD pid, double win) {
    double t = NowSec();
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_pids.find(pid);
    if (it == g_pids.end()) return 0;
    int s = PickSrc(it->second, t);
    if (s < 0) return 0;
    int n = 0;
    for (auto r = it->second.fr[s].rbegin(); r != it->second.fr[s].rend(); ++r) { if (t - r->t > win) break; ++n; }
    return n;
}
static double LastSeen(DWORD pid) {
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_pids.find(pid);
    return it == g_pids.end() ? 0.0 : it->second.lastT;
}
static void UpdateHistSrc(DWORD pid) {   // fonte de frames usada no histograma da sessão
    std::lock_guard<std::mutex> lk(g_mx);
    auto it = g_pids.find(pid);
    if (it == g_pids.end()) return;
    int s = PickSrc(it->second, NowSec());
    if (s >= 0) g_histSrc = s;
}

static std::wstring DiagText(DWORD pid) {   // eventos de frames recebidos nos últimos 2 s, por fonte
    int n[NSRC] = {};
    double t = NowSec();
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = g_pids.find(pid);
        if (it != g_pids.end())
            for (int s = 0; s < NSRC; ++s)
                for (auto r = it->second.fr[s].rbegin(); r != it->second.fr[s].rend(); ++r) { if (t - r->t > 2.0) break; ++n[s]; }
    }
    wchar_t b[200];
    swprintf_s(b, L"DXGI %d | kernel %d / %d / %d / %d / %d (2 s)", n[0], n[1], n[2], n[3], n[4], n[5]);
    return b;
}

static void ShowOverlay(bool v) {
    g_visible = v;
    if (v) { ShowWindow(g_hwnd, SW_SHOWNA); Render(); }
    else ShowWindow(g_hwnd, SW_HIDE);
}

// o overlay aparece se estiver ativo e (modo manual, ou jogo detetado, ou a editar posição)
static void UpdateShown() {
    bool want = g_enabled && (!C.autoShow || g_gameActive || g_edit);
    if (want != g_visible) {
        Log(L"overlay %ls (ativo=%d auto=%d jogo=%d a-arrastar=%d)", want ? L"visivel" : L"escondido", g_enabled, C.autoShow, g_gameActive, g_edit);
        ShowOverlay(want);
    }
}

static bool ProcAlive(DWORD pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;   // sem acesso: assume que existe
    bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}

static void DetectGame(DWORD fg) {
    double now = NowSec();
    DWORD prev = g_gamePid;
    if (g_gamePid) {                                                          // jogo terminou / parou de apresentar
        bool dead = !ProcAlive(g_gamePid);
        if (dead || now - LastSeen(g_gamePid) > 5.0) { Log(L"jogo %lu terminou (%ls)", g_gamePid, dead ? L"processo fechado" : L"sem frames"); g_gamePid = 0; }
    }
    bool fgIsGame = fg && !IsExcluded(fg) && RecentFrames(fg, 2.0) >= 30 &&   // >= ~15 FPS
                    (C.windowed || g_include.count(ExeName(fg)) || IsFullscreenLike(GetForegroundWindow()));
    if (fgIsGame && fg != g_gamePid) g_gamePid = fg;
    if (g_gamePid != prev) OnGameChange(g_gamePid);                           // sessão + perfil do jogo
    bool active = g_gamePid != 0;
    if (active != g_gameActive) { g_gameActive = active; UpdateShown(); }
}

static void RestartTimer() { SetTimer(g_hwnd, TIMER_ID, C.rate, nullptr); }

static void UpdateTip() {
    static int cnt = 0;
    if (!g_nid.hWnd || (++cnt % 2)) return;
    wchar_t t[64];
    if (g_fps.cur > 0) swprintf_s(t, L"Delquen \u2014 %.0f FPS", g_fps.cur); else wcscpy_s(t, L"Delquen");
    if (wcscmp(t, g_nid.szTip)) { wcscpy_s(g_nid.szTip, t); g_nid.uFlags = NIF_TIP; Shell_NotifyIconW(NIM_MODIFY, &g_nid); }
}

static void Tick() {
    static int n = 0;
    DWORD fg = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &fg);
    DetectGame(fg);
    bool mainVis = g_main && IsWindowVisible(g_main);
    if (fg && fg != GetCurrentProcessId() && fg != g_lastFg) { g_lastFg = fg; if (mainVis) RefreshGames(); }
    DWORD pid = g_gamePid;                                  // só se mede um jogo detetado
    g_pid = pid;
    bool rec = g_sess.active;
    if (rec) UpdateHistSrc(pid);
    if (pid && (g_visible || mainVis || rec)) g_fps = ComputeFps(pid); else g_fps = FpsStats();
    if ((g_visible || rec) && n % 2 == 0 && (C.cpu || C.gpu || C.ram || C.vram || rec)) g_hw = PollHw();
    if (++n % 40 == 0) Prune();
    if (mainVis && g_status) {
        wchar_t s[400];
        if (g_fps.cur > 0) swprintf_s(s, L"%ls %.0f FPS - %ls (%ls)", TR(S_MEASURING), g_fps.cur, g_gameExe.c_str(),
                                      g_fps.src == 0 ? L"DirectX" : TR(S_SRC_OTHER));
        else if (g_enabled && C.autoShow && !g_gameActive) wcsncpy_s(s, TR(S_STATUS_AUTO), _TRUNCATE);
        else if (g_enabled && !C.autoShow) wcsncpy_s(s, TR(S_STATUS_MANUAL), _TRUNCATE);
        else wcsncpy_s(s, TR(S_STATUS_WAIT), _TRUNCATE);
        static wchar_t last[400];
        if (wcscmp(s, last)) { wcsncpy_s(last, s, _TRUNCATE); SetWindowTextW(g_status, s); }
    }
    SessionTick();
    if (mainVis) RefreshDiag();
    UpdateTip();
    Render();
}

static void SetVisible(bool v) {
    g_enabled = v; C.enabled = v;
    SaveCfg();
    UpdateShown();
    if (v) Tick();
    if (g_main) InvalidateRect(GetDlgItem(g_main, 1000), nullptr, TRUE);   // botão "Overlay ativo"
}

static void ApplyLock() {
    LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    if (C.lock) ex |= WS_EX_TRANSPARENT; else ex &= ~(LONG_PTR)WS_EX_TRANSPARENT;
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
}
static void SetLock(bool l) { C.lock = l; ApplyLock(); SaveCfg(); SyncUi(); }
static void ResetPos() {
    C.x = C.y = 40;
    SetWindowPos(g_hwnd, HWND_TOPMOST, C.x, C.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    SaveCfg(); SyncUi();
}
static void EnsureOnScreen() {   // evita overlay fora do ecrã (ex.: monitor removido)
    RECT r = {C.x, C.y, C.x + 200, C.y + 100};
    if (!MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) { C.x = 40; C.y = 40; }
}

// ---- sessões (CSV) e perfis por jogo
static std::wstring DataDir();
static void Log(const wchar_t* fmt, ...) {
    wchar_t msg[256];
    va_list ap; va_start(ap, fmt); vswprintf_s(msg, fmt, ap); va_end(ap);
    SYSTEMTIME st; GetLocalTime(&st);
    FILE* f = nullptr;
    if (_wfopen_s(&f, (DataDir() + L"\\debug.log").c_str(), L"a, ccs=UTF-8") == 0 && f) {
        fwprintf(f, L"%02d:%02d:%02d %ls\n", st.wHour, st.wMinute, st.wSecond, msg);
        fclose(f);
    }
}
static std::wstring DataDir() { size_t k = g_ini.find_last_of(L'\\'); return k == std::wstring::npos ? L"." : g_ini.substr(0, k); }
static std::wstring Stem(const std::wstring& exe) { size_t k = exe.find_last_of(L'.'); return k == std::wstring::npos ? exe : exe.substr(0, k); }

static void SessionStart(DWORD pid, const std::wstring& exe) {
    if (!C.record) return;
    { std::lock_guard<std::mutex> lk(g_mx); g_histPid = pid; g_histSrc = 0; HistReset(); }
    g_sess.active = true; g_sess.exe = exe; g_sess.t0 = NowSec(); g_sess.lastRow = 0; g_sess.rows = 0;
    GetLocalTime(&g_sess.st);
    std::wstring dir = DataDir() + L"\\sessions";
    CreateDirectoryW(dir.c_str(), nullptr);
    wchar_t fn[MAX_PATH];
    swprintf_s(fn, L"%ls\\%ls_%04d%02d%02d_%02d%02d%02d.csv", dir.c_str(), Stem(exe).c_str(),
               g_sess.st.wYear, g_sess.st.wMonth, g_sess.st.wDay, g_sess.st.wHour, g_sess.st.wMinute, g_sess.st.wSecond);
    g_sess.path = fn;
    if (_wfopen_s(&g_sess.f, fn, L"w, ccs=UTF-8") == 0 && g_sess.f)
        fwprintf(g_sess.f, L"time_s,fps,frametime_ms,cpu_pct,gpu_pct,gpu_temp_c,ram_gb,vram_gb\n");
}

static void SessionTick() {
    if (!g_sess.active || !g_sess.f) return;
    double now = NowSec();
    if (now - g_sess.lastRow < 1.0) return;
    g_sess.lastRow = now;
    fwprintf(g_sess.f, L"%.0f,%.1f,%.2f,%.0f,%d,%d,%.2f,%.2f\n", now - g_sess.t0, g_fps.cur, g_fps.last,
             g_hw.cpu, g_hw.gpu, g_hw.gpuTemp, g_hw.ramUsed, g_hw.vramUsed);
    if (++g_sess.rows % 10 == 0) fflush(g_sess.f);
}

static void SessionEnd() {
    if (!g_sess.active) return;
    g_sess.active = false;
    double dur = NowSec() - g_sess.t0;
    SessStats ss;
    { std::lock_guard<std::mutex> lk(g_mx); ss = HistStats(); g_histPid = 0; }
    if (g_sess.f) { fclose(g_sess.f); g_sess.f = nullptr; }
    if (dur < 10.0 || ss.n < 30) { DeleteFileW(g_sess.path.c_str()); return; }   // sessões muito curtas são descartadas
    std::wstring hp = DataDir() + L"\\history.csv";
    bool isNew = GetFileAttributesW(hp.c_str()) == INVALID_FILE_ATTRIBUTES;
    FILE* f = nullptr;
    if (_wfopen_s(&f, hp.c_str(), L"a, ccs=UTF-8") == 0 && f) {
        if (isNew) fwprintf(f, L"date,time,exe,duration_s,avg_fps,fps_1pct_low,fps_0.1pct_low,min_fps,max_fps\n");
        const SYSTEMTIME& st = g_sess.st;
        fwprintf(f, L"%04d-%02d-%02d,%02d:%02d:%02d,%ls,%.0f,%.1f,%.1f,%.1f,%.1f,%.1f\n", st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, g_sess.exe.c_str(), dur, ss.avg, ss.low1, ss.low01, ss.minFps, ss.maxFps);
        fclose(f);
    }
}

static std::vector<std::wstring> SplitCsv(const std::wstring& s) {
    std::vector<std::wstring> v; size_t p = 0;
    for (;;) {
        size_t q = s.find(L',', p);
        if (q == std::wstring::npos) { v.push_back(s.substr(p)); break; }
        v.push_back(s.substr(p, q - p)); p = q + 1;
    }
    return v;
}

static std::wstring LastSessions(int maxn) {   // últimas sessões do history.csv (mais recente primeiro)
    std::wstring hp = DataDir() + L"\\history.csv";
    FILE* f = nullptr;
    if (_wfopen_s(&f, hp.c_str(), L"r, ccs=UTF-8") != 0 || !f) return L"";
    std::vector<std::wstring> lines; wchar_t buf[512];
    while (fgetws(buf, 512, f)) {
        std::wstring l = buf;
        while (!l.empty() && (l.back() == L'\n' || l.back() == L'\r')) l.pop_back();
        if (!l.empty() && l.rfind(L"date,", 0) != 0) lines.push_back(l);
    }
    fclose(f);
    std::wstring out; int shown = 0;
    for (size_t i = lines.size(); i-- > 0 && shown < maxn;) {
        auto v = SplitCsv(lines[i]);
        if (v.size() < 9) continue;
        wchar_t row[500];
        swprintf_s(row, L"%ls %ls  %ls  %d %ls  %ls %ls  1%% %ls  0.1%% %ls\r\n", v[0].c_str(), v[1].substr(0, 5).c_str(),
                   v[2].c_str(), (_wtoi(v[3].c_str()) + 30) / 60, TR(S_MIN), TR(S_AVG), v[4].c_str(), v[5].c_str(), v[6].c_str());
        out += row; ++shown;
    }
    return out;
}

static void RefreshAfterProfile() {
    MakeFonts(); EnsureOnScreen();
    SetWindowPos(g_hwnd, HWND_TOPMOST, C.x, C.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    SyncUi(); Render();
}
static void ProfileLeave() {
    if (g_profName.empty()) return;
    CopySubset(C, g_stash); g_profName.clear();
    RefreshAfterProfile();
}
static void ProfileEnter(const std::wstring& exe) {
    if (exe.empty()) return;
    std::wstring sec = ProfSec(exe);
    if (GS(sec.c_str(), L"profile", 0) != 1) return;
    g_stash = C; LoadSubset(sec.c_str(), C); g_profName = exe;
    RefreshAfterProfile();
}
static void ProfileToggle() {   // cria/apaga o perfil do jogo atual
    if (!g_profName.empty()) {
        std::wstring sec = ProfSec(g_profName);
        WritePrivateProfileStringW(sec.c_str(), nullptr, nullptr, g_ini.c_str());   // apaga a secção
        ProfileLeave();
        return;
    }
    if (g_gameExe.empty()) { MessageBoxW(g_main, TR(S_MSG_NOGAME), L"Delquen", MB_ICONINFORMATION); return; }
    g_stash = C; g_profName = g_gameExe;
    std::wstring sec = ProfSec(g_gameExe);
    SS(sec.c_str(), L"profile", 1);
    SaveSubset(sec.c_str());
    SyncUi();
}

static void MarkLastFg(bool isGame) {   // "é um jogo" / "não é um jogo" para o último programa em foco
    std::wstring name = g_lastFg ? ExeName(g_lastFg) : std::wstring();
    if (name.empty()) { MessageBoxW(g_main, TR(S_MSG_NOFOCUS), L"Delquen", MB_ICONINFORMATION); return; }
    if (isGame) { g_include.insert(name); g_ignore.erase(name); SS(L"games", name.c_str(), 1); WritePrivateProfileStringW(L"ignore", name.c_str(), nullptr, g_ini.c_str()); }
    else { g_ignore.insert(name); g_include.erase(name); SS(L"ignore", name.c_str(), 1); WritePrivateProfileStringW(L"games", name.c_str(), nullptr, g_ini.c_str()); }
    if (g_gamePid && ExeName(g_gamePid) == name && !isGame) {   // se estava a ser tratado como jogo, deixa de ser
        g_gamePid = 0; OnGameChange(0);
        if (g_gameActive) { g_gameActive = false; UpdateShown(); }
    }
}

static void OnGameChange(DWORD newPid) {
    SessionEnd();
    ProfileLeave();
    g_gameExe = newPid ? ExeName(newPid) : std::wstring();
    Log(L"jogo atual: pid %lu %ls", newPid, g_gameExe.empty() ? L"(nenhum)" : g_gameExe.c_str());
    if (newPid) { ProfileEnter(g_gameExe); SessionStart(newPid, g_gameExe); }
    SyncUi();
}

static UINT ModsFrom(int hk) {
    UINT m = 0; BYTE h = HIBYTE((WORD)hk);
    if (h & HOTKEYF_SHIFT) m |= MOD_SHIFT;
    if (h & HOTKEYF_CONTROL) m |= MOD_CONTROL;
    if (h & HOTKEYF_ALT) m |= MOD_ALT;
    return m;
}
static bool RegHotkeys() {
    UnregisterHotKey(g_hwnd, 1); UnregisterHotKey(g_hwnd, 2);
    bool a = RegisterHotKey(g_hwnd, 1, ModsFrom(C.hkT), LOBYTE((WORD)C.hkT)) != 0;
    bool b = RegisterHotKey(g_hwnd, 2, ModsFrom(C.hkE), LOBYTE((WORD)C.hkE)) != 0;
    return a && b;
}

static void ShowMenu() {
    HMENU m = CreatePopupMenu(), sens = CreatePopupMenu(), sz = CreatePopupMenu(), bg = CreatePopupMenu(), op = CreatePopupMenu();
    auto ck = [](bool b) { return (UINT)(MF_STRING | (b ? MF_CHECKED : 0)); };
    AppendMenuW(m, MF_STRING, ID_OPEN, TR(S_TRAY_OPEN));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, ck(g_enabled), ID_TOGGLE, TR(S_OVERLAY_ON));
    AppendMenuW(m, ck(C.lock), ID_EDIT, TR(S_LOCK_POS));
    AppendMenuW(m, ck(C.compact), ID_COMPACT, TR(S_COMPACT));
    AppendMenuW(m, ck(C.autoShow), ID_AUTOSHOW, TR(S_ONLY_IN_GAMES));
    for (int i = 0; i < NSENS; ++i) AppendMenuW(sens, ck(*SENS[i].f), ID_SENS + i, SensName(i));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sens, TR(S_TAB_SENSORS));
    for (int i = 0; i < 4; ++i) AppendMenuW(sz, ck(C.scale == SZ[i]), ID_SIZE + i, TR(SZN[i]));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sz, TR(S_SIZE));
    for (int i = 0; i < 3; ++i) AppendMenuW(bg, ck(C.bg == BGV[i]), ID_BG + i, TR(BGN[i]));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)bg, TR(S_BACKGROUND));
    for (int i = 0; i < 3; ++i) { wchar_t b[16]; swprintf_s(b, L"%d%%", OPV[i]); AppendMenuW(op, ck(C.opac == OPV[i]), ID_OP + i, b); }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)op, TR(S_OPACITY));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_RESET, TR(S_RESET_POS));
    AppendMenuW(m, MF_STRING, ID_QUIT, TR(S_EXIT));

    POINT p; GetCursorPos(&p);
    SetForegroundWindow(g_hwnd);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);

    if (c == ID_OPEN) { ShowMain(); return; }
    if (c == ID_TOGGLE) SetVisible(!g_enabled);
    else if (c == ID_EDIT) SetLock(!C.lock);
    else if (c == ID_RESET) ResetPos();
    else if (c == ID_COMPACT) C.compact = !C.compact;
    else if (c == ID_AUTOSHOW) { C.autoShow = !C.autoShow; UpdateShown(); }
    else if (c >= ID_SENS && c < ID_SENS + 50) *SENS[c - ID_SENS].f = !*SENS[c - ID_SENS].f;
    else if (c >= ID_SIZE && c < ID_SIZE + 4) { C.scale = SZ[c - ID_SIZE]; MakeFonts(); }
    else if (c >= ID_BG && c < ID_BG + 3) C.bg = BGV[c - ID_BG];
    else if (c >= ID_OP && c < ID_OP + 3) C.opac = OPV[c - ID_OP];
    else if (c == ID_QUIT) { PostQuitMessage(0); return; }
    if (c) { SaveCfg(); Tick(); SyncUi(); }
}

static LRESULT CALLBACK OverlayProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_TIMER: Tick(); return 0;
    case WM_TRAY:
        if (LOWORD(l) == WM_LBUTTONUP) ShowMain();
        else if (LOWORD(l) == WM_RBUTTONUP) ShowMenu();
        return 0;
    case WM_HOTKEY:
        if (w == 1) SetVisible(!g_enabled);
        if (w == 2) SetLock(!C.lock);
        if (w == 3) PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST: return HTCAPTION;                    // arrastar em qualquer ponto (se não estiver bloqueado)
    case WM_NCLBUTTONDBLCLK: ShowMain(); return 0;          // duplo clique abre a janela principal
    case WM_NCRBUTTONUP: case WM_CONTEXTMENU: if (msg == WM_NCRBUTTONUP) ShowMenu(); return 0;
    case WM_ENTERSIZEMOVE: g_edit = true; Render(); return 0;   // mostra a grelha enquanto arrasta
    case WM_MOVING: {                                       // snap: grelha de 10 px + margens do ecrã
        RECT* r = (RECT*)l;
        int ww = r->right - r->left, hh = r->bottom - r->top, g = 10;
        int x = (r->left + g / 2) / g * g, y = (r->top + g / 2) / g * g;
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfoW(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi)) {
            const RECT& a = mi.rcWork; const int t = 14;
            if (abs(x - a.left) < t) x = a.left; else if (abs(x + ww - a.right) < t) x = a.right - ww;
            if (abs(y - a.top) < t) y = a.top; else if (abs(y + hh - a.bottom) < t) y = a.bottom - hh;
        }
        r->left = x; r->top = y; r->right = x + ww; r->bottom = y + hh;
        return TRUE;
    }
    case WM_EXITSIZEMOVE: {
        g_edit = false;
        RECT r; GetWindowRect(h, &r); C.x = r.left; C.y = r.top;
        SaveCfg(); UpdateShown(); Render(); SyncUi();
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// ================================================================ Janela principal (estilo tradicional, claro/escuro)
enum {
    IDC_ACTIVE = 1000, IDC_EDITPOS, IDC_STATUS, IDC_RESET, IDC_TRAY, IDC_QUIT,
    IDC_TAB0 = 1010,                                            // 1010..1012
    IDC_MODE_FULL = 1100, IDC_MODE_COMPACT, IDC_SCALE, IDC_SCALE_L, IDC_OPAC, IDC_OPAC_L, IDC_BG, IDC_BG_L,
    IDC_X, IDC_Y, IDC_APPLYPOS, IDC_THR_GOOD, IDC_THR_MID, IDC_LOCK,
    IDC_COLBTN = 1120, IDC_SW = 1130, IDC_SENS = 1200,
    IDC_HK_T = 1300, IDC_HK_E, IDC_HK_APPLY, IDC_TRAYCLOSE, IDC_AUTO, IDC_AUTOSHOW, IDC_INFO,
    IDC_RATE0 = 1310,                                           // 1310..1312
    IDC_THEME0 = 1320,                                          // 1320..1322
    IDC_PROFILE = 1330, IDC_RECORD, IDC_OPENDIR, IDC_GAMENAME, IDC_HIST, IDC_IGNORE, IDC_WINDOWED, IDC_LASTFG, IDC_INCLUDE, IDC_LANG, IDC_DIAG
};
enum { K_PUSH = 0, K_TOGGLE, K_TAB, K_RADIO, K_CHECK };

struct Theme { COLORREF bg, panel, text, dim, btn, btnDown, border, accent, input; };
static const Theme LIGHT = {RGB(240, 240, 240), RGB(255, 255, 255), RGB(20, 20, 20), RGB(100, 100, 100),
                            RGB(225, 225, 225), RGB(200, 200, 200), RGB(170, 170, 170), RGB(0, 120, 215), RGB(255, 255, 255)};
static const Theme DARK = {RGB(32, 32, 32), RGB(43, 43, 43), RGB(240, 240, 240), RGB(160, 160, 160),
                           RGB(62, 62, 62), RGB(82, 82, 82), RGB(95, 95, 95), RGB(0, 120, 215), RGB(30, 30, 30)};
static bool g_dark = false;
static const Theme& TH() { return g_dark ? DARK : LIGHT; }
static HBRUSH g_brBg, g_brPanel, g_brInput;

static HWND g_pg[4];
static HFONT g_uiFont;
static HBRUSH g_swBr[6];
static int g_dpi = 96, g_tabSel = 0;
static bool g_sync = false;
static COLORREF g_cust[16];
static const int RATES[3] = {250, 500, 1000};

static int D(int v) { return MulDiv(v, g_dpi, 96); }
static HWND Item(int pg, int id) { return GetDlgItem(g_pg[pg], id); }
static void SetNum(HWND h, int v) { wchar_t b[16]; swprintf_s(b, L"%d", v); SetWindowTextW(h, b); }
static int GetNum(HWND h) { wchar_t b[32] = {}; GetWindowTextW(h, b, 32); return _wtoi(b); }
static int RateIdx() { return C.rate <= 250 ? 0 : C.rate <= 500 ? 1 : 2; }

static bool SystemDark() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v == 0;
}

static void InvalidateButtons() {
    EnumChildWindows(g_main, [](HWND c, LPARAM) -> BOOL {
        wchar_t cn[16]; GetClassNameW(c, cn, 16);
        if (!wcscmp(cn, L"Button")) InvalidateRect(c, nullptr, TRUE);
        return TRUE;
    }, 0);
}

static void ApplyTheme() {
    g_dark = C.theme == 2 || (C.theme == 0 && SystemDark());
    const Theme& t = TH();
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brPanel) DeleteObject(g_brPanel);
    if (g_brInput) DeleteObject(g_brInput);
    g_brBg = CreateSolidBrush(t.bg); g_brPanel = CreateSolidBrush(t.panel); g_brInput = CreateSolidBrush(t.input);
    BOOL d = g_dark;
    DwmSetWindowAttribute(g_main, 20, &d, sizeof(d));       // barra de título escura (Win10 20H1+/Win11)
    DwmSetWindowAttribute(g_main, 19, &d, sizeof(d));
    EnumChildWindows(g_main, [](HWND c, LPARAM) -> BOOL {
        wchar_t cn[32]; GetClassNameW(c, cn, 32);
        if (!wcscmp(cn, L"Edit") || !wcscmp(cn, L"msctls_hotkey32")) SetWindowTheme(c, g_dark ? L"DarkMode_CFD" : nullptr, nullptr);
        return TRUE;
    }, 0);
    RedrawWindow(g_main, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

static int BtnKind(int id) {
    if (id == IDC_ACTIVE) return K_TOGGLE;
    if (id >= IDC_TAB0 && id < IDC_TAB0 + 4) return K_TAB;
    if (id == IDC_MODE_FULL || id == IDC_MODE_COMPACT || (id >= IDC_RATE0 && id < IDC_RATE0 + 3) ||
        (id >= IDC_THEME0 && id < IDC_THEME0 + 3)) return K_RADIO;
    if ((id >= IDC_SENS && id < IDC_SENS + NSENS) || id == IDC_TRAYCLOSE || id == IDC_AUTO || id == IDC_AUTOSHOW || id == IDC_LOCK ||
        id == IDC_PROFILE || id == IDC_RECORD || id == IDC_WINDOWED) return K_CHECK;
    return K_PUSH;
}
static bool BtnState(int id) {
    if (id == IDC_ACTIVE) return g_enabled;
    if (id >= IDC_TAB0 && id < IDC_TAB0 + 4) return g_tabSel == id - IDC_TAB0;
    if (id == IDC_MODE_FULL) return !C.compact;
    if (id == IDC_MODE_COMPACT) return C.compact;
    if (id >= IDC_SENS && id < IDC_SENS + NSENS) return *SENS[id - IDC_SENS].f;
    if (id >= IDC_RATE0 && id < IDC_RATE0 + 3) return RateIdx() == id - IDC_RATE0;
    if (id >= IDC_THEME0 && id < IDC_THEME0 + 3) return C.theme == id - IDC_THEME0;
    if (id == IDC_TRAYCLOSE) return C.trayClose;
    if (id == IDC_AUTO) return C.autostart;
    if (id == IDC_AUTOSHOW) return C.autoShow;
    if (id == IDC_LOCK) return C.lock;
    if (id == IDC_PROFILE) return !g_profName.empty();
    if (id == IDC_RECORD) return C.record;
    if (id == IDC_WINDOWED) return C.windowed;
    return false;
}

static void DrawBtn(DRAWITEMSTRUCT* d) {
    int id = (int)d->CtlID, kind = BtnKind(id);
    bool on = BtnState(id), pressed = (d->itemState & ODS_SELECTED) != 0;
    const Theme& t = TH();
    HDC dc = d->hDC; RECT r = d->rcItem;
    wchar_t txt[128]; GetWindowTextW(d->hwndItem, txt, 128);
    SelectObject(dc, g_uiFont); SetBkMode(dc, TRANSPARENT);

    if (kind == K_CHECK || kind == K_RADIO) {
        FillRect(dc, &r, GetParent(d->hwndItem) == g_main ? g_brBg : g_brPanel);
        int bs = D(14), by = (r.top + r.bottom - bs) / 2, bx = r.left + D(2);
        HPEN pen = CreatePen(PS_SOLID, 1, on ? t.accent : t.border);
        HBRUSH br = CreateSolidBrush(on && kind == K_CHECK ? t.accent : t.input);
        HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, br);
        if (kind == K_RADIO) Ellipse(dc, bx, by, bx + bs, by + bs); else Rectangle(dc, bx, by, bx + bs, by + bs);
        SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen); DeleteObject(br);
        if (on) {
            if (kind == K_RADIO) {
                HBRUSH ab = CreateSolidBrush(t.accent);
                HGDIOBJ o1 = SelectObject(dc, ab), o2 = SelectObject(dc, GetStockObject(NULL_PEN));
                int m = D(4); Ellipse(dc, bx + m, by + m, bx + bs - m, by + bs - m);
                SelectObject(dc, o1); SelectObject(dc, o2); DeleteObject(ab);
            } else {
                HPEN wp = CreatePen(PS_SOLID, std::max(1, D(2)), RGB(255, 255, 255));
                HGDIOBJ o = SelectObject(dc, wp);
                MoveToEx(dc, bx + D(3), by + D(7), nullptr); LineTo(dc, bx + D(6), by + D(10)); LineTo(dc, bx + D(11), by + D(4));
                SelectObject(dc, o); DeleteObject(wp);
            }
        }
        RECT tr = {bx + bs + D(6), r.top, r.right, r.bottom};
        SetTextColor(dc, t.text);
        DrawTextW(dc, txt, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | (g_lang == LANG_AR ? DT_RTLREADING : 0));
        return;
    }

    COLORREF bg = t.btn, tx = t.text;
    if (kind == K_TAB) { bg = on ? t.panel : t.bg; tx = on ? t.text : t.dim; }
    else if (kind == K_TOGGLE) { bg = on ? t.accent : t.btn; tx = on ? RGB(255, 255, 255) : t.text; }
    if (pressed) bg = t.btnDown;
    HBRUSH b = CreateSolidBrush(bg); FillRect(dc, &r, b); DeleteObject(b);
    HBRUSH fb = CreateSolidBrush(t.border); FrameRect(dc, &r, fb); DeleteObject(fb);
    if (kind == K_TAB && on) {
        RECT ar = {r.left, r.top, r.right, r.top + D(2)};
        HBRUSH ab = CreateSolidBrush(t.accent); FillRect(dc, &ar, ab); DeleteObject(ab);
    }
    SetTextColor(dc, tx);
    DrawTextW(dc, txt, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER | (g_lang == LANG_AR ? DT_RTLREADING : 0));
}

static HWND Mk(int pg, const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND par = pg < 0 ? g_main : g_pg[pg];
    return CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | st, D(x), D(y), D(w), D(h), par, (HMENU)(INT_PTR)id, g_hi, nullptr);
}
static HWND Lbl(int pg, const wchar_t* t, int x, int y, int w, int h = 18) { return Mk(pg, L"STATIC", t, SS_LEFT, x, y + 3, w, h, 0); }
static HWND Bt(int pg, const wchar_t* t, int x, int y, int w, int h, int id) { return Mk(pg, L"BUTTON", t, BS_OWNERDRAW | WS_TABSTOP, x, y, w, h, id); }

static void UpdateSwatch(int i) {
    if (g_swBr[i]) DeleteObject(g_swBr[i]);
    g_swBr[i] = CreateSolidBrush(Col(i));
    if (g_pg[0]) InvalidateRect(Item(0, IDC_SW + i), nullptr, TRUE);
}

static void ShowMain() {
    ShowWindow(g_main, SW_SHOW);
    ShowWindow(g_main, SW_RESTORE);
    SetForegroundWindow(g_main);
    Tick();
}

static void SetAutostart(bool on) {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring cmd;
    if (on) cmd = std::wstring(L"schtasks /Create /F /TN Delquen /SC ONLOGON /RL HIGHEST /TR \"\\\"") + exe + L"\\\" /tray\"";
    else cmd = L"schtasks /Delete /F /TN Delquen";
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> b(cmd.begin(), cmd.end()); b.push_back(0);
    if (CreateProcessW(nullptr, b.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}

static void Apply() { SaveCfg(); if (g_visible) Render(); }

static void RefreshGames() {   // página "Jogos": jogo atual e últimas sessões
    if (!g_pg[3]) return;
    std::wstring g = g_gameExe.empty() ? std::wstring(TR(S_NONE_OPEN)) : g_gameExe;
    if (!g_profName.empty()) { g += L"   "; g += TR(S_PROFILE_ACTIVE); }
    SetWindowTextW(Item(3, IDC_GAMENAME), g.c_str());
    std::wstring lf = g_lastFg ? ExeName(g_lastFg) : std::wstring();
    SetWindowTextW(Item(3, IDC_LASTFG), lf.empty() ? L"-" : lf.c_str());
    if (g_tabSel == 3) {
        std::wstring h = LastSessions(5);
        SetWindowTextW(Item(3, IDC_HIST), h.empty() ? TR(S_NO_SESSIONS) : h.c_str());
    }
}

static void RefreshDiag() {   // diagnóstico: que eventos de frames chegam do jogo/programa em foco
    if (!g_pg[3] || !IsWindowVisible(g_pg[3])) return;
    DWORD pid = g_gamePid ? g_gamePid : g_lastFg;
    std::wstring t = pid ? ExeName(pid) + L": " + DiagText(pid) : std::wstring(L"-");
    static std::wstring last;
    if (t != last) { last = t; SetWindowTextW(Item(3, IDC_DIAG), t.c_str()); }
}

static void SelectTab(int i) {
    g_tabSel = i;
    for (int k = 0; k < 4; ++k) ShowWindow(g_pg[k], k == i ? SW_SHOW : SW_HIDE);
    if (i == 3) RefreshGames();
    InvalidateButtons();
}

static void SyncUi() {
    if (!g_main || !g_pg[0]) return;
    g_sync = true;
    HWND p = g_pg[0];
    SendDlgItemMessageW(p, IDC_SCALE, TBM_SETPOS, TRUE, C.scale);
    SendDlgItemMessageW(p, IDC_OPAC, TBM_SETPOS, TRUE, C.opac);
    SendDlgItemMessageW(p, IDC_BG, TBM_SETPOS, TRUE, C.bg);
    wchar_t b[32];
    swprintf_s(b, L"%d%%", C.scale); SetDlgItemTextW(p, IDC_SCALE_L, b);
    swprintf_s(b, L"%d%%", C.opac); SetDlgItemTextW(p, IDC_OPAC_L, b);
    swprintf_s(b, L"%d%%", C.bg * 100 / 255); SetDlgItemTextW(p, IDC_BG_L, b);
    RECT r = {C.x, C.y, 0, 0};
    if (g_hwnd) GetWindowRect(g_hwnd, &r);
    SetNum(Item(0, IDC_X), r.left); SetNum(Item(0, IDC_Y), r.top);
    SetNum(Item(0, IDC_THR_GOOD), C.thrGood); SetNum(Item(0, IDC_THR_MID), C.thrMid);
    for (int i = 0; i < 6; ++i) UpdateSwatch(i);
    SendMessageW(Item(2, IDC_HK_T), HKM_SETHOTKEY, C.hkT, 0);
    SendMessageW(Item(2, IDC_HK_E), HKM_SETHOTKEY, C.hkE, 0);
    std::wstring info = std::wstring(TR(S_INFO_FPS_SRC)) + L" ETW (Microsoft-Windows-DXGI)\r\n" + TR(S_INFO_GPU_SRC) + L" ";
    info += nvUtil ? L"NVIDIA (NVML)" : pq ? TR(S_GPU_PDH) : TR(S_NOT_AVAILABLE);
    info += std::wstring(L"\r\nOpenGL/Vulkan: ") + (g_krnlOk ? TR(S_ACTIVE_EXP) : TR(S_NOT_AVAILABLE));
    SetWindowTextW(Item(2, IDC_INFO), info.c_str());
    RefreshGames();
    g_sync = false;
    InvalidateButtons();
}

static void ResetAll() {
    bool a = C.autostart, asked = C.asked;
    int lg = C.lang;
    g_profName.clear();
    C = DEF; C.autostart = a; C.asked = asked; C.lang = lg;
    g_enabled = C.enabled;
    MakeFonts();
    SetWindowPos(g_hwnd, HWND_TOPMOST, C.x, C.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    RegHotkeys();
    RestartTimer();
    SaveCfg();
    ApplyTheme();
    UpdateShown();
    SyncUi();
    Render();
}

static void PickColor(int i) {
    CHOOSECOLORW cc = {sizeof(cc)};
    cc.hwndOwner = g_main; cc.rgbResult = (COLORREF)C.col[i]; cc.lpCustColors = g_cust;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) { C.col[i] = (int)cc.rgbResult; UpdateSwatch(i); Apply(); }
}

static LRESULT CALLBACK PageProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_COMMAND: case WM_HSCROLL: case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_DRAWITEM:
        return SendMessageW(g_main, msg, w, l);
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r);
        FillRect((HDC)w, &r, g_brPanel);
        HBRUSH fb = CreateSolidBrush(TH().border); FrameRect((HDC)w, &r, fb); DeleteObject(fb);
        return 1;
    }
    }
    return DefWindowProcW(h, msg, w, l);
}

static void BuildUi() {
    if (!g_uiFont) {
        NONCLIENTMETRICSW ncm = {sizeof(ncm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    }

    // cabeçalho
    Bt(-1, TR(S_OVERLAY_ON), 10, 10, 150, 32, IDC_ACTIVE);
    Bt(-1, TR(S_RESET_POS), 170, 10, 130, 32, IDC_EDITPOS);
    Bt(-1, TR(S_ONLY_IN_GAMES), 315, 15, 190, 22, IDC_AUTOSHOW);
    g_status = Mk(-1, L"STATIC", L"", SS_LEFT, 10, 50, 490, 18, IDC_STATUS);

    // separadores (botões desenhados à mão) + painéis
    const wchar_t* names[] = {TR(S_TAB_OVERLAY), TR(S_TAB_SENSORS), TR(S_TAB_SETTINGS), TR(S_TAB_GAMES)};
    for (int i = 0; i < 4; ++i) Bt(-1, names[i], 10 + i * 110, 74, 108, 28, IDC_TAB0 + i);
    for (int i = 0; i < 4; ++i)
        g_pg[i] = CreateWindowExW(0, L"DelquenPage", L"", WS_CHILD | WS_CLIPCHILDREN | (i == 0 ? WS_VISIBLE : 0),
                                  D(10), D(102), D(490), D(346), g_main, nullptr, g_hi, nullptr);

    // ---- página 0: Overlay
    Lbl(0, TR(S_MODE), 10, 10, 75);
    Bt(0, TR(S_FULL), 90, 10, 110, 22, IDC_MODE_FULL);
    Bt(0, TR(S_COMPACT), 205, 10, 110, 22, IDC_MODE_COMPACT);
    struct { int sid; int id, lid, mn, mx; } sl[] = {
        {S_SIZE, IDC_SCALE, IDC_SCALE_L, 50, 250}, {S_OPACITY, IDC_OPAC, IDC_OPAC_L, 20, 100}, {S_BACKGROUND, IDC_BG, IDC_BG_L, 0, 255}};
    for (int i = 0; i < 3; ++i) {
        int y = 42 + i * 32;
        Lbl(0, TR(sl[i].sid), 10, y, 90);
        HWND tb = Mk(0, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 100, y, 290, 26, sl[i].id);
        SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELONG(sl[i].mn, sl[i].mx));
        Mk(0, L"STATIC", L"", SS_LEFT, 400, y + 3, 60, 18, sl[i].lid);
    }
    Lbl(0, TR(S_POSITION_X), 10, 140, 95);
    Mk(0, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 108, 140, 52, 22, IDC_X, WS_EX_CLIENTEDGE);
    Lbl(0, L"Y:", 168, 140, 20);
    Mk(0, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 190, 140, 52, 22, IDC_Y, WS_EX_CLIENTEDGE);
    Bt(0, TR(S_APPLY), 254, 139, 90, 24, IDC_APPLYPOS);
    Lbl(0, TR(S_FPS_GOOD_GE), 10, 172, 105);
    Mk(0, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 118, 172, 50, 22, IDC_THR_GOOD, WS_EX_CLIENTEDGE);
    Lbl(0, TR(S_MID_GE), 178, 172, 85);
    Mk(0, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 266, 172, 50, 22, IDC_THR_MID, WS_EX_CLIENTEDGE);
    Lbl(0, TR(S_BELOW_LOW), 326, 172, 140);
    const wchar_t* cn[] = {TR(S_C_GOOD), TR(S_C_MID), TR(S_C_BAD), L"CPU", L"GPU", L"RAM / VRAM"};
    for (int i = 0; i < 6; ++i) {
        int x = 10 + (i / 3) * 230, y = 208 + (i % 3) * 28;
        Lbl(0, cn[i], x, y, 95);
        Mk(0, L"STATIC", L"", SS_LEFT | WS_BORDER, x + 98, y + 3, 24, 18, IDC_SW + i);
        Bt(0, TR(S_CHANGE), x + 126, y, 100, 24, IDC_COLBTN + i);
    }
    Bt(0, TR(S_LOCK_POS), 10, 306, 450, 22, IDC_LOCK);

    // ---- página 1: Sensores
    for (int i = 0; i < NSENS; ++i) Bt(1, SensName(i), 10 + (i / 5) * 230, 10 + (i % 5) * 28, 215, 22, IDC_SENS + i);
    Mk(1, L"STATIC", TR(S_SENS_NOTE), SS_LEFT, 10, 170, 440, 60, 0);

    // ---- página 2: Definições
    Lbl(2, TR(S_HK_TOGGLE), 10, 10, 175);
    Mk(2, HOTKEY_CLASSW, L"", WS_TABSTOP, 190, 10, 150, 24, IDC_HK_T, WS_EX_CLIENTEDGE);
    Lbl(2, TR(S_HK_LOCK), 10, 40, 175);
    Mk(2, HOTKEY_CLASSW, L"", WS_TABSTOP, 190, 40, 150, 24, IDC_HK_E, WS_EX_CLIENTEDGE);
    Bt(2, TR(S_APPLY_HK), 190, 70, 150, 26, IDC_HK_APPLY);
    Lbl(2, TR(S_REFRESH), 10, 108, 175);
    Bt(2, L"250 ms", 190, 108, 80, 22, IDC_RATE0);
    Bt(2, L"500 ms", 275, 108, 80, 22, IDC_RATE0 + 1);
    Bt(2, L"1000 ms", 360, 108, 80, 22, IDC_RATE0 + 2);
    Lbl(2, TR(S_THEME), 10, 138, 175);
    Bt(2, TR(S_TH_SYSTEM), 190, 138, 80, 22, IDC_THEME0);
    Bt(2, TR(S_TH_LIGHT), 275, 138, 80, 22, IDC_THEME0 + 1);
    Bt(2, TR(S_TH_DARK), 360, 138, 80, 22, IDC_THEME0 + 2);
    Lbl(2, TR(S_LANGUAGE), 10, 168, 175);
    { std::wstring lt = std::wstring(LANG_NAME[g_lang]) + L"  \u25BE"; Bt(2, lt.c_str(), 190, 167, 250, 24, IDC_LANG); }
    Bt(2, TR(S_AUTO_SHOW_LONG), 10, 204, 450, 22, IDC_AUTOSHOW);
    Bt(2, TR(S_TRAY_CLOSE), 10, 230, 450, 22, IDC_TRAYCLOSE);
    Bt(2, TR(S_AUTOSTART), 10, 256, 450, 22, IDC_AUTO);
    Mk(2, L"STATIC", L"", SS_LEFT, 10, 288, 450, 52, IDC_INFO);

    // ---- página 3: Jogos
    Lbl(3, TR(S_CURRENT_GAME), 10, 10, 120);
    Mk(3, L"STATIC", L"", SS_LEFT, 135, 13, 335, 18, IDC_GAMENAME);
    Bt(3, TR(S_PROFILE_CB), 10, 38, 450, 22, IDC_PROFILE);
    Bt(3, TR(S_RECORD_CB), 10, 64, 450, 22, IDC_RECORD);
    Bt(3, TR(S_WINDOWED_CB), 10, 90, 450, 22, IDC_WINDOWED);
    Lbl(3, TR(S_LAST_FOCUS), 10, 124, 190);
    Mk(3, L"STATIC", L"", SS_LEFT, 205, 127, 265, 18, IDC_LASTFG);
    Bt(3, TR(S_IS_GAME), 10, 152, 220, 26, IDC_INCLUDE);
    Bt(3, TR(S_NOT_GAME), 240, 152, 220, 26, IDC_IGNORE);
    Bt(3, TR(S_OPEN_DATA), 10, 188, 200, 26, IDC_OPENDIR);
    Mk(3, L"STATIC", L"", SS_LEFT, 220, 188, 250, 34, IDC_DIAG);
    Lbl(3, TR(S_LAST_SESSIONS), 10, 226, 250);
    Mk(3, L"STATIC", L"", SS_LEFT, 10, 250, 450, 88, IDC_HIST);

    // botões inferiores
    Bt(-1, TR(S_RESET_DEFAULTS), 10, 458, 170, 28, IDC_RESET);
    Bt(-1, TR(S_MIN_TO_TRAY), 190, 458, 200, 28, IDC_TRAY);
    Bt(-1, TR(S_EXIT), 410, 458, 90, 28, IDC_QUIT);

    EnumChildWindows(g_main, [](HWND c, LPARAM) -> BOOL { SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE); return TRUE; }, 0);
}

static HICON MakeIcon(int sz) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = sz; bi.bmiHeader.biHeight = -sz;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP col = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(dc, col);
    RECT r = {0, 0, sz, sz};
    HBRUSH b = CreateSolidBrush(RGB(0, 120, 215)); FillRect(dc, &r, b); DeleteObject(b);
    HFONT f = CreateFontW(-sz * 3 / 4, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, L"D", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    GdiFlush();
    DWORD* px = (DWORD*)bits;
    for (int i = 0; i < sz * sz; ++i) px[i] |= 0xFF000000;
    SelectObject(dc, of); DeleteObject(f); SelectObject(dc, old);
    std::vector<BYTE> z(((sz + 15) / 16) * 2 * sz, 0);
    HBITMAP mask = CreateBitmap(sz, sz, 1, 1, z.data());
    ICONINFO ii = {TRUE, 0, 0, mask, col};
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(mask); DeleteObject(col); DeleteDC(dc);
    return ic;
}

static void ShowLangMenu() {   // lista dos 22 idiomas
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < NLANG; ++i) AppendMenuW(m, MF_STRING | (i == g_lang ? MF_CHECKED : 0), 700 + i, LANG_NAME[i]);
    RECT r; GetWindowRect(Item(2, IDC_LANG), &r);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN, r.left, r.bottom, 0, g_main, nullptr);
    DestroyMenu(m);
    if (c >= 700 && c < 700 + NLANG) PostMessageW(g_main, WM_SETLANG, c - 700, 0);   // adiado: o botão ainda está a tratar o clique
}

static void ApplyLayoutDirection() {   // árabe: janela espelhada (direita-para-esquerda)
    LONG_PTR ex = GetWindowLongPtrW(g_main, GWL_EXSTYLE);
    if (g_lang == LANG_AR) ex |= WS_EX_LAYOUTRTL; else ex &= ~(LONG_PTR)WS_EX_LAYOUTRTL;
    SetWindowLongPtrW(g_main, GWL_EXSTYLE, ex);
}

static void SetLang(int idx) {         // muda o idioma sem reiniciar: reconstrói a janela
    if (idx < 0 || idx >= NLANG || idx == g_lang) return;
    C.lang = g_lang = idx;
    SaveCfg();
    int tab = g_tabSel;
    while (HWND c = GetWindow(g_main, GW_CHILD)) DestroyWindow(c);
    memset(g_pg, 0, sizeof(g_pg)); g_status = nullptr;
    ApplyLayoutDirection();
    BuildUi();
    ApplyTheme();
    SelectTab(tab);
    SyncUi();
    SetWindowPos(g_main, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RedrawWindow(g_main, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static void OnClick(int id) {
    if (id == IDC_ACTIVE) SetVisible(!g_enabled);
    else if (id >= IDC_TAB0 && id < IDC_TAB0 + 4) SelectTab(id - IDC_TAB0);
    else if (id == IDC_EDITPOS) ResetPos();
    else if (id == IDC_LOCK) SetLock(!C.lock);
    else if (id == IDC_PROFILE) ProfileToggle();
    else if (id == IDC_IGNORE) MarkLastFg(false);
    else if (id == IDC_INCLUDE) MarkLastFg(true);
    else if (id == IDC_WINDOWED) { C.windowed = !C.windowed; SaveCfg(); }
    else if (id == IDC_RECORD) { C.record = !C.record; SaveCfg(); }
    else if (id == IDC_OPENDIR) ShellExecuteW(nullptr, L"open", DataDir().c_str(), nullptr, nullptr, SW_SHOW);
    else if (id == IDC_QUIT) PostQuitMessage(0);
    else if (id == IDC_TRAY) ShowWindow(g_main, SW_HIDE);
    else if (id == IDC_RESET) ResetAll();
    else if (id == IDC_MODE_FULL || id == IDC_MODE_COMPACT) { C.compact = (id == IDC_MODE_COMPACT); Apply(); }
    else if (id == IDC_APPLYPOS) {
        C.x = GetNum(Item(0, IDC_X)); C.y = GetNum(Item(0, IDC_Y));
        SetWindowPos(g_hwnd, HWND_TOPMOST, C.x, C.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        Apply();
    }
    else if (id >= IDC_COLBTN && id < IDC_COLBTN + 6) PickColor(id - IDC_COLBTN);
    else if (id >= IDC_SENS && id < IDC_SENS + NSENS) { *SENS[id - IDC_SENS].f = !*SENS[id - IDC_SENS].f; Apply(); }
    else if (id == IDC_HK_APPLY) {
        C.hkT = (int)SendMessageW(Item(2, IDC_HK_T), HKM_GETHOTKEY, 0, 0);
        C.hkE = (int)SendMessageW(Item(2, IDC_HK_E), HKM_GETHOTKEY, 0, 0);
        if (!RegHotkeys()) MessageBoxW(g_main, TR(S_MSG_HOTKEY_FAIL), L"Delquen", MB_ICONWARNING);
        SaveCfg();
    }
    else if (id >= IDC_RATE0 && id < IDC_RATE0 + 3) { C.rate = RATES[id - IDC_RATE0]; RestartTimer(); SaveCfg(); }
    else if (id >= IDC_THEME0 && id < IDC_THEME0 + 3) { C.theme = id - IDC_THEME0; SaveCfg(); ApplyTheme(); }
    else if (id == IDC_TRAYCLOSE) { C.trayClose = !C.trayClose; SaveCfg(); }
    else if (id == IDC_AUTO) { C.autostart = !C.autostart; SetAutostart(C.autostart); SaveCfg(); }
    else if (id == IDC_AUTOSHOW) { C.autoShow = !C.autoShow; SaveCfg(); UpdateShown(); Tick(); }
    else if (id == IDC_LANG) ShowLangMenu();
    InvalidateButtons();
}

static LRESULT CALLBACK MainProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_COMMAND: {
        if (g_sync) return 0;
        int id = LOWORD(w), code = HIWORD(w);
        if ((id == IDC_THR_GOOD || id == IDC_THR_MID) && code == EN_CHANGE) {
            int v = std::clamp(GetNum((HWND)l), 1, 1000);
            if (id == IDC_THR_GOOD) C.thrGood = v; else C.thrMid = v;
            Apply();
        } else if (code == BN_CLICKED && BtnKind(id) >= 0 && (HWND)l) OnClick(id);
        return 0;
    }
    case WM_DRAWITEM: DrawBtn((DRAWITEMSTRUCT*)l); return TRUE;
    case WM_HSCROLL: {
        HWND c = (HWND)l;
        int id = GetDlgCtrlID(c);
        int pos = (int)SendMessageW(c, TBM_GETPOS, 0, 0);
        wchar_t b[16];
        if (id == IDC_SCALE) { C.scale = pos; MakeFonts(); swprintf_s(b, L"%d%%", pos); SetDlgItemTextW(g_pg[0], IDC_SCALE_L, b); }
        else if (id == IDC_OPAC) { C.opac = pos; swprintf_s(b, L"%d%%", pos); SetDlgItemTextW(g_pg[0], IDC_OPAC_L, b); }
        else if (id == IDC_BG) { C.bg = pos; swprintf_s(b, L"%d%%", pos * 100 / 255); SetDlgItemTextW(g_pg[0], IDC_BG_L, b); }
        if (g_visible) Render();
        if (LOWORD(w) == TB_ENDTRACK) SaveCfg();
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r);
        FillRect((HDC)w, &r, g_brBg);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w; HWND c = (HWND)l;
        int id = GetDlgCtrlID(c);
        const Theme& t = TH();
        if (id >= IDC_SW && id < IDC_SW + 6) return (LRESULT)g_swBr[id - IDC_SW];
        bool top = GetParent(c) == g_main;
        SetTextColor(dc, top ? t.dim : t.text);
        SetBkColor(dc, top ? t.bg : t.panel);
        return (LRESULT)(top ? g_brBg : g_brPanel);
    }
    case WM_CTLCOLOREDIT: {
        const Theme& t = TH();
        SetTextColor((HDC)w, t.text); SetBkColor((HDC)w, t.input);
        return (LRESULT)g_brInput;
    }
    case WM_SETTINGCHANGE:
        if (C.theme == 0 && l && !wcscmp((const wchar_t*)l, L"ImmersiveColorSet")) ApplyTheme();
        return 0;
    case WM_SHOWMAIN: ShowMain(); return 0;
    case WM_SETLANG: SetLang((int)w); return 0;
    case WM_CLOSE:
        if (C.trayClose) { ShowWindow(h, SW_HIDE); return 0; }
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* e) {
    Log(L"FALHA: excecao 0x%08X no endereco %p", (unsigned)e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void InvalidParam(const wchar_t*, const wchar_t* func, const wchar_t*, unsigned, uintptr_t) {
    Log(L"PARAMETRO INVALIDO em %ls", func ? func : L"(desconhecido)");   // em vez de terminar o programa
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR cmd, int) {
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Delquen_FPS_MONITOR_SINGLE");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND e = FindWindowW(L"DelquenMain", nullptr);
        if (e) PostMessageW(e, WM_SHOWMAIN, 0, 0);
        return 0;
    }
    g_hi = hi;
    SetProcessDPIAware();
    { HDC sdc = GetDC(nullptr); g_dpi = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(nullptr, sdc); }
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    LoadCfg();
    SetUnhandledExceptionFilter(CrashFilter);
    _set_invalid_parameter_handler(InvalidParam);
    Log(L"=== arranque v2.0, idioma=%d (%ls) ===", g_lang, LANG_NAME[g_lang]);
    { WIN32_FILE_ATTRIBUTE_DATA fa; std::wstring lp = g_ini.substr(0, g_ini.find_last_of(L'\\')) + L"\\debug.log";
      if (GetFileAttributesExW(lp.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 200000) DeleteFileW(lp.c_str()); }
    LoadIgnore();
    EnsureOnScreen();
    g_enabled = C.enabled;
    if (!StartEtw()) { MessageBoxW(nullptr, TR(S_MSG_ETW_FAIL), L"Delquen", MB_ICONERROR); return 1; }
    InitNvml();
    InitDxgiAdapter();

    g_mdc = CreateCompatibleDC(nullptr);
    SetBkMode(g_mdc, TRANSPARENT);
    MakeFonts();

    // overlay
    WNDCLASSW oc = {};
    oc.lpfnWndProc = OverlayProc; oc.hInstance = hi; oc.lpszClassName = L"DelquenOverlay";
    RegisterClassW(&oc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                             oc.lpszClassName, L"Delquen Overlay", WS_POPUP, C.x, C.y, 10, 10, nullptr, nullptr, hi, nullptr);
    ApplyLock();
    // ícone do logo (recurso 1, ver delquen.rc); se o .exe foi compilado sem o recurso, desenha-se um ícone simples
    HICON icBig = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON icSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!icBig) icBig = MakeIcon(32);
    if (!icSm) icSm = MakeIcon(16);

    // janela principal
    WNDCLASSW mc = {};
    mc.lpfnWndProc = MainProc; mc.hInstance = hi; mc.lpszClassName = L"DelquenMain";
    mc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mc.hIcon = icBig;
    RegisterClassW(&mc);
    WNDCLASSW pcw = {};
    pcw.lpfnWndProc = PageProc; pcw.hInstance = hi; pcw.lpszClassName = L"DelquenPage"; pcw.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&pcw);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT rc = {0, 0, D(510), D(500)};
    AdjustWindowRect(&rc, style, FALSE);
    g_main = CreateWindowExW(0, mc.lpszClassName, L"Delquen", style, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hi, nullptr);
    SendMessageW(g_main, WM_SETICON, ICON_SMALL, (LPARAM)icSm);
    ApplyLayoutDirection();
    ApplyTheme();
    BuildUi();
    ApplyTheme();
    Log(L"interface criada");

    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = icSm;
    wcscpy_s(g_nid.szTip, L"Delquen");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    RegHotkeys();
    RegisterHotKey(g_hwnd, 3, 0, VK_F12);
    RestartTimer();
    UpdateShown();
    SyncUi();

    bool tray = cmd && wcsstr(cmd, L"/tray");
    if (!C.asked && !tray) {   // primeira execução: perguntar pelo funcionamento em segundo plano
        int r = MessageBoxW(nullptr,
            TR(S_MSG_FIRST),
            L"Delquen", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | (g_lang == LANG_AR ? MB_RTLREADING | MB_RIGHT : 0));
        C.asked = true;
        if (r == IDYES) { C.autostart = true; C.autoShow = true; C.trayClose = true; SetAutostart(true); }
        else C.autoShow = false;
        SaveCfg();
        UpdateShown();
        SyncUi();
    }
    if (!tray) ShowMain();
    Log(L"pronto (tray=%d)", tray ? 1 : 0);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }

    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    SessionEnd();
    StopEtw();
    CloseHandle(mtx);
    return 0;
}
