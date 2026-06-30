#include "model.h"

namespace ss {

const char* storeName(Store s) {
    switch (s) {
        case Store::Steam: return "Steam";
        case Store::Epic: return "Epic";
        case Store::Gog: return "GOG";
        case Store::Xbox: return "Xbox";
    }
    return "Unknown";
}

}  // namespace ss
