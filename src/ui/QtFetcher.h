// Installs a QNetworkAccessManager-backed HTTP fetcher into ss::http, so core/'s
// cover + Web-API code does network I/O without core depending on Qt. Synchronous
// (core calls it from worker threads), with a short timeout — matches urllib use.

#pragma once

namespace ss::ui {

// Call once at startup, before any cover/Web-API resolution.
void installQtFetcher();

}  // namespace ss::ui
