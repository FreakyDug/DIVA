//===-- LibScopeView/Scope.cpp ----------------------------------*- C++ -*-===//
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
/// Implementations for the Scope class and its subclasses.
///
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "Error.h"
#include "FileUtilities.h"
#include "Line.h"
#include "PrintContext.h"
#include "Reader.h"
#include "Symbol.h"
#include "Type.h"

#include <algorithm>
#include <sstream>

using namespace LibScopeView;

Scope::Scope(LevelType Lvl) : Element(Lvl) {
  setIsScope();

  Scope::setTag();
}

Scope::Scope() : Element() {
  setIsScope();

  Scope::setTag();
}

Scope::~Scope() {
  for (Type *Ty : TheTypes)
    delete (Ty);
  for (Symbol *Sym : TheSymbols)
    delete (Sym);
  for (Scope *Scp : TheScopes)
    delete (Scp);
  for (Line *Ln : TheLines)
    delete (Ln);
}

uint32_t Scope::ScopesAllocated = 0;

void Scope::setTag() {
  ++Scope::ScopesAllocated;
#ifndef NDEBUG
  Tag = Scope::ScopesAllocated;
#endif
}

uint32_t Scope::getTag() const {
#ifndef NDEBUG
  return Tag;
#else
  return 0;
#endif
}

// Scope Kind.
const char *Scope::KindAggregate = "Aggregate";
const char *Scope::KindArray = "Array";
const char *Scope::KindBlock = "Block";
const char *Scope::KindCatchBlock = "CatchBlock";
const char *Scope::KindClass = "Class";
const char *Scope::KindCompileUnit = "CompileUnit";
const char *Scope::KindEntryPoint = "EntryPoint";
const char *Scope::KindEnumeration = "Enum";
const char *Scope::KindFile = "InputFile";
const char *Scope::KindFunction = "Function";
const char *Scope::KindFunctionType = "FunctionType";
const char *Scope::KindInlinedFunction = "Function";
const char *Scope::KindLabel = "Label";
const char *Scope::KindLexicalBlock = "LexicalBlock";
const char *Scope::KindNamespace = "Namespace";
const char *Scope::KindStruct = "Struct";
const char *Scope::KindTemplate = "Template";
const char *Scope::KindTemplateAlias = "Alias";
const char *Scope::KindTemplatePack = "TemplateParameter";
const char *Scope::KindTryBlock = "TryBlock";
const char *Scope::KindUndefined = "Undefined";
const char *Scope::KindUnion = "Union";

const char *Scope::getKindAsString() const {
  const char *Kind = KindUndefined;
  if (getIsArrayType())
    Kind = KindArray;
  else if (getIsBlock())
    Kind = KindBlock;
  else if (getIsCompileUnit())
    Kind = KindCompileUnit;
  else if (getIsEnumerationType())
    Kind = KindEnumeration;
  else if (getIsInlinedSubroutine())
    Kind = KindInlinedFunction;
  else if (getIsNamespace())
    Kind = KindNamespace;
  else if (getIsTemplatePack())
    Kind = KindTemplatePack;
  else if (getIsRoot())
    Kind = KindFile;
  else if (getIsTemplateAlias())
    Kind = KindTemplateAlias;
  else if (getIsClassType())
    Kind = KindClass;
  else if (getIsFunction())
    Kind = KindFunction;
  else if (getIsStructType())
    Kind = KindStruct;
  else if (getIsUnionType())
    Kind = KindUnion;
  return Kind;
}

void Scope::addObject(Line *Ln) {
  if (getCanHaveLines()) {
    // Add it to parent.
    TheLines.push_back(Ln);
    Ln->setParent(this);

    // Update Object Summary Table.
    getReader()->incrementFound(Ln);

    // Do not add the line records to the children, as they represent the
    // logical view for the text section. Preserve the original sequence.
    // m_children->push_back(line);

    // Indicate that this tree branch has lines.
    traverse(&Scope::getHasLines, &Scope::setHasLines, /*down=*/false);
  } else {
    throw std::logic_error("Cannot set line records on a scope that's not a "
                           "function or module.\n");
  }
}

void Scope::addObject(Scope *Scp) {
  // Add it to parent.
  TheScopes.push_back(Scp);
  Children.push_back(Scp);
  Scp->setParent(this);

  // Object Summary Table.
  getReader()->incrementFound(Scp);

  // If the object is a global reference, mark its parent as having global
  // references; that information is used, to print only those branches
  // with global references.
  if (Scp->getIsGlobalReference()) {
    traverse(&Scope::getHasGlobals, &Scope::setHasGlobals, /*down=*/false);
  } else {
    traverse(&Scope::getHasLocals, &Scope::setHasLocals, /*down=*/false);
  }

  // Indicate that this tree branch has scopes.
  traverse(&Scope::getHasScopes, &Scope::setHasScopes, /*down=*/false);
}

void Scope::addObject(Symbol *Sym) {
  // Add it to parent.
  TheSymbols.push_back(Sym);
  Children.push_back(Sym);
  Sym->setParent(this);

  // Object Summary Table.
  getReader()->incrementFound(Sym);

  // If the object is a global reference, mark its parent as having global
  // references; that information is used, to print only those branches
  // with global references.
  if (Sym->getIsGlobalReference()) {
    traverse(&Scope::getHasGlobals, &Scope::setHasGlobals, /*down=*/false);
  } else {
    traverse(&Scope::getHasLocals, &Scope::setHasLocals, /*down=*/false);
  }

  // Indicate that this tree branch has symbols.
  traverse(&Scope::getHasSymbols, &Scope::setHasSymbols, /*down=*/false);
}

void Scope::addObject(Type *Ty) {
  // Add it to parent.
  TheTypes.push_back(Ty);
  Children.push_back(Ty);
  Ty->setParent(this);

  // Object Summary Table.
  getReader()->incrementFound(Ty);

  // If the object is a global reference, mark its parent as having global
  // references; that information is used, to print only those branches
  // with global references.
  if (Ty->getIsGlobalReference()) {
    traverse(&Scope::getHasGlobals, &Scope::setHasGlobals, /*down=*/false);
  } else {
    traverse(&Scope::getHasLocals, &Scope::setHasLocals, /*down=*/false);
  }

  // Indicate that this tree branch has types.
  traverse(&Scope::getHasTypes, &Scope::setHasTypes, /*down=*/false);
}

void Scope::getQualifiedName(std::string &qualified_name) const {
  if (getIsRoot() || getIsCompileUnit()) {
    return;
  }

  Scope *ScpParent = getParent();
  if (ScpParent) {
    ScpParent->getQualifiedName(qualified_name);
  }
  if (!qualified_name.empty()) {
    qualified_name.append("::");
  }
  qualified_name.append(getName());
}

void Scope::sortScopes(const SortingKey &SortKey) {
  // Get the sorting callback function.
  SortFunction SortFunc = getSortFunction(SortKey);
  if (SortFunc) {
    sortScopes(SortFunc);
  }
}

void Scope::sortScopes(SortFunction SortFunc) {
  // Sort the contained objects, using the associated line.
  std::sort(TheTypes.begin(), TheTypes.end(), SortFunc);
  std::sort(TheSymbols.begin(), TheSymbols.end(), SortFunc);
  std::sort(TheScopes.begin(), TheScopes.end(), SortFunc);

  // Sort the contained objects, using the associated line.
  std::sort(Children.begin(), Children.end(), SortFunc);

  // Scopes.
  for (Scope *Scp : TheScopes)
    Scp->sortScopes(SortFunc);
}

void Scope::sortCompileUnits(const SortingKey &SortKey) {
  // Sort the contained objects, using the sort criteria.
  SortFunction SortFunc = getSortFunction(SortKey);
  if (SortFunc) {
    std::sort(TheScopes.begin(), TheScopes.end(), SortFunc);
    std::sort(Children.begin(), Children.end(), SortFunc);
  }
}

void Scope::traverse(ScopeGetFunction GetFunc, ScopeSetFunction SetFunc,
                     bool down) {
  // First traverse the parent tree.
  Scope *parent = this;
  while (parent) {
    // Terminates if the 'set_function' has already been executed.
    if ((parent->*GetFunc)()) {
      break;
    }
    (parent->*SetFunc)();
    parent = parent->getParent();
  }
  // If requested, traverse the children tree.
  if (down) {
    // Traverse(get_function,set_function);
  }
}

void Scope::traverse(ObjGetFunction GetFunc, ObjSetFunction SetFunc,
                     bool down) {
  // First traverse the parent tree.
  Scope *parent = this;
  while (parent) {
    // Terminates if the 'set_function' have been already executed.
    if ((parent->*GetFunc)()) {
      break;
    }
    (parent->*SetFunc)();
    parent = parent->getParent();
  }
  // If requested, traverse the children tree.
  if (down) {
    traverse(GetFunc, SetFunc);
  }
}

void Scope::traverse(ObjGetFunction GetFunc, ObjSetFunction SetFunc) {
  (this->*SetFunc)();

  // Types.
  for (Type *Ty : TheTypes)
    (Ty->*SetFunc)();

  // Symbols.
  for (Symbol *Sym : TheSymbols)
    (Sym->*SetFunc)();

  // Line records.
  for (Line *Ln : TheLines)
    (Ln->*SetFunc)();

  // Scopes.
  for (Scope *Scp : TheScopes)
    Scp->traverse(GetFunc, SetFunc);
}

bool Scope::resolvePrinting(const PrintSettings &Settings) {
  bool DoPrint = true;

  bool Globals = Settings.ShowOnlyGlobals;
  bool Locals = Settings.ShowOnlyLocals;
  if (Globals == Locals) {
    // Print both Global and Local.
  } else {
    // Check for Global or Local Objects.
    if ((Globals && !(getHasGlobals() || getIsGlobalReference())) ||
        (Locals && !(getHasLocals() || !getIsGlobalReference()))) {
      DoPrint = false;
    }
  }

  // For the case of functions, skip it if unnamed (name is NULL) or
  // unlined (line number is zero).
  if (DoPrint && getIsFunction()) {
    if (!Settings.ShowGenerated && isNotPrintable()) {
      DoPrint = false;
    }
  }

  // Check if we are using any pattern.
  if (DoPrint) {
    // Indicate that this tree branch has a matched pattern.
    if (!Settings.WithChildrenFilters.empty() ||
        !Settings.WithChildrenFilterAnys.empty()) {
      DoPrint = getHasPattern();
    }
  }

  return DoPrint;
}

void Scope::print(bool SplitCU, bool Match, bool IsNull,
                  const PrintSettings &Settings) {
  // If 'split_cu', we use the scope name (CU name) as the ouput file.
  if (SplitCU && getIsCompileUnit()) {
    std::string OutFilePath(GlobalPrintContext->getLocation() +
                            flattenFilePath(getName()) + ".txt");

    // Open print context.
    if (!GlobalPrintContext->open(OutFilePath)) {
      fatalError(LibScopeError::ErrorCode::ERR_SPLIT_UNABLE_TO_OPEN_FILE,
                 OutFilePath);
    }
  }

  // Check conditions such as local, global, etc.
  bool DoPrint = resolvePrinting(Settings);

  // Don't print in quiet mode unless splitting output.
  DoPrint = DoPrint && (!Settings.QuietMode || Settings.SplitOutput);

  if (DoPrint) {
    // Dump the object itself.
    dump(Settings);
    // Dump the children.
    for (Object *Obj : Children) {
      if (Match && !Obj->getHasPattern())
        continue;
      Obj->print(SplitCU, Match, IsNull, Settings);
    }
    // Dump the line records.
    for (Line *Ln : TheLines) {
      if (Match && !Ln->getHasPattern())
        continue;
      Ln->print(SplitCU, Match, IsNull, Settings);
    }
  }

  // Restore the original output context.
  if (SplitCU && getIsCompileUnit()) {
    // Close the print context.
    GlobalPrintContext->close();
  }
}

const char *Scope::resolveName() {
  // If the scope has a DW_AT_specification or DW_AT_abstract_origin,
  // follow the chain to resolve the name from those references.
  if (getHasReference()) {
    Scope *Specification = getReference();
    if (isUnnamed()) {
      setName(Specification->resolveName());
    }
  }
  return getName();
}

void Scope::dump(const PrintSettings &Settings) {
  // Check if the object needs to be printed.
  if (dumpAllowed() || Settings.printObject(*this)) {
    // Object Summary Table.
    getReader()->incrementPrinted(this);

    // Common Object Data.
    Element::dump(Settings);

    // Specific Object Data.
    dumpExtra(Settings);
  }
}

void Scope::dumpExtra(const PrintSettings &Settings) {
  std::string Text = getAsText(Settings);
  if (!Text.empty())
    GlobalPrintContext->print("%s\n", Text.c_str());
}

bool Scope::dump(bool DoHeader, const char *Header,
                 const PrintSettings &Settings) {
  if (DoHeader) {
    GlobalPrintContext->print("\n%s\n", Header);
    DoHeader = false;
  }

  // Dump object.
  dump(Settings);

  return DoHeader;
}

std::string Scope::getAsText(const PrintSettings &Settings) const {
  std::stringstream Result;
  if (getIsBlock()) {
    Result << '{' << getKindAsString() << '}';
    if (Settings.ShowBlockAttributes) {
      if (getIsTryBlock())
        Result << '\n' << getAttributeInfoAsText("try", Settings);
      else if (getIsCatchBlock())
        Result << '\n' << getAttributeInfoAsText("catch", Settings);
    }
  }
  return Result.str();
}

std::string Scope::getAsYAML() const {
  if (getIsBlock()) {
    std::stringstream YAML;
    YAML << getCommonYAML() << "\nattributes:"
         << "\n  try: " << (getIsTryBlock() ? "true" : "false")
         << "\n  catch: " << (getIsCatchBlock() ? "true" : "false");
    return YAML.str();
  }
  return "";
}

ScopeAggregate::ScopeAggregate(LevelType Lvl) : Scope(Lvl) {
  Reference = nullptr;
}

ScopeAggregate::ScopeAggregate() : Scope() { Reference = nullptr; }

ScopeAggregate::~ScopeAggregate() {}

std::string ScopeAggregate::getAsText(const PrintSettings &Settings) const {
  std::string Result;
  const char *Name = getName();
  Result = "{";
  Result += getKindAsString();
  Result += "} \"";
  if (Name != nullptr)
    Result += Name;
  Result += '"';

  if (getIsTemplate()) {
    Result += '\n';
    Result += getAttributeInfoAsText("Template", Settings);
  }

  return Result;
}

std::string ScopeAggregate::getAsYAML() const {
  std::stringstream Result;

  Result << getCommonYAML();
  Result << "\nattributes:\n  is_template: "
         << (getIsTemplate() ? "true" : "false");

  // If we're getting YAML for a Union. then we can't have any inheritance
  // attributes.
  if (getIsUnionType())
    return Result.str();

  Result << "\n  inherits_from:";

  bool hasInheritance = false;
  for (auto type : TheTypes) {
    if (type->getIsInheritance()) {
      hasInheritance = true;
      Result << "\n" << static_cast<TypeImport *>(type)->getAsYAML();
    }
  }

  if (!hasInheritance)
    Result << " []";

  return Result.str();
}

ScopeAlias::ScopeAlias(LevelType Lvl) : Scope(Lvl) {}

ScopeAlias::ScopeAlias() : Scope() {}

ScopeAlias::~ScopeAlias() {}

void ScopeAlias::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeAlias::getAsText(const PrintSettings &Settings) const {
  std::stringstream Result;
  Result << "{" << getKindAsString() << "} \"" << getName() << "\" -> "
         << getTypeDieOffsetAsString(Settings) << '"' << getTypeQualifiedName()
         << getTypeAsString(Settings) << '"';
  return Result.str();
}

std::string ScopeAlias::getAsYAML() const {
  return getCommonYAML() + std::string("\nattributes: {}");
}

ScopeArray::ScopeArray(LevelType Lvl) : Scope(Lvl) {}

ScopeArray::ScopeArray() : Scope() {}

ScopeArray::~ScopeArray() {}

void ScopeArray::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeArray::getAsText(const PrintSettings &Settings) const {
  std::stringstream Result;
  Result << "{" << getKindAsString() << "} "
         << getTypeDieOffsetAsString(Settings) << '"' << getName() << '"';
  return Result.str();
}

ScopeCompileUnit::ScopeCompileUnit(LevelType Lvl) : Scope(Lvl) {}

ScopeCompileUnit::ScopeCompileUnit() : Scope() {}

ScopeCompileUnit::~ScopeCompileUnit() {}

void ScopeCompileUnit::setName(const char *Name) {
  std::string Path = unifyFilePath(Name);
  Scope::setName(Path.c_str());
}

void ScopeCompileUnit::dump(const PrintSettings &Settings) {
  // An extra line to improve readibility.
  if (Settings.printObject(*this)) {
    GlobalPrintContext->print("\n");
  }
  Scope::dump(Settings);
}

void ScopeCompileUnit::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
  resetFileIndex();
}

std::string ScopeCompileUnit::getAsText(const PrintSettings &) const {
  std::string ObjectAsText;
  ObjectAsText.append("{").append(getKindAsString()).append("}");
  ObjectAsText.append(" \"").append(getName()).append("\"");

  return ObjectAsText;
}

std::string ScopeCompileUnit::getAsYAML() const {
  return getCommonYAML() + std::string("\nattributes: {}");
}

ScopeEnumeration::ScopeEnumeration(LevelType Lvl)
    : Scope(Lvl), IsClass(false) {}

ScopeEnumeration::ScopeEnumeration() : Scope(), IsClass(false) {}

ScopeEnumeration::~ScopeEnumeration() {}

void ScopeEnumeration::dumpExtra(const PrintSettings &Settings) {
  // Print the full type name.
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeEnumeration::getAsText(const PrintSettings &) const {
  std::string ObjectAsText;
  std::string Name = getName();

  ObjectAsText.append("{").append(getKindAsString()).append("}");

  if (getIsClass())
    ObjectAsText.append(" ").append("class");

  ObjectAsText.append(" \"").append(Name).append("\"");

  if (getType() && Name != getType()->getName())
    ObjectAsText.append(" -> \"").append(getType()->getName()).append("\"");

  return ObjectAsText;
}

std::string ScopeEnumeration::getAsYAML() const {
  std::stringstream YAML;
  YAML << getCommonYAML() << "\nattributes:"
       << "\n  class: " << (getIsClass() ? "true" : "false")
       << "\n  enumerators:";

  std::stringstream Enumerators;
  for (auto *Child : getChildren()) {
    if (!(Child->getIsType() &&
          static_cast<const Type *>(Child)->getIsEnumerator()))
      // TODO: Raise a warning here?
      continue;
    auto *ChildEnumerator = static_cast<TypeEnumerator *>(Child);
    Enumerators << "\n    - enumerator: \"" << ChildEnumerator->getName()
                << "\"\n      value: " << ChildEnumerator->getValue();
  }

  if (Enumerators.str().empty())
    YAML << " []";
  else
    YAML << Enumerators.str();

  return YAML.str();
}

ScopeFunction::ScopeFunction(LevelType Lvl)
    : Scope(Lvl), IsStatic(false), DeclaredInline(false), IsDeclaration(false) {
  Reference = nullptr;
}

ScopeFunction::ScopeFunction()
    : Scope(), IsStatic(false), DeclaredInline(false), IsDeclaration(false) {
  Reference = nullptr;
}

ScopeFunction::~ScopeFunction() {}

void ScopeFunction::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeFunction::getAsText(const PrintSettings &Settings) const {
  std::string Result = "{";
  Result += getKindAsString();
  Result += "}";

  if (getIsStatic())
    Result += " static";
  if (getIsDeclaredInline())
    Result += " inline";

  std::string Name;
  getQualifiedName(Name);

  Result += " \"";
  Result += Name;
  Result += "\"";
  Result += " -> ";
  Result += getDieOffsetAsString(Settings);
  Result += "\"";
  Result += getTypeQualifiedName();
  Result += getTypeAsString(Settings);
  Result += "\"";

  // Attributes.
  if (Reference && Reference->getIsFunction()) {
    Result += '\n';
    Result += getAttributeInfoAsText("Declaration @ ", Settings);
    // Cast to element as Scope has a different overload (not override) of
    // getFileName that returns nothing.
    if (!Reference->getInvalidFileName())
      Result += static_cast<LibScopeView::Element *>(Reference)->getFileName(
          /*format_options*/ true);
    else
      Result += '?';
    Result += ',';
    Result += std::to_string(Reference->getLineNumber());
  } else {
    if (!getIsDeclaration()) {
      Result += '\n';
      Result += getAttributeInfoAsText("No declaration", Settings);
    }
  }

  if (getIsTemplate()) {
    Result += '\n';
    Result += getAttributeInfoAsText("Template", Settings);
  }
  if (getIsInlined()) {
    Result += '\n';
    Result += getAttributeInfoAsText("Inlined", Settings);
  }
  if (getIsDeclaration()) {
    Result += '\n';
    Result += getAttributeInfoAsText("Is declaration", Settings);
  }

  return Result;
}

std::string ScopeFunction::getAsYAML() const {
  std::stringstream YAML;
  YAML << getCommonYAML() << "\nattributes:\n";

  // Attributes.
  YAML << "  declaration:\n";
  if (Reference && Reference->getIsFunction()) {
    // Cast to element as Scope has a different overload (not override) of
    // getFileName that returns nothing.
    YAML << "    file: ";
    if (!Reference->getInvalidFileName())
      YAML << "\""
           << static_cast<LibScopeView::Element *>(Reference)->getFileName(
                  /*format_options*/ true)
           << "\"";
    else
      YAML << "\"?\"";
    YAML << "\n    line: " << Reference->getLineNumber() << "\n";
  } else {
    YAML << "    file: null\n    line: null\n";
  }
  YAML << "  is_template: " << (getIsTemplate() ? "true" : "false") << "\n"
       << "  static: " << (getIsStatic() ? "true" : "false") << "\n"
       << "  inline: " << (getIsDeclaredInline() ? "true" : "false") << "\n"
       << "  is_inlined: " << (getIsInlined() ? "true" : "false") << "\n"
       << "  is_declaration: " << (getIsDeclaration() ? "true" : "false");

  return YAML.str();
}

ScopeFunctionInlined::ScopeFunctionInlined(LevelType Lvl)
    : ScopeFunction(Lvl), CallLineNumber(0) {
  Discriminator = 0;
}

ScopeFunctionInlined::ScopeFunctionInlined()
    : ScopeFunction(), CallLineNumber(0) {
  Discriminator = 0;
}

ScopeFunctionInlined::~ScopeFunctionInlined() {}

ScopeNamespace::ScopeNamespace(LevelType Lvl) : Scope(Lvl) {
  Reference = nullptr;
}

ScopeNamespace::ScopeNamespace() : Scope() { Reference = nullptr; }

ScopeNamespace::~ScopeNamespace() {}

void ScopeNamespace::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeNamespace::getAsText(const PrintSettings &) const {
  std::stringstream Result;
  Result << '{' << getKindAsString() << '}';
  std::string Name;
  getQualifiedName(Name);
  if (!Name.empty())
    Result << " \"" << Name << '"';
  return Result.str();
}

std::string ScopeNamespace::getAsYAML() const {
  return getCommonYAML() + std::string("\nattributes: {}");
}

ScopeTemplatePack::ScopeTemplatePack(LevelType Lvl) : Scope(Lvl) {}

ScopeTemplatePack::ScopeTemplatePack() : Scope() {}

ScopeTemplatePack::~ScopeTemplatePack() {}

void ScopeTemplatePack::dumpExtra(const PrintSettings &Settings) {
  // Print the full type name.
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeTemplatePack::getAsText(const PrintSettings &) const {
  std::string Result;
  Result += "{";
  Result += getKindAsString();
  Result += "}";
  Result += " \"";
  Result += getName();
  Result += "\"";
  return Result;
}

std::string ScopeTemplatePack::getAsYAML() const {
  std::stringstream YAML;
  YAML << getCommonYAML() << "\nattributes:\n  types:";

  auto isTemplate = [](const Object *Obj) -> bool {
    return Obj->getIsType() &&
           static_cast<const Type *>(Obj)->getIsTemplateParam();
  };
  if (getChildrenCount() == 0 ||
      std::none_of(getChildren().cbegin(), getChildren().cend(), isTemplate)) {
    YAML << " []";
    return YAML.str();
  }

  for (const auto *Child : getChildren()) {
    if (isTemplate(Child))
      YAML << "\n    - " << Child->getAsYAML();
  }

  return YAML.str();
}

ScopeRoot::ScopeRoot(LevelType Lvl) : Scope(Lvl) {}

ScopeRoot::ScopeRoot() : Scope() {}

ScopeRoot::~ScopeRoot() {}

void ScopeRoot::setName(const char *Name) {
  std::string Path = unifyFilePath(Name);
  Scope::setName(Path.c_str());
}

void ScopeRoot::dump(const PrintSettings &Settings) {
  if (!Settings.SplitOutput) {
    Scope::dump(Settings);
  }
}

void ScopeRoot::dumpExtra(const PrintSettings &Settings) {
  GlobalPrintContext->print("%s\n", getAsText(Settings).c_str());
}

std::string ScopeRoot::getAsText(const PrintSettings &) const {
  std::stringstream Result;
  Result << "{" << getKindAsString() << "} \"" << getName() << '"';
  return Result.str();
}
