// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "BaseWindow.h"
#define GETRGBALPHA(_x_, _y_) ((RGBALPHA *)pDIBits + (_y_) * size.cx + (_x_))
class CShadowWindow : public CBaseWindow
{
public:
    CShadowWindow(_In_ CBaseWindow *pWndOwner)
    {
        _pWndOwner = pWndOwner;
        _color = RGB(0, 0, 0);
        _sizeShift.cx = 0;
        _sizeShift.cy = 0;
        _isGradient = FALSE;
        _roundCornerRadius = 0;   // 新增
    }
    virtual ~CShadowWindow()
    {
    }
    void SetRoundCornerRadius(int radius) { _roundCornerRadius = radius; } // 新增
    BOOL _Create(ATOM atom, DWORD dwExStyle, DWORD dwStyle, _In_opt_ CBaseWindow *pParent = nullptr, int wndWidth = 0, int wndHeight = 0);

    void _Show(BOOL isShowWnd);

    LRESULT CALLBACK _WindowProcCallback(_In_ HWND wndHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void _OnSettingChange();
    void _OnOwnerWndMoved(BOOL isResized);
    void _InitShadow();
private:
    BOOL _Initialize();
    void _InitSettings();
    void _AdjustWindowPos();


private:
    CBaseWindow* _pWndOwner;
    COLORREF _color;
    SIZE _sizeShift;
    BOOL _isGradient;
    int _roundCornerRadius;   // 新增：圆角半径（像素）
};
