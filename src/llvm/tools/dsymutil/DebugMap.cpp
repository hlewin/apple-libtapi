//===- tools/dsymutil/DebugMap.cpp - Generic debug map representation -----===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DebugMap.h"
#include "BinaryHolder.h"
#include "ErrorReporting.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Chrono.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm {

namespace dsymutil {

using namespace llvm::object;

DebugMapObject::DebugMapObject(StringRef ObjectFilename,
                               sys::TimePoint<std::chrono::seconds> Timestamp,
                               uint8_t Type)
    : Filename(ObjectFilename), Timestamp(Timestamp), Type(Type) {}

bool DebugMapObject::addSymbol(StringRef Name, Optional<uint64_t> ObjectAddress,
                               uint64_t LinkedAddress, uint32_t Size) {
  auto InsertResult = Symbols.insert(
      std::make_pair(Name, SymbolMapping(ObjectAddress, LinkedAddress, Size)));

  if (ObjectAddress && InsertResult.second)
    AddressToMapping[*ObjectAddress] = &*InsertResult.first;
  return InsertResult.second;
}

void DebugMapObject::print(raw_ostream &OS) const {
  OS << getObjectFilename() << ":\n";
  // Sort the symbols in alphabetical order, like llvm-nm (and to get
  // deterministic output for testing).
  using Entry = std::pair<StringRef, SymbolMapping>;
  std::vector<Entry> Entries;
  Entries.reserve(Symbols.getNumItems());
  for (const auto &Sym : make_range(Symbols.begin(), Symbols.end()))
    Entries.push_back(std::make_pair(Sym.getKey(), Sym.getValue()));
  std::sort(
      Entries.begin(), Entries.end(),
      [](const Entry &LHS, const Entry &RHS) { return LHS.first < RHS.first; });
  for (const auto &Sym : Entries) {
    if (Sym.second.ObjectAddress)
      OS << format("\t%016" PRIx64, uint64_t(*Sym.second.ObjectAddress));
    else
      OS << "\t????????????????";
    OS << format(" => %016" PRIx64 "+0x%x\t%s\n",
                 uint64_t(Sym.second.BinaryAddress), uint32_t(Sym.second.Size),
                 Sym.first.data());
  }
  OS << '\n';
}

#ifndef NDEBUG
void DebugMapObject::dump() const { print(errs()); }
#endif

DebugMapObject &
DebugMap::addDebugMapObject(StringRef ObjectFilePath,
                            sys::TimePoint<std::chrono::seconds> Timestamp,
                            uint8_t Type) {
  Objects.emplace_back(new DebugMapObject(ObjectFilePath, Timestamp, Type));
  return *Objects.back();
}

const DebugMapObject::DebugMapEntry *
DebugMapObject::lookupSymbol(StringRef SymbolName) const {
  StringMap<SymbolMapping>::const_iterator Sym = Symbols.find(SymbolName);
  if (Sym == Symbols.end())
    return nullptr;
  return &*Sym;
}

const DebugMapObject::DebugMapEntry *
DebugMapObject::lookupObjectAddress(uint64_t Address) const {
  auto Mapping = AddressToMapping.find(Address);
  if (Mapping == AddressToMapping.end())
    return nullptr;
  return Mapping->getSecond();
}

void DebugMap::print(raw_ostream &OS) const {
  yaml::Output yout(OS, /* Ctxt = */ nullptr, /* WrapColumn = */ 0);
  yout << const_cast<DebugMap &>(*this);
}

#ifndef NDEBUG
void DebugMap::dump() const { print(errs()); }
#endif

namespace {

struct YAMLContext {
  StringRef PrependPath;
  Triple BinaryTriple;
};

} // end anonymous namespace

ErrorOr<std::vector<std::unique_ptr<DebugMap>>>
DebugMap::parseYAMLDebugMap(StringRef InputFile, StringRef PrependPath,
                            bool Verbose) {
  auto ErrOrFile = MemoryBuffer::getFileOrSTDIN(InputFile);
  if (auto Err = ErrOrFile.getError())
    return Err;

  YAMLContext Ctxt;

  Ctxt.PrependPath = PrependPath;

  std::unique_ptr<DebugMap> Res;
  yaml::Input yin((*ErrOrFile)->getBuffer(), &Ctxt);
  yin >> Res;

  if (auto EC = yin.error())
    return EC;
  std::vector<std::unique_ptr<DebugMap>> Result;
  Result.push_back(std::move(Res));
  return std::move(Result);
}

} // end namespace dsymutil

namespace yaml {

// Normalize/Denormalize between YAML and a DebugMapObject.
struct MappingTraits<dsymutil::DebugMapObject>::YamlDMO {
  YamlDMO(IO &io) { Timestamp = 0; }
  YamlDMO(IO &io, dsymutil::DebugMapObject &Obj);
  dsymutil::DebugMapObject denormalize(IO &IO);

  std::string Filename;
  int64_t Timestamp;
  std::vector<dsymutil::DebugMapObject::YAMLSymbolMapping> Entries;
};

void MappingTraits<std::pair<std::string, DebugMapObject::SymbolMapping>>::
    mapping(IO &io, std::pair<std::string, DebugMapObject::SymbolMapping> &s) {
  io.mapRequired("sym", s.first);
  io.mapOptional("objAddr", s.second.ObjectAddress);
  io.mapRequired("binAddr", s.second.BinaryAddress);
  io.mapOptional("size", s.second.Size);
}

void MappingTraits<dsymutil::DebugMapObject>::mapping(
    IO &io, dsymutil::DebugMapObject &DMO) {
  MappingNormalization<YamlDMO, dsymutil::DebugMapObject> Norm(io, DMO);
  io.mapRequired("filename", Norm->Filename);
  io.mapOptional("timestamp", Norm->Timestamp);
  io.mapRequired("symbols", Norm->Entries);
}

void ScalarTraits<Triple>::output(const Triple &val, void *, raw_ostream &out) {
  out << val.str();
}

StringRef ScalarTraits<Triple>::input(StringRef scalar, void *, Triple &value) {
  value = Triple(scalar);
  return StringRef();
}

size_t
SequenceTraits<std::vector<std::unique_ptr<dsymutil::DebugMapObject>>>::size(
    IO &io, std::vector<std::unique_ptr<dsymutil::DebugMapObject>> &seq) {
  return seq.size();
}

dsymutil::DebugMapObject &
SequenceTraits<std::vector<std::unique_ptr<dsymutil::DebugMapObject>>>::element(
    IO &, std::vector<std::unique_ptr<dsymutil::DebugMapObject>> &seq,
    size_t index) {
  if (index >= seq.size()) {
    seq.resize(index + 1);
    seq[index].reset(new dsymutil::DebugMapObject);
  }
  return *seq[index];
}

void MappingTraits<dsymutil::DebugMap>::mapping(IO &io,
                                                dsymutil::DebugMap &DM) {
  io.mapRequired("triple", DM.BinaryTriple);
  io.mapOptional("binary-path", DM.BinaryPath);
  if (void *Ctxt = io.getContext())
    reinterpret_cast<YAMLContext *>(Ctxt)->BinaryTriple = DM.BinaryTriple;
  io.mapOptional("objects", DM.Objects);
}

void MappingTraits<std::unique_ptr<dsymutil::DebugMap>>::mapping(
    IO &io, std::unique_ptr<dsymutil::DebugMap> &DM) {
  if (!DM)
    DM.reset(new DebugMap());
  io.mapRequired("triple", DM->BinaryTriple);
  io.mapOptional("binary-path", DM->BinaryPath);
  if (void *Ctxt = io.getContext())
    reinterpret_cast<YAMLContext *>(Ctxt)->BinaryTriple = DM->BinaryTriple;
  io.mapOptional("objects", DM->Objects);
}

MappingTraits<dsymutil::DebugMapObject>::YamlDMO::YamlDMO(
    IO &io, dsymutil::DebugMapObject &Obj) {
  Filename = Obj.Filename;
  Timestamp = sys::toTimeT(Obj.getTimestamp());
  Entries.reserve(Obj.Symbols.size());
  for (auto &Entry : Obj.Symbols)
    Entries.push_back(std::make_pair(Entry.getKey(), Entry.getValue()));
}

dsymutil::DebugMapObject
MappingTraits<dsymutil::DebugMapObject>::YamlDMO::denormalize(IO &IO) {
  BinaryHolder BinHolder(/* Verbose =*/false);
  const auto &Ctxt = *reinterpret_cast<YAMLContext *>(IO.getContext());
  SmallString<80> Path(Ctxt.PrependPath);
  StringMap<uint64_t> SymbolAddresses;

  sys::path::append(Path, Filename);
  auto ErrOrObjectFiles = BinHolder.GetObjectFiles(Path);
  if (auto EC = ErrOrObjectFiles.getError()) {
    warn_ostream() << "Unable to open " << Path << " " << EC.message() << '\n';
  } else if (auto ErrOrObjectFile = BinHolder.Get(Ctxt.BinaryTriple)) {
    // Rewrite the object file symbol addresses in the debug map. The
    // YAML input is mainly used to test llvm-dsymutil without
    // requiring binaries checked-in. If we generate the object files
    // during the test, we can't hard-code the symbols addresses, so
    // look them up here and rewrite them.
    for (const auto &Sym : ErrOrObjectFile->symbols()) {
      uint64_t Address = Sym.getValue();
      Expected<StringRef> Name = Sym.getName();
      if (!Name ||
          (Sym.getFlags() & (SymbolRef::SF_Absolute | SymbolRef::SF_Common))) {
        // TODO: Actually report errors helpfully.
        if (!Name)
          consumeError(Name.takeError());
        continue;
      }
      SymbolAddresses[*Name] = Address;
    }
  }

  dsymutil::DebugMapObject Res(Path, sys::toTimePoint(Timestamp), MachO::N_OSO);
  for (auto &Entry : Entries) {
    auto &Mapping = Entry.second;
    Optional<uint64_t> ObjAddress;
    if (Mapping.ObjectAddress)
      ObjAddress = *Mapping.ObjectAddress;
    auto AddressIt = SymbolAddresses.find(Entry.first);
    if (AddressIt != SymbolAddresses.end())
      ObjAddress = AddressIt->getValue();
    Res.addSymbol(Entry.first, ObjAddress, Mapping.BinaryAddress, Mapping.Size);
  }
  return Res;
}

} // end namespace yaml
} // end namespace llvm
