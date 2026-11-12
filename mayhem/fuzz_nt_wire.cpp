// Fuzzes the NetworkTables 4 wire-protocol decoders: JSON text frames (as seen by a
// server, i.e. client->server messages, and as seen by a client) and MessagePack binary
// value frames. The first input byte selects the decoder.
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "PubSubOptions.hpp"
#include "net/MessageHandler.hpp"
#include "net/WireDecoder.hpp"
#include "wpi/nt/ntcore_cpp.hpp"
#include "wpi/util/Logger.hpp"
#include "wpi/util/json.hpp"

namespace {

class ClientHandler : public wpi::nt::net::ClientMessageHandler {
 public:
  void ClientPublish(int, std::string_view, std::string_view,
                     const wpi::util::json& properties,
                     const wpi::nt::PubSubOptionsImpl&) override {
    (void)properties.to_string();
  }
  void ClientUnpublish(int) override {}
  void ClientSetProperties(std::string_view,
                           const wpi::util::json& update) override {
    (void)update.to_string();
  }
  void ClientSubscribe(int, std::span<const std::string>,
                       const wpi::nt::PubSubOptionsImpl&) override {}
  void ClientUnsubscribe(int) override {}
  void ClientSetValue(int, const wpi::nt::Value& value) override {
    (void)value.type();
    (void)value.last_change();
  }
};

class ServerHandler : public wpi::nt::net::ServerMessageHandler {
 public:
  int ServerAnnounce(std::string_view, int, std::string_view,
                     const wpi::util::json& properties,
                     std::optional<int>) override {
    (void)properties.to_string();
    return 0;
  }
  void ServerUnannounce(std::string_view, int) override {}
  void ServerPropertiesUpdate(std::string_view,
                              const wpi::util::json& update, bool) override {
    (void)update.to_string();
  }
  void ServerSetValue(int, const wpi::nt::Value& value) override {
    (void)value.type();
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) {
    return -1;
  }
  uint8_t mode = data[0] % 3;
  data++;
  size--;
  wpi::util::Logger logger{
      [](unsigned int, const char*, unsigned int, const char*) {}};
  if (mode == 0) {
    ClientHandler handler;
    (void)wpi::nt::net::WireDecodeText(
        std::string_view{reinterpret_cast<const char*>(data), size}, handler,
        logger);
  } else if (mode == 1) {
    ServerHandler handler;
    wpi::nt::net::WireDecodeText(
        std::string_view{reinterpret_cast<const char*>(data), size}, handler,
        logger);
  } else {
    std::span<const uint8_t> in{data, size};
    while (!in.empty()) {
      int id;
      wpi::nt::Value value;
      std::string error;
      if (!wpi::nt::net::WireDecodeBinary(&in, &id, &value, &error, 0)) {
        break;
      }
      (void)value.type();
    }
  }
  return 0;
}
