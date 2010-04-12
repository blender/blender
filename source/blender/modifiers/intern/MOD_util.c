/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation;
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "string.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_utildefines.h"

#include "MOD_util.h"

#include "MEM_guardedalloc.h"

#include "RE_shader_ext.h"

void get_texture_value(Tex *texture, float *tex_co, TexResult *texres)
{
	int result_type;

	result_type = multitex_ext(texture, tex_co, NULL, NULL, 0, texres);

	/* if the texture gave an RGB value, we assume it didn't give a valid
	* intensity, so calculate one (formula from do_material_tex).
	* if the texture didn't give an RGB value, copy the intensity across
	*/
	if(result_type & TEX_RGB)
		texres->tin = (0.35f * texres->tr + 0.45f * texres->tg
				+ 0.2f * texres->tb);
	else
		texres->tr = texres->tg = texres->tb = texres->tin;
}

void modifier_vgroup_cache(ModifierData *md, float (*vertexCos)[3])
{
	while((md=md->next) && md->type==eModifierType_Armature) {
		ArmatureModifierData *amd = (ArmatureModifierData*) md;
		if(amd->multi && amd->prevCos==NULL)
			amd->prevCos= MEM_dupallocN(vertexCos);
		else
			break;
	}
	/* lattice/mesh modifier too */
}

void validate_layer_name(const CustomData *data, int type, char *name, char *outname)
{
	int index = -1;

	/* if a layer name was given, try to find that layer */
	if(name[0])
		index = CustomData_get_named_layer_index(data, CD_MTFACE, name);

	if(index < 0) {
		/* either no layer was specified, or the layer we want has been
		* deleted, so assign the active layer to name
		*/
		index = CustomData_get_active_layer_index(data, CD_MTFACE);
		strcpy(outname, data->layers[index].name);
	}
	else
		strcpy(outname, name);
}

/* returns a cdderivedmesh if dm == NULL or is another type of derivedmesh */
DerivedMesh *get_cddm(struct Scene *scene, Object *ob, struct EditMesh *em, DerivedMesh *dm, float (*vertexCos)[3])
{
	if(dm && dm->type == DM_TYPE_CDDM)
		return dm;

	if(!dm) {
		dm= get_dm(scene, ob, em, dm, vertexCos, 0);
	}
	else {
		dm= CDDM_copy(dm);
		CDDM_apply_vert_coords(dm, vertexCos);
	}

	if(dm)
		CDDM_calc_normals(dm);
	
	return dm;
}


static int is_last_displist(Object *ob)
{
	Curve *cu = ob->data;
	static int curvecount=0, totcurve=0;

	if(curvecount == 0){
		DispList *dl;

		totcurve = 0;
		for(dl=cu->disp.first; dl; dl=dl->next)
			totcurve++;
	}

	curvecount++;

	if(curvecount == totcurve){
		curvecount = 0;
		return 1;
	}

	return 0;
}

/* returns a derived mesh if dm == NULL, for deforming modifiers that need it */
DerivedMesh *get_dm(struct Scene *scene, Object *ob, struct EditMesh *em, DerivedMesh *dm, float (*vertexCos)[3], int orco)
{
	if(dm)
		return dm;

	if(ob->type==OB_MESH) {
		if(em) dm= CDDM_from_editmesh(em, ob->data);
		else dm = CDDM_from_mesh((struct Mesh *)(ob->data), ob);

		if(vertexCos) {
			CDDM_apply_vert_coords(dm, vertexCos);
			//CDDM_calc_normals(dm);
		}
		
		if(orco)
			DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, get_mesh_orco_verts(ob));
	}
	else if(ELEM3(ob->type,OB_FONT,OB_CURVE,OB_SURF)) {
		if(is_last_displist(ob)) {
			dm= CDDM_from_curve(ob);
		}
	}

	return dm;
}
