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
#include "CandidateListUIPresenter.h"
#include "CompositionProcessorEngine.h"

//////////////////////////////////////////////////////////////////////
//
// CSampleIME class
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// _IsRangeCovered
//
// Returns TRUE if pRangeTest is entirely contained within pRangeCover.
//
//----------------------------------------------------------------------------

BOOL CSampleIME::_IsRangeCovered(TfEditCookie ec, _In_ ITfRange *pRangeTest, _In_ ITfRange *pRangeCover)
{
    LONG lResult = 0;;

    if (FAILED(pRangeCover->CompareStart(ec, pRangeTest, TF_ANCHOR_START, &lResult)) 
        || (lResult > 0))
    {
        return FALSE;
    }

    if (FAILED(pRangeCover->CompareEnd(ec, pRangeTest, TF_ANCHOR_END, &lResult)) 
        || (lResult < 0))
    {
        return FALSE;
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// _DeleteCandidateList
//
//----------------------------------------------------------------------------

VOID CSampleIME::_DeleteCandidateList(BOOL isForce, _In_opt_ ITfContext *pContext)
{
    isForce;pContext;

    CCompositionProcessorEngine* pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;
    pCompositionProcessorEngine->PurgeVirtualKey();

    if (_pCandidateListUIPresenter)
    {
        _pCandidateListUIPresenter->_EndCandidateList();

        _candidateMode = CANDIDATE_NONE;
        _isCandidateWithWildcard = FALSE;
    }
}

//+---------------------------------------------------------------------------
//
// _HandleComplete
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleComplete(TfEditCookie ec, _In_ ITfContext *pContext)
{
    _fBlockNewInput = FALSE;
    _fWaitForPush = FALSE;
    _DeleteCandidateList(FALSE, pContext);

    // just terminate the composition
    _TerminateComposition(ec, pContext);

    return S_OK;
}
HRESULT CSampleIME::_HandleCommitRawText(TfEditCookie ec, _In_ ITfContext* pContext)
{
    OutputDebugString(L"_HandleCommitRawText called\n");

    // 1. 如果候选窗口存在，关闭它（不提交候选词）
    if (_pCandidateListUIPresenter != nullptr)
    {
        _DeleteCandidateList(FALSE, pContext);
    }

    // 2. 如果有组合，提交组合中的原始文本
    if (_IsComposing())
    {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange)))
        {
            // 获取组合中的文本（即编码字母）
            ULONG cch = 0;
            HRESULT hr = pRange->GetText(ec, 0, NULL, 0, &cch);
            if (SUCCEEDED(hr) && cch > 0)
            {
                WCHAR* pchText = new (std::nothrow) WCHAR[cch + 1];
                if (pchText)
                {
                    ULONG copied = 0;
                    hr = pRange->GetText(ec, 0, pchText, cch, &copied);
                    if (SUCCEEDED(hr) && copied > 0)
                    {
                        pchText[copied] = L'\0';

                        // 直接用 SetText 替换整个组合范围（这样文本会插入到文档）
                        pRange->SetText(ec, 0, pchText, copied);

                        // 将光标移到插入文本的末尾
                        pRange->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION sel;
                        sel.range = pRange;
                        sel.style.ase = TF_AE_NONE;
                        sel.style.fInterimChar = FALSE;
                        pContext->SetSelection(ec, 1, &sel);
                    }
                    delete[] pchText;
                }
            }
            pRange->Release();
        }

        // 结束组合（移除虚线，释放组合对象）
        _TerminateComposition(ec, pContext);

        // 清理状态标志
        _fBlockNewInput = FALSE;
        _fWaitForPush = FALSE;
    }

    return S_OK;
}
//+---------------------------------------------------------------------------
//
// _HandleCancel
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCancel(TfEditCookie ec, _In_ ITfContext *pContext)
{
    _fBlockNewInput = FALSE;
    _fWaitForPush = FALSE;
    _RemoveDummyCompositionForComposing(ec, _pComposition);

    _DeleteCandidateList(FALSE, pContext);

    _TerminateComposition(ec, pContext);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionInput
//
// If the keystroke happens within a composition, eat the key and return S_OK.
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionInput(TfEditCookie ec, _In_ ITfContext *pContext, WCHAR wch)
{
    if (_fBlockNewInput)
    {
        MessageBeep(MB_ICONASTERISK);
        return S_OK;  // 吃掉按键
    }
    // 顶字逻辑：如果处于等待顶字状态，先上屏第一个候选，再重新处理当前按键
    if (_fWaitForPush && _pComposition && _pCandidateListUIPresenter)
    {
        // 上屏当前第一个候选
        _HandleCandidateFinalize(ec, pContext);
        _fWaitForPush = FALSE;
        // 重新处理当前按键（递归调用，只会一次）
        return _HandleCompositionInput(ec, pContext, wch);
    }
    ITfRange* pRangeComposition = nullptr;
    TF_SELECTION tfSelection;
    ULONG fetched = 0;
    BOOL isCovered = TRUE;

    CCompositionProcessorEngine* pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    if ((_pCandidateListUIPresenter != nullptr) && (_candidateMode != CANDIDATE_INCREMENTAL))
    {
        _HandleCompositionFinalize(ec, pContext, FALSE);
    }

    // Start the new (std::nothrow) compositon if there is no composition.
    if (!_IsComposing())
    {
        HRESULT hrStart = _StartComposition(pContext);
        if (FAILED(hrStart))
        {
            // 如果启动组合失败，不要处理该按键，直接返回
            OutputDebugString(L"_StartComposition 失败，按键被忽略");
            return S_OK; // 或 S_FALSE，但必须吃掉按键，防止应用程序收到该键
        }
    }

    // first, test where a keystroke would go in the document if we did an insert
    if (pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched) != S_OK || fetched != 1)
    {
        return S_FALSE;
    }

    // is the insertion point covered by a composition?
    if (SUCCEEDED(_pComposition->GetRange(&pRangeComposition)))
    {
        isCovered = _IsRangeCovered(ec, tfSelection.range, pRangeComposition);

        pRangeComposition->Release();

        if (!isCovered)
        {
            goto Exit;
        }
    }
    CCompositionProcessorEngine* pEngine = _pCompositionProcessorEngine;
    // Add virtual key to composition processor engine
    pCompositionProcessorEngine->AddVirtualKey(wch);

    // 调用 worker，返回 S_OK 表示有候选，S_FALSE 表示无候选
    HRESULT hr = _HandleCompositionInputWorker(pEngine, ec, pContext);

    if (hr == S_FALSE)  // 无候选
    {
        // 回退刚添加的字母（因为无效）
        //pEngine->RemoveVirtualKey(pEngine->GetVirtualKeyLength() - 1);
        // 发出提示音
        MessageBeep(MB_ICONASTERISK);   // 或 Beep(800, 200);
        // 设置阻塞标志，后续字母不再进入组合
        _fBlockNewInput = TRUE;
        // 不取消组合，保留现有编码
        return S_OK;
    }
    else
    {
        // 有效输入，清除阻塞标志
        _fBlockNewInput = FALSE;
    }

Exit:
    tfSelection.range->Release();
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionInputWorker
//
// If the keystroke happens within a composition, eat the key and return S_OK.
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionInputWorker(_In_ CCompositionProcessorEngine *pCompositionProcessorEngine, TfEditCookie ec, _In_ ITfContext *pContext)
{
    if (!_pComposition || !pCompositionProcessorEngine || !pContext)
    {
        OutputDebugString(L"_HandleCompositionInputWorker: invalid state\n");
        return E_POINTER;
    }
    HRESULT hr = S_OK;
    CSampleImeArray<CStringRange> readingStrings;
    BOOL isWildcardIncluded = TRUE;

    //
    // Get reading string from composition processor engine
    //
    //OutputDebugString(L"1 - 进入 GetReadingStrings");
    pCompositionProcessorEngine->GetReadingStrings(&readingStrings, &isWildcardIncluded);
    //OutputDebugString(L"2 - GetReadingStrings 完成");

    for (UINT index = 0; index < readingStrings.Count(); index++)
    {
        //OutputDebugString(L"3 - 进入 _AddComposingAndChar");
        hr = _AddComposingAndChar(ec, pContext, readingStrings.GetAt(index));
        //OutputDebugString(L"4 - _AddComposingAndChar 完成");
        if (FAILED(hr))
        {
            return hr;
        }
    }

    //
    // Get candidate string from composition processor engine
    //
    CSampleImeArray<CCandidateListItem> candidateList;
   // OutputDebugString(L"5 - 进入 GetCandidateList");
    pCompositionProcessorEngine->GetCandidateList(&candidateList, TRUE, FALSE);
    //OutputDebugString(L"6 - GetCandidateList 完成");
    if ((candidateList.Count()))
    {
        hr = _CreateAndStartCandidate(pCompositionProcessorEngine, ec, pContext);
        if (SUCCEEDED(hr))
        {
            _pCandidateListUIPresenter->_ClearList();
            _pCandidateListUIPresenter->_SetText(&candidateList, TRUE);
        }
    }
    else if (_pCandidateListUIPresenter)
    {
        _pCandidateListUIPresenter->_ClearList();
    }
    else if (readingStrings.Count() && isWildcardIncluded)
    {
        hr = _CreateAndStartCandidate(pCompositionProcessorEngine, ec, pContext);
        if (SUCCEEDED(hr))
        {
            _pCandidateListUIPresenter->_ClearList();
        }
    }
    // 自动上屏与顶字状态设置
    DWORD_PTR keystrokeLen = pCompositionProcessorEngine->GetVirtualKeyLength();
    UINT candidateCount = 0;
    if (_pCandidateListUIPresenter)
    {
        candidateCount = _pCandidateListUIPresenter->_GetCandidateCount();
    }
    int firstSource = 0;
    if (candidateList.Count() > 0)
        firstSource = candidateList.GetAt(0)->_source;
    if ((!Global::isPinyinMode || firstSource>0) && keystrokeLen == 4)
    {
        if (candidateCount == 1)
        {
            // 唯一候选，自动上屏
            _HandleCandidateFinalize(ec, pContext);
            return S_OK;
        }
        else if (candidateCount > 1)
        {
            // 多候选，进入顶字等待状态
            _fWaitForPush = TRUE;
        }
        else
        {
            _fWaitForPush = FALSE;
        }
    }
    else
    {
        // 编码长度不是4时，清除等待标志
        _fWaitForPush = FALSE;
    }
    return (candidateList.Count() > 0) ? S_OK : S_FALSE;
}
//+---------------------------------------------------------------------------
//
// _CreateAndStartCandidate
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_CreateAndStartCandidate(_In_ CCompositionProcessorEngine *pCompositionProcessorEngine, TfEditCookie ec, _In_ ITfContext *pContext)
{
    HRESULT hr = S_OK;
    if (((_candidateMode == CANDIDATE_PHRASE) && (_pCandidateListUIPresenter))
        || ((_candidateMode == CANDIDATE_NONE) && (_pCandidateListUIPresenter)))
    {
        // Recreate candidate list
        _pCandidateListUIPresenter->_EndCandidateList();
        delete _pCandidateListUIPresenter;
        _pCandidateListUIPresenter = nullptr;

        _candidateMode = CANDIDATE_NONE;
        _isCandidateWithWildcard = FALSE;
    }

    if (_pCandidateListUIPresenter == nullptr)
    {
        _pCandidateListUIPresenter = new (std::nothrow) CCandidateListUIPresenter(this, Global::AtomCandidateWindow,
            CATEGORY_CANDIDATE,
            pCompositionProcessorEngine->GetCandidateListIndexRange(),
            FALSE);
        if (!_pCandidateListUIPresenter)
        {
            return E_OUTOFMEMORY;
        }

        _candidateMode = CANDIDATE_INCREMENTAL;
        _isCandidateWithWildcard = FALSE;

        // we don't cache the document manager object. So get it from pContext.
        ITfDocumentMgr* pDocumentMgr = nullptr;
        if (SUCCEEDED(pContext->GetDocumentMgr(&pDocumentMgr)))
        {
            // get the composition range.
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(_pComposition->GetRange(&pRange)))
            {
                hr = _pCandidateListUIPresenter->_StartCandidateList(_tfClientId, pDocumentMgr, pContext, ec, pRange, pCompositionProcessorEngine->GetCandidateWindowWidth());
                pRange->Release();
            }
            pDocumentMgr->Release();
        }
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionFinalize
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionFinalize(TfEditCookie ec, _In_ ITfContext *pContext, BOOL isCandidateList)
{
    HRESULT hr = S_OK;

    if (isCandidateList && _pCandidateListUIPresenter)
    {
        // Finalize selected candidate string from CCandidateListUIPresenter
        DWORD_PTR candidateLen = 0;
        const WCHAR *pCandidateString = nullptr;

        candidateLen = _pCandidateListUIPresenter->_GetSelectedCandidateString(&pCandidateString);

        CStringRange candidateString;
        candidateString.Set(pCandidateString, candidateLen);

        if (candidateLen)
        {
            // Finalize character
            hr = _AddCharAndFinalize(ec, pContext, &candidateString);
            if (FAILED(hr))
            {
                return hr;
            }
        }
    }
    else
    {
        // Finalize current text store strings
        if (_IsComposing())
        {
            ULONG fetched = 0;
            TF_SELECTION tfSelection;

            if (FAILED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched)) || fetched != 1)
            {
                return S_FALSE;
            }

            ITfRange* pRangeComposition = nullptr;
            if (SUCCEEDED(_pComposition->GetRange(&pRangeComposition)))
            {
                if (_IsRangeCovered(ec, tfSelection.range, pRangeComposition))
                {
                    _EndComposition(pContext);
                }

                pRangeComposition->Release();
            }

            tfSelection.range->Release();
        }
    }

    _HandleCancel(ec, pContext);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionConvert
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionConvert(TfEditCookie ec, _In_ ITfContext *pContext, BOOL isWildcardSearch)
{
    HRESULT hr = S_OK;

    CSampleImeArray<CCandidateListItem> candidateList;

    //
    // Get candidate string from composition processor engine
    //
    CCompositionProcessorEngine* pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;
    pCompositionProcessorEngine->GetCandidateList(&candidateList, FALSE, isWildcardSearch);

    // If there is no candlidate listin the current reading string, we don't do anything. Just wait for
    // next char to be ready for the conversion with it.
    int nCount = candidateList.Count();
    if (nCount)
    {
        if (_pCandidateListUIPresenter)
        {
            _pCandidateListUIPresenter->_EndCandidateList();
            delete _pCandidateListUIPresenter;
            _pCandidateListUIPresenter = nullptr;

            _candidateMode = CANDIDATE_NONE;
            _isCandidateWithWildcard = FALSE;
        }

        // 
        // create an instance of the candidate list class.
        // 
        if (_pCandidateListUIPresenter == nullptr)
        {
            _pCandidateListUIPresenter = new (std::nothrow) CCandidateListUIPresenter(this, Global::AtomCandidateWindow,
                CATEGORY_CANDIDATE,
                pCompositionProcessorEngine->GetCandidateListIndexRange(),
                FALSE);
            if (!_pCandidateListUIPresenter)
            {
                return E_OUTOFMEMORY;
            }

            _candidateMode = CANDIDATE_ORIGINAL;
        }

        _isCandidateWithWildcard = isWildcardSearch;

        // we don't cache the document manager object. So get it from pContext.
        ITfDocumentMgr* pDocumentMgr = nullptr;
        if (SUCCEEDED(pContext->GetDocumentMgr(&pDocumentMgr)))
        {
            // get the composition range.
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(_pComposition->GetRange(&pRange)))
            {
                hr = _pCandidateListUIPresenter->_StartCandidateList(_tfClientId, pDocumentMgr, pContext, ec, pRange, pCompositionProcessorEngine->GetCandidateWindowWidth());
                pRange->Release();
            }
            pDocumentMgr->Release();
        }
        if (SUCCEEDED(hr))
        {
            _pCandidateListUIPresenter->_SetText(&candidateList, FALSE);
        }
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionBackspace
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionBackspace(TfEditCookie ec, _In_ ITfContext *pContext)
{
    ITfRange* pRangeComposition = nullptr;
    TF_SELECTION tfSelection;
    ULONG fetched = 0;
    BOOL isCovered = TRUE;

    // Start the new (std::nothrow) compositon if there is no composition.
    if (!_IsComposing())
    {
        return S_OK;
    }

    // first, test where a keystroke would go in the document if we did an insert
    if (FAILED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched)) || fetched != 1)
    {
        return S_FALSE;
    }

    // is the insertion point covered by a composition?
    if (SUCCEEDED(_pComposition->GetRange(&pRangeComposition)))
    {
        isCovered = _IsRangeCovered(ec, tfSelection.range, pRangeComposition);

        pRangeComposition->Release();

        if (!isCovered)
        {
            goto Exit;
        }
    }

    //
    // Add virtual key to composition processor engine
    //
    CCompositionProcessorEngine* pCompositionProcessorEngine = nullptr;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    DWORD_PTR vKeyLen = pCompositionProcessorEngine->GetVirtualKeyLength();

    if (vKeyLen)
    {
        pCompositionProcessorEngine->RemoveVirtualKey(vKeyLen - 1);

        if (pCompositionProcessorEngine->GetVirtualKeyLength())
        {
            HRESULT hr = _HandleCompositionInputWorker(pCompositionProcessorEngine, ec, pContext);
            // 如果 worker 返回 S_OK，说明有候选，清除阻塞标志
            if (hr == S_OK)
            {
                _fBlockNewInput = FALSE;
            }
        }
        else
        {
            _HandleCancel(ec, pContext);
        }
    }

Exit:
    tfSelection.range->Release();
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionArrowKey
//
// Update the selection within a composition.
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionArrowKey(TfEditCookie ec, _In_ ITfContext *pContext, KEYSTROKE_FUNCTION keyFunction)
{
    ITfRange* pRangeComposition = nullptr;
    TF_SELECTION tfSelection;
    ULONG fetched = 0;

    // get the selection
    if (FAILED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched))
        || fetched != 1)
    {
        // no selection, eat the keystroke
        return S_OK;
    }

    // get the composition range
    if (FAILED(_pComposition->GetRange(&pRangeComposition)))
    {
        goto Exit;
    }

    // For incremental candidate list
    if (_pCandidateListUIPresenter)
    {
        _pCandidateListUIPresenter->AdviseUIChangedByArrowKey(keyFunction);
    }

    pContext->SetSelection(ec, 1, &tfSelection);

    pRangeComposition->Release();

Exit:
    tfSelection.range->Release();
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionPunctuation
//
// 处理标点符号输入：
//   1. 如果存在候选列表，先强制提交第一个候选词（或当前选中项）并彻底结束组合
//   2. 然后根据符号类型：
//      - 左括号类（包括 < 双书名号、> 单书名号等）：成对输出，光标居中
//      - 其他标点：直接输出
//----------------------------------------------------------------------------
// 根据窗口句柄获取进程exe文件名（小写返回方便对比）
BOOL GetProcessNameFromHwnd(HWND hWnd, WCHAR szExeNameOut[MAX_PATH])
{
    ZeroMemory(szExeNameOut, sizeof(WCHAR) * MAX_PATH);
    DWORD dwPid = 0;
    GetWindowThreadProcessId(hWnd, &dwPid);
    if (dwPid == 0) return FALSE;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPid);
    if (hProcess == NULL) return FALSE;

    WCHAR szFullPath[MAX_PATH] = { 0 };
    DWORD bufLen = MAX_PATH;
    BOOL bOk = QueryFullProcessImageNameW(hProcess, 0, szFullPath, &bufLen);
    CloseHandle(hProcess);
    if (!bOk) return FALSE;

    // 提取文件名（反斜杠最后一段）
    WCHAR* pFileName = wcsrchr(szFullPath, L'\\');
    if (pFileName == nullptr) pFileName = szFullPath;
    else pFileName++;

    // 转小写统一判断
    StringCchCopyW(szExeNameOut, MAX_PATH, pFileName);
    _wcslwr_s(szExeNameOut, MAX_PATH);
    return TRUE;
}

// 类型枚举，方便分支判断
enum EDITOR_TYPE
{
    ET_OTHER,        // 其他软件
    ET_MICROSOFT_WORD,// 微软Word WINWORD.EXE
    ET_WPS_WRITER,  // WPS文字 wps.exe
    ET_NOTEPAD,     // 记事本 RichEditD2DPT
    ET_VFP          // VFP vfp99400000
};
EDITOR_TYPE GetCurrentEditorType(HWND hFocus)
{
    if (!hFocus) return ET_OTHER;

    // 先判断记事本/VFP（类名唯一，不用读进程）
    WCHAR szCls[256] = { 0 };
    GetClassNameW(hFocus, szCls, ARRAYSIZE(szCls));
    OutputDebugString(L"00成对括号检测 GetCurrentEditorType:  -------------------当前程序名称： ------------------------------------------------------------Global::isGetFocus：---T");
    OutputDebugString(szCls);
    if (wcscmp(szCls, L"RichEditD2DPT") == 0)
        return ET_NOTEPAD;
    if (wcscmp(szCls, L"vfp99400000") == 0 || wcsstr(szCls, L"9c000000") != NULL)
        return ET_VFP;

    // 只剩 _WwG，区分Word/WPS
    WCHAR szExe[MAX_PATH] = { 0 };
    if (!GetProcessNameFromHwnd(hFocus, szExe))
        return ET_OTHER;

    if (wcscmp(szExe, L"winword.exe") == 0 || wcscmp(szExe, L"excel.exe") == 0 || wcscmp(szExe, L"powerpnt.exe") == 0 || wcscmp(szExe, L"msaccess.exe") == 0)
        return ET_MICROSOFT_WORD;
    if (wcscmp(szExe, L"wps.exe") == 0 || wcscmp(szExe, L"wpp.exe") == 0 || wcscmp(szExe, L"et.exe") == 0 || wcscmp(szExe, L"wpspdf.exe") == 0 || wcscmp(szExe, L"wpsoffice.exe") == 0)
        return ET_WPS_WRITER;

    return ET_OTHER;
}

HRESULT CSampleIME::_HandleCompositionPunctuation(TfEditCookie ec, _In_ ITfContext* pContext, WCHAR wch)
{
    HRESULT hr = S_OK;

    // 1. 如果存在候选窗口，先提交当前选中的候选（或第一个候选），并结束组合
    if (_pCandidateListUIPresenter != nullptr && _pComposition != nullptr)
    {
        hr = _HandleCandidateFinalize(ec, pContext);
        if (FAILED(hr))
            return hr;
        // 此时 _pComposition 已被置为 NULL，组合结束
    }

    // 如果组合还在（理论上应该已结束），则强制结束
    if (_pComposition != nullptr)
    {
        _TerminateComposition(ec, pContext);
        _pComposition = nullptr;
    }

    // 2. 获取当前插入点（选择）
    TF_SELECTION tfSelection;
    ULONG fetched = 0;
    if (FAILED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched)) || fetched != 1)
    {
        return S_FALSE; // 无法获取插入点，放弃处理
    }

    ITfRange* pRange = tfSelection.range;
    // 我们将在插入后释放它，但先保留

    // 3. 判断标点类型
    CCompositionProcessorEngine* pEngine = _pCompositionProcessorEngine;
    if (pEngine == nullptr)
    {
        pRange->Release();
        return E_FAIL;
    }
    BOOL fFullWidth = pEngine->GetFullWidthPunctuation();

    WCHAR leftChar = 0, rightChar = 0;
    BOOL isPaired = FALSE;
    BOOL isRightOnly = FALSE;

    // ---------- 成对符号判断 ----------
    if (wch == L'(' || wch == L'（' ||
        wch == L'[' || wch == L'［' ||
        wch == L'{' || wch == L'｛' ||
        wch == L'<' || wch == L'《' || wch == L'〈')
    {
        isPaired = TRUE;
        if (wch == L'(' || wch == L'（') {
            leftChar = fFullWidth ? L'（' : L'(';
            rightChar = fFullWidth ? L'）' : L')';
        }
        else if (wch == L'[' || wch == L'［') {
            leftChar = fFullWidth ? L'［' : L'[';
            rightChar = fFullWidth ? L'］' : L']';
        }
        else if (wch == L'{' || wch == L'｛') {
            leftChar = fFullWidth ? L'｛' : L'{';
            rightChar = fFullWidth ? L'｝' : L'}';
        }
        else { // 书名号
            if (wch == L'〈') {
                leftChar = L'〈';
                rightChar = L'〉';
            }
            else {
                leftChar = fFullWidth ? L'《' : L'<';
                rightChar = fFullWidth ? L'》' : L'>';
            }
        }
    }
    else if (wch == L'\'' || wch == L'‘' || wch == L'’' ||
        wch == L'\"' || wch == L'“' || wch == L'”')
    {
        isPaired = TRUE;
        if (wch == L'\'' || wch == L'‘' || wch == L'’') {
            leftChar = fFullWidth ? L'‘' : L'\'';
            rightChar = fFullWidth ? L'’' : L'\'';
        }
        else {
            leftChar = fFullWidth ? L'“' : L'\"';
            rightChar = fFullWidth ? L'”' : L'\"';
        }
    }
    else if (wch == L'》' || wch == L'〉' || (wch == L'>' && fFullWidth))
    {
        isPaired = TRUE;
        leftChar =  L'〈' ;
        rightChar =  L'〉';
    }
    else if (wch == L']')
    {
        isRightOnly = TRUE;
        wch = fFullWidth ? L'］' : L']';

    }
    else if (wch == L'}')
    {
        isRightOnly = TRUE;
        wch = fFullWidth ? L'｝' : L'}';
    }

    // ---------- 执行插入 ----------
    if (isPaired)
    {
        WCHAR pair[3] = { leftChar, rightChar, 0 };
        hr = pRange->SetText(ec, 0, pair, 2);
        if (SUCCEEDED(hr))
        {
            HWND hFocus = GetFocus();
            EDITOR_TYPE editor = GetCurrentEditorType(hFocus);
            if (editor != ET_VFP) {
                pRange->Collapse(ec, TF_ANCHOR_START);
                // 向右移动一个字符到两个括号之间
                LONG shift = 1;
                LONG cchShiftedStart = 0, cchShiftedEnd = 0;
                HRESULT hrStart = pRange->ShiftStart(ec, shift, &cchShiftedStart, NULL);
                HRESULT hrEnd = pRange->ShiftEnd(ec, shift, &cchShiftedEnd, NULL);
                if (SUCCEEDED(hrStart) && SUCCEEDED(hrEnd))
                {
                    pRange->Collapse(ec, TF_ANCHOR_START);
                    // 设置为新选区
                    TF_SELECTION newSel;
                    newSel.range = pRange;
                    newSel.style.ase = TF_AE_NONE;
                    newSel.style.fInterimChar = FALSE;
                    pContext->SetSelection(ec, 1, &newSel);
                }
            }
            if (editor == ET_MICROSOFT_WORD || editor == ET_NOTEPAD)
            {
                // 微软Word、记事本：无需额外按键，直接结束
                OutputDebugString(L"微软Word/记事本，仅TSF定位\n");
            }
            else if (editor == ET_VFP)
            {
                // VFP自绘控件：直接窗口消息，绕过输入拦截
                keybd_event(VK_LEFT, 0, 0, 0);
                keybd_event(VK_LEFT, 0, KEYEVENTF_KEYUP, 0);
                OutputDebugString(L"VFP编辑器，SendMessage左移\n");
            }
            else
            {
                // WPS文字、Qt、Electron、其余所有软件：SendInput模拟左箭头
                if (editor == ET_WPS_WRITER) {
                    Sleep(200);
                }
                INPUT input[2] = {};
                input[0].type = INPUT_KEYBOARD;
                input[0].ki.wVk = VK_LEFT;
                input[1].type = INPUT_KEYBOARD;
                input[1].ki.wVk = VK_LEFT;
                input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(2, input, sizeof(INPUT));

                if (editor == ET_WPS_WRITER)
                    OutputDebugString(L"WPS文字，执行模拟左移\n");
                else
                    OutputDebugString(L"第三方编辑器，模拟左移\n");
            }
        }
    }
    else if (isRightOnly)
    {
        // 单独右符号
        hr = pRange->SetText(ec, 0, &wch, 1);
        if (SUCCEEDED(hr))
        {
            // 光标移到插入文本末尾（默认就是）
            pRange->Collapse(ec, TF_ANCHOR_END);
            pContext->SetSelection(ec, 1, &tfSelection);
        }
    }
    else
    {
        // 普通标点（非成对）
        WCHAR punctuation = fFullWidth ? pEngine->GetPunctuation(wch) : wch;
        hr = pRange->SetText(ec, 0, &punctuation, 1);
        if (SUCCEEDED(hr))
        {
            pRange->Collapse(ec, TF_ANCHOR_END);
            pContext->SetSelection(ec, 1, &tfSelection);
        }
    }

    pRange->Release();

    // 最后，确保没有残留的组合（如果因某些原因还有）
    if (_pComposition != nullptr)
    {
        _TerminateComposition(ec, pContext);
        _pComposition = nullptr;
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// _HandleCompositionDoubleSingleByte
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::_HandleCompositionDoubleSingleByte(TfEditCookie ec, _In_ ITfContext *pContext, WCHAR wch)
{
    HRESULT hr = S_OK;

    WCHAR fullWidth = Global::FullWidthCharTable[wch - 0x20];

    CStringRange fullWidthString;
    fullWidthString.Set(&fullWidth, 1);

    // Finalize character
    hr = _AddCharAndFinalize(ec, pContext, &fullWidthString);
    if (FAILED(hr))
    {
        return hr;
    }

    _HandleCancel(ec, pContext);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// _InvokeKeyHandler
//
// This text service is interested in handling keystrokes to demonstrate the
// use the compositions. Some apps will cancel compositions if they receive
// keystrokes while a compositions is ongoing.
//
// param
//    [in] uCode - virtual key code of WM_KEYDOWN wParam
//    [in] dwFlags - WM_KEYDOWN lParam
//    [in] dwKeyFunction - Function regarding virtual key
//----------------------------------------------------------------------------

HRESULT CSampleIME::_InvokeKeyHandler(_In_ ITfContext *pContext, UINT code, WCHAR wch, DWORD flags, _KEYSTROKE_STATE keyState)
{
    flags;

    CKeyHandlerEditSession* pEditSession = nullptr;
    HRESULT hr = E_FAIL;

    // we'll insert a char ourselves in place of this keystroke
    pEditSession = new (std::nothrow) CKeyHandlerEditSession(this, pContext, code, wch, keyState);
    if (pEditSession == nullptr)
    {
        goto Exit;
    }

    //
    // Call CKeyHandlerEditSession::DoEditSession().
    //
    // Do not specify TF_ES_SYNC so edit session is not invoked on WinWord
    //
    hr = pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);

    pEditSession->Release();

Exit:
    return hr;
}

// KeyHandler.cpp
HRESULT CSampleIME::_HandleDirectCommit(TfEditCookie ec, _In_ ITfContext* pContext)
{
    OutputDebugString(L"_HandleDirectCommit called\n");
    HRESULT hr = S_OK;

    // 1. 如果有候选窗口，取第一个候选词并提交
    if (_pCandidateListUIPresenter != nullptr)
    {
        const WCHAR* pCandidateString = nullptr;
        DWORD_PTR candidateLen = _pCandidateListUIPresenter->_GetSelectedCandidateString(&pCandidateString);
        if (candidateLen > 0 && pCandidateString != nullptr)
        {
            CStringRange candidateStr;
            candidateStr.Set(pCandidateString, candidateLen);
            hr = _AddCharAndFinalize(ec, pContext, &candidateStr);
            _HandleCancel(ec, pContext);
            return hr;
        }
    }

    // 2. 没有候选窗口，但有组合字符串（比如刚输入几个字母还未转换）
    if (_IsComposing())
    {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(_pComposition->GetRange(&pRange)))
        {
            // 先获取文本长度
            ULONG cch = 0;
            hr = pRange->GetText(ec, 0, NULL, 0, &cch);
            if (SUCCEEDED(hr) && cch > 0)
            {
                // 分配缓冲区（+1 给结束符，但 GetText 不会自动加结束符）
                WCHAR* pchText = new (std::nothrow) WCHAR[cch + 1];
                if (pchText)
                {
                    ULONG cchCopied = 0;
                    hr = pRange->GetText(ec, 0, pchText, cch, &cchCopied);
                    if (SUCCEEDED(hr) && cchCopied > 0)
                    {
                        pchText[cchCopied] = L'\0'; // 手动添加结束符
                        CStringRange currentText;
                        currentText.Set(pchText, cchCopied);
                        hr = _AddCharAndFinalize(ec, pContext, &currentText);
                        _HandleCancel(ec, pContext);
                    }
                    delete[] pchText;
                }
                else
                {
                    hr = E_OUTOFMEMORY;
                }
            }
            pRange->Release();
        }
    }

    return hr;
}