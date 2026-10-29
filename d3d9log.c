/* Lab-only logging proxy d3d9.dll for Sparta (AWE engine). Forwards Direct3DCreate9 to the system
   d3d9 and hooks, by vtable patching, IDirect3D9::CreateDevice and a few IDirect3DDevice9 methods
   to log viewport / scissor / clear / present rectangles. Writes d3d9log.txt in the working dir.
   Changes no game behaviour. */
#include <windows.h>

typedef struct {
    UINT W, H; DWORD Fmt; UINT Count; DWORD MS; DWORD MSQ; DWORD Swap; HWND hWnd;
    BOOL Windowed; BOOL AutoDS; DWORD DSFmt; DWORD Flags; UINT Hz; UINT PresInt;
} PP;
typedef struct { DWORD X, Y, W, H; float MinZ, MaxZ; } VP;

static HANDLE g_log = INVALID_HANDLE_VALUE;
static volatile LONG g_lines;

static void L(const char *fmt, ...)
{
    char buf[512]; DWORD n; va_list ap;
    if (g_log == INVALID_HANDLE_VALUE) return;
    if (InterlockedIncrement(&g_lines) > 3000) return;
    va_start(ap, fmt); n = wvsprintfA(buf, fmt, ap); va_end(ap);
    buf[n++] = '\r'; buf[n++] = '\n';
    WriteFile(g_log, buf, n, &n, 0);
}

static void *Patch(void **vt, int idx, void *fn)
{
    DWORD old; void *orig = vt[idx];
    if (VirtualProtect(&vt[idx], 4, PAGE_READWRITE, &old)) { vt[idx] = fn; VirtualProtect(&vt[idx], 4, old, &old); }
    return orig;
}

typedef HRESULT (WINAPI *FnCreateDevice)(void *, UINT, DWORD, HWND, DWORD, PP *, void **);
typedef HRESULT (WINAPI *FnReset)(void *, PP *);
typedef HRESULT (WINAPI *FnPresent)(void *, const RECT *, const RECT *, HWND, void *);
typedef HRESULT (WINAPI *FnClear)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD);
typedef HRESULT (WINAPI *FnSetVP)(void *, const VP *);
typedef HRESULT (WINAPI *FnSetScissor)(void *, const RECT *);

static FnCreateDevice oCreate; static FnReset oReset; static FnPresent oPresent;
static FnClear oClear; static FnSetVP oSetVP; static FnSetScissor oSetSc;
static VP g_lastvp; static LONG g_np, g_nc;
static LONG g_frame, g_on, g_dn, g_up; static DWORD g_fvf;
typedef HRESULT (WINAPI *FnSetFVF)(void *, DWORD);
typedef HRESULT (WINAPI *FnDrawUP)(void *, DWORD, UINT, const void *, UINT);
typedef HRESULT (WINAPI *FnDrawIUP)(void *, DWORD, UINT, UINT, UINT, const void *, DWORD, const void *, UINT);
typedef HRESULT (WINAPI *FnDraw)(void *, DWORD, UINT, UINT);
typedef HRESULT (WINAPI *FnDrawI)(void *, DWORD, INT, UINT, UINT, UINT, UINT);
typedef HRESULT (WINAPI *FnSetTx)(void *, DWORD, const float *);
typedef HRESULT (WINAPI *FnSetVSC)(void *, UINT, const float *, UINT);
static FnSetFVF oSetFVF; static FnDrawUP oDrawUP; static FnDrawIUP oDrawIUP; static FnDraw oDraw; static FnDrawI oDrawI;
static FnSetTx oSetTx; static FnSetVSC oSetVSC;
#define F(x) ((int)((x) * 1000.0f))
typedef HRESULT (WINAPI *FnSetSS)(void *, UINT, void *, UINT, UINT);
static FnSetSS oSetSS; static void *g_vb; static UINT g_vbOff, g_vbStride;
static HRESULT WINAPI hSetSS(void *d, UINT s, void *vb, UINT off, UINT st)
{ if (s == 0) { g_vb = vb; g_vbOff = off; g_vbStride = st; } return oSetSS(d, s, vb, off, st); }
static HRESULT WINAPI hSetFVF(void *d, DWORD f) { g_fvf = f; return oSetFVF(d, f); }
static HRESULT WINAPI hDrawUP(void *d, DWORD t, UINT n, const void *p, UINT st)
{
    if (g_on && (g_fvf & 0x4) && g_up < 60) {
        UINT nv = (t == 5 || t == 6) ? n + 2 : (t == 4 ? n * 3 : n + 1), k;
        g_up++;
        L("DrawUP fvf=%x type=%d prims=%d stride=%d", g_fvf, t, n, st);
        for (k = 0; k < nv && k < 4; k++) {
            const float *v = (const float *)((const BYTE *)p + k * st);
            L("   v%d x=%d y=%d z=%d rhw=%d", k, F(v[0]), F(v[1]), F(v[2]), F(v[3]));
        }
    }
    return oDrawUP(d, t, n, p, st);
}
static HRESULT WINAPI hDrawIUP(void *d, DWORD t, UINT mi, UINT nv, UINT np, const void *id, DWORD ifmt, const void *p, UINT st)
{
    if (0) {
        const float *v = (const float *)p; g_dn++;
        L("DrawIUP fvf=%x type=%d nverts=%d stride=%d v0=(%d,%d,%d,%d)", g_fvf, t, nv, st, F(v[0]), F(v[1]), F(v[2]), F(v[3]));
    }
    return oDrawIUP(d, t, mi, nv, np, id, ifmt, p, st);
}
static HRESULT WINAPI hDraw(void *d, DWORD t, UINT s, UINT n)
{ if (0) { g_dn++; L("DrawPrim fvf=%x type=%d prims=%d", g_fvf, t, n); } return oDraw(d, t, s, n); }
static HRESULT WINAPI hDrawI(void *d, DWORD t, INT bv, UINT mi, UINT nv, UINT sx, UINT n)
{
    if (g_on && (g_fvf & 0x4) && g_vb && g_vbStride >= 16 && g_up < 45) {
        void **vt = *(void ***)g_vb; BYTE *data = 0;
        typedef HRESULT (WINAPI *FnLock)(void *, UINT, UINT, void **, DWORD);
        typedef HRESULT (WINAPI *FnUnlock)(void *);
        if (((FnLock)vt[11])(g_vb, 0, 0, (void **)&data, 0x10) == 0 && data) {
            UINT k; int x0 = 1 << 30, x1 = -(1 << 30), y0 = 1 << 30, y1 = -(1 << 30);
            for (k = 0; k < nv && k < 4000; k++) {
                const float *v = (const float *)(data + g_vbOff + (UINT)(bv + (INT)mi + (INT)k) * g_vbStride);
                int x = (int)v[0], y = (int)v[1];
                if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
            }
            g_up++;
            L("GUI draw nverts=%d prims=%d x=[%d..%d] y=[%d..%d]", nv, n, x0, x1, y0, y1);
            ((FnUnlock)vt[12])(g_vb);
        }
    }
    return oDrawI(d, t, bv, mi, nv, sx, n);
}
static HRESULT WINAPI hSetTx(void *d, DWORD st, const float *m)
{
    if (g_on && g_dn < 70 && st < 4) { g_dn++; L("SetTransform state=%d m00=%d m11=%d m30=%d m31=%d", st, F(m[0]), F(m[5]), F(m[12]), F(m[13])); }
    return oSetTx(d, st, m);
}
static HRESULT WINAPI hSetVSC(void *d, UINT r, const float *p, UINT c)
{
    if (g_on && g_dn < 70 && r < 8) { g_dn++; L("VSConst reg=%d cnt=%d (%d,%d,%d,%d)", r, c, F(p[0]), F(p[1]), F(p[2]), F(p[3])); }
    return oSetVSC(d, r, p, c);
}

static void LogPP(const char *tag, PP *p)
{
    L("%s: back=%dx%d windowed=%d hDevWnd=%x fmt=%d count=%d swap=%d ms=%d hz=%d flags=%x",
      tag, p->W, p->H, p->Windowed, (DWORD)p->hWnd, p->Fmt, p->Count, p->Swap, p->MS, p->Hz, p->Flags);
}

static HRESULT WINAPI hReset(void *d, PP *p) { LogPP("Reset", p); return oReset(d, p); }

static HRESULT WINAPI hPresent(void *d, const RECT *s, const RECT *dst, HWND h, void *dirty)
{
    if (g_np < 4) {
        RECT c = {0,0,0,0}; HWND w = h ? h : GetActiveWindow();
        if (w) GetClientRect(w, &c);
        g_np++;
        L("Present #%d srcRect=%d dstRect=%d hwnd=%x client=%dx%d", g_np, s ? 1 : 0, dst ? 1 : 0, (DWORD)h, c.right, c.bottom);
    }
    g_frame++; g_dn = 0; g_up = 0; g_on = ((g_frame % 500) == 0);
    if (g_on) L("--- frame %d ---", g_frame);
    return oPresent(d, s, dst, h, dirty);
}

static HRESULT WINAPI hClear(void *d, DWORD n, const void *r, DWORD f, DWORD c, float z, DWORD st)
{
    if (g_nc < 3) {
        const RECT *rr = (const RECT *)r; g_nc++;
        if (rr) L("Clear n=%d flags=%x rect=%d,%d,%d,%d", n, f, rr->left, rr->top, rr->right, rr->bottom);
        else    L("Clear n=%d flags=%x (full viewport)", n, f);
    }
    return oClear(d, n, r, f, c, z, st);
}

static HRESULT WINAPI hSetVP(void *d, const VP *v)
{
    if (v->X != g_lastvp.X || v->Y != g_lastvp.Y || v->W != g_lastvp.W || v->H != g_lastvp.H) {
        g_lastvp = *v;
        L("SetViewport X=%d Y=%d W=%d H=%d", v->X, v->Y, v->W, v->H);
    }
    return oSetVP(d, v);
}

static HRESULT WINAPI hSetSc(void *d, const RECT *r)
{
    static RECT last; static int n;
    if (n < 30 && (r->left != last.left || r->right != last.right || r->top != last.top || r->bottom != last.bottom)) {
        last = *r; n++;
        L("SetScissorRect %d,%d,%d,%d", r->left, r->top, r->right, r->bottom);
    }
    return oSetSc(d, r);
}

static int g_devpatched;

static HRESULT WINAPI hCreateDevice(void *self, UINT a, DWORD t, HWND hf, DWORD bh, PP *p, void **out)
{
    HRESULT hr; RECT c = {0,0,0,0};
    if (hf) GetClientRect(hf, &c);
    L("CreateDevice adapter=%d type=%d focus=%x focusClient=%dx%d behavior=%x", a, t, (DWORD)hf, c.right, c.bottom, bh);
    LogPP("  in", p);
    hr = oCreate(self, a, t, hf, bh, p, out);
    LogPP("  out", p);
    L("  hr=%x", hr);
    if (hr == 0 && out && *out && !g_devpatched) {
        void **vt = *(void ***)*out; g_devpatched = 1;
        oReset   = (FnReset)     Patch(vt, 16, hReset);
        oPresent = (FnPresent)   Patch(vt, 17, hPresent);
        oClear   = (FnClear)     Patch(vt, 43, hClear);
        oSetVP   = (FnSetVP)     Patch(vt, 47, hSetVP);
        oSetSc   = (FnSetScissor)Patch(vt, 75, hSetSc);
        oSetTx   = (FnSetTx)  Patch(vt, 44, hSetTx);
        oDraw    = (FnDraw)   Patch(vt, 81, hDraw);
        oDrawI   = (FnDrawI)  Patch(vt, 82, hDrawI);
        oDrawUP  = (FnDrawUP) Patch(vt, 83, hDrawUP);
        oDrawIUP = (FnDrawIUP)Patch(vt, 84, hDrawIUP);
        oSetFVF  = (FnSetFVF) Patch(vt, 89, hSetFVF);
        oSetVSC  = (FnSetVSC) Patch(vt, 94, hSetVSC);
        oSetSS   = (FnSetSS)  Patch(vt, 100, hSetSS);
    }
    return hr;
}

static HMODULE g_real;
typedef void *(WINAPI *PFN)(UINT);

void *WINAPI Direct3DCreate9(UINT v)
{
    PFN f; void *d3d;
    if (!g_real) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\d3d9.dll");
        g_real = LoadLibraryA(path);
    }
    if (!g_real) return 0;
    f = (PFN)GetProcAddress(g_real, "Direct3DCreate9");
    d3d = f ? f(v) : 0;
    if (d3d && !oCreate) oCreate = (FnCreateDevice)Patch(*(void ***)d3d, 16, hCreateDevice);
    return d3d;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x)
{
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        g_log = CreateFileA("d3d9log.txt", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    }
    return TRUE;
}
