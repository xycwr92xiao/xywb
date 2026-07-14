// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "DictionaryParser.h"
#include "SampleIMEBaseStructure.h"

//---------------------------------------------------------------------
//
// ctor
//
//---------------------------------------------------------------------

CDictionaryParser::CDictionaryParser(LCID locale)
{
    _locale = locale;
}

//---------------------------------------------------------------------
//
// dtor
//
//---------------------------------------------------------------------

CDictionaryParser::~CDictionaryParser()
{
}

//---------------------------------------------------------------------
//
// ParseLine
//
// dwBufLen - in character count
//
//---------------------------------------------------------------------
// DictionaryParser.cpp
BOOL CDictionaryParser::ParseLine(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen,
    _Out_ CParserStringRange* psrgKeyword,
    _Inout_opt_ CSampleImeArray<CParserStringRange>* pValue)
{
    if (dwBufLen == 0) return FALSE;

    // 1. 跳过行首空白
    DWORD_PTR start = 0;
    while (start < dwBufLen && IsSpace(_locale, pwszBuffer[start])) start++;
    if (start >= dwBufLen) return FALSE;  // 空行

    // 2. 查找第一个空格位置（编码结束）
    DWORD_PTR keyword_end = start;
    while (keyword_end < dwBufLen && !IsSpace(_locale, pwszBuffer[keyword_end])) keyword_end++;
    if (keyword_end == start) return FALSE;  // 没有编码

    // 提取编码
    psrgKeyword->Set(pwszBuffer + start, keyword_end - start);

    // 如果不需要解析值，直接返回
    if (pValue == nullptr) return TRUE;

    // 3. 跳过编码后的空白
    DWORD_PTR value_start = keyword_end;
    while (value_start < dwBufLen && IsSpace(_locale, pwszBuffer[value_start])) value_start++;
    if (value_start >= dwBufLen) return TRUE;  // 只有编码，没有词语

    // 4. 按空白分割剩余的词语
    while (value_start < dwBufLen) {
        // 跳过空白（已在循环开始时保证 value_start 指向非空白）
        DWORD_PTR token_start = value_start;
        DWORD_PTR token_end = value_start;
        while (token_end < dwBufLen && !IsSpace(_locale, pwszBuffer[token_end])) token_end++;
        if (token_end > token_start) {
            CParserStringRange* pNewValue = pValue->Append();
            if (pNewValue) {
                pNewValue->Set(pwszBuffer + token_start, token_end - token_start);
            }
        }
        // 跳到下一个非空白字符
        value_start = token_end;
        while (value_start < dwBufLen && IsSpace(_locale, pwszBuffer[value_start])) value_start++;
    }

    return TRUE;
}
//BOOL CDictionaryParser::ParseLine(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen, _Out_ CParserStringRange *psrgKeyword, _Inout_opt_ CSampleImeArray<CParserStringRange> *pValue)
//{
//    LPCWSTR pwszKeyWordDelimiter = nullptr;
//    pwszKeyWordDelimiter = GetToken(pwszBuffer, dwBufLen, Global::KeywordDelimiter, psrgKeyword);
//    if (!(pwszKeyWordDelimiter))
//    {
//        return FALSE;    // End of file
//    }
//
//    dwBufLen -= (pwszKeyWordDelimiter - pwszBuffer);
//    pwszBuffer = pwszKeyWordDelimiter + 1;
//    dwBufLen--;
//
//    // Get value.
//    if (pValue)
//    {
//        if (dwBufLen)
//        {
//            CParserStringRange* psrgValue = pValue->Append();
//            if (!psrgValue)
//            {
//                return FALSE;
//            }
//            psrgValue->Set(pwszBuffer, dwBufLen);
//            RemoveWhiteSpaceFromBegin(psrgValue);
//            RemoveWhiteSpaceFromEnd(psrgValue);
//            RemoveStringDelimiter(psrgValue);
//        }
//    }
//
//    return TRUE;
//}

//---------------------------------------------------------------------
//
// GetToken
//
// dwBufLen - in character count
//
// return   - pointer of delimiter which specified chDelimiter
//
//---------------------------------------------------------------------
_Ret_maybenull_
LPCWSTR CDictionaryParser::GetToken(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen, _In_ const WCHAR chDelimiter, _Out_ CParserStringRange *psrgValue)
{
    WCHAR ch = '\0';

    psrgValue->Set(pwszBuffer, dwBufLen);

    ch = *pwszBuffer;
    while ((ch) && (ch != chDelimiter) && dwBufLen)
    {
        dwBufLen--;
        pwszBuffer++;

        if (ch == Global::StringDelimiter)
        {
            while (*pwszBuffer && (*pwszBuffer != Global::StringDelimiter) && dwBufLen)
            {
                dwBufLen--;
                pwszBuffer++;
            }
            if (*pwszBuffer && dwBufLen)
            {
                dwBufLen--;
                pwszBuffer++;
            }
            else
            {
                return nullptr;
            }
        }
        ch = *pwszBuffer;
    }

    if (*pwszBuffer && dwBufLen)
    {
        LPCWSTR pwszStart = psrgValue->Get();

        psrgValue->Set(pwszStart, pwszBuffer - pwszStart);

        RemoveWhiteSpaceFromBegin(psrgValue);
        RemoveWhiteSpaceFromEnd(psrgValue);
        RemoveStringDelimiter(psrgValue);

        return pwszBuffer;
    }

    RemoveWhiteSpaceFromBegin(psrgValue);
    RemoveWhiteSpaceFromEnd(psrgValue);
    RemoveStringDelimiter(psrgValue);

    return nullptr;
}

//---------------------------------------------------------------------
//
// RemoveWhiteSpaceFromBegin
// RemoveWhiteSpaceFromEnd
// RemoveStringDelimiter
//
//---------------------------------------------------------------------

BOOL CDictionaryParser::RemoveWhiteSpaceFromBegin(_Inout_opt_ CStringRange *pString)
{
    DWORD_PTR dwIndexTrace = 0;  // in char

    if (pString == nullptr)
    {
        return FALSE;
    }

    if (SkipWhiteSpace(_locale, pString->Get(), pString->GetLength(), &dwIndexTrace) != S_OK)
    {
        return FALSE;
    }

    pString->Set(pString->Get() + dwIndexTrace, pString->GetLength() - dwIndexTrace);
    return TRUE;
}

BOOL CDictionaryParser::RemoveWhiteSpaceFromEnd(_Inout_opt_ CStringRange *pString)
{
    if (pString == nullptr)
    {
        return FALSE;
    }

    DWORD_PTR dwTotalBufLen = pString->GetLength();
    LPCWSTR pwszEnd = pString->Get() + dwTotalBufLen - 1;

    while (dwTotalBufLen && (IsSpace(_locale, *pwszEnd) || *pwszEnd == L'\r' || *pwszEnd == L'\n'))
    {
        pwszEnd--;
        dwTotalBufLen--;
    }

    pString->Set(pString->Get(), dwTotalBufLen);
    return TRUE;
}

BOOL CDictionaryParser::RemoveStringDelimiter(_Inout_opt_ CStringRange *pString)
{
    if (pString == nullptr)
    {
        return FALSE;
    }

    if (pString->GetLength() >= 2)
    {
        if ((*pString->Get() == Global::StringDelimiter) && (*(pString->Get()+pString->GetLength()-1) == Global::StringDelimiter))
        {
            pString->Set(pString->Get()+1, pString->GetLength()-2);
            return TRUE;
        }
    }

    return FALSE;
}

//---------------------------------------------------------------------
//
// GetOneLine
//
// dwBufLen - in character count
//
//---------------------------------------------------------------------

DWORD_PTR CDictionaryParser::GetOneLine(_In_z_ LPCWSTR pwszBuffer, DWORD_PTR dwBufLen)
{
    DWORD_PTR dwIndexTrace = 0;     // in char

    if (FAILED(FindChar(L'\r', pwszBuffer, dwBufLen, &dwIndexTrace)))
    {
        if (FAILED(FindChar(L'\0', pwszBuffer, dwBufLen, &dwIndexTrace)))
        {
            return dwBufLen;
        }
    }

    return dwIndexTrace;
}
