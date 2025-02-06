/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: BSL-1.0 */

/** \file
 * \ingroup blt
 *
 * Adapted from `boost::locale`.
 */

#include <optional>
#include <string>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::locale {

void init(const StringRef locale_full_name,   /* Local name. */
          const Vector<std::string> &domains, /* Application names. */
          const Vector<std::string> &paths);  /* Search paths for .mo files. */
void free();

std::optional<StringRefNull> translate(int domain, StringRef context, StringRef key);
const char *full_name();

#if defined(__APPLE__) && !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)
std::string macos_user_locale();
#endif

}  // namespace blender::locale
