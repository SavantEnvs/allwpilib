// Fuzzes wpi::util::json: parsing, serialization (compact and pretty) and typed traversal.
#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "wpi/util/json.hpp"

using wpi::util::json;

static void Walk(const json& j, int depth) {
  if (depth > 64) {
    return;
  }
  switch (j.type()) {
    case json::Type::Bool:
      (void)j.get_bool();
      break;
    case json::Type::Int:
      (void)j.get_int();
      (void)j.get_number();
      break;
    case json::Type::Uint:
      (void)j.get_uint();
      (void)j.get_number();
      break;
    case json::Type::Float:
      (void)j.get_float();
      (void)j.get_number();
      break;
    case json::Type::Double:
      (void)j.get_double();
      (void)j.get_number();
      break;
    case json::Type::String:
      (void)j.get_string();
      break;
    case json::Type::Array:
      for (const auto& e : j.get_array()) {
        Walk(e, depth + 1);
      }
      break;
    case json::Type::Object:
      for (const auto& kv : j.get_object()) {
        (void)j.contains(kv.first);
        Walk(kv.second, depth + 1);
      }
      break;
    default:
      break;
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = json::parse(
      std::string_view{reinterpret_cast<const char*>(data), size});
  if (!result) {
    return -1;
  }
  const json& j = *result;
  Walk(j, 0);
  (void)j.to_string();
  (void)j.to_string_pretty();
  return 0;
}
