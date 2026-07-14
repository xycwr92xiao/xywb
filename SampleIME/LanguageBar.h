// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once
#include <windows.h>
#include <commctrl.h>
#include "Private.h"
#include "Compartment.h"
// 菜单项 ID 定义
#define ID_MENU_INPUT_MODE      1001
#define ID_MENU_FULL_WIDTH      1002
#define ID_MENU_PUNCTUATION     1003
#define ID_MENU_EMOJI           1004
#define ID_MENU_SETTINGS        1005
#define ID_MENU_USER_WORD       1006
#define ID_MENU_HELP            1007
#define ID_MENU_RELOAD_DICT     1008   // 重新加载词库
#define ID_MENU_EDITWORDS       1009   // 
#define ID_MENU_SHOWTOOL        1010   //
#define ID_MENU_CHANGEHV        1011   //

class CToolbarWindow;
class CCompartment;
class CCompartmentEventSink;

class CLangBarItemButton : public ITfLangBarItemButton,
    public ITfSource
{
public:
    CLangBarItemButton(REFGUID guidLangBar, LPCWSTR description, LPCWSTR tooltip, DWORD onIconIndex, DWORD offIconIndex, DWORD onIconIndexPinyin, DWORD onIconIndexPinyinWubi, BOOL isSecureMode, CSampleIME* pIME);   // 新增参数
    ~CLangBarItemButton();
    void RefreshIcon();
	void RefreshToolBarIcon();
    void _ShowUserWordDialog();  // 手工造词
    void    _ShowEmojiDialog();     // 表情符号
    void    _ShowSettingsDialog();  // 设置
    void    _EditUserWords();  // 
    void     SetToolbarVisible(BOOL bShow);
    void RecreateToolbar();
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // ITfLangBarItem
    STDMETHODIMP GetInfo(_Out_ TF_LANGBARITEMINFO *pInfo);
    STDMETHODIMP GetStatus(_Out_ DWORD *pdwStatus);
    STDMETHODIMP Show(BOOL fShow);
    STDMETHODIMP GetTooltipString(_Out_ BSTR *pbstrToolTip);

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, _In_ const RECT *prcArea);
    STDMETHODIMP InitMenu(_In_ ITfMenu *pMenu);
    STDMETHODIMP OnMenuSelect(UINT wID);
    STDMETHODIMP GetIcon(_Out_ HICON *phIcon);
    STDMETHODIMP GetText(_Out_ BSTR *pbstrText);

    // ITfSource
    STDMETHODIMP AdviseSink(__RPC__in REFIID riid, __RPC__in_opt IUnknown *punk, __RPC__out DWORD *pdwCookie);
    STDMETHODIMP UnadviseSink(DWORD dwCookie);

    // Add/Remove languagebar item
    HRESULT _AddItem(_In_ ITfThreadMgr *pThreadMgr);
    HRESULT _RemoveItem(_In_ ITfThreadMgr *pThreadMgr);

    // Register compartment for button On/Off switch
    BOOL _RegisterCompartment(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, REFGUID guidCompartment);
    BOOL _UnregisterCompartment(_In_ ITfThreadMgr *pThreadMgr);

    void CleanUp();

    void SetStatus(DWORD dwStatus, BOOL fSet);
    // 新增右键菜单相关

private:
    ITfLangBarItemSink* _pLangBarItemSink;
    CToolbarWindow* _pToolbar = nullptr;  // 新增
    TF_LANGBARITEMINFO _tfLangBarItemInfo;
    LPCWSTR _pTooltipText;
    DWORD _onIconIndex;
    DWORD _offIconIndex;
    DWORD _onIconIndexPinyin;     // 拼音模式下的中文图标索引
    DWORD _onIconIndexPinyinWubi;   // 混合模式（拼音+五笔）中文图标

    BOOL _isAddedToLanguageBar;
    BOOL _isSecureMode;
    DWORD _status;

    CCompartment* _pCompartment;
    CCompartmentEventSink* _pCompartmentEventSink;
    static HRESULT _CompartmentCallback(_In_ void *pv, REFGUID guidCompartment);

    // The cookie for the sink to CLangBarItemButton.
    static const DWORD _cookie = 0;
    LONG _refCount;
    ITfThreadMgr* _pThreadMgr;
    TfClientId    _tfClientId;
    HMENU   _hMenu;                 // 右键菜单句柄

    void    _CreatePopupMenu();     // 创建菜单
    void    _UpdateMenuItems();     // 更新菜单项状态（勾选/文本）
    void    _ShowHelpDialog();      // 帮助
    CSampleIME* _pSampleIME;   // 新增成员
    static BOOL s_bRecreating;   // 静态标志
};
