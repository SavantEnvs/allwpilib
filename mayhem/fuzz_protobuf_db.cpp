// Fuzzes wpi::util::ProtobufMessageDatabase: FileDescriptorProto parsing + descriptor
// pool registration (upb), then decoding a payload against message "M".
// Input: [u8 n][n bytes FileDescriptorProto][payload].
#include <stddef.h>
#include <stdint.h>

#include <span>

#include "wpi/util/protobuf/ProtobufMessageDatabase.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1 || size - 1 < data[0]) {
    return -1;
  }
  size_t n = data[0];
  std::span<const uint8_t> file{data + 1, n};
  std::span<const uint8_t> payload{data + 1 + n, size - 1 - n};
  wpi::util::ProtobufMessageDatabase db;
  if (!db.Add(file)) {
    return -1;
  }
  if (const auto* msg = db.Find("M")) {
    (void)db.Decode(msg, payload);
  }
  return 0;
}
