// Minimal SHA-256 (stdlib only). Added for the Xbox store: computing a UWP
// PackageFamilyName requires hashing the package Publisher string. Kept generic.

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ss::crypto {

// Raw 32-byte SHA-256 digest of `data`.
std::array<uint8_t, 32> sha256(const std::string& data);

}  // namespace ss::crypto
