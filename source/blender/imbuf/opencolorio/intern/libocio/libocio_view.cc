/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_view.hh"

namespace blender::ocio {

int LibOCIOView::max_nits() const
{
  /* This is current based on heuristics using the view transform name, as OpenColorIO has no
   * standard way to specify this. Recognizes "- SDR" and "- HDR XXXX nits" as used by the
   * Blender and ACES configs. */
  const StringRefNull n = name_;
  if (n.endswith(" - SDR")) {
    return 100;
  }
  const int64_t nits_pos = n.find(" nits");
  if (nits_pos == StringRef::not_found) {
    return 0;
  }
  const int64_t num_end = nits_pos;
  int64_t num_start = num_end;
  while (num_start > 0 && n[num_start - 1] >= '0' && n[num_start - 1] <= '9') {
    num_start--;
  }
  int value = 0;
  for (int64_t i = num_start; i < num_end; i++) {
    value = value * 10 + (n[i] - '0');
  }
  return value > 0 ? value : 0;
}

}  // namespace blender::ocio
