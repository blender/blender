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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory, Robin Allen, Bob Holcomb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_nodetree.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <string.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"
#include "rna_internal_types.h"

#include "BLI_listbase.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_image.h"
#include "BKE_texture.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "WM_types.h"

#include "MEM_guardedalloc.h"

EnumPropertyItem nodetree_type_items[] = {
	{NTREE_SHADER,      "SHADER",       ICON_MATERIAL,      "Shader",       "Shader nodes"},
	{NTREE_TEXTURE,     "TEXTURE",      ICON_TEXTURE,       "Texture",      "Texture nodes"},
	{NTREE_COMPOSIT,    "COMPOSITING",  ICON_RENDERLAYERS,  "Compositing",  "Compositing nodes"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_quality_items[] = {
	{NTREE_QUALITY_HIGH,   "HIGH",     0,    "High",     "High quality"},
	{NTREE_QUALITY_MEDIUM, "MEDIUM",   0,    "Medium",   "Medium quality"},
	{NTREE_QUALITY_LOW,    "LOW",      0,    "Low",      "Low quality"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_chunksize_items[] = {
    {NTREE_CHUNCKSIZE_32,   "32",     0,    "32x32",     "Chunksize of 32x32"},
    {NTREE_CHUNCKSIZE_64,   "64",     0,    "64x64",     "Chunksize of 64x64"},
    {NTREE_CHUNCKSIZE_128,   "128",     0,    "128x128",     "Chunksize of 128x128"},
    {NTREE_CHUNCKSIZE_256,   "256",     0,    "256x256",     "Chunksize of 256x256"},
    {NTREE_CHUNCKSIZE_512,   "512",     0,    "512x512",     "Chunksize of 512x512"},
    {NTREE_CHUNCKSIZE_1024,   "1024",     0,    "1024x1024",     "Chunksize of 1024x1024"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_socket_type_items[] = {
	{SOCK_FLOAT,   "VALUE",     0,    "Value",     ""},
	{SOCK_VECTOR,  "VECTOR",    0,    "Vector",    ""},
	{SOCK_RGBA,    "RGBA",      0,    "RGBA",      ""},
	{SOCK_SHADER,  "SHADER",    0,    "Shader",    ""},
	{SOCK_BOOLEAN, "BOOLEAN",   0,    "Boolean",   ""},
	{SOCK_MESH,    "MESH",      0,    "Mesh",      ""},
	{SOCK_INT,     "INT",       0,    "Int",       ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_math_items[] = {
	{ 0, "ADD",          0, "Add",          ""},
	{ 1, "SUBTRACT",     0, "Subtract",     ""},
	{ 2, "MULTIPLY",     0, "Multiply",     ""},
	{ 3, "DIVIDE",       0, "Divide",       ""},
	{ 4, "SINE",         0, "Sine",         ""},
	{ 5, "COSINE",       0, "Cosine",       ""},
	{ 6, "TANGENT",      0, "Tangent",      ""},
	{ 7, "ARCSINE",      0, "Arcsine",      ""},
	{ 8, "ARCCOSINE",    0, "Arccosine",    ""},
	{ 9, "ARCTANGENT",   0, "Arctangent",   ""},
	{10, "POWER",        0, "Power",        ""},
	{11, "LOGARITHM",    0, "Logarithm",    ""},
	{12, "MINIMUM",      0, "Minimum",      ""},
	{13, "MAXIMUM",      0, "Maximum",      ""},
	{14, "ROUND",        0, "Round",        ""},
	{15, "LESS_THAN",    0, "Less Than",    ""},
	{16, "GREATER_THAN", 0, "Greater Than", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_vec_math_items[] = {
	{0, "ADD",           0, "Add",           ""},
	{1, "SUBTRACT",      0, "Subtract",      ""},
	{2, "AVERAGE",       0, "Average",       ""},
	{3, "DOT_PRODUCT",   0, "Dot Product",   ""},
	{4, "CROSS_PRODUCT", 0, "Cross Product", ""},
	{5, "NORMALIZE",     0, "Normalize",     ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem node_filter_items[] = {
	{0, "SOFTEN",  0, "Soften",  ""},
	{1, "SHARPEN", 0, "Sharpen", ""},
	{2, "LAPLACE", 0, "Laplace", ""},
	{3, "SOBEL",   0, "Sobel",   ""},
	{4, "PREWITT", 0, "Prewitt", ""},
	{5, "KIRSCH",  0, "Kirsch",  ""},
	{6, "SHADOW",  0, "Shadow",  ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem prop_noise_basis_items[] = {
	{SHD_NOISE_PERLIN, "PERLIN", 0, "Perlin", ""},
	{SHD_NOISE_VORONOI_F1, "VORONOI_F1", 0, "Voronoi F1", ""},
	{SHD_NOISE_VORONOI_F2, "VORONOI_F2", 0, "Voronoi F2", ""},
	{SHD_NOISE_VORONOI_F3, "VORONOI_F3", 0, "Voronoi F3", ""},
	{SHD_NOISE_VORONOI_F4, "VORONOI_F4", 0, "Voronoi F4", ""},
	{SHD_NOISE_VORONOI_F2_F1, "VORONOI_F2_F1", 0, "Voronoi F2-F1", ""},
	{SHD_NOISE_VORONOI_CRACKLE, "VORONOI_CRACKLE", 0, "Voronoi Crackle", ""},
	{SHD_NOISE_CELL_NOISE, "CELL_NOISE", 0, "Cell Noise", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem prop_noise_type_items[] = {
	{SHD_NOISE_SOFT, "SOFT", 0, "Soft", ""},
	{SHD_NOISE_HARD, "HARD", 0, "Hard", ""},
	{0, NULL, 0, NULL, NULL}
};

#if 0
EnumPropertyItem prop_wave_items[] = {
	{SHD_WAVE_SINE, "SINE", 0, "Sine", "Use a sine wave to produce bands"},
	{SHD_WAVE_SAW, "SAW", 0, "Saw", "Use a saw wave to produce bands"},
	{SHD_WAVE_TRI, "TRI", 0, "Tri", "Use a triangle wave to produce bands"},
	{0, NULL, 0, NULL, NULL}
};
#endif

/* Add any new socket value subtype here.
 * When adding a new subtype here, make sure you also add it
 * to the subtype definitions in DNA_node_types.h.
 * This macro is used by the RNA and the internal converter functions
 * to define all socket subtypes. The SUBTYPE macro must be defined
 * before using this macro, and undefined afterwards.
 */
#define NODE_DEFINE_SUBTYPES_INT \
    SUBTYPE(INT, Int, NONE, None) \
    SUBTYPE(INT, Int, UNSIGNED, Unsigned)

#define NODE_DEFINE_SUBTYPES_FLOAT \
    SUBTYPE(FLOAT, Float, NONE, None) \
    SUBTYPE(FLOAT, Float, UNSIGNED, Unsigned) \
    SUBTYPE(FLOAT, Float, PERCENTAGE, Percentage) \
    SUBTYPE(FLOAT, Float, FACTOR, Factor) \
    SUBTYPE(FLOAT, Float, ANGLE, Angle) \
    SUBTYPE(FLOAT, Float, TIME, Time) \
    SUBTYPE(FLOAT, Float, DISTANCE, Distance)

#define NODE_DEFINE_SUBTYPES_VECTOR \
    SUBTYPE(VECTOR, Vector, NONE, None) \
    SUBTYPE(VECTOR, Vector, TRANSLATION, Translation) \
    SUBTYPE(VECTOR, Vector, DIRECTION, Direction) \
    SUBTYPE(VECTOR, Vector, VELOCITY, Velocity) \
    SUBTYPE(VECTOR, Vector, ACCELERATION, Acceleration) \
    SUBTYPE(VECTOR, Vector, EULER, Euler) \
    SUBTYPE(VECTOR, Vector, XYZ, XYZ)

#define NODE_DEFINE_SUBTYPES \
    NODE_DEFINE_SUBTYPES_INT \
    NODE_DEFINE_SUBTYPES_FLOAT \
    NODE_DEFINE_SUBTYPES_VECTOR

#ifdef RNA_RUNTIME

#include "BLI_linklist.h"

#include "ED_node.h"

#include "RE_pipeline.h"

#include "DNA_scene_types.h"
#include "WM_api.h"

static StructRNA *rna_Node_refine(struct PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;

	switch (node->type) {
		
		#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
			case ID: return &RNA_##Category##StructName;
				
		#include "rna_nodetree_types.h"
		
		case NODE_GROUP:
			return &RNA_NodeGroup;
		case NODE_FORLOOP:
			return &RNA_NodeForLoop;
		case NODE_WHILELOOP:
			return &RNA_NodeWhileLoop;
		case NODE_FRAME:
			return &RNA_NodeFrame;
			
		default:
			return &RNA_Node;
	}
}

static StructRNA *rna_NodeTree_refine(struct PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;

	switch (ntree->type) {
		case NTREE_SHADER:
			return &RNA_ShaderNodeTree;
		case NTREE_COMPOSIT:
			return &RNA_CompositorNodeTree;
		case NTREE_TEXTURE:
			return &RNA_TextureNodeTree;
		default:
			return &RNA_NodeTree;
	}
}

static char *rna_Node_path(PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;

	return BLI_sprintfN("nodes[\"%s\"]", node->name);
}

static StructRNA *rna_NodeSocket_refine(PointerRNA *ptr)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	
	if (sock->default_value) {
		/* This returns the refined socket type with the full definition
		 * of the default input value with type and subtype.
		 */
		
		#define SUBTYPE(socktype, stypename, id, idname) \
		{ \
			bNodeSocketValue##stypename * value = (bNodeSocketValue##stypename *)sock->default_value; \
			if (value->subtype == PROP_##id) \
				return &RNA_NodeSocket##stypename##idname; \
		}
		
		switch (sock->type) {
			case SOCK_FLOAT:
				NODE_DEFINE_SUBTYPES_FLOAT
				break;
			case SOCK_INT:
				NODE_DEFINE_SUBTYPES_INT
				break;
			case SOCK_BOOLEAN:
				return &RNA_NodeSocketBoolean;
			case SOCK_VECTOR:
				NODE_DEFINE_SUBTYPES_VECTOR
				break;
			case SOCK_RGBA:
				return &RNA_NodeSocketRGBA;
			case SOCK_SHADER:
				return &RNA_NodeSocketShader;
		}
		
		#undef SUBTYPE
	}
	
	return &RNA_NodeSocket;
}

static char *rna_NodeSocket_path(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNode *node;
	int socketindex;
	
	/* group sockets */
	socketindex = BLI_findindex(&ntree->inputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("inputs[%d]", socketindex);
	
	socketindex = BLI_findindex(&ntree->outputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("outputs[%d]", socketindex);
	
	/* node sockets */
	if (!nodeFindNode(ntree, sock, &node, NULL, NULL)) return NULL;
	
	socketindex = BLI_findindex(&node->inputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("nodes[\"%s\"].inputs[%d]", node->name, socketindex);
	
	socketindex = BLI_findindex(&node->outputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("nodes[\"%s\"].outputs[%d]", node->name, socketindex);
	
	return NULL;
}

/* Button Set Funcs for Matte Nodes */
static void rna_Matte_t1_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;
	
	chroma->t1 = value;
	
	if (value < chroma->t2)
		chroma->t2 = value;
}

static void rna_Matte_t2_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;
	
	if (value > chroma->t1)
		value = chroma->t1;
	
	chroma->t2 = value;
}

static void rna_distance_matte_t1_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;

	chroma->t1 = value;
}

static void rna_distance_matte_t2_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;

	chroma->t2 = value;
}

static void rna_difference_matte_t1_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;

	chroma->t1 = value;
}

static void rna_difference_matte_t2_set(PointerRNA *ptr, float value)
{
	bNode *node = (bNode *)ptr->data;
	NodeChroma *chroma = node->storage;

	chroma->t2 = value;
}


static void rna_Node_scene_set(PointerRNA *ptr, PointerRNA value)
{
	bNode *node = (bNode *)ptr->data;

	if (node->id) {
		id_us_min(node->id);
		node->id = NULL;
	}

	node->id = value.data;

	id_us_plus(node->id);
}



static void node_update(Main *bmain, Scene *UNUSED(scene), bNodeTree *ntree, bNode *node)
{
	ED_node_generic_update(bmain, ntree, node);
}

static void rna_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	node_update(bmain, scene, ntree, node);
}

static void rna_Node_tex_image_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	node_update(bmain, scene, ntree, node);
	WM_main_add_notifier(NC_IMAGE, NULL);
}

static void rna_Node_material_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	if (node->id)
		nodeSetActive(ntree, node);

	node_update(bmain, scene, ntree, node);
}

static void rna_NodeGroup_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;
	
	ntreeUpdateTree((bNodeTree *)node->id);
	
	node_update(bmain, scene, ntree, node);
}

static void rna_Node_name_set(PointerRNA *ptr, const char *value)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;
	char oldname[sizeof(node->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, node->name, sizeof(node->name));
	/* set new name */
	BLI_strncpy_utf8(node->name, value, sizeof(node->name));
	
	nodeUniqueName(ntree, node);
	
	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename(NULL, "nodes", oldname, node->name);
}

static void rna_NodeSocket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNode *node;
	
	if (nodeFindNode(ntree, sock, &node, NULL, NULL))
		node_update(bmain, scene, ntree, node);
}

static void rna_NodeGroupSocket_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNode *node;
	
	ntreeUpdateTree(ntree);
	
	if (nodeFindNode(ntree, sock, &node, NULL, NULL))
		node_update(bmain, scene, ntree, node);
}

#if 0 /* UNUSED */
static void rna_NodeLink_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;

	ntree->update |= NTREE_UPDATE_LINKS;
	ntreeUpdateTree(ntree);
}
#endif

static void rna_NodeSocketInt_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNodeSocketValueInt *val = (bNodeSocketValueInt *)sock->default_value;
	*softmin = val->min;
	*softmax = val->max;
}

static void rna_NodeSocketFloat_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNodeSocketValueFloat *val = (bNodeSocketValueFloat *)sock->default_value;
	*softmin = val->min;
	*softmax = val->max;
}

static void rna_NodeSocketVector_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNodeSocketValueVector *val = (bNodeSocketValueVector *)sock->default_value;
	*softmin = val->min;
	*softmax = val->max;
}

static void rna_Node_image_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;
	Image *ima = (Image *)node->id;
	ImageUser *iuser = node->storage;
	
	BKE_image_multilayer_index(ima->rr, iuser);
	BKE_image_signal(ima, iuser, IMA_SIGNAL_SRC_CHANGE);
	
	rna_Node_update(bmain, scene, ptr);
}

static EnumPropertyItem *renderresult_layers_add_enum(RenderLayer *rl)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int i = 0, totitem = 0;
	
	while (rl) {
		tmp.identifier = rl->name;
		/* little trick: using space char instead empty string makes the item selectable in the dropdown */
		if (rl->name[0] == '\0')
			tmp.name = " ";
		else
			tmp.name = rl->name;
		tmp.value = i++;
		RNA_enum_item_add(&item, &totitem, &tmp);
		rl = rl->next;
	}
	
	RNA_enum_item_end(&item, &totitem);

	return item;
}

static EnumPropertyItem *rna_Node_image_layer_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                    PropertyRNA *UNUSED(prop), int *free)
{
	bNode *node = (bNode *)ptr->data;
	Image *ima = (Image *)node->id;
	EnumPropertyItem *item = NULL;
	RenderLayer *rl;
	
	if (!ima || !(ima->rr)) return NULL;
	
	rl = ima->rr->layers.first;
	item = renderresult_layers_add_enum(rl);
	
	*free = 1;
	
	return item;
}

static EnumPropertyItem *rna_Node_scene_layer_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                    PropertyRNA *UNUSED(prop), int *free)
{
	bNode *node = (bNode *)ptr->data;
	Scene *sce = (Scene *)node->id;
	EnumPropertyItem *item = NULL;
	RenderLayer *rl;
	
	if (!sce) return NULL;
	
	rl = sce->r.layers.first;
	item = renderresult_layers_add_enum(rl);
	
	*free = 1;
	
	return item;
}

static EnumPropertyItem *rna_Node_channel_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                PropertyRNA *UNUSED(prop), int *free)
{
	bNode *node = (bNode *)ptr->data;
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int totitem = 0;
	
	switch (node->custom1) {
		case CMP_NODE_CHANNEL_MATTE_CS_RGB:
			tmp.identifier = "R"; tmp.name = "R"; tmp.value = 1;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "G"; tmp.name = "G"; tmp.value = 2;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "B"; tmp.name = "B"; tmp.value = 3;
			RNA_enum_item_add(&item, &totitem, &tmp);
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_HSV:
			tmp.identifier = "H"; tmp.name = "H"; tmp.value = 1;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "S"; tmp.name = "S"; tmp.value = 2;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "V"; tmp.name = "V"; tmp.value = 3;
			RNA_enum_item_add(&item, &totitem, &tmp);
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_YUV:
			tmp.identifier = "Y"; tmp.name = "Y"; tmp.value = 1;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "G"; tmp.name = "U"; tmp.value = 2;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "V"; tmp.name = "V"; tmp.value = 3;
			RNA_enum_item_add(&item, &totitem, &tmp);
			break;
		case CMP_NODE_CHANNEL_MATTE_CS_YCC:
			tmp.identifier = "Y"; tmp.name = "Y"; tmp.value = 1;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "CB"; tmp.name = "Cr"; tmp.value = 2;
			RNA_enum_item_add(&item, &totitem, &tmp);
			tmp.identifier = "CR"; tmp.name = "Cb"; tmp.value = 3;
			RNA_enum_item_add(&item, &totitem, &tmp);
			break;
		default:
			break;
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;
	
	return item;
}

static PointerRNA rna_NodeTree_active_node_get(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNode *node = nodeGetActive(ntree);
	return rna_pointer_inherit_refine(ptr, &RNA_Node, node);
}

static void rna_NodeTree_active_node_set(PointerRNA *ptr, PointerRNA value)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNode *node = (bNode *)value.data;
	if (node && BLI_findindex(&ntree->nodes, node) != -1)
		nodeSetActive(ntree, node);
	else
		nodeClearActive(ntree);
}

static bNode *rna_NodeTree_node_new(bNodeTree *ntree, bContext *C, ReportList *reports,
                                    int type, bNodeTree *group)
{
	bNode *node;
	bNodeTemplate ntemp;

	if (type == NODE_GROUP && group == NULL) {
		BKE_reportf(reports, RPT_ERROR, "node type \'GROUP\' missing group argument");
		return NULL;
	}
	
	ntemp.type = type;
	ntemp.ngroup = group;
	ntemp.scene = CTX_data_scene(C);
	ntemp.main = CTX_data_main(C);
	node = nodeAddNode(ntree, &ntemp);
	
	if (node == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Unable to create node");
	}
	else {
		ntreeUpdateTree(ntree); /* update group node socket links*/
		nodeUpdate(ntree, node);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

		if (group)
			id_us_plus(&group->id);
	}

	return node;
}

static bNode *rna_NodeTree_node_composite_new(bNodeTree *ntree, bContext *C, ReportList *reports,
                                              int type, bNodeTree *group)
{
	/* raises error on failure */
	bNode *node = rna_NodeTree_node_new(ntree, C, reports, type, group);
	
	if (node) {
		if (ELEM4(node->type, CMP_NODE_COMPOSITE, CMP_NODE_DEFOCUS, CMP_NODE_OUTPUT_FILE, CMP_NODE_R_LAYERS)) {
			/* annoying, find the node tree we are in, scene can be NULL */
			Scene *scene;
			for (scene = CTX_data_main(C)->scene.first; scene; scene = scene->id.next) {
				if (scene->nodetree == ntree) {
					break;
				}
			}
			node->id = (ID *)scene;
			id_us_plus(node->id);
		}

		ntreeCompositForceHidden(ntree, CTX_data_scene(C));
		ntreeUpdateTree(ntree);
	}

	return node;
}

static bNode *rna_NodeTree_node_texture_new(bNodeTree *ntree, bContext *C, ReportList *reports,
                                            int type, bNodeTree *group)
{
	/* raises error on failure */
	bNode *node = rna_NodeTree_node_new(ntree, C, reports, type, group);

	if (node) {
		ntreeTexCheckCyclics(ntree);
	}

	return node;
}

static void rna_NodeTree_node_remove(bNodeTree *ntree, ReportList *reports, bNode *node)
{
	if (BLI_findindex(&ntree->nodes, node) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to locate node '%s' in nodetree", node->name);
	}
	else {
		if (node->id)
			id_us_min(node->id);

		nodeFreeNode(ntree, node);
		ntreeUpdateTree(ntree); /* update group node socket links*/

		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
}

static void rna_NodeTree_node_clear(bNodeTree *ntree)
{
	bNode *node = ntree->nodes.first;

	while (node) {
		bNode *next_node = node->next;

		if (node->id)
			id_us_min(node->id);

		nodeFreeNode(ntree, node);

		node = next_node;
	}

	ntreeUpdateTree(ntree); /* update group node socket links*/

	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static bNodeLink *rna_NodeTree_link_new(bNodeTree *ntree, ReportList *reports,
                                        bNodeSocket *fromsock, bNodeSocket *tosock)
{
	bNodeLink *ret;
	bNode *fromnode = NULL, *tonode = NULL;
	int from_in_out, to_in_out;

	nodeFindNode(ntree, fromsock, &fromnode, NULL, &from_in_out);
	nodeFindNode(ntree, tosock, &tonode, NULL, &to_in_out);
	
	if (&from_in_out == &to_in_out) {
		BKE_reportf(reports, RPT_ERROR, "Same input/output direction of sockets");
		return NULL;
	}

	/* unlink node input socket */
	if (to_in_out == SOCK_IN)
		nodeRemSocketLinks(ntree, tosock);
	else
		nodeRemSocketLinks(ntree, fromsock);

	ret = nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
	
	if (ret) {
		if (tonode)
			nodeUpdate(ntree, tonode);

		ntreeUpdateTree(ntree);

		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}

	return ret;
}

static void rna_NodeTree_link_remove(bNodeTree *ntree, ReportList *reports, bNodeLink *link)
{
	if (BLI_findindex(&ntree->links, link) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to locate link in nodetree");
	}
	else {
		nodeRemLink(ntree, link);
		ntreeUpdateTree(ntree);

		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
}

static void rna_NodeTree_link_clear(bNodeTree *ntree)
{
	bNodeLink *link = ntree->links.first;

	while (link) {
		bNodeLink *next_link = link->next;

		nodeRemLink(ntree, link);

		link = next_link;
	}
	ntreeUpdateTree(ntree);

	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static bNodeSocket *rna_NodeTree_input_new(bNodeTree *ntree, ReportList *UNUSED(reports), const char *name, int type)
{
	/* XXX should check if tree is a group here! no good way to do this currently. */
	bNodeSocket *gsock = node_group_add_socket(ntree, name, type, SOCK_IN);
	
	ntree->update |= NTREE_UPDATE_GROUP_IN;
	ntreeUpdateTree(ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	return gsock;
}

static bNodeSocket *rna_NodeTree_output_new(bNodeTree *ntree, ReportList *UNUSED(reports), const char *name, int type)
{
	/* XXX should check if tree is a group here! no good way to do this currently. */
	bNodeSocket *gsock = node_group_add_socket(ntree, name, type, SOCK_OUT);
	
	ntree->update |= NTREE_UPDATE_GROUP_OUT;
	ntreeUpdateTree(ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	return gsock;
}

static bNodeSocket *rna_NodeTree_input_expose(bNodeTree *ntree, ReportList *reports, bNodeSocket *sock, int add_link)
{
	bNode *node;
	bNodeSocket *gsock;
	int index, in_out;
	
	if (!nodeFindNode(ntree, sock, &node, &index, &in_out))
		BKE_reportf(reports, RPT_ERROR, "Unable to locate socket in nodetree");
	else if (in_out != SOCK_IN)
		BKE_reportf(reports, RPT_ERROR, "Socket is not an input");
	else {
		/* XXX should check if tree is a group here! no good way to do this currently. */
		gsock = node_group_add_socket(ntree, sock->name, sock->type, SOCK_IN);
		if (add_link)
			nodeAddLink(ntree, NULL, gsock, node, sock);
		
		ntree->update |= NTREE_UPDATE_GROUP_IN;
		ntreeUpdateTree(ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
		return gsock;
	}
	return NULL;
}

static bNodeSocket *rna_NodeTree_output_expose(bNodeTree *ntree, ReportList *reports, bNodeSocket *sock, int add_link)
{
	bNode *node;
	bNodeSocket *gsock;
	int index, in_out;
	
	if (!nodeFindNode(ntree, sock, &node, &index, &in_out))
		BKE_reportf(reports, RPT_ERROR, "Unable to locate socket in nodetree");
	else if (in_out != SOCK_OUT)
		BKE_reportf(reports, RPT_ERROR, "Socket is not an output");
	else {
		/* XXX should check if tree is a group here! no good way to do this currently. */
		gsock = node_group_add_socket(ntree, sock->name, sock->type, SOCK_OUT);
		if (add_link)
			nodeAddLink(ntree, node, sock, NULL, gsock);
		
		ntree->update |= NTREE_UPDATE_GROUP_OUT;
		ntreeUpdateTree(ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
		return gsock;
	}
	return NULL;
}

static void rna_Mapping_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	init_tex_mapping(node->storage);
	rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeOutputFile_slots_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	bNode *node = ptr->data;
	rna_iterator_listbase_begin(iter, &node->inputs, NULL);
}

static PointerRNA rna_NodeOutputFile_slot_file_get(CollectionPropertyIterator *iter)
{
	PointerRNA ptr;
	bNodeSocket *sock = rna_iterator_listbase_get(iter);
	RNA_pointer_create(iter->parent.id.data, &RNA_NodeOutputFileSlotFile, sock->storage, &ptr);
	return ptr;
}

static PointerRNA rna_NodeOutputFile_slot_layer_get(CollectionPropertyIterator *iter)
{
	PointerRNA ptr;
	bNodeSocket *sock = rna_iterator_listbase_get(iter);
	RNA_pointer_create(iter->parent.id.data, &RNA_NodeOutputFileSlotLayer, sock->storage, &ptr);
	return ptr;
}

static int rna_NodeOutputFileSocket_find_node(bNodeTree *ntree, NodeImageMultiFileSocket *data, bNode **nodep, bNodeSocket **sockp)
{
	bNode *node;
	bNodeSocket *sock;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		for (sock = node->inputs.first; sock; sock = sock->next) {
			NodeImageMultiFileSocket *sockdata = sock->storage;
			if (sockdata == data) {
				*nodep = node;
				*sockp = sock;
				return 1;
			}
		}
	}
	
	*nodep = NULL;
	*sockp = NULL;
	return 0;
}

static void rna_NodeOutputFileSlotFile_path_set(PointerRNA *ptr, const char *value)
{
	bNodeTree *ntree = ptr->id.data;
	NodeImageMultiFileSocket *sockdata = ptr->data;
	bNode *node;
	bNodeSocket *sock;
	
	if (rna_NodeOutputFileSocket_find_node(ntree, sockdata, &node, &sock)) {
		ntreeCompositOutputFileSetPath(node, sock, value);
	}
}

static void rna_NodeOutputFileSlotLayer_name_set(PointerRNA *ptr, const char *value)
{
	bNodeTree *ntree = ptr->id.data;
	NodeImageMultiFileSocket *sockdata = ptr->data;
	bNode *node;
	bNodeSocket *sock;
	
	if (rna_NodeOutputFileSocket_find_node(ntree, sockdata, &node, &sock)) {
		ntreeCompositOutputFileSetLayer(node, sock, value);
	}
}

#else

static EnumPropertyItem prop_image_layer_items[] = {
	{ 0, "PLACEHOLDER",          0, "Placeholder",          ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_scene_layer_items[] = {
	{ 0, "PLACEHOLDER",          0, "Placeholder",          ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_tri_channel_items[] = {
	{ 1, "R", 0, "R", ""},
	{ 2, "G", 0, "G", ""},
	{ 3, "B", 0, "B", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_flip_items[] = {
	{0, "X",  0, "Flip X",     ""},
	{1, "Y",  0, "Flip Y",     ""},
	{2, "XY", 0, "Flip X & Y", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_ycc_items[] = {
	{ 0, "ITUBT601", 0, "ITU 601",  ""},
	{ 1, "ITUBT709", 0, "ITU 709",  ""},
	{ 2, "JFIF",     0, "Jpeg",     ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_glossy_items[] = {
	{SHD_GLOSSY_SHARP,    "SHARP",    0, "Sharp",    ""},
	{SHD_GLOSSY_BECKMANN, "BECKMANN", 0, "Beckmann", ""},
	{SHD_GLOSSY_GGX,      "GGX",      0, "GGX",      ""},
	{0, NULL, 0, NULL, NULL}
};

#define MaxNodes 50000

enum
{
	Category_GroupNode,
	Category_LoopNode,
	Category_LayoutNode,
	Category_ShaderNode,
	Category_CompositorNode,
	Category_TextureNode,
};

typedef struct NodeInfo {
	int defined;
	int category;
	const char *enum_name;
	const char *struct_name;
	const char *base_name;
	int icon;
	const char *ui_name;
	const char *ui_desc;
} NodeInfo;

static NodeInfo nodes[MaxNodes];

static void reg_node(int ID, int category, const char *enum_name, const char *struct_name,
		const char *base_name, const char *ui_name, const char *ui_desc)
{
	NodeInfo *ni = nodes + ID;
	
	ni->defined = 1;
	ni->category = category;
	ni->enum_name = enum_name;
	ni->struct_name = struct_name;
	ni->base_name = base_name;
	ni->ui_name = ui_name;
	ni->ui_desc = ui_desc;
}

static void init(void)
{
	memset(nodes, 0, sizeof nodes);
	
	#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		reg_node(ID, Category_##Category, EnumName, STRINGIFY_ARG(Category##StructName), #Category, UIName, UIDesc);
		
	#include "rna_nodetree_types.h"
	
	reg_node(NODE_GROUP, Category_GroupNode, "GROUP", "NodeGroup", "Node", "Group", "");
	reg_node(NODE_FORLOOP, Category_LoopNode, "FORLOOP", "NodeForLoop", "Node", "ForLoop", "");
	reg_node(NODE_WHILELOOP, Category_LoopNode, "WHILELOOP", "NodeWhileLoop", "Node", "WhileLoop", "");
	reg_node(NODE_FRAME, Category_LayoutNode, "FRAME", "NodeFrame", "Node", "Frame", "");
	reg_node(NODE_REROUTE, Category_LayoutNode, "REROUTE", "NodeReroute", "Node", "Reroute", "");
}

static StructRNA *def_node(BlenderRNA *brna, int node_id)
{
	StructRNA *srna;
	NodeInfo *node = nodes + node_id;
	
	srna = RNA_def_struct(brna, node->struct_name, node->base_name);
	RNA_def_struct_ui_text(srna, node->ui_name, node->ui_desc);
	RNA_def_struct_sdna(srna, "bNode");
	
	return srna;
}

static void alloc_node_type_items(EnumPropertyItem *items, int category)
{
	int i;
	int count = 4;
	EnumPropertyItem *item  = items;
	
	for (i = 0; i < MaxNodes; i++)
		if (nodes[i].defined && nodes[i].category == category)
			count++;
		
	/*item = items = MEM_callocN(count * sizeof(EnumPropertyItem), "alloc_node_type_items");*/
	
	for (i = 0; i < MaxNodes; i++) {
		NodeInfo *node = nodes + i;
		if (node->defined && node->category == category) {
			item->value = i;
			item->identifier = node->enum_name;
			item->icon = node->icon;
			item->name = node->ui_name;
			item->description = node->ui_desc;
		
			item++;
		}
	}

	item->value = NODE_GROUP;
	item->identifier = "GROUP";
	item->icon = 0;
	item->name = "Group";
	item->description = "";
	
	item++;
	
	item->value = NODE_REROUTE;
	item->identifier = "REROUTE";
	item->icon = 0;
	item->name = "Reroute";
	item->description = "";
	
	item++;
	
	/* NOTE!, increase 'count' when adding items here */
	
	memset(item, 0, sizeof(EnumPropertyItem));
}


/* -- Common nodes ---------------------------------------------------------- */

static void def_group(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "NodeTree");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");
}

static void def_forloop(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "NodeTree");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");
}

static void def_whileloop(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "NodeTree");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");

	prop = RNA_def_property(srna, "max_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_range(prop, 0.0f, SHRT_MAX);
	RNA_def_property_ui_text(prop, "Max. Iterations", "Limit for number of iterations");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");
}

static void def_frame(StructRNA *srna)
{
	PropertyRNA *prop; 
	
	RNA_def_struct_sdna_from(srna, "NodeFrame", "storage");
	
	prop = RNA_def_property(srna, "shrink", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_FRAME_SHRINK);
	RNA_def_property_ui_text(prop, "Shrink", "Shrink the frame to minimal bounding box");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "label_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "label_size");
	RNA_def_property_range(prop, 8, 64);
	RNA_def_property_ui_text(prop, "Label Font Size", "Font size to use for displaying the label");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);
}

static void def_math(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_math_items);
	RNA_def_property_ui_text(prop, "Operation", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 1);
	RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0..1 range");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_vector_math(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_vec_math_items);
	RNA_def_property_ui_text(prop, "Operation", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_rgb_curve(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_vector_curve(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_time(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Curve", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_colorramp(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_mix_rgb(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, ramp_blend_items);
	RNA_def_property_ui_text(prop, "Blend Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 1);
	RNA_def_property_ui_text(prop, "Alpha", "Include alpha of second input in this operation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 2);
	RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0..1 range");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_texture(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Texture");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Texture", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "node_output", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_ui_text(prop, "Node Output", "For node-based textures, which output node to use");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}


/* -- Shader Nodes ---------------------------------------------------------- */

static void def_sh_material(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_material_update");

	prop = RNA_def_property(srna, "use_diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_DIFF);
	RNA_def_property_ui_text(prop, "Diffuse", "Material Node outputs Diffuse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_SPEC);
	RNA_def_property_ui_text(prop, "Specular", "Material Node outputs Specular");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "invert_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", SH_NODE_MAT_NEG);
	RNA_def_property_ui_text(prop, "Invert Normal", "Material Node uses inverted normal");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_mapping(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "TexMapping", "storage");

	prop = RNA_def_property(srna, "translation", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	/* Not PROP_XYZ, this is now in radians, no more degrees */
	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Rotation", "");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, "Scale", "");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum", "Minimum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum", "Maximum value for clipping");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	prop = RNA_def_property(srna, "use_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MIN);
	RNA_def_property_ui_text(prop, "Has Minimum", "Whether to use minimum clipping value");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
	
	prop = RNA_def_property(srna, "use_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MAX);
	RNA_def_property_ui_text(prop, "Has Maximum", "Whether to use maximum clipping value");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");
}

static void def_sh_geometry(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeGeometry", "storage");
	
	prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvname");
	RNA_def_property_ui_text(prop, "UV Map", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "colname");
	RNA_def_property_ui_text(prop, "Vertex Color Layer", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_attribute(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeShaderAttribute", "storage");
	
	prop = RNA_def_property(srna, "attribute_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Attribute Name", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "texture_mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "base.tex_mapping");
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Texture Mapping", "Texture coordinate mapping settings");

	prop = RNA_def_property(srna, "color_mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "base.color_mapping");
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Color Mapping", "Color mapping settings");
}

static void def_sh_tex_sky(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexSky", "storage");
	def_sh_tex(srna);
	
	prop = RNA_def_property(srna, "sun_direction", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_ui_text(prop, "Sun Direction", "Direction from where the sun is shining");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "turbidity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Turbidity", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_tex_environment(StructRNA *srna)
{
	static const EnumPropertyItem prop_color_space_items[] = {
		{SHD_COLORSPACE_COLOR, "COLOR", 0, "Color",
		                       "Image contains color data, and will be converted to linear color for rendering"},
		{SHD_COLORSPACE_NONE, "NONE", 0, "Non-Color Data",
		                      "Image contains non-color data, for example a displacement or normal map, "
		                      "and will not be converted"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_projection_items[] = {
		{SHD_PROJ_EQUIRECTANGULAR, "EQUIRECTANGULAR", 0, "Equirectangular",
		                           "Equirectangular or latitude-longitude projection"},
		{SHD_PROJ_MIRROR_BALL, "MIRROR_BALL", 0, "Mirror Ball",
		                       "Projection from an orthographic photo of a mirror ball"},
		{0, NULL, 0, NULL, NULL}
	};
	
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_tex_image_update");

	RNA_def_struct_sdna_from(srna, "NodeTexEnvironment", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_color_space_items);
	RNA_def_property_ui_text(prop, "Color Space", "Image file color space");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "projection", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_projection_items);
	RNA_def_property_ui_text(prop, "Projection", "Projection of the input image");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User",
	                         "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_image(StructRNA *srna)
{
	static const EnumPropertyItem prop_color_space_items[] = {
		{SHD_COLORSPACE_COLOR, "COLOR", 0, "Color",
		                       "Image contains color data, and will be converted to linear color for rendering"},
		{SHD_COLORSPACE_NONE, "NONE", 0, "Non-Color Data",
		                      "Image contains non-color data, for example a displacement or normal map, "
		                      "and will not be converted"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_tex_image_update");

	RNA_def_struct_sdna_from(srna, "NodeTexImage", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_color_space_items);
	RNA_def_property_ui_text(prop, "Color Space", "Image file color space");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, "Image User",
	                         "Parameters defining which layer, pass and frame of the image is displayed");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_gradient(StructRNA *srna)
{
	static EnumPropertyItem prop_gradient_type[] = {
		{SHD_BLEND_LINEAR, "LINEAR", 0, "Linear", "Create a linear progression"},
		{SHD_BLEND_QUADRATIC, "QUADRATIC", 0, "Quadratic", "Create a quadratic progression"},
		{SHD_BLEND_EASING, "EASING", 0, "Easing", "Create a progression easing from one step to the next"},
		{SHD_BLEND_DIAGONAL, "DIAGONAL", 0, "Diagonal", "Create a diagonal progression"},
		{SHD_BLEND_SPHERICAL, "SPHERICAL", 0, "Spherical", "Create a spherical progression"},
		{SHD_BLEND_QUADRATIC_SPHERE, "QUADRATIC_SPHERE", 0, "Quadratic sphere",
		                             "Create a quadratic progression in the shape of a sphere"},
		{SHD_BLEND_RADIAL, "RADIAL", 0, "Radial", "Create a radial progression"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexGradient", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "gradient_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_gradient_type);
	RNA_def_property_ui_text(prop, "Gradient Type", "Style of the color blending");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_noise(StructRNA *srna)
{
	RNA_def_struct_sdna_from(srna, "NodeTexNoise", "storage");
	def_sh_tex(srna);
}

static void def_sh_tex_checker(StructRNA *srna)
{
	RNA_def_struct_sdna_from(srna, "NodeTexChecker", "storage");
	def_sh_tex(srna);
}

static void def_sh_tex_magic(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexMagic", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "turbulence_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "depth");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Depth", "Level of detail in the added turbulent noise");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_musgrave(StructRNA *srna)
{
	static EnumPropertyItem prop_musgrave_type[] = {
		{SHD_MUSGRAVE_MULTIFRACTAL, "MULTIFRACTAL", 0, "Multifractal", ""},
		{SHD_MUSGRAVE_RIDGED_MULTIFRACTAL, "RIDGED_MULTIFRACTAL", 0, "Ridged Multifractal", ""},
		{SHD_MUSGRAVE_HYBRID_MULTIFRACTAL, "HYBRID_MULTIFRACTAL", 0, "Hybrid Multifractal", ""},
		{SHD_MUSGRAVE_FBM, "FBM", 0, "fBM", ""},
		{SHD_MUSGRAVE_HETERO_TERRAIN, "HETERO_TERRAIN", 0, "Hetero Terrain", ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexMusgrave", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "musgrave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "musgrave_type");
	RNA_def_property_enum_items(prop, prop_musgrave_type);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_voronoi(StructRNA *srna)
{
	static EnumPropertyItem prop_coloring_items[] = {
		{SHD_VORONOI_INTENSITY, "INTENSITY", 0, "Intensity", "Only calculate intensity"},
		{SHD_VORONOI_CELLS, "CELLS", 0, "Cells", "Color cells by position"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexVoronoi", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "coloring", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "coloring");
	RNA_def_property_enum_items(prop, prop_coloring_items);
	RNA_def_property_ui_text(prop, "Coloring", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_wave(StructRNA *srna)
{
	static EnumPropertyItem prop_wave_type_items[] = {
		{SHD_WAVE_BANDS, "BANDS", 0, "Bands", "Use standard wave texture in bands"},
		{SHD_WAVE_RINGS, "RINGS", 0, "Rings", "Use wave texture in rings"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexWave", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "wave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "wave_type");
	RNA_def_property_enum_items(prop, prop_wave_type_items);
	RNA_def_property_ui_text(prop, "Wave Type", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_glossy(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_glossy_items);
	RNA_def_property_ui_text(prop, "Distribution", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Compositor Nodes ------------------------------------------------------ */

static void def_cmp_alpha_over(StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* XXX: Tooltip */
	prop = RNA_def_property(srna, "use_premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Convert Premul", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	RNA_def_struct_sdna_from(srna, "NodeTwoFloats", "storage");
	
	prop = RNA_def_property(srna, "premul", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Premul", "Mix Factor");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_hue_saturation(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeHueSat", "storage");
	
	prop = RNA_def_property(srna, "color_hue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hue");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Hue", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sat");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Saturation", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "val");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_blur(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem filter_type_items[] = {
		{R_FILTER_BOX,        "FLAT",       0, "Flat",          ""},
		{R_FILTER_TENT,       "TENT",       0, "Tent",          ""},
		{R_FILTER_QUAD,       "QUAD",       0, "Quadratic",     ""},
		{R_FILTER_CUBIC,      "CUBIC",      0, "Cubic",         ""},
		{R_FILTER_GAUSS,      "GAUSS",      0, "Gaussian",      ""},
		{R_FILTER_FAST_GAUSS, "FAST_GAUSS", 0, "Fast Gaussian", ""},
		{R_FILTER_CATROM,     "CATROM",     0, "Catrom",        ""},
		{R_FILTER_MITCH,      "MITCH",      0, "Mitch",         ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem aspect_correction_type_items[] = {
		{CMP_NODE_BLUR_ASPECT_NONE, "NONE", 0,  "None", ""},
		{CMP_NODE_BLUR_ASPECT_Y,    "Y",    0,  "Y",    ""},
		{CMP_NODE_BLUR_ASPECT_X,    "X",    0,  "X",    ""},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");
	
	prop = RNA_def_property(srna, "size_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizex");
	RNA_def_property_range(prop, 0, 2048);
	RNA_def_property_ui_text(prop, "Size X", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "size_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sizey");
	RNA_def_property_range(prop, 0, 2048);
	RNA_def_property_ui_text(prop, "Size Y", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "relative", 1);
	RNA_def_property_ui_text(prop, "Relative", "Use relative (percent) values to define blur radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "aspect_correction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aspect");
	RNA_def_property_enum_items(prop, aspect_correction_type_items);
	RNA_def_property_ui_text(prop, "Aspect Correction", "Type of aspect correction to use");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "factor_x", PROP_FLOAT, PROP_PERCENTAGE);
	RNA_def_property_float_sdna(prop, NULL, "percentx");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Relative Size X", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "factor_y", PROP_FLOAT, PROP_PERCENTAGE);
	RNA_def_property_float_sdna(prop, NULL, "percenty");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Relative Size Y", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filtertype");
	RNA_def_property_enum_items(prop, filter_type_items);
	RNA_def_property_ui_text(prop, "Filter Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_bokeh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bokeh", 1);
	RNA_def_property_ui_text(prop, "Bokeh", "Use circular filter (slower)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_gamma_correction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gamma", 1);
	RNA_def_property_ui_text(prop, "Gamma", "Apply filter on gamma corrected values");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
}

static void def_cmp_filter(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_filter_items);
	RNA_def_property_ui_text(prop, "Filter Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_map_value(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "TexMapping", "storage");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "loc");
	RNA_def_property_array(prop, 1);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Offset", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_array(prop, 1);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Size", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_min", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MIN);
	RNA_def_property_ui_text(prop, "Use Minimum", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_max", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TEXMAP_CLIP_MAX);
	RNA_def_property_ui_text(prop, "Use Maximum", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_array(prop, 1);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Minimum", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_array(prop, 1);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Maximum", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_vector_blur(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeBlurData", "storage");
	
	prop = RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samples");
	RNA_def_property_range(prop, 1, 256);
	RNA_def_property_ui_text(prop, "Samples", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "speed_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minspeed");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Min Speed",
	                         "Minimum speed for a pixel to be blurred (used to separate background from foreground)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
		
	prop = RNA_def_property(srna, "speed_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxspeed");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Max Speed", "Maximum speed, or zero for none");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac");
	RNA_def_property_range(prop, 0.0, 20.0);
	RNA_def_property_ui_range(prop, 0.0, 2.0, 1.0, 2);
	RNA_def_property_ui_text(prop, "Blur Factor",
	                         "Scaling factor for motion vectors (actually, 'shutter speed', in frames)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_curved", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "curved", 1);
	RNA_def_property_ui_text(prop, "Curved", "Interpolate between frames in a Bezier curve, rather than linearly");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_levels(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem channel_items[] = {
		{1, "COMBINED_RGB", 0, "C", "Combined RGB"},
		{2, "RED", 0, "R", "Red Channel"},
		{3, "GREEN", 0, "G", "Green Channel"},
		{4, "BLUE", 0, "B", "Blue Channel"},
		{5, "LUMINANCE", 0, "L", "Luminance Channel"},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, channel_items);
	RNA_def_property_ui_text(prop, "Channel", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_node_image_user(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "frames");
	RNA_def_property_range(prop, 0, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Frames", "Number of images of a movie to use"); /* copied from the rna_image.c */
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sfra");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	/* copied from the rna_image.c */
	RNA_def_property_ui_text(prop, "Start Frame",
	                         "Global starting frame of the movie/sequence, assuming first picture has a #1");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
	/* copied from the rna_image.c */
	RNA_def_property_ui_text(prop, "Offset", "Offset the number of the frame to use in the animation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cycl", 1);
	RNA_def_property_ui_text(prop, "Cyclic", "Cycle the images in the movie"); /* copied from the rna_image.c */
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_auto_refresh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMA_ANIM_ALWAYS);
	/* copied from the rna_image.c */
	RNA_def_property_ui_text(prop, "Auto-Refresh", "Always refresh image on frame changes");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "layer");
	RNA_def_property_enum_items(prop, prop_image_layer_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Node_image_layer_itemf");
	RNA_def_property_ui_text(prop, "Layer", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_image_layer_update");
}

static void def_cmp_image(StructRNA *srna)
{
	PropertyRNA *prop;
	
#if 0
	static EnumPropertyItem type_items[] = {
		{IMA_SRC_FILE,      "IMAGE",     0, "Image",     ""},
		{IMA_SRC_MOVIE,     "MOVIE",     "Movie",     ""},
		{IMA_SRC_SEQUENCE,  "SEQUENCE",  "Sequence",  ""},
		{IMA_SRC_GENERATED, "GENERATED", "Generated", ""},
		{0, NULL, 0, NULL, NULL}
	};
#endif
	
	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	/* NB: image user properties used in the UI are redefined in def_node_image_user,
	 * to trigger correct updates of the node editor. RNA design problem that prevents
	 * updates from nested structs ...
	 */
	RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
	def_node_image_user(srna);
}

static void def_cmp_render_layers(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Node_scene_set", NULL, NULL);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_ui_text(prop, "Scene", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, prop_scene_layer_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Node_scene_layer_itemf");
	RNA_def_property_ui_text(prop, "Layer", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void rna_def_cmp_output_file_slot_file(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "NodeOutputFileSlotFile", NULL);
	RNA_def_struct_sdna(srna, "NodeImageMultiFileSocket");
	RNA_def_struct_ui_text(srna, "Output File Slot", "Single layer file slot of the file output node");
	
	prop = RNA_def_property(srna, "use_node_format", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_node_format", 1);
	RNA_def_property_ui_text(prop, "Use Node Format", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);
	
	prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ImageFormatSettings");
	
	prop = RNA_def_property(srna, "path", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "path");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NodeOutputFileSlotFile_path_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Path", "Subpath used for this slot");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);
}
static void rna_def_cmp_output_file_slot_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "NodeOutputFileSlotLayer", NULL);
	RNA_def_struct_sdna(srna, "NodeImageMultiFileSocket");
	RNA_def_struct_ui_text(srna, "Output File Layer Slot", "Multilayer slot of the file output node");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "layer");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_NodeOutputFileSlotLayer_name_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "OpenEXR layer name used for this slot");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);
}
static void def_cmp_output_file(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeImageMultiFile", "storage");
	
	prop = RNA_def_property(srna, "base_path", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "base_path");
	RNA_def_property_ui_text(prop, "Base Path", "Base output path for the image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "active_input_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_input");
	RNA_def_property_ui_text(prop, "Active Input Index", "Active input index in details view list");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ImageFormatSettings");
	
	/* XXX using two different collections here for the same basic DNA list!
	 * Details of the output slots depend on whether the node is in Multilayer EXR mode.
	 */
	
	prop = RNA_def_property(srna, "file_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_NodeOutputFile_slots_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end",
	                                  "rna_NodeOutputFile_slot_file_get", NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "NodeOutputFileSlotFile");
	RNA_def_property_ui_text(prop, "File Slots", "");
	
	prop = RNA_def_property(srna, "layer_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_NodeOutputFile_slots_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end",
	                                  "rna_NodeOutputFile_slot_layer_get", NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "NodeOutputFileSlotLayer");
	RNA_def_property_ui_text(prop, "EXR Layer Slots", "");
}

static void def_cmp_dilate_erode(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem type_items[] = {
	    {CMP_NODE_DILATEERODE_STEP,             "STEP",      0, "Step",      ""},
	    {CMP_NODE_DILATEERODE_DISTANCE_THRESH,  "THRESHOLD", 0, "Threshold", ""},
	    {CMP_NODE_DILATEERODE_DISTANCE,         "DISTANCE",  0, "Distance",  ""},
	    {CMP_NODE_DILATEERODE_DISTANCE_FEATHER, "FEATHER",   0, "Feather",  ""},
	    {0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Distance", "Distance to grow/shrink (number of iterations)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_range(prop, -100, 100);
	RNA_def_property_ui_text(prop, "Distance", "Distance to grow/shrink (number of iterations)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* CMP_NODE_DILATEERODE_DISTANCE_THRESH only */
	prop = RNA_def_property(srna, "edge", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, -100, 100);
	RNA_def_property_ui_text(prop, "Edge", "Edge to inset");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeDilateErode", "storage");

	/* CMP_NODE_DILATEERODE_DISTANCE_FEATHER only */
	prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "falloff");
	RNA_def_property_enum_items(prop, proportional_falloff_curve_only_items);
	RNA_def_property_ui_text(prop, "Falloff", "Falloff type the feather");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_scale(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem space_items[] = {
		{CMP_SCALE_RELATIVE, "RELATIVE",   0, "Relative",   ""},
		{CMP_SCALE_ABSOLUTE, "ABSOLUTE",   0, "Absolute",   ""},
		{CMP_SCALE_SCENEPERCENT, "SCENE_SIZE", 0, "Scene Size", ""},
		{CMP_SCALE_RENDERPERCENT, "RENDER_SIZE", 0, "Render Size", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* matching bgpic_camera_frame_items[] */
	static const EnumPropertyItem space_frame_items[] = {
		{0, "STRETCH", 0, "Stretch", ""},
		{CMP_SCALE_RENDERSIZE_FRAME_ASPECT, "FIT", 0, "Fit", ""},
		{CMP_SCALE_RENDERSIZE_FRAME_ASPECT | CMP_SCALE_RENDERSIZE_FRAME_CROP, "CROP", 0, "Crop", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, space_items);
	RNA_def_property_ui_text(prop, "Space", "Coordinate space to scale relative to");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* expose 2 flags as a enum of 3 items */
	prop = RNA_def_property(srna, "frame_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, space_frame_items);
	RNA_def_property_ui_text(prop, "Frame Method", "How the image fits in the camera frame");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_ui_text(prop, "X Offset", "Offset image horizontally (factor of image size)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_ui_text(prop, "Y Offset", "Offset image vertically (factor of image size)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_rotate(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem rotate_items[] = {
		{0, "NEAREST",   0, "Nearest",   ""},
		{1, "BILINEAR",   0, "Bilinear",   ""},
		{2, "BICUBIC", 0, "Bicubic", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, rotate_items);
	RNA_def_property_ui_text(prop, "Filter", "Method to use to filter rotation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_diff_matte(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_float_funcs(prop, NULL, "rna_difference_matte_t1_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tolerance", "Color distances below this threshold are keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_float_funcs(prop, NULL, "rna_difference_matte_t2_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Color distances below this additional threshold are partially keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_color_matte(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

	prop = RNA_def_property(srna, "color_hue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "H", "Hue tolerance for colors to be considered a keying color");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "S", "Saturation Tolerance for the color");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t3");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "V", "Value Tolerance for the color");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_distance_matte(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem color_space_items[] = {
		{1, "RGB", 0, "RGB", "RGB color space"},
		{2, "YCC", 0, "YCC", "YCbCr Suppression"},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

	prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "channel");
	RNA_def_property_enum_items(prop, color_space_items);
	RNA_def_property_ui_text(prop, "Channel", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	
	prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_float_funcs(prop, NULL, "rna_distance_matte_t1_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tolerance", "Color distances below this threshold are keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_float_funcs(prop, NULL, "rna_distance_matte_t2_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Color distances below this additional threshold are partially keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_color_spill(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem channel_items[] = {
		{1, "R", 0, "R", "Red Spill Suppression"},
		{2, "G", 0, "G", "Green Spill Suppression"},
		{3, "B", 0, "B", "Blue Spill Suppression"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem limit_channel_items[] = {
		{1, "R", 0, "R", "Limit by Red"},
		{2, "G", 0, "G", "Limit by Green"},
		{3, "B", 0, "B", "Limit by Blue"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem algorithm_items[] = {
		{0, "SIMPLE", 0, "Simple", "Simple Limit Algorithm"},
		{1, "AVERAGE", 0, "Average", "Average Limit Algorithm"},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, channel_items);
	RNA_def_property_ui_text(prop, "Channel", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, algorithm_items);
	RNA_def_property_ui_text(prop, "Algorithm", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeColorspill", "storage");

	prop = RNA_def_property(srna, "limit_channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "limchan");
	RNA_def_property_enum_items(prop, limit_channel_items);
	RNA_def_property_ui_text(prop, "Limit Channel", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "limscale");
	RNA_def_property_range(prop, 0.5f, 1.5f);
	RNA_def_property_ui_text(prop, "Ratio", "Scale limit by value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_unspill", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "unspill", 0);
	RNA_def_property_ui_text(prop, "Unspill", "Compensate all channels (differently) by hand");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "unspill_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "uspillr");
	RNA_def_property_range(prop, 0.0f, 1.5f);
	RNA_def_property_ui_text(prop, "R", "Red spillmap scale");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "unspill_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "uspillg");
	RNA_def_property_range(prop, 0.0f, 1.5f);
	RNA_def_property_ui_text(prop, "G", "Green spillmap scale");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "unspill_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "uspillb");
	RNA_def_property_range(prop, 0.0f, 1.5f);
	RNA_def_property_ui_text(prop, "B", "Blue spillmap scale");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_luma_matte(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "limit_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t1_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "High", "Values higher than this setting are 100% opaque");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "limit_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t2_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Low", "Values lower than this setting are 100% keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_chroma_matte(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");
	
	prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t1_set", NULL);
	RNA_def_property_range(prop, DEG2RADF(1.0f), DEG2RADF(80.0f));
	RNA_def_property_ui_text(prop, "Acceptance", "Tolerance for a color to be considered a keying color");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t2_set", NULL);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(30.0f));
	RNA_def_property_ui_text(prop, "Cutoff", "Tolerance below which colors will be considered as exact matches");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fsize");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Lift", "Alpha lift");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fstrength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Alpha falloff");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "shadow_adjust", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t3");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shadow Adjust", "Adjusts the brightness of any shadows captured");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_channel_matte(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem color_space_items[] = {
		{CMP_NODE_CHANNEL_MATTE_CS_RGB, "RGB", 0, "RGB",   "RGB Color Space"},
		{CMP_NODE_CHANNEL_MATTE_CS_HSV, "HSV", 0, "HSV",   "HSV Color Space"},
		{CMP_NODE_CHANNEL_MATTE_CS_YUV, "YUV", 0, "YUV",   "YUV Color Space"},
		{CMP_NODE_CHANNEL_MATTE_CS_YCC, "YCC", 0, "YCbCr", "YCbCr Color Space"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem algorithm_items[] = {
		{0, "SINGLE", 0, "Single", "Limit by single channel"},
		{1, "MAX", 0, "Max", "Limit by max of other channels "},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "color_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, color_space_items);
	RNA_def_property_ui_text(prop, "Color Space", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "matte_channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, prop_tri_channel_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Node_channel_itemf");
	RNA_def_property_ui_text(prop, "Channel", "Channel used to determine matte");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeChroma", "storage");

	prop = RNA_def_property(srna, "limit_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "algorithm");
	RNA_def_property_enum_items(prop, algorithm_items);
	RNA_def_property_ui_text(prop, "Algorithm", "Algorithm to use to limit channel");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "limit_channel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "channel");
	RNA_def_property_enum_items(prop, prop_tri_channel_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Node_channel_itemf");
	RNA_def_property_ui_text(prop, "Limit Channel", "Limit by this channel's value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "limit_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t1");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t1_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "High", "Values higher than this setting are 100% opaque");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "limit_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "t2");
	RNA_def_property_float_funcs(prop, NULL, "rna_Matte_t2_set", NULL);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Low", "Values lower than this setting are 100% keyed");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_flip(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_flip_items);
	RNA_def_property_ui_text(prop, "Axis", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_splitviewer(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem axis_items[] = {
		{0, "X",  0, "X",     ""},
		{1, "Y",  0, "Y",     ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "factor", PROP_INT, PROP_FACTOR);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_id_mask(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_range(prop, 0, 32767);
	RNA_def_property_ui_text(prop, "Index", "Pass index number to convert to alpha");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_antialiasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 0);
	RNA_def_property_ui_text(prop, "Anti-Aliasing", "Apply an anti-aliasing filter to the mask");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_double_edge_mask(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem BufEdgeMode_items[] = {
		{0, "BLEED_OUT",  0, "Bleed Out",     "Allow mask pixels to bleed along edges"},
		{1, "KEEP_IN",  0, "Keep In",     "Restrict mask pixels from touching edges"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem InnerEdgeMode_items[] = {
		{0, "ALL", 0, "All", "All pixels on inner mask edge are considered during mask calculation"},
		{1, "ADJACENT_ONLY", 0, "Adjacent Only",
		 "Only inner mask pixels adjacent to outer mask pixels are considered during mask calculation"},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "inner_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom2");
	RNA_def_property_enum_items(prop, InnerEdgeMode_items);
	RNA_def_property_ui_text(prop, "Inner Edge Mode", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "edge_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, BufEdgeMode_items);
	RNA_def_property_ui_text(prop, "Buffer Edge Mode", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_map_uv(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "alpha", PROP_INT, PROP_FACTOR);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_defocus(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem bokeh_items[] = {
		{8, "OCTAGON",  0, "Octagonal",  "8 sides"},
		{7, "HEPTAGON", 0, "Heptagonal", "7 sides"},
		{6, "HEXAGON",  0, "Hexagonal",  "6 sides"},
		{5, "PENTAGON", 0, "Pentagonal", "5 sides"},
		{4, "SQUARE",   0, "Square",     "4 sides"},
		{3, "TRIANGLE", 0, "Triangular", "3 sides"},
		{0, "CIRCLE",   0, "Circular",   ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	RNA_def_struct_sdna_from(srna, "NodeDefocus", "storage");
	
	prop = RNA_def_property(srna, "bokeh", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bktype");
	RNA_def_property_enum_items(prop, bokeh_items);
	RNA_def_property_ui_text(prop, "Bokeh Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* TODO: angle in degrees */
	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_range(prop, 0.0f, DEG2RADF(90.0f));
	RNA_def_property_ui_text(prop, "Angle", "Bokeh shape rotation offset");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_gamma_correction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gamco", 1);
	RNA_def_property_ui_text(prop, "Gamma Correction", "Enable gamma correction before and after main process");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* TODO */
	prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fstop");
	RNA_def_property_range(prop, 0.0f, 128.0f);
	RNA_def_property_ui_text(prop, "fStop",
	                         "Amount of focal blur, 128=infinity=perfect focus, half the value doubles "
	                         "the blur radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "blur_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxblur");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Max Blur", "Blur limit, maximum CoC radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bthresh");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Threshold",
	                         "CoC radius threshold, prevents background bleed on in-focus midground, 0=off");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "preview", 1);
	RNA_def_property_ui_text(prop, "Preview", "Enable low quality mode, useful for preview");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "no_zbuf", 1);
	RNA_def_property_ui_text(prop, "Use Z-Buffer",
	                         "Disable when using an image as input instead of actual z-buffer "
	                         "(auto enabled if node not image based, eg. time node)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "z_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Z-Scale",
	                         "Scale the Z input when not using a z-buffer, controls maximum blur designated "
	                         "by the color white or input value 1");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_invert(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "invert_rgb", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_CHAN_RGB);
	RNA_def_property_ui_text(prop, "RGB", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "invert_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_CHAN_A);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_crop(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "use_crop_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Crop Image Size", "Whether to crop the size of the input image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom2", 1);
	RNA_def_property_ui_text(prop, "Relative", "Use relative values to crop image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeTwoXYs", "storage");

	prop = RNA_def_property(srna, "min_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "x1");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "X1", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "max_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "x2");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "X2", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "min_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "y1");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "Y1", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "max_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "y2");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "Y2", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rel_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac_x1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "X1", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rel_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac_x2");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "X2", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rel_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac_y1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Y1", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rel_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fac_y2");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Y2", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_dblur(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeDBlurData", "storage");
	
	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "iter");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Iterations", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "wrap", 1);
	RNA_def_property_ui_text(prop, "Wrap", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "center_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "center_x");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Center X", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "center_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "center_y");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Center Y", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Distance", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, 0.0f, DEG2RADF(360.0f));
	RNA_def_property_ui_text(prop, "Angle", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "spin", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "spin");
	RNA_def_property_range(prop, DEG2RADF(-360.0f), DEG2RADF(360.0f));
	RNA_def_property_ui_text(prop, "Spin", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zoom");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Zoom", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bilateral_blur(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeBilateralBlurData", "storage");
	
	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "iter");
	RNA_def_property_range(prop, 1, 128);
	RNA_def_property_ui_text(prop, "Iterations", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "sigma_color", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sigma_color");
	RNA_def_property_range(prop, 0.01f, 3.0f);
	RNA_def_property_ui_text(prop, "Color Sigma", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "sigma_space", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sigma_space");
	RNA_def_property_range(prop, 0.01f, 30.0f);
	RNA_def_property_ui_text(prop, "Space Sigma", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_premul_key(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] = {
		{0, "KEY_TO_PREMUL", 0, "Key to Premul", ""},
		{1, "PREMUL_TO_KEY", 0, "Premul to Key", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Mapping", "Conversion between premultiplied alpha and key alpha");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
}

static void def_cmp_glare(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] = {
		{3, "GHOSTS",      0, "Ghosts",      ""},
		{2, "STREAKS",     0, "Streaks",     ""},
		{1, "FOG_GLOW",    0, "Fog Glow",    ""},
		{0, "SIMPLE_STAR", 0, "Simple Star", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem quality_items[] = {
		{0, "HIGH",   0, "High",   ""},
		{1, "MEDIUM", 0, "Medium", ""},
		{2, "LOW",    0, "Low",    ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	RNA_def_struct_sdna_from(srna, "NodeGlare", "storage");
	
	prop = RNA_def_property(srna, "glare_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Glare Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "quality", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "quality");
	RNA_def_property_enum_items(prop, quality_items);
	RNA_def_property_ui_text(prop, "Quality",
	                         "If not set to high quality, the effect will be applied to a low-res copy "
	                         "of the source image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "iter");
	RNA_def_property_range(prop, 2, 5);
	RNA_def_property_ui_text(prop, "Iterations", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "color_modulation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colmod");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color Modulation",
	                         "Amount of Color Modulation, modulates colors of streaks and ghosts for "
	                         "a spectral dispersion effect");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "mix", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mix");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Mix",
	                         "-1 is original image only, 0 is exact 50/50 mix, 1 is processed image only");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "threshold");
	RNA_def_property_range(prop, 0.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Threshold",
	                         "The glare filter will only be applied to pixels brighter than this value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "streaks", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "angle");
	RNA_def_property_range(prop, 2, 16);
	RNA_def_property_ui_text(prop, "Streaks", "Total number of streaks");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "angle_offset", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "angle_ofs");
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_text(prop, "Angle Offset", "Streak angle offset");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "fade", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fade");
	RNA_def_property_range(prop, 0.75f, 1.0f);
	RNA_def_property_ui_text(prop, "Fade", "Streak fade-out factor");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_rotate_45", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "angle", 0);
	RNA_def_property_ui_text(prop, "Rotate 45", "Simple star filter: add 45 degree rotation offset");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "size");
	RNA_def_property_range(prop, 6, 9);
	RNA_def_property_ui_text(prop, "Size",
	                         "Glow/glare size (not actual size; relative to initial size of bright area of pixels)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	/* TODO */
}

static void def_cmp_tonemap(StructRNA *srna)
{
	PropertyRNA *prop;
	
	static EnumPropertyItem type_items[] = {
		{1, "RD_PHOTORECEPTOR", 0, "R/D Photoreceptor", ""},
		{0, "RH_SIMPLE",        0, "Rh Simple",         ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	RNA_def_struct_sdna_from(srna, "NodeTonemap", "storage");
	
	prop = RNA_def_property(srna, "tonemap_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Tonemap Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "key", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "key");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Key", "The value the average luminance is mapped to");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, 0.001f, 10.0f);
	RNA_def_property_ui_text(prop, "Offset",
	                         "Normally always 1, but can be used as an extra control to alter the brightness curve");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gamma");
	RNA_def_property_range(prop, 0.001f, 3.0f);
	RNA_def_property_ui_text(prop, "Gamma", "If not used, set to 1");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "f");
	RNA_def_property_range(prop, -8.0f, 8.0f);
	RNA_def_property_ui_text(prop, "Intensity", "If less than zero, darkens image; otherwise, makes it brighter");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "m");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Contrast", "Set to 0 to use estimate from input image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "adaptation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "a");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Adaptation", "If 0, global; if 1, based on pixel intensity");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "correction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "c");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color Correction", "If 0, same for all channels; if 1, each independent");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_lensdist(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeLensDist", "storage");
	
	prop = RNA_def_property(srna, "use_projector", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "proj", 1);
	RNA_def_property_ui_text(prop, "Projector",
	                         "Enable/disable projector mode (the effect is applied in horizontal direction only)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_jitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "jit", 1);
	RNA_def_property_ui_text(prop, "Jitter", "Enable/disable jittering (faster, but also noisier)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "use_fit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "fit", 1);
	RNA_def_property_ui_text(prop, "Fit",
	                         "For positive distortion factor only: scale image such that black areas are not visible");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_colorbalance(StructRNA *srna)
{
	PropertyRNA *prop;
	static float default_1[3] = {1.f, 1.f, 1.f};
	
	static EnumPropertyItem type_items[] = {
		{0, "LIFT_GAMMA_GAIN", 0, "Lift/Gamma/Gain", ""},
		{1, "OFFSET_POWER_SLOPE", 0, "Offset/Power/Slope (ASC-CDL)", "ASC-CDL standard color correction"},
		{0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "correction_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Correction Formula", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	RNA_def_struct_sdna_from(srna, "NodeColorBalance", "storage");
	
	prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "lift");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Lift", "Correction for Shadows");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gamma");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Gamma", "Correction for Midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gain");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Gain", "Correction for Highlights");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "lift");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Offset", "Correction for Shadows");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gamma");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_range(prop, 0.f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Power", "Correction for Midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "slope", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gain");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_range(prop, 0.f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Slope", "Correction for Highlights");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_huecorrect(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Mapping", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_zcombine(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 0);
	RNA_def_property_ui_text(prop, "Use Alpha", "Take Alpha channel into account when doing the Z operation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_ycc(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_ycc_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_movieclip(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "MovieClipUser", "storage");
}

static void def_cmp_stabilize2d(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem filter_type_items[] = {
		{0, "NEAREST",   0, "Nearest",   ""},
		{1, "BILINEAR",   0, "Bilinear",   ""},
		{2, "BICUBIC", 0, "Bicubic", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, filter_type_items);
	RNA_def_property_ui_text(prop, "Filter", "Method to use to filter stabilization");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_moviedistortion(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem distortion_type_items[] = {
		{0, "UNDISTORT",   0, "Undistort",   ""},
		{1, "DISTORT", 0, "Distort", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "distortion_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, distortion_type_items);
	RNA_def_property_ui_text(prop, "Distortion", "Distortion to use to filter image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_mask(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Mask");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mask", "");

	prop = RNA_def_property(srna, "use_antialiasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_NODEFLAG_MASK_AA);
	RNA_def_property_ui_text(prop, "Anti-Alias", "Apply an anti-aliasing filter to the mask");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_feather", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "custom1", CMP_NODEFLAG_MASK_NO_FEATHER);
	RNA_def_property_ui_text(prop, "Feather", "Use feather information from the mask");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void dev_cmd_transform(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem filter_type_items[] = {
		{0, "NEAREST",   0, "Nearest",   ""},
		{1, "BILINEAR",   0, "Bilinear",   ""},
		{2, "BICUBIC", 0, "Bicubic", ""},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, filter_type_items);
	RNA_def_property_ui_text(prop, "Filter", "Method to use to filter transform");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Compositor Nodes ------------------------------------------------------ */

EnumPropertyItem node_masktype_items[] = {
	{0, "ADD",           0, "Add",           ""},
	{1, "SUBTRACT",      0, "Subtract",      ""},
	{2, "MULTIPLY",      0, "Multiply",      ""},
	{3, "NOT",           0, "Not",           ""},
	{0, NULL, 0, NULL, NULL}
};

static void def_cmp_boxmask(StructRNA *srna) 
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "mask_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_masktype_items);
	RNA_def_property_ui_text(prop, "Mask type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeBoxMask", "storage");

	prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -1.0f, 2.0f);
	RNA_def_property_ui_text(prop, "X", "X position of the middle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "y");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -1.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Y", "Y position of the middle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_float_default(prop, 0.3f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Width", "Width of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_float_default(prop, 0.2f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Height", "Height of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Rotation", "Rotation angle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_ellipsemask(StructRNA *srna)
{
	PropertyRNA *prop;
	prop = RNA_def_property(srna, "mask_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_masktype_items);
	RNA_def_property_ui_text(prop, "Mask type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeEllipseMask", "storage");

	prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -1.0f, 2.0f);
	RNA_def_property_ui_text(prop, "X", "X position of the middle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "y");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -1.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Y", "Y position of the middle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_float_default(prop, 0.3f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Width", "Width of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_float_default(prop, 0.2f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Height", "Height of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Rotation", "Rotation angle of the box");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bokehblur(StructRNA *srna)
{
	PropertyRNA *prop;
	prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, 0.0f, 128.0f);
	RNA_def_property_ui_text(prop, "fStop",
	                         "Amount of focal blur, 128=infinity=perfect focus, half the value doubles "
	                         "the blur radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "blur_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Max Blur", "Blur limit, maximum CoC radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
}

static void def_cmp_bokehimage(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeBokehImage", "storage");

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -720.0f, 720.0f);
	RNA_def_property_ui_text(prop, "Angle", "Angle of the bokeh");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "flaps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "flaps");
	RNA_def_property_int_default(prop, 5);
	RNA_def_property_range(prop, 3, 24);
	RNA_def_property_ui_text(prop, "Flaps", "Number of flaps");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rounding", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rounding");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Rounding", "Level of rounding of the bokeh");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "catadioptric", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "catadioptric");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Catadioptric", "Level of catadioptric of the bokeh");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "shift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lensshift");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Lens shift", "Shift of the lens components");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

}

static void def_cmp_switch(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "check", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 0);
	RNA_def_property_ui_text(prop, "Switch", "Off: first socket, On: second socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_colorcorrection(StructRNA *srna)
{
	PropertyRNA *prop;
	prop = RNA_def_property(srna, "red", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_boolean_default(prop, TRUE);
	RNA_def_property_ui_text(prop, "Red", "Red channel active");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "green", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 2);
	RNA_def_property_boolean_default(prop, TRUE);
	RNA_def_property_ui_text(prop, "Green", "Green channel active");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "blue", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 4);
	RNA_def_property_boolean_default(prop, TRUE);
	RNA_def_property_ui_text(prop, "Blue", "Blue channel active");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	RNA_def_struct_sdna_from(srna, "NodeColorCorrection", "storage");
	
	prop = RNA_def_property(srna, "midtones_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "startmidtones");
	RNA_def_property_float_default(prop, 0.2f);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Midtones Start", "Start of midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "midtones_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "endmidtones");
	RNA_def_property_float_default(prop, 0.7f);
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Midtones End", "End of midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "master_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "master.saturation");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Master Saturation", "Master saturation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "master_contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "master.contrast");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Master Contrast", "Master contrast");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "master_gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "master.gamma");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Master Gamma", "Master gamma");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "master_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "master.gain");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Master Gain", "Master gain");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "master_lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "master.lift");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_text(prop, "Master Lift", "Master lift");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

//
	prop = RNA_def_property(srna, "shadows_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shadows.saturation");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Shadows Saturation", "Shadows saturation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "shadows_contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shadows.contrast");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Shadows Contrast", "Shadows contrast");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "shadows_gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shadows.gamma");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Shadows Gamma", "Shadows gamma");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "shadows_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shadows.gain");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Shadows Gain", "Shadows gain");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "shadows_lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shadows.lift");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_text(prop, "Shadows Lift", "Shadows lift");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
//
	prop = RNA_def_property(srna, "midtones_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midtones.saturation");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Midtones Saturation", "Midtones saturation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "midtones_contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midtones.contrast");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Midtones Contrast", "Midtones contrast");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "midtones_gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midtones.gamma");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Midtones Gamma", "Midtones gamma");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "midtones_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midtones.gain");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Midtones Gain", "Midtones gain");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "midtones_lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "midtones.lift");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_text(prop, "Midtones Lift", "Midtones lift");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
//
	prop = RNA_def_property(srna, "highlights_saturation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "highlights.saturation");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Highlights Saturation", "Highlights saturation");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "highlights_contrast", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "highlights.contrast");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Highlights Contrast", "Highlights contrast");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "highlights_gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "highlights.gamma");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Highlights Gamma", "Highlights gamma");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "highlights_gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "highlights.gain");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0, 4);
	RNA_def_property_ui_text(prop, "Highlights Gain", "Highlights gain");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "highlights_lift", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "highlights.lift");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, -1, 1);
	RNA_def_property_ui_text(prop, "Highlights Lift", "Highlights lift");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_viewer(StructRNA *srna)
{
	PropertyRNA *prop;
	static EnumPropertyItem tileorder_items[] = {
		{0, "CENTEROUT",      0, "Center",         "Expand from center"},
		{1, "RANDOM",         0, "Random",         "Random tiles"},
		{2, "BOTTOMUP",       0, "Bottom up",      "Expand from bottom"},
		{3, "RULE_OF_THIRDS", 0, "Rule of thirds", "Expand from 9 places"},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "tile_order", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, tileorder_items);
	RNA_def_property_ui_text(prop, "Tile order", "Tile order");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "center_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "center_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_keyingscreen(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeKeyingScreenData", "storage");

	prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "tracking_object");
	RNA_def_property_ui_text(prop, "Tracking Object", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_keying(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeKeyingData", "storage");

	prop = RNA_def_property(srna, "screen_balance", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "screen_balance");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Screen Balance", "Balance between two non-primary channels primary channel is comparing against");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "despill_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "despill_factor");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Despill", "Factor of despilling screen color from image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "clip_black", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "clip_black");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clip Black", "Value of non-scaled matte pixel which considers as fully background pixel");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "clip_white", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "clip_white");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clip White", "Value of non-scaled matte pixel which considers as fully foreground pixel");
	RNA_def_property_update(prop, NC_NODE|NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "blur_pre", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "blur_pre");
	RNA_def_property_range(prop, 0, 2048);
	RNA_def_property_ui_text(prop, "Pre Blur", "Chroma pre-blur size which applies before running keyer");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "blur_post", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "blur_post");
	RNA_def_property_range(prop, 0, 2048);
	RNA_def_property_ui_text(prop, "Post Blur", "Matte blur size which applies after clipping and dilate/eroding");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "dilate_distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dilate_distance");
	RNA_def_property_range(prop, -100, 100);
	RNA_def_property_ui_text(prop, "Dilate/Erode", "Matte dilate/erode side");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "edge_kernel_radius", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "edge_kernel_radius");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Edge Kernel Radius", "Radius of kernel used to detect whether pixel belongs to edge");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "edge_kernel_tolerance", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "edge_kernel_tolerance");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Edge Kernel Tolerance", "Tolerance to pixels inside kernel which are treating as belonging to the same plane");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "feather_falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "feather_falloff");
	RNA_def_property_enum_items(prop, proportional_falloff_curve_only_items);
	RNA_def_property_ui_text(prop, "Feather Falloff", "Falloff type the feather");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "feather_distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "feather_distance");
	RNA_def_property_range(prop, -100, 100);
	RNA_def_property_ui_text(prop, "Feather Distance", "Distance to grow/shrink the feather");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_trackpos(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Relative", "Return relative position to first track's marker");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeTrackPosData", "storage");

	prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "tracking_object");
	RNA_def_property_ui_text(prop, "Tracking Object", "");
	RNA_def_property_update(prop, NC_NODE|NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "track_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "track_name");
	RNA_def_property_ui_text(prop, "Track", "");
	RNA_def_property_update(prop, NC_NODE|NA_EDITED, "rna_Node_update");
}

/* -- Texture Nodes --------------------------------------------------------- */

static void def_tex_output(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "TexNodeOutput", "storage");
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Output Name", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_image(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* is this supposed to be exposed? not sure.. */
#if 0
	prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "storage");
	RNA_def_property_struct_type(prop, "ImageUser");
	RNA_def_property_ui_text(prop, "Settings", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
#endif
}

static void def_tex_bricks(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Offset Amount", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom1");
	RNA_def_property_range(prop, 2, 99);
	RNA_def_property_ui_text(prop, "Offset Frequency", "Offset every N rows");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_range(prop, 0.0f, 99.0f);
	RNA_def_property_ui_text(prop, "Squash Amount", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_range(prop, 2, 99);
	RNA_def_property_ui_text(prop, "Squash Frequency", "Squash every N rows");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -------------------------------------------------------------------------- */

static EnumPropertyItem shader_node_type_items[MaxNodes];
static void rna_def_shader_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	alloc_node_type_items(shader_node_type_items, Category_ShaderNode);

	srna = RNA_def_struct(brna, "ShaderNode", "Node");
	RNA_def_struct_ui_text(srna, "Shader Node", "Material shader node");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, shader_node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static EnumPropertyItem compositor_node_type_items[MaxNodes];
static void rna_def_compositor_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	alloc_node_type_items(compositor_node_type_items, Category_CompositorNode);
	
	srna = RNA_def_struct(brna, "CompositorNode", "Node");
	RNA_def_struct_ui_text(srna, "Compositor Node", "");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, compositor_node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

static EnumPropertyItem texture_node_type_items[MaxNodes];
static void rna_def_texture_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	alloc_node_type_items(texture_node_type_items, Category_TextureNode);
	
	srna = RNA_def_struct(brna, "TextureNode", "Node");
	RNA_def_struct_ui_text(srna, "Texture Node", "");
	RNA_def_struct_sdna(srna, "bNode");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, texture_node_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
}

/* -------------------------------------------------------------------------- */

static void rna_def_nodetree_link_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "NodeLinks");
	srna = RNA_def_struct(brna, "NodeLinks", NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Node Links", "Collection of Node Links");

	func = RNA_def_function(srna, "new", "rna_NodeTree_link_new");
	RNA_def_function_ui_description(func, "Add a node link to this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "input", "NodeSocket", "", "The input socket");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "output", "NodeSocket", "", "The output socket");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return */
	parm = RNA_def_pointer(func, "link", "NodeLink", "", "New node link");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_link_remove");
	RNA_def_function_ui_description(func, "remove a node link from the node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "link", "NodeLink", "", "The node link to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_link_clear");
	RNA_def_function_ui_description(func, "remove all node links from the node tree");
}

/* shared between all note tree types*/
static void rna_def_nodetree_active_api(StructRNA *srna, PropertyRNA *cprop)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_pointer_funcs(prop, "rna_NodeTree_active_node_get", "rna_NodeTree_active_node_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Node", "Active node in this tree");
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);
}

static void rna_def_composite_nodetree_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "CompositorNodes");
	srna = RNA_def_struct(brna, "CompositorNodes", NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Compositor Nodes", "Collection of Compositor Nodes");

	func = RNA_def_function(srna, "new", "rna_NodeTree_node_composite_new");
	RNA_def_function_ui_description(func, "Add a node to this node tree");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_enum(func, "type", compositor_node_type_items, 0, "Type", "Type of node to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_pointer(func, "group", "NodeTree", "", "The group tree");
	/* return value */
	parm = RNA_def_pointer(func, "node", "Node", "", "New node");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_node_remove");
	RNA_def_function_ui_description(func, "Remove a node from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "node", "Node", "", "The node to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_node_clear");
	RNA_def_function_ui_description(func, "Remove all nodes from this node tree");

	rna_def_nodetree_active_api(srna, cprop);
}

static void rna_def_shader_nodetree_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "ShaderNodes");
	srna = RNA_def_struct(brna, "ShaderNodes", NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Shader Nodes", "Collection of Shader Nodes");

	func = RNA_def_function(srna, "new", "rna_NodeTree_node_new");
	RNA_def_function_ui_description(func, "Add a node to this node tree");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_enum(func, "type", shader_node_type_items, 0, "Type", "Type of node to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_pointer(func, "group", "NodeTree", "", "The group tree");
	/* return value */
	parm = RNA_def_pointer(func, "node", "Node", "", "New node");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_node_remove");
	RNA_def_function_ui_description(func, "Remove a node from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "node", "Node", "", "The node to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_node_clear");
	RNA_def_function_ui_description(func, "Remove all nodes from this node tree");

	rna_def_nodetree_active_api(srna, cprop);
}

static void rna_def_texture_nodetree_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "TextureNodes");
	srna = RNA_def_struct(brna, "TextureNodes", NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Texture Nodes", "Collection of Texture Nodes");

	func = RNA_def_function(srna, "new", "rna_NodeTree_node_texture_new");
	RNA_def_function_ui_description(func, "Add a node to this node tree");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_enum(func, "type", texture_node_type_items, 0, "Type", "Type of node to add");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_pointer(func, "group", "NodeTree", "", "The group tree");
	/* return value */
	parm = RNA_def_pointer(func, "node", "Node", "", "New node");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_node_remove");
	RNA_def_function_ui_description(func, "Remove a node from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "node", "Node", "", "The node to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_node_clear");
	RNA_def_function_ui_description(func, "Remove all nodes from this node tree");

	rna_def_nodetree_active_api(srna, cprop);
}

static void rna_def_node_socket(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NodeSocket", NULL);
	RNA_def_struct_ui_text(srna, "Node Socket", "Input or output socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	RNA_def_struct_refine_func(srna, "rna_NodeSocket_refine");
	RNA_def_struct_ui_icon(srna, ICON_PLUG);
	RNA_def_struct_path_func(srna, "rna_NodeSocket_path");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, node_socket_type_items);
	RNA_def_property_enum_default(prop, 0);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Node Socket type");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	/* XXX must be editable for group sockets. if necessary use a special rna definition for these */
/*	RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
	RNA_def_property_ui_text(prop, "Name", "Socket name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroupSocket_update");

	prop = RNA_def_property(srna, "group_socket", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "groupsock");
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Group Socket",
	                         "For group nodes, the group input or output socket this corresponds to");

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SOCK_COLLAPSED);
	RNA_def_property_ui_text(prop, "Expanded", "Socket links are expanded in the user interface");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);
}

static void rna_def_node_socket_subtype(BlenderRNA *brna, int type, int subtype, const char *name, const char *ui_name)
{
	StructRNA *srna;
	PropertyRNA *prop = NULL;
	PropertySubType propsubtype = PROP_NONE;
	
	#define SUBTYPE(socktype, stypename, id, idname) { PROP_##id, #socktype "_" #id, 0, #idname, ""},
	static EnumPropertyItem subtype_items[] = {
		NODE_DEFINE_SUBTYPES
		{0, NULL, 0, NULL, NULL}
	};
	#undef SUBTYPE

	#define SUBTYPE(socktype, stypename, id, idname)    if (subtype == (PROP_##id)) propsubtype = PROP_##id;
	NODE_DEFINE_SUBTYPES
	#undef SUBTYPE
	
	srna = RNA_def_struct(brna, name, "NodeSocket");
	RNA_def_struct_ui_text(srna, ui_name, "Input or output socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	RNA_def_struct_ui_icon(srna, ICON_PLUG);
	RNA_def_struct_path_func(srna, "rna_NodeSocket_path");
	
	switch (type) {
		case SOCK_INT:
			RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");

			prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
			RNA_def_property_enum_sdna(prop, NULL, "subtype");
			RNA_def_property_enum_items(prop, subtype_items);
			RNA_def_property_ui_text(prop, "Subtype", "Subtype defining the socket value details");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

			prop = RNA_def_property(srna, "default_value", PROP_INT, propsubtype);
			RNA_def_property_int_sdna(prop, NULL, "value");
			RNA_def_property_int_funcs(prop, NULL, NULL, "rna_NodeSocketInt_range");
			RNA_def_property_ui_text(prop, "Default Value", "");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");
			break;
		case SOCK_FLOAT:
			RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");

			prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
			RNA_def_property_enum_sdna(prop, NULL, "subtype");
			RNA_def_property_enum_items(prop, subtype_items);
			RNA_def_property_ui_text(prop, "Subtype", "Subtype defining the socket value details");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

			prop = RNA_def_property(srna, "default_value", PROP_FLOAT, propsubtype);
			RNA_def_property_float_sdna(prop, NULL, "value");
			RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketFloat_range");
			RNA_def_property_ui_text(prop, "Default Value", "");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");
			break;
		case SOCK_BOOLEAN:
			RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");
		
			prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
			RNA_def_property_boolean_sdna(prop, NULL, "value", 1);
			RNA_def_property_ui_text(prop, "Default Value", "");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");
			break;
		case SOCK_VECTOR:
			RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");

			prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
			RNA_def_property_enum_sdna(prop, NULL, "subtype");
			RNA_def_property_enum_items(prop, subtype_items);
			RNA_def_property_ui_text(prop, "Subtype", "Subtype defining the socket value details");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

			prop = RNA_def_property(srna, "default_value", PROP_FLOAT, propsubtype);
			RNA_def_property_float_sdna(prop, NULL, "value");
			RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketVector_range");
			RNA_def_property_ui_text(prop, "Default Value", "");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");
			break;
		case SOCK_RGBA:
			RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");

			prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
			RNA_def_property_float_sdna(prop, NULL, "value");
			RNA_def_property_ui_text(prop, "Default Value", "");
			RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");
			break;
	}
	
	/* XXX need to reset the from-type here, so subtype subclasses cast correctly! (RNA bug) */
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "Node", NULL);
	RNA_def_struct_ui_text(srna, "Node", "Node in a node tree");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_ui_icon(srna, ICON_NODE);
	RNA_def_struct_refine_func(srna, "rna_Node_refine");
	RNA_def_struct_path_func(srna, "rna_Node_path");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "locx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -10000.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique node identifier");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Node_name_set");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "label");
	RNA_def_property_ui_text(prop, "Label", "Optional custom node label");
	RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
	
	prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "inputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Inputs", "");
	
	prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "outputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Outputs", "");

	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent", "Parent this node is attached to");
	
	prop = RNA_def_property(srna, "use_custom_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_CUSTOM_COLOR);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Custom Color", "Use custom color for the node");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color", "Custom color of the node body");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_SELECT);
	RNA_def_property_ui_text(prop, "Select", "");

	prop = RNA_def_property(srna, "show_options", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_OPTIONS);
	RNA_def_property_ui_text(prop, "Show Options", "");

	prop = RNA_def_property(srna, "show_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_PREVIEW);
	RNA_def_property_ui_text(prop, "Show Preview", "");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_HIDDEN);
	RNA_def_property_ui_text(prop, "Hide", "");

	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_MUTED);
	RNA_def_property_ui_text(prop, "Mute", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "show_texture", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_ACTIVE_TEXTURE);
	RNA_def_property_ui_text(prop, "Show Texture", "Draw node in viewport textured draw mode");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void rna_def_node_link(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NodeLink", NULL);
	RNA_def_struct_ui_text(srna, "NodeLink", "Link between nodes in a node tree");
	RNA_def_struct_sdna(srna, "bNodeLink");
	RNA_def_struct_ui_icon(srna, ICON_NODE);

	prop = RNA_def_property(srna, "from_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fromnode");
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "From node", "");

	prop = RNA_def_property(srna, "to_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tonode");
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "To node", "");

	prop = RNA_def_property(srna, "from_socket", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fromsock");
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "From socket", "");

	prop = RNA_def_property(srna, "to_socket", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tosock");
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "To socket", "");
}

static void rna_def_group_sockets_api(BlenderRNA *brna, PropertyRNA *cprop, int in_out)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, (in_out == SOCK_IN ? "GroupInputs" : "GroupOutputs"));
	srna = RNA_def_struct(brna, (in_out == SOCK_IN ? "GroupInputs" : "GroupOutputs"), NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Group Sockets", "Collection of group sockets");

	func = RNA_def_function(srna, "new", (in_out == SOCK_IN ? "rna_NodeTree_input_new" : "rna_NodeTree_output_new"));
	RNA_def_function_ui_description(func, "Add a socket to the group tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_string(func, "name", "Socket", MAX_NAME, "Name", "Name of the socket");
	RNA_def_enum(func, "type", node_socket_type_items, SOCK_FLOAT, "Type", "Type of socket");
	/* return value */
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "expose",
	                        (in_out == SOCK_IN ? "rna_NodeTree_input_expose" : "rna_NodeTree_output_expose"));
	RNA_def_function_ui_description(func, "Expose an internal socket in the group tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_pointer(func, "sock", "NodeSocket", "Socket", "Internal node socket to expose");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "add_link", TRUE, "Add Link", "If TRUE, adds a link to the internal socket");
	/* return value */
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
	RNA_def_function_return(func, parm);
}

static void rna_def_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NodeTree", "ID");
	RNA_def_struct_ui_text(srna, "Node Tree",
	                       "Node tree consisting of linked nodes used for shading, textures and compositing");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODETREE);
	RNA_def_struct_refine_func(srna, "rna_NodeTree_refine");

	/* AnimData */
	rna_def_animdata_common(srna);
	
	/* NodeLinks Collection */
	prop = RNA_def_property(srna, "links", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "links", NULL);
	RNA_def_property_struct_type(prop, "NodeLink");
	RNA_def_property_ui_text(prop, "Links", "");
	rna_def_nodetree_link_api(brna, prop);

	/* Grease Pencil */
	prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GreasePencil");
	RNA_def_property_ui_text(prop, "Grease Pencil Data", "Grease Pencil datablock");
	RNA_def_property_update(prop, NC_NODE, NULL);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, nodetree_type_items);
	RNA_def_property_ui_text(prop, "Type", "Node Tree type");

	/* group sockets */
	prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "inputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Inputs", "");
	rna_def_group_sockets_api(brna, prop, SOCK_IN);
	
	prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "outputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Outputs", "");
	rna_def_group_sockets_api(brna, prop, SOCK_OUT);
}

static void rna_def_composite_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CompositorNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Compositor Node Tree", "Node tree consisting of linked nodes used for compositing");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODETREE);

	/* Nodes Collection */
	prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");

	rna_def_composite_nodetree_api(brna, prop);

	prop = RNA_def_property(srna, "render_quality", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "render_quality");
	RNA_def_property_enum_items(prop, node_quality_items);
	RNA_def_property_ui_text(prop, "Render Quality", "Quality when rendering");

	prop = RNA_def_property(srna, "edit_quality", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "edit_quality");
	RNA_def_property_enum_items(prop, node_quality_items);
	RNA_def_property_ui_text(prop, "Edit Quality", "Quality when editing");

	prop = RNA_def_property(srna, "chunk_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "chunksize");
	RNA_def_property_enum_items(prop, node_chunksize_items);
	RNA_def_property_ui_text(prop, "Chunksize", "Max size of a tile (smaller values gives better distribution "
	                                            "of multiple threads, but more overhead)");

	prop = RNA_def_property(srna, "use_opencl", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NTREE_COM_OPENCL);
	RNA_def_property_ui_text(prop, "OpenCL", "Enable GPU calculations");

	prop = RNA_def_property(srna, "two_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NTREE_TWO_PASS);
	RNA_def_property_ui_text(prop, "Two Pass", "Use two pass execution during editing: first calculate fast nodes, "
	                                           "second pass calculate all nodes");
}

static void rna_def_shader_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShaderNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Shader Node Tree",
	                       "Node tree consisting of linked nodes used for materials (and other shading datablocks)");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODETREE);

	/* Nodes Collection */
	prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");

	rna_def_shader_nodetree_api(brna, prop);
}

static void rna_def_texture_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "TextureNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Texture Node Tree", "Node tree consisting of linked nodes used for textures");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODETREE);

	/* Nodes Collection */
	prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");

	rna_def_texture_nodetree_api(brna, prop);
}

static void define_specific_node(BlenderRNA *brna, int id, void (*func)(StructRNA *))
{
	StructRNA *srna = def_node(brna, id);
	
	if (func)
		func(srna);
}

void RNA_def_nodetree(BlenderRNA *brna)
{
	init();
	rna_def_nodetree(brna);
	
	rna_def_node_socket(brna);
	
	/* Generate RNA definitions for all socket subtypes */
	#define SUBTYPE(socktype, stypename, id, idname) \
	rna_def_node_socket_subtype(brna, SOCK_##socktype, PROP_##id, "NodeSocket"#stypename#idname, \
	                            #idname" "#stypename" Node Socket");
	NODE_DEFINE_SUBTYPES
	#undef SUBTYPE
	rna_def_node_socket_subtype(brna, SOCK_BOOLEAN, 0, "NodeSocketBoolean", "Boolean Node Socket");
	rna_def_node_socket_subtype(brna, SOCK_RGBA, 0, "NodeSocketRGBA", "RGBA Node Socket");
	rna_def_node_socket_subtype(brna, SOCK_SHADER, 0, "NodeSocketShader", "Shader Closure Node Socket");
	
	rna_def_node(brna);
	rna_def_node_link(brna);
	rna_def_shader_node(brna);
	rna_def_compositor_node(brna);
	rna_def_texture_node(brna);
	
	rna_def_composite_nodetree(brna);
	rna_def_shader_nodetree(brna);
	rna_def_texture_nodetree(brna);
	#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		define_specific_node(brna, ID, DefFunc);
		
	#include "rna_nodetree_types.h"
	
	define_specific_node(brna, NODE_GROUP, def_group);
	define_specific_node(brna, NODE_FORLOOP, def_forloop);
	define_specific_node(brna, NODE_WHILELOOP, def_whileloop);
	define_specific_node(brna, NODE_FRAME, def_frame);
	define_specific_node(brna, NODE_REROUTE, 0);
	
	/* special socket types */
	rna_def_cmp_output_file_slot_file(brna);
	rna_def_cmp_output_file_slot_layer(brna);
}

/* clean up macro definition */
#undef NODE_DEFINE_SUBTYPES

#endif
