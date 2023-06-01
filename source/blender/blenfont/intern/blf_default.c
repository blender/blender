/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
/* Keep in sync with `UI_DEFAULT_TEXT_POINTS` */
static float global_font_size = 11.0f;

void BLF_default_size(float size)
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

  BLF_size(global_font_default, global_font_size * UI_SCALE_FAC);

  return global_font_default;
}

void BLF_draw_default(float x, float y, float z, const char *str, const size_t str_len)
{
  ASSERT_DEFAULT_SET;
  BLF_size(global_font_default, global_font_size * UI_SCALE_FAC);
  BLF_position(global_font_default, x, y, z);
  BLF_draw(global_font_default, str, str_len);
}
