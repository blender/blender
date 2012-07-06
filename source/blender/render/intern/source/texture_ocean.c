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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stddef.h>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BKE_global.h"	/* XXX */

#include "BKE_modifier.h"
#include "BKE_ocean.h"
#include "BKE_utildefines.h"

#include "render_types.h"
#include "RE_shader_ext.h"

#include "texture.h"


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */




/* ***** actual texture sampling ***** */
int ocean_texture(Tex *tex, float *texvec, TexResult *texres)
{
	OceanTex *ot = tex->ot;
	ModifierData *md;
	OceanModifierData *omd;

	texres->tin = 0.0f;

	if ( !(ot) ||
	     !(ot->object) ||
	     !(md  = (ModifierData *)modifiers_findByType(ot->object, eModifierType_Ocean)) ||
	     !(omd = (OceanModifierData *)md)->ocean)
	{
		return 0;
	}
	else {
		const int do_normals = (omd->flag & MOD_OCEAN_GENERATE_NORMALS);
		int cfra = R.r.cfra;
		int retval = TEX_INT;

		OceanResult ocr;
		const float u = 0.5f + 0.5f * texvec[0];
		const float v = 0.5f + 0.5f * texvec[1];

		if (omd->oceancache && omd->cached == TRUE) {

			CLAMP(cfra, omd->bakestart, omd->bakeend);
			cfra -= omd->bakestart;	/* shift to 0 based */

			BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra, u, v);

		}
		else {	/* non-cached */

			if (G.rendering)
				BKE_ocean_eval_uv_catrom(omd->ocean, &ocr, u, v);
			else
				BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);

			ocr.foam = BKE_ocean_jminus_to_foam(ocr.Jminus, omd->foam_coverage);
		}

		switch (ot->output) {
			case TEX_OCN_DISPLACEMENT:
				/* XYZ displacement */
				texres->tr = 0.5f + 0.5f * ocr.disp[0];
				texres->tg = 0.5f + 0.5f * ocr.disp[2];
				texres->tb = 0.5f + 0.5f * ocr.disp[1];

				texres->tr = MAX2(0.0f, texres->tr);
				texres->tg = MAX2(0.0f, texres->tg);
				texres->tb = MAX2(0.0f, texres->tb);

				BRICONTRGB;

				retval = TEX_RGB;
				break;

			case TEX_OCN_EMINUS:
				/* -ve eigenvectors ? */
				texres->tr = ocr.Eminus[0];
				texres->tg = ocr.Eminus[2];
				texres->tb = ocr.Eminus[1];
				retval = TEX_RGB;
				break;

			case TEX_OCN_EPLUS:
				/* -ve eigenvectors ? */
				texres->tr = ocr.Eplus[0];
				texres->tg = ocr.Eplus[2];
				texres->tb = ocr.Eplus[1];
				retval = TEX_RGB;
				break;

			case TEX_OCN_JPLUS:
				texres->tin = ocr.Jplus;
				retval = TEX_INT;
				break;

			case TEX_OCN_FOAM:

				texres->tin = ocr.foam;

				BRICONT;

				retval = TEX_INT;
				break;
		}

		/* if normals needed */

		if (texres->nor && do_normals) {
			normalize_v3_v3(texres->nor, ocr.normal);
			retval |= TEX_NOR;
		}

		texres->ta = 1.0f;

		return retval;
	}
}
