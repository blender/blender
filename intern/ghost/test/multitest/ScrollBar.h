/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

typedef struct _ScrollBar ScrollBar;

/***/

ScrollBar *scrollbar_new(int inset, int minthumb);

int scrollbar_is_scrolling(ScrollBar *sb);
int scrollbar_contains_pt(ScrollBar *sb, int pt[2]);

void scrollbar_start_scrolling(ScrollBar *sb, int yco);
void scrollbar_keep_scrolling(ScrollBar *sb, int yco);
void scrollbar_stop_scrolling(ScrollBar *sb);

void scrollbar_set_thumbpct(ScrollBar *sb, float pct);
void scrollbar_set_thumbpos(ScrollBar *sb, float pos);
void scrollbar_set_rect(ScrollBar *sb, int rect[2][2]);

float scrollbar_get_thumbpct(ScrollBar *sb);
float scrollbar_get_thumbpos(ScrollBar *sb);
void scrollbar_get_rect(ScrollBar *sb, int rect_r[2][2]);

void scrollbar_get_thumb(ScrollBar *sb, int thumb_r[2][2]);

void scrollbar_free(ScrollBar *sb);
