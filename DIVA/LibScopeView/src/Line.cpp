//===-- LibScopeView/Line.cpp -----------------------------------*- C++ -*-===//
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
/// Implementation for the Line class.
///
//===----------------------------------------------------------------------===//

#include "Line.h"
#include "PrintContext.h"
#include "Reader.h"
#include "Utilities.h"

#include <sstream>

using namespace LibScopeView;

Line::Line(LevelType Lvl) : Element(Lvl), Discriminator(0) {
  setIsLine();

  Line::setTag();
}

Line::Line() : Element(), Discriminator(0) {
  setIsLine();

  Line::setTag();
}

Line::~Line() {}

uint32_t Line::LinesAllocated = 0;

void Line::setTag() {
  ++Line::LinesAllocated;
#ifndef NDEBUG
  Tag = Line::LinesAllocated;
#endif
}

uint32_t Line::getTag() const {
#ifndef NDEBUG
  return Tag;
#else
  return 0;
#endif
}

// Line Kind.
const char *Line::KindBasicBlock = "BasicBlock";
const char *Line::KindDiscriminator = "Discriminator";
const char *Line::KindEndSequence = "EndSequence";
const char *Line::KindEpilogueBegin = "EpilogueBegin";
const char *Line::KindLine = "CodeLine";
const char *Line::KindNewStatement = "NewStatement";
const char *Line::KindPrologueEnd = "PrologueEnd";
const char *Line::KindUndefined = "Undefined";

const char *Line::getKindAsString() const {
  const char *Kind = KindUndefined;
  if (getIsLineRecord()) {
    Kind = KindLine;
  }
  return Kind;
}

std::string Line::getLineNumberAsStringStripped() {
  std::string number = getLineNumberAsString();
  return trim(number);
}

void Line::dump(const PrintSettings &Settings) {
  if (Settings.printObject(*this)) {
    // Object Summary Table.
    getReader()->incrementPrinted(this);

    // Common Object Data.
    Element::dump(Settings);

    // Specific Object Data.
    dumpExtra(Settings);
  }
}

void Line::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string Line::getAsText(const PrintSettings &Settings) const {
  std::stringstream Result;
  Result << '{' << getKindAsString() << '}';
  if (Settings.ShowCodelineAttributes) {
    if (getIsNewStatement()) {
      Result << '\n' << getAttributeInfoAsText(KindNewStatement, Settings);
    }
    if (getIsPrologueEnd()) {
      Result << '\n' << getAttributeInfoAsText(KindPrologueEnd, Settings);
    }
    if (getIsLineEndSequence()) {
      Result << '\n' << getAttributeInfoAsText(KindEndSequence, Settings);
    }
    if (getIsNewBasicBlock()) {
      Result << '\n' << getAttributeInfoAsText(KindBasicBlock, Settings);
    }
    if (getHasDiscriminator()) {
      Result << '\n' << getAttributeInfoAsText(KindDiscriminator, Settings);
    }
    if (getIsEpilogueBegin()) {
      Result << '\n' << getAttributeInfoAsText(KindEpilogueBegin, Settings);
    }
  }
  return Result.str();
}

std::string Line::getAsYAML() const {
  std::stringstream YAML;
  std::stringstream Attrs;
  const std::string YAMLTrue(": true");
  const std::string YAMLFalse(": false");
  Attrs << "\n  " << KindNewStatement
        << (getIsNewStatement() ? YAMLTrue : YAMLFalse);
  Attrs << "\n  " << KindPrologueEnd
        << (getIsPrologueEnd() ? YAMLTrue : YAMLFalse);
  Attrs << "\n  " << KindEndSequence
        << (getIsLineEndSequence() ? YAMLTrue : YAMLFalse);
  Attrs << "\n  " << KindBasicBlock
        << (getIsNewBasicBlock() ? YAMLTrue : YAMLFalse);
  Attrs << "\n  " << KindDiscriminator
        << (getHasDiscriminator() ? YAMLTrue : YAMLFalse);
  Attrs << "\n  " << KindEpilogueBegin
        << (getIsEpilogueBegin() ? YAMLTrue : YAMLFalse);

  if (Attrs.str().empty())
    Attrs << " {}";

  YAML << getCommonYAML() << "\nattributes:" << Attrs.str();
  return YAML.str();
}
