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

namespace ss::covers {

// Raw image bytes of real Steam art for `appid`, or nullopt if nothing exists.
// allow_network=false uses only on-disk sources (offline / tests).
std::optional<std::string> coverBytes(int64_t appid, bool allowNetwork = true);

}  // namespace ss::covers
