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
#include "CompositionProcessorEngine.h"
#include "KeyHandlerEditSession.h"
#include "Compartment.h"

// 用于在右Shift按下时关闭候选窗口并提交当前组合文本的 EditSession
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

// 0xF003, 0xF004 are the keys that the touch keyboard sends for next/previous
#define THIRDPARTY_NEXTPAGE  static_cast<WORD>(0xF003)
#define THIRDPARTY_PREVPAGE  static_cast<WORD>(0xF004)

// Because the code mostly works with VKeys, here map a WCHAR back to a VKKey for certain
// vkeys that the IME handles specially
__inline UINT VKeyFromVKPacketAndWchar(UINT vk, WCHAR wch)
{
    UINT vkRet = vk;
    if (LOWORD(vk) == VK_PACKET)
    {
        if (wch == L' ')
        {
            vkRet = VK_SPACE;
        }
        else if ((wch >= L'0') && (wch <= L'9'))
        {
            vkRet = static_cast<UINT>(wch);
        }
        else if ((wch >= L'a') && (wch <= L'z'))
        {
            vkRet = (UINT)(L'A') + ((UINT)(L'z') - static_cast<UINT>(wch));
        }
        else if ((wch >= L'A') && (wch <= L'Z'))
        {
            vkRet = static_cast<UINT>(wch);
        }
        else if (wch == THIRDPARTY_NEXTPAGE)
        {
            vkRet = VK_NEXT;
        }
        else if (wch == THIRDPARTY_PREVPAGE)
        {
            vkRet = VK_PRIOR;
        }
    }
    return vkRet;
}

//+---------------------------------------------------------------------------
//
// _IsKeyEaten
//
//----------------------------------------------------------------------------

BOOL CSampleIME::_IsKeyEaten(_In_ ITfContext *pContext, UINT codeIn, _Out_ UINT *pCodeOut, _Out_writes_(1) WCHAR *pwch, _Out_opt_ _KEYSTROKE_STATE *pKeyState)
{
    pContext;

    *pCodeOut = codeIn;
    BOOL isOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);

    BOOL isDoubleSingleByte = FALSE;
    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._GetCompartmentBOOL(isDoubleSingleByte);

    BOOL isPunctuation = FALSE;
    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    CompartmentPunctuation._GetCompartmentBOOL(isPunctuation);

    if (pKeyState)
    {
        pKeyState->Category = CATEGORY_NONE;
        pKeyState->Function = FUNCTION_NONE;
    }
    if (pwch)
    {
        *pwch = L'\0';
    }

    // if the keyboard is disabled, we don't eat keys.
    if (_IsKeyboardDisabled())
    {
        return FALSE;
    }

    //
    // Map virtual key to character code
    //
    BOOL isTouchKeyboardSpecialKeys = FALSE;
    WCHAR wch = ConvertVKey(codeIn);
    *pCodeOut = VKeyFromVKPacketAndWchar(codeIn, wch);
    if ((wch == THIRDPARTY_NEXTPAGE) || (wch == THIRDPARTY_PREVPAGE))
    {
        // We always eat the above softkeyboard special keys
        isTouchKeyboardSpecialKeys = TRUE;
        if (pwch)
        {
            *pwch = wch;
        }
    }

    // if the keyboard is closed, we don't eat keys, with the exception of the touch keyboard specials keys
    if (!isOpen && !isDoubleSingleByte && !isPunctuation)
    {
        return isTouchKeyboardSpecialKeys;// 英文模式，不处理普通按键
    }

    if (pwch)
    {
        *pwch = wch;
    }

    //
    // Get composition engine
    //
    CCompositionProcessorEngine *pCompositionProcessorEngine;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;

    if (isOpen)
    {
        //
        // The candidate or phrase list handles the keys through ITfKeyEventSink.
        //
        // eat only keys that CKeyHandlerEditSession can handles.
        //
        // 新增：大写字母直接上屏，不进行编码查找
        if (pwch && *pwch >= L'A' && *pwch <= L'Z')
        {
            if (pKeyState)
            {
                // 如果当前有组合或候选，先结束它们，让按键落入应用程序
                if (_IsComposing() || _pCandidateListUIPresenter != nullptr)
                {
                    pKeyState->Category = CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION;
                    pKeyState->Function = FUNCTION_FINALIZE_TEXTSTORE;
                }
                else
                {
                    pKeyState->Category = CATEGORY_NONE;
                    pKeyState->Function = FUNCTION_NONE;
                }
            }
            return FALSE;   // 让系统继续处理该按键，应用程序将收到大写字母
        }
        if (pCompositionProcessorEngine->IsVirtualKeyNeed(*pCodeOut, pwch, _IsComposing(), _candidateMode, _isCandidateWithWildcard, pKeyState))
        {
            return TRUE;
        }
    }

    //
    // Punctuation
    //
    if (pCompositionProcessorEngine->IsPunctuation(wch))
    {
        if ((_candidateMode == CANDIDATE_NONE) && isPunctuation)
        {
            if (pKeyState)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = FUNCTION_PUNCTUATION;
            }
            return TRUE;
        }
    }

    //
    // Double/Single byte
    //
    if (isDoubleSingleByte && pCompositionProcessorEngine->IsDoubleSingleByte(wch))
    {
        if (_candidateMode == CANDIDATE_NONE)
        {
            if (pKeyState)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = FUNCTION_DOUBLE_SINGLE_BYTE;
            }
            return TRUE;
        }
    }

    return isTouchKeyboardSpecialKeys;
}

//+---------------------------------------------------------------------------
//
// ConvertVKey
//
//----------------------------------------------------------------------------

WCHAR CSampleIME::ConvertVKey(UINT code)
{
    //
    // Map virtual key to scan code
    //
    UINT scanCode = 0;
    scanCode = MapVirtualKey(code, 0);

    //
    // Keyboard state
    //
    BYTE abKbdState[256] = {'\0'};
    if (!GetKeyboardState(abKbdState))
    {
        return 0;
    }

    //
    // Map virtual key to character code
    //
    WCHAR wch = '\0';
    if (ToUnicode(code, scanCode, abKbdState, &wch, 1, 0) == 1)
    {
        return wch;
    }

    return 0;
}

//+---------------------------------------------------------------------------
//
// _IsKeyboardDisabled
//
//----------------------------------------------------------------------------

BOOL CSampleIME::_IsKeyboardDisabled()
{
    ITfDocumentMgr* pDocMgrFocus = nullptr;
    ITfContext* pContext = nullptr;
    BOOL isDisabled = FALSE;

    if ((_pThreadMgr->GetFocus(&pDocMgrFocus) != S_OK) ||
        (pDocMgrFocus == nullptr))
    {
        // if there is no focus document manager object, the keyboard 
        // is disabled.
        isDisabled = TRUE;
    }
    else if ((pDocMgrFocus->GetTop(&pContext) != S_OK) ||
        (pContext == nullptr))
    {
        // if there is no context object, the keyboard is disabled.
        isDisabled = TRUE;
    }
    else
    {
        CCompartment CompartmentKeyboardDisabled(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_DISABLED);
        CompartmentKeyboardDisabled._GetCompartmentBOOL(isDisabled);

        CCompartment CompartmentEmptyContext(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_EMPTYCONTEXT);
        CompartmentEmptyContext._GetCompartmentBOOL(isDisabled);
    }

    if (pContext)
    {
        pContext->Release();
    }

    if (pDocMgrFocus)
    {
        pDocMgrFocus->Release();
    }

    return isDisabled;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnSetFocus
//
// Called by the system whenever this service gets the keystroke device focus.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnSetFocus(BOOL fForeground)
{
	fForeground;

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyDown
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);
    // ========== 新增：处理左 Ctrl 按下 ==========
    BOOL isInputKey = (wParam >= 'A' && wParam <= 'Z') ||
        (wParam >= '0' && wParam <= '9') ||
        wParam == VK_TAB ||
        wParam == VK_RETURN ||
        wParam == VK_SPACE ||
        wParam == VK_OEM_COMMA ||      // ,
        wParam == VK_OEM_PERIOD ||     // .
        wParam == VK_OEM_1 ||          // ;
        wParam == VK_OEM_2 ||          // /
        wParam == VK_OEM_3 ||          // `
        wParam == VK_OEM_5 ||          // \ (中文下常作顿号)
        wParam == VK_OEM_4 ||          // [
        wParam == VK_OEM_6 ||          // ]
        wParam == VK_OEM_7 ||          // '
        wParam == VK_OEM_PLUS ||       // =
        wParam == VK_OEM_MINUS;        // -
    if (_isOnlyCtrlShift && isInputKey)_isOnlyCtrlShift = FALSE;
    // OnKeyDown 中
    if (wParam == VK_SHIFT) {
        _isOnlyCtrlShift = TRUE;
        BOOL isOnlyShift = (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0;//VK_MENU代表Alt键
        if (_isOnlyCtrlShift && !isOnlyShift)_isOnlyCtrlShift = FALSE;
        if (isOnlyShift) {
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            return S_OK;
        }
    }
    if (wParam == VK_CONTROL)
    {
        _isOnlyCtrlShift = TRUE;
        BOOL isLeftCtrl = ((lParam >> 24) & 1) == 0;
        BOOL isRightCtrl = ((lParam >> 24) & 1) == 1;
        BOOL isOnlyCtrl = (GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0;
        if (_isOnlyCtrlShift && !isOnlyCtrl)_isOnlyCtrlShift = FALSE;
        if (isLeftCtrl && isOnlyCtrl)
        {
            // 如果已经标记为按下，说明是重复触发的消息，直接跳过
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
            // OutputDebugString(L"左Ctrl 被按下了--------KeyEventSink------------------------------------------------- ");
             // 记录按下的时间（毫秒）
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            *pIsEaten = TRUE;  // 可选：是否吃掉按键
            return S_OK;
        }
        if (isRightCtrl && isOnlyCtrl)
        {
            // 如果已经标记为按下，说明是重复触发的消息，直接跳过
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
            //OutputDebugString(L"右Ctrl 被按下了-------------------------KeyEventSink-------------------------------- ");
            // 记录按下的时间（毫秒）
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            *pIsEaten = TRUE;  // 可选：是否吃掉按键
            return S_OK;
        }
    }
    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;

    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, &KeystrokeState);

    if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        //
        // Invoke key handler edit session
        //
        KeystrokeState.Category = CATEGORY_COMPOSING;

        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnKeyDown
//
// Called by the system to offer this service a keystroke.  If *pIsEaten == TRUE
// on exit, the application will not handle the keystroke.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnKeyDown(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);
    BOOL isInputKey = (wParam >= 'A' && wParam <= 'Z') ||
        (wParam >= '0' && wParam <= '9') ||
        wParam == VK_TAB ||
        wParam == VK_RETURN ||
        wParam == VK_SPACE ||
        wParam == VK_OEM_COMMA ||      // ,
        wParam == VK_OEM_PERIOD ||     // .
		wParam == VK_OEM_1 ||          // ;
        wParam == VK_OEM_2 ||          // /
		wParam == VK_OEM_3 ||          // `
        wParam == VK_OEM_5 ||          // \ (中文下常作顿号)
        wParam == VK_OEM_4 ||          // [
        wParam == VK_OEM_6 ||          // ]
		wParam == VK_OEM_7 ||          // '
        wParam == VK_OEM_PLUS ||       // =
        wParam == VK_OEM_MINUS;        // -
    if (_isOnlyCtrlShift && isInputKey)_isOnlyCtrlShift = FALSE;
    if (wParam == VK_SHIFT) {
        _isOnlyCtrlShift = TRUE;
        BOOL isOnlyShift = (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0;
        if (_isOnlyCtrlShift && !isOnlyShift)_isOnlyCtrlShift = FALSE;
        if (isOnlyShift) {
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            return S_OK;
        }
    }
    // 检测单独的左 Ctrl 键按下
    if (wParam == VK_CONTROL)
    {
        _isOnlyCtrlShift = TRUE;
        // 检查扩展键标志：0 = 左 Ctrl, 1 = 右 Ctrl
        BOOL isLeftCtrl = ((lParam >> 24) & 1) == 0;
        BOOL isRightCtrl = ((lParam >> 24) & 1) == 1;
        // 检查是否只有 Ctrl 键被按下（没有其他修饰键）
        BOOL isOnlyCtrl = (GetKeyState(VK_SHIFT) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0;
        if (_isOnlyCtrlShift && !isOnlyCtrl)_isOnlyCtrlShift = FALSE;
        if (isLeftCtrl && isOnlyCtrl)
        {
            // 如果已经标记为按下，说明是重复触发的消息，直接跳过
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
           // OutputDebugString(L"左Ctrl 被按下了--------------------------------------------------------- ");
            // 记录按下的时间（毫秒）
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            *pIsEaten = TRUE;  // 可选：是否吃掉按键
            return S_OK;
        }
        if (isRightCtrl && isOnlyCtrl)
        {
            // 如果已经标记为按下，说明是重复触发的消息，直接跳过
            if (_isCtrlOrShiftDown)
            {
                *pIsEaten = TRUE;   // 可选：吃掉重复按键
                return S_OK;
            }
            //OutputDebugString(L"右Ctrl 被按下了--------------------------------------------------------- ");
            // 记录按下的时间（毫秒）
            _isCtrlOrShiftDown = TRUE;
            _ctrlKeyDownTime = GetTickCount64();
            *pIsEaten = TRUE;  // 可选：是否吃掉按键
            return S_OK;
        }
    }
    _KEYSTROKE_STATE KeystrokeState;
    WCHAR wch = '\0';
    UINT code = 0;

    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, &KeystrokeState);

    if (*pIsEaten)
    {
        bool needInvokeKeyHandler = true;
        //
        // Invoke key handler edit session
        //
        if (code == VK_ESCAPE)
        {
            KeystrokeState.Category = CATEGORY_COMPOSING;
        }

        // Always eat THIRDPARTY_NEXTPAGE and THIRDPARTY_PREVPAGE keys, but don't always process them.
        if ((wch == THIRDPARTY_NEXTPAGE) || (wch == THIRDPARTY_PREVPAGE))
        {
            needInvokeKeyHandler = !((KeystrokeState.Category == CATEGORY_NONE) && (KeystrokeState.Function == FUNCTION_NONE));
        }

        if (needInvokeKeyHandler)
        {
            _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
        }
    }
    else if (KeystrokeState.Category == CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION)
    {
        // Invoke key handler edit session
        KeystrokeState.Category = CATEGORY_COMPOSING;
        _InvokeKeyHandler(pContext, code, wch, (DWORD)lParam, KeystrokeState);
    }

    return S_OK;
}

void CSampleIME::OnChangeWubiOrPying(ITfContext* pContext)
{
    // 1. 先关闭候选窗口并提交当前组合文本（如果有）
    if (_pCandidateListUIPresenter != nullptr && pContext != nullptr)
    {
        CCloseCandidateAndCommitEditSession* pEditSession = new CCloseCandidateAndCommitEditSession(this, pContext);
        if (pEditSession)
        {
            HRESULT hr = S_OK;
            pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
            pEditSession->Release();
        }
    }
    // 2. 切换五笔/拼音模式/混合模式
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
	//Global::nMaxHorizontalItems = Global::isPinyinMode ? 10 : 5; // 水平模式显示5个候选，竖直模式显示9个候选
	//Global::nMaxVerticalItems = 10 ; // 水平模式显示10个候选，竖直模式显示5个候选
    //Global::isHorizontalMode = Global::isPinyinMode && Global::isPyAndWbMode || !Global::isPinyinMode;
    // 3. 确保切换到中文模式
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    BOOL isOpen = FALSE;
    CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);
    if (!isOpen)
    {
        CompartmentKeyboardOpen._SetCompartmentBOOL(TRUE);
        Global::isChineseMode = TRUE;
    }

    // 4. 重新加载词典并刷新语言栏图标
    if (_pCompositionProcessorEngine)
    {
        _pCompositionProcessorEngine->ChangeWborPinyinDictionary();
    }
}
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnTestKeyUp
//
// Called by the system to query this service wants a potential keystroke.
//----------------------------------------------------------------------------
STDAPI CSampleIME::OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pIsEaten)
{
    if (pIsEaten == nullptr) return E_INVALIDARG;
    // 仅更新修饰键状态，其他什么都不做
    // 直接调用 OnKeyUp（它会检查标记并执行切换）
    HRESULT hr = OnKeyUp(pContext, wParam, lParam, pIsEaten);
    // 注意：OnKeyUp 会设置 *pIsEaten，我们直接返回它的结果
    return hr;
}
//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnKeyUp
//
// Called by the system to offer this service a keystroke.  If *pIsEaten == TRUE
// on exit, the application will not handle the keystroke.
//----------------------------------------------------------------------------
STDAPI CSampleIME::OnKeyUp(ITfContext *pContext, WPARAM wParam, LPARAM lParam, BOOL *pIsEaten)
{
    Global::UpdateModifiers(wParam, lParam);

    if (wParam == VK_SHIFT) {
        UINT scanCode = (lParam >> 16) & 0xFF;
        BOOL isRightShift = (scanCode == 0x36);
        BOOL isLeftShift = (scanCode == 0x2A);
        BOOL isOnlyShift = (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0;
        if (isOnlyShift && _isCtrlOrShiftDown && _isOnlyCtrlShift)
        {
            ULONGLONG duration = GetTickCount64() - _ctrlKeyDownTime;
            //WCHAR msg[256];
            //swprintf_s(msg, ARRAYSIZE(msg), L"OnKeyUp左 Shift 弹起，按下时长：%u 毫秒", (unsigned int)duration);
            //OutputDebugString(msg);
            if (duration < 400) { // 短按
                if (isLeftShift) {
                    WCHAR szSoundPath[MAX_PATH];
                    PathCombine(szSoundPath, g_szDllPath, L"sound\\letter.wav");
                    PlaySound(szSoundPath, NULL, SND_FILENAME | SND_ASYNC);
                    OnChangeWubiOrPying(pContext);
                }
                else if (isRightShift) {
                    OnRightShift(pContext);
                }
            }
            // 长按可忽略或添加额外处理
        }
        // ★★★ 关键：无论是否切换，都清除标记 ★★★
        _isCtrlOrShiftDown = FALSE;
        _isOnlyCtrlShift = FALSE;
        *pIsEaten = TRUE;
        return S_OK;
    }
    if (wParam == VK_CONTROL)
    {
        BOOL isLeftCtrl = ((lParam >> 24) & 1) == 0;
        BOOL isRightCtrl = ((lParam >> 24) & 1) == 1;
        BOOL isOnlyCtrl = (GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetKeyState(VK_MENU) & 0x8000) == 0;
        if (isOnlyCtrl && _isCtrlOrShiftDown && _isOnlyCtrlShift)
        {
            ULONGLONG duration = GetTickCount64() - _ctrlKeyDownTime;
            if (isLeftCtrl) {
                if (duration < 400) {
                    Beep(800, 100);
                    OnChangeWubiOrPying(pContext);
                }
                else {
                    // 长按左 Ctrl 处理（可选）
                }
            }
            else if (isRightCtrl) {
                if (duration < 400) {
                    // 短按右 Ctrl：切换水平/垂直模式
                    Global::isHorizontalMode = !Global::isHorizontalMode;
                    Beep(1600, 100);
                }
                else {
                    // 长按右 Ctrl：显示用户造词对话框
                    Beep(600, 100);
                    if (_pCompositionProcessorEngine)
                        _pCompositionProcessorEngine->ShowUserWordDialog();
                }
            }
        }
        // ★ 清除所有相关标记
        _isCtrlOrShiftDown = FALSE;
        _isOnlyCtrlShift = FALSE;
        *pIsEaten = TRUE;
        return S_OK;
    }
   _ctrlKeyDownTime = 0;
    WCHAR wch = '\0';
    UINT code = 0;
    *pIsEaten = _IsKeyEaten(pContext, (UINT)wParam, &code, &wch, NULL);
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink::OnPreservedKey
//
// Called when a hotkey (registered by us, or by the system) is typed.
//----------------------------------------------------------------------------

STDAPI CSampleIME::OnPreservedKey(ITfContext *pContext, REFGUID rguid, BOOL *pIsEaten)
{
    pContext;
    CCompositionProcessorEngine *pCompositionProcessorEngine;
    pCompositionProcessorEngine = _pCompositionProcessorEngine;
	if (IsEqualGUID(rguid, Global::SampleIMEGuidUserWordPreserveKey))//是用户词典的保留键
    {
        OutputDebugString(L"您按下了热键 Ctrl + W  ------------------------下面将手工造词-------------------T ");
        if (_pCompositionProcessorEngine)
        {
            _pCompositionProcessorEngine->ShowUserWordDialog();
        }
        *pIsEaten = TRUE;   // 吃掉该按键，不传给应用程序
        return S_OK;
    }
    // 如果是 IME 模式切换的保留键，并且是右Shift，同时候选窗口存在
    //if (IsEqualGUID(rguid, Global::SampleIMEGuidImeModePreserveKey) )
    //{
    //    // 记录当前修饰键状态，以判断是否是右Shift
    //    //USHORT modifiers = Global::ModifiersValue;
    //    BOOL isRightShift = m_bLastShiftIsRight;
    //    BOOL isLeftShift = m_bLastShiftIsLeft;

    //    OutputDebugString(Global::isChineseMode ? L"00-----中文--------- Global::isChineseMode--------------T " : L"00 ------英文-------- Global::isChineseMode--------F ");
    //    OutputDebugString(isRightShift ? L"00是 IME 模式切换的保留键----------------isRightShift------------------T " : L"00 是 IME 模式切换的保留键--------------isRightShift---------F ");
    //    OutputDebugString(isLeftShift ? L"00是 IME 模式切换的保留键----------------isLeftShift------------------T " : L"00 是 IME 模式切换的保留键--------------isLeftShift---------F ");
    //    if (isRightShift) {
    //        pCompositionProcessorEngine->OnPreservedKey(rguid, pIsEaten, _GetThreadMgr(), _GetClientId());// 处理右 Shift 切换中英文模式的逻辑
    //        // 右 Shift：切换中英文模式
    //        // 注意：此时 compartment 的值已经被 TSF 自动切换了（因为这是一个保留键）
    //        // 所以只需读取当前状态并更新全局变量
    //        CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    //        BOOL isOpen = FALSE;
    //        CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);
    //        Global::isChineseMode = isOpen;

    //        OutputDebugString(L"右 Shift 切换中英文模式，当前状态: ");
    //        OutputDebugString(Global::isChineseMode ? L"中文" : L"英文");
    //        if (_pCandidateListUIPresenter != nullptr && pContext != nullptr)
    //        {
    //            OutputDebugString(isRightShift ? L"11isRightShift -------------------------------------------T " : L"System dict path: isRightShift:-----------------F ");
    //            // 创建 EditSession，关闭候选窗口并提交当前组合文本
    //            CCloseCandidateAndCommitEditSession* pEditSession =
    //                new (std::nothrow) CCloseCandidateAndCommitEditSession(this, pContext);
    //            if (pEditSession)
    //            {
    //                HRESULT hr = S_OK;
    //                OutputDebugString(isRightShift ? L"44  您按下的是 : isRightShift ---------将执行切换英文，所输入字母上屏-----------T " : L" isRightShift值是假:-----------------F ");
    //                pContext->RequestEditSession(_tfClientId, pEditSession,
    //                    TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    //                pEditSession->Release();
    //            }
    //        }
    //        if (_pCompositionProcessorEngine)
    //        {
    //            _pCompositionProcessorEngine->RefreshToolBarIcon();// 刷新语言栏图标
    //        }
    //    }
    //    else if (isLeftShift)
    //    {
    //         OutputDebugString(isLeftShift ? L"isLeftShift -------------------------------------------T " : L" isLeftShift:-----------------F ");
    //         // 左 Shift：切换五笔/拼音模式（你自己的逻辑）
    //         // 不影响 Global::isChineseMode
    //         
    //    }
    //}
    return S_OK;
}
STDAPI CSampleIME::OnRightShift(ITfContext* pContext)
{
    class CCommitRawAndSwitchEditSession : public CEditSessionBase
    {
    public:
        CCommitRawAndSwitchEditSession(CSampleIME* pTS, ITfContext* pCtx, BOOL bNewMode)
            : CEditSessionBase(pTS, pCtx), _bNewMode(bNewMode) {
        }

        STDMETHODIMP DoEditSession(TfEditCookie ec) override
        {
            // 提交组合中的原始文本（不提交候选词）
            _pTextService->_HandleCommitRawText(ec, _pContext);

            // 切换中英文模式
            CCompartment CompartmentKeyboardOpen(
                _pTextService->_GetThreadMgr(),
                _pTextService->_GetClientId(),
                GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
            CompartmentKeyboardOpen._SetCompartmentBOOL(_bNewMode);
            Global::isChineseMode = _bNewMode;

            return S_OK;
        }
    private:
        BOOL _bNewMode;
    };

    BOOL isOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);
    BOOL newMode = !isOpen;
    if (newMode) {
        WCHAR szSoundPath[MAX_PATH];
        PathCombine(szSoundPath, g_szDllPath, L"sound\\kong.wav");
        PlaySound(szSoundPath, NULL, SND_FILENAME | SND_ASYNC);
    }
    else Beep(1200,200);
    if (pContext)
    {
        CCommitRawAndSwitchEditSession* pEditSession =
            new CCommitRawAndSwitchEditSession(this, pContext, newMode);
        if (pEditSession)
        {
            HRESULT hr = S_OK;
            pContext->RequestEditSession(_tfClientId, pEditSession,
                TF_ES_SYNC | TF_ES_READWRITE, &hr);
            pEditSession->Release();
        }
    }
    else
    {
        CompartmentKeyboardOpen._SetCompartmentBOOL(newMode);
        Global::isChineseMode = newMode;
    }

    if (_pCompositionProcessorEngine)
        _pCompositionProcessorEngine->RefreshToolBarIcon();

    return S_OK;
}
//+---------------------------------------------------------------------------
//
// _InitKeyEventSink
//
// Advise a keystroke sink.
//----------------------------------------------------------------------------

BOOL CSampleIME::_InitKeyEventSink()
{
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    HRESULT hr = S_OK;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        return FALSE;
    }

    hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink *)this, TRUE);

    pKeystrokeMgr->Release();

    return (hr == S_OK);
}

//+---------------------------------------------------------------------------
//
// _UninitKeyEventSink
//
// Unadvise a keystroke sink.  Assumes we have advised one already.
//----------------------------------------------------------------------------

void CSampleIME::_UninitKeyEventSink()
{
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;

    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        return;
    }

    pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);

    pKeystrokeMgr->Release();
}
