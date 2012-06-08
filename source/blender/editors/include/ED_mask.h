/*
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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_mask.h
 *  \ingroup editors
 */

#ifndef __ED_MASK_H__
#define __ED_MASK_H__

struct wmKeyConfig;
struct MaskLayer;
struct MaskLayerShape;

/* mask_editor.c */
void ED_operatortypes_mask(void);
void ED_keymap_mask(struct wmKeyConfig *keyconf);
void ED_operatormacros_mask(void);

/* mask_draw.c */
void ED_mask_draw(const bContext *C, const char draw_flag, const char draw_type);

/* mask_shapekey.c */
void ED_mask_layer_shape_auto_key(struct MaskLayer *masklay, const int frame);
int ED_mask_layer_shape_auto_key_all(struct Mask *mask, const int frame);
int ED_mask_layer_shape_auto_key_select(struct Mask *mask, const int frame);

/* ----------- Mask AnimEdit API ------------------ */
short masklayer_frames_looper(struct MaskLayer *masklay, struct Scene *scene,
                              short (*masklay_shape_cb)(struct MaskLayerShape *, struct Scene *));
void masklayer_make_cfra_list(struct MaskLayer *masklay, ListBase *elems, short onlysel);

short is_masklayer_frame_selected(struct MaskLayer *masklay);
void set_masklayer_frame_selection(struct MaskLayer *masklay, short mode);
void select_mask_frames(struct MaskLayer *masklay, short select_mode);
void select_mask_frame(struct MaskLayer *masklay, int selx, short select_mode);
void borderselect_masklayer_frames(struct MaskLayer *masklay, float min, float max, short select_mode);

void delete_masklayer_frames(struct MaskLayer *masklay);
void duplicate_masklayer_frames(struct MaskLayer *masklay);

//void free_gpcopybuf(void);
//void copy_gpdata(void);
//void paste_gpdata(void);

void snap_masklayer_frames(struct MaskLayer *masklay, short mode);
void mirror_masklayer_frames(struct MaskLayer *masklay, short mode);

#endif /* __ED_MASK_H__ */
