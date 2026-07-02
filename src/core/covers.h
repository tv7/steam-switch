// Layered Steam cover-art resolver. Port of core/covers.py.
//
// Order: Steam local librarycache portrait (exact filename only) -> disk cache ->
// legacy flat CDN -> store appdetails API (live hashed URL). Only NETWORK hits are
// disk-cached, so the rate-limited API runs at most once per game and a stale copy
// never pins over Steam's fresh local art. (cover-art-resolution memory.)

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ss::covers {

// Raw image bytes of real Steam art for `appid`, or nullopt if nothing exists.
// allow_network=false uses only on-disk sources (offline / tests).
std::optional<std::string> coverBytes(int64_t appid, bool allowNetwork = true);

// Wide/hero art variant (the CINEMA hero banner): Steam local
// librarycache library_hero.jpg -> disk cache (<appid>_hero.jpg) -> flat CDN
// library_hero.jpg -> appdetails header_image. Same discipline as coverBytes:
// only network hits are disk-cached.
std::optional<std::string> heroBytes(int64_t appid, bool allowNetwork = true);

// Pixel dimensions (w,h) parsed from raw PNG/JPEG bytes (exposed for tests; used
// by the dimension-checked librarycache fallback for hidden demo/beta appids).
std::optional<std::pair<int, int>> imageSize(const std::string& bytes);

// The on-disk cache of network-fetched art (data/covers/, all stores + hero
// variants). Size in bytes / wipe it (returns files removed). Safe to clear any
// time — every entry re-resolves from its network source on demand.
long long cacheSizeBytes();
int clearCache();

}  // namespace ss::covers
