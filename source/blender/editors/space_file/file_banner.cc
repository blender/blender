/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "DNA_space_types.h"

#include "BLI_function_ref.hh"

#include "file_banner.hh"
#include "file_intern.hh"

namespace blender {

static bool file_banners_any_visible(const SpaceFile &sfile)
{
  for (BannerType &banner : banner_types) {
    if (banner.poll && banner.poll(sfile)) {
      return true;
    }
  }
  return false;
}

void file_banners_update(const SpaceFile &sfile)
{
  const bool any_visible = file_banners_any_visible(sfile);

  if (sfile.runtime->banners_state.any_visible != any_visible) {
    sfile.layout->dirty = true;
    sfile.runtime->banners_state.any_visible = any_visible;
  }
}

void file_banners_for_first_visible(const SpaceFile &sfile,
                                    FunctionRef<void(const BannerType &)> fn)
{
  for (BannerType &banner : banner_types) {
    if (banner.poll && banner.poll(sfile)) {
      BLI_assert(sfile.runtime->banners_state.any_visible == true);
      fn(banner);
      return;
    }
  }

  BLI_assert(sfile.runtime->banners_state.any_visible == false);
}

}  // namespace blender
