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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_customdata.h"

#ifdef RNA_RUNTIME

/*static float rna_MVert_no_get(PointerRNA *ptr, int index)
{
	MVert *mvert= (MVert*)ptr->data;
	return mvert->no[index]/32767.0f;
}*/

static float rna_MVert_bevel_weight_get(PointerRNA *ptr)
{
	MVert *mvert= (MVert*)ptr->data;
	return mvert->bweight/255.0f;
}

static void rna_MVert_bevel_weight_set(PointerRNA *ptr, float value)
{
	MVert *mvert= (MVert*)ptr->data;
	mvert->bweight= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MEdge_bevel_weight_get(PointerRNA *ptr)
{
	MEdge *medge= (MEdge*)ptr->data;
	return medge->bweight/255.0f;
}

static void rna_MEdge_bevel_weight_set(PointerRNA *ptr, float value)
{
	MEdge *medge= (MEdge*)ptr->data;
	medge->bweight= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MEdge_crease_get(PointerRNA *ptr)
{
	MEdge *medge= (MEdge*)ptr->data;
	return medge->crease/255.0f;
}

static void rna_MEdge_crease_set(PointerRNA *ptr, float value)
{
	MEdge *medge= (MEdge*)ptr->data;
	medge->crease= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MCol_color1_get(PointerRNA *ptr, int index)
{
	MCol *mcol= (MCol*)ptr->data;
	return (&mcol[0].r)[index]/255.0f;
}

static void rna_MCol_color1_set(PointerRNA *ptr, int index, float value)
{
	MCol *mcol= (MCol*)ptr->data;
	(&mcol[0].r)[index]= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MCol_color2_get(PointerRNA *ptr, int index)
{
	MCol *mcol= (MCol*)ptr->data;
	return (&mcol[1].r)[index]/255.0f;
}

static void rna_MCol_color2_set(PointerRNA *ptr, int index, float value)
{
	MCol *mcol= (MCol*)ptr->data;
	(&mcol[1].r)[index]= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MCol_color3_get(PointerRNA *ptr, int index)
{
	MCol *mcol= (MCol*)ptr->data;
	return (&mcol[2].r)[index]/255.0f;
}

static void rna_MCol_color3_set(PointerRNA *ptr, int index, float value)
{
	MCol *mcol= (MCol*)ptr->data;
	(&mcol[2].r)[index]= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static float rna_MCol_color4_get(PointerRNA *ptr, int index)
{
	MCol *mcol= (MCol*)ptr->data;
	return (&mcol[2].r)[index]/255.0f;
}

static void rna_MCol_color4_set(PointerRNA *ptr, int index, float value)
{
	MCol *mcol= (MCol*)ptr->data;
	(&mcol[3].r)[index]= (char)(CLAMPIS(value*255.0f, 0, 255));
}

static int rna_Mesh_texspace_editable(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	return (me->texflag & AUTOSPACE)? PROP_NOT_EDITABLE: 0;
}

static void rna_MVert_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;

	if(me->dvert) {
		MVert *mvert= (MVert*)ptr->data;
		MDeformVert *dvert= me->dvert + (mvert-me->mvert);

		rna_iterator_array_begin(iter, (void*)dvert->dw, sizeof(MDeformWeight), dvert->totweight, NULL);
	}
	else
		rna_iterator_array_begin(iter, NULL, 0, 0, NULL);
}

static void rna_MMultires_level_range(PointerRNA *ptr, int *min, int *max)
{
	Multires *mr= (Multires*)ptr->data;
	*min= 1;
	*max= mr->level_count;
}

static void rna_MFace_mat_index_range(PointerRNA *ptr, int *min, int *max)
{
	Mesh *me= (Mesh*)ptr->id.data;
	*min= 0;
	*max= me->totcol-1;
}

static int rna_CustomDataLayer_length(PointerRNA *ptr, int type)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer;
	int i, length= 0;

	for(layer=me->fdata.layers, i=0; i<me->fdata.totlayer; layer++, i++)
		if(layer->type == type)
			length++;

	return length;
}

static int rna_CustomDataLayer_active_get(PointerRNA *ptr, int type, int render)
{
	Mesh *me= (Mesh*)ptr->id.data;
	int n= ((CustomDataLayer*)ptr->data) - me->fdata.layers;

	if(render) return (n == CustomData_get_render_layer_index(&me->fdata, type));
	else return (n == CustomData_get_active_layer_index(&me->fdata, type));
}

static void rna_CustomDataLayer_active_set(PointerRNA *ptr, int value, int type, int render)
{
	Mesh *me= (Mesh*)ptr->id.data;
	int n= ((CustomDataLayer*)ptr->data) - me->fdata.layers;

	if(value == 0)
		return;

	if(render) CustomData_set_layer_render_index(&me->fdata, type, n);
	else CustomData_set_layer_active_index(&me->fdata, type, n);
}

static int rna_uv_layer_check(CollectionPropertyIterator *iter, void *data)
{
	CustomDataLayer *layer= (CustomDataLayer*)data;
	return (layer->type != CD_MTFACE);
}

static void rna_Mesh_uv_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	rna_iterator_array_begin(iter, (void*)me->fdata.layers, sizeof(CustomDataLayer), me->fdata.totlayer, rna_uv_layer_check);
}

static int rna_Mesh_uv_layers_length(PointerRNA *ptr)
{
	return rna_CustomDataLayer_length(ptr, CD_MTFACE);
}

static float rna_MTFace_uv1_get(PointerRNA *ptr, int index)
{
	MTFace *mtface= (MTFace*)ptr->data;
	return mtface->uv[0][index];
}

static void rna_MTFace_uv1_set(PointerRNA *ptr, int index, float value)
{
	MTFace *mtface= (MTFace*)ptr->data;
	mtface->uv[0][index]= value;
}

static float rna_MTFace_uv2_get(PointerRNA *ptr, int index)
{
	MTFace *mtface= (MTFace*)ptr->data;
	return mtface->uv[1][index];
}

static void rna_MTFace_uv2_set(PointerRNA *ptr, int index, float value)
{
	MTFace *mtface= (MTFace*)ptr->data;
	mtface->uv[1][index]= value;
}

static float rna_MTFace_uv3_get(PointerRNA *ptr, int index)
{
	MTFace *mtface= (MTFace*)ptr->data;
	return mtface->uv[2][index];
}

static void rna_MTFace_uv3_set(PointerRNA *ptr, int index, float value)
{
	MTFace *mtface= (MTFace*)ptr->data;
	mtface->uv[2][index]= value;
}

static float rna_MTFace_uv4_get(PointerRNA *ptr, int index)
{
	MTFace *mtface= (MTFace*)ptr->data;
	return mtface->uv[3][index];
}

static void rna_MTFace_uv4_set(PointerRNA *ptr, int index, float value)
{
	MTFace *mtface= (MTFace*)ptr->data;
	mtface->uv[3][index]= value;
}

static void rna_MTFaceLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer= (CustomDataLayer*)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MTFace), me->totface, NULL);
}

static int rna_MTFaceLayer_data_length(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	return me->totface;
}

static int rna_MTFaceLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, CD_MTFACE, 1);
}

static int rna_MTFaceLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, CD_MTFACE, 0);
}

static void rna_MTFaceLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, value, CD_MTFACE, 1);
}

static void rna_MTFaceLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, value, CD_MTFACE, 0);
}

static int rna_vcol_layer_check(CollectionPropertyIterator *iter, void *data)
{
	CustomDataLayer *layer= (CustomDataLayer*)data;
	return (layer->type != CD_MCOL);
}

static void rna_Mesh_vcol_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	rna_iterator_array_begin(iter, (void*)me->fdata.layers, sizeof(CustomDataLayer), me->fdata.totlayer, rna_vcol_layer_check);
}

static int rna_Mesh_vcol_layers_length(PointerRNA *ptr)
{
	return rna_CustomDataLayer_length(ptr, CD_MCOL);
}

static void rna_MColLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer= (CustomDataLayer*)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MCol)*4, me->totface, NULL);
}

static int rna_MColLayer_data_length(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	return me->totface;
}

static int rna_MColLayer_active_render_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, CD_MCOL, 1);
}

static int rna_MColLayer_active_get(PointerRNA *ptr)
{
	return rna_CustomDataLayer_active_get(ptr, CD_MCOL, 0);
}

static void rna_MColLayer_active_render_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, value, CD_MCOL, 1);
}

static void rna_MColLayer_active_set(PointerRNA *ptr, int value)
{
	rna_CustomDataLayer_active_set(ptr, value, CD_MCOL, 0);
}

static void rna_MFloatPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer= (CustomDataLayer*)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MFloatProperty), me->totface, NULL);
}

static int rna_MFloatPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	return me->totface;
}

static int rna_float_layer_check(CollectionPropertyIterator *iter, void *data)
{
	CustomDataLayer *layer= (CustomDataLayer*)data;
	return (layer->type != CD_PROP_FLT);
}

static void rna_Mesh_float_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	rna_iterator_array_begin(iter, (void*)me->fdata.layers, sizeof(CustomDataLayer), me->fdata.totlayer, rna_float_layer_check);
}

static int rna_Mesh_float_layers_length(PointerRNA *ptr)
{
	return rna_CustomDataLayer_length(ptr, CD_PROP_FLT);
}

static int rna_int_layer_check(CollectionPropertyIterator *iter, void *data)
{
	CustomDataLayer *layer= (CustomDataLayer*)data;
	return (layer->type != CD_PROP_INT);
}

static void rna_MIntPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer= (CustomDataLayer*)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MIntProperty), me->totface, NULL);
}

static int rna_MIntPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	return me->totface;
}

static void rna_Mesh_int_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	rna_iterator_array_begin(iter, (void*)me->fdata.layers, sizeof(CustomDataLayer), me->fdata.totlayer, rna_int_layer_check);
}

static int rna_Mesh_int_layers_length(PointerRNA *ptr)
{
	return rna_CustomDataLayer_length(ptr, CD_PROP_INT);
}

static int rna_string_layer_check(CollectionPropertyIterator *iter, void *data)
{
	CustomDataLayer *layer= (CustomDataLayer*)data;
	return (layer->type != CD_PROP_STR);
}

static void rna_MStringPropertyLayer_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	CustomDataLayer *layer= (CustomDataLayer*)ptr->data;
	rna_iterator_array_begin(iter, layer->data, sizeof(MStringProperty), me->totface, NULL);
}

static int rna_MStringPropertyLayer_data_length(PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->id.data;
	return me->totface;
}

static void rna_Mesh_string_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mesh *me= (Mesh*)ptr->data;
	rna_iterator_array_begin(iter, (void*)me->fdata.layers, sizeof(CustomDataLayer), me->fdata.totlayer, rna_string_layer_check);
}

static int rna_Mesh_string_layers_length(PointerRNA *ptr)
{
	return rna_CustomDataLayer_length(ptr, CD_PROP_STR);
}

#else

static void rna_def_mvert_group(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MVertGroup", NULL, "Mesh Vertex Group");
	RNA_def_struct_sdna(srna, "MDeformWeight");

	/* XXX how to point to actual group? */
	prop= RNA_def_property(srna, "group", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "def_nr");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Group Index", "");

	prop= RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Weight", "Vertex Weight");
}

static void rna_def_mvert(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MVert", NULL, "Mesh Vertex");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Location", "Vertex Location");

	/*prop= RNA_def_property(srna, "no", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_funcs(prop, "rna_MVert_no_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Normal", "Vertex Normal");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);*/

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Selected", "");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hidden", "");

	prop= RNA_def_property(srna, "bevel_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MVert_bevel_weight_get", "rna_MVert_bevel_weight_set", NULL);
	RNA_def_property_ui_text(prop, "Bevel Weight", "Weight used by the Bevel modifier 'Only Vertices' option");

	prop= RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_MVert_groups_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0, 0);
	RNA_def_property_struct_type(prop, "MVertGroup");
	RNA_def_property_ui_text(prop, "Groups", "Weights for the vertex groups this vertex is member of");
}

static void rna_def_medge(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MEdge", NULL, "Mesh Edge");

	prop= RNA_def_property(srna, "verts", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "v1");
	RNA_def_property_array(prop, 2);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");

	prop= RNA_def_property(srna, "crease", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MEdge_crease_get", "rna_MEdge_crease_set", NULL);
	RNA_def_property_ui_text(prop, "Crease", "Weight used by the Subsurf modifier for creasing");

	prop= RNA_def_property(srna, "bevel_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_MEdge_bevel_weight_get", "rna_MEdge_bevel_weight_set", NULL);
	RNA_def_property_ui_text(prop, "Bevel Weight", "Weight used by the Bevel modifier");

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Selected", "");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hidden", "");

	prop= RNA_def_property(srna, "seam", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SEAM);
	RNA_def_property_ui_text(prop, "Seam", "Seam edge for UV unwrapping");

	prop= RNA_def_property(srna, "sharp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SHARP);
	RNA_def_property_ui_text(prop, "Sharp", "Sharp edge for the EdgeSplit modifier");
}

static void rna_def_mface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MFace", NULL, "Mesh Face");

	prop= RNA_def_property(srna, "verts", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "v1");
	RNA_def_property_array(prop, 4);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Vertices", "Vertex indices");

	prop= RNA_def_property(srna, "mat_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "mat_nr");
	RNA_def_property_ui_text(prop, "Material Index", "");
	RNA_def_property_range(prop, 0, MAXMAT-1);
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MFace_mat_index_range");

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_FACE_SEL);
	RNA_def_property_ui_text(prop, "Selected", "");

	prop= RNA_def_property(srna, "hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_HIDE);
	RNA_def_property_ui_text(prop, "Hidden", "");

	prop= RNA_def_property(srna, "smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_SMOOTH);
	RNA_def_property_ui_text(prop, "Smooth", "");
}

static void rna_def_mtface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static const EnumPropertyItem transp_items[]= {
		{TF_SOLID, "OPAQUE", "Opaque", "Render color of textured face as color"},
		{TF_ADD, "ADD", "Add", "Render face transparent and add color of face"},
		{TF_ALPHA, "ALPHA", "Alpha", "Render polygon transparent, depending on alpha channel of the texture"},
		{TF_CLIP, "CLIPALPHA", "Clip Alpha", "Use the images alpha values clipped with no blending (binary alpha)"},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "MTFace", NULL, "Mesh Texture Face");

	/* prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tpage");
	RNA_def_property_ui_text(prop, "Image", ""); */

	prop= RNA_def_property(srna, "tex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_TEX);
	RNA_def_property_ui_text(prop, "Tex", "Render face with texture");

	prop= RNA_def_property(srna, "tiles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_TILES);
	RNA_def_property_ui_text(prop, "Tiles", "Use tilemode for face");

	prop= RNA_def_property(srna, "light", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_LIGHT);
	RNA_def_property_ui_text(prop, "Light", "Use light for face");

	prop= RNA_def_property(srna, "invisible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_INVISIBLE);
	RNA_def_property_ui_text(prop, "Invisible", "Make face invisible");

	prop= RNA_def_property(srna, "collision", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_DYNAMIC);
	RNA_def_property_ui_text(prop, "Collision", "Use face for collision and ray-sensor detection");

	prop= RNA_def_property(srna, "shared", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_SHAREDCOL);
	RNA_def_property_ui_text(prop, "Shared", "Blend vertex colors across face when vertices are shared");

	prop= RNA_def_property(srna, "twoside", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_TWOSIDE);
	RNA_def_property_ui_text(prop, "Twoside", "Render face twosided");

	prop= RNA_def_property(srna, "obcolor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_OBCOL);
	RNA_def_property_ui_text(prop, "ObColor", "Use ObColor instead of vertex colors");

	prop= RNA_def_property(srna, "halo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_BILLBOARD);
	RNA_def_property_ui_text(prop, "Halo", "Screen aligned billboard");

	prop= RNA_def_property(srna, "billboard", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_BILLBOARD2);
	RNA_def_property_ui_text(prop, "Billboard", "Billboard with Z-axis constraint");

	prop= RNA_def_property(srna, "shadow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_SHADOW);
	RNA_def_property_ui_text(prop, "Shadow", "Face is used for shadow");

	prop= RNA_def_property(srna, "text", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_BMFONT);
	RNA_def_property_ui_text(prop, "Text", "Enable bitmap text on face");

	prop= RNA_def_property(srna, "alphasort", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", TF_ALPHASORT);
	RNA_def_property_ui_text(prop, "Alpha Sort", "Enable sorting of faces for correct alpha drawing (slow, use Clip Alpha instead when possible)");

	prop= RNA_def_property(srna, "transp", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, transp_items);
	RNA_def_property_ui_text(prop, "Transparency", "Transparency blending mode");

	prop= RNA_def_property(srna, "uv_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TF_SEL1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Selected", "");

	prop= RNA_def_property(srna, "uv_pinned", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "unwrap", TF_PIN1);
	RNA_def_property_array(prop, 4);
	RNA_def_property_ui_text(prop, "UV Pinned", "");

	prop= RNA_def_property(srna, "uv1", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MTFace_uv1_get", "rna_MTFace_uv1_set", NULL);
	RNA_def_property_ui_text(prop, "UV 1", "");

	prop= RNA_def_property(srna, "uv2", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MTFace_uv2_get", "rna_MTFace_uv2_set", NULL);
	RNA_def_property_ui_text(prop, "UV 2", "");

	prop= RNA_def_property(srna, "uv3", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MTFace_uv3_get", "rna_MTFace_uv3_set", NULL);
	RNA_def_property_ui_text(prop, "UV 3", "");

	prop= RNA_def_property(srna, "uv4", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_MTFace_uv4_get", "rna_MTFace_uv4_set", NULL);
	RNA_def_property_ui_text(prop, "UV 4", "");

	srna= RNA_def_struct(brna, "MTFaceLayer", NULL, "Mesh Texture Face Layer");
	RNA_def_struct_sdna(srna, "CustomDataLayer");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "");

	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MTFaceLayer_active_get", "rna_MTFaceLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");

	prop= RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MTFaceLayer_active_render_get", "rna_MTFaceLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MTFace");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MTFaceLayer_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, "rna_MTFaceLayer_data_length", 0, 0);
}

static void rna_def_msticky(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MSticky", NULL, "Mesh Vertex Sticky Texture Coordinate");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_ui_text(prop, "Location", "Sticky texture coordinate location");
}

static void rna_def_mcol(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MCol", NULL, "Mesh Vertex Color");

	prop= RNA_def_property(srna, "color1", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MCol_color1_get", "rna_MCol_color1_set", NULL);
	RNA_def_property_ui_text(prop, "Color 1", "");

	prop= RNA_def_property(srna, "color2", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MCol_color2_get", "rna_MCol_color2_set", NULL);
	RNA_def_property_ui_text(prop, "Color 2", "");

	prop= RNA_def_property(srna, "color3", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MCol_color3_get", "rna_MCol_color3_set", NULL);
	RNA_def_property_ui_text(prop, "Color 3", "");

	prop= RNA_def_property(srna, "color4", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MCol_color4_get", "rna_MCol_color4_set", NULL);
	RNA_def_property_ui_text(prop, "Color 4", "");

	srna= RNA_def_struct(brna, "MColLayer", NULL, "Mesh Texture Face Layer");
	RNA_def_struct_sdna(srna, "CustomDataLayer");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "");

	prop= RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_MColLayer_active_get", "rna_MColLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Sets the layer as active for display and editing");

	prop= RNA_def_property(srna, "active_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "active_rnd", 0);
	RNA_def_property_boolean_funcs(prop, "rna_MColLayer_active_render_get", "rna_MColLayer_active_render_set");
	RNA_def_property_ui_text(prop, "Active Render", "Sets the layer as active for rendering");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MCol");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MColLayer_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, "rna_MColLayer_data_length", 0, 0);
}

static void rna_def_mproperties(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* Float */
	srna= RNA_def_struct(brna, "MFloatProperty", NULL, "Mesh Float Property");

	prop= RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f");
	RNA_def_property_ui_text(prop, "Value", "");

	srna= RNA_def_struct(brna, "MFloatPropertyLayer", NULL, "Mesh Float Property Layer");
	RNA_def_struct_sdna(srna, "CustomDataLayer");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MFloatProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MFloatPropertyLayer_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, "rna_MFloatPropertyLayer_data_length", 0, 0);

	/* Int */
	srna= RNA_def_struct(brna, "MIntProperty", NULL, "Mesh Int Property");

	prop= RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "i");
	RNA_def_property_ui_text(prop, "Value", "");

	srna= RNA_def_struct(brna, "MIntPropertyLayer", NULL, "Mesh Int Property Layer");
	RNA_def_struct_sdna(srna, "CustomDataLayer");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MIntProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MIntPropertyLayer_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, "rna_MIntPropertyLayer_data_length", 0, 0);

	/* String */
	srna= RNA_def_struct(brna, "MStringProperty", NULL, "Mesh String Property");

	prop= RNA_def_property(srna, "value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "s");
	RNA_def_property_ui_text(prop, "Value", "");

	srna= RNA_def_struct(brna, "MStringPropertyLayer", NULL, "Mesh String Property Layer");
	RNA_def_struct_sdna(srna, "CustomDataLayer");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MStringProperty");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_MStringPropertyLayer_data_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, "rna_MStringPropertyLayer_data_length", 0, 0);
}

static void rna_def_mmultires(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MMultires", NULL, "Mesh Multires");
	RNA_def_struct_sdna(srna, "Multires");

	prop= RNA_def_property(srna, "level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "newlvl");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MMultires_level_range");
	RNA_def_property_ui_text(prop, "Level", "");

	prop= RNA_def_property(srna, "edge_level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "edgelvl");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MMultires_level_range");
	RNA_def_property_ui_text(prop, "Edge Level", "");

	prop= RNA_def_property(srna, "pin_level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pinlvl");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MMultires_level_range");
	RNA_def_property_ui_text(prop, "Pin Level", "Set level to apply modifiers to during render");

	prop= RNA_def_property(srna, "render_level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "renderlvl");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_MMultires_level_range");
	RNA_def_property_ui_text(prop, "Render Level", "Set level to render");
}

static void rna_def_mesh(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Mesh", "ID", "Mesh");

	prop= RNA_def_property(srna, "verts", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mvert", "totvert");
	RNA_def_property_struct_type(prop, "MVert");
	RNA_def_property_ui_text(prop, "Vertices", "Vertices of the mesh");

	prop= RNA_def_property(srna, "edges", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "medge", "totedge");
	RNA_def_property_struct_type(prop, "MEdge");
	RNA_def_property_ui_text(prop, "Edges", "Edges of the mesh");

	prop= RNA_def_property(srna, "faces", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mface", "totface");
	RNA_def_property_struct_type(prop, "MFace");
	RNA_def_property_ui_text(prop, "Faces", "Faces of the mesh");

	prop= RNA_def_property(srna, "sticky", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "msticky", "totvert");
	RNA_def_property_struct_type(prop, "MSticky");
	RNA_def_property_ui_text(prop, "Sticky", "Sticky texture coordinates");

	prop= RNA_def_property(srna, "uv_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_uv_layers_begin", 0, 0, 0, 0, "rna_Mesh_uv_layers_length", 0, 0);
	RNA_def_property_struct_type(prop, "MTFaceLayer");
	RNA_def_property_ui_text(prop, "UV Layers", "");

	prop= RNA_def_property(srna, "vcol_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_vcol_layers_begin", 0, 0, 0, 0, "rna_Mesh_vcol_layers_length", 0, 0);
	RNA_def_property_struct_type(prop, "MColLayer");
	RNA_def_property_ui_text(prop, "Vertex Color Layers", "");

	prop= RNA_def_property(srna, "float_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_float_layers_begin", 0, 0, 0, 0, "rna_Mesh_float_layers_length", 0, 0);
	RNA_def_property_struct_type(prop, "MFloatPropertyLayer");
	RNA_def_property_ui_text(prop, "Float Property Layers", "");

	prop= RNA_def_property(srna, "int_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_int_layers_begin", 0, 0, 0, 0, "rna_Mesh_int_layers_length", 0, 0);
	RNA_def_property_struct_type(prop, "MIntPropertyLayer");
	RNA_def_property_ui_text(prop, "Int Property Layers", "");

	prop= RNA_def_property(srna, "string_layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "fdata.layers", "fdata.totlayer");
	RNA_def_property_collection_funcs(prop, "rna_Mesh_string_layers_begin", 0, 0, 0, 0, "rna_Mesh_string_layers_length", 0, 0);
	RNA_def_property_struct_type(prop, "MStringPropertyLayer");
	RNA_def_property_ui_text(prop, "String Property Layers", "");

	prop= RNA_def_property(srna, "autosmooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_AUTOSMOOTH);
	RNA_def_property_ui_text(prop, "Auto Smooth", "Treats all set-smoothed faces with angles less than the specified angle as 'smooth' during render");

	prop= RNA_def_property(srna, "autosmooth_angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "smoothresh");
	RNA_def_property_range(prop, 1, 80);
	RNA_def_property_ui_text(prop, "Auto Smooth Angle", "Defines maximum angle between face normals that 'Auto Smooth' will operate on");

	prop= RNA_def_property(srna, "novnormalflip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_NOPUNOFLIP);
	RNA_def_property_ui_text(prop, "No Vertex Normal Flip", "Disables flipping of vertexnormals during render");

	prop= RNA_def_property(srna, "twosided", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ME_TWOSIDED);
	RNA_def_property_ui_text(prop, "Double Sided", "Render/display the mesh with double or single sided lighting");

	prop= RNA_def_property(srna, "autotexspace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", AUTOSPACE);
	RNA_def_property_ui_text(prop, "Auto Texture Space", "Adjusts active object's texture space automatically when transforming object");

	prop= RNA_def_property(srna, "texspace_loc", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Texure Space Location", "Texture space location");
	RNA_def_property_funcs(prop, NULL, "rna_Mesh_texspace_editable");

	prop= RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
	RNA_def_property_funcs(prop, NULL, "rna_Mesh_texspace_editable");

	/* not supported yet
	prop= RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
	RNA_def_property_funcs(prop, NULL, "rna_Mesh_texspace_editable");*/

	prop= RNA_def_property(srna, "texco_mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "texcomesh");
	RNA_def_property_ui_text(prop, "Texture Space Mesh", "Derive texture coordinates from another mesh");

	prop= RNA_def_property(srna, "multires", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mr");
	RNA_def_property_ui_text(prop, "Multires", "");

	/*prop= RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Materials", "");*/

	/*prop= RNA_def_property(srna, "key", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Key", "");*/
}

void RNA_def_mesh(BlenderRNA *brna)
{
	rna_def_mesh(brna);
	rna_def_mvert(brna);
	rna_def_mvert_group(brna);
	rna_def_medge(brna);
	rna_def_mface(brna);
	rna_def_mtface(brna);
	rna_def_msticky(brna);
	rna_def_mcol(brna);
	rna_def_mproperties(brna);
	rna_def_mmultires(brna);
}

#endif

