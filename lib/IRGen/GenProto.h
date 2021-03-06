//===--- GenProto.h - Swift IR generation for prototypes --------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file provides the private interface to the protocol-emission code.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_GENPROTO_H
#define SWIFT_IRGEN_GENPROTO_H

#include "swift/SIL/SILFunction.h"

#include "Fulfillment.h"
#include "GenericRequirement.h"
#include "MetadataPath.h"

namespace llvm {
  class Type;
}

namespace swift {
  class CanType;
  class FuncDecl;
  class ProtocolConformanceRef;
  struct SILDeclRef;
  class SILType;
  class SILFunction;

namespace irgen {
  class Address;
  class Explosion;
  class CallEmission;
  class IRGenFunction;
  class IRGenModule;
  class ProtocolInfo;
  class TypeInfo;

  /// Set an LLVM value name for the given type metadata.
  void setTypeMetadataName(IRGenModule &IGM, llvm::Value *value, CanType type);

  /// Set an LLVM value name for the given protocol witness table.
  void setProtocolWitnessTableName(IRGenModule &IGM, llvm::Value *value,
                                   CanType type, ProtocolDecl *protocol);
  
  /// Extract the method pointer from an archetype's witness table
  /// as a function value.
  void emitWitnessMethodValue(IRGenFunction &IGF,
                              CanType baseTy,
                              llvm::Value **baseMetadataCache,
                              SILDeclRef member,
                              ProtocolConformanceRef conformance,
                              Explosion &out);

  /// Given a type T and an associated type X of some protocol P to
  /// which T conforms, return the type metadata for T.X.
  ///
  /// \param parentMetadata - the type metadata for T
  /// \param wtable - the witness table witnessing the conformance of T to P
  /// \param associatedType - the declaration of X; a member of P
  llvm::Value *emitAssociatedTypeMetadataRef(IRGenFunction &IGF,
                                             llvm::Value *parentMetadata,
                                             llvm::Value *wtable,
                                           AssociatedTypeDecl *associatedType);

  /// Given a type T and an associated type X of a protocol PT to which
  /// T conforms, where X is required to implement some protocol PX, return
  /// the witness table witnessing the conformance of T.X to PX.
  ///
  /// PX must be a direct requirement of X.
  ///
  /// \param parentMetadata - the type metadata for T
  /// \param wtable - the witness table witnessing the conformance of T to PT
  /// \param associatedType - the declaration of X; a member of PT
  /// \param associatedTypeMetadata - the type metadata for T.X
  /// \param associatedProtocol - the declaration of PX
  llvm::Value *emitAssociatedTypeWitnessTableRef(IRGenFunction &IGF,
                                                 llvm::Value *parentMetadata,
                                                 llvm::Value *wtable,
                                          AssociatedTypeDecl *associatedType,
                                          llvm::Value *associatedTypeMetadata,
                                          ProtocolDecl *associatedProtocol);

  /// Add the witness parameters necessary for calling a function with
  /// the given generics clause.
  void expandPolymorphicSignature(IRGenModule &IGM,
                                  CanSILFunctionType type,
                                  SmallVectorImpl<llvm::Type*> &types);

  /// Return the number of trailing arguments necessary for calling a
  /// witness method.
  inline unsigned getTrailingWitnessSignatureLength(IRGenModule &IGM,
                                                    CanSILFunctionType type) {
    return 2;
  }

  /// Add the trailing arguments necessary for calling a witness method.
  void expandTrailingWitnessSignature(IRGenModule &IGM,
                                      CanSILFunctionType type,
                                      SmallVectorImpl<llvm::Type*> &types);

  struct WitnessMetadata {
    llvm::Value *SelfMetadata = nullptr;
    llvm::Value *SelfWitnessTable = nullptr;
  };

  /// Collect any required metadata for a witness method from the end
  /// of the given parameter list.
  void collectTrailingWitnessMetadata(IRGenFunction &IGF, SILFunction &fn,
                                      Explosion &params,
                                      WitnessMetadata &metadata);

  using GetParameterFn = llvm::function_ref<llvm::Value*(unsigned)>;

  /// In the prelude of a generic function, perform the bindings for a
  /// generics clause.
  ///
  /// \param witnessMetadata - can be omitted if the function is
  ///   definitely not a witness method
  void emitPolymorphicParameters(IRGenFunction &IGF,
                                 SILFunction &Fn,
                                 Explosion &args,
                                 WitnessMetadata *witnessMetadata,
                                 const GetParameterFn &getParameter);
  
  /// When calling a polymorphic call, pass the arguments for the
  /// generics clause.
  void emitPolymorphicArguments(IRGenFunction &IGF,
                                CanSILFunctionType origType,
                                CanSILFunctionType substType,
                                ArrayRef<Substitution> subs,
                                WitnessMetadata *witnessMetadata,
                                Explosion &args);

  /// Emit references to the witness tables for the substituted type
  /// in the given substitution.
  void emitWitnessTableRefs(IRGenFunction &IGF,
                            const Substitution &sub,
                            llvm::Value **metadataCache,
                            SmallVectorImpl<llvm::Value *> &out);

  /// Emit a witness table reference.
  llvm::Value *emitWitnessTableRef(IRGenFunction &IGF,
                                   CanType srcType,
                                   llvm::Value **srcMetadataCache,
                                   ProtocolConformanceRef conformance);

  llvm::Value *emitWitnessTableRef(IRGenFunction &IGF,
                                   CanType srcType,
                                   ProtocolConformanceRef conformance);

  /// An entry in a list of known protocols.
  class ProtocolEntry {
    ProtocolDecl *Protocol;
    const ProtocolInfo &Impl;

  public:
    explicit ProtocolEntry(ProtocolDecl *proto, const ProtocolInfo &impl)
      : Protocol(proto), Impl(impl) {}

    ProtocolDecl *getProtocol() const { return Protocol; }
    const ProtocolInfo &getInfo() const { return Impl; }
  };

  using GetWitnessTableFn =
    llvm::function_ref<llvm::Value*(unsigned originIndex)>;
  llvm::Value *emitImpliedWitnessTableRef(IRGenFunction &IGF,
                                          ArrayRef<ProtocolEntry> protos,
                                          ProtocolDecl *target,
                                    const GetWitnessTableFn &getWitnessTable);

  /// A class for computing how to pass arguments to a polymorphic
  /// function.  The subclasses of this are the places which need to
  /// be updated if the convention changes.
  class PolymorphicConvention {
  public:
    enum class SourceKind {
      /// Metadata is derived from a source class pointer.
      ClassPointer,

      /// Metadata is derived from a type metadata pointer.
      Metadata,

      /// Metadata is derived from the origin type parameter.
      GenericLValueMetadata,

      /// Metadata is obtained directly from the from a Self metadata
      /// parameter passed via the WitnessMethod convention.
      SelfMetadata,

      /// Metadata is derived from the Self witness table parameter
      /// passed via the WitnessMethod convention.
      SelfWitnessTable,
    };

    static bool requiresSourceIndex(SourceKind kind) {
      return (kind == SourceKind::ClassPointer ||
              kind == SourceKind::Metadata ||
              kind == SourceKind::GenericLValueMetadata);
    }

    enum : unsigned { InvalidSourceIndex = ~0U };

    class Source {
      /// The kind of source this is.
      SourceKind Kind;

      /// The parameter index, for ClassPointer and Metadata sources.
      unsigned Index;

    public:
      CanType Type;

      Source(SourceKind kind, unsigned index, CanType type)
        : Kind(kind), Index(index), Type(type) {
        assert(index != InvalidSourceIndex || !requiresSourceIndex(kind));
      }

      SourceKind getKind() const { return Kind; }
      unsigned getParamIndex() const {
        assert(requiresSourceIndex(getKind()));
        return Index;
      }

      template <typename Allocator>
      const reflection::MetadataSource *getMetadataSource(Allocator &A) const {
        switch (Kind) {
        case SourceKind::ClassPointer:
          return A.template createReferenceCapture(Index);
        case SourceKind::Metadata:
          return A.template createMetadataCapture(Index);
        default:
          return nullptr;
        }
      }
    };

  protected:
    IRGenModule &IGM;
    ModuleDecl &M;
    CanSILFunctionType FnType;

    CanGenericSignature Generics;

    std::vector<Source> Sources;

    FulfillmentMap Fulfillments;

    GenericSignature::ConformsToArray getConformsTo(Type t) {
      return Generics->getConformsTo(t, M);
    }

  public:
    PolymorphicConvention(IRGenModule &IGM, CanSILFunctionType fnType);

    ArrayRef<Source> getSources() const { return Sources; }

    using RequirementCallback =
      llvm::function_ref<void(GenericRequirement requirement)>;

    void enumerateRequirements(const RequirementCallback &callback);

    void enumerateUnfulfilledRequirements(const RequirementCallback &callback);

    /// Returns a Fulfillment for a type parameter requirement, or
    /// nullptr if it's unfulfilled.
    const Fulfillment *getFulfillmentForTypeMetadata(CanType type) const;

    /// Return the source of type metadata at a particular source index.
    Source getSource(size_t SourceIndex) const {
      return Sources[SourceIndex];
    }

  private:
    void initGenerics();
    void considerNewTypeSource(SourceKind kind, unsigned paramIndex,
                               CanType type, IsExact_t isExact,
                               bool isSelfParameter);
    bool considerType(CanType type, IsExact_t isExact, bool isSelfParameter,
                      unsigned sourceIndex, MetadataPath &&path);

    /// Testify to generic parameters in the Self type of a protocol
    /// witness method.
    void considerWitnessSelf(CanSILFunctionType fnType);

    /// Testify to generic parameters in the Self type of an @objc
    /// generic or protocol method.
    void considerObjCGenericSelf(CanSILFunctionType fnType);

    void considerParameter(SILParameterInfo param, unsigned paramIndex,
                           bool isSelfParameter);

    void addSelfMetadataFulfillment(CanType arg);
    void addSelfWitnessTableFulfillment(CanType arg, ProtocolDecl *proto);
  };

  /// A class for binding type parameters of a generic function.
  class EmitPolymorphicParameters : public PolymorphicConvention {
    IRGenFunction &IGF;
    SILFunction &Fn;

  public:
    EmitPolymorphicParameters(IRGenFunction &IGF, SILFunction &Fn);

    void emit(Explosion &in, WitnessMetadata *witnessMetadata,
              const GetParameterFn &getParameter);

  private:
    CanType getTypeInContext(CanType type) const;

    CanType getArgTypeInContext(unsigned paramIndex) const;

    /// Fulfill local type data from any extra information associated with
    /// the given source.
    void bindExtraSource(const Source &source, Explosion &in,
                         WitnessMetadata *witnessMetadata);

    void bindParameterSources(const GetParameterFn &getParameter);

    void bindParameterSource(SILParameterInfo param, unsigned paramIndex,
                             const GetParameterFn &getParameter) ;
    // Did the convention decide that the parameter at the given index
    // was a class-pointer source?
    bool isClassPointerSource(unsigned paramIndex);
    
    void bindArchetypeAccessPaths();
    void addPotentialArchetypeAccessPath(CanType targetDepType,
                                         CanType sourceDepType);
  };

} // end namespace irgen
} // end namespace swift

#endif
