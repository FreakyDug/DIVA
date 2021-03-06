//===-- LibScopeView/Object.cpp ---------------------------------*- C++ -*-===//
///
/// Copyright (c) 2017 by Sony Interactive Entertainment Inc.
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
/// Implementation for the Object and Element classes.
///
//===----------------------------------------------------------------------===//

#include "FileUtilities.h"
#include "Line.h"
#include "PrintContext.h"
#include "Reader.h"
#include "StringPool.h"
#include "Symbol.h"
#include "Type.h"
#include "Utilities.h"

// Disable some clang warnings for dwarf.h.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#include "dwarf.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <assert.h>
#include <cstring>
#include <sstream>

using namespace LibScopeView;

void LibScopeView::printAllocationInfo() {
#ifndef NDEBUG
  // Data structure sizes.
  GlobalPrintContext->print("\n** Size of data structures: **\n");
  GlobalPrintContext->print("Scope:  %3d\n", sizeof(Scope));
  GlobalPrintContext->print("Symbol: %3d\n", sizeof(Symbol));
  GlobalPrintContext->print("Type:   %3d\n", sizeof(Type));
  GlobalPrintContext->print("Line:   %3d\n", sizeof(Line));
#endif // NDEBUG

  GlobalPrintContext->print("\n** Allocated Objects: **\n");
  GlobalPrintContext->print("%s %6d\n", "Scopes:  ", Scope::getInstanceCount());
  GlobalPrintContext->print("%s %6d\n",
                            "Symbols: ", Symbol::getInstanceCount());
  GlobalPrintContext->print("%s %6d\n", "Types:   ", Type::getInstanceCount());
  GlobalPrintContext->print("%s %6d\n", "Lines:   ", Line::getInstanceCount());
}

//===----------------------------------------------------------------------===//
// Class to represent the logical view of an object.
//===----------------------------------------------------------------------===//

Object::Object(LevelType Lvl) {
  commonConstructor();

  Level = Lvl;
}

Object::Object() { commonConstructor(); }

Object::~Object() {}

void Object::commonConstructor() {

  LineNumber = 0;
  Parent = nullptr;
  Level = 0;
  DieOffset = 0;
  DieTag = 0;

#ifndef NDEBUG
  Tag = 0;
#endif
}

void Object::setTag() {
#ifndef NDEBUG
  Tag = 0;
#endif
}

uint32_t Object::getTag() const {
#ifndef NDEBUG
  return Tag;
#else
  return 0;
#endif
}

namespace {

const char *OffsetAsString(Dwarf_Off Offset) {
  // [0x00000000]
  const unsigned MaxLineSize = 16;
  static char Buffer[MaxLineSize];
  int Res = snprintf(Buffer, MaxLineSize, "[0x%08" DW_PR_DUx "]", Offset);
  assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxLineSize) &&
         "string overflow");
  (void)Res;
  return Buffer;
}

} // namespace

const char *Object::getDieOffsetAsString(const PrintSettings &Settings) const {
  const char *Str = "";
  if (Settings.ShowDWARFOffset)
    Str = OffsetAsString(getDieOffset());
  return Str;
}

const char *
Object::getTypeDieOffsetAsString(const PrintSettings &Settings) const {
  const char *Str = "";
  if (Settings.ShowDWARFOffset) {
    Str = OffsetAsString(getType() ? getType()->getDieOffset() : 0);
  }
  return Str;
}

Dwarf_Off Object::getDieParent() const {
  Object *DieParent = getParent();
  return DieParent ? DieParent->getDieOffset() : 0;
}

const char *Object::getNoLineString() const {
  static const char *NoLine = "        ";
  return NoLine;
}

const char *Object::getLineAsString(uint64_t LnNumber) const {
  const unsigned MaxLineSize = 16;
  static char Buffer[MaxLineSize];
  const char *Str = Buffer;
  if (LnNumber) {
    int Res;
    Res = snprintf(Buffer, MaxLineSize, "%5s %s",
                   std::to_string(LnNumber).c_str(), "  ");
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxLineSize) &&
           "string overflow");
    (void)Res;
  } else {
    Str = getNoLineString();
  }
  return Str;
}

std::string Object::getLineNumberAsStringStripped() {
  return trim(getLineNumberAsString());
}

const char *Object::getReferenceAsString(uint64_t LnNumber, bool Spaces) const {
  const char *Str = "";
  if (LnNumber) {
    const unsigned MaxLineSize = 16;
    static char Buffer[MaxLineSize];
    int Res = snprintf(Buffer, MaxLineSize, "@%s%s",
                       std::to_string(LnNumber).c_str(), Spaces ? " " : "");
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxLineSize) &&
           "string overflow");
    (void)Res;
    Str = Buffer;
  }
  return Str;
}

const char *Object::getTypeAsString(const PrintSettings &Settings) const {
  return getHasType() ? (getTypeName()) : (Settings.ShowVoid ? "void" : "");
}

void Object::resolveQualifiedName(const Scope *ExplicitParent) {
  std::string QualifiedName;

  // Get the qualified name, excluding the Compile Unit, Functions, and the
  // scope root.
  const Object *ObjParent = ExplicitParent;
  while (ObjParent && !ObjParent->getIsCompileUnit() &&
         !ExplicitParent->getIsFunction() &&
         !(ObjParent->getIsScope() &&
           static_cast<const Scope *>(ObjParent)->getIsRoot())) {

    if (strlen(ObjParent->getName()) != 0) {
      QualifiedName.insert(0, "::");
      QualifiedName.insert(0, ObjParent->getName());
    }
    ObjParent = ObjParent->getParent();
  }

  if (!QualifiedName.empty()) {
    setQualifiedName(QualifiedName.c_str());
    setHasQualifiedName();
  }
}

std::string Object::getIndentString(const PrintSettings &Settings) const {
  // No indent for root.
  if (getLevel() == 0 && getIsScope() && getParent() == nullptr)
    return "";
  return Settings.ShowIndent ? std::string((getLevel() + 1) * 2, ' ') : "";
}

bool Object::referenceMatch(const Object *Obj) const {
  if ((getHasReference() && !Obj->getHasReference()) ||
      (!getHasReference() && Obj->getHasReference())) {
    return false;
  }

  return true;
}

bool Object::setFullName(const PrintSettings &Settings, Type *BaseType,
                         Scope *BaseScope, Scope *SpecScope,
                         const char *BaseText) {
  // In the case of scopes that have been updated using the specification
  // or abstract_origin attributes, the name already contain some patterns,
  // such as '()' or 'class'; for that case do not add the pattern.
  const char *ParentTypename = nullptr;
  if (BaseType) {
    ParentTypename = BaseType->getName();
  } else {
    if (BaseScope) {
      ParentTypename = BaseScope->getName();
    }
  }

  const char *BaseParent = nullptr;
  const char *PreText = nullptr;
  const char *PostText = nullptr;
  const char *PostPostText = nullptr;
  bool GetBaseTypename = false;

  bool UseParentTypeName = true;
  bool UseBaseText = true;
  Dwarf_Half tag = getDieTag();
  switch (tag) {
  case DW_TAG_base_type:
  case DW_TAG_compile_unit:
  case DW_TAG_namespace:
  case DW_TAG_class_type:
  case DW_TAG_structure_type:
  case DW_TAG_union_type:
  case DW_TAG_unspecified_type:
  case DW_TAG_enumeration_type:
  case DW_TAG_enumerator:
  case DW_TAG_inheritance:
  case DW_TAG_GNU_template_parameter_pack:
    GetBaseTypename = true;
    break;
  case DW_TAG_array_type:
  case DW_TAG_subrange_type:
  case DW_TAG_imported_module:
  case DW_TAG_imported_declaration:
  case DW_TAG_subprogram:
  case DW_TAG_subroutine_type:
  case DW_TAG_inlined_subroutine:
  case DW_TAG_entry_point:
  case DW_TAG_label:
  case DW_TAG_typedef:
    GetBaseTypename = true;
    UseParentTypeName = false;
    break;
  case DW_TAG_const_type:
    PreText = "const";
    break;
  case DW_TAG_pointer_type:
    PostText = "*";
    // For the following sample code,
    //   void *p;
    // some compilers do not generate a DIE for the 'void' type.
    //   <0x0000002a> DW_TAG_variable
    //                  DW_AT_name p
    //                  DW_AT_type <0x0000003f>
    //   <0x0000003f> DW_TAG_pointer_type
    // For that case, we can emit the 'void' type.
    if (!BaseType && !getType() && Settings.ShowVoid)
      ParentTypename = "void";
    break;
  case DW_TAG_ptr_to_member_type:
    PostText = "*";
    break;
  case DW_TAG_rvalue_reference_type:
    PostText = "&&";
    break;
  case DW_TAG_reference_type:
    PostText = "&";
    break;
  case DW_TAG_restrict_type:
    PreText = "restrict";
    break;
  case DW_TAG_volatile_type:
    PreText = "volatile";
    break;
  case DW_TAG_template_type_parameter:
  case DW_TAG_template_value_parameter:
  case DW_TAG_catch_block:
  case DW_TAG_lexical_block:
  case DW_TAG_try_block:
    UseBaseText = false;
    break;
  case DW_TAG_GNU_template_template_parameter:
    break;
  default:
    return false;
  }

  // Overwrite if no given value.
  if (!BaseText) {
    if (GetBaseTypename) {
      BaseText = getName();
    }
  }

  // Concatenate the elements to get the full type name.
  // Type will be: base_parent + pre + base + parent + post.
  std::string FullName;

  if (BaseParent) {
    FullName.append(BaseParent);
  }
  if (PreText && !SpecScope) {
    FullName.append(PreText);
  }
  if (UseBaseText && BaseText) {
    if (PreText)
      FullName.append(" ");
    FullName.append(BaseText);
  }
  if (UseParentTypeName && ParentTypename) {
    if (UseBaseText && BaseText)
      FullName.append(" ");
    else if (PreText)
      FullName.append(" ");
    FullName.append(ParentTypename);
  }
  if (PostText && !SpecScope) {
    if (UseParentTypeName && ParentTypename)
      FullName.append(" ");
    else if (UseBaseText && BaseText)
      FullName.append(" ");
    else if (PreText)
      FullName.append(" ");
    FullName.append(PostText);
  }
  if (PostPostText) {
    FullName.append(PostPostText);
  }

  // Remove any double spaces.
  size_t StartPos = 0;
  std::string Spaces("  ");
  std::string Single(" ");
  while ((StartPos = FullName.find(Spaces, StartPos)) != std::string::npos) {
    FullName.replace(StartPos, Spaces.length(), Single);
    StartPos += Single.length();
  }

  setName(FullName.c_str());

  return true;
}

namespace {

std::string getTagString(const Dwarf_Half DWTag, const bool IsLine) {
  if (IsLine)
    return "[DW_AT_stml_list]";

  if (DWTag) {
    const char *tag_name;
    if (dwarf_get_TAG_name(DWTag, &tag_name) == DW_DLV_OK)
      return std::string("[") + std::string(tag_name) + std::string("]");
  }
  return "[DW_TAG_file]";
}

} // namespace

// Number of characters written by PrintAttributes.
size_t Object::IndentationSize = 0;

std::string Object::getAttributesAsText(const PrintSettings &Settings) {
  static bool CalculateIndentation = true;
  // Record the required space for the offsets (object and parent) and
  // DWARF tag. These fields are not required for the {InputFile} object.
  static size_t OffsetWidth = 0;
  static size_t ParentWidth = 0;
  static size_t TagWidth = 0;

  // Calculate the indentation size, so we can use that value when printing
  // additional attributes to DIVA objects. This value is calculated just for
  // the first object.
  if (CalculateIndentation) {
    CalculateIndentation = false;
    if (Settings.ShowDWARFOffset) {
      OffsetWidth = static_cast<size_t>(
          snprintf(nullptr, 0, "[0x%08" DW_PR_DUx "]", getDieOffset()));
      IndentationSize += OffsetWidth;
    }
    if (Settings.ShowDWARFParent) {
      ParentWidth = static_cast<size_t>(
          snprintf(nullptr, 0, "[0x%08" DW_PR_DUx "]", getDieParent()));
      IndentationSize += ParentWidth;
    }
    if (Settings.ShowLevel) {
      IndentationSize +=
          static_cast<size_t>(snprintf(nullptr, 0, "%03d", getLevel()));
    }
    if (Settings.ShowIsGlobal) {
      IndentationSize += static_cast<size_t>(
          snprintf(nullptr, 0U, "%c", getIsGlobalReference() ? 'X' : ' '));
    }
    if (Settings.ShowDWARFTag) {
      std::string tag_str = getTagString(getDieTag(), getIsLine());
      TagWidth =
          static_cast<size_t>(snprintf(nullptr, 0U, "%-42s", tag_str.c_str()));
      IndentationSize += TagWidth;
    }
  }

  // The values for the attributes are well defined (DIE offsets, scope level
  // and 'global' bits, etc. A buffer size of 64 is more than adequate to hold
  // the largest value (DWARF tag).
  const unsigned MaxSize = 64;
  int Res;
  static char Literal[MaxSize];
  Literal[0] = 0;

  std::string Attributes;

  // Do not print the DIE offset, Level or DWARF TAG for a {InputFile} object.
  bool IsInputFileObject = (getIsScope() && !getParent());
  if (Settings.ShowDWARFOffset) {
    if (IsInputFileObject) {
      Res = std::snprintf(Literal, MaxSize, "%s",
                          std::string(OffsetWidth, ' ').c_str());
    } else {
      Res = std::snprintf(Literal, MaxSize, "[0x%08" DW_PR_DUx "]",
                          getDieOffset());
    }
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxSize) &&
           "string overflow");
    Attributes += std::string(Literal);
  }
  if (Settings.ShowDWARFParent) {
    if (IsInputFileObject) {
      Res = std::snprintf(Literal, MaxSize, "%s",
                          std::string(ParentWidth, ' ').c_str());
    } else {
      Res = std::snprintf(Literal, MaxSize, "[0x%08" DW_PR_DUx "]",
                          getDieParent());
    }
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxSize) &&
           "string overflow");
    Attributes += std::string(Literal);
  }
  if (Settings.ShowLevel) {
    if (IsInputFileObject) {
      Res = std::snprintf(Literal, MaxSize, "   ");
    } else {
      Res = std::snprintf(Literal, MaxSize, "%03d", getLevel());
    }
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxSize) &&
           "string overflow");
    Attributes += std::string(Literal);
  }
  if (Settings.ShowIsGlobal) {
    Res = std::snprintf(Literal, MaxSize, "%c",
                        getIsGlobalReference() ? 'X' : ' ');
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxSize) &&
           "string overflow");
    Attributes += std::string(Literal);
  }
  if (Settings.ShowDWARFTag) {
    if (IsInputFileObject) {
      Res = std::snprintf(Literal, MaxSize, "%s",
                          std::string(TagWidth, ' ').c_str());
    } else {
      std::string tag_str = getTagString(getDieTag(), getIsLine());
      Res = std::snprintf(Literal, MaxSize, "%-42s", tag_str.c_str());
    }
    assert((Res >= 0) && (static_cast<unsigned>(Res) < MaxSize) &&
           "string overflow");
    Attributes += std::string(Literal);
  }
  (void)Res;

  return Attributes;
}

void Object::printAttributes(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s", getAttributesAsText(Settings).c_str());
}

// Record the last seen filename index. It is reset after the object that
// represents the Compile Unit is printed.
size_t Object::LastFilenameIndex = 0;

void Object::printFileIndex() {
  // Check if there is a change in the File ID sequence.
  size_t FNameIndex = getFileNameIndex();
  if (getInvalidFileName() || FNameIndex != LastFilenameIndex) {
    LastFilenameIndex = FNameIndex;

    // Keep a nice layout.
    GlobalPrintContext->print("\n");
    std::string Indent(IndentationSize, ' ');
    GlobalPrintContext->print(Indent.c_str());

    const char *Source = "  {Source}";
    if (getInvalidFileName()) {
      GlobalPrintContext->print("%s [0x%08x]\n", Source, FNameIndex);
    } else {
      std::string FName = getFileName(/*format_options=*/true);
      GlobalPrintContext->print("%s \"%s\"\n", Source, FName.c_str());
    }
  }
}

void Object::dump(const PrintSettings &Settings) {
  // Print the File ID if needed.
  if (getFileNameIndex())
    printFileIndex();

  // Print Debug Data (tag, offset, etc).
  printAttributes(Settings);

  // Print the line and any discriminator.
  GlobalPrintContext->print(" %5s %s ", getLineNumberAsString(),
                            getIndentString(Settings).c_str());
}

void Object::print(bool /*SplitCU*/, bool /*Match*/, bool /*IsNull*/,
                   const PrintSettings &Settings) {
  dump(Settings);
}

std::string
Object::getAttributeInfoAsText(const std::string &AttributeText,
                               const PrintSettings &Settings) const {
  // First we want to indent for any left aligned info being printed
  // (indentation_size). An extra space is printed before the line number, after
  // the line number and after the level indent so we need to indent an extra 3
  // for those. Then we want to indent the space where the line number would be.
  // Then We want to indent the attribute info 4 columns to the right of the
  // object.
  static const std::string ConstantIndent(
      std::string(IndentationSize, ' ') + std::string(3, ' ') +
      getNoLineString() + std::string(4, ' '));

  // Then we want to indent based on the object level and add the dash.
  return ConstantIndent + getIndentString(Settings) + "- " + AttributeText;
}

std::string Object::getCommonYAML() const {
  std::stringstream YAML;

  // Kind.
  YAML << "object: \"" << getKindAsString() << "\"\n";

  // Name.
  std::string Name;
  if (getHasQualifiedName())
    Name += getQualifiedName();
  if (getIsSymbol() &&
      static_cast<const Symbol *>(this)->getIsUnspecifiedParameter())
    Name += "...";
  else
    Name += getName();

  YAML << "name: ";
  if (!Name.empty())
    YAML << "\"" << Name << "\"\n";
  else
    YAML << "null\n";

  // Type.
  YAML << "type: ";
  if (getType() &&
      // Template's types are printed in attributes.
      !(getIsType() && static_cast<const Type *>(this)->getIsTemplateParam())) {
    std::string TypeName;
    if (getType()->getHasQualifiedName())
      TypeName += getType()->getQualifiedName();
    TypeName += getType()->getName();
    YAML << "\"" << TypeName << "\"\n";
    // Functions must have types.
  } else if (getIsScope() && static_cast<const Scope *>(this)->getIsFunction())
    YAML << "\"void\"\n";
  else
    YAML << "null\n";

  // Source.
  YAML << "source:\n  line: ";
  if (getLineNumber() != 0)
    YAML << getLineNumber() << '\n';
  else
    YAML << "null\n";

  std::string FileName(getFileName(/*format_options*/ true));
  YAML << "  file: ";
  if (getInvalidFileName())
    YAML << "\"?\"\n";
  else if (!FileName.empty())
    YAML << "\"" << FileName << "\"\n";
  else
    YAML << "null\n";

  // Dwarf.
  YAML << "dwarf:\n  offset: 0x" << std::hex << getDieOffset() << "\n  tag: ";
  if (getDieTag() != 0) {
    const char *TagName;
    dwarf_get_TAG_name(getDieTag(), &TagName);
    YAML << "\"" << TagName << "\"";
  } else
    YAML << "null";

  return YAML.str();
}

//===----------------------------------------------------------------------===//
// Class to represent the basic data for an object.
//===----------------------------------------------------------------------===//
Element::Element(LevelType level) : Object(level) { CommonConstructor(); }

Element::Element() : Object() { CommonConstructor(); }

void Element::CommonConstructor() {
  NameIndex = 0;
  QualifiedIndex = 0;
  FilenameIndex = 0;
  TheType = nullptr;
}

void Element::setName(const char *name) {
  NameIndex = StringPool::getStringIndex(name);
#ifndef NDEBUG
  if (name) {
    Name = name;
  }
#endif
}

const char *Element::getName() const {
  return StringPool::getStringValue(NameIndex);
}

void Element::setNameIndex(size_t name_index) { NameIndex = name_index; }

size_t Element::getNameIndex() const { return NameIndex; }

void Element::setQualifiedName(const char *QualName) {
  QualifiedIndex = StringPool::getStringIndex(QualName);
}

const char *Element::getQualifiedName() const {
  return StringPool::getStringValue(QualifiedIndex);
}

const char *Element::getTypeName() const {
  return getType() ? getType()->getName() : "";
}

std::string Element::getFileName(bool NameOnly) const {
  // If 'format_options', then we use the format command line options, to
  // return the full pathname or just the filename. The string stored in
  // the string pool, is the full pathname.
  std::string FName = StringPool::getStringValue(getFileNameIndex());
  if (NameOnly)
    FName = LibScopeView::getFileName(FName);
  return FName;
}

void Element::setFileName(const char *FileName) {
  FilenameIndex = StringPool::getStringIndex(unifyFilePath(FileName));
}

size_t Element::getFileNameIndex() const { return FilenameIndex; }

void Element::setFileNameIndex(size_t FNameIndex) {
  FilenameIndex = FNameIndex;
}

const char *Element::getTypeQualifiedName() const {
  if (!getType())
    return "";
  return getType()->getQualifiedName();
}
