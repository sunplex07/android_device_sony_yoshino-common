// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>

namespace modem_guard {
enum class Decision { Unreachable, MissingImage, Temporary, Match, Repair };
inline bool image_name(const std::string& s) {
    if (s.size() < 18 || s.size() > 240 || s.compare(0, 9, "amss_fsg_") ||
        s.compare(s.size() - 8, 8, "_tar.mbn")) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    return s.find("..") == std::string::npos;
}
// A failed read/hash is NOT a mismatch. Caller must pass io_ok=false.
inline Decision decide(bool ta_ready, const std::string& name, bool image_exists,
                       bool io_ok, bool prefix_equal, int status) {
    (void)status; // Physical mismatch matters even if the cached status is 0.
    if (!ta_ready || !io_ok) return Decision::Unreachable;
    if (name == "temporary_modem") return Decision::Temporary;
    if (!image_name(name) || !image_exists) return Decision::MissingImage;
    return prefix_equal ? Decision::Match : Decision::Repair;
}
}
