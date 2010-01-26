//===--- Overload.h - C++ Overloading ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the data structures and types used in C++
// overload resolution.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_OVERLOAD_H
#define LLVM_CLANG_SEMA_OVERLOAD_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
  class ASTContext;
  class CXXConstructorDecl;
  class CXXConversionDecl;
  class FunctionDecl;

  /// OverloadingResult - Capture the result of performing overload
  /// resolution.
  enum OverloadingResult {
    OR_Success,             ///< Overload resolution succeeded.
    OR_No_Viable_Function,  ///< No viable function found.
    OR_Ambiguous,           ///< Ambiguous candidates found.
    OR_Deleted              ///< Overload resoltuion refers to a deleted function.
  };
    
  /// ImplicitConversionKind - The kind of implicit conversion used to
  /// convert an argument to a parameter's type. The enumerator values
  /// match with Table 9 of (C++ 13.3.3.1.1) and are listed such that
  /// better conversion kinds have smaller values.
  enum ImplicitConversionKind {
    ICK_Identity = 0,          ///< Identity conversion (no conversion)
    ICK_Lvalue_To_Rvalue,      ///< Lvalue-to-rvalue conversion (C++ 4.1)
    ICK_Array_To_Pointer,      ///< Array-to-pointer conversion (C++ 4.2)
    ICK_Function_To_Pointer,   ///< Function-to-pointer (C++ 4.3)
    ICK_NoReturn_Adjustment,   ///< Removal of noreturn from a type (Clang)
    ICK_Qualification,         ///< Qualification conversions (C++ 4.4)
    ICK_Integral_Promotion,    ///< Integral promotions (C++ 4.5)
    ICK_Floating_Promotion,    ///< Floating point promotions (C++ 4.6)
    ICK_Complex_Promotion,     ///< Complex promotions (Clang extension)
    ICK_Integral_Conversion,   ///< Integral conversions (C++ 4.7)
    ICK_Floating_Conversion,   ///< Floating point conversions (C++ 4.8)
    ICK_Complex_Conversion,    ///< Complex conversions (C99 6.3.1.6)
    ICK_Floating_Integral,     ///< Floating-integral conversions (C++ 4.9)
    ICK_Complex_Real,          ///< Complex-real conversions (C99 6.3.1.7)
    ICK_Pointer_Conversion,    ///< Pointer conversions (C++ 4.10)
    ICK_Pointer_Member,        ///< Pointer-to-member conversions (C++ 4.11)
    ICK_Boolean_Conversion,    ///< Boolean conversions (C++ 4.12)
    ICK_Compatible_Conversion, ///< Conversions between compatible types in C99
    ICK_Derived_To_Base,       ///< Derived-to-base (C++ [over.best.ics])
    ICK_Num_Conversion_Kinds   ///< The number of conversion kinds
  };

  /// ImplicitConversionCategory - The category of an implicit
  /// conversion kind. The enumerator values match with Table 9 of
  /// (C++ 13.3.3.1.1) and are listed such that better conversion
  /// categories have smaller values.
  enum ImplicitConversionCategory {
    ICC_Identity = 0,              ///< Identity
    ICC_Lvalue_Transformation,     ///< Lvalue transformation
    ICC_Qualification_Adjustment,  ///< Qualification adjustment
    ICC_Promotion,                 ///< Promotion
    ICC_Conversion                 ///< Conversion
  };

  ImplicitConversionCategory
  GetConversionCategory(ImplicitConversionKind Kind);

  /// ImplicitConversionRank - The rank of an implicit conversion
  /// kind. The enumerator values match with Table 9 of (C++
  /// 13.3.3.1.1) and are listed such that better conversion ranks
  /// have smaller values.
  enum ImplicitConversionRank {
    ICR_Exact_Match = 0, ///< Exact Match
    ICR_Promotion,       ///< Promotion
    ICR_Conversion       ///< Conversion
  };

  ImplicitConversionRank GetConversionRank(ImplicitConversionKind Kind);

  /// StandardConversionSequence - represents a standard conversion
  /// sequence (C++ 13.3.3.1.1). A standard conversion sequence
  /// contains between zero and three conversions. If a particular
  /// conversion is not needed, it will be set to the identity conversion
  /// (ICK_Identity). Note that the three conversions are
  /// specified as separate members (rather than in an array) so that
  /// we can keep the size of a standard conversion sequence to a
  /// single word.
  struct StandardConversionSequence {
    /// First -- The first conversion can be an lvalue-to-rvalue
    /// conversion, array-to-pointer conversion, or
    /// function-to-pointer conversion.
    ImplicitConversionKind First : 8;

    /// Second - The second conversion can be an integral promotion,
    /// floating point promotion, integral conversion, floating point
    /// conversion, floating-integral conversion, pointer conversion,
    /// pointer-to-member conversion, or boolean conversion.
    ImplicitConversionKind Second : 8;

    /// Third - The third conversion can be a qualification conversion.
    ImplicitConversionKind Third : 8;

    /// Deprecated - Whether this the deprecated conversion of a
    /// string literal to a pointer to non-const character data
    /// (C++ 4.2p2).
    bool Deprecated : 1;

    /// IncompatibleObjC - Whether this is an Objective-C conversion
    /// that we should warn about (if we actually use it).
    bool IncompatibleObjC : 1;

    /// ReferenceBinding - True when this is a reference binding
    /// (C++ [over.ics.ref]).
    bool ReferenceBinding : 1;

    /// DirectBinding - True when this is a reference binding that is a
    /// direct binding (C++ [dcl.init.ref]).
    bool DirectBinding : 1;

    /// RRefBinding - True when this is a reference binding of an rvalue
    /// reference to an rvalue (C++0x [over.ics.rank]p3b4).
    bool RRefBinding : 1;

    /// FromType - The type that this conversion is converting
    /// from. This is an opaque pointer that can be translated into a
    /// QualType.
    void *FromTypePtr;

    /// ToType - The type that this conversion is converting to. This
    /// is an opaque pointer that can be translated into a QualType.
    void *ToTypePtr;

    /// CopyConstructor - The copy constructor that is used to perform
    /// this conversion, when the conversion is actually just the
    /// initialization of an object via copy constructor. Such
    /// conversions are either identity conversions or derived-to-base
    /// conversions.
    CXXConstructorDecl *CopyConstructor;

    void setFromType(QualType T) { FromTypePtr = T.getAsOpaquePtr(); }
    void setToType(QualType T) { ToTypePtr = T.getAsOpaquePtr(); }
    QualType getFromType() const {
      return QualType::getFromOpaquePtr(FromTypePtr);
    }
    QualType getToType() const {
      return QualType::getFromOpaquePtr(ToTypePtr);
    }

    void setAsIdentityConversion();
    ImplicitConversionRank getRank() const;
    bool isPointerConversionToBool() const;
    bool isPointerConversionToVoidPointer(ASTContext& Context) const;
    void DebugPrint() const;
  };

  /// UserDefinedConversionSequence - Represents a user-defined
  /// conversion sequence (C++ 13.3.3.1.2).
  struct UserDefinedConversionSequence {
    /// Before - Represents the standard conversion that occurs before
    /// the actual user-defined conversion. (C++ 13.3.3.1.2p1):
    ///
    ///   If the user-defined conversion is specified by a constructor
    ///   (12.3.1), the initial standard conversion sequence converts
    ///   the source type to the type required by the argument of the
    ///   constructor. If the user-defined conversion is specified by
    ///   a conversion function (12.3.2), the initial standard
    ///   conversion sequence converts the source type to the implicit
    ///   object parameter of the conversion function.
    StandardConversionSequence Before;

    /// EllipsisConversion - When this is true, it means user-defined
    /// conversion sequence starts with a ... (elipsis) conversion, instead of 
    /// a standard conversion. In this case, 'Before' field must be ignored.
    // FIXME. I much rather put this as the first field. But there seems to be
    // a gcc code gen. bug which causes a crash in a test. Putting it here seems
    // to work around the crash.
    bool EllipsisConversion : 1;
    
    /// After - Represents the standard conversion that occurs after
    /// the actual user-defined conversion.
    StandardConversionSequence After;

    /// ConversionFunction - The function that will perform the
    /// user-defined conversion.
    FunctionDecl* ConversionFunction;

    void DebugPrint() const;
  };

  /// Represents an ambiguous user-defined conversion sequence.
  struct AmbiguousConversionSequence {
    typedef llvm::SmallVector<FunctionDecl*, 4> ConversionSet;

    void *FromTypePtr;
    void *ToTypePtr;
    char Buffer[sizeof(ConversionSet)];

    QualType getFromType() const {
      return QualType::getFromOpaquePtr(FromTypePtr);
    }
    QualType getToType() const {
      return QualType::getFromOpaquePtr(ToTypePtr);
    }
    void setFromType(QualType T) { FromTypePtr = T.getAsOpaquePtr(); }
    void setToType(QualType T) { ToTypePtr = T.getAsOpaquePtr(); }

    ConversionSet &conversions() {
      return *reinterpret_cast<ConversionSet*>(Buffer);
    }

    const ConversionSet &conversions() const {
      return *reinterpret_cast<const ConversionSet*>(Buffer);
    }

    void addConversion(FunctionDecl *D) {
      conversions().push_back(D);
    }

    typedef ConversionSet::iterator iterator;
    iterator begin() { return conversions().begin(); }
    iterator end() { return conversions().end(); }

    typedef ConversionSet::const_iterator const_iterator;
    const_iterator begin() const { return conversions().begin(); }
    const_iterator end() const { return conversions().end(); }

    void construct();
    void destruct();
    void copyFrom(const AmbiguousConversionSequence &);
  };

  /// BadConversionSequence - Records information about an invalid
  /// conversion sequence.
  struct BadConversionSequence {
    enum FailureKind {
      no_conversion,
      unrelated_class,
      suppressed_user,
      bad_qualifiers
    };

    // This can be null, e.g. for implicit object arguments.
    Expr *FromExpr;

    FailureKind Kind;

  private:
    // The type we're converting from (an opaque QualType).
    void *FromTy;

    // The type we're converting to (an opaque QualType).
    void *ToTy;

  public:
    void init(FailureKind K, Expr *From, QualType To) {
      init(K, From->getType(), To);
      FromExpr = From;
    }
    void init(FailureKind K, QualType From, QualType To) {
      Kind = K;
      FromExpr = 0;
      setFromType(From);
      setToType(To);
    }

    QualType getFromType() const { return QualType::getFromOpaquePtr(FromTy); }
    QualType getToType() const { return QualType::getFromOpaquePtr(ToTy); }

    void setFromExpr(Expr *E) {
      FromExpr = E;
      setFromType(E->getType());
    }
    void setFromType(QualType T) { FromTy = T.getAsOpaquePtr(); }
    void setToType(QualType T) { ToTy = T.getAsOpaquePtr(); }
  };

  /// ImplicitConversionSequence - Represents an implicit conversion
  /// sequence, which may be a standard conversion sequence
  /// (C++ 13.3.3.1.1), user-defined conversion sequence (C++ 13.3.3.1.2),
  /// or an ellipsis conversion sequence (C++ 13.3.3.1.3).
  struct ImplicitConversionSequence {
    /// Kind - The kind of implicit conversion sequence. BadConversion
    /// specifies that there is no conversion from the source type to
    /// the target type.  AmbiguousConversion represents the unique
    /// ambiguous conversion (C++0x [over.best.ics]p10).
    enum Kind {
      StandardConversion = 0,
      UserDefinedConversion,
      AmbiguousConversion,
      EllipsisConversion,
      BadConversion
    };

  private:
    /// ConversionKind - The kind of implicit conversion sequence.
    Kind ConversionKind;

    void setKind(Kind K) {
      if (isAmbiguous()) Ambiguous.destruct();
      ConversionKind = K;
    }

  public:
    union {
      /// When ConversionKind == StandardConversion, provides the
      /// details of the standard conversion sequence.
      StandardConversionSequence Standard;

      /// When ConversionKind == UserDefinedConversion, provides the
      /// details of the user-defined conversion sequence.
      UserDefinedConversionSequence UserDefined;

      /// When ConversionKind == AmbiguousConversion, provides the
      /// details of the ambiguous conversion.
      AmbiguousConversionSequence Ambiguous;

      /// When ConversionKind == BadConversion, provides the details
      /// of the bad conversion.
      BadConversionSequence Bad;
    };

    ImplicitConversionSequence() : ConversionKind(BadConversion) {}
    ~ImplicitConversionSequence() {
      if (isAmbiguous()) Ambiguous.destruct();
    }
    ImplicitConversionSequence(const ImplicitConversionSequence &Other)
      : ConversionKind(Other.ConversionKind)
    {
      switch (ConversionKind) {
      case StandardConversion: Standard = Other.Standard; break;
      case UserDefinedConversion: UserDefined = Other.UserDefined; break;
      case AmbiguousConversion: Ambiguous.copyFrom(Other.Ambiguous); break;
      case EllipsisConversion: break;
      case BadConversion: Bad = Other.Bad; break;
      }
    }

    ImplicitConversionSequence &
        operator=(const ImplicitConversionSequence &Other) {
      if (isAmbiguous()) Ambiguous.destruct();
      new (this) ImplicitConversionSequence(Other);
      return *this;
    }
    
    Kind getKind() const { return ConversionKind; }
    bool isBad() const { return ConversionKind == BadConversion; }
    bool isStandard() const { return ConversionKind == StandardConversion; }
    bool isEllipsis() const { return ConversionKind == EllipsisConversion; }
    bool isAmbiguous() const { return ConversionKind == AmbiguousConversion; }
    bool isUserDefined() const {
      return ConversionKind == UserDefinedConversion;
    }

    void setBad() { setKind(BadConversion); }
    void setStandard() { setKind(StandardConversion); }
    void setEllipsis() { setKind(EllipsisConversion); }
    void setUserDefined() { setKind(UserDefinedConversion); }
    void setAmbiguous() {
      if (isAmbiguous()) return;
      ConversionKind = AmbiguousConversion;
      Ambiguous.construct();
    }

    // The result of a comparison between implicit conversion
    // sequences. Use Sema::CompareImplicitConversionSequences to
    // actually perform the comparison.
    enum CompareKind {
      Better = -1,
      Indistinguishable = 0,
      Worse = 1
    };

    void DebugPrint() const;
  };

  enum OverloadFailureKind {
    ovl_fail_too_many_arguments,
    ovl_fail_too_few_arguments,
    ovl_fail_bad_conversion,
    ovl_fail_bad_deduction,

    /// This conversion candidate was not considered because it
    /// duplicates the work of a trivial or derived-to-base
    /// conversion.
    ovl_fail_trivial_conversion,

    /// This conversion candidate is not viable because its result
    /// type is not implicitly convertible to the desired type.
    ovl_fail_bad_final_conversion
  };

  /// OverloadCandidate - A single candidate in an overload set (C++ 13.3).
  struct OverloadCandidate {
    /// Function - The actual function that this candidate
    /// represents. When NULL, this is a built-in candidate
    /// (C++ [over.oper]) or a surrogate for a conversion to a
    /// function pointer or reference (C++ [over.call.object]).
    FunctionDecl *Function;

    // BuiltinTypes - Provides the return and parameter types of a
    // built-in overload candidate. Only valid when Function is NULL.
    struct {
      QualType ResultTy;
      QualType ParamTypes[3];
    } BuiltinTypes;

    /// Surrogate - The conversion function for which this candidate
    /// is a surrogate, but only if IsSurrogate is true.
    CXXConversionDecl *Surrogate;

    /// Conversions - The conversion sequences used to convert the
    /// function arguments to the function parameters.
    llvm::SmallVector<ImplicitConversionSequence, 4> Conversions;

    /// Viable - True to indicate that this overload candidate is viable.
    bool Viable;

    /// IsSurrogate - True to indicate that this candidate is a
    /// surrogate for a conversion to a function pointer or reference
    /// (C++ [over.call.object]).
    bool IsSurrogate;

    /// IgnoreObjectArgument - True to indicate that the first
    /// argument's conversion, which for this function represents the
    /// implicit object argument, should be ignored. This will be true
    /// when the candidate is a static member function (where the
    /// implicit object argument is just a placeholder) or a
    /// non-static member function when the call doesn't have an
    /// object argument.
    bool IgnoreObjectArgument;

    /// FailureKind - The reason why this candidate is not viable.
    /// Actually an OverloadFailureKind.
    unsigned char FailureKind;

    /// PathAccess - The 'path access' to the given function/conversion.
    /// Actually an AccessSpecifier.
    unsigned Access;

    AccessSpecifier getAccess() const {
      return AccessSpecifier(Access);
    }

    /// FinalConversion - For a conversion function (where Function is
    /// a CXXConversionDecl), the standard conversion that occurs
    /// after the call to the overload candidate to convert the result
    /// of calling the conversion function to the required type.
    StandardConversionSequence FinalConversion;

    /// hasAmbiguousConversion - Returns whether this overload
    /// candidate requires an ambiguous conversion or not.
    bool hasAmbiguousConversion() const {
      for (llvm::SmallVectorImpl<ImplicitConversionSequence>::const_iterator
             I = Conversions.begin(), E = Conversions.end(); I != E; ++I) {
        if (I->isAmbiguous()) return true;
      }
      return false;
    }
  };

  /// OverloadCandidateSet - A set of overload candidates, used in C++
  /// overload resolution (C++ 13.3).
  class OverloadCandidateSet : public llvm::SmallVector<OverloadCandidate, 16> {
    typedef llvm::SmallVector<OverloadCandidate, 16> inherited;
    llvm::SmallPtrSet<Decl *, 16> Functions;
    
  public:
    /// \brief Determine when this overload candidate will be new to the
    /// overload set.
    bool isNewCandidate(Decl *F) { 
      return Functions.insert(F->getCanonicalDecl()); 
    }

    /// \brief Clear out all of the candidates.
    void clear() {
      inherited::clear();
      Functions.clear();
    }
  };
} // end namespace clang

#endif // LLVM_CLANG_SEMA_OVERLOAD_H
