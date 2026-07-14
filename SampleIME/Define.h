// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once
#include "resource.h"

#define TEXTSERVICE_MODEL        L"Apartment"
#define TEXTSERVICE_LANGID       MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)
#define TEXTSERVICE_ICON_INDEX   -IDIS_SAMPLEIME
#define TEXTSERVICE_DIC L"xywb_system_words.txt"
#define TEXTSERVICE_UDIC L"xywb_user_words.txt"
#define TEXTSERVICE_PYDIC L"xypy_system_words.txt"
#define TEXTSERVICE_PYUDIC L"xypy_user_words.txt"
#define TEXTSERVICE_FUHAO L"xywb_emoticon.txt"
#define TEXTSERVICE_EMOJI L"xywb_emoji.txt"

#define IME_MODE_ON_ICON_INDEX      IDI_IME_MODE_ON
#define IME_MODE_OFF_ICON_INDEX     IDI_IME_MODE_OFF
#define IME_DOUBLE_ON_INDEX         IDI_DOUBLE_SINGLE_BYTE_ON
#define IME_DOUBLE_OFF_INDEX        IDI_DOUBLE_SINGLE_BYTE_OFF
#define IME_PUNCTUATION_ON_INDEX    IDI_PUNCTUATION_ON
#define IME_PUNCTUATION_OFF_INDEX   IDI_PUNCTUATION_OFF

#define SAMPLEIME_FONT_DEFAULT L"Microsoft YaHei UI"

//---------------------------------------------------------------------
// defined Candidated Window
//---------------------------------------------------------------------
#define CANDWND_ROW_WIDTH				(30)
#define CANDWND_BORDER_COLOR			(RGB(0x00, 0x00, 0x00))
#define CANDWND_BORDER_WIDTH			(2)
#define CANDWND_NUM_COLOR				(RGB(0xA4, 0xA4, 0xA4))//序号颜色为灰色
#define CANDWND_SELECTED_ITEM_COLOR		(RGB(0xFF, 0xFF, 0xFF))//选中项文字色为白色
#define CANDWND_SELECTED_BK_COLOR		(RGB(0xA6, 0xA6, 0x00))//选中项背景色为黄色
#define CANDWND_ITEM_COLOR				(RGB(0x00, 0x00, 0x00))

//---------------------------------------------------------------------
// defined modifier
//---------------------------------------------------------------------
#define _TF_MOD_ON_KEYUP_SHIFT_ONLY    (0x00010000 | TF_MOD_ON_KEYUP)
#define _TF_MOD_ON_KEYUP_CONTROL_ONLY  (0x00020000 | TF_MOD_ON_KEYUP)
#define _TF_MOD_ON_KEYUP_ALT_ONLY      (0x00040000 | TF_MOD_ON_KEYUP)

#define CAND_WIDTH     (13)      // * tmMaxCharWidth
#ifdef _WIN64
#define IMEDLL_BITS 64
#else
#define IMEDLL_BITS 32
#endif
//---------------------------------------------------------------------
// string length of CLSID
//---------------------------------------------------------------------
#define CLSID_STRLEN    (38)  // strlen("{xxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxx}")
