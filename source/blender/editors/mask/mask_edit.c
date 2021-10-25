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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_edit.c
 *  \ingroup edmask
 */


#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mask.h"  /* own include */
#include "ED_image.h"
#include "ED_object.h" /* ED_keymap_proportional_maskmode only */
#include "ED_clip.h"
#include "ED_sequencer.h"
#include "ED_transform.h"

#include "UI_view2d.h"

#include "RNA_access.h"

#include "mask_intern.h"  /* own include */

/********************** generic poll functions *********************/

int ED_maskedit_poll(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
				return ED_space_clip_maskedit_poll(C);
			case SPACE_SEQ:
				return ED_space_sequencer_maskedit_poll(C);
			case SPACE_IMAGE:
				return ED_space_image_maskedit_poll(C);
		}
	}
	return false;
}

int ED_maskedit_mask_poll(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
				return ED_space_clip_maskedit_mask_poll(C);
			case SPACE_SEQ:
				return ED_space_sequencer_maskedit_mask_poll(C);
			case SPACE_IMAGE:
				return ED_space_image_maskedit_mask_poll(C);
		}
	}
	return false;
}

/********************** registration *********************/

/* takes event->mval */
void ED_mask_mouse_pos(ScrArea *sa, ARegion *ar, const int mval[2], float co[2])
{
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				ED_clip_mouse_pos(sc, ar, mval, co);
				BKE_mask_coord_from_movieclip(sc->clip, &sc->user, co, co);
				break;
			}
			case SPACE_SEQ:
			{
				UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &co[0], &co[1]);
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				ED_image_mouse_pos(sima, ar, mval, co);
				BKE_mask_coord_from_image(sima->image, &sima->iuser, co, co);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				zero_v2(co);
				break;
		}
	}
	else {
		BLI_assert(0);
		zero_v2(co);
	}
}

/* input:  x/y   - mval space
 * output: xr/yr - mask point space */
void ED_mask_point_pos(ScrArea *sa, ARegion *ar, float x, float y, float *xr, float *yr)
{
	float co[2];

	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				ED_clip_point_stable_pos(sc, ar, x, y, &co[0], &co[1]);
				BKE_mask_coord_from_movieclip(sc->clip, &sc->user, co, co);
				break;
			}
			case SPACE_SEQ:
				zero_v2(co); /* MASKTODO */
				break;
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				ED_image_point_pos(sima, ar, x, y, &co[0], &co[1]);
				BKE_mask_coord_from_image(sima->image, &sima->iuser, co, co);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				zero_v2(co);
				break;
		}
	}
	else {
		BLI_assert(0);
		zero_v2(co);
	}

	*xr = co[0];
	*yr = co[1];
}

void ED_mask_point_pos__reverse(ScrArea *sa, ARegion *ar, float x, float y, float *xr, float *yr)
{
	float co[2];

	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				co[0] = x;
				co[1] = y;
				BKE_mask_coord_to_movieclip(sc->clip, &sc->user, co, co);
				ED_clip_point_stable_pos__reverse(sc, ar, co, co);
				break;
			}
			case SPACE_SEQ:
				zero_v2(co); /* MASKTODO */
				break;
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				co[0] = x;
				co[1] = y;
				BKE_mask_coord_to_image(sima->image, &sima->iuser, co, co);
				ED_image_point_pos__reverse(sima, ar, co, co);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				zero_v2(co);
				break;
		}
	}
	else {
		BLI_assert(0);
		zero_v2(co);
	}

	*xr = co[0];
	*yr = co[1];
}

void ED_mask_get_size(ScrArea *sa, int *width, int *height)
{
	if (sa && sa->spacedata.first) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				ED_space_clip_get_size(sc, width, height);
				break;
			}
			case SPACE_SEQ:
			{
//				Scene *scene = CTX_data_scene(C);
//				*width = (scene->r.size * scene->r.xsch) / 100;
//				*height = (scene->r.size * scene->r.ysch) / 100;
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				ED_space_image_get_size(sima, width, height);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				*width = 0;
				*height = 0;
				break;
		}
	}
	else {
		BLI_assert(0);
		*width = 0;
		*height = 0;
	}
}

void ED_mask_zoom(ScrArea *sa, ARegion *ar, float *zoomx, float *zoomy)
{
	if (sa && sa->spacedata.first) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				ED_space_clip_get_zoom(sc, ar, zoomx, zoomy);
				break;
			}
			case SPACE_SEQ:
			{
				*zoomx = *zoomy = 1.0f;
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				ED_space_image_get_zoom(sima, ar, zoomx, zoomy);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				*zoomx = *zoomy = 1.0f;
				break;
		}
	}
	else {
		BLI_assert(0);
		*zoomx = *zoomy = 1.0f;
	}
}

void ED_mask_get_aspect(ScrArea *sa, ARegion *UNUSED(ar), float *aspx, float *aspy)
{
	if (sa && sa->spacedata.first) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				ED_space_clip_get_aspect(sc, aspx, aspy);
				break;
			}
			case SPACE_SEQ:
			{
				*aspx = *aspy = 1.0f;  /* MASKTODO - render aspect? */
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				ED_space_image_get_aspect(sima, aspx, aspy);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				*aspx = *aspy = 1.0f;
				break;
		}
	}
	else {
		BLI_assert(0);
		*aspx = *aspy = 1.0f;
	}
}

void ED_mask_pixelspace_factor(ScrArea *sa, ARegion *ar, float *scalex, float *scaley)
{
	if (sa && sa->spacedata.first) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *sc = sa->spacedata.first;
				float aspx, aspy;

				UI_view2d_scale_get(&ar->v2d, scalex, scaley);
				ED_space_clip_get_aspect(sc, &aspx, &aspy);

				*scalex *= aspx;
				*scaley *= aspy;
				break;
			}
			case SPACE_SEQ:
			{
				*scalex = *scaley = 1.0f;  /* MASKTODO? */
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = sa->spacedata.first;
				float aspx, aspy;

				UI_view2d_scale_get(&ar->v2d, scalex, scaley);
				ED_space_image_get_aspect(sima, &aspx, &aspy);

				*scalex *= aspx;
				*scaley *= aspy;
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				*scalex = *scaley = 1.0f;
				break;
		}
	}
	else {
		BLI_assert(0);
		*scalex = *scaley = 1.0f;
	}
}

void ED_mask_cursor_location_get(ScrArea *sa, float cursor[2])
{
	if (sa) {
		switch (sa->spacetype) {
			case SPACE_CLIP:
			{
				SpaceClip *space_clip = sa->spacedata.first;
				copy_v2_v2(cursor, space_clip->cursor);
				break;
			}
			case SPACE_SEQ:
			{
				zero_v2(cursor);
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *space_image = sa->spacedata.first;
				copy_v2_v2(cursor, space_image->cursor);
				break;
			}
			default:
				/* possible other spaces from which mask editing is available */
				BLI_assert(0);
				zero_v2(cursor);
				break;
		}
	}
	else {
		BLI_assert(0);
		zero_v2(cursor);
	}
}

bool ED_mask_selected_minmax(const bContext *C, float min[2], float max[2])
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *mask_layer;
	bool ok = false;

	if (mask == NULL)
		return ok;

	INIT_MINMAX2(min, max);
	for (mask_layer = mask->masklayers.first;
	     mask_layer != NULL;
	     mask_layer = mask_layer->next)
	{
		MaskSpline *spline;
		if (mask_layer->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}
		for (spline = mask_layer->splines.first;
		     spline != NULL;
		     spline = spline->next)
		{
			MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);
			int i;
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *deform_point = &points_array[i];
				BezTriple *bezt = &point->bezt;
				float handle[2];
				if (!MASKPOINT_ISSEL_ANY(point)) {
					continue;
				}
				if (bezt->f2 & SELECT) {
					minmax_v2v2_v2(min, max, deform_point->bezt.vec[1]);
				}
				if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
					BKE_mask_point_handle(deform_point, MASK_WHICH_HANDLE_STICK, handle);
					minmax_v2v2_v2(min, max, handle);
				}
				else {
					if ((bezt->f1 & SELECT) && (bezt->h1 != HD_VECT)) {
						BKE_mask_point_handle(deform_point, MASK_WHICH_HANDLE_LEFT, handle);
						minmax_v2v2_v2(min, max, handle);
					}
					if ((bezt->f3 & SELECT) && (bezt->h2 != HD_VECT)) {
						BKE_mask_point_handle(deform_point, MASK_WHICH_HANDLE_RIGHT, handle);
						minmax_v2v2_v2(min, max, handle);
					}
				}
				ok = true;
			}
		}
	}
	return ok;
}

/********************** registration *********************/

void ED_operatortypes_mask(void)
{
	WM_operatortype_append(MASK_OT_new);

	/* mask layers */
	WM_operatortype_append(MASK_OT_layer_new);
	WM_operatortype_append(MASK_OT_layer_remove);

	/* add */
	WM_operatortype_append(MASK_OT_add_vertex);
	WM_operatortype_append(MASK_OT_add_feather_vertex);
	WM_operatortype_append(MASK_OT_primitive_circle_add);
	WM_operatortype_append(MASK_OT_primitive_square_add);

	/* geometry */
	WM_operatortype_append(MASK_OT_switch_direction);
	WM_operatortype_append(MASK_OT_normals_make_consistent);
	WM_operatortype_append(MASK_OT_delete);

	/* select */
	WM_operatortype_append(MASK_OT_select);
	WM_operatortype_append(MASK_OT_select_all);
	WM_operatortype_append(MASK_OT_select_border);
	WM_operatortype_append(MASK_OT_select_lasso);
	WM_operatortype_append(MASK_OT_select_circle);
	WM_operatortype_append(MASK_OT_select_linked_pick);
	WM_operatortype_append(MASK_OT_select_linked);
	WM_operatortype_append(MASK_OT_select_more);
	WM_operatortype_append(MASK_OT_select_less);

	/* hide/reveal */
	WM_operatortype_append(MASK_OT_hide_view_clear);
	WM_operatortype_append(MASK_OT_hide_view_set);

	/* feather */
	WM_operatortype_append(MASK_OT_feather_weight_clear);

	/* shape */
	WM_operatortype_append(MASK_OT_slide_point);
	WM_operatortype_append(MASK_OT_slide_spline_curvature);
	WM_operatortype_append(MASK_OT_cyclic_toggle);
	WM_operatortype_append(MASK_OT_handle_type_set);

	/* relationships */
	WM_operatortype_append(MASK_OT_parent_set);
	WM_operatortype_append(MASK_OT_parent_clear);

	/* shapekeys */
	WM_operatortype_append(MASK_OT_shape_key_insert);
	WM_operatortype_append(MASK_OT_shape_key_clear);
	WM_operatortype_append(MASK_OT_shape_key_feather_reset);
	WM_operatortype_append(MASK_OT_shape_key_rekey);

	/* layers */
	WM_operatortype_append(MASK_OT_layer_move);

	/* duplicate */
	WM_operatortype_append(MASK_OT_duplicate);

	/* clipboard */
	WM_operatortype_append(MASK_OT_copy_splines);
	WM_operatortype_append(MASK_OT_paste_splines);
}

void ED_keymap_mask(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	keymap = WM_keymap_find(keyconf, "Mask Editing", 0, 0);
	keymap->poll = ED_maskedit_poll;

	WM_keymap_add_item(keymap, "MASK_OT_new", NKEY, KM_PRESS, KM_ALT, 0);

	/* add menu */
	WM_keymap_add_menu(keymap, "MASK_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);

	/* mask mode supports PET now */
	ED_keymap_proportional_cycle(keyconf, keymap);
	ED_keymap_proportional_maskmode(keyconf, keymap);

	/* geometry */
	WM_keymap_add_item(keymap, "MASK_OT_add_vertex_slide", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MASK_OT_add_feather_vertex_slide", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "MASK_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_delete", DELKEY, KM_PRESS, 0, 0);

	/* selection */
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "MASK_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	WM_keymap_add_item(keymap, "MASK_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_select_circle", CKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);

	WM_keymap_add_item(keymap, "MASK_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MASK_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);

	/* hide/reveal */
	WM_keymap_add_item(keymap, "MASK_OT_hide_view_clear", HKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "MASK_OT_hide_view_set", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);

	kmi = WM_keymap_add_item(keymap, "MASK_OT_hide_view_set", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);

	/* select clip while in maker view,
	 * this matches View3D functionality where you can select an
	 * object while in editmode to allow vertex parenting */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);

	/* shape */
	WM_keymap_add_item(keymap, "MASK_OT_cyclic_toggle", CKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MASK_OT_slide_point", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_slide_spline_curvature", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_handle_type_set", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_normals_make_consistent", NKEY, KM_PRESS, KM_CTRL, 0);
	// WM_keymap_add_item(keymap, "MASK_OT_feather_weight_clear", SKEY, KM_PRESS, KM_ALT, 0);
	/* ... matches curve editmode */

	/* relationships */
	WM_keymap_add_item(keymap, "MASK_OT_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MASK_OT_parent_clear", PKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "MASK_OT_shape_key_insert", IKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MASK_OT_shape_key_clear", IKEY, KM_PRESS, KM_ALT, 0);

	/* duplicate */
	WM_keymap_add_item(keymap, "MASK_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "MASK_OT_copy_splines", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "MASK_OT_paste_splines", VKEY, KM_PRESS, KM_CTRL, 0);

	/* for image editor only */
	WM_keymap_add_item(keymap, "UV_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);

	/* Transform (don't use transform_keymap_for_space() since this maps to different spaces) */
	WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", GKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_resize", SKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "TRANSFORM_OT_rotate", RKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "mode", TFM_MASK_SHRINKFATTEN);
}

void ED_operatormacros_mask(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;

	ot = WM_operatortype_append_macro("MASK_OT_add_vertex_slide", "Add Vertex and Slide",
	                                  "Add new vertex and slide it", OPTYPE_UNDO | OPTYPE_REGISTER);
	ot->description = "Add new vertex and slide it";
	WM_operatortype_macro_define(ot, "MASK_OT_add_vertex");
	otmacro = WM_operatortype_macro_define(ot, "MASK_OT_slide_point");
	RNA_boolean_set(otmacro->ptr, "is_new_point", true);

	ot = WM_operatortype_append_macro("MASK_OT_add_feather_vertex_slide", "Add Feather Vertex and Slide",
	                                  "Add new vertex to feather and slide it", OPTYPE_UNDO | OPTYPE_REGISTER);
	ot->description = "Add new feather vertex and slide it";
	WM_operatortype_macro_define(ot, "MASK_OT_add_feather_vertex");
	otmacro = WM_operatortype_macro_define(ot, "MASK_OT_slide_point");
	RNA_boolean_set(otmacro->ptr, "slide_feather", true);

	ot = WM_operatortype_append_macro("MASK_OT_duplicate_move", "Add Duplicate", "Duplicate mask and move",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "MASK_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_enum_set(otmacro->ptr, "proportional", 0);
	RNA_boolean_set(otmacro->ptr, "mirror", false);
}
