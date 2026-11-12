// Fuzzes wpinet's HTTP handling: the llhttp-based HttpParser (request/response/both,
// fed in chunks), URI escaping, HttpPath/HttpLocation, ada URL parsing, header and
// multipart scanning. The first byte selects the code path, the second the chunk size.
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "wpi/net/HttpParser.hpp"
#include "wpi/net/HttpUtil.hpp"
#include "wpi/util/SmallString.hpp"
#include "wpi/util/raw_istream.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2) {
    return -1;
  }
  uint8_t mode = data[0] % 5;
  size_t chunk = data[1];
  std::string_view in{reinterpret_cast<const char*>(data) + 2, size - 2};

  if (mode <= 2) {
    using Type = wpi::net::HttpParser::Type;
    wpi::net::HttpParser parser{mode == 0   ? Type::REQUEST
                                : mode == 1 ? Type::RESPONSE
                                            : Type::BOTH};
    size_t counted = 0;
    parser.url.connect([&](std::string_view s) { counted += s.size(); });
    parser.header.connect(
        [&](std::string_view k, std::string_view v) { counted += k.size() + v.size(); });
    parser.body.connect([&](std::string_view s) { counted += s.size(); });
    parser.status.connect([&](std::string_view s) { counted += s.size(); });
    if (chunk == 0) {
      chunk = in.size();
    }
    while (!in.empty()) {
      size_t n = in.size() < chunk ? in.size() : chunk;
      parser.Execute(in.substr(0, n));
      in.remove_prefix(n);
      // Real callers stop at the first error or upgrade: llhttp keeps a stale
      // error_pos once the parser is out of the OK state.
      if (parser.GetError() != HPE_OK) {
        break;
      }
    }
    parser.Execute({});
    (void)parser.GetError();
    (void)parser.GetUrl();
    (void)parser.GetMajor();
    (void)parser.GetMinor();
    return 0;
  }

  if (mode == 3) {
    wpi::util::SmallString<128> buf;
    bool error = false;
    (void)wpi::net::UnescapeURI(in, buf, &error);
    buf.clear();
    (void)wpi::net::EscapeURI(in, buf, chunk & 1);
    buf.clear();
    (void)wpi::net::EscapeHTML(in, buf);
    wpi::net::HttpPath path{in};
    for (size_t i = 0; i < path.size(); ++i) {
      (void)path[i];
    }
    (void)path.drop_front(chunk % 4);
    bool lerr = false;
    std::string lmsg;
    wpi::net::HttpLocation loc{in, &lerr, &lmsg};
    (void)loc.path;
    (void)wpi::net::ParseUrl(in);
    return 0;
  }

  wpi::util::raw_mem_istream is{in.data(), in.size()};
  wpi::util::SmallString<64> contentType, contentLength;
  (void)wpi::net::ParseHttpHeaders(is, &contentType, &contentLength);
  std::string saved;
  (void)wpi::net::FindMultipartBoundary(is, "boundary", &saved);
  wpi::net::HttpMultipartScanner scanner{"boundary", true};
  std::string_view rest = in;
  while (!rest.empty()) {
    size_t n = rest.size() < (chunk | 1) ? rest.size() : (chunk | 1);
    scanner.Execute(rest.substr(0, n));
    rest.remove_prefix(n);
  }
  return 0;
}
