/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_function_ref.hh"

namespace blender {

struct SpaceFile;
namespace ui {
struct Layout;
}

struct BannerType {
  bool (*poll)(const SpaceFile &);
  void (*layout)(const SpaceFile &, ui::Layout &);
};

/* To be called before updating the layout and drawing. */
void file_banners_update(const SpaceFile &sfile);
void file_banners_for_first_visible(const SpaceFile &sfile,
                                    FunctionRef<void(const BannerType &)> fn);

extern BannerType remote_libraries_online_access_required_banner;

inline Array<BannerType> banner_types = {
    remote_libraries_online_access_required_banner,
};

}  // namespace blender
