// Fuzzes the WPILOG parser: wpi::log::DataLogReader header validation, record
// iteration, control-record decoding and every typed payload accessor.
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "wpi/datalog/DataLogReader.hpp"
#include "wpi/util/MemoryBuffer.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  wpi::log::DataLogReader reader{wpi::util::MemoryBuffer::GetMemBufferCopy(
      std::span<const uint8_t>{data, size})};
  if (!reader.IsValid()) {
    return -1;
  }
  (void)reader.GetVersion();
  (void)reader.GetExtraHeader();
  for (const wpi::log::DataLogRecord& record : reader) {
    (void)record.GetEntry();
    (void)record.GetTimestamp();
    (void)record.GetSize();
    (void)record.GetRaw();
    if (record.IsControl()) {
      wpi::log::StartRecordData start;
      int finish;
      wpi::log::MetadataRecordData meta;
      if (record.IsStart()) {
        (void)record.GetStartData(&start);
      } else if (record.IsFinish()) {
        (void)record.GetFinishEntry(&finish);
      } else if (record.IsSetMetadata()) {
        (void)record.GetSetMetadataData(&meta);
      }
      continue;
    }
    bool b;
    int64_t i;
    float f;
    double d;
    std::string_view s;
    std::vector<int> ba;
    std::vector<int64_t> ia;
    std::vector<float> fa;
    std::vector<double> da;
    std::vector<std::string_view> sa;
    (void)record.GetBoolean(&b);
    (void)record.GetInteger(&i);
    (void)record.GetFloat(&f);
    (void)record.GetDouble(&d);
    (void)record.GetString(&s);
    (void)record.GetBooleanArray(&ba);
    (void)record.GetIntegerArray(&ia);
    (void)record.GetFloatArray(&fa);
    (void)record.GetDoubleArray(&da);
    (void)record.GetStringArray(&sa);
  }
  return 0;
}
