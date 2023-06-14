/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include <math.h>

#include "MEM_guardedalloc.h"

#include "Basic.h"
#include "ScrollBar.h"

struct _ScrollBar {
  int rect[2][2];
  float thumbpos, thumbpct;

  int inset;
  int minthumb;

  int scrolling;
  float scrolloffs;
};

static int scrollbar_get_thumbH(ScrollBar *sb)
{
  int scrollable_h = rect_height(sb->rect) - 2 * sb->inset;

  return clamp_i(sb->thumbpct * scrollable_h, sb->minthumb, scrollable_h);
}

static int scrollbar_get_thumbableH(ScrollBar *sb)
{
  int scrollable_h = rect_height(sb->rect) - 2 * sb->inset;
  int thumb_h = scrollbar_get_thumbH(sb);

  return scrollable_h - thumb_h;
}

static float scrollbar_co_to_pos(ScrollBar *sb, int yco)
{
  int thumb_h = scrollbar_get_thumbH(sb);
  int thumbable_h = scrollbar_get_thumbableH(sb);
  int thumbable_y = (sb->rect[0][1] + sb->inset) + thumb_h / 2;

  return (float)(yco - thumbable_y) / thumbable_h;
}

/**/

ScrollBar *scrollbar_new(int inset, int minthumb)
{
  ScrollBar *sb = MEM_callocN(sizeof(*sb), "scrollbar_new");
  sb->inset = inset;
  sb->minthumb = minthumb;

  return sb;
}

void scrollbar_get_thumb(ScrollBar *sb, int thumb_r[2][2])
{
  int thumb_h = scrollbar_get_thumbH(sb);
  int thumbable_h = scrollbar_get_thumbableH(sb);

  thumb_r[0][0] = sb->rect[0][0] + sb->inset;
  thumb_r[1][0] = sb->rect[1][0] - sb->inset;

  thumb_r[0][1] = sb->rect[0][1] + sb->inset + sb->thumbpos * thumbable_h;
  thumb_r[1][1] = thumb_r[0][1] + thumb_h;
}

int scrollbar_is_scrolling(ScrollBar *sb)
{
  return sb->scrolling;
}
int scrollbar_contains_pt(ScrollBar *sb, int pt[2])
{
  return rect_contains_pt(sb->rect, pt);
}

void scrollbar_start_scrolling(ScrollBar *sb, int yco)
{
  int thumb_h_2 = scrollbar_get_thumbH(sb) / 2;
  int thumbable_h = scrollbar_get_thumbableH(sb);
  float npos = scrollbar_co_to_pos(sb, yco);

  sb->scrolloffs = sb->thumbpos - npos;
  if (fabs(sb->scrolloffs) >= (float)thumb_h_2 / thumbable_h) {
    sb->scrolloffs = 0.0;
  }

  sb->scrolling = 1;
  sb->thumbpos = clamp_f(npos + sb->scrolloffs, 0.0, 1.0);
}
void scrollbar_keep_scrolling(ScrollBar *sb, int yco)
{
  float npos = scrollbar_co_to_pos(sb, yco);

  sb->thumbpos = clamp_f(npos + sb->scrolloffs, 0.0, 1.0);
}
void scrollbar_stop_scrolling(ScrollBar *sb)
{
  sb->scrolling = 0;
  sb->scrolloffs = 0.0;
}

void scrollbar_set_thumbpct(ScrollBar *sb, float pct)
{
  sb->thumbpct = pct;
}
void scrollbar_set_thumbpos(ScrollBar *sb, float pos)
{
  sb->thumbpos = clamp_f(pos, 0.0, 1.0);
}
void scrollbar_set_rect(ScrollBar *sb, int rect[2][2])
{
  rect_copy(sb->rect, rect);
}

float scrollbar_get_thumbpct(ScrollBar *sb)
{
  return sb->thumbpct;
}
float scrollbar_get_thumbpos(ScrollBar *sb)
{
  return sb->thumbpos;
}
void scrollbar_get_rect(ScrollBar *sb, int rect_r[2][2])
{
  rect_copy(rect_r, sb->rect);
}

void scrollbar_free(ScrollBar *sb)
{
  MEM_freeN(sb);
}
