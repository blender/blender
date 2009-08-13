/**
 *
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

#include "DNA_material_types.h"

#ifdef RNA_RUNTIME

#include "BKE_material.h"
#include "BKE_texture.h"

/*
  Adds material to the first free texture slot.
  If all slots are busy, replaces the first.
*/
static void rna_Material_add_texture(Material *ma, Tex *tex, int mapto, int texco)
{
	int i;
	MTex *mtex;
	int slot= -1;

	for (i= 0; i < MAX_MTEX; i++) {
		if (!ma->mtex[i]) {
			slot= i;
			break;
		}
	}

	if (slot == -1)
		slot= 0;

	if (ma->mtex[slot]) {
		ma->mtex[slot]->tex->id.us--;
	}
	else {
		ma->mtex[slot]= add_mtex();
	}

	mtex= ma->mtex[slot];

	mtex->tex= tex;
	id_us_plus(&tex->id);

	mtex->texco= mapto;
	mtex->mapto= texco;
}

#else

void RNA_api_material(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* copied from rna_def_material_mtex (rna_material.c) */
	static EnumPropertyItem prop_texture_coordinates_items[] = {
		{TEXCO_GLOB, "GLOBAL", 0, "Global", "Uses global coordinates for the texture coordinates."},
		{TEXCO_OBJECT, "OBJECT", 0, "Object", "Uses linked object's coordinates for texture coordinates."},
		{TEXCO_UV, "UV", 0, "UV", "Uses UV coordinates for texture coordinates."},
		{TEXCO_ORCO, "ORCO", 0, "Generated", "Uses the original undeformed coordinates of the object."},
		{TEXCO_STRAND, "STRAND", 0, "Strand", "Uses normalized strand texture coordinate (1D)."},
		{TEXCO_STICKY, "STICKY", 0, "Sticky", "Uses mesh's sticky coordinates for the texture coordinates."},
		{TEXCO_WINDOW, "WINDOW", 0, "Window", "Uses screen coordinates as texture coordinates."},
		{TEXCO_NORM, "NORMAL", 0, "Normal", "Uses normal vector as texture coordinates."},
		{TEXCO_REFL, "REFLECTION", 0, "Reflection", "Uses reflection vector as texture coordinates."},
		{TEXCO_STRESS, "STRESS", 0, "Stress", "Uses the difference of edge lengths compared to original coordinates of the mesh."},
		{TEXCO_TANGENT, "TANGENT", 0, "Tangent", "Uses the optional tangent vector as texture coordinates."},

		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_texture_mapto_items[] = {
		{MAP_COL, "COLOR", 0, "Color", "Causes the texture to affect basic color of the material"},
		{MAP_NORM, "NORMAL", 0, "Normal", "Causes the texture to affect the rendered normal"},
		{MAP_COLSPEC, "SPECULAR_COLOR", 0, "Specularity Color", "Causes the texture to affect the specularity color"},
		{MAP_COLMIR, "MIRROR", 0, "Mirror", "Causes the texture to affect the mirror color"},
		{MAP_REF, "REFLECTION", 0, "Reflection", "Causes the texture to affect the value of the materials reflectivity"},
		{MAP_SPEC, "SPECULARITY", 0, "Specularity", "Causes the texture to affect the value of specularity"},
		{MAP_EMIT, "EMIT", 0, "Emit", "Causes the texture to affect the emit value"},
		{MAP_ALPHA, "ALPHA", 0, "Alpha", "Causes the texture to affect the alpha value"},
		{MAP_HAR, "HARDNESS", 0, "Hardness", "Causes the texture to affect the hardness value"},
		{MAP_RAYMIRR, "RAY_MIRROR", 0, "Ray-Mirror", "Causes the texture to affect the ray-mirror value"},
		{MAP_TRANSLU, "TRANSLUCENCY", 0, "Translucency", "Causes the texture to affect the translucency value"},
		{MAP_AMB, "AMBIENT", 0, "Ambient", "Causes the texture to affect the value of ambient"},
		{MAP_DISPLACE, "DISPLACEMENT", 0, "Displacement", "Let the texture displace the surface"},
		{MAP_WARP, "WARP", 0, "Warp", "Let the texture warp texture coordinates of next channels"},
		{0, NULL, 0, NULL, NULL}};

	func= RNA_def_function(srna, "add_texture", "rna_Material_add_texture");
	RNA_def_function_ui_description(func, "Add a texture to material's free texture slot.");
	parm= RNA_def_pointer(func, "texture", "Texture", "", "Texture to add.");
	parm= RNA_def_enum(func, "texture_coordinates", prop_texture_coordinates_items, TEXCO_UV, "", "Source of texture coordinate information."); /* optional */
	parm= RNA_def_enum(func, "map_to", prop_texture_mapto_items, MAP_COL, "", "Controls which material property the texture affects."); /* optional */
}

#endif

