/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_utils.hh"

#include "BLI_string_utf8.h"

#include <pxr/base/tf/stringUtils.h>
#if PXR_VERSION >= 2403
#  include <pxr/base/tf/unicodeUtils.h>
#endif

namespace blender::io::usd {

std::string make_safe_name(const std::string &name, [[maybe_unused]] bool allow_unicode)
{
#if PXR_VERSION >= 2403
  if (!allow_unicode) {
    return pxr::TfMakeValidIdentifier(name);
  }

  if (name.empty()) {
    return "_";
  }

  std::string buf;
  buf.resize(name.size());  // We won't be exceeding the size of the incoming string

  bool first = true;
  size_t offset = 0;
  for (auto cp : pxr::TfUtf8CodePointView{name}) {
    constexpr pxr::TfUtf8CodePoint cp_underscore = pxr::TfUtf8CodePointFromAscii('_');
    const bool cp_allowed = first ? (cp == cp_underscore || pxr::TfIsUtf8CodePointXidStart(cp)) :
                                    pxr::TfIsUtf8CodePointXidContinue(cp);
    if (!cp_allowed) {
      offset += BLI_str_utf8_from_unicode(uint32_t('_'), buf.data() + offset, buf.size() - offset);
    }
    else {
      offset += BLI_str_utf8_from_unicode(cp.AsUInt32(), buf.data() + offset, buf.size() - offset);
    }

    first = false;
  }

  return buf;
#else
  return pxr::TfMakeValidIdentifier(name);
#endif
}

}  // namespace blender::io::usd
