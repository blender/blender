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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2011 by Bastien Montagne.
* All rights reserved.
*
* Contributor(s): None yet.
*
* ***** END GPL LICENSE BLOCK *****
*
*/

/*
 * XXX I'd like to make modified weights visible in WeightPaint mode,
 *     but couldn't figure a way to do this…
 *     Maybe this will need changes in mesh_calc_modifiers (DerivedMesh.c)?
 *     Or the WeightPaint mode code itself?
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"          /* Texture masking. */

#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"
#include "MOD_util.h"
#include "MOD_weightvg_util.h"
#include "RE_shader_ext.h"        /* Texture masking. */

/* Applies new_w weights to org_w ones, using either a texture, vgroup or constant value as factor.
 * Return values are in org_w.
 * If indices is not NULL, it must be a table of same length as org_w and new_w, mapping to the real
 * vertex index (in case the weight tables do not cover the whole vertices...).
 * XXX The standard “factor” value is assumed in [0.0, 1.0] range. Else, weird results might appear.
 */
void weightvg_do_mask(int num, int *indices, float *org_w, float *new_w, Object *ob,
                      DerivedMesh *dm, float fact, const char defgrp_name[32], Tex *texture,
                      int tex_use_channel, int tex_mapping, Object *tex_map_object,
                      const char *tex_uvlayer_name)
{
	int ref_didx;
	MDeformVert *dvert = NULL;
	int i;

	/* If influence factor is null, nothing to do! */
	if (fact == 0.0) return;

	/* If we want to mask vgroup weights from a texture. */
	if (texture) {
		/* The texture coordinates. */
		float (*tex_co)[3];
		/* See mapping note below… */
		MappingInfoModifierData t_map;
		float (*v_co)[3];

		/* Use new generic get_texture_coords, but do not modify our DNA struct for it…
		 * XXX Why use a ModifierData stuff here ? Why not a simple, generic struct for parameters ?
		 *     What e.g. if a modifier wants to use several textures ?
		 *     Why use only v_co, and not MVert (or both) ?
		 */
		t_map.texture = texture;
		t_map.map_object = tex_map_object;
		BLI_strncpy(t_map.uvlayer_name, tex_uvlayer_name, sizeof(t_map.uvlayer_name));
		t_map.texmapping = tex_mapping;
		v_co = MEM_mallocN(sizeof(*v_co) * num, "WeightVG Modifier, TEX mode, v_co");
		dm->getVertCos(dm, v_co);
		tex_co = MEM_callocN(sizeof(*tex_co) * num, "WeightVG Modifier, TEX mode, tex_co");
		get_texture_coords(&t_map, ob, dm, v_co, tex_co, num);
		MEM_freeN(v_co);

		/* For each weight (vertex), make the mix between org and new weights. */
		for(i = 0; i < num; ++i) {
			int idx = indices ? indices[i] : i;
			TexResult texres;
			float h, s, v; /* For HSV color space. */

			texres.nor = NULL;
			get_texture_value(texture, tex_co[idx], &texres);
			/* Get the good channel value… */
			switch(tex_use_channel) {
			case MOD_WVG_MASK_TEX_USE_INT:
				org_w[i] = (new_w[i] * texres.tin * fact) + (org_w[i] * (1.0 - (texres.tin*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_RED:
				org_w[i] = (new_w[i] * texres.tr * fact) + (org_w[i] * (1.0 - (texres.tr*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_GREEN:
				org_w[i] = (new_w[i] * texres.tg * fact) + (org_w[i] * (1.0 - (texres.tg*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_BLUE:
				org_w[i] = (new_w[i] * texres.tb * fact) + (org_w[i] * (1.0 - (texres.tb*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_HUE:
				rgb_to_hsv(texres.tr, texres.tg, texres.tb, &h, &s, &v);
				org_w[i] = (new_w[i] * h * fact) + (org_w[i] * (1.0 - (h*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_SAT:
				rgb_to_hsv(texres.tr, texres.tg, texres.tb, &h, &s, &v);
				org_w[i] = (new_w[i] * s * fact) + (org_w[i] * (1.0 - (s*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_VAL:
				rgb_to_hsv(texres.tr, texres.tg, texres.tb, &h, &s, &v);
				org_w[i] = (new_w[i] * v * fact) + (org_w[i] * (1.0 - (v*fact)));
				break;
			case MOD_WVG_MASK_TEX_USE_ALPHA:
				org_w[i] = (new_w[i] * texres.ta * fact) + (org_w[i] * (1.0 - (texres.ta*fact)));
				break;
			default:
				org_w[i] = (new_w[i] * texres.tin * fact) + (org_w[i] * (1.0 - (texres.tin*fact)));
				break;
			}
		}

		MEM_freeN(tex_co);
		return;
	}

	/* Check whether we want to set vgroup weights from a constant weight factor or a vertex
	 * group.
	 */
	/* Get vgroup idx from its name. */
	ref_didx = defgroup_name_index(ob, defgrp_name);
	/* Proceed only if vgroup is valid, else use constant factor. */
	if (ref_didx >= 0) {
		/* Get actual dverts (ie vertex group data). */
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		/* Proceed only if vgroup is valid, else assume factor = O. */
		if (dvert == NULL) return;

		/* For each weight (vertex), make the mix between org and new weights. */
		for (i = 0; i < num; i++) {
			int idx = indices ? indices[i] : i;
			int j;
			for (j = 0; j < dvert[idx].totweight; j++) {
				if(dvert[idx].dw[j].def_nr == ref_didx) {
					float f = dvert[idx].dw[j].weight * fact;
					org_w[i] = (new_w[i] * f) + (org_w[i] * (1.0-f));
					break;
				}
			}
			/* If that vertex is not in ref vgroup, assume null factor, and hence do nothing! */
		}
		return;
	}

	/* Default "influence" behavior. */
	/* For each weight (vertex), make the mix between org and new weights. */
	for (i = 0; i < num; i++) {
		org_w[i] = (new_w[i] * fact) + (org_w[i] * (1.0-fact));
	}
}

/* Applies weights to given vgroup (defgroup), and optionnaly add/remove vertices from the group.
 * If indices is not NULL, it must be a table of same length as weights, mapping to the real
 * vertex index (in case the weight table does not cover the whole vertices...).
 */
void weightvg_update_vg(MDeformVert *dvert, int defgrp_idx, int num,
                        const int *indices, const float *weights, int do_add,
                        float add_thresh, int do_rem, float rem_thresh)
{
	int i;

	for (i = 0; i < num; i++) {
		int j;
		char add2vg = do_add;
		float w = weights[i];
		MDeformVert *dv = &dvert[indices ? indices[i] : i];
		MDeformWeight *newdw;

		/* Never allow weights out of [0.0, 1.0] range. */
		CLAMP(w, 0.0, 1.0);

		/* Let's first check to see if this vert is already in the weight group – if so
		 * let's update it, or remove it if needed.
		 */
		for (j = 0; j < dv->totweight; j++) {
			/* If this weight corresponds to the deform group, update the value or,
			 * if lower than rem_threshold, remove the vertex from the vgroup.
			 */
			if (dv->dw[j].def_nr == defgrp_idx) {
				/* Remove the vertex from this vgroup if needed. */
				if (do_rem && w < rem_thresh) {
					dv->totweight--;
					/* If there are still other deform weights attached to this vert then remove
					 * this deform weight, and reshuffle the others.
					 */
					if(dv->totweight) {
						newdw = MEM_mallocN(sizeof(MDeformWeight)*(dv->totweight), "deformWeight");
						if(dv->dw){
							memcpy(newdw, dv->dw, sizeof(MDeformWeight)*j);
							memcpy(newdw+j, dv->dw+j+1, sizeof(MDeformWeight)*(dv->totweight-j));
							MEM_freeN(dv->dw);
						}
						dv->dw = newdw;
					}
					/* If there are no other deform weights left then just remove this one. */
					else {
						MEM_freeN(dv->dw);
						dv->dw = NULL;
					}
				}
				/* Else, just set the new computed weight. */
				else {
					dv->dw[j].weight = w;
				}
				add2vg = 0;
				break;
			}
		}

		/* If the vert wasn't in the deform group, add it if needed!
		 */
		if (add2vg && w > add_thresh) {
			newdw = MEM_callocN(sizeof(MDeformWeight)*(dv->totweight+1), "WeightVGEdit Modifier, deformWeight");
			if(dv->dw) {
				memcpy(newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
				MEM_freeN(dv->dw);
			}
			dv->dw = newdw;
			dv->dw[dv->totweight].weight = w;
			dv->dw[dv->totweight].def_nr = defgrp_idx;
			dv->totweight++;
		}
	}
}

