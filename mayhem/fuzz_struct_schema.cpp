// Fuzzes the WPILib raw-struct schema parser and dynamic struct reader.
// Input: NUL-separated segments. Every segment but the last is a schema registered
// (in order) as struct "A", "B", "C", "D"; the last segment is the struct payload,
// read through every field of every valid descriptor.
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "wpi/util/struct/DynamicStruct.hpp"

using namespace wpi::util;

static void ReadStruct(const DynamicStruct& ds, int depth) {
  for (const auto& field : ds.GetDescriptor()->GetFields()) {
    size_t n = field.GetArraySize();
    switch (field.GetType()) {
      case StructFieldType::BOOL:
        for (size_t i = 0; i < n; ++i) (void)ds.GetBoolField(&field, i);
        break;
      case StructFieldType::CHAR:
        (void)ds.GetStringField(&field);
        break;
      case StructFieldType::INT8:
      case StructFieldType::INT16:
      case StructFieldType::INT32:
      case StructFieldType::INT64:
        for (size_t i = 0; i < n; ++i) (void)ds.GetIntField(&field, i);
        break;
      case StructFieldType::UINT8:
      case StructFieldType::UINT16:
      case StructFieldType::UINT32:
      case StructFieldType::UINT64:
        for (size_t i = 0; i < n; ++i) (void)ds.GetUintField(&field, i);
        break;
      case StructFieldType::FLOAT:
        for (size_t i = 0; i < n; ++i) (void)ds.GetFloatField(&field, i);
        break;
      case StructFieldType::DOUBLE:
        for (size_t i = 0; i < n; ++i) (void)ds.GetDoubleField(&field, i);
        break;
      case StructFieldType::STRUCT:
        if (depth < 16) {
          for (size_t i = 0; i < n; ++i) {
            ReadStruct(ds.GetStructField(&field, i), depth + 1);
          }
        }
        break;
    }
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view in{reinterpret_cast<const char*>(data), size};
  std::vector<std::string_view> segs;
  size_t pos = 0;
  while (segs.size() < 5) {
    size_t nul = in.find('\0', pos);
    if (nul == std::string_view::npos) {
      segs.push_back(in.substr(pos));
      break;
    }
    segs.push_back(in.substr(pos, nul - pos));
    pos = nul + 1;
  }
  if (segs.size() < 2) {
    return -1;
  }
  std::string_view payload = segs.back();
  segs.pop_back();

  StructDescriptorDatabase db;
  static const char* const kNames[] = {"A", "B", "C", "D"};
  std::vector<const StructDescriptor*> descs;
  for (size_t i = 0; i < segs.size() && i < 4; ++i) {
    std::string err;
    descs.push_back(db.Add(kNames[i], segs[i], &err));
  }
  for (const StructDescriptor* desc : descs) {
    if (!desc || !desc->IsValid()) {
      continue;
    }
    // DynamicStruct requires at least GetSize() bytes of data.
    if (payload.size() < desc->GetSize()) {
      continue;
    }
    ReadStruct(
        DynamicStruct{desc, std::span<const uint8_t>{
                                reinterpret_cast<const uint8_t*>(payload.data()),
                                payload.size()}},
        0);
  }
  return 0;
}
