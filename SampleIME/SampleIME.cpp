// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "globals.h"
#include "SampleIME.h"
#include "CandidateListUIPresenter.h"
#include "CompositionProcessorEngine.h"
#include "Compartment.h"

//+---------------------------------------------------------------------------
//
// CreateInstance
//
//----------------------------------------------------------------------------

/* static */
HRESULT CSampleIME::CreateInstance(_In_ IUnknown *pUnkOuter, REFIID riid, _Outptr_ void **ppvObj)
{
    CSampleIME* pSampleIME = nullptr;
    HRESULT hr = S_OK;

    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (nullptr != pUnkOuter)
    {
        return CLASS_E_NOAGGREGATION;
    }

    pSampleIME = new (std::nothrow) CSampleIME();
    if (pSampleIME == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    hr = pSampleIME->QueryInterface(riid, ppvObj);

    pSampleIME->Release();

    return hr;
}

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CSampleIME::CSampleIME()
{
    DllAddRef();

    _pThreadMgr = nullptr;

    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;

    _pTextEditSinkContext = nullptr;
    _textEditSinkCookie = TF_INVALID_COOKIE;

    _activeLanguageProfileNotifySinkCookie = TF_INVALID_COOKIE;

    _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;

    _pComposition = nullptr;

    _pCompositionProcessorEngine = nullptr;

    _candidateMode = CANDIDATE_NONE;
    _pCandidateListUIPresenter = nullptr;
    _isCandidateWithWildcard = FALSE;

    _pDocMgrLastFocused = nullptr;

    _pSIPIMEOnOffCompartment = nullptr;
    _dwSIPIMEOnOffCompartmentSinkCookie = 0;
    _msgWndHandle = nullptr;

    _pContext = nullptr;
    _fWaitForPush = FALSE;
    _refCount = 1;
	_fBlockNewInput = FALSE;
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CSampleIME::~CSampleIME()
{
    if (_pCandidateListUIPresenter)
    {
        delete _pCandidateListUIPresenter;
        _pCandidateListUIPresenter = nullptr;
    }
    DllRelease();
}

//+---------------------------------------------------------------------------
//
// QueryInterface
//
//----------------------------------------------------------------------------

STDAPI CSampleIME::QueryInterface(REFIID riid, _Outptr_ void **ppvObj)
{
    if (ppvObj == nullptr)
    {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor))
    {
        *ppvObj = (ITfTextInputProcessor *)this;
    }
    else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    {
        *ppvObj = (ITfTextInputProcessorEx *)this;
    }
    else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    {
        *ppvObj = (ITfThreadMgrEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfTextEditSink))
    {
        *ppvObj = (ITfTextEditSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    {
        *ppvObj = (ITfKeyEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfActiveLanguageProfileNotifySink))
    {
        *ppvObj = (ITfActiveLanguageProfileNotifySink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink))
    {
        *ppvObj = (ITfKeyEventSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    {
        *ppvObj = (ITfDisplayAttributeProvider *)this;
    }
    else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    {
        *ppvObj = (ITfThreadFocusSink *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFunctionProvider))
    {
        *ppvObj = (ITfFunctionProvider *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFunction))
    {
        *ppvObj = (ITfFunction *)this;
    }
    else if (IsEqualIID(riid, IID_ITfFnGetPreferredTouchKeyboardLayout))
    {
        *ppvObj = (ITfFnGetPreferredTouchKeyboardLayout *)this;
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
// AddRef
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CSampleIME::AddRef()
{
    return ++_refCount;
}

//+---------------------------------------------------------------------------
//
// Release
//
//----------------------------------------------------------------------------

STDAPI_(ULONG) CSampleIME::Release()
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
// ITfTextInputProcessorEx::ActivateEx
//
//----------------------------------------------------------------------------
STDAPI CSampleIME::ActivateEx(ITfThreadMgr *pThreadMgr, TfClientId tfClientId, DWORD dwFlags)
{
    OutputDebugString(L"0011CSampleIME::ActivateEx---------------``````````````````````````````````-T ");
    _pThreadMgr = pThreadMgr;
    _pThreadMgr->AddRef();
    if (!m_bStickySet) {//禁用连续按5次 Shift 键弹出粘滞键对话框
        Global::ActivateStickyHotkey(false, &m_bStickyOriginal);
        m_bStickySet = TRUE;
    }
    _tfClientId = tfClientId;
    _dwActivateFlags = dwFlags;
    //CCompositionProcessorEngine::LoadConfig();
    BOOL desiredState = Global::isChineseMode;
    //CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    //CompartmentKeyboardOpen._SetCompartmentBOOL(desiredState);   // 默认中文输入模式

    //// 新增：设置转换模式，包含 NATIVE（中文模式）和 SYMBOL（中文标点）等
    //CCompartment CompartmentConversion(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
    //DWORD dwConvMode = TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_SYMBOL;  // 可根据需要加 FULLSHAPE 等
    //CompartmentConversion._SetCompartmentDWORD(dwConvMode);

    CCompartment CompartmentDoubleSingleByte(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._SetCompartmentBOOL(FALSE);// 默认半角

    CCompartment CompartmentPunctuation(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    CompartmentPunctuation._SetCompartmentBOOL(desiredState);// 默认中文标点
    if (!_InitThreadMgrEventSink())
    {
        goto ExitError;
    }

    ITfDocumentMgr* pDocMgrFocus = nullptr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgrFocus)) && (pDocMgrFocus != nullptr))
    {
        _InitTextEditSink(pDocMgrFocus);
        pDocMgrFocus->Release();
    }

    if (!_InitKeyEventSink())
    {
        goto ExitError;
    }

    if (!_InitActiveLanguageProfileNotifySink())
    {
        goto ExitError;
    }

    if (!_InitThreadFocusSink())
    {
        goto ExitError;
    }

    if (!_InitDisplayAttributeGuidAtom())
    {
        goto ExitError;
    }

    if (!_InitFunctionProviderSink())
    {
        goto ExitError;
    }

    if (!_AddTextProcessorEngine())
    {
        goto ExitError;
    }

    return S_OK;

ExitError:
    Deactivate();
    return E_FAIL;
}

//+---------------------------------------------------------------------------
//
// ITfTextInputProcessorEx::Deactivate
//
//----------------------------------------------------------------------------

STDAPI CSampleIME::Deactivate()
{
    if (m_bStickySet) {
        Global::ActivateStickyHotkey(m_bStickyOriginal, nullptr);  // 恢复原来的启用状态
        m_bStickySet = FALSE;
    }
    if (Global::hToolBarWnd && ::IsWindow(Global::hToolBarWnd))
    {
        ::PostMessage(Global::hToolBarWnd, WM_CLOSE, 0, 0);
        Global::hToolBarWnd = NULL;
    }
    if (_pCompositionProcessorEngine)
    {
        delete _pCompositionProcessorEngine;
        _pCompositionProcessorEngine = nullptr;
    }

    ITfContext* pContext = _pContext;
    if (_pContext)
    {   
        pContext->AddRef();
        _EndComposition(_pContext);
    }

    if (_pCandidateListUIPresenter)
    {
        delete _pCandidateListUIPresenter;
        _pCandidateListUIPresenter = nullptr;

        if (pContext)
        {
            pContext->Release();
        }

        _candidateMode = CANDIDATE_NONE;
        _isCandidateWithWildcard = FALSE;
    }

    _UninitFunctionProviderSink();

    _UninitThreadFocusSink();

    _UninitActiveLanguageProfileNotifySink();

    _UninitKeyEventSink();

    _UninitThreadMgrEventSink();

    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._ClearCompartment();

    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._ClearCompartment();

    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    CompartmentDoubleSingleByte._ClearCompartment();

    if (_pThreadMgr != nullptr)
    {
        _pThreadMgr->Release();
    }

    _tfClientId = TF_CLIENTID_NULL;

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->Release();
		_pDocMgrLastFocused = nullptr;
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::GetType
//
//----------------------------------------------------------------------------
HRESULT CSampleIME::GetType(__RPC__out GUID *pguid)
{
    HRESULT hr = E_INVALIDARG;
    if (pguid)
    {
        *pguid = Global::SampleIMECLSID;
        hr = S_OK;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::::GetDescription
//
//----------------------------------------------------------------------------
HRESULT CSampleIME::GetDescription(__RPC__deref_out_opt BSTR *pbstrDesc)
{
    HRESULT hr = E_INVALIDARG;
    if (pbstrDesc != nullptr)
    {
        *pbstrDesc = nullptr;
        hr = E_NOTIMPL;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunctionProvider::::GetFunction
//
//----------------------------------------------------------------------------
HRESULT CSampleIME::GetFunction(__RPC__in REFGUID rguid, __RPC__in REFIID riid, __RPC__deref_out_opt IUnknown **ppunk)
{
    HRESULT hr = E_NOINTERFACE;

    if ((IsEqualGUID(rguid, GUID_NULL)) 
        && (IsEqualGUID(riid, __uuidof(ITfFnSearchCandidateProvider))))
    {
        hr = _pITfFnSearchCandidateProvider->QueryInterface(riid, (void**)ppunk);
    }
    else if (IsEqualGUID(rguid, GUID_NULL))
    {
        hr = QueryInterface(riid, (void **)ppunk);
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFunction::GetDisplayName
//
//----------------------------------------------------------------------------
HRESULT CSampleIME::GetDisplayName(_Out_ BSTR *pbstrDisplayName)
{
    HRESULT hr = E_INVALIDARG;
    if (pbstrDisplayName != nullptr)
    {
        *pbstrDisplayName = nullptr;
        hr = E_NOTIMPL;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// ITfFnGetPreferredTouchKeyboardLayout::GetLayout
// The tkblayout will be Optimized layout.
//----------------------------------------------------------------------------
HRESULT CSampleIME::GetLayout(_Out_ TKBLayoutType *ptkblayoutType, _Out_ WORD *pwPreferredLayoutId)
{
    HRESULT hr = E_INVALIDARG;
    if ((ptkblayoutType != nullptr) && (pwPreferredLayoutId != nullptr))
    {
        *ptkblayoutType = TKBLT_OPTIMIZED;
        *pwPreferredLayoutId = TKBL_OPT_SIMPLIFIED_CHINESE_PINYIN;
        hr = S_OK;
    }
    return hr;
}
BOOL CSampleIME::_AddUserWord(LPCWSTR pszCode, LPCWSTR pszWord)
{
    if (_pCompositionProcessorEngine)
        return _pCompositionProcessorEngine->AddUserWord(pszCode, pszWord);
    return FALSE;
}
// SampleIME.cpp
void CSampleIME::UpdateRecentHanzi(const CStringRange* pText) {
    if (!pText || pText->GetLength() == 0) return;
    const WCHAR* pStr = pText->Get();
    DWORD_PTR len = pText->GetLength();

    // 调试输出（可选）
    //WCHAR szMsg[512] = { 0 };
    //size_t copyLen = min(len, (DWORD_PTR)511);
    //wcsncpy_s(szMsg, _countof(szMsg), pStr, copyLen);
    //szMsg[copyLen] = L'\0';
    //OutputDebugString(L"UpdateRecentHanzi 接收到的词语 ：received: ");
    //OutputDebugString(szMsg);
    //OutputDebugString(L"\n");

    // 1. 构建临时缓冲区：当前 m_recentHanzi + 接收到的字符串
    WCHAR temp[1024] = { 0 };
    int tempLen = 0;
    const int MAX_RECENT_HANZI = 15;
    // 添加现有缓存（最多 MAX_RECENT_HANZI 个）
    for (int i = 0; i < MAX_RECENT_HANZI &&  Global::m_recentHanzi[i] != L'\0'; i++) {
        temp[tempLen++] = Global::m_recentHanzi[i];
    }

    // 添加接收到的字符串中的所有字符（允许非汉字）
    for (DWORD_PTR i = 0; i < len; i++) {
        if (tempLen < 1023) temp[tempLen++] = pStr[i];
    }
    temp[tempLen] = L'\0';

    // 2. 从右向左提取最后 MAX_RECENT_HANZI 个汉字
    WCHAR newRecent[MAX_RECENT_HANZI + 1] = { 0 };
    int idx = 0;
    for (int i = tempLen - 1; i >= 0 && idx < MAX_RECENT_HANZI; i--) {
        WCHAR ch = temp[i];
        if (ch >= 0x4E00 && ch <= 0x9FFF) {  // 汉字基本区
            newRecent[idx++] = ch;
        }
    }

    // 反转顺序（因为是从右向左收集的，现在变为正序）
    if (idx > 1) {
        for (int i = 0; i < idx / 2; i++) {
            WCHAR tmp = newRecent[i];
            newRecent[i] = newRecent[idx - 1 - i];
            newRecent[idx - 1 - i] = tmp;
        }
    }
    newRecent[idx] = L'\0';

    // 3. 保存到 m_recentHanzi
    wcscpy_s(Global::m_recentHanzi, MAX_RECENT_HANZI + 1, newRecent);
    // 调试输出最终结果
    //if ( Global::m_recentHanzi[0] != L'\0') {
    //    WCHAR debugMsg[128] = { 0 };

    //}

}

// SampleIME.cpp 顶部（匿名命名空间内）添加：
namespace {
    // 与 KeyEventSink.cpp 中完全相同的类，但为了复用，可以将其声明放到 Private.h 中，此处为了演示直接复制一份（注意避免重复定义，但不同编译单元可各自定义）
    class CCloseCandidateAndCommitEditSession : public CEditSessionBase
    {
    public:
        CCloseCandidateAndCommitEditSession(CSampleIME* pTextService, ITfContext* pContext)
            : CEditSessionBase(pTextService, pContext) {
        }

        STDMETHODIMP DoEditSession(TfEditCookie ec) override
        {
            _pTextService->_HandleComplete(ec, _pContext);
            return S_OK;
        }
    };
}

void CSampleIME::CloseCandidateAndCommit()
{
    if (!_pThreadMgr) return;

    ITfDocumentMgr* pDocMgr = nullptr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr)
        return;

    ITfContext* pContext = nullptr;
    if (FAILED(pDocMgr->GetTop(&pContext)) || !pContext)
    {
        pDocMgr->Release();
        return;
    }

    // 创建 EditSession 并请求异步执行
    CCloseCandidateAndCommitEditSession* pEditSession =
        new CCloseCandidateAndCommitEditSession(this, pContext);
    if (pEditSession)
    {
        HRESULT hr = S_OK;
        pContext->RequestEditSession(_tfClientId, pEditSession,
            TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        pEditSession->Release();
    }

    pContext->Release();
    pDocMgr->Release();
}