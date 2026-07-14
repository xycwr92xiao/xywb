// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved


#pragma once

#include "private.h"
#include "BaseWindow.h"
#include "ScrollBarWindow.h"
#include "SampleIMEBaseStructure.h"
#include <atlcoll.h>
#include "TableDictionaryEngine.h"   // 只需要这个

enum CANDWND_ACTION
{
    CAND_ITEM_SELECT
};

typedef HRESULT (*CANDWNDCALLBACK)(void *pv, enum CANDWND_ACTION action);

class CCompositionProcessorEngine;   // 前置声明

class CCandidateWindow : public CBaseWindow
{
    
public:
    CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void *pv, _In_ CCandidateRange *pIndexRange, _In_ BOOL isStoreAppMode, _In_opt_ CTableDictionaryEngine* pWubiEngine = nullptr);
    virtual ~CCandidateWindow();
    bool _ShowCode;//是否显示编码
    BOOL _Create(ATOM atom, _In_ UINT wndWidth, _In_opt_ HWND parentWndHandle);

    void _Move(int x, int y);
    void _Show(BOOL isShowWnd);

    VOID _SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);
    VOID _SetFillColor(_In_ HBRUSH hBrush);

    LRESULT CALLBACK _WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);
    void _OnPaint(_In_ HDC dcHandle, _In_ PAINTSTRUCT *pps);
    void _OnLButtonDown(POINT pt);
    void _OnLButtonUp(POINT pt);
    void _OnMouseMove(POINT pt);
    void _OnVScroll(DWORD dwSB, _In_ DWORD nPos);

    void _AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode);
    void _ClearList();
    UINT _GetCount()
    {
        return _candidateList.Count();
    }
    UINT _GetSelection()
    {
        return _currentSelection;
    }
    void _SetScrollInfo(_In_ int nMax, _In_ int nPage);

    DWORD _GetCandidateString(_In_ int iIndex, _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString);
    DWORD _GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString);

    BOOL _MoveSelection(_In_ int offSet, _In_ BOOL isNotify);
    BOOL _SetSelection(_In_ int iPage, _In_ BOOL isNotify);
    void _SetSelection(_In_ int nIndex);
    BOOL _MovePage(_In_ int offSet, _In_ BOOL isNotify);
    BOOL _SetSelectionInPage(int nPos);

    HRESULT _GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt);
    HRESULT _SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt);
    HRESULT _GetCurrentPage(_Inout_ UINT *pCurrentPage);
    HRESULT _GetCurrentPage(_Inout_ int *pCurrentPage);
    void SetHorizontalMode(BOOL isHorizontal, UINT maxItems = 10, int spacing = 8);
    BOOL IsHorizontalMode() const { return _isHorizontalMode; }
private:
    CTableDictionaryEngine* _pWubiEngine;   // 仅用于拼音模式查询五笔编码
    BOOL _isHorizontalMode;          // FALSE=纵向(默认)，TRUE=横向
    UINT _maxHorizontalItems;        // 横向最多显示词条数，默认10
    int _itemSpacing;                // 词条之间的水平间距（像素）
    void _AdjustWindowSizeForHorizontal(HDC dcHandle);
    CAtlArray<int> _horizontalItemWidths;   // 存储横向模式下每个词条的宽度（像素）
    // 可选：缓存左边距，默认为 2 像素
    int _horizontalLeftMargin =2;
    RECT CCandidateWindow::_GetItemRect(int pos);
    void _BuildDisplayString(CCandidateListItem* pItem, BOOL isSelected, _Out_writes_z_(256) WCHAR* pOutBuf, size_t bufSize);
    void _HandleMouseMsg(_In_ UINT mouseMsg, _In_ POINT point);
    void _DrawList(_In_ HDC dcHandle, _In_ UINT iIndex, _In_ RECT *prc);
    void _DrawBorder(_In_ HWND wndHandle, _In_ int cx);
    BOOL _SetSelectionOffset(_In_ int offSet);
    BOOL _AdjustPageIndexForSelection();
    HRESULT _CurrentPageHasEmptyItems(_Inout_ BOOL *pfHasEmptyItems);

	// LightDismiss feature support, it will fire messages lightdismiss-related to the light dismiss layout.
    void _FireMessageToLightDismiss(_In_ HWND wndHandle, _In_ WINDOWPOS *pWndPos);

    BOOL _CreateMainWindow(ATOM atom, _In_opt_ HWND parentWndHandle);
    BOOL _CreateVScrollWindow();

    HRESULT _AdjustPageIndex(_Inout_ UINT & currentPage, _Inout_ UINT & currentPageIndex);

    void _ResizeWindow();
    void _DeleteVScrollBarWnd();
    friend COLORREF _AdjustTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);

private:
    UINT _currentSelection;
    CSampleImeArray<CCandidateListItem> _candidateList;
    CSampleImeArray<UINT> _PageIndex;

    COLORREF _crTextColor;
    COLORREF _crBkColor;
    HBRUSH _brshBkColor;

    TEXTMETRIC _TextMetric;
    int _cyRow;
    int _cxTitle;
    UINT _wndWidth;

    CCandidateRange* _pIndexRange;

    CANDWNDCALLBACK _pfnCallback;
    void* _pObj;

    CScrollBarWindow* _pVScrollBarWnd;

    BOOL _dontAdjustOnEmptyItemPage;
    BOOL _isStoreAppMode;
    // 样式相关成员（由 SetHorizontalMode 统一维护）
    COLORREF _backgroundColor;          // 窗口背景色
    COLORREF _selectedBkColor;          // 选中项背景色
    COLORREF _selectedTextColor;        // 选中项文字颜色
    int      _roundCornerRadius;        // 圆角半径（像素），0 表示无圆角
    int      _contentLeftMargin;        // 内容左边距（像素）
    int      _contentTopMargin;         // 内容上边距（像素）
    POINT _lastMousePos;   // 记录上一次鼠标位置（客户区坐标）
};
