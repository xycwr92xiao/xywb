// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "Globals.h"
#include "EditSession.h"
#include "SampleIME.h"

//+---------------------------------------------------------------------------
//
// CStartCompositinoEditSession
//
//----------------------------------------------------------------------------

class CStartCompositionEditSession : public CEditSessionBase
{
public:
    CStartCompositionEditSession(_In_ CSampleIME *pTextService, _In_ ITfContext *pContext) : CEditSessionBase(pTextService, pContext)
    {
    }

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec);
};

//+---------------------------------------------------------------------------
//
// ITfEditSession::DoEditSession
//
//----------------------------------------------------------------------------

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec)
{
    HRESULT hrComposition = E_FAIL;   // 初始化
    ITfInsertAtSelection* pInsertAtSelection = nullptr;
    ITfRange* pRangeInsert = nullptr;
    ITfContextComposition* pContextComposition = nullptr;
    ITfComposition* pComposition = nullptr;

    if (FAILED(_pContext->QueryInterface(IID_ITfInsertAtSelection, (void **)&pInsertAtSelection)))
    {
        goto Exit;
    }

    if (FAILED(pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pRangeInsert)))
    {
        goto Exit;
    }

    if (FAILED(_pContext->QueryInterface(IID_ITfContextComposition, (void **)&pContextComposition)))
    {
        goto Exit;
    }
    hrComposition = pContextComposition->StartComposition(ec, pRangeInsert, _pTextService, &pComposition);
    if (SUCCEEDED(hrComposition) && (nullptr != pComposition))
    {
        _pTextService->_SetComposition(pComposition);

        // set selection to the adjusted range
        TF_SELECTION tfSelection;
        tfSelection.range = pRangeInsert;
        tfSelection.style.ase = TF_AE_NONE;
        tfSelection.style.fInterimChar = FALSE;

        _pContext->SetSelection(ec, 1, &tfSelection);
        _pTextService->_SaveCompositionContext(_pContext);
    }

Exit:
    if (nullptr != pContextComposition)
    {
        pContextComposition->Release();
    }

    if (nullptr != pRangeInsert)
    {
        pRangeInsert->Release();
    }

    if (nullptr != pInsertAtSelection)
    {
        pInsertAtSelection->Release();
    }

    return hrComposition;
}

//////////////////////////////////////////////////////////////////////
//
// CSampleIME class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// _StartComposition
//
// this starts the new (std::nothrow) composition at the selection of the current 
// focus context.
//----------------------------------------------------------------------------

HRESULT CSampleIME::_StartComposition(_In_ ITfContext *pContext)
{
    if (_pComposition)
        return S_OK;
    CStartCompositionEditSession* pStartCompositionEditSession = new (std::nothrow) CStartCompositionEditSession(this, pContext);
    if (!pStartCompositionEditSession)
        return E_OUTOFMEMORY;

    HRESULT hrEdit = E_FAIL;
    HRESULT hrRequest = pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hrEdit);
    pStartCompositionEditSession->Release();

    // RequestEditSession 成功且编辑会话成功，且 _pComposition 已被设置
    if (SUCCEEDED(hrRequest) && SUCCEEDED(hrEdit) && _pComposition)
        return S_OK;

    // 否则返回错误
    return FAILED(hrRequest) ? hrRequest : (FAILED(hrEdit) ? hrEdit : E_FAIL);

}

//+---------------------------------------------------------------------------
//
// _SaveCompositionContext
//
// this saves the context _pComposition belongs to; we need this to clear
// text attribute in case composition has not been terminated on 
// deactivation
//----------------------------------------------------------------------------

void CSampleIME::_SaveCompositionContext(_In_ ITfContext *pContext)
{
    assert(_pContext == nullptr);

    pContext->AddRef();
    _pContext = pContext;
} 
