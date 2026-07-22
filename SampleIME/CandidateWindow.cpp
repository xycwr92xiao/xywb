// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "Globals.h"
#include "BaseWindow.h"
#include "CandidateWindow.h"

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CCandidateWindow::CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void* pv, _In_ CCandidateRange* pIndexRange, _In_ BOOL isStoreAppMode, _In_opt_ CTableDictionaryEngine* pWubiEngine)
    : _pWubiEngine(pWubiEngine)
{
    _currentSelection = 0;

    _SetTextColor(CANDWND_ITEM_COLOR, GetSysColor(COLOR_WINDOW));    // text color is black
    //_SetFillColor((HBRUSH)(COLOR_WINDOW+1));

    _pIndexRange = pIndexRange;

    _pfnCallback = pfnCallback;
    _pObj = pv;

    _cyRow = CANDWND_ROW_WIDTH;
    _cxTitle = 0;

    _pVScrollBarWnd = nullptr;

    _wndWidth = 0;

    _dontAdjustOnEmptyItemPage = FALSE;

    _isStoreAppMode = isStoreAppMode;
    _isHorizontalMode = Global::isHorizontalMode; // Global::isPinyinMode ? FALSE:TRUE;
    _maxHorizontalItems = Global::nMaxHorizontalItems;//Global::isPinyinMode ? 10:5;//默认横向最多显示5个词条
    _ShowCode = Global::showRemainingCode;//是否显示编码
    _lastMousePos.x = -1;
    _lastMousePos.y = -1;
    _itemSpacing = 8;
    SetHorizontalMode(_isHorizontalMode, _maxHorizontalItems, _itemSpacing);
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CCandidateWindow::~CCandidateWindow()
{
    _ClearList();
    _DeleteVScrollBarWnd();
}

//+---------------------------------------------------------------------------
//
// _Create
//
// CandidateWinow is the top window
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_Create(ATOM atom, _In_ UINT wndWidth, _In_opt_ HWND parentWndHandle)
{
    BOOL ret = FALSE;
    _wndWidth = wndWidth;

    ret = _CreateMainWindow(atom, parentWndHandle);
    if (FALSE == ret)
    {
        goto Exit;
    }

    if (!_isHorizontalMode)  // 纵向模式才创建滚动条
    {
        ret = _CreateVScrollWindow();
        if (FALSE == ret) goto Exit;
    }

    _ResizeWindow();

Exit:
    return TRUE;
}
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
BOOL CCandidateWindow::_CreateMainWindow(ATOM atom, _In_opt_ HWND parentWndHandle)
{
    _SetUIWnd(this);

    if (!CBaseWindow::_Create(atom,
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        WS_POPUP | WS_THICKFRAME, 
        NULL, 0, 0, parentWndHandle))
    {
        return FALSE;
    }
    // 启用系统圆角（Win11+）
    HMODULE hDwm = GetModuleHandle(L"dwmapi.dll");
    if (hDwm)
    {
        auto pDwmSetWindowAttribute = (decltype(&DwmSetWindowAttribute))GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute)
        {
            DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
            pDwmSetWindowAttribute(_GetWnd(), DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
        }
    }
    return TRUE;
}


BOOL CCandidateWindow::_CreateVScrollWindow()
{
    BOOL ret = FALSE;

    SHELL_MODE shellMode = _isStoreAppMode ? STOREAPP : DESKTOP;
    CScrollBarWindowFactory* pFactory = CScrollBarWindowFactory::Instance();
    _pVScrollBarWnd = pFactory->MakeScrollBarWindow(shellMode);

    if (_pVScrollBarWnd == nullptr)
    {
        goto Exit;
    }

    _pVScrollBarWnd->_SetUIWnd(this);

    if (!_pVScrollBarWnd->_Create(Global::AtomScrollBarWindow, WS_EX_TOPMOST | WS_EX_TOOLWINDOW, WS_CHILD, this))
    {
        _DeleteVScrollBarWnd();
        goto Exit;
    }
    
    ret = TRUE;

Exit:
    pFactory->Release();
    return ret;
}

void CCandidateWindow::_ResizeWindow()
{
    SIZE size = {0, 0};
    if (_isHorizontalMode)
    {
        CBaseWindow::_Resize(0, 0, _cxTitle, _cyRow + _contentTopMargin * 2 );
        if (_pVScrollBarWnd) _pVScrollBarWnd->_Show(FALSE);
        // 更新圆角（因为大小改变了）
        //if (_GetWnd() && _roundCornerRadius > 0) {
        //    RECT rc;
        //    GetWindowRect(_GetWnd(), &rc);
        //    int w = rc.right - rc.left;
        //    int h = rc.bottom - rc.top;
        //    HRGN hRgn = CreateRoundRectRgn(0, 0, w, h, _roundCornerRadius, _roundCornerRadius);
        //    SetWindowRgn(_GetWnd(), hRgn, TRUE);
        //}
        return;
    }
    // 纵向模式
    _cxTitle = max(_cxTitle, size.cx + 2 * GetSystemMetrics(SM_CXFRAME));
    int candidateListPageCnt = _pIndexRange->Count();
    int totalHeight = _cyRow * candidateListPageCnt + _contentTopMargin * 2 ;
    CBaseWindow::_Resize(0, 0, _cxTitle, totalHeight);

    // 调整滚动条位置（原有代码）
    RECT rcCandRect = { 0,0,0,0 };
    _GetClientRect(&rcCandRect);
    int letf = rcCandRect.right - GetSystemMetrics(SM_CXVSCROLL) * 2 - CANDWND_BORDER_WIDTH;
    int top = rcCandRect.top + CANDWND_BORDER_WIDTH + _contentTopMargin;
    int width = GetSystemMetrics(SM_CXVSCROLL) * 2;
    int height = rcCandRect.bottom - rcCandRect.top - CANDWND_BORDER_WIDTH * 2;
    if (_pVScrollBarWnd) _pVScrollBarWnd->_Resize(letf, top, width, height);

    // 更新圆角（纵向模式圆角半径可能为0，但调用也无妨）
    //if (_GetWnd() && _roundCornerRadius > 0) {
    //    RECT rc;
    //    GetWindowRect(_GetWnd(), &rc);
    //    int w = rc.right - rc.left;
    //    int h = rc.bottom - rc.top;
    //    HRGN hRgn = CreateRoundRectRgn(0, 0, w, h, _roundCornerRadius, _roundCornerRadius);
    //    SetWindowRgn(_GetWnd(), hRgn, TRUE);
    //}
}

//+---------------------------------------------------------------------------
//
// _Move
//
//----------------------------------------------------------------------------

void CCandidateWindow::_Move(int x, int y)
{
    CBaseWindow::_Move(x, y);
}

//+---------------------------------------------------------------------------
//
// _Show
//
//----------------------------------------------------------------------------

void CCandidateWindow::_Show(BOOL isShowWnd)
{

    CBaseWindow::_Show(isShowWnd);
    if (isShowWnd && _GetWnd())
    {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(_GetWnd(), &pt);
        _lastMousePos = pt;
    }
    else
    {
        _lastMousePos.x = -1;
        _lastMousePos.y = -1;
    }
}

//+---------------------------------------------------------------------------
//
// _SetTextColor
// _SetFillColor
//
//----------------------------------------------------------------------------

VOID CCandidateWindow::_SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    _crTextColor = _AdjustTextColor(crColor, crBkColor);
    _crBkColor = crBkColor;
}

VOID CCandidateWindow::_SetFillColor(_In_ HBRUSH hBrush)
{
    _brshBkColor = hBrush;
}

//+---------------------------------------------------------------------------
//
// _WindowProcCallback
//
// Cand window proc.
//----------------------------------------------------------------------------

const int PageCountPosition = 1;
const int StringPosition = 4;

LRESULT CALLBACK CCandidateWindow::_WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HDC dcHandle = nullptr;

        dcHandle = GetDC(wndHandle);
        if (dcHandle)
        {
            HFONT hFontOld = (HFONT)SelectObject(dcHandle, Global::defaultlFontHandle);
            GetTextMetrics(dcHandle, &_TextMetric);

            _cxTitle = _TextMetric.tmMaxCharWidth * _wndWidth;
            SelectObject(dcHandle, hFontOld);
            ReleaseDC(wndHandle, dcHandle);
        }
    }
    return 0;
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(wndHandle, &rcClient);
        // 静态画刷避免频繁创建销毁GDI资源
        static HBRUSH hToolBgBrush = nullptr;
        if (!hToolBgBrush)
            hToolBgBrush = CreateSolidBrush(_backgroundColor);
        else
        {
            // 颜色修改时重建画刷
            COLORREF clr;
            LOGBRUSH lb;
            GetObject(hToolBgBrush, sizeof(lb), &lb);
            clr = lb.lbColor;
            if (clr != _backgroundColor)
            {
                DeleteObject(hToolBgBrush);
                hToolBgBrush = CreateSolidBrush(_backgroundColor);
            }
        }
        FillRect(hdc, &rcClient, hToolBgBrush);
        return TRUE; // 关键：告诉系统背景已全部绘制，不再用系统灰色填充
    }
    case WM_ACTIVATE:
    {
        // wParam == WA_ACTIVE 窗口激活；WA_INACTIVE 失活
        InvalidateRect(wndHandle, NULL, TRUE); // 全部客户区重绘，擦除旧灰色
        UpdateWindow(wndHandle); // 立即执行WM_PAINT+WM_ERASEBKGND填充底色
        break;
    }
    case WM_DESTROY:
        return 0;

    case WM_WINDOWPOSCHANGED:
    {
        WINDOWPOS* pWndPos = (WINDOWPOS*)lParam;

        // move shadow
        // move v-scroll
        if (_pVScrollBarWnd)
        {
            _pVScrollBarWnd->_OnOwnerWndMoved((pWndPos->flags & SWP_NOSIZE) == 0);
        }

        _FireMessageToLightDismiss(wndHandle, pWndPos);
    }
    break;

    case WM_WINDOWPOSCHANGING:
    {
        WINDOWPOS* pWndPos = (WINDOWPOS*)lParam;

        // show/hide shadow
        // show/hide v-scroll
        if (_pVScrollBarWnd)
        {
            if ((pWndPos->flags & SWP_HIDEWINDOW) != 0)
            {
                _pVScrollBarWnd->_Show(FALSE);
            }

            _pVScrollBarWnd->_OnOwnerWndMoved((pWndPos->flags & SWP_NOSIZE) == 0);
        }
    }
    break;

    case WM_SHOWWINDOW:
        // show/hide shadow
        // show/hide v-scroll
        if (_pVScrollBarWnd)
        {
            _pVScrollBarWnd->_Show((BOOL)wParam);
        }
        break;

    case WM_PAINT:
    {
        HDC dcHandle = nullptr;
        PAINTSTRUCT ps;

        dcHandle = BeginPaint(wndHandle, &ps);
        _OnPaint(dcHandle, &ps);
        //_DrawBorder(wndHandle, 2);   // 直接指定 2px 边框
        EndPaint(wndHandle, &ps);
    }
    return 0;

    case WM_SETCURSOR:
    {
        POINT cursorPoint;

        GetCursorPos(&cursorPoint);
        MapWindowPoints(NULL, wndHandle, &cursorPoint, 1);

        // handle mouse message
        _HandleMouseMsg(HIWORD(lParam), cursorPoint);
    }
    return 1;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
        POINT point;

        POINTSTOPOINT(point, MAKEPOINTS(lParam));

        // handle mouse message
        _HandleMouseMsg(uMsg, point);
    }
    // we processes this message, it should return zero. 
    return 0;

    case WM_MOUSEACTIVATE:
    {
        WORD mouseEvent = HIWORD(lParam);
        if (mouseEvent == WM_LBUTTONDOWN ||
            mouseEvent == WM_RBUTTONDOWN ||
            mouseEvent == WM_MBUTTONDOWN)
        {
            return MA_NOACTIVATE;
        }
    }
    break;

    case WM_POINTERACTIVATE:
        return PA_NOACTIVATE;

    case WM_VSCROLL:
    {
        _OnVScroll(LOWORD(wParam), HIWORD(wParam));
        return 0;
    }
    case WM_NCCALCSIZE:
    {
        if (wParam == TRUE)
        {
            NCCALCSIZE_PARAMS* pParams = (NCCALCSIZE_PARAMS*)lParam;
            // 获取原始窗口矩形
            RECT rc = pParams->rgrc[0];
            // 客户区设为整个窗口，但顶部保留 1 像素（边框）
            pParams->rgrc[0] = rc;
            pParams->rgrc[0].top += 1;   // 关键：保留 1px 非客户区，使 DWM 认为有边框
            return WVR_REDRAW;
        }
        break;
    }
    }

    return DefWindowProc(wndHandle, uMsg, wParam, lParam);
}

//+---------------------------------------------------------------------------
//
// _HandleMouseMsg
//
//----------------------------------------------------------------------------

void CCandidateWindow::_HandleMouseMsg(_In_ UINT mouseMsg, _In_ POINT point)
{
    switch (mouseMsg)
    {
    case WM_MOUSEMOVE:
        _OnMouseMove(point);
        break;
    case WM_LBUTTONDOWN:
        _OnLButtonDown(point);
        break;
    case WM_LBUTTONUP:
        _OnLButtonUp(point);
        break;
    }
}

//+---------------------------------------------------------------------------
//
// _OnPaint
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnPaint(_In_ HDC dcHandle, _In_ PAINTSTRUCT *pPaintStruct)
{
    SetBkMode(dcHandle, TRANSPARENT);

    HFONT hFontOld = (HFONT)SelectObject(dcHandle, Global::defaultlFontHandle);

    FillRect(dcHandle, &pPaintStruct->rcPaint, _brshBkColor);
    // 横向模式：先调整窗口宽度以适应内容
    if (_isHorizontalMode)
        _AdjustWindowSizeForHorizontal(dcHandle);

    UINT currentPageIndex = 0;
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        goto cleanup;
    }
    
    _AdjustPageIndex(currentPage, currentPageIndex);

    _DrawList(dcHandle, currentPageIndex, &pPaintStruct->rcPaint);

cleanup:
    SelectObject(dcHandle, hFontOld);
}

//+---------------------------------------------------------------------------
//
// _OnLButtonDown
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnLButtonDown(POINT pt)
{
    if (_isHorizontalMode)
    {
        for (UINT pos = 0; pos < _horizontalItemWidths.GetCount(); pos++)
        {
            RECT rcItem = _GetItemRect(pos);
            if (PtInRect(&rcItem, pt))
            {
                UINT startIdx = *_PageIndex.GetAt(0);   // 当前页起始索引
                _currentSelection = startIdx + pos;
                if (_pfnCallback)
                    _pfnCallback(_pObj, CAND_ITEM_SELECT);
                return;
            }
        }
        return;
    }
    RECT rcWindow = {0, 0, 0, 0};;
    _GetClientRect(&rcWindow);

    int cyLine = _cyRow;
    
    UINT candidateListPageCnt = _pIndexRange->Count();
    UINT index = 0;
    int currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return;
    }

    // Hit test on list items
    index = *_PageIndex.GetAt(currentPage);

    for (UINT pageCount = 0; (index < _candidateList.Count()) && (pageCount < candidateListPageCnt); index++, pageCount++)
    {
        RECT rc = {0, 0, 0, 0};

        rc.left = rcWindow.left;
        rc.right = rcWindow.right - GetSystemMetrics(SM_CXVSCROLL) * 2;
        rc.top = rcWindow.top + (pageCount * cyLine);
        rc.bottom = rcWindow.top + ((pageCount + 1) * cyLine);

        if (PtInRect(&rc, pt) && _pfnCallback)
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            _currentSelection = index;
            _pfnCallback(_pObj, CAND_ITEM_SELECT);
            return;
        }
    }

    SetCursor(LoadCursor(NULL, IDC_ARROW));

    if (_pVScrollBarWnd)
    {
        RECT rc = {0, 0, 0, 0};

        _pVScrollBarWnd->_GetClientRect(&rc);
        MapWindowPoints(_GetWnd(), _pVScrollBarWnd->_GetWnd(), &pt, 1);

        if (PtInRect(&rc, pt))
        {
            _pVScrollBarWnd->_OnLButtonDown(pt);
        }
    }
}

//+---------------------------------------------------------------------------
//
// _OnLButtonUp
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnLButtonUp(POINT pt)
{
    if (nullptr == _pVScrollBarWnd)
    {
        return;
    }

    RECT rc = {0, 0, 0, 0};
    _pVScrollBarWnd->_GetClientRect(&rc);
    MapWindowPoints(_GetWnd(), _pVScrollBarWnd->_GetWnd(), &pt, 1);

    if (_IsCapture())
    {
        _pVScrollBarWnd->_OnLButtonUp(pt);
    }
    else if (PtInRect(&rc, pt))
    {
        _pVScrollBarWnd->_OnLButtonUp(pt);
    }
}

//+---------------------------------------------------------------------------
//
// _OnMouseMove
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnMouseMove(POINT pt)
{
    int dx = pt.x - _lastMousePos.x;
    int dy = pt.y - _lastMousePos.y;
    if (dx * dx + dy * dy < 9)   // 3² = 9
        return;              // 微小移动忽略，不更新参考点，也不触发选中变更
    _lastMousePos = pt;
    // 1. 获取当前页起始索引
    int currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
        return;
    UINT startIdx = *_PageIndex.GetAt(currentPage);

    // 2. 检测鼠标所在候选项
    int newSelection = -1;

    if (_isHorizontalMode)
    {
        // 横向模式：遍历当前页的所有候选项
        size_t count = _horizontalItemWidths.GetCount();
        for (UINT pos = 0; pos < count; pos++)
        {
            RECT rcItem = _GetItemRect(pos);
            if (PtInRect(&rcItem, pt))
            {
                newSelection = startIdx + pos;
                break;
            }
        }
    }
    else
    {
        // 纵向模式：计算每行矩形
        RECT rcWindow;
        _GetClientRect(&rcWindow);
        int cyLine = _cyRow;
        UINT candidateListPageCnt = _pIndexRange->Count();

        for (UINT pageCount = 0; pageCount < candidateListPageCnt; pageCount++)
        {
            RECT rc;
            rc.left = rcWindow.left;
            rc.right = rcWindow.right - GetSystemMetrics(SM_CXVSCROLL) * 2;
            rc.top = rcWindow.top + pageCount * cyLine;
            rc.bottom = rcWindow.top + (pageCount + 1) * cyLine;

            if (PtInRect(&rc, pt))
            {
                UINT idx = startIdx + pageCount;
                if (idx < _candidateList.Count())
                {
                    newSelection = idx;
                    break;
                }
            }
        }
    }

    // 3. 如果找到有效候选项且与当前选中不同，则更新并重绘
    if (newSelection != -1 && newSelection != (int)_currentSelection)
    {
        _currentSelection = newSelection;
        _InvalidateRect();  // 刷新窗口，背景色会随之改变
    }

    // 4. 设置光标（保持原有逻辑）
    RECT rcWindow;
    _GetClientRect(&rcWindow);
    RECT rc = { 0 };
    rc.left = rcWindow.left;
    rc.right = rcWindow.right - GetSystemMetrics(SM_CXVSCROLL) * 2;
    rc.top = rcWindow.top;
    rc.bottom = rcWindow.bottom;
    if (PtInRect(&rc, pt))
        SetCursor(LoadCursor(NULL, IDC_HAND));
    else
        SetCursor(LoadCursor(NULL, IDC_ARROW));
}

//+---------------------------------------------------------------------------
//
// _OnVScroll
//
//----------------------------------------------------------------------------

void CCandidateWindow::_OnVScroll(DWORD dwSB, _In_ DWORD nPos)
{
    switch (dwSB)
    {
    case SB_LINEDOWN:
        _SetSelectionOffset(+1);
        _InvalidateRect();
        break;
    case SB_LINEUP:
        _SetSelectionOffset(-1);
        _InvalidateRect();
        break;
    case SB_PAGEDOWN:
        _MovePage(+1, FALSE);
        _InvalidateRect();
        break;
    case SB_PAGEUP:
        _MovePage(-1, FALSE);
        _InvalidateRect();
        break;
    case SB_THUMBPOSITION:
        _SetSelection(nPos, FALSE);
        _InvalidateRect();
        break;
    }
}

//+---------------------------------------------------------------------------
//
// _DrawList
//
//----------------------------------------------------------------------------

void CCandidateWindow::_DrawList(_In_ HDC dcHandle, _In_ UINT iIndex, _In_ RECT *prc)
{
    int pageCount = 0;
    int candidateListPageCnt = _isHorizontalMode ? (int)_maxHorizontalItems : _pIndexRange->Count();

    int cxLine = _TextMetric.tmAveCharWidth;
    int cyLine = max(_cyRow, _TextMetric.tmHeight);
    int cyOffset = (cyLine == _cyRow ? (cyLine-_TextMetric.tmHeight)/2 : 0);
        const int roundRadius = 6;
	if (_isHorizontalMode)//横向模式的绘制逻辑
    {
        int totalCandCount = static_cast<int>(_candidateList.Count());
        int currentIndex = static_cast<int>(iIndex);
        int remaining = totalCandCount - currentIndex;
        int actualCount = min(candidateListPageCnt, remaining);
        if (actualCount < 0) actualCount = 0;
        int startX = prc->left + _contentLeftMargin;
        int y = prc->top + _contentTopMargin + cyOffset -1;
        // 圆角半径（可根据需求调整，建议8~12px）

        for (int pos = 0; pos < actualCount; pos++)
        {
            UINT idx = iIndex + pos;
            CCandidateListItem* pItem = _candidateList.GetAt(idx);

            WCHAR displayBuf[256] = { 0 };
            _BuildDisplayString(pItem, (_currentSelection == idx), displayBuf, ARRAYSIZE(displayBuf));

            WCHAR numBuf[16];
           // StringCchPrintf(numBuf, 16, L"%d.", (LONG)*_pIndexRange->GetAt(pos));
            StringCchPrintf(numBuf, 16, L"%d", pos + 1);
            SIZE numSize, textSize;
            GetTextExtentPoint32(dcHandle, numBuf, (int)wcslen(numBuf), &numSize);
            GetTextExtentPoint32(dcHandle, displayBuf, (int)wcslen(displayBuf), &textSize);

            // ✅ 统一宽度计算公式
            int itemWidth = numSize.cx + 4 + textSize.cx;  // 不包含 _itemSpacing
			RECT rcItem;//背景绘制区域
            rcItem.left = startX - 3;
            rcItem.right = startX + itemWidth + 6;
            rcItem.top = prc->top + _contentTopMargin ;
            rcItem.bottom = rcItem.top + cyLine - 1;

            // 根据是否选中设置背景色
            if (_currentSelection == idx) {
                // 1. 创建圆角矩形区域
                HRGN hRgn = CreateRoundRectRgn(
                    rcItem.left, rcItem.top,
                    rcItem.right, rcItem.bottom,
                    roundRadius, roundRadius  // 圆角半径（x/y方向）
                );
                if (hRgn)
                {
                    // 2. 创建选中背景色的画刷
                    HBRUSH hBrush = CreateSolidBrush(_selectedBkColor);
                    if (hBrush)
                    {
                        // 3. 填充圆角区域（核心：绘制圆角背景）
                        FillRgn(dcHandle, hRgn, hBrush);
                        DeleteObject(hBrush); // 释放画刷
                    }
                    DeleteObject(hRgn); // 释放区域
                }
                else
                {
                    // 如果创建区域失败，用普通矩形填充
                    HBRUSH hBrush = CreateSolidBrush(_selectedBkColor);
                    FillRect(dcHandle, &rcItem, hBrush);
                    DeleteObject(hBrush);
                }
            }
            else {
                // 非选中项：保持原有背景（也可改为圆角，按需调整）
                SetBkColor(dcHandle, _backgroundColor);
                ExtTextOut(dcHandle, startX, rcItem.top + cyOffset, ETO_OPAQUE, &rcItem, NULL, 0, NULL);
            }

            // 绘制整个词条背景（使用 ETO_OPAQUE 填充）
            //ExtTextOut(dcHandle, startX, rcItem.top + cyOffset, ETO_OPAQUE, &rcItem, NULL, 0, NULL);

            // 绘制序号（黑色文字，背景透明）
            SetBkMode(dcHandle, TRANSPARENT); // 关键：文字背景透明
            SetTextColor(dcHandle, _currentSelection == idx? Global::AdjustColor(Global::candidateSelectedTextColor, 150) : Global::AdjustColor(Global::candidateTextColor,120));
            ExtTextOut(dcHandle, startX, y, 0, &rcItem, numBuf, (int)wcslen(numBuf), NULL);

            // 绘制词条文字（根据选中状态设置文字颜色）
            if (_currentSelection == idx)
            {
                SetTextColor(dcHandle, Global::candidateSelectedTextColor);
            }
            else
            {
                SetTextColor(dcHandle,  Global::candidateTextColor);
            }
            ExtTextOut(dcHandle, startX + numSize.cx + 4, y, 0, &rcItem, displayBuf, (int)wcslen(displayBuf), NULL);

            // 更新下一个词条的起始位置（包含 itemSpacing）
            startX += itemWidth + _itemSpacing;
        }
        return;
    }

        // 原有纵向绘制代码（保持不变）
        // ... 原有 for 循环 ...
    const size_t lenOfPageCount = 16;
    for (; (iIndex < _candidateList.Count()) && (pageCount < candidateListPageCnt); iIndex++, pageCount++)
    {
        WCHAR pageCountString[lenOfPageCount] = { '\0' };
        CCandidateListItem* pItemList = nullptr;

        // 1. 计算整行矩形（从客户区左边界到右边界）
        RECT rcRow;
        rcRow.left = prc->left;
        rcRow.right = prc->right;
        rcRow.top = prc->top + pageCount * cyLine;
        rcRow.bottom = rcRow.top + cyLine;

        // 2. 填充整行背景
        BOOL isSelected = (_currentSelection == iIndex);
        BOOL isAPPRectRgn = FALSE;//是否应用圆角
        if (isSelected) {
            if (isAPPRectRgn) {
                HRGN hRgn = CreateRoundRectRgn(rcRow.left, rcRow.top, rcRow.right, rcRow.bottom, roundRadius, roundRadius);
                if (hRgn) {
                HBRUSH hBrush = CreateSolidBrush(CANDWND_SELECTED_BK_COLOR);
                FillRgn(dcHandle, hRgn, hBrush);
                DeleteObject(hBrush);
                DeleteObject(hRgn);
                }else isAPPRectRgn = FALSE;
            }
            if (!isAPPRectRgn) {
                HBRUSH hBrush = CreateSolidBrush(CANDWND_SELECTED_BK_COLOR);
                FillRect(dcHandle, &rcRow, hBrush);
                DeleteObject(hBrush);
            }
            SetTextColor(dcHandle, CANDWND_SELECTED_ITEM_COLOR);
        }
        else {
            HBRUSH hBrush = CreateSolidBrush(_backgroundColor);
            FillRect(dcHandle, &rcRow, hBrush);
            DeleteObject(hBrush);
            SetTextColor(dcHandle, CANDWND_NUM_COLOR);
        }
        SetBkMode(dcHandle, TRANSPARENT);
        StringCchPrintf(pageCountString, ARRAYSIZE(pageCountString), L"%d", (LONG)*_pIndexRange->GetAt(pageCount));
        ExtTextOut(dcHandle, PageCountPosition * cxLine, pageCount * cyLine + cyOffset, 0, NULL, pageCountString, (UINT)wcslen(pageCountString), NULL);

        // 5. 绘制候选词文字
        // 恢复文字颜色（候选词颜色）
        SetTextColor(dcHandle, isSelected ? _selectedTextColor : _crTextColor);
        if (!_ShowCode) {
            pItemList = _candidateList.GetAt(iIndex);
            ExtTextOut(dcHandle, StringPosition * cxLine, pageCount * cyLine + cyOffset, 0, NULL,
                pItemList->_ItemString.Get(), (DWORD)pItemList->_ItemString.GetLength(), NULL);
        }
        else {
            // 含编码的复杂显示
            pItemList = _candidateList.GetAt(iIndex);
            const WCHAR* pOriginal = pItemList->_ItemString.Get();
            DWORD_PTR originalLen = pItemList->_ItemString.GetLength();

            WCHAR itemBuf[128] = { 0 };
            if (originalLen > 5) {
                StringCchCopyN(itemBuf, 128, pOriginal, 3);
                StringCchCat(itemBuf, 128, L"...");
                WCHAR lastChar[2] = { pOriginal[originalLen - 1], 0 };
                StringCchCat(itemBuf, 128, lastChar);
            }
            else {
                StringCchCopyN(itemBuf, 128, pOriginal, originalLen);
            }

            WCHAR remainBuf[64] = { 0 };
            DWORD_PTR inputLen = 0;
            DWORD_PTR totalKeyLen = pItemList->_FindKeyCode.GetLength();
            if (totalKeyLen > inputLen) {
                const WCHAR* pRemain = pItemList->_FindKeyCode.Get() + inputLen;
                DWORD_PTR remainLen = totalKeyLen - inputLen;
                StringCchCopyN(remainBuf, 64, pRemain, remainLen);
            }

            WCHAR displayBuf[256] = { 0 };
            if (remainBuf[0] != L'\0')
                StringCchPrintf(displayBuf, 256, L"%s [%s]", itemBuf, remainBuf);
            else
                StringCchCopy(displayBuf, 256, itemBuf);

            // 如果是拼音模式且需要显示五笔编码，额外处理（原逻辑保留）
            if (isSelected && Global::isPinyinMode && !Global::isPyAndWbMode && _pWubiEngine) {
                std::wstring code = _pWubiEngine->GetCodeForWord(pOriginal, originalLen);
                if (!code.empty()) {
                    StringCchPrintf(displayBuf, 256, L"%s [%s]", itemBuf, code.c_str());
                }
            }

            ExtTextOut(dcHandle, StringPosition * cxLine, pageCount * cyLine + cyOffset, 0, NULL,
                displayBuf, (UINT)wcslen(displayBuf), NULL);
        }
    }
    // 剩余空行填充（原有代码）
    for (; (pageCount < candidateListPageCnt); pageCount++)
    {
        RECT rcRow;
        rcRow.left = prc->left;
        rcRow.right = prc->right;
        rcRow.top = prc->top + pageCount * cyLine;
        rcRow.bottom = rcRow.top + cyLine;
        FillRect(dcHandle, &rcRow, (HBRUSH)(COLOR_3DHIGHLIGHT + 1));
    }
    
	
}

//+---------------------------------------------------------------------------
//
// _DrawBorder
//
//----------------------------------------------------------------------------
void CCandidateWindow::_DrawBorder(_In_ HWND wndHandle, _In_ int cx)
{
    RECT rcWnd;
    GetWindowRect(wndHandle, &rcWnd);
    OffsetRect(&rcWnd, -rcWnd.left, -rcWnd.top);

    HDC dcHandle = GetWindowDC(wndHandle);

    // 使用实线画笔，颜色不变（或保留 PS_DOT，但实线更清晰）
    HPEN hPen = CreatePen(PS_SOLID, cx, CANDWND_BORDER_COLOR);
    HPEN hPenOld = (HPEN)SelectObject(dcHandle, hPen);
    HBRUSH hBrushOld = (HBRUSH)SelectObject(dcHandle, GetStockObject(NULL_BRUSH)); // 不填充内部

    if (_roundCornerRadius > 0)
    {
        // ✅ 圆角矩形边框，半径与窗口区域一致
        RoundRect(dcHandle, rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom,
            _roundCornerRadius, _roundCornerRadius);
    }
    else
    {
        // 兼容无圆角的情况
        Rectangle(dcHandle, rcWnd.left, rcWnd.top, rcWnd.right, rcWnd.bottom);
    }

    SelectObject(dcHandle, hBrushOld);
    SelectObject(dcHandle, hPenOld);
    DeleteObject(hPen);
    ReleaseDC(wndHandle, dcHandle);
}

//+---------------------------------------------------------------------------
//
// _AddString
//
//----------------------------------------------------------------------------

void CCandidateWindow::_AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode)
{
    DWORD_PTR dwItemString = pCandidateItem->_ItemString.GetLength();
    const WCHAR* pwchString = nullptr;
    if (dwItemString)
    {
        pwchString = new (std::nothrow) WCHAR[ dwItemString ];
        if (!pwchString)
        {
            return;
        }
        memcpy((void*)pwchString, pCandidateItem->_ItemString.Get(), dwItemString * sizeof(WCHAR));
    }

    DWORD_PTR itemWildcard = pCandidateItem->_FindKeyCode.GetLength();
    const WCHAR* pwchWildcard = nullptr;
    if (itemWildcard && isAddFindKeyCode)
    {
        pwchWildcard = new (std::nothrow) WCHAR[ itemWildcard ];
        if (!pwchWildcard)
        {
            if (pwchString)
            {
                delete [] pwchString;
            }
            return;
        }
        memcpy((void*)pwchWildcard, pCandidateItem->_FindKeyCode.Get(), itemWildcard * sizeof(WCHAR));
    }

    CCandidateListItem* pLI = nullptr;
    pLI = _candidateList.Append();
    if (!pLI)
    {
        if (pwchString)
        {
            delete [] pwchString;
            pwchString = nullptr;
        }
        if (pwchWildcard)
        {
            delete [] pwchWildcard;
            pwchWildcard = nullptr;
        }
        return;
    }

    if (pwchString)
    {
        pLI->_ItemString.Set(pwchString, dwItemString);
    }
    if (pwchWildcard)
    {
        pLI->_FindKeyCode.Set(pwchWildcard, itemWildcard);
    }

    return;
}

//+---------------------------------------------------------------------------
//
// _ClearList
//
//----------------------------------------------------------------------------

void CCandidateWindow::_ClearList()
{
    for (UINT index = 0; index < _candidateList.Count(); index++)
    {
        CCandidateListItem* pItemList = nullptr;
        pItemList = _candidateList.GetAt(index);
        delete [] pItemList->_ItemString.Get();
        delete [] pItemList->_FindKeyCode.Get();
    }
    _currentSelection = 0;
    _candidateList.Clear();
    _PageIndex.Clear();
}

//+---------------------------------------------------------------------------
//
// _SetScrollInfo
//
//----------------------------------------------------------------------------

void CCandidateWindow::_SetScrollInfo(_In_ int nMax, _In_ int nPage)
{
    CScrollInfo si;
    si.nMax = nMax;
    si.nPage = nPage;
    si.nPos = 0;

    if (_pVScrollBarWnd)
    {
        _pVScrollBarWnd->_SetScrollInfo(&si);
    }
}

//+---------------------------------------------------------------------------
//
// _GetCandidateString
//
//----------------------------------------------------------------------------

DWORD CCandidateWindow::_GetCandidateString(_In_ int iIndex, _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString)
{
    CCandidateListItem* pItemList = nullptr;

    if (iIndex < 0 )
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    UINT index = static_cast<UINT>(iIndex);
	
	if (index >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    pItemList = _candidateList.GetAt(iIndex);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

//+---------------------------------------------------------------------------
//
// _GetSelectedCandidateString
//
//----------------------------------------------------------------------------

DWORD CCandidateWindow::_GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString)
{
    CCandidateListItem* pItemList = nullptr;

    if (_currentSelection >= _candidateList.Count())
    {
        *ppwchCandidateString = nullptr;
        return 0;
    }

    pItemList = _candidateList.GetAt(_currentSelection);
    if (ppwchCandidateString)
    {
        *ppwchCandidateString = pItemList->_ItemString.Get();
    }
    return (DWORD)pItemList->_ItemString.GetLength();
}

//+---------------------------------------------------------------------------
//
// _SetSelectionInPage
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelectionInPage(int nPos)
{	
    if (nPos < 0)
    {
        return FALSE;
    }

    UINT pos = static_cast<UINT>(nPos);

    if (pos >= _candidateList.Count())
    {
        return FALSE;
    }

    int currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    _currentSelection = *_PageIndex.GetAt(currentPage) + nPos;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _MoveSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_MoveSelection(_In_ int offSet, _In_ BOOL isNotify)
{
    if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    _currentSelection += offSet;

    _dontAdjustOnEmptyItemPage = TRUE;

    if (_pVScrollBarWnd && isNotify)
    {
        _pVScrollBarWnd->_ShiftLine(offSet, isNotify);
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _SetSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelection(_In_ int selectedIndex, _In_ BOOL isNotify)
{
    if (selectedIndex == -1)
    {
        selectedIndex = _candidateList.Count() - 1;
    }

    if (selectedIndex < 0)
    {
        return FALSE;
    }

    int candCnt = static_cast<int>(_candidateList.Count());
    if (selectedIndex >= candCnt)
    {
        return FALSE;
    }

    _currentSelection = static_cast<UINT>(selectedIndex);

    BOOL ret = _AdjustPageIndexForSelection();

    if (_pVScrollBarWnd && isNotify)
    {
        _pVScrollBarWnd->_ShiftPosition(selectedIndex, isNotify);
    }

    return ret;
}

//+---------------------------------------------------------------------------
//
// _SetSelection
//
//----------------------------------------------------------------------------
void CCandidateWindow::_SetSelection(_In_ int nIndex)
{
    _currentSelection = nIndex;
}

//+---------------------------------------------------------------------------
//
// _MovePage
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_MovePage(_In_ int offSet, _In_ BOOL isNotify)
{
    if (offSet == 0)
    {
        return TRUE;
    }

    int currentPage = 0;
    int selectionOffset = 0;
    int newPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return FALSE;
    }

    newPage = currentPage + offSet;
    if ((newPage < 0) || (newPage >= static_cast<int>(_PageIndex.Count())))
    {
        return FALSE;
    }

    // If current selection is at the top of the page AND 
    // we are on the "default" page border, then we don't
    // want adjustment to eliminate empty entries.
    //
    // We do this for keeping behavior inline with downlevel.
    if (_currentSelection % _pIndexRange->Count() == 0 && 
        _currentSelection == *_PageIndex.GetAt(currentPage)) 
    {
        _dontAdjustOnEmptyItemPage = TRUE;
    }

    selectionOffset = _currentSelection - *_PageIndex.GetAt(currentPage);
    _currentSelection = *_PageIndex.GetAt(newPage) + selectionOffset;
    _currentSelection = _candidateList.Count() > _currentSelection ? _currentSelection : _candidateList.Count() - 1;

    // adjust scrollbar position
    if (_pVScrollBarWnd && isNotify)
    {
        _pVScrollBarWnd->_ShiftPage(offSet, isNotify);
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _SetSelectionOffset
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_SetSelectionOffset(_In_ int offSet)
{
	if (_currentSelection + offSet >= _candidateList.Count())
    {
        return FALSE;
    }

    BOOL fCurrentPageHasEmptyItems = FALSE;
    BOOL fAdjustPageIndex = TRUE;

    _CurrentPageHasEmptyItems(&fCurrentPageHasEmptyItems);

    int newOffset = _currentSelection + offSet;

    // For SB_LINEUP and SB_LINEDOWN, we need to special case if CurrentPageHasEmptyItems.
    // CurrentPageHasEmptyItems if we are on the last page.
    if ((offSet == 1 || offSet == -1) &&
        fCurrentPageHasEmptyItems && _PageIndex.Count() > 1)
    {
        int iPageIndex = *_PageIndex.GetAt(_PageIndex.Count() - 1);
        // Moving on the last page and last page has empty items.
        if (newOffset >= iPageIndex)
        {
            fAdjustPageIndex = FALSE;
        }
        // Moving across page border.
        else if (newOffset < iPageIndex)
        {
            fAdjustPageIndex = TRUE;
        }

        _dontAdjustOnEmptyItemPage = TRUE;
    }

    _currentSelection = newOffset;

    if (fAdjustPageIndex)
    {
        return _AdjustPageIndexForSelection();
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _GetPageIndex
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt)
{
    HRESULT hr = S_OK;

    if (uSize > _PageIndex.Count())
    {
        uSize = _PageIndex.Count();
    }
    else
    {
        hr = S_FALSE;
    }

    if (pIndex)
    {
        for (UINT i = 0; i < uSize; i++)
        {
            *pIndex = *_PageIndex.GetAt(i);
            pIndex++;
        }
    }

    *puPageCnt = _PageIndex.Count();

    return hr;
}

//+---------------------------------------------------------------------------
//
// _SetPageIndex
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt)
{
    uPageCnt;

    _PageIndex.Clear();

    for (UINT i = 0; i < uPageCnt; i++)
    {
        UINT *pLastNewPageIndex = _PageIndex.Append();
        if (pLastNewPageIndex != nullptr)
        {
            *pLastNewPageIndex = *pIndex;
            pIndex++;
        }
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _GetCurrentPage
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ UINT *pCurrentPage)
{
    HRESULT hr = S_OK;

    if (pCurrentPage == nullptr)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    *pCurrentPage = 0;

    if (_PageIndex.Count() == 0)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    if (_PageIndex.Count() == 1)
    {
        *pCurrentPage = 0;
         goto Exit;
    }

    UINT i = 0;
    for (i = 1; i < _PageIndex.Count(); i++)
    {
        UINT uPageIndex = *_PageIndex.GetAt(i);

        if (uPageIndex > _currentSelection)
        {
            break;
        }
    }

    *pCurrentPage = i - 1;

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _GetCurrentPage
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_GetCurrentPage(_Inout_ int *pCurrentPage)
{
    HRESULT hr = E_FAIL;
    UINT needCastCurrentPage = 0;
    
    if (nullptr == pCurrentPage)
    {
        goto Exit;
    }

    *pCurrentPage = 0;

    hr = _GetCurrentPage(&needCastCurrentPage);
    if (FAILED(hr))
    {
       goto Exit;
    }

    hr = UIntToInt(needCastCurrentPage, pCurrentPage);
    if (FAILED(hr))
    {
        goto Exit;
    }

Exit:
    return hr;
}

//+---------------------------------------------------------------------------
//
// _AdjustPageIndexForSelection
//
//----------------------------------------------------------------------------

BOOL CCandidateWindow::_AdjustPageIndexForSelection()
{
    UINT candidateListPageCnt = _pIndexRange->Count();
    UINT* pNewPageIndex = nullptr;
    UINT newPageCnt = 0;

    if (_candidateList.Count() < candidateListPageCnt)
    {
        // no needed to restruct page index
        return TRUE;
    }

    // B is number of pages before the current page
    // A is number of pages after the current page
    // uNewPageCount is A + B + 1;
    // A is (uItemsAfter - 1) / candidateListPageCnt + 1 -> 
    //      (_CandidateListCount - _currentSelection - CandidateListPageCount - 1) / candidateListPageCnt + 1->
    //      (_CandidateListCount - _currentSelection - 1) / candidateListPageCnt
    // B is (uItemsBefore - 1) / candidateListPageCnt + 1 ->
    //      (_currentSelection - 1) / candidateListPageCnt + 1
    // A + B is (_CandidateListCount - 2) / candidateListPageCnt + 1

    BOOL isBefore = _currentSelection;
    BOOL isAfter = _candidateList.Count() > _currentSelection + candidateListPageCnt;

    // only have current page
    if (!isBefore && !isAfter) 
    {
        newPageCnt = 1;
    }
    // only have after pages; just count the total number of pages
    else if (!isBefore && isAfter)
    {
        newPageCnt = (_candidateList.Count() - 1) / candidateListPageCnt + 1;
    }
    // we are at the last page
    else if (isBefore && !isAfter)
    {
        newPageCnt = 2 + (_currentSelection - 1) / candidateListPageCnt;
    }
    else if (isBefore && isAfter)
    {
        newPageCnt = (_candidateList.Count() - 2) / candidateListPageCnt + 2;
    }

    pNewPageIndex = new (std::nothrow) UINT[ newPageCnt ];
    if (pNewPageIndex == nullptr)
    {
        return FALSE;
    }
    pNewPageIndex[0] = 0;
    UINT firstPage = _currentSelection % candidateListPageCnt;
    if (firstPage && newPageCnt > 1) 
    {
        pNewPageIndex[1] = firstPage;
    }

    for (UINT i = firstPage ? 2 : 1; i < newPageCnt; ++i)
    {
        pNewPageIndex[i] = pNewPageIndex[i - 1] + candidateListPageCnt;
    }

    _SetPageIndex(pNewPageIndex, newPageCnt);

    delete [] pNewPageIndex;

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _AdjustTextColor
//
//----------------------------------------------------------------------------

COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor)
{
    if (!Global::IsTooSimilar(crColor, crBkColor))
    {
        return crColor;
    }
    else
    {
        return crColor ^ RGB(255, 255, 255);
    }
}

//+---------------------------------------------------------------------------
//
// _CurrentPageHasEmptyItems
//
//----------------------------------------------------------------------------

HRESULT CCandidateWindow::_CurrentPageHasEmptyItems(_Inout_ BOOL *hasEmptyItems)
{
    int candidateListPageCnt = _pIndexRange->Count();
    UINT currentPage = 0;

    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        return S_FALSE;
    }

    if ((currentPage == 0 || currentPage == _PageIndex.Count()-1) &&
        (_PageIndex.Count() > 0) &&
        (*_PageIndex.GetAt(currentPage) > (UINT)(_candidateList.Count() - candidateListPageCnt)))
    {
        *hasEmptyItems = TRUE;
    }
    else 
    {
        *hasEmptyItems = FALSE;
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _FireMessageToLightDismiss
//      fire EVENT_OBJECT_IME_xxx to let LightDismiss know about IME window.
//----------------------------------------------------------------------------

void CCandidateWindow::_FireMessageToLightDismiss(_In_ HWND wndHandle, _In_ WINDOWPOS *pWndPos)
{
    if (nullptr == pWndPos)
    {
        return;
    }

    BOOL isShowWnd = ((pWndPos->flags & SWP_SHOWWINDOW) != 0);
    BOOL isHide = ((pWndPos->flags & SWP_HIDEWINDOW) != 0);
    BOOL needResize = ((pWndPos->flags & SWP_NOSIZE) == 0);
    BOOL needMove = ((pWndPos->flags & SWP_NOMOVE) == 0);
    BOOL needRedraw = ((pWndPos->flags & SWP_NOREDRAW) == 0);

    if (isShowWnd)
    {
        NotifyWinEvent(EVENT_OBJECT_IME_SHOW, wndHandle, OBJID_CLIENT, CHILDID_SELF);
    }
    else if (isHide)
    {
        NotifyWinEvent(EVENT_OBJECT_IME_HIDE, wndHandle, OBJID_CLIENT, CHILDID_SELF);
    }
    else if (needResize || needMove || needRedraw)
    {
        if (IsWindowVisible(wndHandle))
        {
            NotifyWinEvent(EVENT_OBJECT_IME_CHANGE, wndHandle, OBJID_CLIENT, CHILDID_SELF);
        }
    }

}

HRESULT CCandidateWindow::_AdjustPageIndex(_Inout_ UINT & currentPage, _Inout_ UINT & currentPageIndex)
{
    HRESULT hr = E_FAIL;
    UINT candidateListPageCnt = _pIndexRange->Count();

    currentPageIndex = *_PageIndex.GetAt(currentPage);

    BOOL hasEmptyItems = FALSE;
    if (FAILED(_CurrentPageHasEmptyItems(&hasEmptyItems)))
    {
        goto Exit; 
    }

    if (FALSE == hasEmptyItems)
    {
        goto Exit;
    }

    if (TRUE == _dontAdjustOnEmptyItemPage)
    {
        goto Exit;
    }

    UINT tempSelection = _currentSelection;

    // Last page
    UINT candNum = _candidateList.Count();
    UINT pageNum = _PageIndex.Count();

    if ((currentPageIndex > candNum - candidateListPageCnt) && (pageNum > 0) && (currentPage == (pageNum - 1)))
    {
        _currentSelection = candNum - candidateListPageCnt;

        _AdjustPageIndexForSelection();

        _currentSelection = tempSelection;

        if (FAILED(_GetCurrentPage(&currentPage)))
        {
            goto Exit;
        }

        currentPageIndex = *_PageIndex.GetAt(currentPage);
    }
    // First page
    else if ((currentPageIndex < candidateListPageCnt) && (currentPage == 0))
    {
        _currentSelection = 0;

        _AdjustPageIndexForSelection();

        _currentSelection = tempSelection;
    }

    _dontAdjustOnEmptyItemPage = FALSE;
    hr = S_OK;

Exit:
    return hr;
}

void CCandidateWindow::_DeleteVScrollBarWnd()
{
    if (nullptr != _pVScrollBarWnd)
    {
        delete _pVScrollBarWnd;
        _pVScrollBarWnd = nullptr;
    }
}
void CCandidateWindow::_AdjustWindowSizeForHorizontal(HDC dcHandle)
{
    static BOOL s_isAdjusting = FALSE;
    if (s_isAdjusting) return;
    s_isAdjusting = TRUE;

    if (!_isHorizontalMode || _candidateList.Count() == 0 || _PageIndex.Count() == 0)
    {
        s_isAdjusting = FALSE;
        return;
    }
    UINT currentPage = 0;
    if (FAILED(_GetCurrentPage(&currentPage)))
    {
        s_isAdjusting = FALSE;
        return;
    }
    UINT startIdx = *_PageIndex.GetAt(currentPage);

    int maxItemsPerPage = _isHorizontalMode ? (int)_maxHorizontalItems : (int)_pIndexRange->Count();
    int totalCandCount = (int)_candidateList.Count();
    int remaining = totalCandCount - (int)startIdx;
    int actualCount = min(maxItemsPerPage, remaining);
    if (actualCount <= 0)
    {
        s_isAdjusting = FALSE;
        return;
    }
    // 清空并重新填充
    _horizontalItemWidths.RemoveAll();
    int totalWidth = _contentLeftMargin * 2;   // 改用 _contentLeftMargin
    WCHAR numBuf[16];
    WCHAR displayBuf[256];
    SIZE numSize = { 0 }, textSize = { 0 };

    for (int pos = 0; pos < actualCount; pos++)
    {
        UINT idx = startIdx + pos;
        CCandidateListItem* pItem = _candidateList.GetAt(idx);
        if (!pItem) continue;

        _BuildDisplayString(pItem, (pos == 0),displayBuf, ARRAYSIZE(displayBuf));
        StringCchPrintf(numBuf, ARRAYSIZE(numBuf), L"%d.", pos + 1);

        GetTextExtentPoint32(dcHandle, numBuf, (int)wcslen(numBuf), &numSize);
        GetTextExtentPoint32(dcHandle, displayBuf, (int)wcslen(displayBuf), &textSize);

        // 统一宽度计算公式
        int itemWidth = numSize.cx + 4 + textSize.cx + _itemSpacing;
        _horizontalItemWidths.Add(itemWidth);
        totalWidth += itemWidth;
    }
    if(actualCount>4)totalWidth -= 20;
    if (totalWidth != _cxTitle && totalWidth > 0)
    {
        _cxTitle = totalWidth;
        int newHeight = _cyRow + _contentTopMargin * 2;   // 高度加上下边距

        // 先调整大小（不移动位置）
        SetWindowPos(_GetWnd(), NULL, 0, 0, _cxTitle, newHeight,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // 修正位置：确保窗口不超出屏幕边界
        RECT rcWnd;
        GetWindowRect(_GetWnd(), &rcWnd);
        HMONITOR hMonitor = MonitorFromWindow(_GetWnd(), MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hMonitor, &mi);
        RECT rcWork = mi.rcWork;

        int dx = 0;
        if (rcWnd.right > rcWork.right)
            dx = rcWork.right - rcWnd.right;   // 超出右侧，向左移动
        else if (rcWnd.left < rcWork.left)
            dx = rcWork.left - rcWnd.left;     // 超出左侧，向右移动
        RECT rcOldPos;
        GetWindowRect(_GetWnd(), &rcOldPos);
        if (dx != 0)
        {
            SetWindowPos(_GetWnd(), NULL, rcWnd.left + dx, rcWnd.top,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        _ResizeWindow();
        SetWindowPos(_GetWnd(), NULL, rcOldPos.left, rcOldPos.top, _cxTitle, newHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(_GetWnd(), NULL, TRUE);
    }

    s_isAdjusting = FALSE;
}

RECT CCandidateWindow::_GetItemRect(int pos)
{
    RECT rc = { 0,0,0,0 };
    if (!_isHorizontalMode || pos < 0 || pos >= (int)_horizontalItemWidths.GetCount()) return rc;
    int left = _contentLeftMargin;   // 原为 _horizontalLeftMargin
    for (int i = 0; i < pos; i++) left += _horizontalItemWidths[i];
    rc.left = left;
    rc.right = left + _horizontalItemWidths[pos];
    rc.top = _contentTopMargin;
    rc.bottom = rc.top + _cyRow;
    return rc;
}

void CCandidateWindow::_BuildDisplayString(CCandidateListItem* pItem, BOOL isSelected, _Out_writes_z_(256) WCHAR* pOutBuf, size_t bufSize)
{
    if (!pItem || !pOutBuf || bufSize == 0)
    {
        return;
    }

    pOutBuf[0] = L'\0';

    const WCHAR* pOriginal = pItem->_ItemString.Get();
    DWORD_PTR originalLen = pItem->_ItemString.GetLength();

    // 1. 词条缩写（长度按字符数，中英文均视为1个WCHAR）
    WCHAR itemBuf[128] = { 0 };
    if (originalLen > 5)
    {
        StringCchCopyN(itemBuf, ARRAYSIZE(itemBuf), pOriginal, 3);
        StringCchCat(itemBuf, ARRAYSIZE(itemBuf), L"...");
        WCHAR lastChar[2] = { pOriginal[originalLen - 1], 0 };
        StringCchCat(itemBuf, ARRAYSIZE(itemBuf), lastChar);
    }
    else
    {
        StringCchCopyN(itemBuf, ARRAYSIZE(itemBuf), pOriginal, originalLen);
    }
    // 2. 判断是否显示五笔编码（选中 + 拼音模式且非混合模式 + 有引擎）
    BOOL showWubi = isSelected && Global::isPinyinMode && _pWubiEngine && Global::showRemainingCode;
    if (showWubi)
    {
        std::wstring code = _pWubiEngine->GetCodeForWord(pOriginal, originalLen);
        if (!code.empty())
        {
            StringCchPrintf(pOutBuf, bufSize, L"%s [%s]", itemBuf, code.c_str());
            return;
        }
    }
    // 2. 剩余编码
    WCHAR remainBuf[64] = { 0 };
    DWORD_PTR inputLen = 0;   // 这里可以根据需要传入已输入的编码长度，示例中为0
    DWORD_PTR totalKeyLen = pItem->_FindKeyCode.GetLength();
    if (totalKeyLen > inputLen)
    {
        const WCHAR* pRemain = pItem->_FindKeyCode.Get() + inputLen;
        DWORD_PTR remainLen = totalKeyLen - inputLen;
        StringCchCopyN(remainBuf, ARRAYSIZE(remainBuf), pRemain, remainLen);
    }

    // 3. 构建最终显示字符串：“词条 [剩余编码]” 或仅“词条”
    if (remainBuf[0] != L'\0' && Global::showRemainingCode)
    {
        StringCchPrintf(pOutBuf, bufSize, L"%s [%s]", itemBuf, remainBuf);
    }
    else
    {
        StringCchCopy(pOutBuf, bufSize, itemBuf);
    }
}
void CCandidateWindow::SetHorizontalMode(BOOL isHorizontal, UINT maxItems, int spacing)
{
    _isHorizontalMode = isHorizontal;
    _maxHorizontalItems = maxItems;
    _itemSpacing = spacing;

    // ---------- 根据模式设置所有样式参数 ----------
    if (_isHorizontalMode) {
        // 横向（五笔）样式
        _backgroundColor = Global::candidateBgColor;//RGB(245, 255, 236);
        _selectedBkColor = Global::candidateSelectedBgColor;//RGB(0, 120, 215);
        _selectedTextColor = Global::candidateSelectedTextColor;//RGB(255, 255, 255);
        _roundCornerRadius = 12;
        _contentLeftMargin = 12;
        _contentTopMargin = 4;
    }
    else {
        // 纵向（拼音）样式
        _backgroundColor = GetSysColor(COLOR_WINDOW);
        _selectedBkColor = RGB(104, 148, 0);
        _selectedTextColor = RGB(255, 255, 255);
        _roundCornerRadius = 16;
        _contentLeftMargin = 0;
        _contentTopMargin = 0;
    }

    // 重建背景画刷
    if (_brshBkColor) DeleteObject(_brshBkColor);
    _brshBkColor = CreateSolidBrush(_backgroundColor);
    if (_pIndexRange)
    {
        _pIndexRange->Clear();
        UINT count = _isHorizontalMode ? _maxHorizontalItems : (Global::isPinyinMode ? Global::nMaxVerticalItems : Global::nMaxHorizontalItems);
        for (DWORD i = 1; i <= count; i++)
        {
            DWORD* pNewIndexRange = _pIndexRange->Append();
            if (pNewIndexRange != nullptr)
            {
                if (i == count && count == 10)
                {
                    *pNewIndexRange = 0;
                }
                else
                {
                    *pNewIndexRange = i;   // 最后一条映射到数字0（通常显示为10）
                }
            }
        }
    }
    // 如果窗口已创建，则立即应用新样式（包括圆角、重绘）
    HWND hWnd = _GetWnd();
    if (hWnd && IsWindow(hWnd)) {
        // 2. 重新调整窗口大小（因为边距和每页项数可能变化）
        _ResizeWindow();

        // 3. 强制重绘
        InvalidateRect(hWnd, NULL, TRUE);
        UpdateWindow(hWnd);
    }

}

