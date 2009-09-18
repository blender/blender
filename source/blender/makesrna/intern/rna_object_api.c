/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_object.h"
#include "BKE_mball.h"
#include "BKE_main.h"

#include "DNA_mesh_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

/* copied from init_render_mesh (render code) */
static Mesh *rna_Object_create_render_mesh(Object *ob, bContext *C, Scene *scene)
{
	CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	Object *tmpobj = NULL;
	DerivedMesh *dm;
	Mesh *me;
	
	switch(ob->type) {
 	case OB_FONT:
 	case OB_CURVE:
 	case OB_SURF:
 	{
 		int cage = 0; //XXX -todo
 		Curve *tmpcu = NULL;

		/* copies object and modifiers (but not the data) */
		tmpobj= copy_object( ob );
		tmpcu = (Curve *)tmpobj->data;
		tmpcu->id.us--;

		/* if getting the original caged mesh, delete object modifiers */
		if( cage )
			object_free_modifiers(tmpobj);

		/* copies the data */
		tmpobj->data = copy_curve( (Curve *) ob->data );

#if 0
		/* copy_curve() sets disp.first null, so currently not need */
		{
			Curve *cu;
			cu = (Curve *)tmpobj->data;
			if( cu->disp.first )
				MEM_freeN( cu->disp.first );
			cu->disp.first = NULL;
		}
	
#endif

		/* get updated display list, and convert to a mesh */
		makeDispListCurveTypes( scene, tmpobj, 0 );
		nurbs_to_mesh( tmpobj );

		/* nurbs_to_mesh changes the type tp a mesh, check it worked */
		if (tmpobj->type != OB_MESH) {
			free_libblock_us( &(CTX_data_main(C)->object), tmpobj );
			printf("cant convert curve to mesh. Does the curve have any segments?" ); // XXX use report api
		}
		me = tmpobj->data;
		free_libblock_us( &(CTX_data_main(C)->object), tmpobj );
 		break;
 	}
 	case OB_MBALL:
		/* metaballs don't have modifiers, so just convert to mesh */
		ob = find_basis_mball(scene, ob);
		/* todo, re-generatre for render-res */
		// metaball_polygonize(scene, ob)
		me = add_mesh("Mesh");
		mball_to_mesh( &ob->disp, me );
		break;
	case OB_MESH:
	{
		dm= mesh_create_derived_render(scene, ob, mask);
		// dm= mesh_create_derived_view(scene, ob, mask);

		if(!dm)
			return NULL;

		me= add_mesh("tmp_render_mesh");
		me->id.us--; /* we don't assign it to anything */
		DM_to_mesh(dm, me);
		dm->release(dm);
		break;
	}
	default:
		return NULL;
	}


	{	/* update the material */
		short i, *totcol =give_totcolp(ob);

		/* free the current material list */
		if(me->mat)
			MEM_freeN((void *)me->mat);

		me->mat= (Material **)MEM_callocN(sizeof(void *)*(*totcol), "matarray");

		for(i=0; i<*totcol; i++) {
			Material *mat= give_current_material(ob, i+1);
			if(mat) {
				me->mat[i]= mat;
				mat->id.us++;
			}
		}
	}

	return me;
}

#else

void RNA_api_object(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "create_render_mesh", "rna_Object_create_render_mesh");
	RNA_def_function_ui_description(func, "Create a Mesh datablock with all modifiers applied.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	prop= RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh created from object, remove it if it is only used for export.");
	RNA_def_function_return(func, prop);
}

#endif

