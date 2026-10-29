/* Proxy d3d9.dll for Sparta: Ancient Wars (AWE engine): forwards Direct3DCreate9 to the system d3d9 and
   applies in-memory patches to Modules\Sparta.dll (file on disk stays original).
   GUI window base constructor (FUN_1037b360, RVA 0x37B360) defaults the window max size to 2048x1600;
   the GUI root window is therefore clamped to 2048 wide at 1440p+ and mis-placed. Raise the defaults:
     imm32 at RVA 0x37B403: 0x640 (max height 1600) -> 0x1000 (4096)
     imm32 at RVA 0x37B40E: 0x800 (max width 2048)  -> 0x2000 (8192) */
#include <windows.h>

typedef struct { DWORD rva; DWORD from; DWORD to; } PATCH;
static const PATCH g_patches[] = {
    { 0x0037B403, 0x00000640, 0x00001000 },
    { 0x0037B40E, 0x00000800, 0x00002000 },
};

static void ApplyPatches(void)
{
    HMODULE m = GetModuleHandleA("Sparta.dll");
    int i;
    if (!m) return;
    for (i = 0; i < (int)(sizeof(g_patches) / sizeof(g_patches[0])); i++) {
        DWORD *p = (DWORD *)((BYTE *)m + g_patches[i].rva);
        DWORD old;
        if (IsBadReadPtr(p, 4) || *p != g_patches[i].from) continue;
        if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &old)) {
            *p = g_patches[i].to;
            VirtualProtect(p, 4, old, &old);
            FlushInstructionCache(GetCurrentProcess(), p, 4);
        }
    }
}

/* GUI scale: Sparta.dll computes the HUD scale as width * (1/1600) and height * (1/1200) (two float constants,
   each used by one function: FUN_102b7780 / FUN_102b4360). gui_scale.ini [scale] width_div / height_div replace
   the 1600 / 1200 divisors; larger divisor = smaller HUD. */
static void ApplyGuiScale(void)
{
    HMODULE m = GetModuleHandleA("Sparta.dll");
    char path[MAX_PATH]; int i, wd, hd;
    struct { DWORD rva; DWORD orig; int *div; } t[2];
    if (!m) return;
    GetModuleFileNameA(0, path, MAX_PATH);
    for (i = lstrlenA(path); i > 0 && path[i - 1] != '\\'; i--) ;
    lstrcpyA(path + i, "gui_scale.ini");
    wd = (int)GetPrivateProfileIntA("scale", "width_div", 1600, path);
    hd = (int)GetPrivateProfileIntA("scale", "height_div", 1200, path);
    t[0].rva = 0x00927D60; t[0].orig = 0x3A23D70A; t[0].div = &wd;
    t[1].rva = 0x009288C4; t[1].orig = 0x3A5A740E; t[1].div = &hd;
    for (i = 0; i < 2; i++) {
        DWORD *p = (DWORD *)((BYTE *)m + t[i].rva), old; float f;
        if (*t[i].div < 100 || *t[i].div > 20000) continue;
        if (IsBadReadPtr(p, 4) || *p != t[i].orig) continue;
        f = 1.0f / (float)*t[i].div;
        if (VirtualProtect(p, 4, PAGE_READWRITE, &old)) { *p = *(DWORD *)&f; VirtualProtect(p, 4, old, &old); }
    }
}

static HMODULE g_real;
typedef void *(WINAPI *PFN)(UINT);

void *WINAPI Direct3DCreate9(UINT v)
{
    PFN f;
    ApplyPatches();
    ApplyGuiScale();
    if (!g_real) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\d3d9.dll");
        g_real = LoadLibraryA(path);
    }
    if (!g_real) return 0;
    f = (PFN)GetProcAddress(g_real, "Direct3DCreate9");
    return f ? f(v) : 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x)
{
    if (r == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
