/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_utils.hh"

#include "BLI_array.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/unicodeUtils.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

namespace blender::io::usd {

std::string make_safe_name(const StringRef name, bool allow_unicode)
{
  if (name.is_empty()) {
    return "_";
  }

  /* Create temporary buffer with exact amount of space required. */
  const bool has_leading_digit = std::isdigit(name[0]);
  Array<char, 64> storage(name.size() + (has_leading_digit ? 1 : 0));
  MutableSpan<char> buf(storage);

  /* Insert a leading '_' to account for names starting with digits. */
  size_t offset = 0;
  bool first = true;
  if (has_leading_digit) {
    buf[0] = '_';
    offset = 1;
    first = false;
  }

  if (!allow_unicode) {
    buf.take_back(name.size()).copy_from(name);
    offset += name.size();
    return pxr::TfMakeValidIdentifier({buf.data(), offset});
  }

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

  return {buf.data(), offset};
}

pxr::SdfPath get_unique_path(pxr::UsdStageRefPtr stage, const std::string &path)
{
  std::string unique_path = path;
  int suffix = 2;
  while (stage->GetPrimAtPath(pxr::SdfPath(unique_path)).IsValid()) {
    unique_path = path + std::to_string(suffix++);
  }

  return pxr::SdfPath(unique_path);
}

}  // namespace blender::io::usd
