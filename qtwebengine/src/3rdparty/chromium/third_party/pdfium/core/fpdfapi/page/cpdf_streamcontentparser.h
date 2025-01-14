// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_
#define CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_

#include <map>
#include <memory>
#include <set>
#include <stack>
#include <vector>

#include "core/fpdfapi/page/cpdf_contentmark.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxge/cfx_pathdata.h"

class CPDF_AllStates;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Font;
class CPDF_Image;
class CPDF_ImageObject;
class CPDF_PageObject;
class CPDF_PageObjectHolder;
class CPDF_StreamParser;
class CPDF_TextObject;
class CPDF_ColorSpace;
class CPDF_Pattern;

class CPDF_StreamContentParser {
 public:
  CPDF_StreamContentParser(CPDF_Document* pDoc,
                           CPDF_Dictionary* pPageResources,
                           CPDF_Dictionary* pParentResources,
                           const CFX_Matrix* pmtContentToUser,
                           CPDF_PageObjectHolder* pObjectHolder,
                           CPDF_Dictionary* pResources,
                           const CFX_FloatRect& rcBBox,
                           CPDF_AllStates* pAllStates,
                           std::set<const uint8_t*>* parsedSet);
  ~CPDF_StreamContentParser();

  uint32_t Parse(const uint8_t* pData,
                 uint32_t dwSize,
                 uint32_t max_cost,
                 const std::vector<uint32_t>& stream_start_offsets);
  CPDF_PageObjectHolder* GetPageObjectHolder() const {
    return m_pObjectHolder.Get();
  }
  CPDF_AllStates* GetCurStates() const { return m_pCurStates.get(); }
  bool IsColored() const { return m_bColored; }
  const float* GetType3Data() const { return m_Type3Data; }
  CPDF_Font* FindFont(const ByteString& name);

  static ByteStringView FindKeyAbbreviationForTesting(
      const ByteStringView& abbr);
  static ByteStringView FindValueAbbreviationForTesting(
      const ByteStringView& abbr);

 private:
  struct ContentParam {
    enum Type { OBJECT = 0, NUMBER, NAME };

    ContentParam();
    ~ContentParam();

    Type m_Type;
    std::unique_ptr<CPDF_Object> m_pObject;
    struct {
      bool m_bInteger;
      union {
        int m_Integer;
        float m_Float;
      };
    } m_Number;
    struct {
      int m_Len;
      char m_Buffer[32];
    } m_Name;
  };

  static const int kParamBufSize = 16;

  using OpCodes = std::map<uint32_t, void (CPDF_StreamContentParser::*)()>;
  static OpCodes InitializeOpCodes();

  void AddNameParam(const ByteStringView& str);
  void AddNumberParam(const ByteStringView& str);
  void AddObjectParam(std::unique_ptr<CPDF_Object> pObj);
  int GetNextParamPos();
  void ClearAllParams();
  CPDF_Object* GetObject(uint32_t index);
  ByteString GetString(uint32_t index) const;
  float GetNumber(uint32_t index) const;
  // Calls GetNumber() |count| times and returns the values in reverse order.
  // e.g. for |count| = 3, returns [GetNumber(2), GetNumber(1), GetNumber(0)].
  std::vector<float> GetNumbers(size_t count) const;
  int GetInteger(uint32_t index) const {
    return static_cast<int>(GetNumber(index));
  }
  void OnOperator(const ByteStringView& op);
  void AddTextObject(const ByteString* pStrs,
                     float fInitKerning,
                     const std::vector<float>& kernings,
                     size_t nSegs);
  float GetHorizontalTextSize(float fKerning) const;
  float GetVerticalTextSize(float fKerning) const;

  void OnChangeTextMatrix();
  void ParsePathObject();
  void AddPathPoint(float x, float y, FXPT_TYPE type, bool close);
  void AddPathRect(float x, float y, float w, float h);
  void AddPathObject(int FillType, bool bStroke);
  CPDF_ImageObject* AddImage(std::unique_ptr<CPDF_Stream> pStream);
  CPDF_ImageObject* AddImage(uint32_t streamObjNum);
  CPDF_ImageObject* AddImage(const RetainPtr<CPDF_Image>& pImage);

  void AddForm(CPDF_Stream* pStream);
  void SetGraphicStates(CPDF_PageObject* pObj,
                        bool bColor,
                        bool bText,
                        bool bGraph);
  CPDF_ColorSpace* FindColorSpace(const ByteString& name);
  CPDF_Pattern* FindPattern(const ByteString& name, bool bShading);
  CPDF_Object* FindResourceObj(const ByteString& type, const ByteString& name);

  // Takes ownership of |pImageObj|, returns unowned pointer to it.
  CPDF_ImageObject* AddImageObject(std::unique_ptr<CPDF_ImageObject> pImageObj);

  std::vector<float> GetColors() const;
  std::vector<float> GetNamedColors() const;
  int32_t GetCurrentStreamIndex();

  void Handle_CloseFillStrokePath();
  void Handle_FillStrokePath();
  void Handle_CloseEOFillStrokePath();
  void Handle_EOFillStrokePath();
  void Handle_BeginMarkedContent_Dictionary();
  void Handle_BeginImage();
  void Handle_BeginMarkedContent();
  void Handle_BeginText();
  void Handle_CurveTo_123();
  void Handle_ConcatMatrix();
  void Handle_SetColorSpace_Fill();
  void Handle_SetColorSpace_Stroke();
  void Handle_SetDash();
  void Handle_SetCharWidth();
  void Handle_SetCachedDevice();
  void Handle_ExecuteXObject();
  void Handle_MarkPlace_Dictionary();
  void Handle_EndImage();
  void Handle_EndMarkedContent();
  void Handle_EndText();
  void Handle_FillPath();
  void Handle_FillPathOld();
  void Handle_EOFillPath();
  void Handle_SetGray_Fill();
  void Handle_SetGray_Stroke();
  void Handle_SetExtendGraphState();
  void Handle_ClosePath();
  void Handle_SetFlat();
  void Handle_BeginImageData();
  void Handle_SetLineJoin();
  void Handle_SetLineCap();
  void Handle_SetCMYKColor_Fill();
  void Handle_SetCMYKColor_Stroke();
  void Handle_LineTo();
  void Handle_MoveTo();
  void Handle_SetMiterLimit();
  void Handle_MarkPlace();
  void Handle_EndPath();
  void Handle_SaveGraphState();
  void Handle_RestoreGraphState();
  void Handle_Rectangle();
  void Handle_SetRGBColor_Fill();
  void Handle_SetRGBColor_Stroke();
  void Handle_SetRenderIntent();
  void Handle_CloseStrokePath();
  void Handle_StrokePath();
  void Handle_SetColor_Fill();
  void Handle_SetColor_Stroke();
  void Handle_SetColorPS_Fill();
  void Handle_SetColorPS_Stroke();
  void Handle_ShadeFill();
  void Handle_SetCharSpace();
  void Handle_MoveTextPoint();
  void Handle_MoveTextPoint_SetLeading();
  void Handle_SetFont();
  void Handle_ShowText();
  void Handle_ShowText_Positioning();
  void Handle_SetTextLeading();
  void Handle_SetTextMatrix();
  void Handle_SetTextRenderMode();
  void Handle_SetTextRise();
  void Handle_SetWordSpace();
  void Handle_SetHorzScale();
  void Handle_MoveToNextLine();
  void Handle_CurveTo_23();
  void Handle_SetLineWidth();
  void Handle_Clip();
  void Handle_EOClip();
  void Handle_CurveTo_13();
  void Handle_NextLineShowText();
  void Handle_NextLineShowText_Space();
  void Handle_Invalid();

  UnownedPtr<CPDF_Document> const m_pDocument;
  UnownedPtr<CPDF_Dictionary> const m_pPageResources;
  UnownedPtr<CPDF_Dictionary> const m_pParentResources;
  UnownedPtr<CPDF_Dictionary> const m_pResources;
  UnownedPtr<CPDF_PageObjectHolder> const m_pObjectHolder;
  UnownedPtr<std::set<const uint8_t*>> const m_ParsedSet;
  CFX_Matrix m_mtContentToUser;
  const CFX_FloatRect m_BBox;
  uint32_t m_ParamStartPos;
  uint32_t m_ParamCount;
  UnownedPtr<CPDF_StreamParser> m_pSyntax;
  std::unique_ptr<CPDF_AllStates> m_pCurStates;
  std::stack<std::unique_ptr<CPDF_ContentMark>> m_ContentMarksStack;
  std::vector<std::unique_ptr<CPDF_TextObject>> m_ClipTextList;
  UnownedPtr<CPDF_TextObject> m_pLastTextObject;
  float m_DefFontSize;
  std::vector<FX_PATHPOINT> m_PathPoints;
  float m_PathStartX;
  float m_PathStartY;
  float m_PathCurrentX;
  float m_PathCurrentY;
  uint8_t m_PathClipType;
  ByteString m_LastImageName;
  RetainPtr<CPDF_Image> m_pLastImage;
  bool m_bColored;
  bool m_bResourceMissing;
  std::vector<std::unique_ptr<CPDF_AllStates>> m_StateStack;
  float m_Type3Data[6];
  ContentParam m_ParamBuf[kParamBufSize];
  std::vector<uint32_t> m_StreamStartOffsets;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_STREAMCONTENTPARSER_H_
