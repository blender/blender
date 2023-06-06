/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#ifdef WITH_IO_GPENCIL

#  include "DNA_space_types.h"

#  include "BKE_context.h"
#  include "BKE_screen.h"

#  include "WM_api.h"

#  include "io_gpencil.hh"

ARegion *get_invoke_region(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return nullptr;
  }
  ScrArea *area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
  if (area == nullptr) {
    return nullptr;
  }

  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  return region;
}

View3D *get_invoke_view3d(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  if (screen == nullptr) {
    return nullptr;
  }
  ScrArea *area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
  if (area == nullptr) {
    return nullptr;
  }
  if (area != nullptr) {
    return static_cast<View3D *>(area->spacedata.first);
  }

  return nullptr;
}

#endif /* WITH_IO_GPENCIL */
