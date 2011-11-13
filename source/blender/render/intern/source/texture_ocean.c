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
	int retval = TEX_INT;
	OceanTex *ot= tex->ot;	
	OceanResult or;
	const float u = 0.5+0.5*texvec[0];
	const float v = 0.5+0.5*texvec[1];
	float foam;
	int cfra = R.r.cfra;
	int normals=0;
	ModifierData *md;
	
	texres->tin = 0.0f;
	
	if (!ot || !ot->object || !ot->object->modifiers.first)
		return 0;
	
	if ((md = (ModifierData *)modifiers_findByType(ot->object, eModifierType_Ocean))) {
		OceanModifierData *omd = (OceanModifierData *)md;
		
		if (!omd->ocean)
			return 0;

		normals = (omd->flag & MOD_OCEAN_GENERATE_NORMALS);
		
		if (omd->oceancache && omd->cached==TRUE) {
			
			CLAMP(cfra, omd->bakestart, omd->bakeend);
			cfra -= omd->bakestart;	// shift to 0 based
		
			BKE_ocean_cache_eval_uv(omd->oceancache, &or, cfra, u, v);
		
		} else {	// non-cached
			
			if (G.rendering)
				BKE_ocean_eval_uv_catrom(omd->ocean, &or, u, v);
			else
				BKE_ocean_eval_uv(omd->ocean, &or, u, v);
			
			or.foam = BKE_ocean_jminus_to_foam(or.Jminus, omd->foam_coverage);
		}
	}
	
	
	switch (ot->output) {
		case TEX_OCN_DISPLACEMENT:
			/* XYZ displacement */
			texres->tr = 0.5 + 0.5 * or.disp[0];
			texres->tg = 0.5 + 0.5 * or.disp[2];
			texres->tb = 0.5 + 0.5 * or.disp[1];
			
			texres->tr = MAX2(0.0, texres->tr);
			texres->tg = MAX2(0.0, texres->tg);
			texres->tb = MAX2(0.0, texres->tb);

			BRICONTRGB;
			
			retval = TEX_RGB;
			break;
		
		case TEX_OCN_EMINUS:
			/* -ve eigenvectors ? */
			texres->tr = or.Eminus[0];
			texres->tg = or.Eminus[2];
			texres->tb = or.Eminus[1];
			retval = TEX_RGB;
			break;
		
		case TEX_OCN_EPLUS:
			/* -ve eigenvectors ? */
			texres->tr = or.Eplus[0];
			texres->tg = or.Eplus[2];
			texres->tb = or.Eplus[1];
			retval = TEX_RGB;
			break;
			
		case TEX_OCN_JPLUS:
			texres->tin = or.Jplus;
			retval = TEX_INT;
		case TEX_OCN_FOAM:
			
			texres->tin = or.foam;

			BRICONT;			
			
			retval = TEX_INT;
			break;
	}
			
	/* if normals needed */

	if (texres->nor && normals) {

		texres->nor[0] = or.normal[0];
		texres->nor[1] = or.normal[2];
		texres->nor[2] = or.normal[1];

		normalize_v3(texres->nor);
		retval |= TEX_NOR;
	}
	
	texres->ta = 1.0;
	
	return retval;
}

