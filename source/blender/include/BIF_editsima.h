/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

struct Mesh;
struct EditMesh;
struct SpaceImage;
struct EditFace;
struct MTFace;

/* id can be from 0 to 3 */
#define TF_PIN_MASK(id) (TF_PIN1 << id)
#define TF_SEL_MASK(id) (TF_SEL1 << id)

  
/* this checks weather a face is drarn without the local image check
 * - warning - no check for G.sima->flag, use SIMA_FACEDRAW_CHECK
 */
#define SIMA_FACEDRAW_CHECK_NOLOCAL(efa) \
	((G.sima->flag & SI_SYNC_UVSEL) ? (efa->h==0) : (efa->h==0 && efa->f & SELECT))

/* this check includes the local image check - (does the faces image match the space image?) */
#define SIMA_FACEDRAW_CHECK(efa, tf) \
	((G.sima && G.sima->flag & SI_LOCAL_UV) ? ((tf->tpage==G.sima->image) ? SIMA_FACEDRAW_CHECK_NOLOCAL(efa):0) : (SIMA_FACEDRAW_CHECK_NOLOCAL(efa)))

#define SIMA_FACESEL_CHECK(efa, tf) \
	((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? (efa->f & SELECT) : (!(~tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) &&(!efa->v4 || tf->flag & TF_SEL4)))
#define SIMA_FACESEL_SET(efa, tf) \
	((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? (EM_select_face(efa, 1))	: (tf->flag |=  (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))
#define SIMA_FACESEL_UNSET(efa, tf) \
	((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? (EM_select_face(efa, 0))	: (tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))

#define SIMA_UVSEL_CHECK(efa, tf, i)	((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? \
	(G.scene->selectmode == SCE_SELECT_FACE ? efa->f & SELECT :		((*(&efa->v1 + i))->f & SELECT) )	: (tf->flag &   TF_SEL_MASK(i) ))
#define SIMA_UVSEL_SET(efa, tf, i)		((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? \
	(G.scene->selectmode == SCE_SELECT_FACE ? EM_select_face(efa, 1) : ((*(&efa->v1 + i))->f |=  SELECT) ) : (tf->flag |=  TF_SEL_MASK(i) ))
#define SIMA_UVSEL_UNSET(efa, tf, i)	((G.sima && G.sima->flag & SI_SYNC_UVSEL) ? \
	(G.scene->selectmode == SCE_SELECT_FACE ? EM_select_face(efa, 0) : ((*(&efa->v1 + i))->f &= ~SELECT) ) : (tf->flag &= ~TF_SEL_MASK(i) ))

struct Object;

void object_uvs_changed(struct Object *ob);
void object_tface_flags_changed(struct Object *ob, int updateButtons);

int is_uv_tface_editing_allowed(void);
int is_uv_tface_editing_allowed_silent(void);

void get_connected_limit_tface_uv(float *limit);
int minmax_tface_uv(float *min, float *max);
int cent_tface_uv(float *cent, int mode);

void transform_width_height_tface_uv(int *width, int *height);
void transform_aspect_ratio_tface_uv(float *aspx, float *aspy);

void mouseco_to_cursor_sima(void);
void borderselect_sima(short whichuvs);
void mouseco_to_curtile(void);
void mouse_select_sima(void);
void snap_menu_sima(void);
void aspect_sima(struct SpaceImage *sima, float *x, float *y);

void select_invert_tface_uv(void);
void select_swap_tface_uv(void);
void mirrormenu_tface_uv(void);
void mirror_tface_uv(char mirroraxis);
void hide_tface_uv(int swap);
void reveal_tface_uv(void);
void stitch_uv_tface(int mode);
void unlink_selection(void);
void select_linked_tface_uv(int mode);
void pin_tface_uv(int mode);
void weld_align_menu_tface_uv(void);
void weld_align_tface_uv(char tool);
void be_square_tface_uv(struct EditMesh *em);
void select_pinned_tface_uv(void);

void sima_sample_color(void);

#define UV_SELECT_ALL		1
#define UV_SELECT_PINNED	2

void new_image_sima(void);
void reload_image_sima(void);
void save_image_sima(void);
void save_as_image_sima(void);
void save_image_sequence_sima(void);
void replace_image_sima(short imageselect);
void open_image_sima(short imageselect);
void pack_image_sima(void);

/* checks images for forced updates on frame change */
void BIF_image_update_frame(void);

void find_nearest_uv(struct MTFace **nearesttf, struct EditFace **nearestefa, unsigned int *nearestv, int *nearestuv);
