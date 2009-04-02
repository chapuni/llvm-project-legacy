//===--- LangOptions.h - C Language Family Language Options -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the LangOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LANGOPTIONS_H
#define LLVM_CLANG_LANGOPTIONS_H

#include "llvm/Bitcode/SerializationFwd.h"

namespace clang {

/// LangOptions - This class keeps track of the various options that can be
/// enabled, which controls the dialect of C that is accepted.
class LangOptions {
public:
  unsigned Trigraphs         : 1;  // Trigraphs in source files.
  unsigned BCPLComment       : 1;  // BCPL-style '//' comments.
  unsigned DollarIdents      : 1;  // '$' allowed in identifiers.
  unsigned AsmPreprocessor   : 1;  // Preprocessor in asm mode.
  unsigned GNUMode           : 1;  // True in gnu99 mode false in c99 mode (etc)
  unsigned ImplicitInt       : 1;  // C89 implicit 'int'.
  unsigned Digraphs          : 1;  // C94, C99 and C++
  unsigned HexFloats         : 1;  // C99 Hexadecimal float constants.
  unsigned C99               : 1;  // C99 Support
  unsigned Microsoft         : 1;  // Microsoft extensions.
  unsigned CPlusPlus         : 1;  // C++ Support
  unsigned CPlusPlus0x       : 1;  // C++0x Support
  unsigned NoExtensions      : 1;  // All extensions are disabled, strict mode.
  unsigned CXXOperatorNames  : 1;  // Treat C++ operator names as keywords.
    
  unsigned ObjC1             : 1;  // Objective-C 1 support enabled.
  unsigned ObjC2             : 1;  // Objective-C 2 support enabled.
  unsigned ObjCNonFragileABI : 1;  // Objective-C modern abi enabled
    
  unsigned PascalStrings     : 1;  // Allow Pascal strings
  unsigned Boolean           : 1;  // Allow bool/true/false
  unsigned WritableStrings   : 1;  // Allow writable strings
  unsigned LaxVectorConversions : 1;
  unsigned Exceptions        : 1;  // Support exception handling.

  unsigned NeXTRuntime       : 1; // Use NeXT runtime.
  unsigned Freestanding      : 1; // Freestanding implementation
  unsigned NoBuiltin         : 1; // Do not use builtin functions (-fno-builtin)

  unsigned ThreadsafeStatics : 1; // Whether static initializers are protected
                                  // by locks.
  unsigned Blocks            : 1; // block extension to C
  unsigned EmitAllDecls      : 1; // Emit all declarations, even if
                                  // they are unused.
  unsigned MathErrno         : 1; // Math functions must respect errno
                                  // (modulo the platform support).

  unsigned OverflowChecking  : 1; // Extension to call a handler function when
                                  // signed integer arithmetic overflows.

  unsigned HeinousExtensions : 1; // Extensions that we really don't like and
                                  // may be ripped out at any time.
private:
  unsigned GC : 2; // Objective-C Garbage Collection modes.  We declare
                   // this enum as unsigned because MSVC insists on making enums
                   // signed.  Set/Query this value using accessors.  
public:  
  unsigned InstantiationDepth;    // Maximum template instantiation depth.

  enum GCMode { NonGC, GCOnly, HybridGC };
  
  LangOptions() {
    Trigraphs = BCPLComment = DollarIdents = AsmPreprocessor = 0;
    GNUMode = ImplicitInt = Digraphs = 0;
    HexFloats = 0;
    GC = ObjC1 = ObjC2 = ObjCNonFragileABI = 0;
    C99 = Microsoft = CPlusPlus = CPlusPlus0x = NoExtensions = 0;
    CXXOperatorNames = PascalStrings = Boolean = WritableStrings = 0;
    Exceptions = NeXTRuntime = Freestanding = NoBuiltin = 0;
    LaxVectorConversions = 1;
    HeinousExtensions = 0;
    
    // FIXME: The default should be 1.
    ThreadsafeStatics = 0;
    Blocks = 0;
    EmitAllDecls = 0;
    MathErrno = 1;

    OverflowChecking = 0;

    InstantiationDepth = 99;
  }
  
  GCMode getGCMode() const { return (GCMode) GC; }
  void setGCMode(GCMode m) { GC = (unsigned) m; }
  
  /// Emit - Emit this LangOptions object to bitcode.
  void Emit(llvm::Serializer& S) const;
  
  /// Read - Read new values for this LangOption object from bitcode.
  void Read(llvm::Deserializer& S);  
};

}  // end namespace clang

#endif
