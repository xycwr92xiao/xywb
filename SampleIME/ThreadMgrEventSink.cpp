// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "Globals.h"
#include "SampleIME.h"
#include "CandidateListUIPresenter.h"

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnInitDocumentMgr
//
// Sink called by the framework just before the first context is pushed onto
// a document.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnInitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr)
{
    pDocMgr;
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnUninitDocumentMgr
//
// Sink called by the framework just after the last context is popped off a
// document.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnUninitDocumentMgr(_In_ ITfDocumentMgr *pDocMgr)
{
    pDocMgr;
    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnSetFocus
//
// Sink called by the framework when focus changes from one document to
// another.  Either document may be NULL, meaning previously there was no
// focus document, or now no document holds the input focus.
//----------------------------------------------------------------------------
#include "Compartment.h"
STDAPI CSampleIME::OnSetFocus(_In_ ITfDocumentMgr *pDocMgrFocus, _In_ ITfDocumentMgr *pDocMgrPrevFocus)
{
    pDocMgrPrevFocus;
    if (Global::b_showedToolbar) return S_OK;
    _InitTextEditSink(pDocMgrFocus);
    Global::isGetFocus = pDocMgrFocus != nullptr;
    /*HWND hwnd = ::GetForegroundWindow();*/
    //if (hwnd) {
    //    if (GetClassNameW(hwnd, Global::foregroundClassName, 128)) {
    //        OutputDebugString(Global::isGetFocus ? L"00激活输入法 OnSetFocus:  -------------------当前程序名称： ------------------------------------------------------------Global::isGetFocus：---T":
    //            L"00激活输入法 OnSetFocus:  -------------------当前程序名称： ------------------------------------------------------------Global::isGetFocus：---F");
    //        OutputDebugString(Global::foregroundClassName);
    //    }
    //}
    OutputDebugString(Global::isGetFocus ? L"0011CSampleIME::OnSetFocus确保有焦点--Global::isGetFocus----------------T " : L"0011CSampleIME::OnSetFocus确保有焦点--Global::isGetFocus-----------------------F ");
    _UpdateLanguageBarOnSetFocus(pDocMgrFocus);
    if (pDocMgrFocus != nullptr)  // 确保有焦点
    {
        CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
        // 读取当前状态，避免不必要的设置
        BOOL currentState = FALSE;
        CompartmentKeyboardOpen._GetCompartmentBOOL(currentState);
        if (currentState != Global::isChineseMode)  // 仅当当前是英文时才切换
        {
            CompartmentKeyboardOpen._SetCompartmentBOOL(Global::isChineseMode);
        }
        else {

        }
    }

    // We have to hide/unhide candidate list depending on whether they are 
    // associated with pDocMgrFocus.
    //

    if (_pCandidateListUIPresenter)
    {
        ITfDocumentMgr* pCandidateListDocumentMgr = nullptr;
        ITfContext* pTfContext = _pCandidateListUIPresenter->_GetContextDocument();
        if ((nullptr != pTfContext) && SUCCEEDED(pTfContext->GetDocumentMgr(&pCandidateListDocumentMgr)))
        {
            if (pCandidateListDocumentMgr != pDocMgrFocus)
            {
                _pCandidateListUIPresenter->OnKillThreadFocus();
            }
            else 
            {
                _pCandidateListUIPresenter->OnSetThreadFocus();
            }

            pCandidateListDocumentMgr->Release();
        }
    }

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->Release();
		_pDocMgrLastFocused = nullptr;
    }

    _pDocMgrLastFocused = pDocMgrFocus;

    if (_pDocMgrLastFocused)
    {
        _pDocMgrLastFocused->AddRef();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnPushContext
//
// Sink called by the framework when a context is pushed.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnPushContext(_In_ ITfContext *pContext)
{
    pContext;

    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink::OnPopContext
//
// Sink called by the framework when a context is popped.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnPopContext(_In_ ITfContext *pContext)
{
    pContext;

    return E_NOTIMPL;
}

//+---------------------------------------------------------------------------
//
// _InitThreadMgrEventSink
//
// Advise our sink.
//----------------------------------------------------------------------------

BOOL CSampleIME::_InitThreadMgrEventSink()
{
    ITfSource* pSource = nullptr;
    BOOL ret = FALSE;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        return ret;
    }

    if (FAILED(pSource->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this, &_threadMgrEventSinkCookie)))
    {
        _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
        goto Exit;
    }

    ret = TRUE;

Exit:
    pSource->Release();
    return ret;
}

//+---------------------------------------------------------------------------
//
// _UninitThreadMgrEventSink
//
// Unadvise our sink.
//----------------------------------------------------------------------------

void CSampleIME::_UninitThreadMgrEventSink()
{
    ITfSource* pSource = nullptr;

    if (_threadMgrEventSinkCookie == TF_INVALID_COOKIE)
    {
        return; 
    }

    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        pSource->UnadviseSink(_threadMgrEventSinkCookie);
        pSource->Release();
    }

    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
}
