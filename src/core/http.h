// Tiny HTTP indirection. core/ must do network I/O (cover appdetails, Steam
// Web-API) but must stay Qt-free and headless-testable. So instead of linking a
// transport into core, the host installs a fetcher:
//
//   * the Qt UI installs a QNetworkAccessManager-backed fetcher at startup;
//   * tests install a stub (or none — then get() returns nullopt, i.e. offline).
//
// This is the dependency-inversion replacement for Python's urllib calls inside
// core/covers.py and core/accounts.py.

#pragma once

#include <functional>
#include <optional>
#include <string>

namespace ss::http {

// Returns the response body bytes on HTTP 200, or nullopt on any failure.
using Fetcher = std::function<std::optional<std::string>(const std::string& url)>;

void setFetcher(Fetcher f);

// GET via the installed fetcher; nullopt if none is installed (offline) or on error.
std::optional<std::string> get(const std::string& url);

}  // namespace ss::http
