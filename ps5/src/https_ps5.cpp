/*
 * Wind Waker HD for the PlayStation 5 - downloads: there are none.
 *
 * SPDX-License-Identifier: MIT
 */
#include "platform/https.h"

#include <stdexcept>

namespace host {
void download_https(const std::string&, const std::filesystem::path&, uint64_t) {
    throw std::runtime_error("Downloads are not available on the PlayStation 5");
}
}
