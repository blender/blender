/*
 * Copyright 2016, Blender Foundation.
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
 */

/** \file blender/draw/intern/draw_manager_text.c
 *  \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_math.h"

#include "BIF_gl.h"

#include "GPU_matrix.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"
#include "UI_interface.h"

#include "WM_api.h"
#include "BLF_api.h"

#include "draw_manager_text.h"

typedef struct ViewCachedString {
	struct ViewCachedString *next, *prev;
	float vec[3];
	union {
		uchar ub[4];
		int pack;
	} col;
	short sco[2];
	short xoffs;
	short flag;
	int str_len;

	/* str is allocated past the end */
	char str[0];
} ViewCachedString;

typedef struct DRWTextStore {
	ListBase list;
} DRWTextStore;

DRWTextStore *DRW_text_cache_create(void)
{
	DRWTextStore *dt = MEM_callocN(sizeof(*dt), __func__);
	return dt;
}

void DRW_text_cache_destroy(struct DRWTextStore *dt)
{
	BLI_freelistN(&dt->list);
	MEM_freeN(dt);
}

void DRW_text_cache_add(
        DRWTextStore *dt,
        const float co[3],
        const char *str, const int str_len,
        short xoffs, short flag,
        const uchar col[4])
{
	int alloc_len;
	ViewCachedString *vos;

	if (flag & DRW_TEXT_CACHE_STRING_PTR) {
		BLI_assert(str_len == strlen(str));
		alloc_len = sizeof(void *);
	}
	else {
		alloc_len = str_len + 1;
	}

	vos = MEM_mallocN(sizeof(ViewCachedString) + alloc_len, __func__);

	BLI_addtail(&dt->list, vos);

	copy_v3_v3(vos->vec, co);
	copy_v4_v4_uchar(vos->col.ub, col);
	vos->xoffs = xoffs;
	vos->flag = flag;
	vos->str_len = str_len;

	/* allocate past the end */
	if (flag & DRW_TEXT_CACHE_STRING_PTR) {
		memcpy(vos->str, &str, alloc_len);
	}
	else {
		memcpy(vos->str, str, alloc_len);
	}
}

void DRW_text_cache_draw(DRWTextStore *dt, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	ViewCachedString *vos;
	int tot = 0;

	/* project first and test */
	for (vos = dt->list.first; vos; vos = vos->next) {
		if (ED_view3d_project_short_ex(
		        ar,
		        (vos->flag & DRW_TEXT_CACHE_GLOBALSPACE) ? rv3d->persmat : rv3d->persmatob,
		        (vos->flag & DRW_TEXT_CACHE_LOCALCLIP) != 0,
		        vos->vec, vos->sco,
		        V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_WIN | V3D_PROJ_TEST_CLIP_NEAR) == V3D_PROJ_RET_OK)
		{
			tot++;
		}
		else {
			vos->sco[0] = IS_CLIPPED;
		}
	}

	if (tot) {
		int col_pack_prev = 0;

		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_disable();
		}

		float original_proj[4][4];
		GPU_matrix_projection_get(original_proj);
		wmOrtho2_region_pixelspace(ar);

		GPU_matrix_push();
		GPU_matrix_identity_set();

		const int font_id = BLF_default();

		const uiStyle *style = UI_style_get();

		BLF_size(font_id, style->widget.points * U.pixelsize, U.dpi);

		for (vos = dt->list.first; vos; vos = vos->next) {
			if (vos->sco[0] != IS_CLIPPED) {
				if (col_pack_prev != vos->col.pack) {
					BLF_color4ubv(font_id, vos->col.ub);
					col_pack_prev = vos->col.pack;
				}

				BLF_position(
				        font_id,
				        (float)(vos->sco[0] + vos->xoffs), (float)(vos->sco[1]), 2.0f);

				((vos->flag & DRW_TEXT_CACHE_ASCII) ?
				 BLF_draw_ascii :
				 BLF_draw
				 )(font_id,
				   (vos->flag & DRW_TEXT_CACHE_STRING_PTR) ? *((const char **)vos->str) : vos->str,
				   vos->str_len);
			}
		}

		GPU_matrix_pop();
		GPU_matrix_projection_set(original_proj);

		if (rv3d->rflag & RV3D_CLIPPING) {
			ED_view3d_clipping_enable();
		}
	}
}
