// Cover art for the non-Steam stores (Steam art lives in covers.h). Sources:
//
//   * Xbox — the game ships its own logo PNG on disk (ShellVisuals in
//     MicrosoftGame.config); `coverHint` is its absolute path. Pure local read.
//   * Epic — the launcher keeps a catalog cache (Data\Catalog\catcache.bin,
//     base64-wrapped JSON) whose entries carry the store's keyImages URLs;
//     `coverHint` is the game's CatalogItemId. Local parse + one network fetch.
//   * GOG — the public products API (api.gog.com) serves box art by product id
//     (= launchId). Network only.
//
// Same caching discipline as Steam covers: only NETWORK results are disk-cached
// (keyed by `cacheId`, the UI's stable synthetic id), local art is re-read fresh.

#pragma once

#include "model.h"

#include <cstdint>
#include <optional>
#include <string>

namespace ss::store_covers {

// Raw image bytes for a non-Steam game, or nullopt. `cacheId` keys the disk cache
// (use the UI's stable synthetic id); allowNetwork=false = on-disk sources only.
std::optional<std::string> coverBytes(Store store, const std::string& launchId,
                                      const std::string& coverHint, int64_t cacheId,
                                      bool allowNetwork = true);

// Wide/hero art variant (the CINEMA hero banner), cached as <cacheId>_hero.jpg:
//   * Xbox — display catalog TitledHeroArt / SuperHeroArt (real store banners).
//   * Epic — catcache keyImages DieselGameBox (landscape).
//   * GOG — v2 games API backgroundImage (v1 background fallback).
// Network-only sources except the disk cache; nullopt when offline/absent.
std::optional<std::string> heroBytes(Store store, const std::string& launchId,
                                     const std::string& coverHint, int64_t cacheId,
                                     bool allowNetwork = true);

// ---- exposed for tests / ssdiag ---------------------------------------------
// Resolve `catalogItemId`'s cover URL from the launcher's catcache.bin on disk
// ("" = cache missing/unreadable or no entry). The local half of the Epic path.
std::string epicCoverUrl(const std::string& catalogItemId);
// Decode standard base64 (the catcache.bin wrapper); nullopt on bad input.
std::optional<std::string> base64Decode(const std::string& in);
// Pick the best cover URL for `catalogItemId` out of catcache.bin's JSON text
// (prefers the portrait DieselGameBoxTall, then DieselGameBox); "" if absent.
std::string epicCoverUrlFromCatalog(const std::string& catalogJson,
                                    const std::string& catalogItemId);
// Same, for the wide hero art (DieselGameBox first, then the tall box).
std::string epicHeroUrlFromCatalog(const std::string& catalogJson,
                                   const std::string& catalogItemId);

}  // namespace ss::store_covers
