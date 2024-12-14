/* SPDX-FileCopyrightText: 2025 Blender Authors
 * SPDX-License-Identifier: BSL-1.0
 *
 * Adapted from boost::locale */

#include <locale>
#include <string>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::locale {

/* Info about a locale. */
struct Info {
  Info(const StringRef locale_full_name);

  std::string language = "C";
  std::string script;
  std::string country;
  std::string variant;

  std::string to_full_name() const;

#if defined(__APPLE__) && !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)
  static std::string macos_user_locale();
#endif
};

/* Message facet to install into std::locale for translation. */
class MessageFacet : public std::locale::facet {
 public:
  static std::locale::id id;
  static std::locale install(const std::locale &locale,
                             const Info &info,
                             const Vector<std::string> &domains, /* Application names. */
                             const Vector<std::string> &paths);  /* Search paths for .mo files. */

  virtual const char *translate(const int domain,
                                const StringRef context,
                                const StringRef key) const = 0;
};

}  // namespace blender::locale
