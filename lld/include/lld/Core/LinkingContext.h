//===- lld/Core/LinkingContext.h - Linker Target Info Interface -----------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_CORE_TARGET_INFO_H
#define LLD_CORE_TARGET_INFO_H

#include "lld/Core/Error.h"
#include "lld/Core/LLVM.h"
#include "lld/Core/range.h"
#include "lld/Core/Reference.h"

#include "lld/Driver/InputGraph.h"
#include "lld/ReaderWriter/Reader.h"

#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

namespace llvm {
class Triple;
}

namespace lld {
class PassManager;
class File;
class Writer;
class InputGraph;
class InputElement;

/// \brief The LinkingContext class encapsulates "what and how" to link.
///
/// The base class LinkingContext contains the options needed by core linking.
/// Subclasses of LinkingContext have additional options needed by specific
/// Readers
/// and Writers. For example, ELFLinkingContext has methods that supplies
/// options
/// to the ELF Reader and Writer.
class LinkingContext {
public:
  /// \brief The types of output file that the linker
  /// creates.
  enum class OutputFileType : uint8_t {
    Default, // The default output type for this target
    YAML,    // The output type is set to YAML
    Native   // The output file format is Native (Atoms)
  };

  virtual ~LinkingContext();

  /// \name Methods needed by core linking
  /// @{

  /// Name of symbol linker should use as "entry point" to program,
  /// usually "main" or "start".
  virtual StringRef entrySymbolName() const { return _entrySymbolName; }

  /// Whether core linking should remove Atoms not reachable by following
  /// References from the entry point Atom or from all global scope Atoms
  /// if globalsAreDeadStripRoots() is true.
  bool deadStrip() const { return _deadStrip; }

  /// Only used if deadStrip() returns true.  Means all global scope Atoms
  /// should be marked live (along with all Atoms they reference).  Usually
  /// this method returns false for main executables, but true for dynamic
  /// shared libraries.
  bool globalsAreDeadStripRoots() const {
    assert(_deadStrip && "only applicable when deadstripping enabled");
    return _globalsAreDeadStripRoots;
  }

  /// Only used if deadStrip() returns true.  This method returns the names
  /// of DefinedAtoms that should be marked live (along with all Atoms they
  /// reference). Only Atoms with scope scopeLinkageUnit or scopeGlobal can
  /// be kept live using this method.
  const std::vector<StringRef> &deadStripRoots() const {
    return _deadStripRoots;
  }

  /// Add the given symbol name to the dead strip root set. Only used if
  /// deadStrip() returns true.
  void addDeadStripRoot(StringRef symbolName) {
    assert(_deadStrip && "only applicable when deadstripping enabled");
    _deadStripRoots.push_back(symbolName);
  }

  /// Archive files (aka static libraries) are normally lazily loaded.  That is,
  /// object files within an archive are only loaded and linked in, if the
  /// object file contains a DefinedAtom which will replace an existing
  /// UndefinedAtom.  If this method returns true, core linking will also look
  /// for archive members to replace existing tentative definitions in addition
  /// to replacing undefines. Note: a "tentative definition" (also called a
  /// "common" symbols) is a C (but not C++) concept. They are modeled in lld
  /// as a DefinedAtom with merge() of mergeAsTentative.
  bool searchArchivesToOverrideTentativeDefinitions() const {
    return _searchArchivesToOverrideTentativeDefinitions;
  }

  /// Normally core linking will turn a tentative definition into a real
  /// definition if not replaced by a real DefinedAtom from some object file.
  /// If this method returns true, core linking will search all supplied
  /// dynamic shared libraries for symbol names that match remaining tentative
  /// definitions.  If any are found, the corresponding tentative definition
  /// atom is replaced with SharedLibraryAtom.
  bool searchSharedLibrariesToOverrideTentativeDefinitions() const {
    return _searchSharedLibrariesToOverrideTentativeDefinitions;
  }

  /// Normally, every UndefinedAtom must be replaced by a DefinedAtom or a
  /// SharedLibraryAtom for the link to be successful.  This method controls
  /// whether core linking prints out a list of remaining UndefinedAtoms.
  ///
  /// \todo This should be a method core linking calls with a list of the
  /// UndefinedAtoms so that different drivers can format the error message
  /// as needed.
  bool printRemainingUndefines() const { return _printRemainingUndefines; }

  /// Normally, every UndefinedAtom must be replaced by a DefinedAtom or a
  /// SharedLibraryAtom for the link to be successful.  This method controls
  /// whether core linking considers remaining undefines to be an error.
  bool allowRemainingUndefines() const { return _allowRemainingUndefines; }

  /// In the lld model, a SharedLibraryAtom is a proxy atom for something
  /// that will be found in a dynamic shared library when the program runs.
  /// A SharedLibraryAtom optionally contains the name of the shared library
  /// in which to find the symbol name at runtime.  Core linking may merge
  /// two SharedLibraryAtom with the same name.  If this method returns true,
  /// when merging core linking will also verify that they both have the same
  /// loadName() and if not print a warning.
  ///
  /// \todo This should be a method core linking calls so that drivers can
  /// format the warning as needed.
  bool warnIfCoalesableAtomsHaveDifferentLoadName() const {
    return _warnIfCoalesableAtomsHaveDifferentLoadName;
  }

  /// In C/C++ you can mark a function's prototype with
  /// __attribute__((weak_import)) or __attribute__((weak)) to say the function
  /// may not be available at runtime and/or build time and in which case its
  /// address will evaluate to NULL. In lld this is modeled using the
  /// UndefinedAtom::canBeNull() method.  During core linking, UndefinedAtom
  /// with the same name are automatically merged.  If this method returns
  /// true, core link also verfies that the canBeNull() value for merged
  /// UndefinedAtoms are the same and warns if not.
  ///
  /// \todo This should be a method core linking calls so that drivers can
  /// format the warning as needed.
  bool warnIfCoalesableAtomsHaveDifferentCanBeNull() const {
    return _warnIfCoalesableAtomsHaveDifferentCanBeNull;
  }

  /// Normally, every UndefinedAtom must be replaced by a DefinedAtom or a
  /// SharedLibraryAtom for the link to be successful.  This method controls
  /// whether core linking considers remaining undefines from the shared library
  /// to be an error.
  bool allowShlibUndefines() const { return _allowShlibUndefines; }

  /// If true, core linking will write the path to each input file to stdout
  /// (i.e. llvm::outs()) as it is used.  This is used to implement the -t
  /// linker option.
  ///
  /// \todo This should be a method core linking calls so that drivers can
  /// format the line as needed.
  bool logInputFiles() const { return _logInputFiles; }

  /// Parts of LLVM use global variables which are bound to command line
  /// options (see llvm::cl::Options). This method returns "command line"
  /// options which are used to configure LLVM's command line settings.
  /// For instance the -debug-only XXX option can be used to dynamically
  /// trace different parts of LLVM and lld.
  const std::vector<const char *> &llvmOptions() const { return _llvmOptions; }

  /// \name Methods used by Drivers to configure TargetInfo
  /// @{
  void setOutputPath(StringRef str) { _outputPath = str; }

  // Set the entry symbol name. You may also need to call addDeadStripRoot() for
  // the symbol if your platform supports dead-stripping, so that the symbol
  // will not be removed from the output.
  void setEntrySymbolName(StringRef name) {
    _entrySymbolName = name;
  }

  void setDeadStripping(bool enable) { _deadStrip = enable; }
  void setGlobalsAreDeadStripRoots(bool v) { _globalsAreDeadStripRoots = v; }
  void setSearchArchivesToOverrideTentativeDefinitions(bool search) {
    _searchArchivesToOverrideTentativeDefinitions = search;
  }
  void setSearchSharedLibrariesToOverrideTentativeDefinitions(bool search) {
    _searchSharedLibrariesToOverrideTentativeDefinitions = search;
  }
  void setWarnIfCoalesableAtomsHaveDifferentCanBeNull(bool warn) {
    _warnIfCoalesableAtomsHaveDifferentCanBeNull = warn;
  }
  void setWarnIfCoalesableAtomsHaveDifferentLoadName(bool warn) {
    _warnIfCoalesableAtomsHaveDifferentLoadName = warn;
  }
  void setPrintRemainingUndefines(bool print) {
    _printRemainingUndefines = print;
  }
  void setAllowRemainingUndefines(bool allow) {
    _allowRemainingUndefines = allow;
  }
  void setAllowShlibUndefines(bool allow) { _allowShlibUndefines = allow; }
  void setLogInputFiles(bool log) { _logInputFiles = log; }

  void appendLLVMOption(const char *opt) { _llvmOptions.push_back(opt); }
  virtual void setInputGraph(std::unique_ptr<InputGraph> inputGraph) {
    _inputGraph = std::move(inputGraph);
  }
  virtual InputGraph &inputGraph() const { return *_inputGraph; }

  /// This method adds undefined symbols specified by the -u option to the to
  /// the list of undefined symbols known to the linker. This option essentially
  /// forces an undefined symbol to be create. You may also need to call
  /// addDeadStripRoot() for the symbol if your platform supports dead
  /// stripping, so that the symbol will not be removed from the output.
  void addInitialUndefinedSymbol(StringRef symbolName) {
    _initialUndefinedSymbols.push_back(symbolName);
  }

  /// Iterators for symbols that appear on the command line
  typedef std::vector<StringRef> StringRefVector;
  typedef StringRefVector::iterator StringRefVectorIter;
  typedef StringRefVector::const_iterator StringRefVectorConstIter;

  /// Create linker internal files containing atoms for the linker to include
  /// during link. Flavors can override this function in their LinkingContext
  /// to add more internal files. These internal files are positioned before
  /// the actual input files.
  virtual bool createInternalFiles(std::vector<std::unique_ptr<File> > &) const;

  /// Return the list of undefined symbols that are specified in the
  /// linker command line, using the -u option.
  range<const StringRef *> initialUndefinedSymbols() const {
    return _initialUndefinedSymbols;
  }

  /// After all set* methods are called, the Driver calls this method
  /// to validate that there are no missing options or invalid combinations
  /// of options.  If there is a problem, a description of the problem
  /// is written to the supplied stream.
  ///
  /// \returns true if there is an error with the current settings.
  bool validate(raw_ostream &diagnostics);

  /// @}
  /// \name Methods used by Driver::link()
  /// @{

  /// Returns the file system path to which the linked output should be written.
  ///
  /// \todo To support in-memory linking, we need an abstraction that allows
  /// the linker to write to an in-memory buffer.
  StringRef outputPath() const { return _outputPath; }

  /// Set the various output file types that the linker would
  /// create
  bool setOutputFileType(StringRef outputFileType) {
    StringRef lowerOutputFileType = outputFileType.lower();
    if (lowerOutputFileType == "yaml")
      _outputFileType = OutputFileType::YAML;
    else if (lowerOutputFileType == "native")
      _outputFileType = OutputFileType::YAML;
    else
      return false;
    return true;
  }

  /// Returns the output file that that the linker needs to create
  OutputFileType outputFileType() const { return _outputFileType; }

  /// Returns the YAML reader.
  virtual Reader &getYAMLReader() const { return *_yamlReader; }

  /// Returns the LLD Native file format reader.
  virtual Reader &getNativeReader() const { return *_nativeReader; }

  /// Return the default reader for the target
  virtual Reader &getDefaultReader() const = 0;

  /// This method is called by core linking to give the Writer a chance
  /// to add file format specific "files" to set of files to be linked. This is
  /// how file format specific atoms can be added to the link.
  virtual bool createImplicitFiles(std::vector<std::unique_ptr<File> > &) const;

  /// This method is called by core linking to build the list of Passes to be
  /// run on the merged/linked graph of all input files.
  virtual void addPasses(PassManager &pm) const;

  /// Calls through to the writeFile() method on the specified Writer.
  ///
  /// \param linkedFile This is the merged/linked graph of all input file Atoms.
  virtual error_code writeFile(const File &linkedFile) const;

  /// nextFile returns the next file that needs to be processed by the resolver.
  /// The LinkingContext's can override the default behavior to change the way
  /// the resolver operates. This uses the currentInputElement. When there are
  /// no more files to be processed an appropriate InputGraphError is
  /// returned. Ordinals are assigned to files returned by nextFile, which means
  /// ordinals would be assigned in the way files are resolved.
  virtual ErrorOr<File &> nextFile();

  /// Set the resolver state for the current Input element This is used by the
  /// InputGraph to decide the next file that needs to be processed for various
  /// types of nodes in the InputGraph. The resolver state is nothing but a
  /// bitmask of various types of states that the resolver handles when adding
  /// atoms.
  virtual void setResolverState(uint32_t resolverState);

  /// Return the next ordinal and Increment it.
  virtual uint64_t getNextOrdinalAndIncrement() const { return _nextOrdinal++; }

  /// @}

  /// \name Methods needed by YAML I/O and error messages to convert Kind values
  /// to and from strings.
  /// @{

  /// Abstract method to parse a kind name string into an integral
  /// Reference::Kind
  virtual ErrorOr<Reference::Kind> relocKindFromString(StringRef str) const = 0;

  /// Abstract method to return the name for a given integral
  /// Reference::Kind.
  virtual ErrorOr<std::string> stringFromRelocKind(Reference::Kind k) const = 0;

  /// @}

protected:
  LinkingContext(); // Must be subclassed

  /// Abstract method to lazily instantiate the Writer.
  virtual Writer &writer() const = 0;

  /// Method to create a internal file for the entry symbol
  virtual std::unique_ptr<File> createEntrySymbolFile() const;

  /// Method to create a internal file for an undefined symbol
  virtual std::unique_ptr<File> createUndefinedSymbolFile() const;

  StringRef _outputPath;
  StringRef _entrySymbolName;
  bool _deadStrip;
  bool _globalsAreDeadStripRoots;
  bool _searchArchivesToOverrideTentativeDefinitions;
  bool _searchSharedLibrariesToOverrideTentativeDefinitions;
  bool _warnIfCoalesableAtomsHaveDifferentCanBeNull;
  bool _warnIfCoalesableAtomsHaveDifferentLoadName;
  bool _printRemainingUndefines;
  bool _allowRemainingUndefines;
  bool _logInputFiles;
  bool _allowShlibUndefines;
  OutputFileType _outputFileType;
  std::vector<StringRef> _deadStripRoots;
  std::vector<const char *> _llvmOptions;
  std::unique_ptr<Reader> _yamlReader;
  std::unique_ptr<Reader> _nativeReader;
  StringRefVector _initialUndefinedSymbols;
  std::unique_ptr<InputGraph> _inputGraph;
  mutable llvm::BumpPtrAllocator _allocator;
  InputElement *_currentInputElement;
  mutable uint64_t _nextOrdinal;

private:
  /// Validate the subclass bits. Only called by validate.
  virtual bool validateImpl(raw_ostream &diagnostics) = 0;
};
} // end namespace lld

#endif
