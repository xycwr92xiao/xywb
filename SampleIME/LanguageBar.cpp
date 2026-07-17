// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "SampleIME.h"
#include "CompositionProcessorEngine.h"
#include "LanguageBar.h"
#include "Globals.h"
#include "Compartment.h"
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <windowsx.h>
// ======================== 新增全局设置变量 ========================

COLORREF g_toolbarBgColor = Global::toolbarBgColor;// RGB(224, 237, 248);
COLORREF g_toolbarHoverColor = Global::toolbarHoverColor;// RGB(180, 200, 220);

COLORREF g_candidateBgColor = Global::candidateBgColor;// RGB(245, 255, 236);
COLORREF g_candidateTextColor = Global::candidateTextColor;// RGB(0, 0, 0);
COLORREF g_candidateSelectedBgColor = Global::candidateSelectedBgColor; //RGB(0, 120, 215)
COLORREF g_candidateSelectedTextColor = Global::candidateSelectedTextColor;// RGB(255, 255, 255);
BOOL g_isChineseMode = TRUE;
BOOL g_isPinyinMode = FALSE;
BOOL g_isPyAndWbMode = FALSE;
BOOL g_isHorizontal = TRUE;
BOOL g_isShowToolBar = FALSE;
int g_maxCandidatesHorizontal = 5;
int g_maxCandidatesVertical = 10;
BOOL g_showRemainingCode = TRUE;
WCHAR g_shortcutKey = L'W';
BOOL _bChangeHV = FALSE;
BOOL CLangBarItemButton::s_bRecreating = FALSE;
// ======================== 注册表存取函数 ========================

// 获取 DLL 所在目录，拼接 xywb.ini 路径
static BOOL GetIniPath(WCHAR* pszPath, DWORD cchSize)
{

    if (FAILED(StringCchCopy(pszPath, cchSize, g_szDllPath)))
        return FALSE;
    if (FAILED(StringCchCat(pszPath, cchSize, L"xywb.ini")))
        return FALSE;
    return TRUE;
}

// 从 INI 加载所有配置（在 DllMain 或 CSampleIME 初始化时调用一次）
void LoadIniSettings()
{
    WCHAR szIni[MAX_PATH];
    if (!GetIniPath(szIni, MAX_PATH))
        return;
    if (GetFileAttributes(szIni) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugString(L"xywb.ini not found, using default config.\n");
        return;
    }
    // ---- [Settings] ----
    int val = GetPrivateProfileInt(L"Settings", L"ShowToolsBar", 0, szIni);
    g_isShowToolBar = (val == 1);   //

    val = GetPrivateProfileInt(L"Settings", L"HorizontalToolsBar", 1, szIni);
    g_isHToolbarWin = (val == 1);   //

    val = GetPrivateProfileInt(L"Settings", L"PinyinMode", 0, szIni);
    g_isPinyinMode = (val == 1);   // 0=五笔, 1=拼音

    val = GetPrivateProfileInt(L"Settings", L"PyAndWbMode", 0, szIni);
    g_isPyAndWbMode = (val == 1);

    val = GetPrivateProfileInt(L"Settings", L"ChineseMode", 1, szIni);
    g_isChineseMode = (val == 1);

    val = GetPrivateProfileInt(L"Settings", L"HorizontalMode", 0, szIni);
    g_isHorizontal = (val == 1);   // 0=纵向, 1=横向

    val = GetPrivateProfileInt(L"Settings", L"MaxHorizontalItems", 5, szIni);
    g_maxCandidatesHorizontal = max(3, min(10, val));

    val = GetPrivateProfileInt(L"Settings", L"MaxVerticalItems", 10, szIni);
    g_maxCandidatesVertical = max(3, min(10, val));

    val = GetPrivateProfileInt(L"Settings", L"ShowRemainingCode", 1, szIni);
    g_showRemainingCode = (val == 1);

    // 颜色（以十六进制字符串读取，如 "0xE0EDF8"）
    WCHAR szColor[16];
    auto ReadColor = [&](LPCWSTR key, COLORREF defaultClr) -> COLORREF {
        if (GetPrivateProfileString(L"Settings", key, L"", szColor, 16, szIni) > 0)
        {
            // 去掉可能的 "0x" 前缀
            if (wcsncmp(szColor, L"0x", 2) == 0)
                return (COLORREF)wcstoul(szColor + 2, NULL, 16);
            else
                return (COLORREF)wcstoul(szColor, NULL, 16);
        }
        return defaultClr;
        };
    g_candidateBgColor = ReadColor(L"CandBgColor", Global::candidateBgColor);
    g_candidateTextColor = ReadColor(L"CandTextColor", Global::candidateTextColor);
    g_candidateSelectedBgColor = ReadColor(L"CandSelBgColor", Global::candidateSelectedBgColor);
    g_candidateSelectedTextColor = ReadColor(L"CandSelTextColor", Global::candidateSelectedTextColor);
    g_toolbarBgColor = ReadColor(L"ToolbarBgColor", Global::toolbarBgColor);
    g_toolbarHoverColor = ReadColor(L"ToolbarHoverColor", Global::toolbarHoverColor);

    // ---- [HotKeys] ----
    WCHAR szHotKey[64];
    if (GetPrivateProfileString(L"HotKeys", L"UserWordDialog", L"Ctrl+W", szHotKey, 64, szIni) > 0)
    {
        // 解析最后一个字符作为快捷键字母（假设格式为 "Ctrl+X"）
        size_t len = wcslen(szHotKey);
        if (len > 0)
            g_shortcutKey = szHotKey[len - 1];
        else
            g_shortcutKey = L'W';
    }
    else
        g_shortcutKey = L'W';
}

// 保存所有配置到 INI
void SaveIniSettings()
{
    WCHAR szIni[MAX_PATH];
    if (!GetIniPath(szIni, MAX_PATH))
        return;

    // ---- [Settings] ----
    WCHAR szBuf[32];
    wsprintf(szBuf, L"%d", g_isPinyinMode ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"PinyinMode", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_isShowToolBar ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"ShowToolsBar", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_isHToolbarWin ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"HorizontalToolsBar", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_isPyAndWbMode ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"PyAndWbMode", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_isChineseMode ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"ChineseMode", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_isHorizontal ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"HorizontalMode", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_maxCandidatesHorizontal);
    WritePrivateProfileString(L"Settings", L"MaxHorizontalItems", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_maxCandidatesVertical);
    WritePrivateProfileString(L"Settings", L"MaxVerticalItems", szBuf, szIni);

    wsprintf(szBuf, L"%d", g_showRemainingCode ? 1 : 0);
    WritePrivateProfileString(L"Settings", L"ShowRemainingCode", szBuf, szIni);

    // 颜色保存为十六进制 "0xRRGGBB"
    auto WriteColor = [&](LPCWSTR key, COLORREF clr) {
        wsprintf(szBuf, L"0x%06X", clr);
        WritePrivateProfileString(L"Settings", key, szBuf, szIni);
        };
    WriteColor(L"CandBgColor", g_candidateBgColor);
    WriteColor(L"CandTextColor", g_candidateTextColor);
    WriteColor(L"CandSelBgColor", g_candidateSelectedBgColor);
    WriteColor(L"CandSelTextColor", g_candidateSelectedTextColor);
    WriteColor(L"ToolbarBgColor", g_toolbarBgColor);
    WriteColor(L"ToolbarHoverColor", g_toolbarHoverColor);
    Global::candidateBgColor = g_candidateBgColor;//RGB(245, 255, 236);
    Global::candidateSelectedBgColor = g_candidateSelectedBgColor;//RGB(0, 120, 215);
    Global::candidateSelectedTextColor = g_candidateSelectedTextColor;//RGB(255, 255, 255);
    // ---- [HotKeys] ----
    wsprintf(szBuf, L"Ctrl+%c", g_shortcutKey);
    WritePrivateProfileString(L"HotKeys", L"UserWordDialog", szBuf, szIni);
}
// ======================== 完整 CToolbarWindow 类 ========================

#define TOOLBAR_WIDTH   220
#define TOOLBAR_HEIGHT  40
#define DRAG_AREA_WIDTH 30
#define BUTTON_WIDTH    30
#define BUTTON_HEIGHT   30
#define ICON_SIZE       24
#define ‌ROUND_CORNER    12//圆角
#define TB_CMD_IME       100   // 索引0：中英文切换
#define TB_CMD_INPUT     101   // 索引1：输入模式切换（五笔/拼音/混合）
#define TB_CMD_DOUBLE    102   // 索引2：全角/半角
#define TB_CMD_PUNCT     103   // 索引3：标点符号
#define TB_CMD_USERWORD  104   // 索引4：手工造词
#define TB_CMD_SETTINGS  105   // 索引5：设置（原第6个按钮）
#define WM_USER_RECREATE  (WM_USER + 101)
class CToolbarWindow
{
public:
    CToolbarWindow(CSampleIME* pIME, CLangBarItemButton* pOwner);
    ~CToolbarWindow();
    HWND _GetHwnd() const { return _hWnd; }
    BOOL Create(HWND hParent);
    void Show(BOOL bShow);
    BOOL IsVisible() const;
    void UpdateAllButtons();
    void _OnCompartmentChanged(REFGUID guid);
    void _DrawButton(HDC hdc, int idx, const RECT* rc, DWORD state);
private:
    CSampleIME* _pIME;
    HWND _hWnd;
    HWND hwndForeground;
    HWND _hButtons[6];
    HICON _hIcons[6][2];
    CCompartmentEventSink* _pCompSink[4];

    // 悬停状态（由父窗口跟踪）
    // 鼠标跟踪
    BOOL _bTracking;

    void _InitIcons();
    void _CreateButtons(HWND hParent);
    void _UpdateButton(int idx);
    void _OnButtonClick(int idx);
    BOOL _bHover[6];
    WNDPROC _oldButtonProc[6];                          // 保存原窗口过程
    static LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    // 父窗口消息处理
    static LRESULT CALLBACK _WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT _OnCommand(WPARAM wParam, LPARAM lParam);
    LRESULT _OnDrawItem(WPARAM wParam, LPARAM lParam);
    void _ShowContextMenu(POINT pt);
    CLangBarItemButton* _pOwner;  // 新增成员
    // 跟踪鼠标，实现悬停
    void _TrackMouse(BOOL bStart);
    int  _HitTestButton(POINT pt);  // 返回按钮索引，-1表示无
    BOOL _bInitialized;   // 是否已初始化
};

// ---- 实现 ----

CToolbarWindow::CToolbarWindow(CSampleIME* pIME, CLangBarItemButton* pOwner)
    : _pIME(pIME), _pOwner(pOwner), _hWnd(nullptr), _bTracking(FALSE), hwndForeground(nullptr)
{
    for (int i = 0; i < 6; ++i) _bHover[i] = FALSE;
    for (int i = 0; i < 6; ++i) {
        _hButtons[i] = nullptr;
    }
    for (int i = 0; i < 4; ++i) _pCompSink[i] = nullptr;
    for (int i = 0; i < 6; ++i) for (int j = 0; j < 2; ++j) _hIcons[i][j] = nullptr;
    _InitIcons();
    hwndForeground = ::GetForegroundWindow();
}

CToolbarWindow::~CToolbarWindow()
{
    for (int i = 0; i < 4; ++i)
    {
        if (_pCompSink[i])
        {
            _pCompSink[i]->_Unadvise();
            delete _pCompSink[i];
            _pCompSink[i] = nullptr;
        }
    }
}

void CToolbarWindow::_InitIcons()
{
    _hIcons[0][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_MODE_OFF), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[0][1] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_MODE_ON), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[1][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_WUBI), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[1][1] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_PINYIN), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[2][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_DOUBLE_SINGLE_BYTE_OFF), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[2][1] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_DOUBLE_SINGLE_BYTE_ON), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[3][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_PUNCTUATION_OFF), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[3][1] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_PUNCTUATION_ON), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[4][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_EMOJI), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[4][1] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_PINYINWUBI), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);  //借用存储一下混合状态图标
    _hIcons[5][0] = (HICON)LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(IDI_TOOL_SETTING), IMAGE_ICON, ICON_SIZE, ICON_SIZE, LR_DEFAULTCOLOR);
    _hIcons[5][1] = _hIcons[5][0];
}

static HRESULT ToolbarCompartmentCallback(void* pv, REFGUID guid)
{
    CToolbarWindow* pThis = (CToolbarWindow*)pv;
    if (pThis) pThis->_OnCompartmentChanged(guid);
    return S_OK;
}
#define ROUND_CORNER 12   // 圆角半径，可根据需要调整
#define WM_USER_MOVE_TOOLBAR (WM_USER + 100)
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
BOOL CToolbarWindow::Create(HWND hParent)
{
    static ATOM s_atom = 0;
    if (s_atom == 0)
    {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = _WndProc;
        wc.hInstance = Global::dllInstanceHandle;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(Global::toolbarBgColor);
        wc.lpszClassName = L"XinYuToolbarClass";
        s_atom = RegisterClassEx(&wc);
        if (!s_atom) return FALSE;
    }
    RECT rcWork;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);

    // 根据方向决定窗口尺寸
    int width = g_isHToolbarWin ? TOOLBAR_WIDTH : TOOLBAR_HEIGHT;
    int height = g_isHToolbarWin ? TOOLBAR_HEIGHT : TOOLBAR_WIDTH;
    if (g_tx == 0 && g_ty == 0)
    {
        g_tx = rcWork.right - width;
        g_ty = rcWork.bottom - height;
    }
    if (_bChangeHV) {
        if (g_isHToolbarWin) {
            g_tx = g_tx + TOOLBAR_HEIGHT - TOOLBAR_WIDTH;
            g_ty = g_ty + TOOLBAR_WIDTH - TOOLBAR_HEIGHT;

        }
        else {// 横向转纵向
            g_tx = g_tx - TOOLBAR_HEIGHT + TOOLBAR_WIDTH;
            g_ty = g_ty - TOOLBAR_WIDTH + TOOLBAR_HEIGHT;
        }
		_bChangeHV = FALSE;
    }
    // 修正位置，确保窗口完全在屏幕工作区内
    if (g_tx + width > rcWork.right)  g_tx = rcWork.right - width;
    if (g_ty + height > rcWork.bottom) g_ty = rcWork.bottom - height;
    if (g_tx < rcWork.left) g_tx = rcWork.left;
    if (g_ty < rcWork.top)  g_ty = rcWork.top;

    // 创建窗口（宽高使用上面计算的 width/height）
    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        (LPCTSTR)s_atom, L"", WS_POPUP | WS_THICKFRAME,
        g_tx, g_ty, width, height,
        hParent, NULL, Global::dllInstanceHandle, this);

    if (!hWnd) return FALSE;

    _hWnd = hWnd;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
    // 启用系统圆角（Win11+）
    HMODULE hDwm = GetModuleHandle(L"dwmapi.dll");
    if (hDwm) {
        auto pDwmSetWindowAttribute = (decltype(&DwmSetWindowAttribute))GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute) {
            DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
            pDwmSetWindowAttribute(_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
        }
    }

    SetWindowPos(hWnd, HWND_TOPMOST, g_tx, g_ty, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // 创建按钮（但不可见）
    _CreateButtons(hWnd);

    // 注意：不设置区域、透明，不注册 Compartment，这些将在首次 Show 时完成
    return TRUE;
}
void CToolbarWindow::Show(BOOL bShow)
{
    if (!_hWnd) return;

    if (bShow)
    {
        RECT rcWork;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0);
        int width = g_isHToolbarWin ? TOOLBAR_WIDTH : TOOLBAR_HEIGHT;
        int height = g_isHToolbarWin ? TOOLBAR_HEIGHT : TOOLBAR_WIDTH;
        int x = max(rcWork.left, min(g_tx, rcWork.right - width));
        int y = max(rcWork.top, min(g_ty, rcWork.bottom - height));
        if (x != g_tx || y != g_ty) {
            g_tx = x;
            g_ty = y;
        }

        // 首次显示时执行一次性初始化（区域、透明、Compartment 注册）
        if (!_bInitialized)
        {
            // 注册 Compartment 事件
            ITfThreadMgr* pTM = _pIME->_GetThreadMgr();
            if (pTM)
            {
                GUID guids[4] = {
                    GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                    Global::SampleIMEGuidCompartmentWubiPinyin,
                    Global::SampleIMEGuidCompartmentDoubleSingleByte,
                    Global::SampleIMEGuidCompartmentPunctuation
                };
                for (int i = 0; i < 4; ++i)
                {
                    _pCompSink[i] = new CCompartmentEventSink(ToolbarCompartmentCallback, static_cast<void*>(this));
                    if (_pCompSink[i])
                        _pCompSink[i]->_Advise(pTM, guids[i]);
                }
            }

            _bInitialized = TRUE;
        }

        // 显示主窗口（如果尚未可见）
        if (!IsVisible())
            ShowWindow(_hWnd, SW_SHOWNOACTIVATE);

        // 显示所有按钮（它们之前被创建但不可见）
        for (int i = 0; i < 6; ++i)
        {
            if (_hButtons[i] && !(GetWindowLong(_hButtons[i], GWL_STYLE) & WS_VISIBLE))
                ShowWindow(_hButtons[i], SW_SHOW);
        }

        // 刷新按钮状态
        UpdateAllButtons();

        // 异步移动到正确位置
       // ★ 关键：同步置顶并移动到正确位置（确保窗口始终在最前且位置正确）
        SetWindowPos(_hWnd, HWND_TOPMOST, g_tx, g_ty, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    else
    {
        RECT rect;
        if (::GetWindowRect(_hWnd, &rect)) {
            g_tx = rect.left;
            g_ty = rect.top;
        }
        ShowWindow(_hWnd, SW_HIDE);
        for (int i = 0; i < 6; ++i)
            ShowWindow(_hButtons[i], SW_HIDE);
    }
}
void CToolbarWindow::_CreateButtons(HWND hParent)
{
    int x = DRAG_AREA_WIDTH;
    int y = (TOOLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
    if (!g_isHToolbarWin) {
        x = (TOOLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
        y = DRAG_AREA_WIDTH;
    }
    for (int i = 0; i < 6; ++i)
    {
        _hButtons[i] = CreateWindow(
            L"BUTTON", L"",
            WS_CHILD | BS_PUSHBUTTON,   // 没有 WS_VISIBLE
            x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
            hParent,
            (HMENU)(INT_PTR)(TB_CMD_IME + i),
            Global::dllInstanceHandle, NULL);
        _oldButtonProc[i] = (WNDPROC)SetWindowLongPtr(_hButtons[i], GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
        SetWindowLongPtr(_hButtons[i], GWLP_USERDATA, (LONG_PTR)this);
        if (g_isHToolbarWin)x += BUTTON_WIDTH;
        else y += BUTTON_HEIGHT;
    }
}

LRESULT CALLBACK CToolbarWindow::ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CToolbarWindow* pThis = (CToolbarWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pThis) return DefWindowProc(hWnd, uMsg, wParam, lParam);

    int idx = (int)(GetWindowLongPtr(hWnd, GWLP_ID) - TB_CMD_IME);
    if (idx < 0 || idx >= 6) return DefWindowProc(hWnd, uMsg, wParam, lParam);
    // 新增：拦截按钮背景颜色，统一工具栏底色
    if (uMsg == WM_CTLCOLORBTN)
    {
        HDC hdcBtn = (HDC)wParam;
        SetBkMode(hdcBtn, TRANSPARENT);
        // 返回工具栏背景画刷，按钮空白区域全部继承窗口底色
        static HBRUSH hBtnBg = CreateSolidBrush(Global::toolbarBgColor);
        return (LRESULT)hBtnBg;
    }
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        pThis->_DrawButton(hdc, idx, &rc, 0);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (!pThis->_bHover[idx])
        {
            pThis->_bHover[idx] = TRUE;
            InvalidateRect(hWnd, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE:
    {
        if (pThis->_bHover[idx])
        {
            pThis->_bHover[idx] = FALSE;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }
    }
    return CallWindowProc(pThis->_oldButtonProc[idx], hWnd, uMsg, wParam, lParam);
}

void CToolbarWindow::_DrawButton(HDC hdc, int idx, const RECT* rc, DWORD state)
{
    BOOL bHover = _bHover[idx];
    COLORREF bgColor = Global::toolbarBgColor;
    if (state & ODS_SELECTED)
        bgColor = RGB(160, 180, 200);
    else if (bHover || (state & ODS_HOTLIGHT))
        bgColor = Global::toolbarHoverColor;

    HBRUSH hBrush = CreateSolidBrush(bgColor);
    if (bHover || (state & ODS_HOTLIGHT))
    {
        const int radius = 6;
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hOldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
    }
    else
    {
        FillRect(hdc, rc, hBrush);
    }
    DeleteObject(hBrush);

    BOOL bState = FALSE;
    HICON hIconToDraw = nullptr;
    switch (idx)
    {
    case 0:
    {
        CCompartment comp(_pIME->_GetThreadMgr(), _pIME->_GetClientId(), GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
        comp._GetCompartmentBOOL(bState);
        break;
    }
    case 1:
    {
        if (Global::isPinyinMode && !Global::isPyAndWbMode) {
            hIconToDraw = _hIcons[1][1];
        }
        else if (!Global::isPinyinMode) {
            hIconToDraw = _hIcons[1][0];
        }
        else if (Global::isPinyinMode && Global::isPyAndWbMode) {
            hIconToDraw = _hIcons[4][1];
        }
        break;
    }
    case 2:
    {
        CCompartment comp(_pIME->_GetThreadMgr(), _pIME->_GetClientId(), Global::SampleIMEGuidCompartmentDoubleSingleByte);
        comp._GetCompartmentBOOL(bState);
        break;
    }
    case 3:
    {
        CCompartment comp(_pIME->_GetThreadMgr(), _pIME->_GetClientId(), Global::SampleIMEGuidCompartmentPunctuation);
        comp._GetCompartmentBOOL(bState);
        break;
    }
    default:
        bState = FALSE;
    }

    if (idx != 1) {
        hIconToDraw = (idx <= 3) ? _hIcons[idx][bState ? 1 : 0] : _hIcons[idx][0];
    }
    if (hIconToDraw)
    {
        DrawIconEx(hdc,
            rc->left + (rc->right - rc->left - ICON_SIZE) / 2,
            rc->top + (rc->bottom - rc->top - ICON_SIZE) / 2,
            hIconToDraw, ICON_SIZE, ICON_SIZE,
            0, NULL, DI_NORMAL);
    }
}

LRESULT CToolbarWindow::_OnDrawItem(WPARAM /*wParam*/, LPARAM lParam)
{
    LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
    if (lpDIS->CtlType != ODT_BUTTON) return FALSE;
    int idx = (int)(lpDIS->CtlID - TB_CMD_IME);
    if (idx < 0 || idx >= 6) return FALSE;
    _DrawButton(lpDIS->hDC, idx, &lpDIS->rcItem, lpDIS->itemState);
    return TRUE;
}

int CToolbarWindow::_HitTestButton(POINT pt)
{
    for (int i = 0; i < 6; ++i)
    {
        if (_hButtons[i] && IsWindowVisible(_hButtons[i]))
        {
            RECT rc;
            GetWindowRect(_hButtons[i], &rc);
            if (PtInRect(&rc, pt))
                return i;
        }
    }
    return -1;
}

void CToolbarWindow::_TrackMouse(BOOL bStart)
{
    if (bStart && !_bTracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, _hWnd, 0 };
        TrackMouseEvent(&tme);
        _bTracking = TRUE;
    }
    else if (!bStart)
    {
        _bTracking = FALSE;
    }
}

#define WM_USER_MOVE_TOOLBAR (WM_USER + 100)

LRESULT CALLBACK CToolbarWindow::_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CToolbarWindow* pThis = (CToolbarWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pThis)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_USER_RECREATE:
    {
        _bChangeHV = TRUE;
        if (pThis->_pOwner)
            pThis->_pOwner->RecreateToolbar();
        return 0;
    }
    case WM_USER_MOVE_TOOLBAR:
    {
        int x = (int)wParam;
        int y = (int)lParam;
        SetWindowPos(pThis->_hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_NCRBUTTONDOWN:
    {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        POINT ptClient = pt;
        ScreenToClient(hWnd, &ptClient);
        if (ptClient.x < DRAG_AREA_WIDTH && ptClient.y >= 0 && ptClient.y < TOOLBAR_HEIGHT)
        {
            pThis->_ShowContextMenu(pt);
            return 0;
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        // 静态画刷避免频繁创建销毁GDI资源
        static HBRUSH hToolBgBrush = nullptr;
        if (!hToolBgBrush)
            hToolBgBrush = CreateSolidBrush(Global::toolbarBgColor);
        else
        {
            // 颜色修改时重建画刷
            COLORREF clr;
            LOGBRUSH lb;
            GetObject(hToolBgBrush, sizeof(lb), &lb);
            clr = lb.lbColor;
            if (clr != Global::toolbarBgColor)
            {
                DeleteObject(hToolBgBrush);
                hToolBgBrush = CreateSolidBrush(Global::toolbarBgColor);
            }
        }
        FillRect(hdc, &rcClient, hToolBgBrush);
        return TRUE;
    }
    case WM_NCHITTEST:
    {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hWnd, &pt);
        if (pt.x < DRAG_AREA_WIDTH && pt.y >= 0 && pt.y < TOOLBAR_HEIGHT)
            return HTCAPTION;
        break;
    }
    case WM_MOVE:
    {
        // 窗口被拖动或移动后，保存新位置
        if (pThis->_hWnd && IsWindowVisible(pThis->_hWnd))
        {
            RECT rc;
            GetWindowRect(pThis->_hWnd, &rc);
            g_tx = rc.left;
            g_ty = rc.top;
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // 绘制拖动图标（不变）
        RECT rc = { 0, 0, DRAG_AREA_WIDTH, TOOLBAR_HEIGHT };
        HICON hIcon = (HICON)LoadImage(Global::dllInstanceHandle,
            MAKEINTRESOURCE(g_isHToolbarWin ? IDI_DRAGH : IDI_DRAGV),
            IMAGE_ICON, 24, 24, LR_DEFAULTCOLOR);
        if (hIcon) {
            DrawIconEx(hdc, g_isHToolbarWin ? 5 : (TOOLBAR_HEIGHT - 24) / 2, g_isHToolbarWin ? (TOOLBAR_HEIGHT - 24) / 2 : 5, hIcon, 24, 24, 0, NULL, DI_NORMAL);
            DestroyIcon(hIcon);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DRAWITEM:
        return pThis->_OnDrawItem(wParam, lParam);
    case WM_COMMAND:
        return pThis->_OnCommand(wParam, lParam);
    case WM_CONTEXTMENU:
    {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        pThis->_ShowContextMenu(pt);
        return 0;
    }
    case WM_DESTROY:
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
        break;
    }
    case WM_NCCALCSIZE:
    {
        if (wParam == TRUE)
        {
            NCCALCSIZE_PARAMS* pParams = (NCCALCSIZE_PARAMS*)lParam;
            // 客户区 = 整个窗口，但顶部留 1 像素边框
            pParams->rgrc[0].top += 1;
            return WVR_REDRAW;
        }
        break;
    }
    case WM_ACTIVATE:
    {
        // wParam == WA_ACTIVE 窗口激活；WA_INACTIVE 失活
        InvalidateRect(hWnd, NULL, TRUE); // 全部客户区重绘，擦除旧灰色
        UpdateWindow(hWnd); // 立即执行WM_PAINT+WM_ERASEBKGND填充底色
        break;
    }
    //case WM_NCACTIVATE:
    //{
    //    InvalidateRect(hWnd, NULL, TRUE);
    //    UpdateWindow(hWnd);
    //    break;
    //}
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void CToolbarWindow::_ShowContextMenu(POINT pt)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 1, L"关闭工具栏");
    AppendMenu(hMenu, MF_STRING, 7, g_isHToolbarWin ? L"水平->垂直" : L"垂直->水平");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, 2, L"表情符号");
    AppendMenu(hMenu, MF_STRING, 3, L"手工造词");
    AppendMenu(hMenu, MF_STRING, 4, L"设置");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, 5, Global::showRemainingCode ? L"显示编码剩余(开)" : L"显示剩余编码(关)");
    AppendMenu(hMenu, MF_STRING, 6, Global::isHorizontalMode ? L"候选窗口变垂直" : L"候选窗口变水平");
    CheckMenuItem(hMenu, 5, Global::showRemainingCode ? MF_CHECKED : MF_UNCHECKED);
    HWND hParent = hwndForeground;
    if (hParent == NULL) hParent = GetDesktopWindow();

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hParent, NULL);
    if (cmd == 1)
    {
        g_isVisibleToolBar = FALSE;
        Show(g_isVisibleToolBar);
    }
    else if (cmd == 6)
    {
        if (_pIME) { _pIME->CloseCandidateAndCommit(); }
        Global::isHorizontalMode = !Global::isHorizontalMode;

    }
    else if (cmd == 2)
    {
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine)
            pEngine->ShowEmojiDialog();
    }
    else if (cmd == 3)
    {
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine)
            pEngine->ShowUserWordDialog();
    }
    else if (cmd == 4)
    {
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine)
            pEngine->ShowSettingsDialog();
    }
    else if (cmd == 5)
    {
        //MessageBox(NULL, L"待添加功能项，敬请期待 ......", L"昕宇提示", MB_OK | MB_ICONINFORMATION);
        if (_pIME) { _pIME->CloseCandidateAndCommit(); }
        Global::showRemainingCode = !Global::showRemainingCode;
        CheckMenuItem(hMenu, 5, Global::showRemainingCode ? MF_CHECKED : MF_UNCHECKED);
    }
    else if (cmd == 7)
    {
        g_isHToolbarWin = !g_isHToolbarWin;
        _bChangeHV = TRUE;
        ::PostMessage(_hWnd, WM_USER_RECREATE, 0, 0);
    }
    DestroyMenu(hMenu);
    if (hwndForeground && IsWindow(hwndForeground))
    {
        // 注意：SetForegroundWindow 受系统限制，但在同一输入会话中通常可行
        SetForegroundWindow(hwndForeground);
        // 可选：为了确保输入焦点，可同时向该窗口发送 WM_SETFOCUS 消息
        // PostMessage(hwndForeground, WM_SETFOCUS, 0, 0);
    }
}

LRESULT CToolbarWindow::_OnCommand(WPARAM wParam, LPARAM /*lParam*/)
{
    int cmd = LOWORD(wParam);
    if (cmd >= TB_CMD_IME && cmd <= TB_CMD_SETTINGS)
    {
        int idx = cmd - TB_CMD_IME;
        _OnButtonClick(idx);
        return 0;
    }
    return DefWindowProc(_hWnd, WM_COMMAND, wParam, 0);
}

void CToolbarWindow::_OnButtonClick(int idx)
{
    if (!_pIME) return;
    ITfThreadMgr* pTM = _pIME->_GetThreadMgr();
    TfClientId cid = _pIME->_GetClientId();
    if (!pTM || cid == TF_CLIENTID_NULL) return;

    switch (idx)
    {
    case 0:
    {
        CCompartment comp(pTM, cid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
        BOOL state = FALSE;
        comp._GetCompartmentBOOL(state);
        comp._SetCompartmentBOOL(!state);
        Global::isChineseMode = !state;
        CCompartment comp2(pTM, cid, Global::SampleIMEGuidCompartmentPunctuation);//切换中文同时，切换标点符号状态
        comp2._SetCompartmentBOOL(!state);
        break;
    }
    case 1:
    {
        if (_pIME)   {       _pIME->CloseCandidateAndCommit();  }
        int state = !Global::isPinyinMode ? 0 : (Global::isPyAndWbMode ? 2 : 1);
        state++;
        if (state == 3) state = 0;
        if (state == 0) {
            Global::isPinyinMode = FALSE;
            Global::isPyAndWbMode = FALSE;
        }
        else if (state == 1) {
            Global::isPinyinMode = TRUE;
            Global::isPyAndWbMode = FALSE;
        }
        else if (state == 2) {
            Global::isPinyinMode = TRUE;
            Global::isPyAndWbMode = TRUE;
        }
        Global::isHorizontalMode = (state != 1);
        _UpdateButton(1);
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine) {
            Beep(1000, 100);
            pEngine->RefreshLanguageBarIcon();
            pEngine->ReloadDictionaries();
        }
        break;
    }
    case 2:
    {
        CCompartment comp(pTM, cid, Global::SampleIMEGuidCompartmentDoubleSingleByte);
        BOOL state = FALSE;
        comp._GetCompartmentBOOL(state);
        comp._SetCompartmentBOOL(!state);
        break;
    }
    case 3:
    {
        CCompartment comp(pTM, cid, Global::SampleIMEGuidCompartmentPunctuation);
        BOOL state = FALSE;
        comp._GetCompartmentBOOL(state);
        comp._SetCompartmentBOOL(!state);
        break;
    }
    case 4:
    {
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine)
            pEngine->ShowUserWordDialog();
        break;
    }
    case 5:
    {
        CCompositionProcessorEngine* pEngine = _pIME->GetCompositionProcessorEngine();
        if (pEngine)
            pEngine->ShowSettingsDialog();
        break;
    }
    }
    UpdateAllButtons();
    if (hwndForeground && IsWindow(hwndForeground))
    {
        SetForegroundWindow(hwndForeground);
        // 可选：为了确保输入焦点，可同时向该窗口发送 WM_SETFOCUS 消息
        // PostMessage(hwndForeground, WM_SETFOCUS, 0, 0);
    }
}

void CToolbarWindow::UpdateAllButtons()
{
    for (int i = 0; i < 6; ++i)
        if (_hButtons[i])
            InvalidateRect(_hButtons[i], NULL, FALSE);
}

void CToolbarWindow::_UpdateButton(int idx)
{
    if (idx >= 0 && idx < 6 && _hButtons[idx])
        InvalidateRect(_hButtons[idx], NULL, FALSE);
}

void CToolbarWindow::_OnCompartmentChanged(REFGUID guid)
{
    if (IsEqualGUID(guid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE))
        _UpdateButton(0);
    else if (IsEqualGUID(guid, Global::SampleIMEGuidCompartmentDoubleSingleByte))
        _UpdateButton(2);
    else if (IsEqualGUID(guid, Global::SampleIMEGuidCompartmentPunctuation))
        _UpdateButton(3);
}



BOOL CToolbarWindow::IsVisible() const
{
    return (_hWnd && IsWindowVisible(_hWnd));
}
// 匿名命名空间（内部链接）

namespace {
#ifndef TCM_SETBKCOLOR
#define TCM_SETBKCOLOR (TCM_FIRST + 17)
#endif

    // 设置对话框控件 ID（避免与现有冲突）
// 设置对话框控件 ID
#define IDC_SETTINGS_CHECK_CHINESE       3001
#define IDC_SETTINGS_COMBO_INPUTMODE     3002
#define IDC_SETTINGS_EDIT_SHORTCUT       3003

#define IDC_SETTINGS_COMBO_ARRANGE       3101
#define IDC_SETTINGS_EDIT_HORIZONTAL     3102
#define IDC_SETTINGS_EDIT_VERTICAL       3103
#define IDC_SETTINGS_CHECK_SHOWREMAIN    3104

#define IDC_SETTINGS_BTN_CAND_BG         3201
#define IDC_SETTINGS_BTN_CAND_TEXT       3202
#define IDC_SETTINGS_BTN_CAND_SELBG      3203
#define IDC_SETTINGS_BTN_CAND_SELTEXT    3204
#define IDC_SETTINGS_BTN_TOOLBAR_BG      3205
#define IDC_SETTINGS_BTN_TOOLBAR_HOVER   3206

// 静态文本控件 ID
#define IDC_SETTINGS_STATIC_INPUTMODE    3301
#define IDC_SETTINGS_STATIC_SHORTCUT     3302
#define IDC_SETTINGS_STATIC_ARRANGE      3303
#define IDC_SETTINGS_STATIC_HORIZONTAL   3304
#define IDC_SETTINGS_STATIC_VERTICAL     3305
#define IDC_SETTINGS_STATIC_HORIZ_NOTE   3306
#define IDC_SETTINGS_STATIC_VERT_NOTE    3307
#define IDC_SETTINGS_STATIC_CANDCOLOR    3308
#define IDC_SETTINGS_STATIC_CANDBG       3309
#define IDC_SETTINGS_STATIC_CANDTEXT     3310
#define IDC_SETTINGS_STATIC_CANDSELBG    3311
#define IDC_SETTINGS_STATIC_CANDSELTEXT  3312
#define IDC_SETTINGS_STATIC_TOOLBARCOLOR 3313
#define IDC_SETTINGS_STATIC_TOOLBARBG    3314
#define IDC_SETTINGS_STATIC_TOOLBARHOVER 3315

    struct SettingsDlgControls {
        HWND hTab;
        // 第一页控件
        HWND hCheckChinese;
        HWND hComboInputMode;
        HWND hEditShortcut;
        HWND hStaticInputMode;
        HWND hStaticShortcut;
        HWND hShowToolBar;
        HWND hHVToolBar;
        // 第二页控件
        HWND hComboArrange;
        HWND hEditHorizontal;
        HWND hEditVertical;
        HWND hCheckShowRemain;
        HWND hBtnCandBg;
        HWND hBtnCandText;
        HWND hBtnCandSelBg;
        HWND hBtnCandSelText;
        HWND hBtnToolbarBg;
        HWND hBtnToolbarHover;
        HWND hStaticArrange;
        HWND hStaticHorizontal;
        HWND hStaticVertical;
        HWND hStaticHorizNote;
        HWND hStaticVertNote;
        HWND hStaticCandColor;
        HWND hStaticCandBg;
        HWND hStaticCandText;
        HWND hStaticCandSelBg;
        HWND hStaticCandSelText;
        HWND hStaticToolbarColor;
        HWND hStaticToolbarBg;
        HWND hStaticToolbarHover;
        // 颜色值
        COLORREF candBg;
        COLORREF candText;
        COLORREF candSelBg;
        COLORREF candSelText;
        COLORREF toolbarBg;
        COLORREF toolbarHover;
        HFONT hFont;
        HFONT hBFont;
    };

    static LRESULT CALLBACK SettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        SettingsDlgControls* pCtrl = (SettingsDlgControls*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

        switch (uMsg)
        {
        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hDlg, &rc);
            // 使用您想要的颜色，例如 RGB(240, 240, 245)
            HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            return TRUE;   // 告诉系统我们已经擦除了背景
        }
        case WM_INITDIALOG:
        {
            pCtrl = new SettingsDlgControls();
            ZeroMemory(pCtrl, sizeof(SettingsDlgControls));
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pCtrl);

            pCtrl->hBFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
            pCtrl->hFont = CreateFont(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);

            // Tab控件
            pCtrl->hTab = CreateWindow(WC_TABCONTROL, NULL,
                WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
                10, 10, 420, 28,
                hDlg, NULL, hInst, NULL);
            SendMessage(pCtrl->hTab, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            SendMessage(pCtrl->hTab, TCM_SETBKCOLOR, 0, (LPARAM)RGB(240, 240, 240));

            TCITEM tie = { 0 };
            tie.mask = TCIF_TEXT;
            WCHAR szTab1[] = L"模式设置";
            WCHAR szTab2[] = L"窗口设置";
            tie.pszText = szTab1;
            TabCtrl_InsertItem(pCtrl->hTab, 0, &tie);
            tie.pszText = szTab2;
            TabCtrl_InsertItem(pCtrl->hTab, 1, &tie);

            // ---- 页签1 ----
            pCtrl->hCheckChinese = CreateWindow(L"BUTTON", L"初始默认中文",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                30, 50, 180, 24,
                hDlg, (HMENU)IDC_SETTINGS_CHECK_CHINESE, hInst, NULL);
            SendMessage(pCtrl->hCheckChinese, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticInputMode = CreateWindow(L"STATIC", L"默认输入模式：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, 88, 120, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_INPUTMODE, hInst, NULL);
            SendMessage(pCtrl->hStaticInputMode, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hComboInputMode = CreateWindow(L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                160, 85, 140, 100,
                hDlg, (HMENU)IDC_SETTINGS_COMBO_INPUTMODE, hInst, NULL);
            SendMessage(pCtrl->hComboInputMode, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            SendMessage(pCtrl->hComboInputMode, CB_ADDSTRING, 0, (LPARAM)L"五笔");
            SendMessage(pCtrl->hComboInputMode, CB_ADDSTRING, 0, (LPARAM)L"拼音");
            SendMessage(pCtrl->hComboInputMode, CB_ADDSTRING, 0, (LPARAM)L"拼音+五笔混合");

            pCtrl->hStaticShortcut = CreateWindow(L"STATIC", L"手工造词快捷键：Ctrl+",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                33, 128, 200, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_SHORTCUT, hInst, NULL);
            SendMessage(pCtrl->hStaticShortcut, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hEditShortcut = CreateWindow(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE | ES_AUTOHSCROLL,
                185, 125, 30, 24,
                hDlg, (HMENU)IDC_SETTINGS_EDIT_SHORTCUT, hInst, NULL);
            SendMessage(pCtrl->hEditShortcut, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            SendMessage(pCtrl->hEditShortcut, EM_LIMITTEXT, 1, 0);

            pCtrl->hShowToolBar = CreateWindow(L"BUTTON", L"初始显示工具栏",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                30, 170, 180, 24,
                hDlg, (HMENU)IDC_SETTINGS_CHECK_CHINESE, hInst, NULL);
            SendMessage(pCtrl->hShowToolBar, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hHVToolBar = CreateWindow(L"BUTTON", L"水平(不勾则垂直)工具栏",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                30, 210, 180, 24,
                hDlg, (HMENU)IDC_SETTINGS_CHECK_CHINESE, hInst, NULL);
            SendMessage(pCtrl->hHVToolBar, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            // ---- 页签2 ----
            pCtrl->hStaticArrange = CreateWindow(L"STATIC", L"候选窗口排列方式：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, 53, 130, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_ARRANGE, hInst, NULL);
            SendMessage(pCtrl->hStaticArrange, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hComboArrange = CreateWindow(L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                170, 50, 100, 100,
                hDlg, (HMENU)IDC_SETTINGS_COMBO_ARRANGE, hInst, NULL);
            SendMessage(pCtrl->hComboArrange, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            SendMessage(pCtrl->hComboArrange, CB_ADDSTRING, 0, (LPARAM)L"横向");
            SendMessage(pCtrl->hComboArrange, CB_ADDSTRING, 0, (LPARAM)L"纵向");

            pCtrl->hStaticHorizontal = CreateWindow(L"STATIC", L"横向最大词条数：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, 88, 130, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_HORIZONTAL, hInst, NULL);
            SendMessage(pCtrl->hStaticHorizontal, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hEditHorizontal = CreateWindow(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                170, 85, 50, 24,
                hDlg, (HMENU)IDC_SETTINGS_EDIT_HORIZONTAL, hInst, NULL);
            SendMessage(pCtrl->hEditHorizontal, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticHorizNote = CreateWindow(L"STATIC", L"(3-10)",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                230, 85, 50, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_HORIZ_NOTE, hInst, NULL);
            SendMessage(pCtrl->hStaticHorizNote, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticVertical = CreateWindow(L"STATIC", L"纵向最大词条数：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, 123, 130, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_VERTICAL, hInst, NULL);
            SendMessage(pCtrl->hStaticVertical, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hEditVertical = CreateWindow(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                170, 120, 50, 24,
                hDlg, (HMENU)IDC_SETTINGS_EDIT_VERTICAL, hInst, NULL);
            SendMessage(pCtrl->hEditVertical, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticVertNote = CreateWindow(L"STATIC", L"(3-10)",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                230, 120, 50, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_VERT_NOTE, hInst, NULL);
            SendMessage(pCtrl->hStaticVertNote, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hCheckShowRemain = CreateWindow(L"BUTTON", L"显示剩余编码",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                30, 155, 150, 24,
                hDlg, (HMENU)IDC_SETTINGS_CHECK_SHOWREMAIN, hInst, NULL);
            SendMessage(pCtrl->hCheckShowRemain, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            // ---- 颜色配置（两列三行，自绘颜色块） ----
            int yColor = 200;
            pCtrl->hStaticCandColor = CreateWindow(L"STATIC", L"候选窗口：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, yColor, 120, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_CANDCOLOR, hInst, NULL);
            SendMessage(pCtrl->hStaticCandColor, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            yColor += 30;
            // 第一行：背景（左）和候选词（右）
            pCtrl->hStaticCandBg = CreateWindow(L"STATIC", L"背景颜色",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, yColor+3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_CANDBG, hInst, NULL);
            SendMessage(pCtrl->hStaticCandBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnCandBg = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                120, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_CAND_BG, hInst, NULL);
            SendMessage(pCtrl->hBtnCandBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticCandText = CreateWindow(L"STATIC", L"候选词",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                220, yColor + 3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_CANDTEXT, hInst, NULL);
            SendMessage(pCtrl->hStaticCandText, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnCandText = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                300, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_CAND_TEXT, hInst, NULL);
            SendMessage(pCtrl->hBtnCandText, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            yColor += 30;
            // 第二行：选中项背景（左）和选中项文本（右）
            pCtrl->hStaticCandSelBg = CreateWindow(L"STATIC", L"选中项背景",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, yColor + 3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_CANDSELBG, hInst, NULL);
            SendMessage(pCtrl->hStaticCandSelBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnCandSelBg = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                120, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_CAND_SELBG, hInst, NULL);
            SendMessage(pCtrl->hBtnCandSelBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticCandSelText = CreateWindow(L"STATIC", L"选中项文本",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                220, yColor + 3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_CANDSELTEXT, hInst, NULL);
            SendMessage(pCtrl->hStaticCandSelText, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnCandSelText = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                300, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_CAND_SELTEXT, hInst, NULL);
            SendMessage(pCtrl->hBtnCandSelText, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            yColor += 35;
            pCtrl->hStaticToolbarColor = CreateWindow(L"STATIC", L"工具栏窗口：",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, yColor, 120, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_TOOLBARCOLOR, hInst, NULL);
            SendMessage(pCtrl->hStaticToolbarColor, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            yColor += 30;
            // 第三行：背景（左）和悬停背景（右）
            pCtrl->hStaticToolbarBg = CreateWindow(L"STATIC", L"背景",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                30, yColor + 3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_TOOLBARBG, hInst, NULL);
            SendMessage(pCtrl->hStaticToolbarBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnToolbarBg = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                120, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_TOOLBAR_BG, hInst, NULL);
            SendMessage(pCtrl->hBtnToolbarBg, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            pCtrl->hStaticToolbarHover = CreateWindow(L"STATIC", L"悬停背景",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                220, yColor + 3, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_STATIC_TOOLBARHOVER, hInst, NULL);
            SendMessage(pCtrl->hStaticToolbarHover, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            pCtrl->hBtnToolbarHover = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                300, yColor, 80, 24,
                hDlg, (HMENU)IDC_SETTINGS_BTN_TOOLBAR_HOVER, hInst, NULL);
            SendMessage(pCtrl->hBtnToolbarHover, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);

            // 确定和取消按钮
            int btnY = yColor + 50;
            HWND hQueDing = CreateWindow(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                250, btnY, 70, 28,
                hDlg, (HMENU)IDOK, hInst, NULL);
            SendMessage(hQueDing, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            HWND hQuXiao = CreateWindow(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                330, btnY, 70, 28,
                hDlg, (HMENU)IDCANCEL, hInst, NULL);
            SendMessage(hQuXiao, WM_SETFONT, (WPARAM)pCtrl->hBFont, TRUE);
            LoadIniSettings();
            // ---- 加载当前设置到控件 ----
            // 颜色值直接使用全局变量
            pCtrl->candBg = g_candidateBgColor;
            pCtrl->candText = g_candidateTextColor;
            pCtrl->candSelBg = g_candidateSelectedBgColor;
            pCtrl->candSelText = g_candidateSelectedTextColor;
            pCtrl->toolbarBg = g_toolbarBgColor;
            pCtrl->toolbarHover = g_toolbarHoverColor;

            SendMessage(pCtrl->hCheckChinese, BM_SETCHECK, g_isChineseMode ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessage(pCtrl->hShowToolBar, BM_SETCHECK, g_isShowToolBar ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessage(pCtrl->hHVToolBar, BM_SETCHECK, g_isHToolbarWin ? BST_CHECKED : BST_UNCHECKED, 0);
            int sel = 0;
            if (g_isPinyinMode && !g_isPyAndWbMode) sel = 1;
            else if (g_isPinyinMode && g_isPyAndWbMode) sel = 2;
            SendMessage(pCtrl->hComboInputMode, CB_SETCURSEL, sel, 0);
            WCHAR szKey[2] = { g_shortcutKey, 0 };
            SetWindowText(pCtrl->hEditShortcut, szKey);

            SendMessage(pCtrl->hComboArrange, CB_SETCURSEL, g_isHorizontal ? 0 : 1, 0);
            WCHAR szVal[16];
            swprintf_s(szVal, L"%d", g_maxCandidatesHorizontal);
            SetWindowText(pCtrl->hEditHorizontal, szVal);
            swprintf_s(szVal, L"%d", g_maxCandidatesVertical);
            SetWindowText(pCtrl->hEditVertical, szVal);
            SendMessage(pCtrl->hCheckShowRemain, BM_SETCHECK, g_showRemainingCode ? BST_CHECKED : BST_UNCHECKED, 0);

            // 默认显示第一页
            TabCtrl_SetCurSel(pCtrl->hTab, 0);
            // 显示第一页，隐藏第二页
            ShowWindow(pCtrl->hCheckChinese, SW_SHOW);
            ShowWindow(pCtrl->hShowToolBar, SW_SHOW);
            ShowWindow(pCtrl->hHVToolBar, SW_SHOW);
            ShowWindow(pCtrl->hComboInputMode, SW_SHOW);
            ShowWindow(pCtrl->hEditShortcut, SW_SHOW);
            ShowWindow(pCtrl->hStaticInputMode, SW_SHOW);
            ShowWindow(pCtrl->hStaticShortcut, SW_SHOW);
            ShowWindow(pCtrl->hComboArrange, SW_HIDE);
            ShowWindow(pCtrl->hEditHorizontal, SW_HIDE);
            ShowWindow(pCtrl->hEditVertical, SW_HIDE);
            ShowWindow(pCtrl->hCheckShowRemain, SW_HIDE);
            ShowWindow(pCtrl->hBtnCandBg, SW_HIDE);
            ShowWindow(pCtrl->hBtnCandText, SW_HIDE);
            ShowWindow(pCtrl->hBtnCandSelBg, SW_HIDE);
            ShowWindow(pCtrl->hBtnCandSelText, SW_HIDE);
            ShowWindow(pCtrl->hBtnToolbarBg, SW_HIDE);
            ShowWindow(pCtrl->hBtnToolbarHover, SW_HIDE);
            ShowWindow(pCtrl->hStaticArrange, SW_HIDE);
            ShowWindow(pCtrl->hStaticHorizontal, SW_HIDE);
            ShowWindow(pCtrl->hStaticVertical, SW_HIDE);
            ShowWindow(pCtrl->hStaticHorizNote, SW_HIDE);
            ShowWindow(pCtrl->hStaticVertNote, SW_HIDE);
            ShowWindow(pCtrl->hStaticCandColor, SW_HIDE);
            ShowWindow(pCtrl->hStaticCandBg, SW_HIDE);
            ShowWindow(pCtrl->hStaticCandText, SW_HIDE);
            ShowWindow(pCtrl->hStaticCandSelBg, SW_HIDE);
            ShowWindow(pCtrl->hStaticCandSelText, SW_HIDE);
            ShowWindow(pCtrl->hStaticToolbarColor, SW_HIDE);
            ShowWindow(pCtrl->hStaticToolbarBg, SW_HIDE);
            ShowWindow(pCtrl->hStaticToolbarHover, SW_HIDE);

            return TRUE;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_DRAWITEM:
        {
            // 不依赖 pCtrl，直接使用全局颜色变量，最安全
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlType != ODT_BUTTON)
                return FALSE;

            COLORREF clr = 0;
            switch (lpDIS->CtlID)
            {
            case IDC_SETTINGS_BTN_CAND_BG: clr = g_candidateBgColor; break;
            case IDC_SETTINGS_BTN_CAND_TEXT: clr = g_candidateTextColor; break;
            case IDC_SETTINGS_BTN_CAND_SELBG: clr = g_candidateSelectedBgColor; break;
            case IDC_SETTINGS_BTN_CAND_SELTEXT: clr = g_candidateSelectedTextColor; break;
            case IDC_SETTINGS_BTN_TOOLBAR_BG: clr = g_toolbarBgColor; break;
            case IDC_SETTINGS_BTN_TOOLBAR_HOVER: clr = g_toolbarHoverColor; break;
            default:
                return FALSE;
            }

            HDC hdc = lpDIS->hDC;
            RECT rc = lpDIS->rcItem;
            HBRUSH hBrush = CreateSolidBrush(clr);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return TRUE;
        }

        case WM_NOTIFY:
        {
            if (!pCtrl) break;
            if (((NMHDR*)lParam)->idFrom == 0 && ((NMHDR*)lParam)->code == TCN_SELCHANGE)
            {
                int sel = TabCtrl_GetCurSel(pCtrl->hTab);
                BOOL showPage1 = (sel == 0);
                ShowWindow(pCtrl->hCheckChinese, showPage1 ? SW_SHOW : SW_HIDE);
				ShowWindow(pCtrl->hShowToolBar, showPage1 ? SW_SHOW : SW_HIDE);
				ShowWindow(pCtrl->hHVToolBar, showPage1 ? SW_SHOW : SW_HIDE);
                ShowWindow(pCtrl->hComboInputMode, showPage1 ? SW_SHOW : SW_HIDE);
                ShowWindow(pCtrl->hEditShortcut, showPage1 ? SW_SHOW : SW_HIDE);
                ShowWindow(pCtrl->hStaticInputMode, showPage1 ? SW_SHOW : SW_HIDE);
                ShowWindow(pCtrl->hStaticShortcut, showPage1 ? SW_SHOW : SW_HIDE);
                ShowWindow(pCtrl->hComboArrange, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hEditHorizontal, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hEditVertical, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hCheckShowRemain, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnCandBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnCandText, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnCandSelBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnCandSelText, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnToolbarBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hBtnToolbarHover, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticArrange, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticHorizontal, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticVertical, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticHorizNote, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticVertNote, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticCandColor, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticCandBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticCandText, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticCandSelBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticCandSelText, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticToolbarColor, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticToolbarBg, showPage1 ? SW_HIDE : SW_SHOW);
                ShowWindow(pCtrl->hStaticToolbarHover, showPage1 ? SW_HIDE : SW_SHOW);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
        {
            if (!pCtrl) break;
            WORD notifyCode = HIWORD(wParam);
            WORD ctrlId = LOWORD(wParam);

            // 处理颜色按钮点击
            if (notifyCode == BN_CLICKED)
            {
                COLORREF* pClr = nullptr;
                switch (ctrlId)
                {
                case IDC_SETTINGS_BTN_CAND_BG:     pClr = &g_candidateBgColor; break;
                case IDC_SETTINGS_BTN_CAND_TEXT:   pClr = &g_candidateTextColor; break;
                case IDC_SETTINGS_BTN_CAND_SELBG:  pClr = &g_candidateSelectedBgColor; break;
                case IDC_SETTINGS_BTN_CAND_SELTEXT:pClr = &g_candidateSelectedTextColor; break;
                case IDC_SETTINGS_BTN_TOOLBAR_BG:  pClr = &g_toolbarBgColor; break;
                case IDC_SETTINGS_BTN_TOOLBAR_HOVER:pClr = &g_toolbarHoverColor; break;
                default:
                    pClr = nullptr; // 不是颜色按钮，不处理
                    break;
                }

                if (pClr)
                {
                    // 通用颜色选择函数
                    auto ChooseColorDialog = [](HWND hParent, COLORREF& clrOut) -> BOOL {
                        CHOOSECOLOR cc = { 0 };
                        static COLORREF customColors[16]; // 自定义颜色缓存
                        cc.lStructSize = sizeof(CHOOSECOLOR);
                        cc.hwndOwner = hParent;
                        cc.rgbResult = clrOut;
                        cc.lpCustColors = customColors;
                        cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                        if (ChooseColor(&cc))
                        {
                            clrOut = cc.rgbResult;
                            return TRUE;
                        }
                        return FALSE;
                        };
                    if (ChooseColorDialog(hDlg, *pClr))
                    {
                        // 同步到 pCtrl 以便保存
                        pCtrl->candBg = g_candidateBgColor;
                        pCtrl->candText = g_candidateTextColor;
                        pCtrl->candSelBg = g_candidateSelectedBgColor;
                        pCtrl->candSelText = g_candidateSelectedTextColor;
                        pCtrl->toolbarBg = g_toolbarBgColor;
                        pCtrl->toolbarHover = g_toolbarHoverColor;

                        // 刷新按钮
                        HWND hBtn = GetDlgItem(hDlg, ctrlId);
                        if (hBtn)
                            InvalidateRect(hBtn, NULL, TRUE);
                    }
                return TRUE;
                }
            }
            if (ctrlId == IDOK)
            {
                // 读取控件值
                      /*      BOOL g_isChineseMode = TRUE;
            BOOL g_isPinyinMode = FALSE;
            BOOL g_isPyAndWbMode = FALSE;
            BOOL g_isHorizontal = TRUE;*/
                g_isShowToolBar = (SendMessage(pCtrl->hShowToolBar, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_isHToolbarWin = (SendMessage(pCtrl->hHVToolBar, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_isChineseMode = (SendMessage(pCtrl->hCheckChinese, BM_GETCHECK, 0, 0) == BST_CHECKED);
                int sel = (int)SendMessage(pCtrl->hComboInputMode, CB_GETCURSEL, 0, 0);
                if (sel == 0) {
                    g_isPinyinMode = FALSE;
                    g_isPyAndWbMode = FALSE;
                }
                else if (sel == 1) {
                    g_isPinyinMode = TRUE;
                    g_isPyAndWbMode = FALSE;
                }
                else {
                    g_isPinyinMode = TRUE;
                    g_isPyAndWbMode = TRUE;
                }

                WCHAR ch[2] = { 0 };
                GetWindowText(pCtrl->hEditShortcut, ch, 2);
                if (ch[0] >= L'A' && ch[0] <= L'Z') g_shortcutKey = ch[0];
                else g_shortcutKey = L'W';

                sel = (int)SendMessage(pCtrl->hComboArrange, CB_GETCURSEL, 0, 0);
                g_isHorizontal = (sel == 0);

                WCHAR szTmp[16];
                GetWindowText(pCtrl->hEditHorizontal, szTmp, 16);
                int val = _wtoi(szTmp);
                if (val >= 3 && val <= 10) g_maxCandidatesHorizontal = val;
                GetWindowText(pCtrl->hEditVertical, szTmp, 16);
                val = _wtoi(szTmp);
                if (val >= 3 && val <= 10) g_maxCandidatesVertical = val;

                g_showRemainingCode = (SendMessage(pCtrl->hCheckShowRemain, BM_GETCHECK, 0, 0) == BST_CHECKED);

                // 同步颜色全局变量（已在点击时更新，但为了完整，再从pCtrl读一次）
                g_candidateBgColor = pCtrl->candBg;
                g_candidateTextColor = pCtrl->candText;
                g_candidateSelectedBgColor = pCtrl->candSelBg;
                g_candidateSelectedTextColor = pCtrl->candSelText;
                g_toolbarBgColor = pCtrl->toolbarBg;
                g_toolbarHoverColor = pCtrl->toolbarHover;

                SaveIniSettings();

                HWND hToolbar = Global::hToolBarWnd;
                if (hToolbar && IsWindow(hToolbar))
                {
                    InvalidateRect(hToolbar, NULL, TRUE);
                    CToolbarWindow* pToolbar = (CToolbarWindow*)GetWindowLongPtr(hToolbar, GWLP_USERDATA);
                    if (pToolbar) pToolbar->UpdateAllButtons();
                }

                DestroyWindow(hDlg);
                return TRUE;
            }
            else if (ctrlId == IDCANCEL)
            {
                DestroyWindow(hDlg);
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
        {
            if (pCtrl)
            {
                if (pCtrl->hFont) DeleteObject(pCtrl->hFont);
                if (pCtrl->hBFont) DeleteObject(pCtrl->hBFont);
                delete pCtrl;
                SetWindowLongPtr(hDlg, GWLP_USERDATA, 0);
            }
            break;
        }
        // 新增WM_CLOSE兜底，点右上角X也能正常关闭
        case WM_CLOSE:
        {
            DestroyWindow(hDlg);
            return 0;
        }
        }
        return DefWindowProc(hDlg, uMsg, wParam, lParam);
    }

    void ShowSettingsDialog()
    {
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES };
        InitCommonControlsEx(&icex);

        HWND hParent = GetDesktopWindow();
        HINSTANCE hInst = Global::dllInstanceHandle;

        const wchar_t szDlgClass[] = L"XinYuSettingsDialogClass";
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = SettingsDlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = szDlgClass;
        RegisterClassEx(&wc);

        int dlgWidth = 450, dlgHeight = 470;
        RECT rcParent = { 0 };
        if (hParent && IsWindow(hParent))
            GetWindowRect(hParent, &rcParent);
        else
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcParent, 0);
        int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;

        HWND hDlg = CreateWindowEx(
            WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
            szDlgClass,
            L"设置",
            WS_CAPTION | WS_SYSMENU | DS_MODALFRAME ,
            x, y, dlgWidth, dlgHeight,
            hParent, NULL, hInst, NULL);
        if (!hDlg) return;

        SendMessage(hDlg, WM_INITDIALOG, 0, 0);
        EnableWindow(hParent, FALSE);
        ShowWindow(hDlg, SW_SHOW);

        MSG msg;
        BOOL bLoop = TRUE;
        while (bLoop && GetMessage(&msg, NULL, 0, 0))
        {
            if (!IsDialogMessage(hDlg, &msg))
            {
                TranslateMessage(&msg);
                LRESULT lRet = DispatchMessage(&msg);
                // 窗口过程返回0代表需要关闭弹窗
                if (lRet == 0 && !IsWindow(hDlg))
                {
                    bLoop = FALSE;
                }
            }
        }
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
        UnregisterClass(szDlgClass, hInst);
    }
    // 控件 ID 定义（也可放在函数内）
#define IDC_WORD_EDIT   2001
#define IDC_CODE_EDIT   2002
#define IDC_ADD_PINYIN  2003

#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR (WM_USER + 67)  // 标准 Edit 控件的正确数值
#endif

    struct RecentWordData {
        WCHAR fullText[16];     // 最多15个汉字+结束符
        int startIndex;        // 从 fullText 的哪个索引开始取（0-based）
      //  BOOL hasRecent;           // 是否真正有最近汉字（是否启用左右键）
        WNDPROC oldEditProc;   // 原编辑框窗口过程
    };
    // 编辑框子类化窗口过程
    static LRESULT CALLBACK WordEditSubclassProc(HWND hEdit, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        RecentWordData* pData = (RecentWordData*)GetWindowLongPtr(hEdit, GWLP_USERDATA);
        if (!pData) return DefWindowProc(hEdit, uMsg, wParam, lParam);

        switch (uMsg) {
        case WM_SETFOCUS:
        {
            HWND hDlg = GetParent(hEdit);
            CSampleIME* pIME = (CSampleIME*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            if (pIME && !Global::isChineseMode) {
                CCompartment Comp(pIME->_GetThreadMgr(), pIME->_GetClientId(), GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
                Comp._SetCompartmentBOOL(TRUE);
            }
            // 全选编辑框中的文本
            SendMessage(hEdit, EM_SETSEL, 0, -1);
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_LEFT) {
                if (pData->startIndex > 0) {
                    pData->startIndex--;
                    // 更新编辑框文本
                    SetWindowText(hEdit, pData->fullText + pData->startIndex);
                    return 0;  // 吃掉消息
                }
                break;
            }
            break;
        case WM_DESTROY:
            // 恢复原窗口过程
            SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)pData->oldEditProc);
            break;
        }
        return CallWindowProc(pData->oldEditProc, hEdit, uMsg, wParam, lParam);
    }
    static LRESULT CALLBACK CodeEditSubclassProc(HWND hEdit, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_SETFOCUS:
        {
            HWND hDlg = GetParent(hEdit);
            CSampleIME* pIME = (CSampleIME*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            if (pIME && Global::isChineseMode) {
                // 切换到英文模式
                CCompartment Comp(pIME->_GetThreadMgr(), pIME->_GetClientId(), GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
                Comp._SetCompartmentBOOL(FALSE);
            }
            // 全选编辑框中的文本
            SendMessage(hEdit, EM_SETSEL, 0, -1);
            break;
        }
        }
        return CallWindowProc((WNDPROC)GetWindowLongPtr(hEdit, GWLP_USERDATA), hEdit, uMsg, wParam, lParam);
    }
    static LRESULT CALLBACK UserWordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CSampleIME* pIME = (CSampleIME*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

        switch (uMsg)
        {
            // 统一对话框背景（使用 WM_ERASEBKGND，放弃无效的 WM_CTLCOLORDLG）
        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hDlg, &rc);
            static HBRUSH hBgBrush = []() { return CreateSolidBrush(RGB(235, 245, 245)); }();
            FillRect(hdc, &rc, hBgBrush);
            return TRUE;
        }
        case WM_CTLCOLORBTN:
        {
            HDC hdcBtn = (HDC)wParam;
            SetBkMode(hdcBtn, TRANSPARENT);
            SetTextColor(hdcBtn, GetSysColor(COLOR_WINDOWTEXT));
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);  // 或 NULL_BRUSH
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            // 可选：设置文字颜色与系统一致
            SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
            // 返回 NULL_BRUSH 使背景透明
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_INITDIALOG:
        {
            HWND hCheckBox = GetDlgItem(hDlg, IDC_ADD_PINYIN);
            SetWindowTheme(hCheckBox, L"", L"");   // 禁用视觉主题

            pIME = (CSampleIME*)lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pIME);
            SetWindowText(hDlg, L"手工造词");

            HWND hCodeEdit = GetDlgItem(hDlg, IDC_CODE_EDIT);
            SetWindowLongPtr(hCodeEdit, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(hCodeEdit, GWLP_WNDPROC));
            SetWindowLongPtr(hCodeEdit, GWLP_WNDPROC, (LONG_PTR)CodeEditSubclassProc);
            // 原先的 else 分支：从剪贴板获取或使用默认文本
             // ========== 新增：优先从剪贴板获取前5个汉字，并更新 Global::m_recentHanzi ==========
            WCHAR clipboardHanzi[16] = { 0 };   // 最多15个汉字+结束符
            if (OpenClipboard(hDlg))
            {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData)
                {
                    LPCWSTR pClip = (LPCWSTR)GlobalLock(hData);
                    if (pClip)
                    {
                        int idx = 0;
                        for (int i = 0; pClip[i] && idx < 15; i++)
                        {
                            if (pClip[i] >= 0x4E00 && pClip[i] <= 0x9FFF)
                            {
                                clipboardHanzi[idx++] = pClip[i];
                            }
                            //else break;
                        }
                        if (idx > 0)
                        {
                            clipboardHanzi[idx] = L'\0';
                            // 更新全局最近汉字缓冲区
                            wcsncpy_s(Global::m_recentHanzi, _countof(Global::m_recentHanzi), clipboardHanzi, _TRUNCATE);
                        }
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            const WCHAR* recent = Global::m_recentHanzi;
            WCHAR displayText[256] = { 0 };          // 用于存储最终显示的词语
            BOOL useRecentSubclass = FALSE;          // 是否启用最近汉字的左右键功能

            RecentWordData* pData = (RecentWordData*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RecentWordData));
            HWND hWordEdit = GetDlgItem(hDlg, IDC_WORD_EDIT);
            pData->oldEditProc = (WNDPROC)SetWindowLongPtr(hWordEdit, GWLP_WNDPROC, (LONG_PTR)WordEditSubclassProc);
            SetWindowLongPtr(hWordEdit, GWLP_USERDATA, (LONG_PTR)pData);
            SetWindowLongPtr(hWordEdit, GWL_STYLE, GetWindowLongPtr(hWordEdit, GWL_STYLE) | WS_TABSTOP);
            SetWindowLongPtr(hCodeEdit, GWL_STYLE, GetWindowLongPtr(hCodeEdit, GWL_STYLE) | WS_TABSTOP);
            if (recent && *recent) {
                if (pData) {
                // 原有正常逻辑：使用最近输入的汉字
                size_t fullLen = wcslen(recent);
                    wcsncpy_s(pData->fullText, _countof(pData->fullText), recent, _TRUNCATE);
                    pData->startIndex = (fullLen > 2) ? (int)(fullLen - 2) : 0;
                    
                    // 复制要显示的文本到 displayText
                    wcsncpy_s(displayText, _countof(displayText), pData->fullText + pData->startIndex, _TRUNCATE);
                    useRecentSubclass = TRUE;                
                }
            }
            else {
                if (wcslen(displayText) == 0) {
                    wcscpy_s(displayText, L"请输入词语");
                }
            }
            if (!pData) {
                    // 降级：直接显示全部最近汉字
                    wcsncpy_s(displayText, _countof(displayText), recent, _TRUNCATE);            
            }

            // 统一设置词语编辑框
            SetDlgItemText(hDlg, IDC_WORD_EDIT, displayText);
            CheckDlgButton(hDlg, IDC_ADD_PINYIN, BST_UNCHECKED);
            SetFocus(GetDlgItem(hDlg, IDC_WORD_EDIT));
            SendMessage(hDlg, DM_SETDEFID, IDOK, 0);

            // 获取编码并设置到编码编辑框（统一处理）
            CCompositionProcessorEngine* pEngine = pIME->GetCompositionProcessorEngine();
            if (pEngine && displayText[0]) {
                std::wstring code = pEngine->GetCodeForWord(displayText, wcslen(displayText));
                if (!code.empty()) {
                    SetDlgItemText(hDlg, IDC_CODE_EDIT, code.c_str());
                }
            }
            return FALSE;
        }

        case WM_COMMAND:
        {
            WORD wID = LOWORD(wParam);
            WORD wNotify = HIWORD(wParam);
            if (wID == IDC_WORD_EDIT && wNotify == EN_UPDATE) {
                WCHAR word[256];
                GetDlgItemText(hDlg, IDC_WORD_EDIT, word, 256);
                CCompositionProcessorEngine* pEngine = pIME->GetCompositionProcessorEngine();
                if (pEngine && word[0]) {
                    std::wstring code = pEngine->GetCodeForWord(word, wcslen(word));
                    if (!code.empty()) {
                        SetDlgItemText(hDlg, IDC_CODE_EDIT, code.c_str());
                    }
                    else {
                        SetDlgItemText(hDlg, IDC_CODE_EDIT, L"");
                    }
                }
                return TRUE;
            }
            switch (LOWORD(wParam))
            {
            case IDOK:
            {
                WCHAR word[256] = { 0 };
                WCHAR code[256] = { 0 };
                GetDlgItemText(hDlg, IDC_WORD_EDIT, word, 256);
                GetDlgItemText(hDlg, IDC_CODE_EDIT, code, 256);
                if (wcslen(word) == 0 || wcslen(code) == 0)
                {
                    MessageBox(hDlg, L"词语和编码都不能为空！", L"提示", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                BOOL bAddPinyin = (IsDlgButtonChecked(hDlg, IDC_ADD_PINYIN) == BST_CHECKED);
                CCompositionProcessorEngine* pEngine = pIME->GetCompositionProcessorEngine();
                if (pEngine) {
                    int ret = pEngine->AddUserWordWithOption(code, word, bAddPinyin);
                    if (ret == 0) { 
                        MessageBox(hDlg, L"添加成功，将重载词库！", L"手工造词", MB_OK | MB_ICONINFORMATION); 
						pEngine->ReloadDictionaries();
                    }
                    else {
                        WCHAR szMsg[256];
                        LPCWSTR pszErrMsg = NULL;
                        switch (ret) {
                        case 1: pszErrMsg = L"词语已存在于用户词库，无需重复添加。"; break;
                        case 2: pszErrMsg = L"词语已存在于系统词库，无需重复添加。"; break;
                        case 3: pszErrMsg = L"无法打开用户词库文件进行写入，请检查文件权限。"; break;
                        case 11: pszErrMsg = L"词语已存在于拼音用户词库，无需重复添加。"; break;
                        case 12: pszErrMsg = L"词语已存在于拼音系统词库，无需重复添加。"; break;
                        case 13: pszErrMsg = L"无法打开拼音用户词库文件进行写入，请检查文件权限。"; break;
                        default: pszErrMsg = L"添加失败，未知错误。"; break;
                        }
                        StringCchPrintf(szMsg, _countof(szMsg), L"%s (错误码：%d)", pszErrMsg, ret);
                        MessageBox(hDlg, szMsg, L"错误", MB_OK | MB_ICONWARNING);
                    }
                }
                DestroyWindow(hDlg);  // 关闭窗口
                return TRUE;
            }
            case IDCANCEL:
                DestroyWindow(hDlg);
                return TRUE;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hDlg);
            return TRUE;
        }
        return DefWindowProc(hDlg, uMsg, wParam, lParam);
    }
	// 在匿名命名空间内，UserWordDlgProc 下方添加以下内容----------------------------用户词组输入对话框相关函数

// 辅助：读取文件内容（支持 UTF-8 带/不带 BOM 以及 UTF-16 LE/BE）
    static std::wstring LoadFileContent(const WCHAR* filePath)
    {
        std::wstring content;
        HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return content;
        }

        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == 0 || fileSize > 1024 * 1024 * 2) // 限制最大 2MB
        {
            CloseHandle(hFile);
            return content;
        }

        std::vector<BYTE> buffer(fileSize + 4);
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL))
        {
            CloseHandle(hFile);
            return content;
        }
        CloseHandle(hFile);

        BYTE* p = buffer.data();
        size_t len = bytesRead;

        // 检测 BOM 并处理
        if (len >= 2 && p[0] == 0xFF && p[1] == 0xFE) {
            // UTF-16 LE with BOM
            p += 2;
            len -= 2;
            if (len % 2 != 0) {
                return content;
            }
            int wideLen = (int)(len / sizeof(wchar_t));
            if (wideLen > 0) {
                content.assign((wchar_t*)p, wideLen);
                OutputDebugString(L"[LoadFile] 直接转换成功，字符数: ");
            }

            return content;
        }
        else if (len >= 2 && p[0] == 0xFE && p[1] == 0xFF) {
            // UTF-16 BE (暂不支持，可尝试 MultiByteToWideChar 但 Windows 下少见)
            return content;
        }
        else if (len >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
            // UTF-8 with BOM
            p += 3;
            len -= 3;
            OutputDebugString(L"[LoadFile] 检测到 UTF-8 BOM\n");
        }


        // 处理 UTF-8（有或没有 BOM）
        int codepage = CP_UTF8;
        int wideLen = MultiByteToWideChar(codepage, 0, (LPCCH)p, (int)len, NULL, 0);
        if (wideLen > 0) {
            content.resize(wideLen);
            MultiByteToWideChar(codepage, 0, (LPCCH)p, (int)len, &content[0], wideLen);
            OutputDebugString(L"[LoadFile] UTF-8 转换成功，字符数: ");
            WCHAR buf[16];
            wsprintf(buf, L"%d\n", wideLen);
            OutputDebugString(buf);
        }

        return content;
    }

    // 获取 DLL 所在目录，并拼接文件名
    static BOOL GetFilePath(const wchar_t* fileName, WCHAR* outPath, DWORD cchOut)
    {
        if (!outPath || cchOut < MAX_PATH) return FALSE;
        if (FAILED(StringCchPrintf(outPath, cchOut, L"%s%s", g_szDllPath, fileName)))
            return FALSE;

        return TRUE;
    }

    // 表情符号对话框的控件 ID
#define IDC_EMOJI_TAB      2101
#define IDC_EMOJI_EDIT     2102
#define IDC_EMOJI_COPY     2103
#define IDC_EMOJI_CLOSE    2104

// 表情符号对话框过程
// 用于存储子控件句柄的结构体
    struct EmojiDlgControls {
        HWND hTab;
        HWND hEdit;
        HWND hCopyBtn;
        HWND hCloseBtn;
        HWND hTipStatic;
        int  currentTab;   // 当前选中的页签: 0=常用符号, 1=颜色表情
        HFONT hFont;          // 新增：自定义字体句柄
        HFONT hBFont;   // 其他控件普通字体（非粗体）
    };

    static LRESULT CALLBACK EmojiDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        EmojiDlgControls* pControls = (EmojiDlgControls*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

        switch (uMsg)
        {
            // 统一对话框背景（使用 WM_ERASEBKGND，放弃无效的 WM_CTLCOLORDLG）
        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hDlg, &rc);
            // 使用静态画刷避免重复创建/销毁（可选优化）
            static HBRUSH hBgBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            FillRect(hdc, &rc, hBgBrush);
            return TRUE;
        }

        // 按钮背景
        case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            static HBRUSH hBtnBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            return (LRESULT)hBtnBrush;
        }

        // 静态文本背景：提示文本保持透明（显示父窗口背景），其他静态控件（若有）也使用统一画刷
        case WM_CTLCOLORSTATIC:
        {
            HWND hStatic = (HWND)lParam;
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            if (pControls)
            {
                if (hStatic == pControls->hTipStatic) {
                    HDC hdcStatic = (HDC)wParam;
                    SetBkMode(hdcStatic, OPAQUE);                     // 关键：不透明
                    SetBkColor(hdcStatic, RGB(240, 240, 240));       // 背景色匹配对话框
                    SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
                    static HBRUSH hStaticBg = CreateSolidBrush(RGB(240, 240, 240));
                    return (LRESULT)hStaticBg;
                }
                else if (hStatic == pControls->hEdit) {
                    HDC hdcStatic = (HDC)wParam; /*wParam 是控件的设备上下文句柄*/
                    SetBkColor(hdcStatic, RGB(255, 255, 255));
                    SetTextColor(hdcStatic, RGB(0, 0, 0));
                    static HBRUSH hStaticBrush = []() { return CreateSolidBrush(RGB(255, 255, 255)); }();
                    // 提示文本使用 NULL_BRUSH，背景透明，显示已擦除的对话框背景
                    return (LRESULT)hStaticBrush;
                }
            }
            // 其他静态控件（本对话框没有，但为完整性）返回统一背景画刷
            static HBRUSH hStaticBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            return (LRESULT)hStaticBrush;
        }

        case WM_INITDIALOG:
        {
            // 分配并存储控件结构体
            pControls = new EmojiDlgControls();
            pControls->currentTab = 0;   // 初始选中常用符号
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pControls);
            HINSTANCE hInstSys = GetModuleHandle(NULL);
            pControls->hFont = CreateFont(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
            // 如果系统没有微软雅黑，可回退到 "Segoe UI" 或 NULL（使用默认字体）
            // 创建普通字体（按钮、静态文本）-12 约 9pt，非粗体
            pControls->hBFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
            // 创建 Tab 控件
            pControls->hTab = CreateWindow(WC_TABCONTROL, NULL,
                WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
                0, 0, 0, 0,  // 位置稍后在 WM_SIZE 中设置
                hDlg, (HMENU)IDC_EMOJI_TAB, hInstSys, NULL);

            SendMessage(pControls->hTab, WM_SETFONT, (WPARAM)pControls->hBFont, TRUE);

            // 设置 Tab 控件背景色（兼容低版本 SDK，使用 SendMessage 直接发送消息）

            TCITEM tie = { 0 };
            tie.mask = TCIF_TEXT;
            WCHAR szTab1[] = L"常用符号";
            WCHAR szTab2[] = L"颜色表情";
            tie.pszText = szTab1;
            TabCtrl_InsertItem(pControls->hTab, 0, &tie);
            tie.pszText = szTab2;
            TabCtrl_InsertItem(pControls->hTab, 1, &tie);
            SendMessage(pControls->hTab, TCM_SETBKCOLOR, 0, (LPARAM)RGB(235, 245, 245));
            // 6. 强制重绘
            InvalidateRect(pControls->hTab, NULL, TRUE);
            UpdateWindow(pControls->hTab);

            // 多行编辑框（只读，带滚动条）
            pControls->hEdit = CreateWindow(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                0, 0, 0, 0,
                hDlg, (HMENU)IDC_EMOJI_EDIT, hInstSys, NULL);
            SetWindowTheme(pControls->hEdit, NULL, NULL); // 禁用视觉主题
            SendMessage(pControls->hEdit, WM_SETFONT, (WPARAM)pControls->hFont, TRUE);
            // 说明文本
            pControls->hTipStatic = CreateWindow(L"STATIC", L"提示：单击选择符号，双击或点击【复制】即可复制选中内容到剪贴板",
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                0, 0, 0, 0,
                hDlg, NULL, Global::dllInstanceHandle, NULL);
            SetWindowTheme(pControls->hTipStatic, L"", L"");   // 禁用视觉主题
            SendMessage(pControls->hTipStatic, WM_SETFONT, (WPARAM)pControls->hBFont, TRUE);
            // 复制按钮
            pControls->hCopyBtn = CreateWindow(L"BUTTON", L"复制",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0,
                hDlg, (HMENU)IDC_EMOJI_COPY, Global::dllInstanceHandle, NULL);
            SendMessage(pControls->hCopyBtn, WM_SETFONT, (WPARAM)pControls->hBFont, TRUE);
            // 关闭按钮
            pControls->hCloseBtn = CreateWindow(L"BUTTON", L"关闭",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0,
                hDlg, (HMENU)IDC_EMOJI_CLOSE, Global::dllInstanceHandle, NULL);
            SendMessage(pControls->hCloseBtn, WM_SETFONT, (WPARAM)pControls->hBFont, TRUE);
            // 默认加载第一个页签的内容
            WCHAR szFilePath[MAX_PATH];
            if (GetFilePath(TEXTSERVICE_FUHAO, szFilePath, MAX_PATH))
            {
                std::wstring content = LoadFileContent(szFilePath);
                if (content.empty())
                    content = L"【文件不存在或为空】\n请将 xywb_emoji.txt 放到 DLL 同目录";
                SetWindowText(pControls->hEdit, content.c_str());
            }
            else
            {
                SetWindowText(pControls->hEdit, L"无法获取文件路径");
            }
            // 设置当前页签索引
            //SetWindowLongPtr(hDlg, GWLP_USERDATA + sizeof(void*), 0); // 额外位置存储当前页签

            return TRUE;
        }

        case WM_SIZE:
        {
            if (!pControls) break;
            RECT rcClient;
            GetClientRect(hDlg, &rcClient);
            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;

            // 定义边距（完全保留原始数值，未作任何修改）
            const int margin = 10;
            const int tabHeight = 28;
            const int editTop = margin + tabHeight + 5;
            const int editBottomMargin = 80;  // 原始值，未改
            const int tipHeight = 25;          // 原始值，未改
            const int btnWidth = 80;
            const int btnHeight = 32;
            const int btnSpacing = 10;

            // 移动 Tab 控件
            SetWindowPos(pControls->hTab, NULL, margin, margin,
                width - 2 * margin, tabHeight, SWP_NOZORDER);

            // 移动编辑框
            int editHeight = height - editTop - editBottomMargin;
            if (editHeight < 50) editHeight = 50;
            SetWindowPos(pControls->hEdit, NULL, margin, editTop,
                width - 2 * margin, editHeight, SWP_NOZORDER);

            // 移动提示静态文本
            int tipTop = editTop + editHeight + 5;
            SetWindowPos(pControls->hTipStatic, NULL, margin, tipTop,
                width - 2 * margin, tipHeight, SWP_NOZORDER);

            // 计算按钮位置
            int btnY = tipTop + tipHeight + 5;
            int copyX = width - margin - 2 * btnWidth - btnSpacing;
            int closeX = width - margin - btnWidth;
            SetWindowPos(pControls->hCopyBtn, NULL, copyX, btnY, btnWidth, btnHeight, SWP_NOZORDER);
            SetWindowPos(pControls->hCloseBtn, NULL, closeX, btnY, btnWidth, btnHeight, SWP_NOZORDER);
            return TRUE;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
            pmmi->ptMinTrackSize.x = 350;
            pmmi->ptMinTrackSize.y = 250;
            break;
        }

        case WM_NOTIFY:
        {
            if (pControls && ((NMHDR*)lParam)->idFrom == IDC_EMOJI_TAB && ((NMHDR*)lParam)->code == TCN_SELCHANGE)
            {
                int sel = TabCtrl_GetCurSel(pControls->hTab);
                // 保存当前页签（使用 GWLP_USERDATA 的另一个槽）
                pControls->currentTab = sel;   // 保存当前页签
                WCHAR szFilePath[MAX_PATH];
                if (GetFilePath(sel == 0 ? TEXTSERVICE_FUHAO : TEXTSERVICE_EMOJI, szFilePath, MAX_PATH))
                {
                    std::wstring content = LoadFileContent(szFilePath);
                    if (content.empty())
                        content = (sel == 0) ? L"【文件不存在或为空】\n请将 xywb_emoticon.txt 放到 DLL 同目录" : L"【文件不存在或为空】\n请将 xywb_emoji.txt 放到 DLL 同目录";
                    SetWindowText(pControls->hEdit, content.c_str());
                }
                else
                {
                    SetWindowText(pControls->hEdit, L"无法获取文件路径");
                }
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
        {
            WORD wID = LOWORD(wParam);
            if (wID == IDC_EMOJI_COPY && pControls)
            {
                // 获取选中文本并复制到剪贴板（使用兼容方法）
                HWND hEdit = pControls->hEdit;
                DWORD selStart, selEnd;
                SendMessage(hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                if (selEnd > selStart)
                {
                    int totalLen = GetWindowTextLength(hEdit);
                    if (totalLen > 0)
                    {
                        std::vector<WCHAR> fullBuf(totalLen + 1);
                        GetWindowText(hEdit, fullBuf.data(), totalLen + 1);
                        int selLen = selEnd - selStart;
                        std::wstring selected(fullBuf.data() + selStart, selLen);
                        if (OpenClipboard(hDlg))
                        {
                            EmptyClipboard();
                            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (selLen + 1) * sizeof(WCHAR));
                            if (hGlobal)
                            {
                                WCHAR* pDest = (WCHAR*)GlobalLock(hGlobal);
                                wcscpy_s(pDest, selLen + 1, selected.c_str());
                                GlobalUnlock(hGlobal);
                                SetClipboardData(CF_UNICODETEXT, hGlobal);
                            }
                            CloseClipboard();
                        }
                    }
                }
                return TRUE;
            }
            else if (wID == IDC_EMOJI_CLOSE || wID == IDCANCEL)
            {
                DestroyWindow(hDlg);
                return TRUE;
            }
            break;
        }

        case WM_DESTROY:
        {
            if (pControls)
            {
                if (pControls->hFont)
                    DeleteObject(pControls->hFont);
                if (pControls->hBFont)
                    DeleteObject(pControls->hBFont);
                delete pControls;
                SetWindowLongPtr(hDlg, GWLP_USERDATA, 0);
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hDlg);
            return TRUE;
        }
        return DefWindowProc(hDlg, uMsg, wParam, lParam);
    }
	// 在匿名命名空间内，EditUserWordsDlgProc 下方添加以下内容
        // 表情符号对话框的控件 ID

// 表情符号对话框过程
// 用于存储子控件句柄的结构体
    struct WordsDlgControls {
        HWND hTab;
        HWND hEdit;
        HWND hCopyBtn;
        HWND hCloseBtn;
        HWND hTipStatic;
        int  currentTab;   // 当前选中的页签: 0=常用符号, 1=颜色表情
        HFONT hFont;          // 新增：自定义字体句柄
        HFONT hBFont;   // 其他控件普通字体（非粗体）
        CSampleIME* pIME;
    };
#define IDC_WORDS_TAB      3101
#define IDC_WORDS_EDIT     3102
    static LRESULT CALLBACK WordsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        WordsDlgControls* pControls = (WordsDlgControls*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

        switch (uMsg)
        {
        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hDlg, &rc);
            static HBRUSH hBgBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            FillRect(hdc, &rc, hBgBrush);
            return TRUE;
        }
        case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            static HBRUSH hBtnBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            return (LRESULT)hBtnBrush;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            static HBRUSH hStaticBrush = []() { return CreateSolidBrush(RGB(240, 240, 240)); }();
            return (LRESULT)hStaticBrush;
        }
        case WM_INITDIALOG:
        {
            // 只加载文件内容，控件已在外部创建
            if (pControls)
            {
                WCHAR szFilePath[MAX_PATH];
                if (GetFilePath(Global::isPinyinMode ? TEXTSERVICE_PYUDIC : TEXTSERVICE_UDIC, szFilePath, MAX_PATH))
                {
                    std::wstring content = LoadFileContent(szFilePath);
                    if (content.empty())
                        content = L"【文件不存在或为空】\n请将词库文件放到 DLL 同目录";
                    SetWindowText(pControls->hEdit, content.c_str());
                }
                else
                {
                    SetWindowText(pControls->hEdit, L"无法获取文件路径");
                }
            }
            return TRUE;
        }
        case WM_SIZE:
        {
            if (!pControls) break;
            RECT rcClient;
            GetClientRect(hDlg, &rcClient);
            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;

            const int margin = 10;
            const int tabHeight = 28;
            const int editTop = margin + tabHeight + 5;
            const int editBottomMargin = 80;
            const int tipHeight = 25;
            const int btnWidth = 80;
            const int btnHeight = 32;
            const int btnSpacing = 10;

            SetWindowPos(pControls->hTab, NULL, margin, margin, width - 2 * margin, tabHeight, SWP_NOZORDER);

            int editHeight = height - editTop - editBottomMargin;
            if (editHeight < 50) editHeight = 50;
            SetWindowPos(pControls->hEdit, NULL, margin, editTop, width - 2 * margin, editHeight, SWP_NOZORDER);

            int tipTop = editTop + editHeight + 5;
            SetWindowPos(pControls->hTipStatic, NULL, margin, tipTop, width - 2 * margin, tipHeight, SWP_NOZORDER);

            int btnY = tipTop + tipHeight + 5;
            int copyX = width - margin - 2 * btnWidth - btnSpacing;
            int closeX = width - margin - btnWidth;
            SetWindowPos(pControls->hCopyBtn, NULL, copyX, btnY, btnWidth, btnHeight, SWP_NOZORDER);
            SetWindowPos(pControls->hCloseBtn, NULL, closeX, btnY, btnWidth, btnHeight, SWP_NOZORDER);
            return TRUE;
        }
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
            pmmi->ptMinTrackSize.x = 350;
            pmmi->ptMinTrackSize.y = 250;
            break;
        }
        case WM_NOTIFY:
        {
            if (pControls && ((NMHDR*)lParam)->idFrom == IDC_WORDS_TAB && ((NMHDR*)lParam)->code == TCN_SELCHANGE)
            {
                int sel = TabCtrl_GetCurSel(pControls->hTab);
                pControls->currentTab = sel;
                WCHAR szFilePath[MAX_PATH];
                if (GetFilePath(sel == 0 ? (Global::isPinyinMode ? TEXTSERVICE_PYUDIC : TEXTSERVICE_UDIC) : (Global::isPinyinMode ? TEXTSERVICE_PYDIC : TEXTSERVICE_DIC), szFilePath, MAX_PATH))
                {
                    std::wstring content = LoadFileContent(szFilePath);
                    if (content.empty())
                        content = (sel == 0) ? L"【文件不存在或为空】\n请将用户词库文件放到 DLL 同目录" : L"【文件不存在或为空】\n请将系统词库文件放到 DLL 同目录";
                    SetWindowText(pControls->hEdit, content.c_str());
                }
                else
                {
                    SetWindowText(pControls->hEdit, L"无法获取文件路径");
                }
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
        {
            WORD wID = LOWORD(wParam);
            switch (wID)
            {
            case IDOK:   // 保存按钮（完全仿手工造词）
            {
                if (!pControls) return TRUE;

                // 获取当前页签对应的文件路径
                int sel = TabCtrl_GetCurSel(pControls->hTab);
                LPCWSTR pszFileName = (sel == 0)
                    ? (Global::isPinyinMode ? TEXTSERVICE_PYUDIC : TEXTSERVICE_UDIC)
                    : (Global::isPinyinMode ? TEXTSERVICE_PYDIC : TEXTSERVICE_DIC);

                WCHAR szFilePath[MAX_PATH];
                if (!GetFilePath(pszFileName, szFilePath, MAX_PATH))
                {
                    MessageBox(hDlg, L"无法获取文件路径", L"错误", MB_OK | MB_ICONERROR);
                    return TRUE;
                }

                // 获取编辑框文本
                HWND hEdit = pControls->hEdit;
                int len = GetWindowTextLengthW(hEdit);
                std::wstring content;
                if (len > 0)
                {
                    content.resize(len + 1);
                    GetWindowTextW(hEdit, &content[0], len + 1);
                    content.resize(len);
                }

                // 写入文件（覆盖）
                HANDLE hFile = CreateFileW(szFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    hFile = CreateFileW(szFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                }
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    MessageBoxW(hDlg, L"无法打开或创建文件，请检查目录权限", L"错误", MB_OK | MB_ICONERROR);
                    return TRUE;
                }

                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                SetEndOfFile(hFile);
                WORD bom = 0xFEFF;
                DWORD written;
                WriteFile(hFile, &bom, sizeof(bom), &written, NULL);
                if (!content.empty())
                {
                    DWORD byteLen = static_cast<DWORD>(content.length() * sizeof(wchar_t));
                    WriteFile(hFile, content.c_str(), byteLen, &written, NULL);
                }
                CloseHandle(hFile);
                CCompositionProcessorEngine* pEngine = pControls->pIME->GetCompositionProcessorEngine();
                // ★ 保存词库成功：弹窗 → 重载 → 关闭（与手工造词完全一致）
                MessageBoxW(hDlg, L"保存成功，将重载词库！", L"提示", MB_OK | MB_ICONINFORMATION);

                if (pEngine)
                {
                    pEngine->ReloadDictionaries();
                }

                DestroyWindow(hDlg);
                return TRUE;
            }
            case IDCANCEL:   // 关闭按钮
                DestroyWindow(hDlg);
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
        {
            if (pControls)
            {
                if (pControls->hFont) DeleteObject(pControls->hFont);
                if (pControls->hBFont) DeleteObject(pControls->hBFont);
                delete pControls;
                SetWindowLongPtr(hDlg, GWLP_USERDATA, 0);
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hDlg);
            return TRUE;
        }
        return DefWindowProc(hDlg, uMsg, wParam, lParam);
    }
}
//+---------------------------------------------------------------------------
//
// CSampleIME::_UpdateLanguageBarOnSetFocus
//
//----------------------------------------------------------------------------

void CSampleIME::_UpdateLanguageBarOnSetFocus(_In_ ITfDocumentMgr *pDocMgrFocus)
{
    BOOL needDisableButtons = FALSE;

    if (!pDocMgrFocus) 
    {
        needDisableButtons = TRUE;
    } 
    else 
    {
        IEnumTfContexts* pEnumContext = nullptr;

        if (FAILED(pDocMgrFocus->EnumContexts(&pEnumContext)) || !pEnumContext) 
        {
            needDisableButtons = TRUE;
        } 
        else 
        {
            ULONG fetched = 0;
            ITfContext* pContext = nullptr;

            if (FAILED(pEnumContext->Next(1, &pContext, &fetched)) || fetched != 1) 
            {
                needDisableButtons = TRUE;
            }

            if (!pContext) 
            {
                // context is not associated
                needDisableButtons = TRUE;
            } 
            else 
            {
                pContext->Release();
            }
        }

        if (pEnumContext) 
        {
            pEnumContext->Release();
        }
    }

    CCompositionProcessorEngine* pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;
    pCompositionProcessorEngine->SetLanguageBarStatus(TF_LBI_STATUS_DISABLED, needDisableButtons);
}

//+---------------------------------------------------------------------------
//
// CCompositionProcessorEngine::SetLanguageBarStatus
//
//----------------------------------------------------------------------------

VOID CCompositionProcessorEngine::SetLanguageBarStatus(DWORD status, BOOL isSet)
{
    if (_pLanguageBar_IMEMode) {
        _pLanguageBar_IMEMode->SetStatus(status, isSet);
        BOOL isDesktop = FALSE;
        if (g_isVisibleToolBar) {
            // 检查焦点窗口是否属于桌面环境
            HWND hwnd = ::GetForegroundWindow();
            if (hwnd) {
                if (GetClassNameW(hwnd, Global::foregroundClassName, 128)) {
                OutputDebugString(Global::isGetFocus ? L"22激活输入法 OnSetFocus:  -------------------当前程序名称： --------------------------Global::isGetFocus：---T":
                    L"022激活输入法 OnSetFocus:  -------------------当前程序名称： ------------------------------Global::isGetFocus：---F");
                OutputDebugString(Global::foregroundClassName);
                    // 桌面常见窗口类名CabinetWClass
                    if (wcscmp(Global::foregroundClassName, L"Progman") == 0 ||
                        wcscmp(Global::foregroundClassName, L"WorkerW") == 0 || /*wcscmp(Global::foregroundClassName, L"CabinetWClass") == 0 ||*/
                        wcscmp(Global::foregroundClassName, L"SysListView32") == 0) {
                        isDesktop = TRUE;
                    }
                }
                if(Global::hToolBarWnd && hwnd == Global::hToolBarWnd) isDesktop = TRUE;
            }
        }
        BOOL isVisible = g_isVisibleToolBar && (Global::isGetFocus || isDesktop);
        /*OutputDebugString(g_isVisibleToolBar ? L"030激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---g_isVisibleToolBar----------------T " : L"030激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---g_isVisibleToolBar----------------F");
        OutputDebugString(isDesktop ? L"031激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---isDesktop----------------T " : L"031激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---isDesktop----------------F");
        OutputDebugString(Global::isGetFocus ? L"032激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---Global::isGetFocus----------------T " : L"032激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---Global::isGetFocus----------------F");*/
        _pLanguageBar_IMEMode->SetToolbarVisible(isVisible);
        OutputDebugString(isVisible ? L"033激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---isVisible----------------T " : L"033激活输入法 SetLanguageBarStatus:  -------------------显示工具栏---isVisible----------------F");
    }
    if (_pLanguageBar_DoubleSingleByte) {
        _pLanguageBar_DoubleSingleByte->SetStatus(status, isSet);
    }
    if (_pLanguageBar_Punctuation) {
        _pLanguageBar_Punctuation->SetStatus(status, isSet);
    }
}

//+---------------------------------------------------------------------------
//
// CLangBarItemButton::ctor
//
//----------------------------------------------------------------------------

CLangBarItemButton::CLangBarItemButton(REFGUID guidLangBar, LPCWSTR description, LPCWSTR tooltip, DWORD onIconIndex, DWORD offIconIndex, DWORD onIconIndexPinyin, DWORD onIconIndexPinyinWubi, BOOL isSecureMode, CSampleIME* pIME)
    : _hMenu(nullptr), _pThreadMgr(nullptr), _tfClientId(TF_CLIENTID_NULL), _pSampleIME(pIME), _pToolbar(nullptr) // 保存指针
{
    DWORD bufLen = 0;
    DllAddRef();

    // initialize TF_LANGBARITEMINFO structure.
    _tfLangBarItemInfo.clsidService = Global::SampleIMECLSID;												    // This LangBarItem belongs to this TextService.
    _tfLangBarItemInfo.guidItem = guidLangBar;															        // GUID of this LangBarItem.
    _tfLangBarItemInfo.dwStyle = (TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY);						    // This LangBar is a button type.
    _tfLangBarItemInfo.ulSort = 0;																			    // The position of this LangBar Item is not specified.
    StringCchCopy(_tfLangBarItemInfo.szDescription, ARRAYSIZE(_tfLangBarItemInfo.szDescription), description);  // Set the description of this LangBar Item.

    // Initialize the sink pointer to NULL.
    _pLangBarItemSink = nullptr;

    // Initialize ICON index and file name.
    _onIconIndex = onIconIndex;
    _offIconIndex = offIconIndex;
    _onIconIndexPinyin = onIconIndexPinyin;   // 新增
	_onIconIndexPinyinWubi = onIconIndexPinyinWubi;   // 新增
    // Initialize compartment.
    _pCompartment = nullptr;
    _pCompartmentEventSink = nullptr;

    _isAddedToLanguageBar = FALSE;
    _isSecureMode = isSecureMode;
    _status = 0;

    _refCount = 1;

    // Initialize Tooltip
    _pTooltipText = nullptr;
    if (tooltip)
    {
		size_t len = 0;
		if (StringCchLength(tooltip, STRSAFE_MAX_CCH, &len) != S_OK)
        {
            len = 0; 
        }
        bufLen = static_cast<DWORD>(len) + 1;
        _pTooltipText = (LPCWSTR) new (std::nothrow) WCHAR[ bufLen ];
        if (_pTooltipText)
        {
            StringCchCopy((LPWSTR)_pTooltipText, bufLen, tooltip);
        }
    }   
}

//+---------------------------------------------------------------------------
//
// CLangBarItemButton::dtor
//
//----------------------------------------------------------------------------

CLangBarItemButton::~CLangBarItemButton()
{
    if (_hMenu)
    {
        DestroyMenu(_hMenu);
        _hMenu = nullptr;
    }
    if (_pToolbar) { delete _pToolbar; _pToolbar = nullptr; Global::hToolBarWnd = NULL; }
    DllRelease();
    CleanUp();
}

//+---------------------------------------------------------------------------
//
// CLangBarItemButton::CleanUp
//
//----------------------------------------------------------------------------

void CLangBarItemButton::CleanUp()
{
    if (_pThreadMgr)
    {
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    if (_pTooltipText)
    {
        delete [] _pTooltipText;
        _pTooltipText = nullptr;
    }

    ITfThreadMgr* pThreadMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        IID_ITfThreadMgr, 
        (void**)&pThreadMgr);
    if (SUCCEEDED(hr))
    {
        _UnregisterCompartment(pThreadMgr);

        _RemoveItem(pThreadMgr);
        pThreadMgr->Release();
        pThreadMgr = nullptr;
    }

    if (_pCompartment)
    {
        delete _pCompartment;
        _pCompartment = nullptr;
    }

    if (_pCompartmentEventSink)
    {
        delete _pCompartmentEventSink;
        _pCompartmentEventSink = nullptr;
    }
}

//+---------------------------------------------------------------------------
//
// CLangBarItemButton::QueryInterface
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::QueryInterface(REFIID riid, _Outptr_ void **ppvObj)
{
    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton))
    {
        *ppvObj = (ITfLangBarItemButton *)this;
    }
    else if (IsEqualIID(riid, IID_ITfSource))
    {
        *ppvObj = (ITfSource *)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}


//+---------------------------------------------------------------------------
//
// CLangBarItemButton::AddRef
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CLangBarItemButton::AddRef()
{
    return ++_refCount;
}

//+---------------------------------------------------------------------------
//
// CLangBarItemButton::Release
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CLangBarItemButton::Release()
{
    LONG cr = --_refCount;

    assert(_refCount >= 0);

    if (_refCount == 0)
    {
        delete this;
    }

    return cr;
}

//+---------------------------------------------------------------------------
//
// GetInfo
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::GetInfo(_Out_ TF_LANGBARITEMINFO *pInfo)
{
    _tfLangBarItemInfo.dwStyle |= TF_LBI_STYLE_SHOWNINTRAY;
    *pInfo = _tfLangBarItemInfo;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// GetStatus
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::GetStatus(_Out_ DWORD *pdwStatus)
{
    if (pdwStatus == nullptr)
    {
        E_INVALIDARG;
    }

    *pdwStatus = _status;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// SetStatus
//
//----------------------------------------------------------------------------

void CLangBarItemButton::SetStatus(DWORD dwStatus, BOOL fSet)
{
    BOOL isChange = FALSE;

    if (fSet) 
    {
        if (!(_status & dwStatus)) 
        {
            _status |= dwStatus;
            isChange = TRUE;
        }
    } 
    else 
    {
        if (_status & dwStatus) 
        {
            _status &= ~dwStatus;
            isChange = TRUE;
        }
    }

    if (isChange && _pLangBarItemSink) 
    {
        _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
    }

    return;
}

//+---------------------------------------------------------------------------
//
// Show
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::Show(BOOL fShow)
{
	fShow;
    if (_pLangBarItemSink)
    {
        _pLangBarItemSink->OnUpdate(TF_LBI_STATUS);
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// GetTooltipString
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::GetTooltipString(_Out_ BSTR *pbstrToolTip)
{
    *pbstrToolTip = SysAllocString(_pTooltipText);

    return (*pbstrToolTip == nullptr) ? E_OUTOFMEMORY : S_OK;
}

STDAPI CLangBarItemButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea)
{
    pt; prcArea;
    if (click == TF_LBI_CLK_RIGHT)
    {
        HWND hForeground = GetForegroundWindow();
        if (hForeground == NULL) {
            hForeground = GetDesktopWindow();
        }
        BOOL bIsNotepad = FALSE;
        HWND hDesktop = FALSE;
        if (hForeground)
        {
            WCHAR szClassName[64] = { 0 };
            GetClassName(hForeground, szClassName, 64);
            // 记事本的主窗口类名可能是 "Notepad" 或 "Edit"
            if (wcscmp(szClassName, L"Notepad") == 0 || wcscmp(szClassName, L"Edit") == 0)
            {
                bIsNotepad = TRUE;
            }
        }
        if (bIsNotepad)
        {
            hDesktop = FindWindow(L"Progman", NULL);
            if (!hDesktop) hDesktop = FindWindow(L"WorkerW", NULL);
            if (!hDesktop) hDesktop = GetDesktopWindow();
        }
        if (bIsNotepad && hDesktop)
        {
            SetForegroundWindow(hDesktop);
            // 非阻塞延时，允许消息处理
            DWORD endTime = GetTickCount() + 150;
            while (GetTickCount() < endTime) {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
            InitMenu(NULL);
            return S_OK;
        }
        // 直接显示右键菜单
        InitMenu(NULL);
        return S_OK;
    }

    // 左键保持原有切换逻辑
    BOOL isOn = FALSE;
    _pCompartment->_GetCompartmentBOOL(isOn);
    _pCompartment->_SetCompartmentBOOL(isOn ? FALSE : TRUE);
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// InitMenu
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::InitMenu(ITfMenu* pMenu)
{
    UNREFERENCED_PARAMETER(pMenu);

    if (!_hMenu)
        _CreatePopupMenu();

    _UpdateMenuItems();

    POINT pt;
    GetCursorPos(&pt);
    // 使用 GetForegroundWindow() 作为父窗口
    HWND hParent = GetForegroundWindow();
    if (hParent == NULL) {
        hParent = GetDesktopWindow();
    }
    UINT uSelected = TrackPopupMenu(_hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hParent, NULL);
    if (uSelected)
        OnMenuSelect(uSelected);

    return S_OK;
}

void CLangBarItemButton::_CreatePopupMenu()
{
    _hMenu = CreatePopupMenu();
    AppendMenu(_hMenu, MF_STRING, ID_MENU_INPUT_MODE, L"输入模式（五笔）");  // 文本稍后更新
    AppendMenu(_hMenu, MF_STRING, ID_MENU_FULL_WIDTH, L"全角模式");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_PUNCTUATION, L"中英文符号");
    AppendMenu(_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(_hMenu, MF_STRING, ID_MENU_EMOJI, L"表情符号");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_EDITWORDS, L"编辑用户词库");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_USER_WORD, L"手工造词");
    AppendMenu(_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(_hMenu, MF_STRING, ID_MENU_HELP, L"帮助");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_SETTINGS, L"设置");
    AppendMenu(_hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(_hMenu, MF_STRING, ID_MENU_SHOWTOOL, L"显示工具栏（开）");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_CHANGEHV, Global::isHorizontalMode?  L"候选列表->纵向": L"候选词条=>横向");
    AppendMenu(_hMenu, MF_STRING, ID_MENU_RELOAD_DICT, L"重载词典");
}

void CLangBarItemButton::_UpdateMenuItems()
{
    if (!_hMenu) return;

    // 1. 输入模式：动态文本
    BOOL isPinyin = Global::isPinyinMode;
    WCHAR szInputMode[64];
    StringCchPrintf(szInputMode, 64, L"输入模式（%s）", isPinyin ? L"拼音" : L"五笔");
    ModifyMenu(_hMenu, ID_MENU_INPUT_MODE, MF_STRING, ID_MENU_INPUT_MODE, szInputMode);
    CheckMenuItem(_hMenu, ID_MENU_INPUT_MODE, MF_CHECKED);
    // 不需要勾选，仅显示当前模式

    // 2. 全角模式（读取 Compartment）
    if (_pCompartment && _pThreadMgr && _tfClientId != TF_CLIENTID_NULL)
    {
        CCompartment CompDouble(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
        BOOL isFullWidth = FALSE;
        CompDouble._GetCompartmentBOOL(isFullWidth);
        CheckMenuItem(_hMenu, ID_MENU_FULL_WIDTH, isFullWidth ? MF_CHECKED : MF_UNCHECKED);
    }

    // 3. 中英文符号
    if (_pCompartment && _pThreadMgr && _tfClientId != TF_CLIENTID_NULL)
    {
        CCompartment CompPunct(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
        BOOL isChinesePunct = FALSE;
        CompPunct._GetCompartmentBOOL(isChinesePunct);
        CheckMenuItem(_hMenu, ID_MENU_PUNCTUATION, isChinesePunct ? MF_CHECKED : MF_UNCHECKED);
    }
    CheckMenuItem(_hMenu, ID_MENU_SHOWTOOL, g_isVisibleToolBar ? MF_CHECKED : MF_UNCHECKED);
}

//+---------------------------------------------------------------------------
//
// OnMenuSelect
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::OnMenuSelect(UINT wID)
{
    switch (wID)
    {
    case ID_MENU_INPUT_MODE:
    {
        // 切换五笔/拼音模式
        Global::isPinyinMode = !Global::isPinyinMode;
		Global::isPyAndWbMode = FALSE; // 切换输入模式时，重置为单一模式
		// 切换输入模式时，确保键盘处于中文状态
        CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
        BOOL isOpen = FALSE;
        CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);
        if (!isOpen)
        {
            CompartmentKeyboardOpen._SetCompartmentBOOL(TRUE);
            Global::isChineseMode = TRUE;
        }
        // 重载词典
        if (_pSampleIME)
        {
            CCompositionProcessorEngine* pEngine = _pSampleIME->GetCompositionProcessorEngine();
            if (pEngine)
            {
                pEngine->ReloadDictionaries();
            }
        }
        RefreshIcon();
    }
    break;

    case ID_MENU_FULL_WIDTH:
    {
        if (_pThreadMgr && _tfClientId != TF_CLIENTID_NULL)
        {
            CCompartment CompDouble(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
            BOOL isFull = FALSE;
            CompDouble._GetCompartmentBOOL(isFull);
            CompDouble._SetCompartmentBOOL(!isFull);
        }
    }
    break;

    case ID_MENU_PUNCTUATION:
    {
        if (_pThreadMgr && _tfClientId != TF_CLIENTID_NULL)
        {
            CCompartment CompPunct(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
            BOOL isChinese = FALSE;
            CompPunct._GetCompartmentBOOL(isChinese);
            CompPunct._SetCompartmentBOOL(!isChinese);
        }
    }
    break;

    case ID_MENU_EMOJI:
        _ShowEmojiDialog();
        break;

    case ID_MENU_SETTINGS:
        _ShowSettingsDialog();
        break;
    case ID_MENU_EDITWORDS:
        _EditUserWords();
        break;
    case ID_MENU_CHANGEHV:
    {
        Global::isHorizontalMode = !Global::isHorizontalMode;
        if (_hMenu) {
            WCHAR szText[64];
            StringCchPrintf(szText, 64, Global::isHorizontalMode ? L"候选列表->纵向" : L"候选词条=>横向");
            ModifyMenu(_hMenu, ID_MENU_CHANGEHV, MF_STRING, ID_MENU_CHANGEHV, szText);
        }
    }

        break;
    case ID_MENU_SHOWTOOL:
    {
        BOOL newState = !g_isVisibleToolBar;
        g_isVisibleToolBar = newState;
        //OutputDebugString(g_isVisibleToolBar ?L"CLangBarItemButton::OnMenuSelect :g_isVisibleToolBar ----------------T ": L"CLangBarItemButton::OnMenuSelect :g_isVisibleToolBar ----------------F ");
        SetToolbarVisible(newState);
    }
        break;
    case ID_MENU_USER_WORD:
        _ShowUserWordDialog();
        break;

    case ID_MENU_HELP:
        _ShowHelpDialog();
        break;
    case ID_MENU_RELOAD_DICT:
    {
        if (_pSampleIME)
        {
            CCompositionProcessorEngine* pEngine = _pSampleIME->GetCompositionProcessorEngine();
            if (pEngine)
            {
                pEngine->ReloadDictionaries();
                MessageBox(NULL, L"词库已重新加载", L"提示", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBox(NULL, L"无法获取输入引擎", L"错误", MB_OK | MB_ICONERROR);
            }
        }
    }
    break;
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// GetIcon
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::GetIcon(_Out_ HICON *phIcon)
{
    BOOL isOn = FALSE;

    if (!_pCompartment)
    {
        return E_FAIL;
    }
    if (!phIcon)
    {
        return E_FAIL;
    }
    *phIcon = nullptr;

    _pCompartment->_GetCompartmentBOOL(isOn);

    DWORD status = 0;
    GetStatus(&status);

	// If IME is working on the UAC mode, the size of ICON should be 24 x 24.
    // 动态获取系统小图标的当前标准尺寸（会在DPI变化时返回正确值）
    int desiredSize = GetSystemMetrics(SM_CXSMICON);

    // 如果处于安全模式（如UAC界面），使用系统大图标尺寸
    if (_isSecureMode)
    {
        desiredSize = GetSystemMetrics(SM_CXICON);
    }
    //int desiredSize = 16;
    //if (_isSecureMode) // detect UAC mode
    //{
    //    desiredSize = _isSecureMode ? 24 : 16;
    //}

    DWORD iconIndex = (isOn && !(status & TF_LBI_STATUS_DISABLED))
        ? (Global::isPinyinMode ? (Global::isPyAndWbMode ? _onIconIndexPinyinWubi : _onIconIndexPinyin) : _onIconIndex)
        : _offIconIndex;

    if (Global::dllInstanceHandle)
    {
        *phIcon = reinterpret_cast<HICON>(LoadImage(Global::dllInstanceHandle, MAKEINTRESOURCE(iconIndex), IMAGE_ICON, desiredSize, desiredSize, 0));
    }

    return (*phIcon != NULL) ? S_OK : E_FAIL;
}

//+---------------------------------------------------------------------------
//
// GetText
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::GetText(_Out_ BSTR *pbstrText)
{
    *pbstrText = SysAllocString(_tfLangBarItemInfo.szDescription);

    return (*pbstrText == nullptr) ? E_OUTOFMEMORY : S_OK;
}

//+---------------------------------------------------------------------------
//
// AdviseSink
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::AdviseSink(__RPC__in REFIID riid, __RPC__in_opt IUnknown *punk, __RPC__out DWORD *pdwCookie)
{
    // We allow only ITfLangBarItemSink interface.
    if (!IsEqualIID(IID_ITfLangBarItemSink, riid))
    {
        return CONNECT_E_CANNOTCONNECT;
    }

    // We support only one sink once.
    if (_pLangBarItemSink != nullptr)
    {
        return CONNECT_E_ADVISELIMIT;
    }

    // Query the ITfLangBarItemSink interface and store it into _pLangBarItemSink.
    if (punk == nullptr)
    {
        return E_INVALIDARG;
    }
    if (punk->QueryInterface(IID_ITfLangBarItemSink, (void **)&_pLangBarItemSink) != S_OK)
    {
        _pLangBarItemSink = nullptr;
        return E_NOINTERFACE;
    }

    // return our cookie.
    *pdwCookie = _cookie;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// UnadviseSink
//
//----------------------------------------------------------------------------

STDAPI CLangBarItemButton::UnadviseSink(DWORD dwCookie)
{
    // Check the given cookie.
    if (dwCookie != _cookie)
    {
        return CONNECT_E_NOCONNECTION;
    }

    // If there is nno connected sink, we just fail.
    if (_pLangBarItemSink == nullptr)
    {
        return CONNECT_E_NOCONNECTION;
    }

    _pLangBarItemSink->Release();
    _pLangBarItemSink = nullptr;

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _AddItem
//
//----------------------------------------------------------------------------

HRESULT CLangBarItemButton::_AddItem(_In_ ITfThreadMgr *pThreadMgr)
{
    HRESULT hr = S_OK;
    ITfLangBarItemMgr* pLangBarItemMgr = nullptr;

    if (_isAddedToLanguageBar)
    {
        return S_OK;
    }

    hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void **)&pLangBarItemMgr);
    if (SUCCEEDED(hr))
    {
        hr = pLangBarItemMgr->AddItem(this);
        if (SUCCEEDED(hr))
        {
            _isAddedToLanguageBar = TRUE;
        }
        pLangBarItemMgr->Release();
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// _RemoveItem
//
//----------------------------------------------------------------------------

HRESULT CLangBarItemButton::_RemoveItem(_In_ ITfThreadMgr *pThreadMgr)
{
    HRESULT hr = S_OK;
    ITfLangBarItemMgr* pLangBarItemMgr = nullptr;

    if (!_isAddedToLanguageBar)
    {
        return S_OK;
    }

    hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void **)&pLangBarItemMgr);
    if (SUCCEEDED(hr))
    {
        hr = pLangBarItemMgr->RemoveItem(this);
        if (SUCCEEDED(hr))
        {
            _isAddedToLanguageBar = FALSE;
        }
        pLangBarItemMgr->Release();
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// _RegisterCompartment
//
//----------------------------------------------------------------------------

BOOL CLangBarItemButton::_RegisterCompartment(_In_ ITfThreadMgr* pThreadMgr, TfClientId tfClientId, REFGUID guidCompartment)
{
    // 保存线程管理器和客户端ID
    _pThreadMgr = pThreadMgr;
    if (_pThreadMgr)
        _pThreadMgr->AddRef();
    _tfClientId = tfClientId;

    _pCompartment = new (std::nothrow) CCompartment(pThreadMgr, tfClientId, guidCompartment);
    if (_pCompartment)
    {
        _pCompartmentEventSink = new (std::nothrow) CCompartmentEventSink(_CompartmentCallback, this);
        if (_pCompartmentEventSink)
        {
            _pCompartmentEventSink->_Advise(pThreadMgr, guidCompartment);
        }
        else
        {
            delete _pCompartment;
            _pCompartment = nullptr;
        }
    }
    return _pCompartment ? TRUE : FALSE;
}

//+---------------------------------------------------------------------------
//
// _UnregisterCompartment
//
//----------------------------------------------------------------------------

BOOL CLangBarItemButton::_UnregisterCompartment(_In_ ITfThreadMgr *pThreadMgr)
{
	pThreadMgr;
    if (_pCompartment)
    {
        // Unadvice ITfCompartmentEventSink
        if (_pCompartmentEventSink)
        {
            _pCompartmentEventSink->_Unadvise();
        }

        // clear ITfCompartment
        _pCompartment->_ClearCompartment();
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _CompartmentCallback
//
//----------------------------------------------------------------------------

// static
HRESULT CLangBarItemButton::_CompartmentCallback(_In_ void *pv, REFGUID guidCompartment)
{
    CLangBarItemButton* fakeThis = (CLangBarItemButton*)pv;

    GUID guid = GUID_NULL;
    fakeThis->_pCompartment->_GetGUID(&guid);

    if (IsEqualGUID(guid, guidCompartment))
    {
        if (fakeThis->_pLangBarItemSink)
        {
            fakeThis->_pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
        }
    }

    return S_OK;
}
void CLangBarItemButton::RefreshIcon()
{
    if (_pLangBarItemSink)
    {
        _pLangBarItemSink->OnUpdate(TF_LBI_ICON);
    }
    if (_pToolbar)
    {
        _pToolbar->UpdateAllButtons();   // 重绘所有按钮，第二个按钮会根据 Global 状态自动选择图标
    }
}
void CLangBarItemButton::RefreshToolBarIcon()
{
    if (_pToolbar)
    {
        _pToolbar->UpdateAllButtons();   // 重绘所有按钮，第二个按钮会根据 Global 状态自动选择图标
    }
}
// 实现 CLangBarItemButton::_ShowEmojiDialog
void CLangBarItemButton::_ShowEmojiDialog()
{
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);

    HWND hParent = GetDesktopWindow();
    HINSTANCE hInst = Global::dllInstanceHandle;
    const wchar_t szDlgClass[] = L"EmojiDialogClass";

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = EmojiDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szDlgClass;
    RegisterClassEx(&wc);

    int dlgWidth = 650, dlgHeight = 493;
    RECT rcParent = { 0 };
    GetWindowRect(hParent, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        szDlgClass,
        L"表情符号",
        WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_SIZEBOX | WS_MAXIMIZEBOX,
        x, y, dlgWidth, dlgHeight,
        hParent, NULL, hInst, NULL);

    if (!hDlg) return;

    // ========== 关键：手动发送 WM_INITDIALOG 以创建子控件 ==========
    SendMessage(hDlg, WM_INITDIALOG, 0, 0);

    // 桌面父窗口时不禁用
    if (hParent != GetDesktopWindow() && hParent != NULL)
        EnableWindow(hParent, FALSE);

    ShowWindow(hDlg, SW_SHOW);
    SendMessage(hDlg, WM_SIZE, 0, 0);   // 强制布局

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(hDlg)) break;
    }

    if (hParent != GetDesktopWindow() && hParent != NULL)
    {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    UnregisterClass(szDlgClass, hInst);
}
void CLangBarItemButton::_ShowSettingsDialog()
{
    ::ShowSettingsDialog();
}
void CLangBarItemButton::RecreateToolbar()
{
    if (s_bRecreating) return;
    s_bRecreating = TRUE;
    Global::b_showedToolbar = TRUE;
    // 1. 销毁旧工具栏
    if (Global::hToolBarWnd && IsWindow(Global::hToolBarWnd)) {
        OutputDebugString(L" 0011 DestroyWindow(Global::hToolBarWnd);----------------T000000000");
        DestroyWindow(Global::hToolBarWnd);
        Global::hToolBarWnd = nullptr;
    }
    if (_pToolbar)
    {
        HWND hWnd = _pToolbar->_GetHwnd();
        if (hWnd)
        {
            OutputDebugString(L" 0022 DestroyWindow(_pToolbar->_GetHwnd(););----------------T1111111 ");
            DestroyWindow(hWnd);      // 同步销毁窗口
        }
        delete _pToolbar;            // 释放对象
        _pToolbar = nullptr;
        Global::hToolBarWnd = nullptr;
    }
    OutputDebugString(L" 0033 new CToolbarWindow(_pSampleIME, this);;----------------T22222222222222222");
    // 2. 创建新工具栏（方向标志 g_isHorizontalWin 已切换）
    _pToolbar = new CToolbarWindow(_pSampleIME, this);
    if (_pToolbar)
    {
        HWND hParent = GetDesktopWindow();
        if (_pToolbar->Create(hParent))
        {
            Global::hToolBarWnd = _pToolbar->_GetHwnd();
            _pToolbar->Show(TRUE);
        }
        else
        {
            delete _pToolbar;
            _pToolbar = nullptr;
        }
    }
	s_bRecreating = FALSE;
    Global::b_showedToolbar = FALSE;
}
void CLangBarItemButton::SetToolbarVisible(BOOL bShow)
{
    //OutputDebugString(bShow ? L"SetToolbarVisible: show toolbar-------T \n" : L"SetToolbarVisible: hide toolbar--------------F\n");
    if (!_pSampleIME) return;
    // 如果 bShow == TRUE 且工具栏未创建，则创建它
    if (_pToolbar && (!IsWindow(_pToolbar->_GetHwnd()) || Global::hToolBarWnd == NULL)){
        delete _pToolbar;
        _pToolbar = nullptr;
        Global::hToolBarWnd = NULL;
    }
    if (!bShow) {
        if (_pToolbar) {
            _pToolbar->Show(FALSE);
            // 更新菜单（可选）
            if (_hMenu) {
                WCHAR szText[64];
                StringCchPrintf(szText, 64, L"显示工具栏（关）");
                ModifyMenu(_hMenu, ID_MENU_SHOWTOOL, MF_STRING, ID_MENU_SHOWTOOL, szText);
            }
        }
        return;
    }
    // 需要显示时，检查是否需要重建
    BOOL needRecreate = FALSE;
    if (!_pToolbar) {
        needRecreate = TRUE;
    }
    else {
        RECT rc;
        if (::GetWindowRect(_pToolbar->_GetHwnd(), &rc)) {
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            // 判断当前窗口方向（简单通过宽高比）
            BOOL currentHorizontal = (w > h);
            if (currentHorizontal != g_isHToolbarWin) {
                needRecreate = TRUE;
            }
        }
        else {
            needRecreate = TRUE; // 窗口无效
        }
    }
    if (needRecreate) {
        // 直接调用 RecreateToolbar（它会销毁旧窗口、创建新窗口并显示）
        RecreateToolbar();
        // 更新菜单
        if (_hMenu) {
            WCHAR szText[64];
            StringCchPrintf(szText, 64, L"显示工具栏（开）");
            ModifyMenu(_hMenu, ID_MENU_SHOWTOOL, MF_STRING, ID_MENU_SHOWTOOL, szText);
        }
        return;
    }
    // 设置可见性
    // 方向一致，直接显示
    if (_pToolbar) {
        _pToolbar->Show(TRUE);
        if (_hMenu) {
            WCHAR szText[64];
            StringCchPrintf(szText, 64, L"显示工具栏（开）");
            ModifyMenu(_hMenu, ID_MENU_SHOWTOOL, MF_STRING, ID_MENU_SHOWTOOL, szText);
        }
    }
    // 更新右键菜单的勾选状态
   // OutputDebugString(L"SetToolbarVisible: toolbar created and shown------------------------------1111111111111111\n");
}

void CLangBarItemButton::_EditUserWords()
{
    if (!_pSampleIME) return;

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);

    HWND hParent = GetDesktopWindow();
    HINSTANCE hInst = Global::dllInstanceHandle;

    // 注册窗口类
    const wchar_t szDlgClass[] = L"WordsDialogClass";
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WordsDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szDlgClass;
    RegisterClassEx(&wc);

    int dlgWidth = 650, dlgHeight = 850;
    RECT rcParent = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &rcParent);
    }
    else {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcParent, 0);
    }
    int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        szDlgClass,
        L"用户词库编辑",
        WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_SIZEBOX | WS_MAXIMIZEBOX,
        x, y, dlgWidth, dlgHeight,
        hParent,
        NULL,
        hInst,
        (LPVOID)_pSampleIME   // 传给 WM_INITDIALOG
    );

    if (!hDlg) return;

    // 创建字体
    HFONT hFont = CreateFont(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
    HFONT hBFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

    // Tab 控件
    HWND hTab = CreateWindow(WC_TABCONTROL, NULL,
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
        10, 10, dlgWidth - 20, 28,
        hDlg, (HMENU)IDC_WORDS_TAB, hInst, NULL);
    SendMessage(hTab, WM_SETFONT, (WPARAM)hBFont, TRUE);
    SendMessage(hTab, TCM_SETBKCOLOR, 0, (LPARAM)RGB(240, 240, 240));

    TCITEM tie = { 0 };
    tie.mask = TCIF_TEXT;
    WCHAR szTab1[] = L"用户词库";
    WCHAR szTab2[] = L"系统词库";
    tie.pszText = szTab1;
    TabCtrl_InsertItem(hTab, 0, &tie);
    tie.pszText = szTab2;
    TabCtrl_InsertItem(hTab, 1, &tie);

    // 多行编辑框
    HWND hEdit = CreateWindow(L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        10, 43, dlgWidth - 20, dlgHeight - 43 - 80 - 25 - 10 - 5 - 5,
        hDlg, (HMENU)IDC_WORDS_EDIT, hInst, NULL);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 提示静态文本
    HWND hTipStatic = CreateWindow(L"STATIC", L"提示：每行一个词条，格式：编码+空格+词语1+空格+词语2+... ，示例：rjpg 昕宇",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        10, dlgHeight - 80 - 25 - 5, dlgWidth - 20, 25,
        hDlg, NULL, hInst, NULL);
    SendMessage(hTipStatic, WM_SETFONT, (WPARAM)hBFont, TRUE);

    // ★ 确定按钮（使用 IDOK）
    HWND hOkBtn = CreateWindow(L"BUTTON", L"确定",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgWidth - 10 - 80 - 10 - 80, dlgHeight - 80, 80, 32,
        hDlg, (HMENU)IDOK, hInst, NULL);
    SendMessage(hOkBtn, WM_SETFONT, (WPARAM)hBFont, TRUE);

    // ★ 关闭按钮（使用 IDCANCEL）
    HWND hCloseBtn = CreateWindow(L"BUTTON", L"关闭",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgWidth - 10 - 80, dlgHeight - 80, 80, 32,
        hDlg, (HMENU)IDCANCEL, hInst, NULL);
    SendMessage(hCloseBtn, WM_SETFONT, (WPARAM)hBFont, TRUE);

    // 存储控件句柄到 GWLP_USERDATA
    WordsDlgControls* pControls = new WordsDlgControls();
    pControls->hTab = hTab;
    pControls->hEdit = hEdit;
    pControls->hTipStatic = hTipStatic;
    pControls->hCopyBtn = hOkBtn;     // 实际上就是 IDOK 按钮
    pControls->hCloseBtn = hCloseBtn; // IDCANCEL
    pControls->hFont = hFont;
    pControls->hBFont = hBFont;
    pControls->pIME = _pSampleIME;
    pControls->currentTab = 0;
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pControls);

    // 发送 WM_INITDIALOG 进行初始化（加载文件内容）
    SendMessage(hDlg, WM_INITDIALOG, 0, 0);

    // 模态消息循环（与手工造词完全相同）
    EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(hDlg)) break;
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    UnregisterClass(szDlgClass, hInst);
}

void CLangBarItemButton::_ShowHelpDialog()
{
    WCHAR szHelp[512];
    StringCchPrintf(szHelp, 512,
        L"昕宇五笔输入法\n版本 1.0\n\n快捷键说明：\n"
        L"左 Ctrl + 空格  ：切换五笔/拼音\n"
        L"Shift/Ctrl+空格 ：切换中/英文输入模式\n"
        L"右Shift         ：快速切换中英文\n"
        L"左Ctrl/Shift    ：快速切换五笔拼音\n"
        L"长右Ctrl/Ctrl+W ：快速手工造词\n"
        L"短右Ctrl        ：切换横向纵向候选窗口\n"
        L"候选：左右上下方向键或数字键或鼠标点击");
    MessageBox(NULL,  szHelp, L"帮助", MB_OK);
}

void CLangBarItemButton::_ShowUserWordDialog()
{
    if (!_pSampleIME) return;

    HWND hParent = NULL;
    if (hParent == NULL) hParent = GetDesktopWindow();
    HINSTANCE hInst = Global::dllInstanceHandle;

    const wchar_t szDlgClass[] = L"UserWordDialogClass";
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = UserWordDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szDlgClass;
    RegisterClassEx(&wc);

    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        szDlgClass,
        L"手工造词",
        WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 230,
        hParent,
        NULL,
        hInst,
        (LPVOID)_pSampleIME   // 传入指针，用于 WM_INITDIALOG
    );

    if (!hDlg) return;

    // 创建子控件...
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hBFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
    HWND label1= CreateWindow(L"STATIC", L"词 语：", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 23, 60, 24, hDlg, NULL, hInst, NULL);
    SendMessage(label1, WM_SETFONT, (WPARAM)hBFont, TRUE);
    label1 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 85, 20, 260, 24, hDlg, (HMENU)IDC_WORD_EDIT, hInst, NULL);
    SendMessage(label1, WM_SETFONT, (WPARAM)hBFont, TRUE);
    label1 = CreateWindow(L"STATIC", L"编 码：", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 63, 60, 24, hDlg, NULL, hInst, NULL);
    SendMessage(label1, WM_SETFONT, (WPARAM)hBFont, TRUE);
    label1 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 85, 60, 260, 24, hDlg, (HMENU)IDC_CODE_EDIT, hInst, NULL);
    SendMessage(label1, WM_SETFONT, (WPARAM)hBFont, TRUE);
    CreateWindow(L"BUTTON", Global::isPinyinMode? L"同时添加五笔词语" : L"同时添加拼音词语", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 85, 100, 180, 24, hDlg, (HMENU)IDC_ADD_PINYIN, hInst, NULL);
    CreateWindow(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 180, 140, 70, 28, hDlg, (HMENU)IDOK, hInst, NULL);
    CreateWindow(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 270, 140, 70, 28, hDlg, (HMENU)IDCANCEL, hInst, NULL);

    EnumChildWindows(hDlg, [](HWND hWnd, LPARAM lParam) -> BOOL {
        SendMessage(hWnd, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
        }, (LPARAM)hFont);

    // 手动发送 WM_INITDIALOG 初始化
    SendMessage(hDlg, WM_INITDIALOG, 0, (LPARAM)_pSampleIME);
    // ========== 新增：窗口居中 ==========
    RECT rcParent = { 0 };
    if (hParent && IsWindow(hParent)) {
        GetWindowRect(hParent, &rcParent);
    }
    else {
        // 没有父窗口则使用屏幕工作区
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcParent, 0);
    }
    int dlgWidth = 380, dlgHeight = 230;
    int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;
    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    // 模态消息循环
    EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(hDlg)) break;
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    UnregisterClass(szDlgClass, hInst);
}