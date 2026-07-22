// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "SampleIME.h"
#include "CompositionProcessorEngine.h"
#include "TableDictionaryEngine.h"
#include "DictionarySearch.h"
#include "TfInputProcessorProfile.h"
#include "Globals.h"
#include "Compartment.h"
#include "LanguageBar.h"
#include "RegKey.h"

//////////////////////////////////////////////////////////////////////
//
// CSampleIME implementation.
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// _AddTextProcessorEngine
//
//----------------------------------------------------------------------------

BOOL CSampleIME::_AddTextProcessorEngine()
{
    LANGID langid = 0;
    CLSID clsid = GUID_NULL;
    GUID guidProfile = GUID_NULL;

    // Get default profile.
    CTfInputProcessorProfile profile;

    if (FAILED(profile.CreateInstance()))
    {
        return FALSE;
    }

    if (FAILED(profile.GetCurrentLanguage(&langid)))
    {
        return FALSE;
    }

    if (FAILED(profile.GetDefaultLanguageProfile(langid, GUID_TFCAT_TIP_KEYBOARD, &clsid, &guidProfile)))
    {
        return FALSE;
    }

    // Is this already added?
    if (_pCompositionProcessorEngine != nullptr)
    {
        LANGID langidProfile = 0;
        GUID guidLanguageProfile = GUID_NULL;

        guidLanguageProfile = _pCompositionProcessorEngine->GetLanguageProfile(&langidProfile);
        if ((langid == langidProfile) && IsEqualGUID(guidProfile, guidLanguageProfile))
        {
            return TRUE;
        }
    }

    // Create composition processor engine
    if (_pCompositionProcessorEngine == nullptr)
    {
        _pCompositionProcessorEngine = new (std::nothrow) CCompositionProcessorEngine(this);
    }
    if (!_pCompositionProcessorEngine)
    {
        return FALSE;
    }

    // setup composition processor engine
    if (FALSE == _pCompositionProcessorEngine->SetupLanguageProfile(langid, guidProfile, _GetThreadMgr(), _GetClientId(), _IsSecureMode(), _IsComLess()))
    {
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////
//
// CompositionProcessorEngine implementation.
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// ctor
//
//----------------------------------------------------------------------------

CCompositionProcessorEngine::CCompositionProcessorEngine(CSampleIME* pTextService)
    : _pThreadMgr(nullptr), _pTextService(pTextService)   // 保存指针   // 初始化为空
{
    _pTableDictionaryEngine = nullptr;
    _pDictionaryFile = nullptr;
    _pUserDictionaryFile = nullptr;
    _pUserDictionaryEngine = nullptr;
    _pWubiDictionaryEngine = nullptr;
    _pWubiDictionaryFile = nullptr;
    _pWubiUserDictionaryEngine = nullptr;
    _pWubiUserDictionaryFile = nullptr;
    _langid = 0xffff;
    _guidProfile = GUID_NULL;
    _tfClientId = TF_CLIENTID_NULL;

    _pLanguageBar_IMEMode = nullptr;
    _pLanguageBar_DoubleSingleByte = nullptr;
    _pLanguageBar_Punctuation = nullptr;

    _pCompartmentConversion = nullptr;
    _pCompartmentKeyboardOpenEventSink = nullptr;
    _pCompartmentConversionEventSink = nullptr;
    _pCompartmentDoubleSingleByteEventSink = nullptr;
    _pCompartmentPunctuationEventSink = nullptr;

    _hasWildcardIncludedInKeystrokeBuffer = FALSE;

    _isWildcard = FALSE;
    _isDisableWildcardAtFirst = FALSE;
    _hasMakePhraseFromText = FALSE;
    _isKeystrokeSort = FALSE;
    _fFullWidthPunctuation = TRUE;
    _candidateListPhraseModifier = 0;

    _candidateWndWidth = CAND_WIDTH;
	//Global::isPyAndWbMode = TRUE;
    InitKeyStrokeTable();
    LoadConfig();
    RefreshLanguageBarIcon();
}

//+---------------------------------------------------------------------------
//
// dtor
//
//----------------------------------------------------------------------------

CCompositionProcessorEngine::~CCompositionProcessorEngine()
{
    if (_pTableDictionaryEngine)
    {
        delete _pTableDictionaryEngine;
        _pTableDictionaryEngine = nullptr;
    }
    if (_pUserDictionaryEngine)
    {
        delete _pUserDictionaryEngine;
		_pUserDictionaryEngine = nullptr;
    }
    if (_pUserDictionaryFile)
    {
        delete _pUserDictionaryFile;
		_pUserDictionaryFile = nullptr;
    }
    if (_pWubiDictionaryEngine)
    {
        delete _pWubiDictionaryEngine;
        _pWubiDictionaryEngine = nullptr;
    }
    if (_pWubiDictionaryFile)
    {
        delete _pWubiDictionaryFile;
        _pWubiDictionaryFile = nullptr;
    }
    if (_pLanguageBar_IMEMode)
    {
        _pLanguageBar_IMEMode->CleanUp();
        _pLanguageBar_IMEMode->Release();
        _pLanguageBar_IMEMode = nullptr;
    }
    if (_pLanguageBar_DoubleSingleByte)
    {
        _pLanguageBar_DoubleSingleByte->CleanUp();
        _pLanguageBar_DoubleSingleByte->Release();
        _pLanguageBar_DoubleSingleByte = nullptr;
    }
    if (_pLanguageBar_Punctuation)
    {
        _pLanguageBar_Punctuation->CleanUp();
        _pLanguageBar_Punctuation->Release();
        _pLanguageBar_Punctuation = nullptr;
    }

    if (_pCompartmentConversion)
    {
        delete _pCompartmentConversion;
        _pCompartmentConversion = nullptr;
    }
    if (_pCompartmentKeyboardOpenEventSink)
    {
        _pCompartmentKeyboardOpenEventSink->_Unadvise();
        delete _pCompartmentKeyboardOpenEventSink;
        _pCompartmentKeyboardOpenEventSink = nullptr;
    }
    if (_pCompartmentConversionEventSink)
    {
        _pCompartmentConversionEventSink->_Unadvise();
        delete _pCompartmentConversionEventSink;
        _pCompartmentConversionEventSink = nullptr;
    }
    if (_pCompartmentDoubleSingleByteEventSink)
    {
        _pCompartmentDoubleSingleByteEventSink->_Unadvise();
        delete _pCompartmentDoubleSingleByteEventSink;
        _pCompartmentDoubleSingleByteEventSink = nullptr;
    }
    if (_pCompartmentPunctuationEventSink)
    {
        _pCompartmentPunctuationEventSink->_Unadvise();
        delete _pCompartmentPunctuationEventSink;
        _pCompartmentPunctuationEventSink = nullptr;
    }

    if (_pDictionaryFile)
    {
        delete _pDictionaryFile;
        _pDictionaryFile = nullptr;
    }
    // ... 原有释放代码 ...
    if (_pThreadMgr)
    {
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
}
// 解析热键字符串，返回 uVKey 和 uModifiers
static BOOL ParseHotKeyString(LPCWSTR pszString, UINT* puVKey, UINT* puModifiers)
{
    if (!pszString || !*pszString || !puVKey || !puModifiers)
        return FALSE;

    // 1. 复制一份可修改的字符串（因为 wcstok_s 会修改内容）
    WCHAR szCopy[256] = { 0 };
    if (wcslen(pszString) >= ARRAYSIZE(szCopy))
        return FALSE;   // 防止溢出
    wcscpy_s(szCopy, ARRAYSIZE(szCopy), pszString);

    // 2. 检测是否包含 '+'（组合键）
    if (wcsstr(szCopy, L"+") == nullptr)
    {
        // 单独一个修饰键（如 "Shift"、"Ctrl"、"Alt"）
        if (_wcsicmp(szCopy, L"Shift") == 0)
        {
            *puVKey = VK_SHIFT;
            *puModifiers = _TF_MOD_ON_KEYUP_SHIFT_ONLY;
            return TRUE;
        }
        else if (_wcsicmp(szCopy, L"Ctrl") == 0 || _wcsicmp(szCopy, L"Control") == 0)
        {
            *puVKey = VK_CONTROL;
            *puModifiers = _TF_MOD_ON_KEYUP_CONTROL_ONLY;
            return TRUE;
        }
        else if (_wcsicmp(szCopy, L"Alt") == 0)
        {
            *puVKey = VK_MENU;
            *puModifiers = _TF_MOD_ON_KEYUP_ALT_ONLY;
            return TRUE;
        }
        else
        {
            return FALSE;   // 单个非修饰键不支持
        }
    }
    else
    {
        // 3. 组合键：用最后一个 '+' 分割，前面是修饰键，后面是虚拟键
        WCHAR* pLastPlus = wcsrchr(szCopy, L'+');
        if (!pLastPlus)
            return FALSE;

        *pLastPlus = L'\0';                     // 分割字符串
        WCHAR* pszMods = szCopy;               // 修饰键部分（可修改）
        LPCWSTR pszVKey = pLastPlus + 1;       // 虚拟键部分（只读）

        // 4. 解析修饰键
        UINT modifiers = 0;
        WCHAR* pNextToken = nullptr;
        WCHAR* pToken = wcstok_s(pszMods, L"+", &pNextToken);   // 现在 pszMods 是 wchar_t*，正确
        while (pToken)
        {
            if (_wcsicmp(pToken, L"Shift") == 0)
                modifiers |= TF_MOD_SHIFT;
            else if (_wcsicmp(pToken, L"Ctrl") == 0 || _wcsicmp(pToken, L"Control") == 0)
                modifiers |= TF_MOD_CONTROL;
            else if (_wcsicmp(pToken, L"Alt") == 0)
                modifiers |= TF_MOD_ALT;
            else
                return FALSE;
            pToken = wcstok_s(nullptr, L"+", &pNextToken);
        }

        // 5. 解析虚拟键
        UINT vKey = 0;
        if (wcslen(pszVKey) == 1)
        {
            WCHAR ch = pszVKey[0];
            if (ch >= L'a' && ch <= L'z')
                vKey = ch - L'a' + L'A';
            else if (ch >= L'A' && ch <= L'Z')
                vKey = ch;
            else if (ch >= L'0' && ch <= L'9')
                vKey = ch;
            else if (ch == L'.')
                vKey = VK_OEM_PERIOD;
            else if (ch == L',')
                vKey = VK_OEM_COMMA;
            else
                return FALSE;
        }
        else
        {
            // 特殊键名
            if (_wcsicmp(pszVKey, L"Space") == 0)          vKey = VK_SPACE;
            else if (_wcsicmp(pszVKey, L"Tab") == 0)       vKey = VK_TAB;
            else if (_wcsicmp(pszVKey, L"Enter") == 0)     vKey = VK_RETURN;
            else if (_wcsicmp(pszVKey, L"Escape") == 0 || _wcsicmp(pszVKey, L"Esc") == 0)
                vKey = VK_ESCAPE;
            else if (_wcsicmp(pszVKey, L"Back") == 0 || _wcsicmp(pszVKey, L"Backspace") == 0)
                vKey = VK_BACK;
            else if (_wcsicmp(pszVKey, L"Up") == 0)        vKey = VK_UP;
            else if (_wcsicmp(pszVKey, L"Down") == 0)      vKey = VK_DOWN;
            else if (_wcsicmp(pszVKey, L"Left") == 0)      vKey = VK_LEFT;
            else if (_wcsicmp(pszVKey, L"Right") == 0)     vKey = VK_RIGHT;
            else if (_wcsicmp(pszVKey, L"PageUp") == 0)    vKey = VK_PRIOR;
            else if (_wcsicmp(pszVKey, L"PageDown") == 0)  vKey = VK_NEXT;
            else if (_wcsicmp(pszVKey, L"Home") == 0)      vKey = VK_HOME;
            else if (_wcsicmp(pszVKey, L"End") == 0)       vKey = VK_END;
            else if (_wcsicmp(pszVKey, L"Period") == 0)    vKey = VK_OEM_PERIOD;
            else if (_wcsicmp(pszVKey, L"Comma") == 0)     vKey = VK_OEM_COMMA;
            else
                return FALSE;
        }

        if (vKey == 0)
            return FALSE;

        *puVKey = vKey;
        *puModifiers = modifiers;
        return TRUE;
    }
}
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
void CCompositionProcessorEngine::LoadConfig()
{
    WCHAR szConfigPath[MAX_PATH] = { 0 };
    if (!g_bConfigLoaded) {
        GetModuleFileName(Global::dllInstanceHandle, g_szDllPath, MAX_PATH);
        WCHAR* pLastSlash = wcsrchr(g_szDllPath, L'\\');
        if (pLastSlash) *(pLastSlash + 1) = L'\0';
        OutputDebugString(L"get dll path is :\n");
        OutputDebugString(g_szDllPath);
    }
    PathCombine(szConfigPath, g_szDllPath, L"xywb.ini");
    // 检查配置文件是否存在，若不存在则直接返回（保留全局变量默认值）
    if (GetFileAttributes(szConfigPath) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugString(L"xywb.ini not found, using default config.\n");
        return;
    }
    // 读取各项配置，若失败则保留全局变量的默认值

    WCHAR szBuffer[64] = { 0 };
    if (!g_bConfigLoaded){
        if (GetPrivateProfileString(L"Settings", L"ShowToolsBar", L"0", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
               g_isVisibleToolBar = (_wtoi(szBuffer) != 0);
        if (GetPrivateProfileString(L"Settings", L"HorizontalToolsBar", L"1", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
            g_isHToolbarWin = (_wtoi(szBuffer) != 0);
         g_bConfigLoaded = TRUE;
    }
    // PinyinMode
    if (GetPrivateProfileString(L"Settings", L"PinyinMode", L"0", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
    {
        Global::isPinyinMode = (_wtoi(szBuffer) != 0);
    }
    // PyAndWbMode
    if (GetPrivateProfileString(L"Settings", L"PyAndWbMode", L"1", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
    {
        Global::isPyAndWbMode = (_wtoi(szBuffer) != 0);
    }
    // ChineseMode
    if (GetPrivateProfileString(L"Settings", L"ChineseMode", L"1", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
    {
        Global::isChineseMode = (_wtoi(szBuffer) != 0);
    }
    // HorizontalMode
    if (GetPrivateProfileString(L"Settings", L"HorizontalMode", L"1", szBuffer, ARRAYSIZE(szBuffer), szConfigPath) > 0)
    {
        Global::isHorizontalMode = (_wtoi(szBuffer) != 0);
    }
    Global::nMaxHorizontalItems = GetPrivateProfileInt(L"Settings", L"MaxHorizontalItems", 5, szConfigPath);
    Global::nMaxVerticalItems = GetPrivateProfileInt(L"Settings", L"MaxVerticalItems", 10, szConfigPath);
    // ---- 新增读取 ----
    // ShowRemainingCode
    Global::showRemainingCode = (GetPrivateProfileInt(L"Settings", L"ShowRemainingCode", 1, szConfigPath) != 0);

    // ShortcutKey（从 [HotKeys] 节读取 UserWordDialog）
    WCHAR szHotKey[64] = { 0 };
    if (GetPrivateProfileString(L"HotKeys", L"UserWordDialog", L"Ctrl+W", szHotKey, ARRAYSIZE(szHotKey), szConfigPath) > 0)
    {
        UINT vKey, mod;
        if (ParseHotKeyString(szHotKey, &vKey, &mod))
        {
            Global::shortcutKey.uVKey = vKey;
            Global::shortcutKey.uModifiers = mod;//控制键
        }
    }
    else
    {
        Global::shortcutKey.uVKey = L'W';
		Global::shortcutKey.uModifiers = TF_MOD_CONTROL;
    }

    // 颜色值（十六进制字符串，如 "0xE0EDF8" 或 "E0EDF8"）
    auto ReadColor = [&](LPCWSTR key, COLORREF defaultClr) -> COLORREF {
        WCHAR szColor[16] = { 0 };
        if (GetPrivateProfileString(L"Settings", key, L"", szColor, ARRAYSIZE(szColor), szConfigPath) > 0)
        {
            // 去掉可能的 "0x" 前缀
            if (wcsncmp(szColor, L"0x", 2) == 0)
                return (COLORREF)wcstoul(szColor + 2, NULL, 16);
            else
                return (COLORREF)wcstoul(szColor, NULL, 16);
        }
        return defaultClr;
        };

    Global::candidateBgColor = ReadColor(L"CandBgColor", Global::candidateBgColor);
    Global::candidateTextColor = ReadColor(L"CandTextColor", Global::candidateTextColor);
    Global::candidateSelectedBgColor = ReadColor(L"CandSelBgColor", Global::candidateSelectedBgColor);
    Global::candidateSelectedTextColor = ReadColor(L"CandSelTextColor", Global::candidateSelectedTextColor);
    Global::toolbarBgColor = ReadColor(L"ToolbarBgColor", Global::toolbarBgColor);
    Global::toolbarHoverColor = ReadColor(L"ToolbarHoverColor", Global::toolbarHoverColor);
    // 可选：输出调试信息
    OutputDebugString(L"Config loaded: ");
    OutputDebugString(Global::isPinyinMode ? L"Pinyin " : L"Wubi ");
    OutputDebugString(Global::isHorizontalMode ? L"Horizontal" : L"Vertical");
    OutputDebugString(L"\n");
}

//+---------------------------------------------------------------------------
//
// SetupLanguageProfile
//
// Setup language profile for Composition Processor Engine.
// param
//     [in] LANGID langid = Specify language ID
//     [in] GUID guidLanguageProfile - Specify GUID language profile which GUID is as same as Text Service Framework language profile.
//     [in] ITfThreadMgr - pointer ITfThreadMgr.
//     [in] tfClientId - TfClientId value.
//     [in] isSecureMode - secure mode
// returns
//     If setup succeeded, returns true. Otherwise returns false.
// N.B. For reverse conversion, ITfThreadMgr is NULL, TfClientId is 0 and isSecureMode is ignored.
//+---------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::SetupLanguageProfile(LANGID langid, REFGUID guidLanguageProfile, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, BOOL isSecureMode, BOOL isComLessMode)
{
    BOOL ret = TRUE;
    if ((tfClientId == 0) && (pThreadMgr == nullptr))
    {
        ret = FALSE;
        goto Exit;
    }
    // 保存线程管理器
    if (pThreadMgr)
    {
        _pThreadMgr = pThreadMgr;
        _pThreadMgr->AddRef();
    }
    _isComLessMode = isComLessMode;
    _langid = langid;
    _guidProfile = guidLanguageProfile;
    _tfClientId = tfClientId;

    SetupPreserved(pThreadMgr, tfClientId);	
	InitializeSampleIMECompartment(pThreadMgr, tfClientId);
    SetupPunctuationPair();
    SetupLanguageBar(pThreadMgr, tfClientId, isSecureMode);
    SetupKeystroke();
    SetupConfiguration();
    SetupDictionaryFile();
    RefreshLanguageBarIcon();
Exit:
    return ret;
}

//+---------------------------------------------------------------------------
//
// AddVirtualKey
// Add virtual key code to Composition Processor Engine for used to parse keystroke data.
// param
//     [in] uCode - Specify virtual key code.
// returns
//     State of Text Processor Engine.
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::AddVirtualKey(WCHAR wch)
{
    if (!wch)
    {
        return FALSE;
    }

    //
    // append one keystroke in buffer.
    //
    DWORD_PTR srgKeystrokeBufLen = _keystrokeBuffer.GetLength();
    PWCHAR pwch = new (std::nothrow) WCHAR[ srgKeystrokeBufLen + 1 ];
    if (!pwch)
    {
        return FALSE;
    }

    memcpy(pwch, _keystrokeBuffer.Get(), srgKeystrokeBufLen * sizeof(WCHAR));
    pwch[ srgKeystrokeBufLen ] = wch;

    if (_keystrokeBuffer.Get())
    {
        delete [] _keystrokeBuffer.Get();
    }

    _keystrokeBuffer.Set(pwch, srgKeystrokeBufLen + 1);

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// RemoveVirtualKey
// Remove stored virtual key code.
// param
//     [in] dwIndex   - Specified index.
// returns
//     none.
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::RemoveVirtualKey(DWORD_PTR dwIndex)
{
    DWORD_PTR srgKeystrokeBufLen = _keystrokeBuffer.GetLength();

    if (dwIndex + 1 < srgKeystrokeBufLen)
    {
        // shift following eles left
        memmove((BYTE*)_keystrokeBuffer.Get() + (dwIndex * sizeof(WCHAR)),
            (BYTE*)_keystrokeBuffer.Get() + ((dwIndex + 1) * sizeof(WCHAR)),
            (srgKeystrokeBufLen - dwIndex - 1) * sizeof(WCHAR));
    }

    _keystrokeBuffer.Set(_keystrokeBuffer.Get(), srgKeystrokeBufLen - 1);
}

//+---------------------------------------------------------------------------
//
// PurgeVirtualKey
// Purge stored virtual key code.
// param
//     none.
// returns
//     none.
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::PurgeVirtualKey()
{
    if (_keystrokeBuffer.Get())
    {
        delete [] _keystrokeBuffer.Get();
        _keystrokeBuffer.Set(NULL, 0);
    }
}

WCHAR CCompositionProcessorEngine::GetVirtualKey(DWORD_PTR dwIndex) 
{ 
    if (dwIndex < _keystrokeBuffer.GetLength())
    {
        return *(_keystrokeBuffer.Get() + dwIndex);
    }
    return 0;
}
//+---------------------------------------------------------------------------
//
// GetReadingStrings
// Retrieves string from Composition Processor Engine.
// param
//     [out] pReadingStrings - Specified returns pointer of CUnicodeString.
// returns
//     none
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::GetReadingStrings(_Inout_ CSampleImeArray<CStringRange> *pReadingStrings, _Out_ BOOL *pIsWildcardIncluded)
{
    CStringRange oneKeystroke;

    _hasWildcardIncludedInKeystrokeBuffer = FALSE;

    if (pReadingStrings->Count() == 0 && _keystrokeBuffer.GetLength())
    {
        CStringRange* pNewString = nullptr;

        pNewString = pReadingStrings->Append();
        if (pNewString)
        {
            *pNewString = _keystrokeBuffer;
        }

        for (DWORD index = 0; index < _keystrokeBuffer.GetLength(); index++)
        {
            oneKeystroke.Set(_keystrokeBuffer.Get() + index, 1);

            if (IsWildcard() && IsWildcardChar(*oneKeystroke.Get()))
            {
                _hasWildcardIncludedInKeystrokeBuffer = TRUE;
            }
        }
    }

    *pIsWildcardIncluded = _hasWildcardIncludedInKeystrokeBuffer;
}

//+---------------------------------------------------------------------------
//
// GetCandidateList
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::GetCandidateList(_Inout_ CSampleImeArray<CCandidateListItem>* pCandidateList,
    BOOL isIncrementalWordSearch, BOOL isWildcardSearch)
{
    //std::lock_guard<std::mutex> lock(_engineMutex);
    if (!IsDictionaryAvailable())
    {
        return;
    }

    // 假设调用方传入的是空列表，但为了安全，也可主动清空（视需求而定）。
    // 原代码不清空，依赖调用方传入新对象，这里保持相同行为。

    if (isIncrementalWordSearch)
    {
        CStringRange wildcardSearch;
        DWORD_PTR keystrokeBufLen = _keystrokeBuffer.GetLength() + 2;
        PWCHAR pwch = new (std::nothrow) WCHAR[keystrokeBufLen];
        if (!pwch)
        {
            return;
        }
        // 检查按键缓冲区是否已包含通配符
        DWORD wildcardIndex = 0;
        BOOL isFindWildcard = FALSE;

        if (IsWildcard())
        {
            for (wildcardIndex = 0; wildcardIndex < _keystrokeBuffer.GetLength(); wildcardIndex++)
            {
                if (IsWildcardChar(*(_keystrokeBuffer.Get() + wildcardIndex)))
                {
                    isFindWildcard = TRUE;
                    break;
                }
            }
        }

        StringCchCopyN(pwch, keystrokeBufLen, _keystrokeBuffer.Get(), _keystrokeBuffer.GetLength());

        if (!isFindWildcard)
        {
            // 增量搜索添加通配符
            StringCchCat(pwch, keystrokeBufLen, L"*");
        }

        size_t len = 0;
        if (StringCchLength(pwch, STRSAFE_MAX_CCH, &len) != S_OK)
        {
            delete[] pwch;
            return;
        }
        wildcardSearch.Set(pwch, len);
        UINT oldCount = 0;
        // ----- 修改点：先搜索用户词库（优先），再搜索系统词库 -----
        if (Global::isPinyinMode && Global::isPyAndWbMode) {
            // 五笔输入法增量搜索时，先搜索五笔词库
            if (_pWubiUserDictionaryEngine)
            {
                oldCount = pCandidateList->Count();
                _pWubiUserDictionaryEngine->CollectWordForWildcard(&wildcardSearch, pCandidateList);
                for (UINT i = oldCount; i < pCandidateList->Count(); ++i)
                    pCandidateList->GetAt(i)->_source = 2;   // 五笔用户
            }
            if (_pWubiDictionaryEngine)
            {
                oldCount = pCandidateList->Count();
                _pWubiDictionaryEngine->CollectWordForWildcard(&wildcardSearch, pCandidateList);
                for (UINT i = oldCount; i < pCandidateList->Count(); ++i)
                    pCandidateList->GetAt(i)->_source = 1;   // 五笔系统
			}
        }
        // 1. 用户词库
        if (_pUserDictionaryEngine)
        {
            _pUserDictionaryEngine->CollectWordForWildcard(&wildcardSearch, pCandidateList);
        }
        // 2. 系统词库
        if (_pTableDictionaryEngine)
        {
            _pTableDictionaryEngine->CollectWordForWildcard(&wildcardSearch, pCandidateList);
        }
        // ----------------------------------------------------
        if (0 >= pCandidateList->Count())
        {
            delete[] pwch;
            return;
        }

        if (IsKeystrokeSort())
        {
            // 对合并后的整个列表排序
            _pTableDictionaryEngine->SortListItemByFindKeyCode(pCandidateList);
        }
        // 截断每个候选词的 _FindKeyCode，去除用户已输入部分
        for (UINT index = 0; index < pCandidateList->Count(); index++)
        {
            CCandidateListItem* pLI = pCandidateList->GetAt(index);
            DWORD_PTR keystrokeBufferLen = 0;

            if (IsWildcard())
            {
                keystrokeBufferLen = wildcardIndex;
            }
            else
            {
                keystrokeBufferLen = _keystrokeBuffer.GetLength();
            }

            CStringRange newFindKeyCode;
            newFindKeyCode.Set(pLI->_FindKeyCode.Get() + keystrokeBufferLen,
                pLI->_FindKeyCode.GetLength() - keystrokeBufferLen);
            pLI->_FindKeyCode.Set(newFindKeyCode);
        }

        delete[] pwch;
    }
    else if (isWildcardSearch)
    {
        // 通配符搜索：用户优先
        if (_pUserDictionaryEngine)
        {
            _pUserDictionaryEngine->CollectWordForWildcard(&_keystrokeBuffer, pCandidateList);
        }
        if (_pTableDictionaryEngine)
        {
            _pTableDictionaryEngine->CollectWordForWildcard(&_keystrokeBuffer, pCandidateList);
        }
    }
    else
    {
        // 普通精确搜索：用户优先
        if (_pUserDictionaryEngine)
        {
            _pUserDictionaryEngine->CollectWord(&_keystrokeBuffer, pCandidateList);
        }
        if (_pTableDictionaryEngine)
        {
            _pTableDictionaryEngine->CollectWord(&_keystrokeBuffer, pCandidateList);
        }
    }
    // 原函数中遗留的无效循环，保留不变
    //for (UINT index = 0; index < pCandidateList->Count();)
    //{
    //    CCandidateListItem* pLI = pCandidateList->GetAt(index);
    //    CStringRange startItemString;
    //    CStringRange endItemString;

    //    startItemString.Set(pLI->_ItemString.Get(), 1);
    //    endItemString.Set(pLI->_ItemString.Get() + pLI->_ItemString.GetLength() - 1, 1);

    //    index++;
    //}
}

//+---------------------------------------------------------------------------
//
// GetCandidateStringInConverted
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::GetCandidateStringInConverted(CStringRange &searchString, _In_ CSampleImeArray<CCandidateListItem> *pCandidateList)
{
    //std::lock_guard<std::mutex> lock(_engineMutex);
    if (!IsDictionaryAvailable())
    {
        return;
    }

    // Search phrase from SECTION_TEXT's converted string list
    CStringRange wildcardSearch;
    DWORD_PTR srgKeystrokeBufLen = searchString.GetLength() + 2;
    PWCHAR pwch = new (std::nothrow) WCHAR[ srgKeystrokeBufLen ];
    if (!pwch)
    {
        return;
    }

    StringCchCopyN(pwch, srgKeystrokeBufLen, searchString.Get(), searchString.GetLength());
    StringCchCat(pwch, srgKeystrokeBufLen, L"*");

    // add wildcard char
	size_t len = 0;
	if (StringCchLength(pwch, STRSAFE_MAX_CCH, &len) != S_OK)
    {
        return;
    }
    wildcardSearch.Set(pwch, len);

    _pTableDictionaryEngine->CollectWordFromConvertedStringForWildcard(&wildcardSearch, pCandidateList);

    if (IsKeystrokeSort())
    {
        _pTableDictionaryEngine->SortListItemByFindKeyCode(pCandidateList);
    }

    wildcardSearch.Clear();
    delete [] pwch;
}

//+---------------------------------------------------------------------------
//
// IsPunctuation
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsPunctuation(WCHAR wch)
{
    for (int i = 0; i < ARRAYSIZE(Global::PunctuationTable); i++)
    {
        if (Global::PunctuationTable[i]._Code == wch)
        {
            return TRUE;
        }
    }
    // 检查成对符号（新增）
    if (wch == L'[' || wch == L'{' || wch == L']' || wch == L'}' ||
        wch == L'(' || wch == L')' || wch == L'<' || wch == L'>' ||
        wch == L'《' || wch == L'》' || wch == L'〈' || wch == L'〉' ||
        wch == L'‘' || wch == L'’' || wch == L'“' || wch == L'”' ||
        wch == L'［' || wch == L'｛' || 
        wch == L'（' || wch == L'）')
    {
        return TRUE;
    }
    for (UINT j = 0; j < _PunctuationPair.Count(); j++)
    {
        CPunctuationPair* pPuncPair = _PunctuationPair.GetAt(j);

        if (pPuncPair->_punctuation._Code == wch)
        {
            return TRUE;
        }
    }

    for (UINT k = 0; k < _PunctuationNestPair.Count(); k++)
    {
        CPunctuationNestPair* pPuncNestPair = _PunctuationNestPair.GetAt(k);

        if (pPuncNestPair->_punctuation_begin._Code == wch)
        {
            return TRUE;
        }
        if (pPuncNestPair->_punctuation_end._Code == wch)
        {
            return TRUE;
        }
    }
    return FALSE;
}

//+---------------------------------------------------------------------------
//
// GetPunctuationPair
//
//----------------------------------------------------------------------------

WCHAR CCompositionProcessorEngine::GetPunctuation(WCHAR wch)
{
    for (int i = 0; i < ARRAYSIZE(Global::PunctuationTable); i++)
    {
        if (Global::PunctuationTable[i]._Code == wch)
        {
            return Global::PunctuationTable[i]._Punctuation;
        }
    }

    for (UINT j = 0; j < _PunctuationPair.Count(); j++)
    {
        CPunctuationPair* pPuncPair = _PunctuationPair.GetAt(j);

        if (pPuncPair->_punctuation._Code == wch)
        {
            if (! pPuncPair->_isPairToggle)
            {
                pPuncPair->_isPairToggle = TRUE;
                return pPuncPair->_punctuation._Punctuation;
            }
            else
            {
                pPuncPair->_isPairToggle = FALSE;
                return pPuncPair->_pairPunctuation;
            }
        }
    }

    for (UINT k = 0; k < _PunctuationNestPair.Count(); k++)
    {
        CPunctuationNestPair* pPuncNestPair = _PunctuationNestPair.GetAt(k);

        if (pPuncNestPair->_punctuation_begin._Code == wch)
        {
            if (pPuncNestPair->_nestCount++ == 0)
            {
                return pPuncNestPair->_punctuation_begin._Punctuation;
            }
            else
            {
                return pPuncNestPair->_pairPunctuation_begin;
            }
        }
        if (pPuncNestPair->_punctuation_end._Code == wch)
        {
            if (--pPuncNestPair->_nestCount == 0)
            {
                return pPuncNestPair->_punctuation_end._Punctuation;
            }
            else
            {
                return pPuncNestPair->_pairPunctuation_end;
            }
        }
    }
    return 0;
}

//+---------------------------------------------------------------------------
//
// IsDoubleSingleByte
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsDoubleSingleByte(WCHAR wch)
{
    if (L' ' <= wch && wch <= L'~')
    {
        return TRUE;
    }
    return FALSE;
}

//+---------------------------------------------------------------------------
//
// SetupKeystroke
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetupKeystroke()
{
    SetKeystrokeTable(&_KeystrokeComposition);
    return;
}

//+---------------------------------------------------------------------------
//
// SetKeystrokeTable
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetKeystrokeTable(_Inout_ CSampleImeArray<_KEYSTROKE> *pKeystroke)
{
    for (int i = 0; i < 26; i++)
    {
        _KEYSTROKE* pKS = nullptr;

        pKS = pKeystroke->Append();
        if (!pKS)
        {
            break;
        }
        *pKS = _keystrokeTable[i];
    }
}


//+---------------------------------------------------------------------------
//
// SetupPreserved
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetupPreserved(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId)
{
	//注册热键 shift 键，按下时切换中英文输入模式
    //TF_PRESERVEDKEY preservedKeyImeMode;
    //preservedKeyImeMode.uVKey = VK_SHIFT;
    //preservedKeyImeMode.uModifiers = _TF_MOD_ON_KEYUP_SHIFT_ONLY;
    //SetPreservedKey(Global::SampleIMEGuidImeModePreserveKey, preservedKeyImeMode, Global::ImeModeDescription, &_PreservedKey_IMEMode);

    TF_PRESERVEDKEY preservedKeyDoubleSingleByte;
    preservedKeyDoubleSingleByte.uVKey = VK_SPACE;
    preservedKeyDoubleSingleByte.uModifiers = TF_MOD_SHIFT;
    SetPreservedKey(Global::SampleIMEGuidDoubleSingleBytePreserveKey, preservedKeyDoubleSingleByte, Global::DoubleSingleByteDescription, &_PreservedKey_DoubleSingleByte);

    TF_PRESERVEDKEY preservedKeyPunctuation;
    preservedKeyPunctuation.uVKey = VK_OEM_PERIOD;
    preservedKeyPunctuation.uModifiers = TF_MOD_CONTROL;
    SetPreservedKey(Global::SampleIMEGuidPunctuationPreserveKey, preservedKeyPunctuation, Global::PunctuationDescription, &_PreservedKey_Punctuation);

    // 新增：Ctrl+W 热键
    TF_PRESERVEDKEY preservedKeyUserWord;
    preservedKeyUserWord= Global::shortcutKey;                     // 虚拟键码 'W'
    SetPreservedKey(Global::SampleIMEGuidUserWordPreserveKey,
        preservedKeyUserWord,
        L"手工造词",
        &_PreservedKey_UserWord);
    InitPreservedKey(&_PreservedKey_IMEMode, pThreadMgr, tfClientId);
    InitPreservedKey(&_PreservedKey_DoubleSingleByte, pThreadMgr, tfClientId);
    InitPreservedKey(&_PreservedKey_Punctuation, pThreadMgr, tfClientId);
    InitPreservedKey(&_PreservedKey_UserWord, pThreadMgr, tfClientId);   // 新增
    return;
}

//+---------------------------------------------------------------------------
//
// SetKeystrokeTable
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetPreservedKey(const CLSID clsid, TF_PRESERVEDKEY & tfPreservedKey, _In_z_ LPCWSTR pwszDescription, _Out_ XPreservedKey *pXPreservedKey)
{
    pXPreservedKey->Guid = clsid;

    TF_PRESERVEDKEY *ptfPsvKey1 = pXPreservedKey->TSFPreservedKeyTable.Append();
    if (!ptfPsvKey1)
    {
        return;
    }
    *ptfPsvKey1 = tfPreservedKey;

	size_t srgKeystrokeBufLen = 0;
	if (StringCchLength(pwszDescription, STRSAFE_MAX_CCH, &srgKeystrokeBufLen) != S_OK)
    {
        return;
    }
    pXPreservedKey->Description = new (std::nothrow) WCHAR[srgKeystrokeBufLen + 1];
    if (!pXPreservedKey->Description)
    {
        return;
    }

    StringCchCopy((LPWSTR)pXPreservedKey->Description, srgKeystrokeBufLen, pwszDescription);

    return;
}
//+---------------------------------------------------------------------------
//
// InitPreservedKey
//
// Register a hot key.
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::InitPreservedKey(_In_ XPreservedKey *pXPreservedKey, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId)
{
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;

    if (IsEqualGUID(pXPreservedKey->Guid, GUID_NULL))
    {
        return FALSE;
    }

    if (pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) != S_OK)
    {
        return FALSE;
    }

    for (UINT i = 0; i < pXPreservedKey->TSFPreservedKeyTable.Count(); i++)
    {
        TF_PRESERVEDKEY preservedKey = *pXPreservedKey->TSFPreservedKeyTable.GetAt(i);
        preservedKey.uModifiers &= 0xffff;

		size_t lenOfDesc = 0;
		if (StringCchLength(pXPreservedKey->Description, STRSAFE_MAX_CCH, &lenOfDesc) != S_OK)
        {
            return FALSE;
        }
        pKeystrokeMgr->PreserveKey(tfClientId, pXPreservedKey->Guid, &preservedKey, pXPreservedKey->Description, static_cast<ULONG>(lenOfDesc));
    }

    pKeystrokeMgr->Release();

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// CheckShiftKeyOnly
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::CheckShiftKeyOnly(_In_ CSampleImeArray<TF_PRESERVEDKEY> *pTSFPreservedKeyTable)
{
    for (UINT i = 0; i < pTSFPreservedKeyTable->Count(); i++)
    {
        TF_PRESERVEDKEY *ptfPsvKey = pTSFPreservedKeyTable->GetAt(i);

        if (((ptfPsvKey->uModifiers & (_TF_MOD_ON_KEYUP_SHIFT_ONLY & 0xffff0000)) && !Global::IsShiftKeyDownOnly) ||
            ((ptfPsvKey->uModifiers & (_TF_MOD_ON_KEYUP_CONTROL_ONLY & 0xffff0000)) && !Global::IsControlKeyDownOnly) ||
            ((ptfPsvKey->uModifiers & (_TF_MOD_ON_KEYUP_ALT_ONLY & 0xffff0000)) && !Global::IsAltKeyDownOnly)         )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// OnPreservedKey
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::OnPreservedKey(REFGUID rguid, _Out_ BOOL *pIsEaten, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId)
{
    if (IsEqualGUID(rguid, _PreservedKey_IMEMode.Guid))
    {
        if (!CheckShiftKeyOnly(&_PreservedKey_IMEMode.TSFPreservedKeyTable))
        {
            *pIsEaten = FALSE;
            return;
        }
        BOOL isOpen = FALSE;
        CCompartment CompartmentKeyboardOpen(pThreadMgr, tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
        CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen);
        CompartmentKeyboardOpen._SetCompartmentBOOL(isOpen ? FALSE : TRUE);

        *pIsEaten = TRUE;
    }
    else if (IsEqualGUID(rguid, _PreservedKey_DoubleSingleByte.Guid))
    {
        if (!CheckShiftKeyOnly(&_PreservedKey_DoubleSingleByte.TSFPreservedKeyTable))
        {
            *pIsEaten = FALSE;
            return;
        }
        BOOL isDouble = FALSE;
        CCompartment CompartmentDoubleSingleByte(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
        CompartmentDoubleSingleByte._GetCompartmentBOOL(isDouble);
        CompartmentDoubleSingleByte._SetCompartmentBOOL(isDouble ? FALSE : TRUE);
        *pIsEaten = TRUE;
    }
    else if (IsEqualGUID(rguid, _PreservedKey_Punctuation.Guid))
    {
        if (!CheckShiftKeyOnly(&_PreservedKey_Punctuation.TSFPreservedKeyTable))
        {
            *pIsEaten = FALSE;
            return;
        }
        BOOL isPunctuation = FALSE;
        CCompartment CompartmentPunctuation(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
        CompartmentPunctuation._GetCompartmentBOOL(isPunctuation);
        CompartmentPunctuation._SetCompartmentBOOL(isPunctuation ? FALSE : TRUE);
        *pIsEaten = TRUE;
    }
    else
    {
        *pIsEaten = FALSE;
    }
    *pIsEaten = TRUE;
}

//+---------------------------------------------------------------------------
//
// SetupConfiguration
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetupConfiguration()
{
    _isWildcard = TRUE;
    _isDisableWildcardAtFirst = TRUE;
    _hasMakePhraseFromText = TRUE;
    _isKeystrokeSort = TRUE;
    _candidateWndWidth = CAND_WIDTH;

    SetInitialCandidateListRange();

    SetDefaultCandidateTextFont();

    return;
}

//+---------------------------------------------------------------------------
//
// SetupLanguageBar
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetupLanguageBar(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, BOOL isSecureMode)
{
    DWORD dwEnable = 1;
    CreateLanguageBarButton(dwEnable, GUID_LBI_INPUTMODE, Global::LangbarImeModeDescription, Global::ImeModeDescription, Global::ImeModeOnIcoIndex, Global::ImeModeOffIcoIndex, IDI_IME_MODE_ON_PINYIN, IDI_IME_MODE_ON_PYWB, &_pLanguageBar_IMEMode, isSecureMode, _pTextService);
    CreateLanguageBarButton(dwEnable, Global::SampleIMEGuidLangBarDoubleSingleByte, Global::LangbarDoubleSingleByteDescription, Global::DoubleSingleByteDescription, Global::DoubleSingleByteOnIcoIndex, Global::DoubleSingleByteOffIcoIndex, IDI_IME_MODE_ON_PINYIN, IDI_IME_MODE_ON_PYWB, &_pLanguageBar_DoubleSingleByte, isSecureMode, _pTextService);
    CreateLanguageBarButton(dwEnable, Global::SampleIMEGuidLangBarPunctuation, Global::LangbarPunctuationDescription, Global::PunctuationDescription, Global::PunctuationOnIcoIndex, Global::PunctuationOffIcoIndex, IDI_IME_MODE_ON_PINYIN, IDI_IME_MODE_ON_PYWB, &_pLanguageBar_Punctuation, isSecureMode, _pTextService);

    InitLanguageBar(_pLanguageBar_IMEMode, pThreadMgr, tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    InitLanguageBar(_pLanguageBar_DoubleSingleByte, pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    InitLanguageBar(_pLanguageBar_Punctuation, pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentPunctuation);

    _pCompartmentConversion = new (std::nothrow) CCompartment(pThreadMgr, tfClientId, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
    _pCompartmentKeyboardOpenEventSink = new (std::nothrow) CCompartmentEventSink(CompartmentCallback, this);
    _pCompartmentConversionEventSink = new (std::nothrow) CCompartmentEventSink(CompartmentCallback, this);
    _pCompartmentDoubleSingleByteEventSink = new (std::nothrow) CCompartmentEventSink(CompartmentCallback, this);
    _pCompartmentPunctuationEventSink = new (std::nothrow) CCompartmentEventSink(CompartmentCallback, this);

    if (_pCompartmentKeyboardOpenEventSink)
    {
        _pCompartmentKeyboardOpenEventSink->_Advise(pThreadMgr, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    }
    if (_pCompartmentConversionEventSink)
    {
        _pCompartmentConversionEventSink->_Advise(pThreadMgr, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
    }
    if (_pCompartmentDoubleSingleByteEventSink)
    {
        _pCompartmentDoubleSingleByteEventSink->_Advise(pThreadMgr, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    }
    if (_pCompartmentPunctuationEventSink)
    {
        _pCompartmentPunctuationEventSink->_Advise(pThreadMgr, Global::SampleIMEGuidCompartmentPunctuation);
    }
    OutputDebugString(g_isVisibleToolBar ? L"99切换输入法CCompositionProcessorEngine::SetupLanguageBar:  -------------------显示工具栏--g_isVisibleToolBar----------------------T ":
        L"99切换输入法CCompositionProcessorEngine::SetupLanguageBar:  ----------------g_isVisibleToolBar--------------------F ");
    //if (g_isVisibleToolBar)
    //{
    //    if (Global::hToolBarWnd && IsWindow(Global::hToolBarWnd))
    //    {
    //        OutputDebugString(L"01激活输入法CSampleIME::ActivateEx:  -------------------显示工具栏------------------------T ");
    //        ShowWindow(Global::hToolBarWnd, SW_SHOWNOACTIVATE);
    //    }
    //    else {
    //        OutputDebugString(L"02激活输入法CSampleIME::ActivateEx:  -------------------显示工具栏------------------------F ");
    //        if (_pLanguageBar_IMEMode) { //为什么下面总得不到执行
    //            OutputDebugString(L"03激活输入法CSampleIME::ActivateEx:  -------------------显示工具栏---_pLanguageBar_IMEMode----------------T ");
    //            _pLanguageBar_IMEMode->SetToolbarVisible(g_isVisibleToolBar);
    //        }
    //    }
    //}
    return;
}

//+---------------------------------------------------------------------------
//
// CreateLanguageBarButton
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::CreateLanguageBarButton(DWORD dwEnable, GUID guidLangBar, _In_z_ LPCWSTR pwszDescriptionValue, _In_z_ LPCWSTR pwszTooltipValue,
    DWORD dwOnIconIndex, DWORD dwOffIconIndex, DWORD dwOnIconIndexPinyin, DWORD dwOnIconIndexPinyinWubi, _Outptr_result_maybenull_ CLangBarItemButton **ppLangBarItemButton, BOOL isSecureMode, CSampleIME* pTextService)
{
	dwEnable;

    if (ppLangBarItemButton)
    {
        *ppLangBarItemButton = new (std::nothrow) CLangBarItemButton(guidLangBar, pwszDescriptionValue, pwszTooltipValue, dwOnIconIndex, dwOffIconIndex, dwOnIconIndexPinyin, dwOnIconIndexPinyinWubi, isSecureMode, pTextService);
    }

    return;
}

//+---------------------------------------------------------------------------
//
// InitLanguageBar
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::InitLanguageBar(_In_ CLangBarItemButton *pLangBarItemButton, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, REFGUID guidCompartment)
{
    if (pLangBarItemButton)
    {
        if (pLangBarItemButton->_AddItem(pThreadMgr) == S_OK)
        {
            if (pLangBarItemButton->_RegisterCompartment(pThreadMgr, tfClientId, guidCompartment))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//+---------------------------------------------------------------------------
//
// SetupDictionaryFile
//
//----------------------------------------------------------------------------
BOOL CCompositionProcessorEngine::SetupDictionaryFile()
{
    // std::lock_guard<std::mutex> lock(_engineMutex);
    return _SetupDictionaryFile();
}
void CCompositionProcessorEngine::ReloadDictionaries() {
    // 释放原有词典并重新加载
    //std::lock_guard<std::mutex> lock(_engineMutex);
    _SetupDictionaryFile();
}
BOOL CCompositionProcessorEngine::_SetupDictionaryFile()
{
    if (_pTableDictionaryEngine) delete _pTableDictionaryEngine;
    if (_pDictionaryFile) delete _pDictionaryFile;
    if (_pUserDictionaryEngine) delete _pUserDictionaryEngine;
    if (_pUserDictionaryFile) delete _pUserDictionaryFile;
    _pTableDictionaryEngine = nullptr;
    _pDictionaryFile = nullptr;
    _pUserDictionaryEngine = nullptr;
    _pUserDictionaryFile = nullptr;

    // 加载系统五笔词库（始终加载，用于查询编码）
    if (Global::isPinyinMode) {
        if (_pWubiDictionaryEngine) delete _pWubiDictionaryEngine;
        if (_pWubiDictionaryFile) delete _pWubiDictionaryFile;
        _pWubiDictionaryEngine = nullptr;
        _pWubiDictionaryFile = nullptr;
        _pWubiDictionaryFile = new CFileMapping();
        WCHAR szWubiPath[MAX_PATH];
        StringCchPrintf(szWubiPath, MAX_PATH, L"%s%s", g_szDllPath, TEXTSERVICE_DIC);
        if (_pWubiDictionaryFile->CreateFile(szWubiPath, GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ | FILE_SHARE_WRITE))
        {
            // 关键：调用 SetupReadBuffer 建立内存映射
            if (_pWubiDictionaryFile->SetupReadBuffer()) {
                _pWubiDictionaryEngine = new CTableDictionaryEngine(GetLocale(), _pWubiDictionaryFile);
            }
            else {
                delete _pWubiDictionaryFile;
                _pWubiDictionaryFile = nullptr;
            }
        }
        else
        {
            delete _pWubiDictionaryFile;
            _pWubiDictionaryFile = nullptr;
        }
        if (Global::isPyAndWbMode) {
        // 五笔输入法同时启用时， 也要加载五笔用户词库
            if (_pWubiUserDictionaryEngine) delete _pWubiUserDictionaryEngine;
            if (_pWubiUserDictionaryFile) delete _pWubiUserDictionaryFile;
            _pWubiUserDictionaryEngine = nullptr;
            _pWubiUserDictionaryFile = nullptr;
            _pWubiUserDictionaryFile = new CFileMapping();
            WCHAR szWubiUserPath[MAX_PATH];
            StringCchPrintf(szWubiUserPath, MAX_PATH, L"%s%s", g_szDllPath, TEXTSERVICE_UDIC);
            if (_pWubiUserDictionaryFile->CreateFile(szWubiUserPath, GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ | FILE_SHARE_WRITE))
            {
                // 关键：调用 SetupReadBuffer 建立内存映射
                if (_pWubiUserDictionaryFile->SetupReadBuffer()) {
                    _pWubiUserDictionaryEngine = new CTableDictionaryEngine(GetLocale(), _pWubiUserDictionaryFile);
                }
                else {
                    delete _pWubiUserDictionaryFile;
                    _pWubiUserDictionaryFile = nullptr;
                }
            }
            else
            {
                delete _pWubiUserDictionaryFile;
                _pWubiUserDictionaryFile = nullptr;
			}
        }
    }
    else {
        if (_pWubiUserDictionaryEngine) delete _pWubiUserDictionaryEngine;
        if (_pWubiUserDictionaryFile) delete _pWubiUserDictionaryFile;
        _pWubiUserDictionaryEngine = nullptr;
        _pWubiUserDictionaryFile = nullptr;
        if (_pWubiDictionaryEngine) delete _pWubiDictionaryEngine;
        if (_pWubiDictionaryFile) delete _pWubiDictionaryFile;
        _pWubiDictionaryEngine = nullptr;
        _pWubiDictionaryFile = nullptr;
            
    }
    // 2. 加载系统词库（原 TEXTSERVICE_DIC）
    WCHAR szSystemPath[MAX_PATH];
    StringCchPrintf(szSystemPath, MAX_PATH, L"%s%s", g_szDllPath, Global::isPinyinMode ? TEXTSERVICE_PYDIC :TEXTSERVICE_DIC);
    _pDictionaryFile = new (std::nothrow) CFileMapping();
    if (_pDictionaryFile && _pDictionaryFile->CreateFile(szSystemPath, GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ | FILE_SHARE_WRITE))
    {
        if (_pDictionaryFile->SetupReadBuffer()) // 添加此检查
        { 
            _pTableDictionaryEngine = new (std::nothrow) CTableDictionaryEngine(GetLocale(), _pDictionaryFile);
            if (!_pTableDictionaryEngine) { delete _pDictionaryFile; _pDictionaryFile = nullptr; }else {}
        }
        else
        {
            delete _pDictionaryFile;
            _pDictionaryFile = nullptr;
        }
    }
    else
    {
        delete _pDictionaryFile;
        _pDictionaryFile = nullptr;
    }
    // 3. 加载用户词库（新文件 xywb_user_words.txt）
    WCHAR szUserPath[MAX_PATH];
    StringCchPrintf(szUserPath, MAX_PATH, L"%s%s", g_szDllPath, Global::isPinyinMode ? TEXTSERVICE_PYUDIC :TEXTSERVICE_UDIC);
    _pUserDictionaryFile = new (std::nothrow) CFileMapping();
    if (_pUserDictionaryFile && _pUserDictionaryFile->CreateFile(szUserPath, GENERIC_READ, OPEN_EXISTING, FILE_SHARE_READ | FILE_SHARE_WRITE))
    { 
        if (_pUserDictionaryFile->SetupReadBuffer()) // 添加此检查
        {
             _pUserDictionaryEngine = new (std::nothrow) CTableDictionaryEngine(GetLocale(), _pUserDictionaryFile);
                if (!_pUserDictionaryEngine){delete _pUserDictionaryFile;_pUserDictionaryFile = nullptr;}
        }
        else
        {
            delete _pUserDictionaryFile;
            _pUserDictionaryFile = nullptr;
        }
    }
    else
    {
        delete _pUserDictionaryFile;
        _pUserDictionaryFile = nullptr;
    }
    if(_pTableDictionaryEngine != nullptr) OutputDebugString(Global::isPinyinMode ? (Global::isPyAndWbMode? L"加载拼音五笔混合词库成功 ---Global::isPinyinMode---T":L"加载拼音词库成功 ---Global::isPinyinMode---T")
        : L"加载五笔词库成功   ---Global::isPinyinMode-------F");
    // 返回 TRUE 只要系统词库存在即可（用户词库可选）
    return (_pTableDictionaryEngine != nullptr);
}

//+---------------------------------------------------------------------------
//
// GetDictionaryFile
//
//----------------------------------------------------------------------------

CFile* CCompositionProcessorEngine::GetDictionaryFile()
{
    return _pDictionaryFile;
}

//+---------------------------------------------------------------------------
//
// SetupPunctuationPair
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::SetupPunctuationPair()
{
    // Punctuation pair
    const int pair_count = 2;
    CPunctuationPair punc_quotation_mark(L'"', 0x201C, 0x201D);
    CPunctuationPair punc_apostrophe(L'\'', 0x2018, 0x2019);

    CPunctuationPair puncPairs[pair_count] = {
        punc_quotation_mark,
        punc_apostrophe,
    };

    for (int i = 0; i < pair_count; ++i)
    {
        CPunctuationPair *pPuncPair = _PunctuationPair.Append();
        *pPuncPair = puncPairs[i];
    }

    // Punctuation nest pair
    CPunctuationNestPair punc_angle_bracket(L'<', 0x300A, 0x3008, L'>', 0x300B, 0x3009);

    CPunctuationNestPair* pPuncNestPair = _PunctuationNestPair.Append();
    *pPuncNestPair = punc_angle_bracket;
}

void CCompositionProcessorEngine::InitializeSampleIMECompartment(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId)
{
	// set initial mode
    CCompartment CompartmentKeyboardOpen(pThreadMgr, tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    CompartmentKeyboardOpen._SetCompartmentBOOL(Global::isChineseMode);// 默认中文输入模式

    CCompartment CompartmentDoubleSingleByte(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    CompartmentDoubleSingleByte._SetCompartmentBOOL(FALSE);// 默认半角

    CCompartment CompartmentPunctuation(pThreadMgr, tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    CompartmentPunctuation._SetCompartmentBOOL(TRUE);// 默认中文标点

    PrivateCompartmentsUpdated();
    _SyncFullWidthPunctuationWithOpenState();
}
//+---------------------------------------------------------------------------
//
// CompartmentCallback
//
//----------------------------------------------------------------------------

// static
HRESULT CCompositionProcessorEngine::CompartmentCallback(_In_ void *pv, REFGUID guidCompartment)
{
    CCompositionProcessorEngine* fakeThis = (CCompositionProcessorEngine*)pv;
    if (nullptr == fakeThis)
    {
        return E_INVALIDARG;
    }

    ITfThreadMgr* pThreadMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, (void**)&pThreadMgr);
    if (FAILED(hr))
    {
        return E_FAIL;
    }

    if (IsEqualGUID(guidCompartment, Global::SampleIMEGuidCompartmentDoubleSingleByte) ||
        IsEqualGUID(guidCompartment, Global::SampleIMEGuidCompartmentPunctuation))
    {
        fakeThis->PrivateCompartmentsUpdated();
    }
    else if (IsEqualGUID(guidCompartment, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION) ||
        IsEqualGUID(guidCompartment, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE))
    {
        fakeThis->ConversionModeCompartmentUpdated();
    }
    else if (IsEqualGUID(guidCompartment, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE))
    {
        fakeThis->KeyboardOpenCompartmentUpdated();
    }

    pThreadMgr->Release();
    pThreadMgr = nullptr;

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// UpdatePrivateCompartments
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::ConversionModeCompartmentUpdated()
{
    if (!_pCompartmentConversion || !_pThreadMgr)
        return;

    DWORD conversionMode = 0;
    if (FAILED(_pCompartmentConversion->_GetCompartmentDWORD(conversionMode)))
        return;

    BOOL fOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    if (FAILED(CompartmentKeyboardOpen._GetCompartmentBOOL(fOpen)))
        return;

    DWORD newConvMode = conversionMode;

    // 以开关为准，强制修正 conversionMode 中的 NATIVE 标志
    if (fOpen)
        newConvMode |= TF_CONVERSIONMODE_NATIVE;   // 中文模式必须加 NATIVE
    else
        newConvMode &= ~TF_CONVERSIONMODE_NATIVE;  // 英文模式清除 NATIVE

    // 同时处理全半角、标点等其他标志（保留原有逻辑）
    BOOL isDouble = FALSE;
    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    if (SUCCEEDED(CompartmentDoubleSingleByte._GetCompartmentBOOL(isDouble)))
    {
        if (isDouble)
            newConvMode |= TF_CONVERSIONMODE_FULLSHAPE;
        else
            newConvMode &= ~TF_CONVERSIONMODE_FULLSHAPE;
    }

    BOOL isPunctuation = FALSE;
    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    if (SUCCEEDED(CompartmentPunctuation._GetCompartmentBOOL(isPunctuation)))
    {
        if (isPunctuation)
            newConvMode |= TF_CONVERSIONMODE_SYMBOL;
        else
            newConvMode &= ~TF_CONVERSIONMODE_SYMBOL;
    }

    if (newConvMode != conversionMode)
        _pCompartmentConversion->_SetCompartmentDWORD(newConvMode);
}

//+---------------------------------------------------------------------------
//
// PrivateCompartmentsUpdated()
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::PrivateCompartmentsUpdated()
{
    if (!_pCompartmentConversion || !_pThreadMgr)
    {
        return;
    }

    DWORD conversionMode = 0;
    DWORD conversionModePrev = 0;
    if (FAILED(_pCompartmentConversion->_GetCompartmentDWORD(conversionMode)))
    {
        return;
    }

    conversionModePrev = conversionMode;

    BOOL isDouble = FALSE;
    CCompartment CompartmentDoubleSingleByte(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentDoubleSingleByte);
    if (SUCCEEDED(CompartmentDoubleSingleByte._GetCompartmentBOOL(isDouble)))
    {
        if (!isDouble && (conversionMode & TF_CONVERSIONMODE_FULLSHAPE))
        {
            conversionMode &= ~TF_CONVERSIONMODE_FULLSHAPE;
        }
        else if (isDouble && !(conversionMode & TF_CONVERSIONMODE_FULLSHAPE))
        {
            conversionMode |= TF_CONVERSIONMODE_FULLSHAPE;
        }
    }

    BOOL isPunctuation = FALSE;
    CCompartment CompartmentPunctuation(_pThreadMgr, _tfClientId, Global::SampleIMEGuidCompartmentPunctuation);
    if (SUCCEEDED(CompartmentPunctuation._GetCompartmentBOOL(isPunctuation)))
    {
        if (!isPunctuation && (conversionMode & TF_CONVERSIONMODE_SYMBOL))
        {
            conversionMode &= ~TF_CONVERSIONMODE_SYMBOL;
        }
        else if (isPunctuation && !(conversionMode & TF_CONVERSIONMODE_SYMBOL))
        {
            conversionMode |= TF_CONVERSIONMODE_SYMBOL;
        }
    }

    if (conversionMode != conversionModePrev)
    {
        _pCompartmentConversion->_SetCompartmentDWORD(conversionMode);
    }
}

//+---------------------------------------------------------------------------
//
// KeyboardOpenCompartmentUpdated
//
//----------------------------------------------------------------------------

void CCompositionProcessorEngine::KeyboardOpenCompartmentUpdated()
{
    if (!_pCompartmentConversion || !_pThreadMgr)
    {
        return;
    }

    DWORD conversionMode = 0;
    DWORD conversionModePrev = 0;
    if (FAILED(_pCompartmentConversion->_GetCompartmentDWORD(conversionMode)))
    {
        return;
    }

    conversionModePrev = conversionMode;

    BOOL isOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    if (SUCCEEDED(CompartmentKeyboardOpen._GetCompartmentBOOL(isOpen)))
    {
        Global::isChineseMode = isOpen;

        if (isOpen && !(conversionMode & TF_CONVERSIONMODE_NATIVE))
        {
            conversionMode |= TF_CONVERSIONMODE_NATIVE;
        }
        else if (!isOpen && (conversionMode & TF_CONVERSIONMODE_NATIVE))
        {
            conversionMode &= ~TF_CONVERSIONMODE_NATIVE;
        }
    }

    if (conversionMode != conversionModePrev)
    {
        _pCompartmentConversion->_SetCompartmentDWORD(conversionMode);
    }
    _SyncFullWidthPunctuationWithOpenState();

}


//////////////////////////////////////////////////////////////////////
//
// XPreservedKey implementation.
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// UninitPreservedKey
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::XPreservedKey::UninitPreservedKey(_In_ ITfThreadMgr *pThreadMgr)
{
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;

    if (IsEqualGUID(Guid, GUID_NULL))
    {
        return FALSE;
    }

    if (FAILED(pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr)))
    {
        return FALSE;
    }

    for (UINT i = 0; i < TSFPreservedKeyTable.Count(); i++)
    {
        TF_PRESERVEDKEY pPreservedKey = *TSFPreservedKeyTable.GetAt(i);
        pPreservedKey.uModifiers &= 0xffff;

        pKeystrokeMgr->UnpreserveKey(Guid, &pPreservedKey);
    }

    pKeystrokeMgr->Release();

    return TRUE;
}

CCompositionProcessorEngine::XPreservedKey::XPreservedKey()
{
    Guid = GUID_NULL;
    Description = nullptr;
}

CCompositionProcessorEngine::XPreservedKey::~XPreservedKey()
{
    ITfThreadMgr* pThreadMgr = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr, (void**)&pThreadMgr);
    if (SUCCEEDED(hr))
    {
        UninitPreservedKey(pThreadMgr);
        pThreadMgr->Release();
        pThreadMgr = nullptr;
    }

    if (Description)
    {
        delete [] Description;
    }
}
//+---------------------------------------------------------------------------
//
// CSampleIME::CreateInstance 
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::CreateInstance(REFCLSID rclsid, REFIID riid, _Outptr_result_maybenull_ LPVOID* ppv, _Out_opt_ HINSTANCE* phInst, BOOL isComLessMode)
{
    HRESULT hr = S_OK;
    if (phInst == nullptr)
    {
        return E_INVALIDARG;
    }

    *phInst = nullptr;

    if (!isComLessMode)
    {
        hr = ::CoCreateInstance(rclsid, 
            NULL, 
            CLSCTX_INPROC_SERVER,
            riid,
            ppv);
    }
    else
    {
        hr = CSampleIME::ComLessCreateInstance(rclsid, riid, ppv, phInst);
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// CSampleIME::ComLessCreateInstance
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::ComLessCreateInstance(REFGUID rclsid, REFIID riid, _Outptr_result_maybenull_ void **ppv, _Out_opt_ HINSTANCE *phInst)
{
    HRESULT hr = S_OK;
    HINSTANCE sampleIMEDllHandle = nullptr;
    WCHAR wchPath[MAX_PATH] = {'\0'};
    WCHAR szExpandedPath[MAX_PATH] = {'\0'};
    DWORD dwCnt = 0;
    *ppv = nullptr;

    hr = phInst ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        *phInst = nullptr;
        hr = CSampleIME::GetComModuleName(rclsid, wchPath, ARRAYSIZE(wchPath));
        if (SUCCEEDED(hr))
        {
            dwCnt = ExpandEnvironmentStringsW(wchPath, szExpandedPath, ARRAYSIZE(szExpandedPath));
            hr = (0 < dwCnt && dwCnt <= ARRAYSIZE(szExpandedPath)) ? S_OK : E_FAIL;
            if (SUCCEEDED(hr))
            {
                sampleIMEDllHandle = LoadLibraryEx(szExpandedPath, NULL, 0);
                hr = sampleIMEDllHandle ? S_OK : E_FAIL;
                if (SUCCEEDED(hr))
                {
                    *phInst = sampleIMEDllHandle;
                    FARPROC pfn = GetProcAddress(sampleIMEDllHandle, "DllGetClassObject");
                    hr = pfn ? S_OK : E_FAIL;
                    if (SUCCEEDED(hr))
                    {
                        IClassFactory *pClassFactory = nullptr;
                        hr = ((HRESULT (STDAPICALLTYPE *)(REFCLSID rclsid, REFIID riid, LPVOID *ppv))(pfn))(rclsid, IID_IClassFactory, (void **)&pClassFactory);
                        if (SUCCEEDED(hr) && pClassFactory)
                        {
                            hr = pClassFactory->CreateInstance(NULL, riid, ppv);
                            pClassFactory->Release();
                        }
                    }
                }
            }
        }
    }

    if (!SUCCEEDED(hr) && phInst && *phInst)
    {
        FreeLibrary(*phInst);
        *phInst = 0;
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// CSampleIME::GetComModuleName
//
//----------------------------------------------------------------------------

HRESULT CSampleIME::GetComModuleName(REFGUID rclsid, _Out_writes_(cchPath)WCHAR* wchPath, DWORD cchPath)
{
    HRESULT hr = S_OK;

    CRegKey key;
    WCHAR wchClsid[CLSID_STRLEN + 1];
    hr = CLSIDToString(rclsid, wchClsid) ? S_OK : E_FAIL;
    if (SUCCEEDED(hr))
    {
        WCHAR wchKey[MAX_PATH];
        hr = StringCchPrintfW(wchKey, ARRAYSIZE(wchKey), L"CLSID\\%s\\InProcServer32", wchClsid);
        if (SUCCEEDED(hr))
        {
            hr = (key.Open(HKEY_CLASSES_ROOT, wchKey, KEY_READ) == ERROR_SUCCESS) ? S_OK : E_FAIL;
            if (SUCCEEDED(hr))
            {
                WCHAR wszModel[MAX_PATH];
                ULONG cch = ARRAYSIZE(wszModel);
                hr = (key.QueryStringValue(L"ThreadingModel", wszModel, &cch) == ERROR_SUCCESS) ? S_OK : E_FAIL;
                if (SUCCEEDED(hr))
                {
                    if (CompareStringOrdinal(wszModel, 
                        -1, 
                        L"Apartment", 
                        -1,
                        TRUE) == CSTR_EQUAL)
                    {
                        hr = (key.QueryStringValue(NULL, wchPath, &cchPath) == ERROR_SUCCESS) ? S_OK : E_FAIL;
                    }
                    else
                    {
                        hr = E_FAIL;
                    }
                }
            }
        }
    }

    return hr;
}

void CCompositionProcessorEngine::InitKeyStrokeTable()
{
    for (int i = 0; i < 26; i++)
    {
        _keystrokeTable[i].VirtualKey = 'A' + i;
        _keystrokeTable[i].Modifiers = 0;
        _keystrokeTable[i].Function = FUNCTION_INPUT;
    }
}

void CCompositionProcessorEngine::ShowAllLanguageBarIcons()
{
    SetLanguageBarStatus(TF_LBI_STATUS_HIDDEN, FALSE);
}

void CCompositionProcessorEngine::HideAllLanguageBarIcons()
{
    SetLanguageBarStatus(TF_LBI_STATUS_HIDDEN, TRUE);
}

void CCompositionProcessorEngine::SetInitialCandidateListRange()
{
    _candidateListIndexRange.Clear();
    UINT numperpage = Global::isHorizontalMode ? Global::nMaxHorizontalItems : Global::nMaxVerticalItems;
    for (DWORD i = 1; i <= numperpage; i++)
    {
        DWORD* pNewIndexRange = _candidateListIndexRange.Append();
        if (pNewIndexRange != nullptr)
        {
            if (i == numperpage && numperpage == 10)
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

void CCompositionProcessorEngine::SetDefaultCandidateTextFont()
{
    // Candidate Text Font
    if (Global::defaultlFontHandle == nullptr)
    {
		WCHAR fontName[50] = {'\0'}; 
		LoadString(Global::dllInstanceHandle, IDS_DEFAULT_FONT, fontName, 50);
        Global::defaultlFontHandle = CreateFont(-MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, FW_MEDIUM, 0, 0, 0, 0, 0, 0, 0, 0, fontName);
        if (!Global::defaultlFontHandle)
        {
			LOGFONT lf;
			SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &lf, 0);
            // Fall back to the default GUI font on failure.
            Global::defaultlFontHandle = CreateFont(-MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), 0, 0, 0, FW_MEDIUM, 0, 0, 0, 0, 0, 0, 0, 0, lf.lfFaceName);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
//    CCompositionProcessorEngine
//
//////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
// CCompositionProcessorEngine::IsVirtualKeyNeed
//
// Test virtual key code need to the Composition Processor Engine.
// param
//     [in] uCode - Specify virtual key code.
//     [in/out] pwch       - char code
//     [in] fComposing     - Specified composing.
//     [in] fCandidateMode - Specified candidate mode.
//     [out] pKeyState     - Returns function regarding virtual key.
// returns
//     If engine need this virtual key code, returns true. Otherwise returns false.
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsVirtualKeyNeed(UINT uCode, _In_reads_(1) WCHAR *pwch, BOOL fComposing, CANDIDATE_MODE candidateMode, BOOL hasCandidateWithWildcard, _Out_opt_ _KEYSTROKE_STATE *pKeyState)
{
    if (pKeyState)
    {
        pKeyState->Category = CATEGORY_NONE;
        pKeyState->Function = FUNCTION_NONE;
    }

    if (candidateMode == CANDIDATE_ORIGINAL || candidateMode == CANDIDATE_PHRASE || candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION)
    {
        fComposing = FALSE;
    }

    if (fComposing || candidateMode == CANDIDATE_INCREMENTAL || candidateMode == CANDIDATE_NONE)
    {
        if (IsVirtualKeyKeystrokeComposition(uCode, pKeyState, FUNCTION_NONE))
        {
            return TRUE;
        }
        else if ((IsWildcard() && IsWildcardChar(*pwch) && !IsDisableWildcardAtFirst()) ||
            (IsWildcard() && IsWildcardChar(*pwch) &&  IsDisableWildcardAtFirst() && _keystrokeBuffer.GetLength()))
        {
            if (pKeyState)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = FUNCTION_INPUT;
            }
            return TRUE;
        }
        else if (_hasWildcardIncludedInKeystrokeBuffer && uCode == VK_SPACE)
        {
            if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_CONVERT_WILDCARD; } return TRUE;
        }
    }

    if (candidateMode == CANDIDATE_ORIGINAL || candidateMode == CANDIDATE_PHRASE || candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION)
    {
        BOOL isRetCode = TRUE;
        if (IsVirtualKeyKeystrokeCandidate(uCode, pKeyState, candidateMode, &isRetCode, &_KeystrokeCandidate))
        {
            return isRetCode;
        }

        if (hasCandidateWithWildcard)
        {
            if (IsVirtualKeyKeystrokeCandidate(uCode, pKeyState, candidateMode, &isRetCode, &_KeystrokeCandidateWildcard))
            {
                return isRetCode;
            }
        }

        // Candidate list could not handle key. We can try to restart the composition.
        if (IsVirtualKeyKeystrokeComposition(uCode, pKeyState, FUNCTION_INPUT))
        {
            if (candidateMode != CANDIDATE_ORIGINAL)
            {
                return TRUE;
            }
            else
            {
                if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_FINALIZE_CANDIDATELIST_AND_INPUT; } 
                return TRUE;
            }
        }
    } 

    // CANDIDATE_INCREMENTAL should process Keystroke.Candidate virtual keys.
    else if (candidateMode == CANDIDATE_INCREMENTAL)
    {
        BOOL isRetCode = TRUE;
        if (IsVirtualKeyKeystrokeCandidate(uCode, pKeyState, candidateMode, &isRetCode, &_KeystrokeCandidate))
        {
            return isRetCode;
        }
    }

    if (!fComposing && candidateMode != CANDIDATE_ORIGINAL && candidateMode != CANDIDATE_PHRASE && candidateMode != CANDIDATE_WITH_NEXT_COMPOSITION) 
    {
        if (IsVirtualKeyKeystrokeComposition(uCode, pKeyState, FUNCTION_INPUT))
        {
            return TRUE;
        }
    }

    // System pre-defined keystroke
    if (fComposing)
    {
        if ((candidateMode != CANDIDATE_INCREMENTAL))
        {
            switch (uCode)
            {
            case VK_RETURN:  if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_DIRECT_COMMIT; } return TRUE;
            case VK_LEFT:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_LEFT; } return TRUE;
            case VK_RIGHT:  if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_RIGHT; } return TRUE;
            case VK_SPACE: if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_FINALIZE_CANDIDATELIST; } return TRUE;
            case VK_ESCAPE: if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_CANCEL; } return TRUE;
            case VK_BACK:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_BACKSPACE; } return TRUE;

            case VK_UP:     if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_UP; } return TRUE;
            case VK_DOWN:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_DOWN; } return TRUE;
            case VK_PRIOR:  if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_PAGE_UP; } return TRUE;
            case VK_NEXT:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_PAGE_DOWN; } return TRUE;

            case VK_HOME:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_PAGE_TOP; } return TRUE;
            case VK_END:    if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_MOVE_PAGE_BOTTOM; } return TRUE;

            case VK_TAB:  if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_CONVERT; } return TRUE;

            }
            if (pwch && *pwch && IsPunctuation(*pwch)) { if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_PUNCTUATION; } return TRUE; }
        }
        else if ((candidateMode == CANDIDATE_INCREMENTAL))
        {
            switch (uCode)
            {
                // VK_LEFT, VK_RIGHT - set *pIsEaten = FALSE for application could move caret left or right.
                // and for CUAS, invoke _HandleCompositionCancel() edit session due to ignore CUAS default key handler for send out terminate composition
            /*case VK_LEFT:
            case VK_RIGHT:
                {
                    if (pKeyState)
                    {
                        pKeyState->Category = CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION;
                        pKeyState->Function = FUNCTION_CANCEL;
                    }
                }
                return FALSE;*/
            case VK_OEM_PLUS:      // '+' 键
                if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_DOWN; } return TRUE;
            case VK_OEM_MINUS:     // '-' 键
                if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_UP; } return TRUE;
            case VK_LEFT: if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_LEFT; } return TRUE;
            case VK_RIGHT: if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_RIGHT; } return TRUE;
            case VK_SPACE:
            case VK_RETURN: if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_FINALIZE_CANDIDATELIST; } return TRUE;
            case VK_ESCAPE: if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_CANCEL; } return TRUE;

                // VK_BACK - remove one char from reading string.
            case VK_BACK:   if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_BACKSPACE; } return TRUE;

            case VK_UP:     if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_UP; } return TRUE;
            case VK_DOWN:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_DOWN; } return TRUE;
            case VK_PRIOR:  if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_UP; } return TRUE;
            case VK_NEXT:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_DOWN; } return TRUE;

            case VK_HOME:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_TOP; } return TRUE;
            case VK_END:    if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_BOTTOM; } return TRUE;

            case VK_TAB:
                if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_CONVERT; } return TRUE;
            }
            if (pwch && *pwch && IsPunctuation(*pwch)) { if (pKeyState) {pKeyState->Category = CATEGORY_COMPOSING;pKeyState->Function = FUNCTION_PUNCTUATION;} return TRUE; }
        }
    }

    if ((candidateMode == CANDIDATE_ORIGINAL) || (candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION))
    {
        switch (uCode)
        {
        case VK_UP:     if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_UP; } return TRUE;
        case VK_DOWN:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_DOWN; } return TRUE;
        case VK_LEFT:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_LEFT; } return TRUE;
        case VK_RIGHT:  if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_RIGHT; } return TRUE;
        case VK_PRIOR:  if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_UP; } return TRUE;
        case VK_NEXT:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_DOWN; } return TRUE;
        case VK_HOME:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_TOP; } return TRUE;
        case VK_END:    if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_MOVE_PAGE_BOTTOM; } return TRUE;
        case VK_RETURN: if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_DIRECT_COMMIT; } return TRUE;
        case VK_SPACE:
            if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_FINALIZE_CANDIDATELIST; } return TRUE;
        case VK_TAB:  if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_CONVERT; } return TRUE;
        case VK_BACK:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_CANCEL; } return TRUE;

        case VK_ESCAPE:
            {
                if (candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION)
                {
                    if (pKeyState)
                    {
                        pKeyState->Category = CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION;
                        pKeyState->Function = FUNCTION_FINALIZE_TEXTSTORE;
                    }
                    return TRUE;
                }
                else
                {
                    if (pKeyState)
                    {
                        pKeyState->Category = CATEGORY_CANDIDATE;
                        pKeyState->Function = FUNCTION_CANCEL;
                    }
                    return TRUE;
                }
            }
        }
        if (pwch && *pwch && IsPunctuation(*pwch)) { if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_PUNCTUATION; } return TRUE; }
        
        if (candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION)
        {
            if (IsVirtualKeyKeystrokeComposition(uCode, NULL, FUNCTION_NONE))
            {
                if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_FINALIZE_TEXTSTORE_AND_INPUT; } return TRUE;
            }
        }
    }

    if (candidateMode == CANDIDATE_PHRASE)
    {
        switch (uCode)
        {
        case VK_UP:     if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_UP; } return TRUE;
        case VK_DOWN:   if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_DOWN; } return TRUE;
        case VK_LEFT:     if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_UP; } return TRUE;
        case VK_RIGHT:   if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_DOWN; } return TRUE;
        case VK_PRIOR:  if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_PAGE_UP; } return TRUE;
        case VK_NEXT:   if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_PAGE_DOWN; } return TRUE;
        case VK_HOME:   if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_PAGE_TOP; } return TRUE;
        case VK_END:    if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_MOVE_PAGE_BOTTOM; } return TRUE;
        case VK_RETURN: if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_FINALIZE_CANDIDATELIST; } return TRUE;
        case VK_SPACE:  if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_CONVERT; } return TRUE;
        case VK_ESCAPE: if (pKeyState) { pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_CANCEL; } return TRUE;
        case VK_BACK:   if (pKeyState) { pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_CANCEL; } return TRUE;
        }
        if (pwch && *pwch && IsPunctuation(*pwch)) { if (pKeyState) { pKeyState->Category = CATEGORY_COMPOSING; pKeyState->Function = FUNCTION_PUNCTUATION; } return TRUE; }
    }

    if (IsKeystrokeRange(uCode, pKeyState, candidateMode))
    {
        return TRUE;
    }
    else if (pKeyState && pKeyState->Category != CATEGORY_NONE)
    {
        return FALSE;
    }
    // 非组合状态 + 无候选列表 + 标点符号 + 中文模式 -> 强制交给输入法处理标点转换
    if (!fComposing && pwch && *pwch && IsPunctuation(*pwch))
    {
        BOOL fOpen = FALSE;
        if (_pThreadMgr && _tfClientId != TF_CLIENTID_NULL)
        {
            CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
            if (SUCCEEDED(CompartmentKeyboardOpen._GetCompartmentBOOL(fOpen)) && fOpen)
            {
                if (pKeyState)
                {
                    pKeyState->Category = CATEGORY_COMPOSING;
                    pKeyState->Function = FUNCTION_PUNCTUATION;
                }
                return TRUE;
            }
        }
    }

    if (*pwch && !IsVirtualKeyKeystrokeComposition(uCode, pKeyState, FUNCTION_NONE))
    {
        if (pKeyState)
        {
            pKeyState->Category = CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION;
            pKeyState->Function = FUNCTION_FINALIZE_TEXTSTORE;
        }
        return FALSE;
    }

    return FALSE;
}

//+---------------------------------------------------------------------------
//
// CCompositionProcessorEngine::IsVirtualKeyKeystrokeComposition
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsVirtualKeyKeystrokeComposition(UINT uCode, _Out_opt_ _KEYSTROKE_STATE *pKeyState, KEYSTROKE_FUNCTION function)
{
    if (pKeyState == nullptr)
    {
        return FALSE;
    }

    pKeyState->Category = CATEGORY_NONE;
    pKeyState->Function = FUNCTION_NONE;

    for (UINT i = 0; i < _KeystrokeComposition.Count(); i++)
    {
        _KEYSTROKE *pKeystroke = nullptr;

        pKeystroke = _KeystrokeComposition.GetAt(i);

        if ((pKeystroke->VirtualKey == uCode) && Global::CheckModifiers(Global::ModifiersValue, pKeystroke->Modifiers))
        {
            if (function == FUNCTION_NONE)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = pKeystroke->Function;
                return TRUE;
            }
            else if (function == pKeystroke->Function)
            {
                pKeyState->Category = CATEGORY_COMPOSING;
                pKeyState->Function = pKeystroke->Function;
                return TRUE;
            }
        }
    }

    return FALSE;
}

//+---------------------------------------------------------------------------
//
// CCompositionProcessorEngine::IsVirtualKeyKeystrokeCandidate
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsVirtualKeyKeystrokeCandidate(UINT uCode, _In_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode, _Out_ BOOL *pfRetCode, _In_ CSampleImeArray<_KEYSTROKE> *pKeystrokeMetric)
{
    if (pfRetCode == nullptr)
    {
        return FALSE;
    }
    *pfRetCode = FALSE;

    for (UINT i = 0; i < pKeystrokeMetric->Count(); i++)
    {
        _KEYSTROKE *pKeystroke = nullptr;

        pKeystroke = pKeystrokeMetric->GetAt(i);

        if ((pKeystroke->VirtualKey == uCode) && Global::CheckModifiers(Global::ModifiersValue, pKeystroke->Modifiers))
        {
            *pfRetCode = TRUE;
            if (pKeyState)
            {
                pKeyState->Category = (candidateMode == CANDIDATE_ORIGINAL ? CATEGORY_CANDIDATE :
                    candidateMode == CANDIDATE_PHRASE ? CATEGORY_PHRASE : CATEGORY_CANDIDATE);

                pKeyState->Function = pKeystroke->Function;
            }
            return TRUE;
        }
    }

    return FALSE;
}

//+---------------------------------------------------------------------------
//
// CCompositionProcessorEngine::IsKeyKeystrokeRange
//
//----------------------------------------------------------------------------

BOOL CCompositionProcessorEngine::IsKeystrokeRange(UINT uCode, _Out_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode)
{
    if (pKeyState == nullptr)
    {
        return FALSE;
    }

    pKeyState->Category = CATEGORY_NONE;
    pKeyState->Function = FUNCTION_NONE;

    if (_candidateListIndexRange.IsRange(uCode))
    {
        if (candidateMode == CANDIDATE_PHRASE)
        {
            // Candidate phrase could specify modifier
            if ((GetCandidateListPhraseModifier() == 0 && Global::ModifiersValue == 0) ||
                (GetCandidateListPhraseModifier() != 0 && Global::CheckModifiers(Global::ModifiersValue, GetCandidateListPhraseModifier())))
            {
                pKeyState->Category = CATEGORY_PHRASE; pKeyState->Function = FUNCTION_SELECT_BY_NUMBER;
                return TRUE;
            }
            else
            {
                pKeyState->Category = CATEGORY_INVOKE_COMPOSITION_EDIT_SESSION; pKeyState->Function = FUNCTION_FINALIZE_TEXTSTORE_AND_INPUT;
                return FALSE;
            }
        }
        else if (candidateMode == CANDIDATE_WITH_NEXT_COMPOSITION)
        {
            // Candidate phrase could specify modifier
            if ((GetCandidateListPhraseModifier() == 0 && Global::ModifiersValue == 0) ||
                (GetCandidateListPhraseModifier() != 0 && Global::CheckModifiers(Global::ModifiersValue, GetCandidateListPhraseModifier())))
            {
                pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_SELECT_BY_NUMBER;
                return TRUE;
            }
            // else next composition
        }
        else if (candidateMode != CANDIDATE_NONE)
        {
            pKeyState->Category = CATEGORY_CANDIDATE; pKeyState->Function = FUNCTION_SELECT_BY_NUMBER;
            return TRUE;
        }
    }
    return FALSE;
}

void CCompositionProcessorEngine::_SyncFullWidthPunctuationWithOpenState()
{
    if (_pThreadMgr == nullptr || _tfClientId == TF_CLIENTID_NULL)
        return;

    BOOL fOpen = FALSE;
    CCompartment CompartmentKeyboardOpen(_pThreadMgr, _tfClientId, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
    if (SUCCEEDED(CompartmentKeyboardOpen._GetCompartmentBOOL(fOpen)))
    {
        // IME 开启（中文模式） → 全角标点；IME 关闭（英文模式） → 半角标点
        _fFullWidthPunctuation = fOpen ? TRUE : FALSE;
    }
}
// 合并两个词典的候选结果（用户优先，并去重）
void CCompositionProcessorEngine::_MergeCandidateLists(
    CSampleImeArray<CCandidateListItem>* pUserList,
    CSampleImeArray<CCandidateListItem>* pSystemList,
    CSampleImeArray<CCandidateListItem>* pResultList)
{
    LCID locale = GetLocale();

    // 1. 添加所有用户候选
    for (UINT i = 0; i < pUserList->Count(); i++)
    {
        CCandidateListItem* pItem = pResultList->Append();
        *pItem = *pUserList->GetAt(i);   // 深拷贝
    }

    // 2. 添加系统候选（去重：与用户候选词语不重复）
    for (UINT i = 0; i < pSystemList->Count(); i++)
    {
        CCandidateListItem* pSysItem = pSystemList->GetAt(i);
        BOOL bDuplicate = FALSE;
        for (UINT j = 0; j < pUserList->Count(); j++)
        {
            if (CStringRange::Compare(locale,
                &pSysItem->_ItemString,
                &pUserList->GetAt(j)->_ItemString) == CSTR_EQUAL)
            {
                bDuplicate = TRUE;
                break;
            }
        }
        if (!bDuplicate)
        {
            CCandidateListItem* pItem = pResultList->Append();
            *pItem = *pSysItem;
        }
    }
}
void CCompositionProcessorEngine::ChangeWborPinyinDictionary()
{
    //std::lock_guard<std::mutex> lock(_engineMutex);
    // 释放旧用户词库
    //if (_pTableDictionaryEngine) { delete _pTableDictionaryEngine; _pTableDictionaryEngine = nullptr; }
    //if (_pDictionaryFile) { delete _pDictionaryFile;   _pDictionaryFile = nullptr; }

    //if (_pUserDictionaryEngine) { delete _pUserDictionaryEngine; _pUserDictionaryEngine = nullptr; }
    //if (_pUserDictionaryFile) { delete _pUserDictionaryFile;   _pUserDictionaryFile = nullptr; }
    SetupDictionaryFile();
    //SetInitialCandidateListRange();   // 根据当前 Global::isPinyinMode 重新设置 _candidateListIndexRange
    if (_pLanguageBar_IMEMode)
    {
        _pLanguageBar_IMEMode->RefreshIcon();
    }
    OutputDebugString(Global::isPinyinMode ? (Global::isPyAndWbMode ? L"切换到拼音模式": L"切换到五笔拼音混合模式") : L"切换到五笔模式");
}
BOOL CCompositionProcessorEngine::AddUserWord(LPCWSTR pszCode, LPCWSTR pszWord)
{
    if (!pszCode || !pszWord) return FALSE;

    // 获取用户词库文件路径
    WCHAR szUserPath[MAX_PATH];
    StringCchPrintf(szUserPath, MAX_PATH, L"%s%s", g_szDllPath, TEXTSERVICE_UDIC);

    // 以追加方式打开文件（如果不存在则创建）
    HANDLE hFile = CreateFile(szUserPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    // 构造一行：编码 空格 词语 换行
    WCHAR szLine[512];
    StringCchPrintf(szLine, 512, L"%s %s\r\n", pszCode, pszWord);
    DWORD dwWritten = 0;
    BOOL bRet = WriteFile(hFile, szLine, (DWORD)(wcslen(szLine) * sizeof(WCHAR)), &dwWritten, NULL);
    CloseHandle(hFile);

    if (bRet)
    {
        // 重新加载用户词库，使新词立即生效
        ChangeWborPinyinDictionary();
    }
    return bRet;
}
// CompositionProcessorEngine.cpp
std::wstring CCompositionProcessorEngine::GetCodeForWord(const WCHAR* pszWord, DWORD_PTR cchWord) {
    //std::lock_guard<std::mutex> lock(_engineMutex);
    // 关键：系统词库引擎可能为空
    if (!_pTableDictionaryEngine) return L"";

    // 2. 提取所有汉字（保留顺序）
    std::wstring hanziStr;
    hanziStr.reserve(cchWord);
    for (DWORD_PTR i = 0; i < cchWord; ++i)
    {
        WCHAR ch = pszWord[i];
        if (ch >= 0x4E00 && ch <= 0x9FFF)   // 基本汉字 Unicode 范围
            hanziStr.push_back(ch);
    }
    if (hanziStr.empty()) return L"";


    // 2. 定义 lambda（放在使用之前）
    auto GetSingleCode = [this](WCHAR ch) -> std::wstring {
        WCHAR temp[2] = { ch, L'\0' };
        return _pTableDictionaryEngine ? _pTableDictionaryEngine->GetCodeForWord(temp, 1) : L"";
        };
    std::wstring result;
    // 拼音模式：查询拼音词典
    if (Global::isPinyinMode) {
        // 拼音模式：每个汉字取完整拼音，用空格分隔
        for (size_t i = 0; i < hanziStr.size(); ++i) {
            result += GetSingleCode(hanziStr[i]);
        }
    }
    else { // 五笔模式
        // 对于词组，逐个汉字查编码拼接
        size_t wordLen = hanziStr.size();
        if (wordLen == 1)
        {
            // 单字：直接取完整编码
            result = GetSingleCode(hanziStr[0]);
        }
        else if (wordLen == 2)
        {
            // 双字：每字前两位编码
            std::wstring code1 = GetSingleCode(hanziStr[0]);
            std::wstring code2 = GetSingleCode(hanziStr[1]);
            result = code1.substr(0, 2) + code2.substr(0, 2);
        }
        else if (wordLen == 3)
        {
            // 三字：第一字1位 + 第二字1位 + 第三字前2位
            std::wstring code1 = GetSingleCode(hanziStr[0]);
            std::wstring code2 = GetSingleCode(hanziStr[1]);
            std::wstring code3 = GetSingleCode(hanziStr[2]);
            result = code1.substr(0, 1) + code2.substr(0, 1) + code3.substr(0, 2);
        }
        else   // wordLen >= 4
        {
            // 四字及以上：前三字各取1位 + 最后一字取1位
            std::wstring code1 = GetSingleCode(hanziStr[0]);
            std::wstring code2 = GetSingleCode(hanziStr[1]);
            std::wstring code3 = GetSingleCode(hanziStr[2]);
            std::wstring codeLast = GetSingleCode(hanziStr[wordLen - 1]);
            result = code1.substr(0, 1) + code2.substr(0, 1) + code3.substr(0, 1) + codeLast.substr(0, 1);
        }
    }
    // 调试输出：查询结果
    return result;
}

BOOL CCompositionProcessorEngine::WordExistsInDictFile(LPCWSTR pszDictPath, LPCWSTR pszWord) {
    if (!pszDictPath || !pszWord || !*pszWord) return FALSE;
        HANDLE hFile = CreateFile(pszDictPath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0) {
        CloseHandle(hFile);
        return FALSE;
    }

    // 读取文件内容
    WCHAR* pBuf = new WCHAR[fileSize / sizeof(WCHAR) + 2];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, pBuf, fileSize, &bytesRead, NULL)) {
        delete[] pBuf;
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);
    pBuf[bytesRead / sizeof(WCHAR)] = L'\0';

    // 逐行查找
    const WCHAR* p = pBuf;
    while (*p) {
        // 跳过空行
        while (*p == L'\r' || *p == L'\n') p++;
        if (!*p) break;

        // 定位行尾
        const WCHAR* lineEnd = p;
        while (*lineEnd && *lineEnd != L'\r' && *lineEnd != L'\n') lineEnd++;

        // 解析行：跳过编码（编码后有一个空格）
        const WCHAR* token = p;
        while (token < lineEnd && *token != L' ') token++;
        if (token >= lineEnd) { p = lineEnd; continue; } // 没有空格，无效行
        token++; // 跳到第一个词语开头

        // 扫描该行的所有词语
        const WCHAR* wordStart = token;
        while (wordStart < lineEnd) {
            const WCHAR* wordEnd = wordStart;
            while (wordEnd < lineEnd && *wordEnd != L' ') wordEnd++;
            size_t wordLen = wordEnd - wordStart;
            if (wordLen == wcslen(pszWord) && _wcsnicmp(wordStart, pszWord, wordLen) == 0) {
                delete[] pBuf;
                return TRUE;
            }
            wordStart = wordEnd + 1; // 下一个词语开头（跳过空格）
        }

        p = lineEnd;
    }

    delete[] pBuf;
    return FALSE;
}
int CCompositionProcessorEngine::AddUserWordWithOption(LPCWSTR pszCode, LPCWSTR pszWord, BOOL bAlsoAddPinyin) {
    // 获取 DLL 所在目录
    //std::lock_guard<std::mutex> lock(_engineMutex);

    // 根据当前模式确定用户词库文件路径
    const WCHAR* pTargetDict = Global::isPinyinMode ? TEXTSERVICE_PYUDIC : TEXTSERVICE_UDIC;
    WCHAR szUserPath[MAX_PATH];
    StringCchPrintf(szUserPath, MAX_PATH, L"%s%s", g_szDllPath, pTargetDict);

    // 查重：用户词库
    if (WordExistsInDictFile(szUserPath, pszWord)) {
        return 1;
    }

    // 可选：查重系统词库
    const WCHAR* pSystemDict = Global::isPinyinMode ? TEXTSERVICE_PYDIC : TEXTSERVICE_DIC;
    WCHAR szSysPath[MAX_PATH];
    StringCchPrintf(szSysPath, MAX_PATH, L"%s%s", g_szDllPath, pSystemDict);
    if (WordExistsInDictFile(szSysPath, pszWord)) {
        return 2;
    }
    // 以追加方式写入

    HANDLE hFile = CreateFile(szUserPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { 
        return 3; }

    // 检查是否为新创建的空文件---------------------写入开始
    LARGE_INTEGER fileSize;
    if (GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart == 0) {
        WORD bom = 0xFEFF;  // UTF-16 LE BOM
        DWORD written;
        WriteFile(hFile, &bom, sizeof(bom), &written, NULL);
    }
    // 写入 UTF-16 LE 数据（WCHAR 本身就是）
    WCHAR szLine[512];
    StringCchPrintf(szLine, 512, L"%s %s\r\n", pszCode, pszWord);
    DWORD dwWritten = 0;
    BOOL bRet = WriteFile(hFile, szLine, (DWORD)(wcslen(szLine) * sizeof(WCHAR)), &dwWritten, NULL);
    CloseHandle(hFile);
    //-----------------------------------------------写入结束
    
    // 5. 如果五笔模式且勾选了同时添加拼音词语，则额外写入拼音用户词库
    if (bRet && !Global::isPinyinMode && bAlsoAddPinyin) {
		//查重，还未完成的功能：如果拼音词库里已经有这个词了，就不添加了
            // 根据当前模式确定用户词库文件路径
        pTargetDict = Global::isPinyinMode ? TEXTSERVICE_UDIC : TEXTSERVICE_PYUDIC;
        StringCchPrintf(szUserPath, MAX_PATH, L"%s%s", g_szDllPath, pTargetDict);

        // 查重：用户词库
        if (WordExistsInDictFile(szUserPath, pszWord)) {
            return 11;
        }

        // 可选：查重系统词库
        pSystemDict = Global::isPinyinMode ? TEXTSERVICE_DIC : TEXTSERVICE_PYDIC;
        StringCchPrintf(szSysPath, MAX_PATH, L"%s%s", g_szDllPath, pSystemDict);
        if (WordExistsInDictFile(szSysPath, pszWord)) {
            return 12;
        }
		//----------------------------------------------------------------------------查重结束！
        std::wstring pinyinCode = GetCodeForWord(pszWord, wcslen(pszWord));
        if (!pinyinCode.empty()) {
            //写入
            WCHAR szPyPath[MAX_PATH];
            StringCchPrintf(szPyPath, MAX_PATH, L"%s%s", g_szDllPath, TEXTSERVICE_PYUDIC);
            HANDLE hPyFile = CreateFile(szPyPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hPyFile != INVALID_HANDLE_VALUE) {
                if (GetFileSizeEx(hPyFile, &fileSize) && fileSize.QuadPart == 0) {
                    WORD bom = 0xFEFF;  // UTF-16 LE BOM
                    DWORD written;
                    WriteFile(hPyFile, &bom, sizeof(bom), &written, NULL);
                }
                // 写入 UTF-16 LE 数据（WCHAR 本身就是）
                WCHAR szPyLine[512];
                StringCchPrintf(szPyLine, 512, L"%s %s\r\n", pszCode, pszWord);
                dwWritten = 0;
                bRet = WriteFile(hPyFile, szPyLine, (DWORD)(wcslen(szPyLine) * sizeof(WCHAR)), &dwWritten, NULL);
                CloseHandle(hPyFile);

            }
            else {
                return 13;
            }
        }
    }
    return 0;
}
void CCompositionProcessorEngine::RefreshLanguageBarIcon()
{
    if (_pLanguageBar_IMEMode)
        _pLanguageBar_IMEMode->RefreshIcon();
}
void CCompositionProcessorEngine::RefreshToolBarIcon()
{
    if (_pLanguageBar_IMEMode)
        _pLanguageBar_IMEMode->RefreshToolBarIcon();
}
void CCompositionProcessorEngine::ShowUserWordDialog()
{
    if (_pLanguageBar_IMEMode)   // 任选一个语言栏按钮
    {
        _pLanguageBar_IMEMode->_ShowUserWordDialog();
    }
}
void CCompositionProcessorEngine::ShowEmojiDialog()
{
    if (_pLanguageBar_IMEMode)
        _pLanguageBar_IMEMode->_ShowEmojiDialog();
}
void CCompositionProcessorEngine::ShowSettingsDialog()
{
    if (_pLanguageBar_IMEMode)
        _pLanguageBar_IMEMode->_ShowSettingsDialog();
}