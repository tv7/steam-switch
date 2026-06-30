#include "http.h"

namespace ss::http {

namespace {
Fetcher g_fetcher;
}

void setFetcher(Fetcher f) { g_fetcher = std::move(f); }

std::optional<std::string> get(const std::string& url) {
    if (!g_fetcher) return std::nullopt;
    return g_fetcher(url);
}

}  // namespace ss::http
