/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_movieclip.h
 *  \ingroup editors
 */

#ifndef ED_MOVIECLIP_H
#define ED_MOVIECLIP_H

struct bContext;
struct ImBuf;
struct Main;
struct MovieClip;
struct SpaceClip;

/* clip_editor.c */
void ED_space_clip_set(struct bContext *C, struct SpaceClip *sc, struct MovieClip *clip);
struct MovieClip *ED_space_clip(struct SpaceClip *sc);
void ED_space_clip_size(struct SpaceClip *sc, int *width, int *height);

struct ImBuf *ED_space_clip_acquire_buffer(struct SpaceClip *sc);

void ED_clip_update_frame(const struct Main *mainp, int cfra);

#endif /* ED_TEXT_H */

