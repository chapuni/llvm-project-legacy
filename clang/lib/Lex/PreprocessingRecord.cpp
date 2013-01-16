//===--- PreprocessingRecord.cpp - Record of Preprocessing ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the PreprocessingRecord class, which maintains a record
//  of what occurred during preprocessing, and its helpers.
//
//===----------------------------------------------------------------------===//
#include "clang/Lex/PreprocessingRecord.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/Token.h"
#include "llvm/Support/Capacity.h"
#include "llvm/Support/ErrorHandling.h"

using namespace clang;

ExternalPreprocessingRecordSource::~ExternalPreprocessingRecordSource() { }


InclusionDirective::InclusionDirective(PreprocessingRecord &PPRec,
                                       InclusionKind Kind, 
                                       StringRef FileName, 
                                       bool InQuotes, bool ImportedModule,
                                       const FileEntry *File,
                                       SourceRange Range)
  : PreprocessingDirective(InclusionDirectiveKind, Range), 
    InQuotes(InQuotes), Kind(Kind), ImportedModule(ImportedModule), File(File)
{ 
  char *Memory 
    = (char*)PPRec.Allocate(FileName.size() + 1, llvm::alignOf<char>());
  memcpy(Memory, FileName.data(), FileName.size());
  Memory[FileName.size()] = 0;
  this->FileName = StringRef(Memory, FileName.size());
}

PreprocessingRecord::PreprocessingRecord(SourceManager &SM)
  : SourceMgr(SM),
    ExternalSource(0) {
}

/// \brief Returns a pair of [Begin, End) iterators of preprocessed entities
/// that source range \p Range encompasses.
std::pair<PreprocessingRecord::iterator, PreprocessingRecord::iterator>
PreprocessingRecord::getPreprocessedEntitiesInRange(SourceRange Range) {
  if (Range.isInvalid())
    return std::make_pair(iterator(), iterator());

  if (CachedRangeQuery.Range == Range) {
    return std::make_pair(iterator(this, CachedRangeQuery.Result.first),
                          iterator(this, CachedRangeQuery.Result.second));
  }

  std::pair<int, int> Res = getPreprocessedEntitiesInRangeSlow(Range);
  
  CachedRangeQuery.Range = Range;
  CachedRangeQuery.Result = Res;
  
  return std::make_pair(iterator(this, Res.first), iterator(this, Res.second));
}

static bool isPreprocessedEntityIfInFileID(PreprocessedEntity *PPE, FileID FID,
                                           SourceManager &SM) {
  assert(!FID.isInvalid());
  if (!PPE)
    return false;

  SourceLocation Loc = PPE->getSourceRange().getBegin();
  if (Loc.isInvalid())
    return false;
  
  if (SM.isInFileID(SM.getFileLoc(Loc), FID))
    return true;
  else
    return false;
}

/// \brief Returns true if the preprocessed entity that \arg PPEI iterator
/// points to is coming from the file \arg FID.
///
/// Can be used to avoid implicit deserializations of preallocated
/// preprocessed entities if we only care about entities of a specific file
/// and not from files \#included in the range given at
/// \see getPreprocessedEntitiesInRange.
bool PreprocessingRecord::isEntityInFileID(iterator PPEI, FileID FID) {
  if (FID.isInvalid())
    return false;

  int Pos = PPEI.Position;
  if (Pos < 0) {
    assert(unsigned(-Pos-1) < LoadedPreprocessedEntities.size() &&
           "Out-of bounds loaded preprocessed entity");
    assert(ExternalSource && "No external source to load from");
    unsigned LoadedIndex = LoadedPreprocessedEntities.size()+Pos;
    if (PreprocessedEntity *PPE = LoadedPreprocessedEntities[LoadedIndex])
      return isPreprocessedEntityIfInFileID(PPE, FID, SourceMgr);

    // See if the external source can see if the entity is in the file without
    // deserializing it.
    llvm::Optional<bool>
      IsInFile = ExternalSource->isPreprocessedEntityInFileID(LoadedIndex, FID);
    if (IsInFile.hasValue())
      return IsInFile.getValue();

    // The external source did not provide a definite answer, go and deserialize
    // the entity to check it.
    return isPreprocessedEntityIfInFileID(
                                       getLoadedPreprocessedEntity(LoadedIndex),
                                          FID, SourceMgr);
  }

  assert(unsigned(Pos) < PreprocessedEntities.size() &&
         "Out-of bounds local preprocessed entity");
  return isPreprocessedEntityIfInFileID(PreprocessedEntities[Pos],
                                        FID, SourceMgr);
}

/// \brief Returns a pair of [Begin, End) iterators of preprocessed entities
/// that source range \arg R encompasses.
std::pair<int, int>
PreprocessingRecord::getPreprocessedEntitiesInRangeSlow(SourceRange Range) {
  assert(Range.isValid());
  assert(!SourceMgr.isBeforeInTranslationUnit(Range.getEnd(),Range.getBegin()));
  
  std::pair<unsigned, unsigned>
    Local = findLocalPreprocessedEntitiesInRange(Range);
  
  // Check if range spans local entities.
  if (!ExternalSource || SourceMgr.isLocalSourceLocation(Range.getBegin()))
    return std::make_pair(Local.first, Local.second);
  
  std::pair<unsigned, unsigned>
    Loaded = ExternalSource->findPreprocessedEntitiesInRange(Range);
  
  // Check if range spans local entities.
  if (Loaded.first == Loaded.second)
    return std::make_pair(Local.first, Local.second);
  
  unsigned TotalLoaded = LoadedPreprocessedEntities.size();
  
  // Check if range spans loaded entities.
  if (Local.first == Local.second)
    return std::make_pair(int(Loaded.first)-TotalLoaded,
                          int(Loaded.second)-TotalLoaded);
  
  // Range spands loaded and local entities.
  return std::make_pair(int(Loaded.first)-TotalLoaded, Local.second);
}

std::pair<unsigned, unsigned>
PreprocessingRecord::findLocalPreprocessedEntitiesInRange(
                                                      SourceRange Range) const {
  if (Range.isInvalid())
    return std::make_pair(0,0);
  assert(!SourceMgr.isBeforeInTranslationUnit(Range.getEnd(),Range.getBegin()));

  unsigned Begin = findBeginLocalPreprocessedEntity(Range.getBegin());
  unsigned End = findEndLocalPreprocessedEntity(Range.getEnd());
  return std::make_pair(Begin, End);
}

namespace {

template <SourceLocation (SourceRange::*getRangeLoc)() const>
struct PPEntityComp {
  const SourceManager &SM;

  explicit PPEntityComp(const SourceManager &SM) : SM(SM) { }

  bool operator()(PreprocessedEntity *L, PreprocessedEntity *R) const {
    SourceLocation LHS = getLoc(L);
    SourceLocation RHS = getLoc(R);
    return SM.isBeforeInTranslationUnit(LHS, RHS);
  }

  bool operator()(PreprocessedEntity *L, SourceLocation RHS) const {
    SourceLocation LHS = getLoc(L);
    return SM.isBeforeInTranslationUnit(LHS, RHS);
  }

  bool operator()(SourceLocation LHS, PreprocessedEntity *R) const {
    SourceLocation RHS = getLoc(R);
    return SM.isBeforeInTranslationUnit(LHS, RHS);
  }

  SourceLocation getLoc(PreprocessedEntity *PPE) const {
    SourceRange Range = PPE->getSourceRange();
    return (Range.*getRangeLoc)();
  }
};

}

unsigned PreprocessingRecord::findBeginLocalPreprocessedEntity(
                                                     SourceLocation Loc) const {
  if (SourceMgr.isLoadedSourceLocation(Loc))
    return 0;

  size_t Count = PreprocessedEntities.size();
  size_t Half;
  std::vector<PreprocessedEntity *>::const_iterator
    First = PreprocessedEntities.begin();
  std::vector<PreprocessedEntity *>::const_iterator I;

  // Do a binary search manually instead of using std::lower_bound because
  // The end locations of entities may be unordered (when a macro expansion
  // is inside another macro argument), but for this case it is not important
  // whether we get the first macro expansion or its containing macro.
  while (Count > 0) {
    Half = Count/2;
    I = First;
    std::advance(I, Half);
    if (SourceMgr.isBeforeInTranslationUnit((*I)->getSourceRange().getEnd(),
                                            Loc)){
      First = I;
      ++First;
      Count = Count - Half - 1;
    } else
      Count = Half;
  }

  return First - PreprocessedEntities.begin();
}

unsigned PreprocessingRecord::findEndLocalPreprocessedEntity(
                                                     SourceLocation Loc) const {
  if (SourceMgr.isLoadedSourceLocation(Loc))
    return 0;

  std::vector<PreprocessedEntity *>::const_iterator
  I = std::upper_bound(PreprocessedEntities.begin(),
                       PreprocessedEntities.end(),
                       Loc,
                       PPEntityComp<&SourceRange::getBegin>(SourceMgr));
  return I - PreprocessedEntities.begin();
}

PreprocessingRecord::PPEntityID
PreprocessingRecord::addPreprocessedEntity(PreprocessedEntity *Entity) {
  assert(Entity);
  SourceLocation BeginLoc = Entity->getSourceRange().getBegin();

  if (isa<MacroDefinition>(Entity)) {
    assert((PreprocessedEntities.empty() ||
            !SourceMgr.isBeforeInTranslationUnit(BeginLoc,
                   PreprocessedEntities.back()->getSourceRange().getBegin())) &&
           "a macro definition was encountered out-of-order");
    PreprocessedEntities.push_back(Entity);
    return getPPEntityID(PreprocessedEntities.size()-1, /*isLoaded=*/false);
  }

  // Check normal case, this entity begin location is after the previous one.
  if (PreprocessedEntities.empty() ||
      !SourceMgr.isBeforeInTranslationUnit(BeginLoc,
                   PreprocessedEntities.back()->getSourceRange().getBegin())) {
    PreprocessedEntities.push_back(Entity);
    return getPPEntityID(PreprocessedEntities.size()-1, /*isLoaded=*/false);
  }

  // The entity's location is not after the previous one; this can happen with
  // include directives that form the filename using macros, e.g:
  // "#include MACRO(STUFF)"
  // or with macro expansions inside macro arguments where the arguments are
  // not expanded in the same order as listed, e.g:
  // \code
  //  #define M1 1
  //  #define M2 2
  //  #define FM(x,y) y x
  //  FM(M1, M2)
  // \endcode

  typedef std::vector<PreprocessedEntity *>::iterator pp_iter;

  // Usually there are few macro expansions when defining the filename, do a
  // linear search for a few entities.
  unsigned count = 0;
  for (pp_iter RI    = PreprocessedEntities.end(),
               Begin = PreprocessedEntities.begin();
       RI != Begin && count < 4; --RI, ++count) {
    pp_iter I = RI;
    --I;
    if (!SourceMgr.isBeforeInTranslationUnit(BeginLoc,
                                           (*I)->getSourceRange().getBegin())) {
      pp_iter insertI = PreprocessedEntities.insert(RI, Entity);
      return getPPEntityID(insertI - PreprocessedEntities.begin(),
                           /*isLoaded=*/false);
    }
  }

  // Linear search unsuccessful. Do a binary search.
  pp_iter I = std::upper_bound(PreprocessedEntities.begin(),
                               PreprocessedEntities.end(),
                               BeginLoc,
                               PPEntityComp<&SourceRange::getBegin>(SourceMgr));
  pp_iter insertI = PreprocessedEntities.insert(I, Entity);
  return getPPEntityID(insertI - PreprocessedEntities.begin(),
                       /*isLoaded=*/false);
}

void PreprocessingRecord::SetExternalSource(
                                    ExternalPreprocessingRecordSource &Source) {
  assert(!ExternalSource &&
         "Preprocessing record already has an external source");
  ExternalSource = &Source;
}

unsigned PreprocessingRecord::allocateLoadedEntities(unsigned NumEntities) {
  unsigned Result = LoadedPreprocessedEntities.size();
  LoadedPreprocessedEntities.resize(LoadedPreprocessedEntities.size() 
                                    + NumEntities);
  return Result;
}

void PreprocessingRecord::RegisterMacroDefinition(MacroInfo *Macro,
                                                  PPEntityID PPID) {
  MacroDefinitions[Macro] = PPID;
}

/// \brief Retrieve the preprocessed entity at the given ID.
PreprocessedEntity *PreprocessingRecord::getPreprocessedEntity(PPEntityID PPID){
  if (PPID.ID < 0) {
    unsigned Index = -PPID.ID - 1;
    assert(Index < LoadedPreprocessedEntities.size() &&
           "Out-of bounds loaded preprocessed entity");
    return getLoadedPreprocessedEntity(Index);
  }

  if (PPID.ID == 0)
    return 0;
  unsigned Index = PPID.ID - 1;
  assert(Index < PreprocessedEntities.size() &&
         "Out-of bounds local preprocessed entity");
  return PreprocessedEntities[Index];
}

/// \brief Retrieve the loaded preprocessed entity at the given index.
PreprocessedEntity *
PreprocessingRecord::getLoadedPreprocessedEntity(unsigned Index) {
  assert(Index < LoadedPreprocessedEntities.size() && 
         "Out-of bounds loaded preprocessed entity");
  assert(ExternalSource && "No external source to load from");
  PreprocessedEntity *&Entity = LoadedPreprocessedEntities[Index];
  if (!Entity) {
    Entity = ExternalSource->ReadPreprocessedEntity(Index);
    if (!Entity) // Failed to load.
      Entity = new (*this)
         PreprocessedEntity(PreprocessedEntity::InvalidKind, SourceRange());
  }
  return Entity;
}

MacroDefinition *PreprocessingRecord::findMacroDefinition(const MacroInfo *MI) {
  llvm::DenseMap<const MacroInfo *, PPEntityID>::iterator Pos
    = MacroDefinitions.find(MI);
  if (Pos == MacroDefinitions.end())
    return 0;
  
  PreprocessedEntity *Entity = getPreprocessedEntity(Pos->second);
  if (Entity->isInvalid())
    return 0;
  return cast<MacroDefinition>(Entity);
}

void PreprocessingRecord::addMacroExpansion(const Token &Id,
                                            const MacroInfo *MI,
                                            SourceRange Range) {
  // We don't record nested macro expansions.
  if (Id.getLocation().isMacroID())
    return;

  if (MI->isBuiltinMacro())
    addPreprocessedEntity(
                      new (*this) MacroExpansion(Id.getIdentifierInfo(),Range));
  else if (MacroDefinition *Def = findMacroDefinition(MI))
    addPreprocessedEntity(
                       new (*this) MacroExpansion(Def, Range));
}

void PreprocessingRecord::Ifdef(SourceLocation Loc, const Token &MacroNameTok,
                                const MacroInfo *MI) {
  // This is not actually a macro expansion but record it as a macro reference.
  if (MI)
    addMacroExpansion(MacroNameTok, MI, MacroNameTok.getLocation());
}

void PreprocessingRecord::Ifndef(SourceLocation Loc, const Token &MacroNameTok,
                                 const MacroInfo *MI) {
  // This is not actually a macro expansion but record it as a macro reference.
  if (MI)
    addMacroExpansion(MacroNameTok, MI, MacroNameTok.getLocation());
}

void PreprocessingRecord::Defined(const Token &MacroNameTok,
                                  const MacroInfo *MI) {
  // This is not actually a macro expansion but record it as a macro reference.
  if (MI)
    addMacroExpansion(MacroNameTok, MI, MacroNameTok.getLocation());
}

void PreprocessingRecord::MacroExpands(const Token &Id, const MacroInfo* MI,
                                       SourceRange Range) {
  addMacroExpansion(Id, MI, Range);
}

void PreprocessingRecord::MacroDefined(const Token &Id,
                                       const MacroInfo *MI) {
  SourceRange R(MI->getDefinitionLoc(), MI->getDefinitionEndLoc());
  MacroDefinition *Def
      = new (*this) MacroDefinition(Id.getIdentifierInfo(), R);
  MacroDefinitions[MI] = addPreprocessedEntity(Def);
}

void PreprocessingRecord::MacroUndefined(const Token &Id,
                                         const MacroInfo *MI) {
  // Note: MI may be null (when #undef'ining an undefined macro).
  if (MI)
    MacroDefinitions.erase(MI);
}

void PreprocessingRecord::InclusionDirective(
    SourceLocation HashLoc,
    const clang::Token &IncludeTok,
    StringRef FileName,
    bool IsAngled,
    CharSourceRange FilenameRange,
    const FileEntry *File,
    StringRef SearchPath,
    StringRef RelativePath,
    const Module *Imported) {
  InclusionDirective::InclusionKind Kind = InclusionDirective::Include;
  
  switch (IncludeTok.getIdentifierInfo()->getPPKeywordID()) {
  case tok::pp_include: 
    Kind = InclusionDirective::Include; 
    break;
    
  case tok::pp_import: 
    Kind = InclusionDirective::Import; 
    break;
    
  case tok::pp_include_next: 
    Kind = InclusionDirective::IncludeNext; 
    break;
    
  case tok::pp___include_macros: 
    Kind = InclusionDirective::IncludeMacros;
    break;
    
  default:
    llvm_unreachable("Unknown include directive kind");
  }

  SourceLocation EndLoc;
  if (!IsAngled) {
    EndLoc = FilenameRange.getBegin();
  } else {
    EndLoc = FilenameRange.getEnd();
    if (FilenameRange.isCharRange())
      EndLoc = EndLoc.getLocWithOffset(-1); // the InclusionDirective expects
                                            // a token range.
  }
  clang::InclusionDirective *ID
    = new (*this) clang::InclusionDirective(*this, Kind, FileName, !IsAngled,
                                            (bool)Imported,
                                            File, SourceRange(HashLoc, EndLoc));
  addPreprocessedEntity(ID);
}

size_t PreprocessingRecord::getTotalMemory() const {
  return BumpAlloc.getTotalMemory()
    + llvm::capacity_in_bytes(MacroDefinitions)
    + llvm::capacity_in_bytes(PreprocessedEntities)
    + llvm::capacity_in_bytes(LoadedPreprocessedEntities);
}
