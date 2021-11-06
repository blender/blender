/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup blf
 *
 * Default API, that uses Blender's user preferences for the default size.
 */

#include "DNA_userdef_types.h"

#include "BLI_assert.h"

#include "BLF_api.h"

#include "blf_internal.h"

/* call BLF_default_set first! */
#define ASSERT_DEFAULT_SET BLI_assert(global_font_default != -1)

/* Default size and dpi, for BLF_draw_default. */
static int global_font_default = -1;
static int global_font_dpi = 72;
/* Keep in sync with `UI_style_get()->widgetlabel.points` */
static int global_font_size = 11;

void BLF_default_dpi(int dpi)
{
  global_font_dpi = dpi;
}

void BLF_default_size(int size)
{
  global_font_size = size;
}

void BLF_default_set(int fontid)
{
  if ((fontid == -1) || blf_font_id_is_valid(fontid)) {
    global_font_default = fontid;
  }
}

int BLF_default(void)
{
  ASSERT_DEFAULT_SET;
  return global_font_default;
}

int BLF_set_default(void)
{
  ASSERT_DEFAULT_SET;

  BLF_size(global_font_default, global_font_size, global_font_dpi);

  return global_font_default;
}

void BLF_draw_default(float x, float y, float z, const char *str, const size_t str_len)
{
  ASSERT_DEFAULT_SET;
  BLF_size(global_font_default, global_font_size, global_font_dpi);
  BLF_position(global_font_default, x, y, z);
  BLF_draw(global_font_default, str, str_len);
}
