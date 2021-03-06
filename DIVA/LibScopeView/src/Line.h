//===-- LibScopeView/Line.h -------------------------------------*- C++ -*-===//
///
/// Copyright (c) Sony Interactive Entertainment Inc.
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to
/// deal in the Software without restriction, including without limitation the
/// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
/// sell copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
/// IN THE SOFTWARE.
///
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of the Line class.
///
//===----------------------------------------------------------------------===//

#ifndef LINE_H
#define LINE_H

#include "Object.h"

namespace LibScopeView {

/// \brief  Class to represent a single line info entry.
///
/// Contains a filename, line number and address.
class Line : public Element {
public:
  Line();
  Line(LevelType Lvl);
  virtual ~Line() override;

  Line &operator=(const Line &) = delete;
  Line(const Line &) = delete;

private:
  // Line Kind.
  static const char *KindDiscriminator;
  static const char *KindBasicBlock;
  static const char *KindEndSequence;
  static const char *KindEpilogueBegin;
  static const char *KindLine;
  static const char *KindNewStatement;
  static const char *KindPrologueEnd;
  static const char *KindUndefined;

private:
  // Flags specifying various properties of the line.
  enum LineAttributes {
    IsLineRecord,
    HasDiscriminator,
    IsLineEndSequence,
    IsNewBasicBlock,
    IsNewStatement,
    IsEpilogueBegin,
    IsPrologueEnd,
    LineAttributesSize
  };
  std::bitset<LineAttributesSize> LineAttributesFlags;

private:
  // Discriminator value (DW_LNE_set_discriminator). The DWARF standard
  // defines the discriminator as an unsigned LEB128 integer. In our case,
  // unless is required, we will use an unsigned half integer.
  Dwarf_Half Discriminator;

public:
  // Gets the line kind as a string (eg, "LINE").
  const char *getKindAsString() const override;

public:
  /// \brief Flags associated with the line.
  bool getIsLineRecord() const { return LineAttributesFlags[IsLineRecord]; }
  void setIsLineRecord() { LineAttributesFlags.set(IsLineRecord); }

  /// \brief Has any discriminator.
  bool getHasDiscriminator() const {
    return LineAttributesFlags[HasDiscriminator];
  }
  void setHasDiscriminator() { LineAttributesFlags.set(HasDiscriminator); }

  /// \brief Is line end sequence.
  bool getIsLineEndSequence() const {
    return LineAttributesFlags[IsLineEndSequence];
  }
  void setIsLineEndSequence() { LineAttributesFlags.set(IsLineEndSequence); }

  /// \brief Is new basic block.
  bool getIsNewBasicBlock() const {
    return LineAttributesFlags[IsNewBasicBlock];
  }
  void setIsNewBasicBlock() { LineAttributesFlags.set(IsNewBasicBlock); }

  /// \brief Is new statement.
  bool getIsNewStatement() const { return LineAttributesFlags[IsNewStatement]; }
  void setIsNewStatement() { LineAttributesFlags.set(IsNewStatement); }

  /// \brief Is epilogue begin.
  bool getIsEpilogueBegin() const {
    return LineAttributesFlags[IsEpilogueBegin];
  }
  void setIsEpilogueBegin() { LineAttributesFlags.set(IsEpilogueBegin); }

  /// \brief Is prologue end.
  bool getIsPrologueEnd() const { return LineAttributesFlags[IsPrologueEnd]; }
  void setIsPrologueEnd() { LineAttributesFlags.set(IsPrologueEnd); }

public:
  /// \brief Line address.
  Dwarf_Addr getAddress() const { return getDieOffset(); }
  void setAddress(Dwarf_Addr const Address) { setDieOffset(Address); }

  /// \brief Line discriminator.
  Dwarf_Half getDiscriminator() const override { return Discriminator; }
  void setDiscriminator(Dwarf_Half Discrim) override {
    Discriminator = Discrim;
    setHasDiscriminator();
  }

  /// \brief Line number for display.
  ///
  /// In the case of Inlined Functions, we use the DW_AT_call_line attribute;
  /// otherwise use DW_AT_decl_line attribute.
  const char *getLineNumberAsString() const override {
    return getLineAsString(getLineNumber());
  }
  std::string getLineNumberAsStringStripped() override;

public:
  void dump(const PrintSettings &Settings) override;
  virtual void dumpExtra(const PrintSettings &Settings);

  /// \brief Returns a text representation of this DIVA Object.
  std::string getAsText(const PrintSettings &Settings) const override;
  /// \brief Returns a YAML representation of this DIVA Object.
  std::string getAsYAML() const override;

private:
  static uint32_t LinesAllocated;

public:
  static uint32_t getInstanceCount() { return LinesAllocated; }
  uint32_t getTag() const override;
  void setTag() override;
};

} // namespace LibScopeView

#endif // LINE_H
