// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved


#pragma once

#include "sal.h"
#include "TableDictionaryEngine.h"
#include "KeyHandlerEditSession.h"
#include "SampleIMEBaseStructure.h"
#include "FileMapping.h"
#include "Compartment.h"
#include "define.h"
#include <mutex>   // 文件顶部
class CCompositionProcessorEngine
{
public:
    CCompositionProcessorEngine(CSampleIME* pTextService);
    ~CCompositionProcessorEngine(void);
    static BOOL WordExistsInDictFile(LPCWSTR pszDictPath, LPCWSTR pszWord);
    void CreateLanguageBarButton(DWORD dwEnable, GUID guidLangBar, _In_z_ LPCWSTR pwszDescriptionValue, _In_z_ LPCWSTR pwszTooltipValue,
        DWORD dwOnIconIndex, DWORD dwOffIconIndex, DWORD dwOnIconIndexPinyin, DWORD dwOnIconIndexPinyinWubi, _Outptr_result_maybenull_ CLangBarItemButton** ppLangBarItemButton, BOOL isSecureMode, CSampleIME* pTextService);
    BOOL SetupLanguageProfile(LANGID langid, REFGUID guidLanguageProfile, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, BOOL isSecureMode, BOOL isComLessMode);
    CTableDictionaryEngine* GetWubiEngine() const { return _pWubiDictionaryEngine; }
    CLangBarItemButton* GetIMEModeLangBar() { return _pLanguageBar_IMEMode; }
    // Get language profile.
    GUID GetLanguageProfile(LANGID *plangid)
    {
        *plangid = _langid;
        return _guidProfile;
    }
    // Get locale
    LCID GetLocale()
    {
        return MAKELCID(_langid, SORT_DEFAULT);
    }

    BOOL IsVirtualKeyNeed(UINT uCode, _In_reads_(1) WCHAR *pwch, BOOL fComposing, CANDIDATE_MODE candidateMode, BOOL hasCandidateWithWildcard, _Out_opt_ _KEYSTROKE_STATE *pKeyState);
    void LoadConfig();   // 读取 INI 并更新全局变量
    void RefreshLanguageBarIcon();
    void RefreshToolBarIcon();
    void ShowUserWordDialog();              // 新增公共方法
    void ShowEmojiDialog();
    void ShowSettingsDialog();
    BOOL AddVirtualKey(WCHAR wch);
    void RemoveVirtualKey(DWORD_PTR dwIndex);
    void PurgeVirtualKey();

    DWORD_PTR GetVirtualKeyLength() { return _keystrokeBuffer.GetLength(); }
    WCHAR GetVirtualKey(DWORD_PTR dwIndex);

    void GetReadingStrings(_Inout_ CSampleImeArray<CStringRange> *pReadingStrings, _Out_ BOOL *pIsWildcardIncluded);
    void GetCandidateList(_Inout_ CSampleImeArray<CCandidateListItem> *pCandidateList, BOOL isIncrementalWordSearch, BOOL isWildcardSearch);
    void GetCandidateStringInConverted(CStringRange &searchString, _In_ CSampleImeArray<CCandidateListItem> *pCandidateList);

    // Preserved key handler
    void OnPreservedKey(REFGUID rguid, _Out_ BOOL *pIsEaten, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId);

    // Punctuation
    BOOL IsPunctuation(WCHAR wch);
    WCHAR GetPunctuation(WCHAR wch);

    BOOL IsDoubleSingleByte(WCHAR wch);
    BOOL IsWildcard() { return _isWildcard; }
    BOOL IsDisableWildcardAtFirst() { return _isDisableWildcardAtFirst; }
    BOOL IsWildcardChar(WCHAR wch) { return ((IsWildcardOneChar(wch) || IsWildcardAllChar(wch)) ? TRUE : FALSE); }
    BOOL IsWildcardOneChar(WCHAR wch) { return (wch==L'?' ? TRUE : FALSE); }
    BOOL IsWildcardAllChar(WCHAR wch) { return (wch==L'*' ? TRUE : FALSE); }
    BOOL IsMakePhraseFromText() { return _hasMakePhraseFromText; }
    BOOL IsKeystrokeSort() { return _isKeystrokeSort; }

    // Dictionary engine
    BOOL IsDictionaryAvailable() { return (_pTableDictionaryEngine ? TRUE : FALSE); }

    // Language bar control
    void SetLanguageBarStatus(DWORD status, BOOL isSet);

    void ConversionModeCompartmentUpdated();

    void ShowAllLanguageBarIcons();
    void HideAllLanguageBarIcons();

    inline CCandidateRange *GetCandidateListIndexRange() { return &_candidateListIndexRange; }
    inline UINT GetCandidateListPhraseModifier() { return _candidateListPhraseModifier; }
    inline UINT GetCandidateWindowWidth() { return _candidateWndWidth; }
    void SetFullWidthPunctuation(BOOL fFull) { _fFullWidthPunctuation = fFull; }
    BOOL GetFullWidthPunctuation() const { return _fFullWidthPunctuation; }
    void ChangeWborPinyinDictionary();      // 重载用户词库
    BOOL AddUserWord(LPCWSTR pszCode, LPCWSTR pszWord);  // 追加新词
    // 获取词语的编码（根据当前模式：五笔或拼音）
    std::wstring GetCodeForWord(const WCHAR* pszWord, DWORD_PTR cchWord);
    // 将词语+编码写入用户词库（根据模式和复选框）
    int AddUserWordWithOption(LPCWSTR pszCode, LPCWSTR pszWord, BOOL bAlsoAddPinyin);
    // 重新加载词库（公开）
    BOOL SetupDictionaryFile();
    void ReloadDictionaries();
    void SetInitialCandidateListRange();
private:
    CSampleIME* _pTextService;   // 新增成员
    ITfThreadMgr* _pThreadMgr;   // 保存的线程管理器
    void InitKeyStrokeTable();
    BOOL InitLanguageBar(_In_ CLangBarItemButton *pLanguageBar, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, REFGUID guidCompartment);

    struct _KEYSTROKE;
    BOOL IsVirtualKeyKeystrokeComposition(UINT uCode, _Out_opt_ _KEYSTROKE_STATE *pKeyState, KEYSTROKE_FUNCTION function);
    BOOL IsVirtualKeyKeystrokeCandidate(UINT uCode, _In_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode, _Out_ BOOL *pfRetCode, _In_ CSampleImeArray<_KEYSTROKE> *pKeystrokeMetric);
    BOOL IsKeystrokeRange(UINT uCode, _Out_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode);

    void SetupKeystroke();
    void SetupPreserved(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId);
    void SetupConfiguration();
    void SetupLanguageBar(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, BOOL isSecureMode);
    void SetKeystrokeTable(_Inout_ CSampleImeArray<_KEYSTROKE> *pKeystroke);
    void SetupPunctuationPair();


    void SetDefaultCandidateTextFont();
	void InitializeSampleIMECompartment(_In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId);

    class XPreservedKey;
    void SetPreservedKey(const CLSID clsid, TF_PRESERVEDKEY & tfPreservedKey, _In_z_ LPCWSTR pwszDescription, _Out_ XPreservedKey *pXPreservedKey);
    BOOL InitPreservedKey(_In_ XPreservedKey *pXPreservedKey, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId);
    BOOL CheckShiftKeyOnly(_In_ CSampleImeArray<TF_PRESERVEDKEY> *pTSFPreservedKeyTable);

    static HRESULT CompartmentCallback(_In_ void *pv, REFGUID guidCompartment);
    void PrivateCompartmentsUpdated();
    void KeyboardOpenCompartmentUpdated();

    CFile* GetDictionaryFile();

private:
    struct _KEYSTROKE
    {
        UINT VirtualKey;
        UINT Modifiers;
        KEYSTROKE_FUNCTION Function;

        _KEYSTROKE()
        {
            VirtualKey = 0;
            Modifiers = 0;
            Function = FUNCTION_NONE;
        }
    };
    _KEYSTROKE _keystrokeTable[26];

    CTableDictionaryEngine* _pTableDictionaryEngine;
    CFileMapping* _pUserDictionaryFile;
    CTableDictionaryEngine* _pUserDictionaryEngine;
    CTableDictionaryEngine* _pWubiDictionaryEngine;
    CFileMapping* _pWubiDictionaryFile;
    CTableDictionaryEngine* _pWubiUserDictionaryEngine;
    CFileMapping* _pWubiUserDictionaryFile;
    CStringRange _keystrokeBuffer;

    BOOL _hasWildcardIncludedInKeystrokeBuffer;

    LANGID _langid;
    GUID _guidProfile;
    TfClientId  _tfClientId;

    CSampleImeArray<_KEYSTROKE> _KeystrokeComposition;
    CSampleImeArray<_KEYSTROKE> _KeystrokeCandidate;
    CSampleImeArray<_KEYSTROKE> _KeystrokeCandidateWildcard;
    CSampleImeArray<_KEYSTROKE> _KeystrokeCandidateSymbol;
    CSampleImeArray<_KEYSTROKE> _KeystrokeSymbol;

    // Preserved key data
    class XPreservedKey
    {
    public:
        XPreservedKey();
        ~XPreservedKey();
        BOOL UninitPreservedKey(_In_ ITfThreadMgr *pThreadMgr);

    public:
        CSampleImeArray<TF_PRESERVEDKEY> TSFPreservedKeyTable;
        GUID Guid;
        LPCWSTR Description;
    };

    XPreservedKey _PreservedKey_IMEMode;
    XPreservedKey _PreservedKey_DoubleSingleByte;
    XPreservedKey _PreservedKey_Punctuation;
    XPreservedKey _PreservedKey_UserWord;   // 新增
    // Punctuation data
    CSampleImeArray<CPunctuationPair> _PunctuationPair;
    CSampleImeArray<CPunctuationNestPair> _PunctuationNestPair;

    // Language bar data
    CLangBarItemButton* _pLanguageBar_IMEMode;
    CLangBarItemButton* _pLanguageBar_DoubleSingleByte;
    CLangBarItemButton* _pLanguageBar_Punctuation;

    // Compartment
    CCompartment* _pCompartmentConversion;
    CCompartmentEventSink* _pCompartmentConversionEventSink;
    CCompartmentEventSink* _pCompartmentKeyboardOpenEventSink;
    CCompartmentEventSink* _pCompartmentDoubleSingleByteEventSink;
    CCompartmentEventSink* _pCompartmentPunctuationEventSink;

    // Configuration data
    unsigned _isWildcard : 1;
    unsigned _isDisableWildcardAtFirst : 1;
    unsigned _hasMakePhraseFromText : 1;
    unsigned _isKeystrokeSort : 1;
    unsigned _isComLessMode : 1;
    CCandidateRange _candidateListIndexRange;
    UINT _candidateListPhraseModifier;
    UINT _candidateWndWidth;

    CFileMapping* _pDictionaryFile;
    BOOL _fFullWidthPunctuation;   // TRUE=全角，FALSE=半角
    void _SyncFullWidthPunctuationWithOpenState();
    static const int OUT_OF_FILE_INDEX = -1;
    // 合并候选列表的辅助函数
    void _MergeCandidateLists(
        CSampleImeArray<CCandidateListItem>* pUserList,
        CSampleImeArray<CCandidateListItem>* pSystemList,
        CSampleImeArray<CCandidateListItem>* pResultList);
    mutable std::mutex _engineMutex;   // 普通互斥锁
    BOOL _SetupDictionaryFile();
};

