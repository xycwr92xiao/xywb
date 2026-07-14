// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "DictionarySearch.h"

//+---------------------------------------------------------------------------
//
// CollectWord
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CSampleImeArray<CStringRange> *pWordStrings)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CStringRange* pPhrase = nullptr;
            pPhrase = pWordStrings->Append();
            if (pPhrase)
            {
                *pPhrase = *pdret->_FindPhraseList.GetAt(index);
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordForWildcard(_In_ CStringRange *pKeyCode, _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhraseForWildcard(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordFromConvertedStringForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordFromConvertedStringForWildcard(_In_ CStringRange *pString, _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pString);

    while (dshSearch.FindConvertedStringForWildcard(&pdret)) // TAIL ALL CHAR MATCH
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(index));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

// TableDictionaryEngine.cpp
std::wstring CTableDictionaryEngine::GetCodeForWord(const WCHAR* pszWord, DWORD_PTR cchWord) {

    if (!_pDictionaryFile) return L"";
    std::wstring result;
    const WCHAR* pBuf = _pDictionaryFile->GetBuffer();
    if (!pBuf) {
         return GetCodeFromFile(pszWord, cchWord, Global::isPinyinMode?FALSE:TRUE);
    } 
    DWORD_PTR fileSize = _pDictionaryFile->GetSize() / sizeof(WCHAR);
    DWORD_PTR pos = 0;

    CStringRange target;
    target.Set(pszWord, cchWord);

    CDictionaryParser parser(_locale);   // 用于解析行
    while (pos < fileSize) {
        DWORD_PTR lineLen = CDictionaryParser::GetOneLine(pBuf + pos, fileSize - pos);
        if (lineLen == 0) break;

        CParserStringRange keyword;
        CSampleImeArray<CParserStringRange> values;
        if (parser.ParseLine(pBuf + pos, lineLen, &keyword, &values)) {
            for (UINT i = 0; i < values.Count(); i++) {
                if (CStringRange::Compare(_locale, &target, values.GetAt(i)) == CSTR_EQUAL) {
                    // 检查编码长度是否 ≥ 2std::wstring(keyword.Get(), keyword.GetLength())
                    
                    if (keyword.GetLength() >= 2) {
                        result = std::wstring(keyword.Get(), keyword.GetLength());
                        // 安全复制接收的词语（可能不以空字符结尾）
                        WCHAR dbgWord[256] = { 0 };
                        size_t copyLen = min((size_t)cchWord, 255);
                        wcsncpy_s(dbgWord, _countof(dbgWord), pszWord, copyLen);
                        dbgWord[copyLen] = L'\0';
                        OutputDebugString(L"TableDictionaryEngine::GetCodeForWord转换或查询的汉字词语是: word=[");
                        OutputDebugString(dbgWord);
                        OutputDebugString(L"] 编码是code=[");
                        if (!result.empty()) {
                            // 安全复制查询到的编码
                            OutputDebugString(result.c_str());
                        }
                        return result;   // 找到符合条件的编码，返回
                    }
                }
            }
        }
        pos += lineLen;
        // 跳过行尾 CR/LF
        while (pos < fileSize && (pBuf[pos] == L'\r' || pBuf[pos] == L'\n'))
            pos++;
    }
    return result;
}

std::wstring CTableDictionaryEngine::GetCodeFromFile(const WCHAR* pszWord, DWORD_PTR cchWord,BOOL isFromWubiBase) {
    // 参数检查
    if (!pszWord || cchWord == 0) return L"";

    // 获取系统词库文件路径
    WCHAR szDictPath[MAX_PATH];
    StringCchPrintf(szDictPath, MAX_PATH, L"%s%s", g_szDllPath, isFromWubiBase ? TEXTSERVICE_DIC : TEXTSERVICE_PYDIC);

    // 打开文件（只读，共享读）
    HANDLE hFile = CreateFile(szDictPath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return L"";
    }

    // 获取文件大小
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0) {
        CloseHandle(hFile);
        return L"";
    }

    // 读取文件内容到内存（临时）
    WCHAR* pBuf = new (std::nothrow) WCHAR[fileSize / sizeof(WCHAR) + 2];
    if (!pBuf) {
        CloseHandle(hFile);
        return L"";
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, pBuf, fileSize, &bytesRead, NULL)) {
        delete[] pBuf;
        CloseHandle(hFile);
        return L"";
    }
    CloseHandle(hFile);
    pBuf[bytesRead / sizeof(WCHAR)] = L'\0';  // 确保以空字符结尾

    // 逐行解析
    CDictionaryParser parser(_locale);
    const WCHAR* p = pBuf;
    DWORD_PTR remaining = bytesRead / sizeof(WCHAR);
    while (remaining > 0) {
        DWORD_PTR lineLen = CDictionaryParser::GetOneLine(p, remaining);
        if (lineLen == 0) break;

        CParserStringRange keyword;
        CSampleImeArray<CParserStringRange> values;
        if (parser.ParseLine(p, lineLen, &keyword, &values)) {
            // 遍历该行的所有词语，匹配目标字符串
            for (UINT i = 0; i < values.Count(); ++i) {
                CStringRange target;
                target.Set(pszWord, cchWord);
                if (CStringRange::Compare(_locale, &target, values.GetAt(i)) == CSTR_EQUAL) {
                    if (keyword.GetLength() >= 2) {
                        // 找到匹配，返回编码
                        std::wstring result(keyword.Get(), keyword.GetLength());
                        delete[] pBuf;
                        return result;
                    }
                }
            }
        }
        // 移动到下一行
        remaining -= lineLen;
        p += lineLen;
        // 跳过行尾的 \r\n
        while (remaining > 0 && (*p == L'\r' || *p == L'\n')) {
            ++p;
            --remaining;
        }
    }

    delete[] pBuf;
    return L"";
}