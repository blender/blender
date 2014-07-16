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
#include <limits.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"
#include "rna_internal_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "WM_types.h"

#include "MEM_guardedalloc.h"

EnumPropertyItem node_socket_in_out_items[] = {
	{ SOCK_IN, "IN", 0, "Input", "" },
	{ SOCK_OUT, "OUT", 0, "Output", "" },
	{ 0, NULL, 0, NULL, NULL }
};

#ifndef RNA_RUNTIME
static EnumPropertyItem node_socket_type_items[] = {
	{SOCK_CUSTOM,  "CUSTOM",    0,    "Custom",    ""},
	{SOCK_FLOAT,   "VALUE",     0,    "Value",     ""},
	{SOCK_INT,     "INT",       0,    "Int",       ""},
	{SOCK_BOOLEAN, "BOOLEAN",   0,    "Boolean",   ""},
	{SOCK_VECTOR,  "VECTOR",    0,    "Vector",    ""},
	{SOCK_STRING,  "STRING",    0,    "String",    ""},
	{SOCK_RGBA,    "RGBA",      0,    "RGBA",      ""},
	{SOCK_SHADER,  "SHADER",    0,    "Shader",    ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_quality_items[] = {
	{NTREE_QUALITY_HIGH,   "HIGH",     0,    "High",     "High quality"},
	{NTREE_QUALITY_MEDIUM, "MEDIUM",   0,    "Medium",   "Medium quality"},
	{NTREE_QUALITY_LOW,    "LOW",      0,    "Low",      "Low quality"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_chunksize_items[] = {
	{NTREE_CHUNCKSIZE_32,   "32",     0,    "32x32",     "Chunksize of 32x32"},
	{NTREE_CHUNCKSIZE_64,   "64",     0,    "64x64",     "Chunksize of 64x64"},
	{NTREE_CHUNCKSIZE_128,  "128",    0,    "128x128",   "Chunksize of 128x128"},
	{NTREE_CHUNCKSIZE_256,  "256",    0,    "256x256",   "Chunksize of 256x256"},
	{NTREE_CHUNCKSIZE_512,  "512",    0,    "512x512",   "Chunksize of 512x512"},
	{NTREE_CHUNCKSIZE_1024, "1024",   0,    "1024x1024", "Chunksize of 1024x1024"},
	{0, NULL, 0, NULL, NULL}
};
#endif

#define DEF_ICON_BLANK_SKIP
#define DEF_ICON(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_VICO(name)
EnumPropertyItem node_icon_items[] = {
#include "UI_icons.h"
	{0, NULL, 0, NULL, NULL}};
#undef DEF_ICON_BLANK_SKIP
#undef DEF_ICON
#undef DEF_VICO

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
	{17, "MODULO",       0, "Modulo",       ""},
	{18, "ABSOLUTE",     0, "Absolute",     ""},
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

#ifndef RNA_RUNTIME
static EnumPropertyItem node_sampler_type_items[] = {
	{0, "NEAREST",   0, "Nearest",   ""},
	{1, "BILINEAR",   0, "Bilinear",   ""},
	{2, "BICUBIC", 0, "Bicubic", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

#ifdef RNA_RUNTIME

#include "BLI_linklist.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"

#include "BKE_global.h"

#include "ED_node.h"
#include "ED_render.h"

#include "NOD_common.h"
#include "NOD_socket.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "DNA_scene_types.h"
#include "WM_api.h"


int rna_node_tree_type_to_enum(bNodeTreeType *typeinfo)
{
	int i = 0, result = -1;
	NODE_TREE_TYPES_BEGIN (nt)
	{
		if (nt == typeinfo) {
			result = i;
			break;
		}
		++i;
	}
	NODE_TREE_TYPES_END;
	return result;
}

int rna_node_tree_idname_to_enum(const char *idname)
{
	int i = 0, result = -1;
	NODE_TREE_TYPES_BEGIN (nt)
	{
		if (STREQ(nt->idname, idname)) {
			result = i;
			break;
		}
		++i;
	}
	NODE_TREE_TYPES_END;
	return result;
}

bNodeTreeType *rna_node_tree_type_from_enum(int value)
{
	int i = 0;
	bNodeTreeType *result = NULL;
	NODE_TREE_TYPES_BEGIN (nt)
	{
		if (i == value) {
			result = nt;
			break;
		}
		++i;
	}
	NODE_TREE_TYPES_END;
	return result;
}

EnumPropertyItem *rna_node_tree_type_itemf(void *data, int (*poll)(void *data, bNodeTreeType *), bool *r_free)
{
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item = NULL;
	int totitem = 0, i = 0;
	
	NODE_TREE_TYPES_BEGIN (nt)
	{
		if (poll && !poll(data, nt)) {
			++i;
			continue;
		}
		
		tmp.value = i;
		tmp.identifier = nt->idname;
		tmp.icon = nt->ui_icon;
		tmp.name = nt->ui_name;
		tmp.description = nt->ui_description;
		
		RNA_enum_item_add(&item, &totitem, &tmp);
		
		++i;
	}
	NODE_TREE_TYPES_END;

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

int rna_node_type_to_enum(bNodeType *typeinfo)
{
	int i = 0, result = -1;
	NODE_TYPES_BEGIN(ntype)
		if (ntype == typeinfo) {
			result = i;
			break;
		}
		++i;
	NODE_TYPES_END
	return result;
}

int rna_node_idname_to_enum(const char *idname)
{
	int i = 0, result = -1;
	NODE_TYPES_BEGIN(ntype)
		if (STREQ(ntype->idname, idname)) {
			result = i;
			break;
		}
		++i;
	NODE_TYPES_END
	return result;
}

bNodeType *rna_node_type_from_enum(int value)
{
	int i = 0;
	bNodeType *result = NULL;
	NODE_TYPES_BEGIN(ntype)
		if (i == value) {
			result = ntype;
			break;
		}
		++i;
	NODE_TYPES_END
	return result;
}

EnumPropertyItem *rna_node_type_itemf(void *data, int (*poll)(void *data, bNodeType *), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int totitem = 0, i = 0;
	
	NODE_TYPES_BEGIN(ntype)
		if (poll && !poll(data, ntype)) {
			++i;
			continue;
		}
		
		tmp.value = i;
		tmp.identifier = ntype->idname;
		tmp.icon = ntype->ui_icon;
		tmp.name = ntype->ui_name;
		tmp.description = ntype->ui_description;
		
		RNA_enum_item_add(&item, &totitem, &tmp);
		
		++i;
	NODE_TYPES_END
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

int rna_node_socket_type_to_enum(bNodeSocketType *typeinfo)
{
	int i = 0, result = -1;
	NODE_SOCKET_TYPES_BEGIN(stype)
		if (stype == typeinfo) {
			result = i;
			break;
		}
		++i;
	NODE_SOCKET_TYPES_END
	return result;
}

int rna_node_socket_idname_to_enum(const char *idname)
{
	int i = 0, result = -1;
	NODE_SOCKET_TYPES_BEGIN(stype)
		if (STREQ(stype->idname, idname)) {
			result = i;
			break;
		}
		++i;
	NODE_SOCKET_TYPES_END
	return result;
}

bNodeSocketType *rna_node_socket_type_from_enum(int value)
{
	int i = 0;
	bNodeSocketType *result = NULL;
	NODE_SOCKET_TYPES_BEGIN(stype)
		if (i == value) {
			result = stype;
			break;
		}
		++i;
	NODE_SOCKET_TYPES_END
	return result;
}

EnumPropertyItem *rna_node_socket_type_itemf(void *data, int (*poll)(void *data, bNodeSocketType *), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int totitem = 0, i = 0;
	StructRNA *srna;
	
	NODE_SOCKET_TYPES_BEGIN(stype)
		if (poll && !poll(data, stype)) {
			++i;
			continue;
		}
		
		srna = stype->ext_socket.srna;
		tmp.value = i;
		tmp.identifier = stype->idname;
		tmp.icon = RNA_struct_ui_icon(srna);
		tmp.name = RNA_struct_ui_name(srna);
		tmp.description = RNA_struct_ui_description(srna);
		
		RNA_enum_item_add(&item, &totitem, &tmp);
		
		++i;
	NODE_SOCKET_TYPES_END
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

static EnumPropertyItem *rna_node_static_type_itemf(bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp;
	int totitem = 0;
	
	/* hack, don't want to add include path to RNA just for this, since in the future RNA types
	 * for nodes should be defined locally at runtime anyway ...
	 */
	
	tmp.value = NODE_CUSTOM;
	tmp.identifier = "CUSTOM";
	tmp.name = "Custom";
	tmp.description = "Custom Node";
	tmp.icon = ICON_NONE;
	RNA_enum_item_add(&item, &totitem, &tmp);
	
	tmp.value = NODE_UNDEFINED;
	tmp.identifier = "UNDEFINED";
	tmp.name = "UNDEFINED";
	tmp.description = "";
	tmp.icon = ICON_NONE;
	RNA_enum_item_add(&item, &totitem, &tmp);
	
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
	if (STREQ(#Category, "Node")) { \
		tmp.value = ID; \
		tmp.identifier = EnumName; \
		tmp.name = UIName; \
		tmp.description = UIDesc; \
		tmp.icon = ICON_NONE; \
		RNA_enum_item_add(&item, &totitem, &tmp); \
	}
#include "../../nodes/NOD_static_types.h"
#undef DefNode
	
	if (RNA_struct_is_a(ptr->type, &RNA_ShaderNode)) {
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		if (STREQ(#Category, "ShaderNode")) { \
			tmp.value = ID; \
			tmp.identifier = EnumName; \
			tmp.name = UIName; \
			tmp.description = UIDesc; \
			tmp.icon = ICON_NONE; \
			RNA_enum_item_add(&item, &totitem, &tmp); \
		}
#include "../../nodes/NOD_static_types.h"
#undef DefNode
	}

	if (RNA_struct_is_a(ptr->type, &RNA_CompositorNode)) {
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		if (STREQ(#Category, "CompositorNode")) { \
			tmp.value = ID; \
			tmp.identifier = EnumName; \
			tmp.name = UIName; \
			tmp.description = UIDesc; \
			tmp.icon = ICON_NONE; \
			RNA_enum_item_add(&item, &totitem, &tmp); \
		}
#include "../../nodes/NOD_static_types.h"
#undef DefNode
	}
	
	if (RNA_struct_is_a(ptr->type, &RNA_TextureNode)) {
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
		if (STREQ(#Category, "TextureNode")) { \
			tmp.value = ID; \
			tmp.identifier = EnumName; \
			tmp.name = UIName; \
			tmp.description = UIDesc; \
			tmp.icon = ICON_NONE; \
			RNA_enum_item_add(&item, &totitem, &tmp); \
		}
#include "../../nodes/NOD_static_types.h"
#undef DefNode
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

/* ******** Node Tree ******** */

static StructRNA *rna_NodeTree_refine(struct PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	
	if (ntree->typeinfo->ext.srna)
		return ntree->typeinfo->ext.srna;
	else
		return &RNA_NodeTree;
}

static int rna_NodeTree_poll(const bContext *C, bNodeTreeType *ntreetype)
{
	extern FunctionRNA rna_NodeTree_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, ntreetype->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_NodeTree_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	ntreetype->ext.call((bContext *)C, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static void rna_NodeTree_update_reg(bNodeTree *ntree)
{
	extern FunctionRNA rna_NodeTree_update_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_id_pointer_create(&ntree->id, &ptr);
	func = &rna_NodeTree_update_func; /* RNA_struct_find_function(&ptr, "update"); */

	RNA_parameter_list_create(&list, &ptr, func);
	ntree->typeinfo->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeTree_get_from_context(const bContext *C, bNodeTreeType *ntreetype,
                                          bNodeTree **r_ntree, ID **r_id, ID **r_from)
{
	extern FunctionRNA rna_NodeTree_get_from_context_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret1, *ret2, *ret3;

	RNA_pointer_create(NULL, ntreetype->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_NodeTree_get_from_context_func; /* RNA_struct_find_function(&ptr, "get_from_context"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	ntreetype->ext.call((bContext *)C, &ptr, func, &list);
	
	RNA_parameter_get_lookup(&list, "result_1", &ret1);
	RNA_parameter_get_lookup(&list, "result_2", &ret2);
	RNA_parameter_get_lookup(&list, "result_3", &ret3);
	*r_ntree = *(bNodeTree **)ret1;
	*r_id = *(ID **)ret2;
	*r_from = *(ID **)ret3;

	RNA_parameter_list_free(&list);
}

static void rna_NodeTree_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	bNodeTreeType *nt = RNA_struct_blender_type_get(type);

	if (!nt)
		return;

	RNA_struct_free_extension(type, &nt->ext);

	ntreeTypeFreeLink(nt);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
}

static StructRNA *rna_NodeTree_register(
        Main *bmain, ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeTreeType *nt, dummynt;
	bNodeTree dummyntree;
	PointerRNA dummyptr;
	int have_function[3];

	/* setup dummy tree & tree type to store static properties in */
	memset(&dummynt, 0, sizeof(bNodeTreeType));
	memset(&dummyntree, 0, sizeof(bNodeTree));
	dummyntree.typeinfo = &dummynt;
	RNA_pointer_create(NULL, &RNA_NodeTree, &dummyntree, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummynt.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering node tree class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummynt.idname));
		return NULL;
	}

	/* check if we have registered this tree type before, and remove it */
	nt = ntreeTypeFind(dummynt.idname);
	if (nt)
		rna_NodeTree_unregister(bmain, nt->ext.srna);
	
	/* create a new node tree type */
	nt = MEM_callocN(sizeof(bNodeTreeType), "node tree type");
	memcpy(nt, &dummynt, sizeof(dummynt));

	nt->type = NTREE_CUSTOM;

	nt->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname, &RNA_NodeTree); 
	nt->ext.data = data;
	nt->ext.call = call;
	nt->ext.free = free;
	RNA_struct_blender_type_set(nt->ext.srna, nt);

	RNA_def_struct_ui_text(nt->ext.srna, nt->ui_name, nt->ui_description);
	RNA_def_struct_ui_icon(nt->ext.srna, nt->ui_icon);

	nt->poll = (have_function[0]) ? rna_NodeTree_poll : NULL;
	nt->update = (have_function[1]) ? rna_NodeTree_update_reg : NULL;
	nt->get_from_context = (have_function[2]) ? rna_NodeTree_get_from_context : NULL;

	ntreeTypeAdd(nt);

	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static bool rna_NodeTree_check(bNodeTree *ntree, ReportList *reports)
{
	if (!ntreeIsRegistered(ntree)) {
		if (reports)
			BKE_reportf(reports, RPT_ERROR, "Node tree '%s' has undefined type %s", ntree->id.name + 2, ntree->idname);
		
		return false;
	}
	else
		return true;
}

static void rna_NodeTree_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;

	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	WM_main_add_notifier(NC_SCENE | ND_NODES, &ntree->id);

	ED_node_tag_update_nodetree(bmain, ntree);
}

static bNode *rna_NodeTree_node_new(bNodeTree *ntree, bContext *C, ReportList *reports, const char *type)
{
	bNodeType *ntype;
	bNode *node;
	
	if (!rna_NodeTree_check(ntree, reports))
		return NULL;
	
	ntype = nodeTypeFind(type);
	if (!ntype) {
		BKE_reportf(reports, RPT_ERROR, "Node type %s undefined", type);
		return NULL;
	}
	
	if (ntype->poll && !ntype->poll(ntype, ntree)) {
		BKE_reportf(reports, RPT_ERROR, "Cannot add node of type %s to node tree '%s'", type, ntree->id.name + 2);
		return NULL;
	}
	
	node = nodeAddNode(C, ntree, type);
	BLI_assert(node && node->typeinfo);
	
	if (ntree->type == NTREE_TEXTURE) {
		ntreeTexCheckCyclics(ntree);
	}
	
	ntreeUpdateTree(CTX_data_main(C), ntree);
	nodeUpdate(ntree, node);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

	return node;
}

static void rna_NodeTree_node_remove(bNodeTree *ntree, ReportList *reports, PointerRNA *node_ptr)
{
	bNode *node = node_ptr->data;
	
	if (!rna_NodeTree_check(ntree, reports))
		return;
	
	if (BLI_findindex(&ntree->nodes, node) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to locate node '%s' in node tree", node->name);
		return;
	}

	id_us_min(node->id);
	nodeFreeNode(ntree, node);
	RNA_POINTER_INVALIDATE(node_ptr);

	ntreeUpdateTree(G.main, ntree); /* update group node socket links */
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_node_clear(bNodeTree *ntree, ReportList *reports)
{
	bNode *node = ntree->nodes.first;

	if (!rna_NodeTree_check(ntree, reports))
		return;

	while (node) {
		bNode *next_node = node->next;

		if (node->id)
			id_us_min(node->id);

		nodeFreeNode(ntree, node);

		node = next_node;
	}

	ntreeUpdateTree(G.main, ntree);

	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static PointerRNA rna_NodeTree_active_node_get(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNode *node = nodeGetActive(ntree);
	return rna_pointer_inherit_refine(ptr, &RNA_Node, node);
}

static void rna_NodeTree_active_node_set(PointerRNA *ptr, const PointerRNA value)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNode *node = (bNode *)value.data;
	
	if (node && BLI_findindex(&ntree->nodes, node) != -1)
		nodeSetActive(ntree, node);
	else
		nodeClearActive(ntree);
}

static bNodeLink *rna_NodeTree_link_new(bNodeTree *ntree, ReportList *reports,
                                        bNodeSocket *fromsock, bNodeSocket *tosock,
                                        int verify_limits)
{
	bNodeLink *ret;
	bNode *fromnode = NULL, *tonode = NULL;

	if (!rna_NodeTree_check(ntree, reports))
		return NULL;

	nodeFindNode(ntree, fromsock, &fromnode, NULL);
	nodeFindNode(ntree, tosock, &tonode, NULL);
	/* check validity of the sockets:
	 * if sockets from different trees are passed in this will fail!
	 */
	if (!fromnode || !tonode)
		return NULL;
	
	if (&fromsock->in_out == &tosock->in_out) {
		BKE_report(reports, RPT_ERROR, "Same input/output direction of sockets");
		return NULL;
	}

	if (verify_limits) {
		/* remove other socket links if limit is exceeded */
		if (nodeCountSocketLinks(ntree, fromsock) + 1 > fromsock->limit)
			nodeRemSocketLinks(ntree, fromsock);
		if (nodeCountSocketLinks(ntree, tosock) + 1 > tosock->limit)
			nodeRemSocketLinks(ntree, tosock);
	}

	ret = nodeAddLink(ntree, fromnode, fromsock, tonode, tosock);
	
	if (ret) {
		if (tonode)
			nodeUpdate(ntree, tonode);

		ntreeUpdateTree(G.main, ntree);

		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
	return ret;
}

static void rna_NodeTree_link_remove(bNodeTree *ntree, ReportList *reports, PointerRNA *link_ptr)
{
	bNodeLink *link = link_ptr->data;

	if (!rna_NodeTree_check(ntree, reports))
		return;

	if (BLI_findindex(&ntree->links, link) == -1) {
		BKE_report(reports, RPT_ERROR, "Unable to locate link in node tree");
		return;
	}

	nodeRemLink(ntree, link);
	RNA_POINTER_INVALIDATE(link_ptr);

	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_link_clear(bNodeTree *ntree, ReportList *reports)
{
	bNodeLink *link = ntree->links.first;

	if (!rna_NodeTree_check(ntree, reports))
		return;

	while (link) {
		bNodeLink *next_link = link->next;

		nodeRemLink(ntree, link);

		link = next_link;
	}
	ntreeUpdateTree(G.main, ntree);

	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static int rna_NodeTree_active_input_get(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNodeSocket *gsock;
	int index;

	for (gsock = ntree->inputs.first, index = 0; gsock; gsock = gsock->next, ++index)
		if (gsock->flag & SELECT)
			return index;
	return -1;
}

static void rna_NodeTree_active_input_set(PointerRNA *ptr, int value)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNodeSocket *gsock;
	int index;
	
	for (gsock = ntree->inputs.first, index = 0; gsock; gsock = gsock->next, ++index) {
		if (index == value)
			gsock->flag |= SELECT;
		else
			gsock->flag &= ~SELECT;
	}
	for (gsock = ntree->outputs.first; gsock; gsock = gsock->next) {
		gsock->flag &= ~SELECT;
	}
}

static int rna_NodeTree_active_output_get(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNodeSocket *gsock;
	int index;

	for (gsock = ntree->outputs.first, index = 0; gsock; gsock = gsock->next, ++index)
		if (gsock->flag & SELECT)
			return index;
	return -1;
}

static void rna_NodeTree_active_output_set(PointerRNA *ptr, int value)
{
	bNodeTree *ntree = (bNodeTree *)ptr->data;
	bNodeSocket *gsock;
	int index;
	
	for (gsock = ntree->inputs.first; gsock; gsock = gsock->next) {
		gsock->flag &= ~SELECT;
	}
	for (gsock = ntree->outputs.first, index = 0; gsock; gsock = gsock->next, ++index) {
		if (index == value)
			gsock->flag |= SELECT;
		else
			gsock->flag &= ~SELECT;
	}
}

static bNodeSocket *rna_NodeTree_inputs_new(bNodeTree *ntree, ReportList *reports, const char *type, const char *name)
{
	bNodeSocket *sock;
	
	if (!rna_NodeTree_check(ntree, reports))
		return NULL;
	
	sock = ntreeAddSocketInterface(ntree, SOCK_IN, type, name);
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	
	return sock;
}

static bNodeSocket *rna_NodeTree_outputs_new(bNodeTree *ntree, ReportList *reports, const char *type, const char *name)
{
	bNodeSocket *sock;
	
	if (!rna_NodeTree_check(ntree, reports))
		return NULL;
	
	sock = ntreeAddSocketInterface(ntree, SOCK_OUT, type, name);
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	
	return sock;
}

static void rna_NodeTree_socket_remove(bNodeTree *ntree, ReportList *reports, bNodeSocket *sock)
{
	if (!rna_NodeTree_check(ntree, reports))
		return;
	
	if (BLI_findindex(&ntree->inputs, sock) == -1 && BLI_findindex(&ntree->outputs, sock) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to locate socket '%s' in node", sock->identifier);
	}
	else {
		ntreeRemoveSocketInterface(ntree, sock);
		
		ntreeUpdateTree(G.main, ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
}

static void rna_NodeTree_inputs_clear(bNodeTree *ntree, ReportList *reports)
{
	bNodeSocket *sock, *nextsock;
	
	if (!rna_NodeTree_check(ntree, reports))
		return;
	
	for (sock = ntree->inputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		ntreeRemoveSocketInterface(ntree, sock);
	}

	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_outputs_clear(bNodeTree *ntree, ReportList *reports)
{
	bNodeSocket *sock, *nextsock;
	
	if (!rna_NodeTree_check(ntree, reports))
		return;
	
	for (sock = ntree->outputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		ntreeRemoveSocketInterface(ntree, sock);
	}

	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_inputs_move(bNodeTree *ntree, int from_index, int to_index)
{
	bNodeSocket *sock;
	
	if (from_index == to_index)
		return;
	if (from_index < 0 || to_index < 0)
		return;
	
	sock = BLI_findlink(&ntree->inputs, from_index);
	if (to_index < from_index) {
		bNodeSocket *nextsock = BLI_findlink(&ntree->inputs, to_index);
		if (nextsock) {
			BLI_remlink(&ntree->inputs, sock);
			BLI_insertlinkbefore(&ntree->inputs, nextsock, sock);
		}
	}
	else {
		bNodeSocket *prevsock = BLI_findlink(&ntree->inputs, to_index);
		if (prevsock) {
			BLI_remlink(&ntree->inputs, sock);
			BLI_insertlinkafter(&ntree->inputs, prevsock, sock);
		}
	}
	
	ntree->update |= NTREE_UPDATE_GROUP_IN;
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_outputs_move(bNodeTree *ntree, int from_index, int to_index)
{
	bNodeSocket *sock;
	
	if (from_index == to_index)
		return;
	if (from_index < 0 || to_index < 0)
		return;
	
	sock = BLI_findlink(&ntree->outputs, from_index);
	if (to_index < from_index) {
		bNodeSocket *nextsock = BLI_findlink(&ntree->outputs, to_index);
		if (nextsock) {
			BLI_remlink(&ntree->outputs, sock);
			BLI_insertlinkbefore(&ntree->outputs, nextsock, sock);
		}
	}
	else {
		bNodeSocket *prevsock = BLI_findlink(&ntree->outputs, to_index);
		if (prevsock) {
			BLI_remlink(&ntree->outputs, sock);
			BLI_insertlinkafter(&ntree->outputs, prevsock, sock);
		}
	}
	
	ntree->update |= NTREE_UPDATE_GROUP_OUT;
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_NodeTree_interface_update(bNodeTree *ntree, bContext *C)
{
	ntree->update |= NTREE_UPDATE_GROUP;
	ntreeUpdateTree(G.main, ntree);
	
	ED_node_tag_update_nodetree(CTX_data_main(C), ntree);
}


/* ******** NodeLink ******** */

static int rna_NodeLink_is_hidden_get(PointerRNA *ptr)
{
	bNodeLink *link = ptr->data;
	return nodeLinkIsHidden(link);
}


/* ******** Node ******** */

static StructRNA *rna_Node_refine(struct PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;
	
	if (node->typeinfo->ext.srna)
		return node->typeinfo->ext.srna;
	else
		return ptr->type;
}

static char *rna_Node_path(PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;
	char name_esc[sizeof(node->name) * 2];

	BLI_strescape(name_esc, node->name, sizeof(name_esc));
	return BLI_sprintfN("nodes[\"%s\"]", name_esc);
}

char *rna_Node_ImageUser_path(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node;
	char name_esc[sizeof(node->name) * 2];

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == SH_NODE_TEX_ENVIRONMENT) {
			NodeTexEnvironment *data = node->storage;
			if (&data->iuser != ptr->data)
				continue;
		}
		else if (node->type == SH_NODE_TEX_IMAGE) {
			NodeTexImage *data = node->storage;
			if (&data->iuser != ptr->data)
				continue;
		}
		else
			continue;

		BLI_strescape(name_esc, node->name, sizeof(name_esc));
		return BLI_sprintfN("nodes[\"%s\"].image_user", name_esc);
	}

	return NULL;
}

static int rna_Node_poll(bNodeType *ntype, bNodeTree *ntree)
{
	extern FunctionRNA rna_Node_poll_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, ntype->ext.srna, NULL, &ptr); /* dummy */
	func = &rna_Node_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node_tree", &ntree);
	ntype->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static int rna_Node_poll_instance(bNode *node, bNodeTree *ntree)
{
	extern FunctionRNA rna_Node_poll_instance_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	int visible;

	RNA_pointer_create(NULL, node->typeinfo->ext.srna, node, &ptr); /* dummy */
	func = &rna_Node_poll_instance_func; /* RNA_struct_find_function(&ptr, "poll_instance"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node_tree", &ntree);
	node->typeinfo->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "visible", &ret);
	visible = *(int *)ret;

	RNA_parameter_list_free(&list);

	return visible;
}

static int rna_Node_poll_instance_default(bNode *node, bNodeTree *ntree)
{
	/* use the basic poll function */
	return rna_Node_poll(node->typeinfo, ntree);
}

static void rna_Node_update_reg(bNodeTree *ntree, bNode *node)
{
	extern FunctionRNA rna_Node_update_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create((ID *)ntree, node->typeinfo->ext.srna, node, &ptr);
	func = &rna_Node_update_func; /* RNA_struct_find_function(&ptr, "update"); */

	RNA_parameter_list_create(&list, &ptr, func);
	node->typeinfo->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_init(const bContext *C, PointerRNA *ptr)
{
	extern FunctionRNA rna_Node_init_func;

	bNode *node = (bNode *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_Node_init_func; /* RNA_struct_find_function(&ptr, "init"); */

	RNA_parameter_list_create(&list, ptr, func);
	node->typeinfo->ext.call((bContext *)C, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_copy(PointerRNA *ptr, struct bNode *copynode)
{
	extern FunctionRNA rna_Node_copy_func;

	bNode *node = (bNode *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_Node_copy_func; /* RNA_struct_find_function(&ptr, "copy"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "node", &copynode);
	node->typeinfo->ext.call(NULL, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_free(PointerRNA *ptr)
{
	extern FunctionRNA rna_Node_free_func;

	bNode *node = (bNode *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_Node_free_func; /* RNA_struct_find_function(&ptr, "free"); */

	RNA_parameter_list_create(&list, ptr, func);
	node->typeinfo->ext.call(NULL, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons(struct uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	extern FunctionRNA rna_Node_draw_buttons_func;

	bNode *node = (bNode *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_Node_draw_buttons_func; /* RNA_struct_find_function(&ptr, "draw_buttons"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	node->typeinfo->ext.call(C, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_draw_buttons_ext(struct uiLayout *layout, bContext *C, PointerRNA *ptr)
{
	extern FunctionRNA rna_Node_draw_buttons_ext_func;

	bNode *node = (bNode *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_Node_draw_buttons_ext_func; /* RNA_struct_find_function(&ptr, "draw_buttons_ext"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	node->typeinfo->ext.call(C, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_Node_draw_label(bNodeTree *ntree, bNode *node, char *label, int maxlen)
{
	extern FunctionRNA rna_Node_draw_label_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;
	void *ret;
	char *rlabel;

	func = &rna_Node_draw_label_func; /* RNA_struct_find_function(&ptr, "draw_label"); */

	RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
	RNA_parameter_list_create(&list, &ptr, func);
	node->typeinfo->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_get_lookup(&list, "label", &ret);
	rlabel = *(char **)ret;
	BLI_strncpy(label, rlabel != NULL ? rlabel : "", maxlen);

	RNA_parameter_list_free(&list);
}

static int rna_Node_is_registered_node_type(StructRNA *type)
{
	return (RNA_struct_blender_type_get(type) != NULL);
}

static void rna_Node_is_registered_node_type_runtime(bContext *UNUSED(C), ReportList *UNUSED(reports), PointerRNA *ptr, ParameterList *parms)
{
	int result = (RNA_struct_blender_type_get(ptr->type) != NULL);
	RNA_parameter_set_lookup(parms, "result", &result);
}

static void rna_Node_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	bNodeType *nt = RNA_struct_blender_type_get(type);

	if (!nt)
		return;

	RNA_struct_free_extension(type, &nt->ext);

	/* this also frees the allocated nt pointer, no MEM_free call needed! */
	nodeUnregisterType(nt);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
}

/* Generic internal registration function.
 * Can be used to implement callbacks for registerable RNA node subtypes.
 */
static bNodeType *rna_Node_register_base(Main *bmain, ReportList *reports, StructRNA *basetype,
                                         void *data, const char *identifier,
                                         StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt, dummynt;
	bNode dummynode;
	PointerRNA dummyptr;
	FunctionRNA *func;
	PropertyRNA *parm;
	int have_function[9];

	/* setup dummy node & node type to store static properties in */
	memset(&dummynt, 0, sizeof(bNodeType));
	/* this does some additional initialization of default values */
	node_type_base_custom(&dummynt, identifier, "", 0, 0);

	memset(&dummynode, 0, sizeof(bNode));
	dummynode.typeinfo = &dummynt;
	RNA_pointer_create(NULL, basetype, &dummynode, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	if (strlen(identifier) >= sizeof(dummynt.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering node class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummynt.idname));
		return NULL;
	}

	/* check if we have registered this node type before, and remove it */
	nt = nodeTypeFind(dummynt.idname);
	if (nt)
		rna_Node_unregister(bmain, nt->ext.srna);
	
	/* create a new node type */
	nt = MEM_callocN(sizeof(bNodeType), "node type");
	memcpy(nt, &dummynt, sizeof(dummynt));
	/* make sure the node type struct is freed on unregister */
	nt->needs_free = 1;

	nt->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, nt->idname, basetype);
	nt->ext.data = data;
	nt->ext.call = call;
	nt->ext.free = free;
	RNA_struct_blender_type_set(nt->ext.srna, nt);

	RNA_def_struct_ui_text(nt->ext.srna, nt->ui_name, nt->ui_description);
	RNA_def_struct_ui_icon(nt->ext.srna, nt->ui_icon);

	func = RNA_def_function_runtime(nt->ext.srna, "is_registered_node_type", rna_Node_is_registered_node_type_runtime);
	RNA_def_function_ui_description(func, "True if a registered node type");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	parm = RNA_def_boolean(func, "result", false, "Result", "");
	RNA_def_function_return(func, parm);

	/* XXX bad level call! needed to initialize the basic draw functions ... */
	ED_init_custom_node_type(nt);

	nt->poll = (have_function[0]) ? rna_Node_poll : NULL;
	nt->poll_instance = (have_function[1]) ? rna_Node_poll_instance : rna_Node_poll_instance_default;
	nt->updatefunc = (have_function[2]) ? rna_Node_update_reg : NULL;
	nt->initfunc_api = (have_function[3]) ? rna_Node_init : NULL;
	nt->copyfunc_api = (have_function[4]) ? rna_Node_copy : NULL;
	nt->freefunc_api = (have_function[5]) ? rna_Node_free : NULL;
	nt->draw_buttons = (have_function[6]) ? rna_Node_draw_buttons : NULL;
	nt->draw_buttons_ex = (have_function[7]) ? rna_Node_draw_buttons_ext : NULL;
	nt->labelfunc = (have_function[8]) ? rna_Node_draw_label : NULL;
	
	/* sanitize size values in case not all have been registered */
	if (nt->maxwidth < nt->minwidth)
		nt->maxwidth = nt->minwidth;
	if (nt->maxheight < nt->minheight)
		nt->maxheight = nt->minheight;
	CLAMP(nt->width, nt->minwidth, nt->maxwidth);
	CLAMP(nt->height, nt->minheight, nt->maxheight);
	
	return nt;
}

static StructRNA *rna_Node_register(
        Main *bmain, ReportList *reports,
        void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt = rna_Node_register_base(bmain, reports, &RNA_Node, data, identifier, validate, call, free);
	if (!nt)
		return NULL;
	
	nodeRegisterType(nt);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static StructRNA *rna_ShaderNode_register(
        Main *bmain, ReportList *reports,
        void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt = rna_Node_register_base(bmain, reports, &RNA_ShaderNode, data, identifier, validate, call, free);
	if (!nt)
		return NULL;
	
	nodeRegisterType(nt);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static StructRNA *rna_CompositorNode_register(
        Main *bmain, ReportList *reports,
        void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt = rna_Node_register_base(bmain, reports, &RNA_CompositorNode, data, identifier, validate, call, free);
	if (!nt)
		return NULL;
	
	nodeRegisterType(nt);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static StructRNA *rna_TextureNode_register(
        Main *bmain, ReportList *reports,
        void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt = rna_Node_register_base(bmain, reports, &RNA_TextureNode, data, identifier, validate, call, free);
	if (!nt)
		return NULL;
	
	nodeRegisterType(nt);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static IDProperty *rna_Node_idprops(PointerRNA *ptr, bool create)
{
	bNode *node = ptr->data;
	
	if (create && !node->prop) {
		IDPropertyTemplate val = {0};
		node->prop = IDP_New(IDP_GROUP, &val, "RNA_Node ID properties");
	}
	
	return node->prop;
}

static void rna_Node_parent_set(PointerRNA *ptr, PointerRNA value)
{
	bNode *node = ptr->data;
	bNode *parent = value.data;
	
	if (parent) {
		/* XXX only Frame node allowed for now,
		 * in the future should have a poll function or so to test possible attachment.
		 */
		if (parent->type != NODE_FRAME)
			return;
		
		/* make sure parent is not attached to the node */
		if (nodeAttachNodeCheck(parent, node))
			return;
	}
	
	nodeDetachNode(node);
	if (parent) {
		nodeAttachNode(node, parent);
	}
}

static int rna_Node_parent_poll(PointerRNA *ptr, PointerRNA value)
{
	bNode *node = ptr->data;
	bNode *parent = value.data;
	
	/* XXX only Frame node allowed for now,
	 * in the future should have a poll function or so to test possible attachment.
	 */
	if (parent->type != NODE_FRAME)
		return false;
	
	/* make sure parent is not attached to the node */
	if (nodeAttachNodeCheck(parent, node))
		return false;
	
	return true;
}

static void rna_Node_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	ED_node_tag_update_nodetree(bmain, ntree);
}

static void rna_Node_socket_value_update(ID *id, bNode *UNUSED(node), bContext *C)
{
	ED_node_tag_update_nodetree(CTX_data_main(C), (bNodeTree *)id);
}

static void rna_Node_select_set(PointerRNA *ptr, int value)
{
	bNode *node = (bNode *)ptr->data;
	nodeSetSelected(node, value);
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

static bNodeSocket *rna_Node_inputs_new(ID *id, bNode *node, ReportList *reports, const char *type, const char *name, const char *identifier)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock;
	
	sock = nodeAddSocket(ntree, node, SOCK_IN, type, identifier, name);
	
	if (sock == NULL) {
		BKE_report(reports, RPT_ERROR, "Unable to create socket");
	}
	else {
		ntreeUpdateTree(G.main, ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
	
	return sock;
}

static bNodeSocket *rna_Node_outputs_new(ID *id, bNode *node, ReportList *reports, const char *type, const char *name, const char *identifier)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock;
	
	sock = nodeAddSocket(ntree, node, SOCK_OUT, type, identifier, name);
	
	if (sock == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Unable to create socket");
	}
	else {
		ntreeUpdateTree(G.main, ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
	
	return sock;
}

static void rna_Node_socket_remove(ID *id, bNode *node, ReportList *reports, bNodeSocket *sock)
{
	bNodeTree *ntree = (bNodeTree *)id;
	
	if (BLI_findindex(&node->inputs, sock) == -1 && BLI_findindex(&node->outputs, sock) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to locate socket '%s' in node", sock->identifier);
	}
	else {
		nodeRemoveSocket(ntree, node, sock);
		
		ntreeUpdateTree(G.main, ntree);
		WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	}
}

static void rna_Node_inputs_clear(ID *id, bNode *node)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock, *nextsock;
	
	for (sock = node->inputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		nodeRemoveSocket(ntree, node, sock);
	}

	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_outputs_clear(ID *id, bNode *node)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock, *nextsock;
	
	for (sock = node->outputs.first; sock; sock = nextsock) {
		nextsock = sock->next;
		nodeRemoveSocket(ntree, node, sock);
	}

	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_inputs_move(ID *id, bNode *node, int from_index, int to_index)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock;
	
	if (from_index == to_index)
		return;
	if (from_index < 0 || to_index < 0)
		return;
	
	sock = BLI_findlink(&node->inputs, from_index);
	if (to_index < from_index) {
		bNodeSocket *nextsock = BLI_findlink(&node->inputs, to_index);
		if (nextsock) {
			BLI_remlink(&node->inputs, sock);
			BLI_insertlinkbefore(&node->inputs, nextsock, sock);
		}
	}
	else {
		bNodeSocket *prevsock = BLI_findlink(&node->inputs, to_index);
		if (prevsock) {
			BLI_remlink(&node->inputs, sock);
			BLI_insertlinkafter(&node->inputs, prevsock, sock);
		}
	}
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_outputs_move(ID *id, bNode *node, int from_index, int to_index)
{
	bNodeTree *ntree = (bNodeTree *)id;
	bNodeSocket *sock;
	
	if (from_index == to_index)
		return;
	if (from_index < 0 || to_index < 0)
		return;
	
	sock = BLI_findlink(&node->outputs, from_index);
	if (to_index < from_index) {
		bNodeSocket *nextsock = BLI_findlink(&node->outputs, to_index);
		if (nextsock) {
			BLI_remlink(&node->outputs, sock);
			BLI_insertlinkbefore(&node->outputs, nextsock, sock);
		}
	}
	else {
		bNodeSocket *prevsock = BLI_findlink(&node->outputs, to_index);
		if (prevsock) {
			BLI_remlink(&node->outputs, sock);
			BLI_insertlinkafter(&node->outputs, prevsock, sock);
		}
	}
	
	ntreeUpdateTree(G.main, ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

static void rna_Node_width_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNode *node = ptr->data;
	*min = *softmin = node->typeinfo->minwidth;
	*max = *softmax = node->typeinfo->maxwidth;
}

static void rna_Node_height_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNode *node = ptr->data;
	*min = *softmin = node->typeinfo->minheight;
	*max = *softmax = node->typeinfo->maxheight;
}

static void rna_Node_dimensions_get(PointerRNA *ptr, float *value)
{
	bNode *node = ptr->data;
	value[0] = node->totr.xmax - node->totr.xmin;
	value[1] = node->totr.ymax - node->totr.ymin;
}


/* ******** Node Socket ******** */

static void rna_NodeSocket_draw(bContext *C, struct uiLayout *layout, PointerRNA *ptr, PointerRNA *node_ptr, const char *text)
{
	extern FunctionRNA rna_NodeSocket_draw_func;

	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	func = &rna_NodeSocket_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	RNA_parameter_set_lookup(&list, "node", node_ptr);
	RNA_parameter_set_lookup(&list, "text", &text);
	sock->typeinfo->ext_socket.call(C, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocket_draw_color(bContext *C, PointerRNA *ptr, PointerRNA *node_ptr, float *r_color)
{
	extern FunctionRNA rna_NodeSocket_draw_color_func;

	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	ParameterList list;
	FunctionRNA *func;
	void *ret;

	func = &rna_NodeSocket_draw_color_func; /* RNA_struct_find_function(&ptr, "draw_color"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "node", node_ptr);
	sock->typeinfo->ext_socket.call(C, ptr, func, &list);

	RNA_parameter_get_lookup(&list, "color", &ret);
	copy_v4_v4(r_color, (float *)ret);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocket_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	bNodeSocketType *st = RNA_struct_blender_type_get(type);
	if (!st)
		return;
	
	RNA_struct_free_extension(type, &st->ext_socket);

	nodeUnregisterSocketType(st);

	RNA_struct_free(&BLENDER_RNA, type);

	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
}

static StructRNA *rna_NodeSocket_register(
        Main *UNUSED(bmain), ReportList *reports, void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeSocketType *st, dummyst;
	bNodeSocket dummysock;
	PointerRNA dummyptr;
	int have_function[2];

	/* setup dummy socket & socket type to store static properties in */
	memset(&dummyst, 0, sizeof(bNodeSocketType));
	
	memset(&dummysock, 0, sizeof(bNodeSocket));
	dummysock.typeinfo = &dummyst;
	RNA_pointer_create(NULL, &RNA_NodeSocket, &dummysock, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;
	
	if (strlen(identifier) >= sizeof(dummyst.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering node socket class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyst.idname));
		return NULL;
	}

	/* check if we have registered this socket type before */
	st = nodeSocketTypeFind(dummyst.idname);
	if (!st) {
		/* create a new node socket type */
		st = MEM_callocN(sizeof(bNodeSocketType), "node socket type");
		memcpy(st, &dummyst, sizeof(dummyst));
		
		nodeRegisterSocketType(st);
	}
	
	/* if RNA type is already registered, unregister first */
	if (st->ext_socket.srna) {
		StructRNA *srna = st->ext_socket.srna;
		RNA_struct_free_extension(srna, &st->ext_socket);
		RNA_struct_free(&BLENDER_RNA, srna);
	}
	st->ext_socket.srna = RNA_def_struct_ptr(&BLENDER_RNA, st->idname, &RNA_NodeSocket); 
	st->ext_socket.data = data;
	st->ext_socket.call = call;
	st->ext_socket.free = free;
	RNA_struct_blender_type_set(st->ext_socket.srna, st);
	
	/* XXX bad level call! needed to initialize the basic draw functions ... */
	ED_init_custom_node_socket_type(st);

	st->draw = (have_function[0]) ? rna_NodeSocket_draw : NULL;
	st->draw_color = (have_function[1]) ? rna_NodeSocket_draw_color : NULL;

	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return st->ext_socket.srna;
}

static StructRNA *rna_NodeSocket_refine(PointerRNA *ptr)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	
	if (sock->typeinfo->ext_socket.srna)
		return sock->typeinfo->ext_socket.srna;
	else
		return &RNA_NodeSocket;
}

static char *rna_NodeSocket_path(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNode *node;
	int socketindex;
	char name_esc[sizeof(node->name) * 2];
	
	if (!nodeFindNode(ntree, sock, &node, &socketindex))
		return NULL;
	
	BLI_strescape(name_esc, node->name, sizeof(name_esc));

	if (sock->in_out == SOCK_IN) {
		return BLI_sprintfN("nodes[\"%s\"].inputs[%d]", name_esc, socketindex);
	}
	else {
		return BLI_sprintfN("nodes[\"%s\"].outputs[%d]", name_esc, socketindex);
	}
}

static IDProperty *rna_NodeSocket_idprops(PointerRNA *ptr, bool create)
{
	bNodeSocket *sock = ptr->data;
	
	if (create && !sock->prop) {
		IDPropertyTemplate val = {0};
		sock->prop = IDP_New(IDP_GROUP, &val, "RNA_NodeSocket ID properties");
	}
	
	return sock->prop;
}

static PointerRNA rna_NodeSocket_node_get(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	bNode *node;
	PointerRNA r_ptr;
	
	nodeFindNode(ntree, sock, &node, NULL);
	
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &r_ptr);
	return r_ptr;
}

static void rna_NodeSocket_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	ED_node_tag_update_nodetree(bmain, ntree);
}

static int rna_NodeSocket_is_output_get(PointerRNA *ptr)
{
	bNodeSocket *sock = ptr->data;
	return sock->in_out == SOCK_OUT;
}

static void rna_NodeSocket_link_limit_set(PointerRNA *ptr, int value)
{
	bNodeSocket *sock = ptr->data;
	sock->limit = (value == 0 ? 0xFFF : value);
}

static void rna_NodeSocket_hide_set(PointerRNA *ptr, int value)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	
	/* don't hide linked sockets */
	if (sock->flag & SOCK_IN_USE)
		return;
	
	if (value)
		sock->flag |= SOCK_HIDDEN;
	else
		sock->flag &= ~SOCK_HIDDEN;
}


static void rna_NodeSocketInterface_draw(bContext *C, struct uiLayout *layout, PointerRNA *ptr)
{
	extern FunctionRNA rna_NodeSocketInterface_draw_func;

	bNodeSocket *stemp = (bNodeSocket *)ptr->data;
	ParameterList list;
	FunctionRNA *func;

	if (!stemp->typeinfo)
		return;

	func = &rna_NodeSocketInterface_draw_func; /* RNA_struct_find_function(&ptr, "draw"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	RNA_parameter_set_lookup(&list, "layout", &layout);
	stemp->typeinfo->ext_interface.call(C, ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_draw_color(bContext *C, PointerRNA *ptr, float *r_color)
{
	extern FunctionRNA rna_NodeSocketInterface_draw_color_func;

	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	ParameterList list;
	FunctionRNA *func;
	void *ret;

	if (!sock->typeinfo)
		return;

	func = &rna_NodeSocketInterface_draw_color_func; /* RNA_struct_find_function(&ptr, "draw_color"); */

	RNA_parameter_list_create(&list, ptr, func);
	RNA_parameter_set_lookup(&list, "context", &C);
	sock->typeinfo->ext_interface.call(C, ptr, func, &list);

	RNA_parameter_get_lookup(&list, "color", &ret);
	copy_v4_v4(r_color, (float *)ret);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_register_properties(bNodeTree *ntree, bNodeSocket *stemp, StructRNA *data_srna)
{
	extern FunctionRNA rna_NodeSocketInterface_register_properties_func;

	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	if (!stemp->typeinfo)
		return;

	RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, stemp, &ptr);
	func = &rna_NodeSocketInterface_register_properties_func; /* RNA_struct_find_function(&ptr, "register_properties"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "data_rna_type", &data_srna);
	stemp->typeinfo->ext_interface.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_init_socket(bNodeTree *ntree, bNodeSocket *stemp, bNode *node, bNodeSocket *sock, const char *data_path)
{
	extern FunctionRNA rna_NodeSocketInterface_init_socket_func;

	PointerRNA ptr, node_ptr, sock_ptr;
	ParameterList list;
	FunctionRNA *func;

	if (!stemp->typeinfo)
		return;

	RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, stemp, &ptr);
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &node_ptr);
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &sock_ptr);
	func = &rna_NodeSocketInterface_init_socket_func; /* RNA_struct_find_function(&ptr, "init_socket"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node", &node_ptr);
	RNA_parameter_set_lookup(&list, "socket", &sock_ptr);
	RNA_parameter_set_lookup(&list, "data_path", &data_path);
	stemp->typeinfo->ext_interface.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_from_socket(bNodeTree *ntree, bNodeSocket *stemp, bNode *node, bNodeSocket *sock)
{
	extern FunctionRNA rna_NodeSocketInterface_from_socket_func;

	PointerRNA ptr, node_ptr, sock_ptr;
	ParameterList list;
	FunctionRNA *func;

	if (!stemp->typeinfo)
		return;

	RNA_pointer_create((ID *)ntree, &RNA_NodeSocketInterface, stemp, &ptr);
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &node_ptr);
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &sock_ptr);
	func = &rna_NodeSocketInterface_from_socket_func; /* RNA_struct_find_function(&ptr, "from_socket"); */

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node", &node_ptr);
	RNA_parameter_set_lookup(&list, "socket", &sock_ptr);
	stemp->typeinfo->ext_interface.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void rna_NodeSocketInterface_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	bNodeSocketType *st = RNA_struct_blender_type_get(type);
	if (!st)
		return;
	
	RNA_struct_free_extension(type, &st->ext_interface);
	
	RNA_struct_free(&BLENDER_RNA, type);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
}

static StructRNA *rna_NodeSocketInterface_register(
        Main *UNUSED(bmain), ReportList *UNUSED(reports), void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeSocketType *st, dummyst;
	bNodeSocket dummysock;
	PointerRNA dummyptr;
	int have_function[5];

	/* setup dummy socket & socket type to store static properties in */
	memset(&dummyst, 0, sizeof(bNodeSocketType));
	
	memset(&dummysock, 0, sizeof(bNodeSocket));
	dummysock.typeinfo = &dummyst;
	RNA_pointer_create(NULL, &RNA_NodeSocketInterface, &dummysock, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0)
		return NULL;

	/* check if we have registered this socket type before */
	st = nodeSocketTypeFind(dummyst.idname);
	if (st) {
		/* basic socket type registered by a socket class before. */
	}
	else {
		/* create a new node socket type */
		st = MEM_callocN(sizeof(bNodeSocketType), "node socket type");
		memcpy(st, &dummyst, sizeof(dummyst));
		
		nodeRegisterSocketType(st);
	}
	
	/* if RNA type is already registered, unregister first */
	if (st->ext_interface.srna) {
		StructRNA *srna = st->ext_interface.srna;
		RNA_struct_free_extension(srna, &st->ext_interface);
		RNA_struct_free(&BLENDER_RNA, srna);
	}
	st->ext_interface.srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_NodeSocketInterface); 
	st->ext_interface.data = data;
	st->ext_interface.call = call;
	st->ext_interface.free = free;
	RNA_struct_blender_type_set(st->ext_interface.srna, st);
	
	st->interface_draw = (have_function[0]) ? rna_NodeSocketInterface_draw : NULL;
	st->interface_draw_color = (have_function[1]) ? rna_NodeSocketInterface_draw_color : NULL;
	st->interface_register_properties = (have_function[2]) ? rna_NodeSocketInterface_register_properties : NULL;
	st->interface_init_socket = (have_function[3]) ? rna_NodeSocketInterface_init_socket : NULL;
	st->interface_from_socket = (have_function[4]) ? rna_NodeSocketInterface_from_socket : NULL;
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return st->ext_interface.srna;
}

static StructRNA *rna_NodeSocketInterface_refine(PointerRNA *ptr)
{
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	
	if (sock->typeinfo && sock->typeinfo->ext_interface.srna)
		return sock->typeinfo->ext_interface.srna;
	else
		return &RNA_NodeSocketInterface;
}

static char *rna_NodeSocketInterface_path(PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNodeSocket *sock = (bNodeSocket *)ptr->data;
	int socketindex;
	
	socketindex = BLI_findindex(&ntree->inputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("inputs[%d]", socketindex);
	
	socketindex = BLI_findindex(&ntree->outputs, sock);
	if (socketindex != -1)
		return BLI_sprintfN("outputs[%d]", socketindex);
	
	return NULL;
}

static IDProperty *rna_NodeSocketInterface_idprops(PointerRNA *ptr, bool create)
{
	bNodeSocket *sock = ptr->data;
	
	if (create && !sock->prop) {
		IDPropertyTemplate val = {0};
		sock->prop = IDP_New(IDP_GROUP, &val, "RNA_NodeSocketInterface ID properties");
	}
	
	return sock->prop;
}

static void rna_NodeSocketInterface_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = ptr->id.data;
	bNodeSocket *stemp = ptr->data;
	
	if (!stemp->typeinfo)
		return;
	
	ntree->update |= NTREE_UPDATE_GROUP;
	ntreeUpdateTree(G.main, ntree);
	
	ED_node_tag_update_nodetree(bmain, ntree);
}


/* ******** Standard Node Socket Base Types ******** */

static void rna_NodeSocketStandard_draw(ID *id, bNodeSocket *sock, struct bContext *C, struct uiLayout *layout, PointerRNA *nodeptr,
                                        const char *text)
{
	PointerRNA ptr;
	RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
	sock->typeinfo->draw(C, layout, &ptr, nodeptr, text);
}

static void rna_NodeSocketStandard_draw_color(ID *id, bNodeSocket *sock, struct bContext *C, PointerRNA *nodeptr, float *r_color)
{
	PointerRNA ptr;
	RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
	sock->typeinfo->draw_color(C, &ptr, nodeptr, r_color);
}

static void rna_NodeSocketInterfaceStandard_draw(ID *id, bNodeSocket *sock, struct bContext *C, struct uiLayout *layout)
{
	PointerRNA ptr;
	RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
	sock->typeinfo->interface_draw(C, layout, &ptr);
}

static void rna_NodeSocketInterfaceStandard_draw_color(ID *id, bNodeSocket *sock, struct bContext *C, float *r_color)
{
	PointerRNA ptr;
	RNA_pointer_create(id, &RNA_NodeSocket, sock, &ptr);
	sock->typeinfo->interface_draw_color(C, &ptr, r_color);
}

static void rna_NodeSocketStandard_float_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNodeSocket *sock = ptr->data;
	bNodeSocketValueFloat *dval = sock->default_value;
	int subtype = sock->typeinfo->subtype;
	
	*min = (subtype == PROP_UNSIGNED ? 0.0f : -FLT_MAX);
	*max = FLT_MAX;
	*softmin = dval->min;
	*softmax = dval->max;
}

static void rna_NodeSocketStandard_int_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bNodeSocket *sock = ptr->data;
	bNodeSocketValueInt *dval = sock->default_value;
	int subtype = sock->typeinfo->subtype;
	
	*min = (subtype == PROP_UNSIGNED ? 0 : INT_MIN);
	*max = INT_MAX;
	*softmin = dval->min;
	*softmax = dval->max;
}

static void rna_NodeSocketStandard_vector_range(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
	bNodeSocket *sock = ptr->data;
	bNodeSocketValueVector *dval = sock->default_value;
	
	*min = -FLT_MAX;
	*max = FLT_MAX;
	*softmin = dval->min;
	*softmax = dval->max;
}

/* using a context update function here, to avoid searching the node if possible */
static void rna_NodeSocketStandard_value_update(struct bContext *C, PointerRNA *ptr)
{
	bNode *node;
	
	/* default update */
	rna_NodeSocket_update(CTX_data_main(C), CTX_data_scene(C), ptr);
	
	/* try to use node from context, faster */
	node = CTX_data_pointer_get(C, "node").data;
	if (!node) {
		bNodeTree *ntree = ptr->id.data;
		bNodeSocket *sock = ptr->data;
		
		/* fall back to searching node in the tree */
		nodeFindNode(ntree, sock, &node, NULL);
	}
	
	if (node)
		nodeSynchronizeID(node, true);
}


/* ******** Node Types ******** */

static void rna_NodeInternalSocketTemplate_name_get(PointerRNA *ptr, char *value)
{
	bNodeSocketTemplate *stemp = ptr->data;
	strcpy(value, stemp->name);
}

static int rna_NodeInternalSocketTemplate_name_length(PointerRNA *ptr)
{
	bNodeSocketTemplate *stemp = ptr->data;
	return strlen(stemp->name);
}

static void rna_NodeInternalSocketTemplate_identifier_get(PointerRNA *ptr, char *value)
{
	bNodeSocketTemplate *stemp = ptr->data;
	strcpy(value, stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_identifier_length(PointerRNA *ptr)
{
	bNodeSocketTemplate *stemp = ptr->data;
	return strlen(stemp->identifier);
}

static int rna_NodeInternalSocketTemplate_type_get(PointerRNA *ptr)
{
	bNodeSocketTemplate *stemp = ptr->data;
	return stemp->type;
}

static PointerRNA rna_NodeInternal_input_template(StructRNA *srna, int index)
{
	bNodeType *ntype = RNA_struct_blender_type_get(srna);
	if (ntype && ntype->inputs) {
		bNodeSocketTemplate *stemp = ntype->inputs;
		int i = 0;
		while (i < index && stemp->type >= 0) {
			++i;
			++stemp;
		}
		if (i == index && stemp->type >= 0) {
			PointerRNA ptr;
			RNA_pointer_create(NULL, &RNA_NodeInternalSocketTemplate, stemp, &ptr);
			return ptr;
		}
	}
	return PointerRNA_NULL;
}

static PointerRNA rna_NodeInternal_output_template(StructRNA *srna, int index)
{
	bNodeType *ntype = RNA_struct_blender_type_get(srna);
	if (ntype && ntype->outputs) {
		bNodeSocketTemplate *stemp = ntype->outputs;
		int i = 0;
		while (i < index && stemp->type >= 0) {
			++i;
			++stemp;
		}
		if (i == index && stemp->type >= 0) {
			PointerRNA ptr;
			RNA_pointer_create(NULL, &RNA_NodeInternalSocketTemplate, stemp, &ptr);
			return ptr;
		}
	}
	return PointerRNA_NULL;
}

static int rna_NodeInternal_poll(StructRNA *srna, bNodeTree *ntree)
{
	bNodeType *ntype = RNA_struct_blender_type_get(srna);
	return ntype && (!ntype->poll || ntype->poll(ntype, ntree));
}

static int rna_NodeInternal_poll_instance(bNode *node, bNodeTree *ntree)
{
	bNodeType *ntype = node->typeinfo;
	if (ntype->poll_instance) {
		return ntype->poll_instance(node, ntree);
	}
	else {
		/* fall back to basic poll function */
		return !ntype->poll || ntype->poll(ntype, ntree);
	}
}

static void rna_NodeInternal_update(ID *id, bNode *node)
{
	bNodeTree *ntree = (bNodeTree *)id;
	if (node->typeinfo->updatefunc)
		node->typeinfo->updatefunc(ntree, node);
}

static void rna_NodeInternal_draw_buttons(ID *id, bNode *node, struct bContext *C, struct uiLayout *layout)
{
	if (node->typeinfo->draw_buttons) {
		PointerRNA ptr;
		RNA_pointer_create(id, &RNA_Node, node, &ptr);
		node->typeinfo->draw_buttons(layout, C, &ptr);
	}
}

static void rna_NodeInternal_draw_buttons_ext(ID *id, bNode *node, struct bContext *C, struct uiLayout *layout)
{
	if (node->typeinfo->draw_buttons_ex) {
		PointerRNA ptr;
		RNA_pointer_create(id, &RNA_Node, node, &ptr);
		node->typeinfo->draw_buttons_ex(layout, C, &ptr);
	}
	else if (node->typeinfo->draw_buttons) {
		PointerRNA ptr;
		RNA_pointer_create(id, &RNA_Node, node, &ptr);
		node->typeinfo->draw_buttons(layout, C, &ptr);
	}
}

static StructRNA *rna_NodeCustomGroup_register(
        Main *bmain, ReportList *reports,
        void *data, const char *identifier,
        StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	bNodeType *nt = rna_Node_register_base(bmain, reports, &RNA_NodeCustomGroup, data, identifier, validate, call, free);
	if (!nt)
		return NULL;
	
	/* this updates the group node instance from the tree's interface */
	nt->verifyfunc = node_group_verify;
	
	nodeRegisterType(nt);
	
	/* update while blender is running */
	WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
	
	return nt->ext.srna;
}

static void rna_CompositorNode_tag_need_exec(bNode *node)
{
	node->need_exec = true;
}

static void rna_Node_tex_image_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;

	ED_node_tag_update_nodetree(bmain, ntree);
	WM_main_add_notifier(NC_IMAGE, NULL);
}

static void rna_Node_material_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	if (node->id)
		nodeSetActive(ntree, node);

	ED_node_tag_update_nodetree(bmain, ntree);
}

static void rna_NodeGroup_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;
	
	if (node->id)
		ntreeUpdateTree(bmain, (bNodeTree *)node->id);
	
	ED_node_tag_update_nodetree(bmain, ntree);
}

static void rna_NodeGroup_node_tree_set(PointerRNA *ptr, const PointerRNA value)
{
	bNodeTree *ntree = ptr->id.data;
	bNode *node = ptr->data;
	bNodeTree *ngroup = value.data;
	
	if (nodeGroupPoll(ntree, ngroup)) {
		if (node->id)
			id_us_min(node->id);
		if (ngroup)
			id_us_plus(&ngroup->id);
		
		node->id = &ngroup->id;
	}
}

static int rna_NodeGroup_node_tree_poll(PointerRNA *ptr, const PointerRNA value)
{
	bNodeTree *ntree = ptr->id.data;
	bNodeTree *ngroup = value.data;
	
	/* only allow node trees of the same type as the group node's tree */
	if (ngroup->type != ntree->type)
		return false;
	
	return nodeGroupPoll(ntree, ngroup);
}


static StructRNA *rna_NodeGroup_interface_typef(PointerRNA *ptr)
{
	bNode *node = ptr->data;
	bNodeTree *ngroup = (bNodeTree *)node->id;

	if (ngroup) {
		StructRNA *srna = ntreeInterfaceTypeGet(ngroup, true);
		if (srna)
			return srna;
	}
	return &RNA_PropertyGroup;
}

static StructRNA *rna_NodeGroupInputOutput_interface_typef(PointerRNA *ptr)
{
	bNodeTree *ntree = ptr->id.data;
	
	if (ntree) {
		StructRNA *srna = ntreeInterfaceTypeGet(ntree, true);
		if (srna)
			return srna;
	}
	return &RNA_PropertyGroup;
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
                                                    PropertyRNA *UNUSED(prop), bool *r_free)
{
	bNode *node = (bNode *)ptr->data;
	Image *ima = (Image *)node->id;
	EnumPropertyItem *item = NULL;
	RenderLayer *rl;
	
	if (ima && ima->rr) {
		rl = ima->rr->layers.first;
		item = renderresult_layers_add_enum(rl);
	}
	else {
		int totitem = 0;
		RNA_enum_item_end(&item, &totitem);
	}
	
	*r_free = true;
	
	return item;
}

static EnumPropertyItem *rna_Node_scene_layer_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                    PropertyRNA *UNUSED(prop), bool *r_free)
{
	bNode *node = (bNode *)ptr->data;
	Scene *sce = (Scene *)node->id;
	EnumPropertyItem *item = NULL;
	RenderLayer *rl;
	
	if (sce) {
		rl = sce->r.layers.first;
		item = renderresult_layers_add_enum(rl);
	}
	else {
		int totitem = 0;
		RNA_enum_item_end(&item, &totitem);
	}
	
	*r_free = true;
	
	return item;
}

static EnumPropertyItem *rna_Node_channel_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                PropertyRNA *UNUSED(prop), bool *r_free)
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
	*r_free = true;
	
	return item;
}

static void rna_Image_Node_update_id(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	node->update |= NODE_UPDATE_ID;
	nodeUpdate(ntree, node);	/* to update image node sockets */
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

static void rna_NodeColorBalance_update_lgg(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ntreeCompositColorBalanceSyncFromLGG(ptr->id.data, ptr->data);
	rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeColorBalance_update_cdl(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ntreeCompositColorBalanceSyncFromCDL(ptr->id.data, ptr->data);
	rna_Node_update(bmain, scene, ptr);
}

/* ******** Node Socket Types ******** */

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

static bNodeSocket *rna_NodeOutputFile_slots_new(ID *id, bNode *node, bContext *C, ReportList *UNUSED(reports), const char *name)
{
	bNodeTree *ntree = (bNodeTree *)id;
	Scene *scene = CTX_data_scene(C);
	ImageFormatData *im_format = NULL;
	bNodeSocket *sock;
	if (scene)
		im_format = &scene->r.im_format;
	
	sock = ntreeCompositOutputFileAddSocket(ntree, node, name, im_format);
	
	ntreeUpdateTree(CTX_data_main(C), ntree);
	WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
	
	return sock;
}

static void rna_ShaderNodeScript_mode_set(PointerRNA *ptr, int value)
{
	bNode *node = (bNode *)ptr->data;
	NodeShaderScript *nss = node->storage;

	if (nss->mode != value) {
		nss->mode = value;
		nss->filepath[0] = '\0';
		nss->flag &= ~NODE_SCRIPT_AUTO_UPDATE;

		/* replace text datablock by filepath */
		if (node->id) {
			Text *text = (Text *)node->id;

			if (value == NODE_SCRIPT_EXTERNAL && text->name) {
				BLI_strncpy(nss->filepath, text->name, sizeof(nss->filepath));
				BLI_path_rel(nss->filepath, G.main->name);
			}

			id_us_min(node->id);
			node->id = NULL;
		}

		/* remove any bytecode */
		if (nss->bytecode) {
			MEM_freeN(nss->bytecode);
			nss->bytecode = NULL;
		}

		nss->bytecode_hash[0] = '\0';
	}
}

static void rna_ShaderNodeScript_bytecode_get(PointerRNA *ptr, char *value)
{
	bNode *node = (bNode *)ptr->data;
	NodeShaderScript *nss = node->storage;

	strcpy(value, (nss->bytecode) ? nss->bytecode : "");
}

static int rna_ShaderNodeScript_bytecode_length(PointerRNA *ptr)
{
	bNode *node = (bNode *)ptr->data;
	NodeShaderScript *nss = node->storage;

	return (nss->bytecode) ? strlen(nss->bytecode) : 0;
}

static void rna_ShaderNodeScript_bytecode_set(PointerRNA *ptr, const char *value)
{
	bNode *node = (bNode *)ptr->data;
	NodeShaderScript *nss = node->storage;

	if (nss->bytecode)
		MEM_freeN(nss->bytecode);

	if (value && value[0])
		nss->bytecode = BLI_strdup(value);
	else
		nss->bytecode = NULL;
}

static void rna_ShaderNodeScript_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;
	RenderEngineType *engine_type = RE_engines_find(scene->r.engine);

	if (engine_type && engine_type->update_script_node) {
		/* auto update node */
		RenderEngine *engine = RE_engine_create(engine_type);
		engine_type->update_script_node(engine, ntree, node);
		RE_engine_free(engine);
	}

	ED_node_tag_update_nodetree(bmain, ntree);
}

static void rna_ShaderNodeSubsurface_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	nodeUpdate(ntree, node);
	rna_Node_update(bmain, scene, ptr);
}

static void rna_CompositorNodeScale_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bNodeTree *ntree = (bNodeTree *)ptr->id.data;
	bNode *node = (bNode *)ptr->data;

	nodeUpdate(ntree, node);
	rna_Node_update(bmain, scene, ptr);
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
	{SHD_GLOSSY_SHARP,             "SHARP",             0, "Sharp",    ""},
	{SHD_GLOSSY_BECKMANN,          "BECKMANN",          0, "Beckmann", ""},
	{SHD_GLOSSY_GGX,               "GGX",               0, "GGX",      ""},
	{SHD_GLOSSY_ASHIKHMIN_SHIRLEY, "ASHIKHMIN_SHIRLEY", 0, "Ashikhmin-Shirley", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_anisotropic_items[] = {
	{SHD_GLOSSY_BECKMANN,          "BECKMANN",          0, "Beckmann", ""},
	{SHD_GLOSSY_GGX,               "GGX",               0, "GGX",      ""},
	{SHD_GLOSSY_ASHIKHMIN_SHIRLEY, "ASHIKHMIN_SHIRLEY", 0, "Ashikhmin-Shirley", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_glass_items[] = {
	{SHD_GLOSSY_SHARP,             "SHARP",             0, "Sharp",    ""},
	{SHD_GLOSSY_BECKMANN,          "BECKMANN",          0, "Beckmann", ""},
	{SHD_GLOSSY_GGX,               "GGX",               0, "GGX",      ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_toon_items[] = {
	{SHD_TOON_DIFFUSE,    "DIFFUSE",  0, "Diffuse", ""},
	{SHD_TOON_GLOSSY,     "GLOSSY",   0, "Glossy",  ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_hair_items[] = {
	{SHD_HAIR_REFLECTION,     "Reflection",    0,   "Reflection", ""},
	{SHD_HAIR_TRANSMISSION,   "Transmission",    0,  "Transmission", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem node_script_mode_items[] = {
	{NODE_SCRIPT_INTERNAL, "INTERNAL", 0, "Internal", "Use internal text datablock"},
	{NODE_SCRIPT_EXTERNAL, "EXTERNAL", 0, "External", "Use external .osl or .oso file"},
	{0, NULL, 0, NULL, NULL}
};

/* -- Common nodes ---------------------------------------------------------- */

static void def_group_input(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "interface", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_NodeGroupInputOutput_interface_typef", NULL);
	RNA_def_property_struct_type(prop, "PropertyGroup");
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Interface", "Interface socket data");
}

static void def_group_output(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "interface", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_NodeGroupInputOutput_interface_typef", NULL);
	RNA_def_property_struct_type(prop, "PropertyGroup");
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Interface", "Interface socket data");
	
	prop = RNA_def_property(srna, "is_active_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_DO_OUTPUT);
	RNA_def_property_ui_text(prop, "Active Output", "True if this node is used as the active group output");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_group(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "NodeTree");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_NodeGroup_node_tree_set", NULL, "rna_NodeGroup_node_tree_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeGroup_update");

	prop = RNA_def_property(srna, "interface", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_NodeGroup_interface_typef", NULL);
	RNA_def_property_struct_type(prop, "PropertyGroup");
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Interface", "Interface socket data");
}

static void def_custom_group(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "NodeCustomGroup", "Node");
	RNA_def_struct_ui_text(srna, "Custom Group", "Base node type for custom registered node group types");
	RNA_def_struct_sdna(srna, "bNode");

	RNA_def_struct_register_funcs(srna, "rna_NodeCustomGroup_register", "rna_Node_unregister", NULL);

	def_group(srna);
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

static void def_sh_output(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "is_active_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_DO_OUTPUT);
	RNA_def_property_ui_text(prop, "Active Output", "True if this node is used as the active output");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_output_linestyle(StructRNA *srna)
{
	def_sh_output(srna);
	def_mix_rgb(srna);
}

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
	static EnumPropertyItem prop_vect_type_items[] = {
		{TEXMAP_TYPE_TEXTURE, "TEXTURE", 0, "Texture", "Transform a texture by inverse mapping the texture coordinate"},
		{TEXMAP_TYPE_POINT,   "POINT",   0, "Point",   "Transform a point"},
		{TEXMAP_TYPE_VECTOR,  "VECTOR",  0, "Vector",  "Transform a direction vector"},
		{TEXMAP_TYPE_NORMAL,  "NORMAL",  0, "Normal",  "Transform a normal vector with unit length"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "TexMapping", "storage");

	prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_vect_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of vector that the mapping transforms");
	RNA_def_property_update(prop, 0, "rna_Mapping_Node_update");

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
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
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

static void def_sh_lamp(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "lamp_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Lamp_object_poll");
	RNA_def_property_ui_text(prop, "Lamp Object", "");
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
	static EnumPropertyItem prop_sky_type[] = {
		{SHD_SKY_OLD, "PREETHAM", 0, "Preetham", ""},
		{SHD_SKY_NEW, "HOSEK_WILKIE", 0, "Hosek / Wilkie", ""},
		{0, NULL, 0, NULL, NULL}
	};
	static float default_dir[3] = {0.0f, 0.0f, 1.0f};
	
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexSky", "storage");
	def_sh_tex(srna);

	prop = RNA_def_property(srna, "sky_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sky_model");
	RNA_def_property_enum_items(prop, prop_sky_type);
	RNA_def_property_ui_text(prop, "Sky Type", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "sun_direction", PROP_FLOAT, PROP_DIRECTION);
	RNA_def_property_ui_text(prop, "Sun Direction", "Direction from where the sun is shining");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_dir);
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "turbidity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 10.0f);
	RNA_def_property_ui_range(prop, 1.0f, 10.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Turbidity", "Atmospheric turbidity");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "ground_albedo", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ground Albedo", "Ground color that is subtly reflected in the sky");
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

	static const EnumPropertyItem prop_projection_items[] = {
		{SHD_PROJ_FLAT, "FLAT", 0, "Flat",
		                "Image is projected flat using the X and Y coordinates of the texture vector"},
		{SHD_PROJ_BOX,  "BOX", 0, "Box",
		                "Image is projected using different components for each side of the object space bounding box"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_interpolation_items[] = {
		{SHD_INTERP_LINEAR,  "Linear", 0, "Linear",
		                     "Linear interpolation"},
		{SHD_INTERP_CLOSEST, "Closest", 0, "Closest",
		                     "No interpolation (sample closest texel)"},
		{SHD_INTERP_CUBIC,   "Cubic", 0, "Cubic",
		                     "Cubic interpolation (OSL only)"},
		{SHD_INTERP_SMART,   "Smart", 0, "Smart",
		                     "Bicubic when magnifying, else bilinear (OSL only)"},
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

	prop = RNA_def_property(srna, "projection", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_projection_items);
	RNA_def_property_ui_text(prop, "Projection", "Method to project 2D image on object with a 3D texture vector");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_interpolation_items);
	RNA_def_property_ui_text(prop, "Interpolation", "Texture interpolation");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "projection_blend", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_ui_text(prop, "Projection Blend", "For box projection, amount of blend to use between sides");
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

static void def_sh_tex_brick(StructRNA *srna)
{
	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeTexBrick", "storage");
	def_sh_tex(srna);
	
	prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "offset_freq");
	RNA_def_property_int_default(prop, 2);
	RNA_def_property_range(prop, 1, 99);
	RNA_def_property_ui_text(prop, "Offset Frequency", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "squash_freq");
	RNA_def_property_int_default(prop, 2);
	RNA_def_property_range(prop, 1, 99);
	RNA_def_property_ui_text(prop, "Squash Frequency", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Offset Amount", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "squash");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.0f, 99.0f);
	RNA_def_property_ui_text(prop, "Squash Amount", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
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

static void def_sh_tex_coord(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "from_dupli", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "From Dupli", "Use the parent of the dupli object if possible");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_vect_transform(StructRNA *srna)
{
	static EnumPropertyItem prop_vect_type_items[] = {
		{SHD_VECT_TRANSFORM_TYPE_POINT,  "POINT",   0, "Point",    "Transform a point"},
		{SHD_VECT_TRANSFORM_TYPE_VECTOR, "VECTOR",  0, "Vector",   "Transform a direction vector"},
		{SHD_VECT_TRANSFORM_TYPE_NORMAL, "NORMAL",  0, "Normal",   "Transform a normal vector with unit length"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_vect_space_items[] = {
		{SHD_VECT_TRANSFORM_SPACE_WORLD,  "WORLD",   0, "World",    ""},
		{SHD_VECT_TRANSFORM_SPACE_OBJECT, "OBJECT",  0, "Object",   ""},
		{SHD_VECT_TRANSFORM_SPACE_CAMERA, "CAMERA",  0, "Camera",   ""},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	RNA_def_struct_sdna_from(srna, "NodeShaderVectTransform", "storage");
	
	prop = RNA_def_property(srna, "vector_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_vect_type_items);
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "convert_from", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_vect_space_items);
	RNA_def_property_ui_text(prop, "Convert From", "Space to convert from");
	RNA_def_property_update(prop, 0, "rna_Node_update");
	
	prop = RNA_def_property(srna, "convert_to", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_vect_space_items);
	RNA_def_property_ui_text(prop, "Convert To", "Space to convert to");
	RNA_def_property_update(prop, 0, "rna_Node_update");
}

static void def_sh_tex_wireframe(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "use_pixel_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Pixel Size", "Use screen pixel size instead of world units");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
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

static void def_glass(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_glass_items);
	RNA_def_property_ui_text(prop, "Distribution", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_anisotropic(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_anisotropic_items);
	RNA_def_property_ui_text(prop, "Distribution", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_toon(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_toon_items);
	RNA_def_property_ui_text(prop, "Component", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_bump(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Invert", "Invert the bump mapping direction to push into the surface instead of out");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_hair(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "component", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_hair_items);
	RNA_def_property_ui_text(prop, "Component", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_sh_uvmap(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "from_dupli", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "From Dupli", "Use the parent of the dupli object if possible");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeShaderUVMap", "storage");

	prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "UV Map", "UV coordinates to be used for mapping");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "bNode", NULL);
}

static void def_sh_normal_map(StructRNA *srna)
{
	static EnumPropertyItem prop_space_items[] = {
		{SHD_NORMAL_MAP_TANGENT, "TANGENT", 0, "Tangent Space", "Tangent space normal mapping"},
		{SHD_NORMAL_MAP_OBJECT, "OBJECT", 0, "Object Space", "Object space normal mapping"},
		{SHD_NORMAL_MAP_WORLD, "WORLD", 0, "World Space", "World space normal mapping"},
		{SHD_NORMAL_MAP_BLENDER_OBJECT, "BLENDER_OBJECT", 0, "Blender Object Space", "Object space normal mapping, compatible with Blender render baking"},
		{SHD_NORMAL_MAP_BLENDER_WORLD, "BLENDER_WORLD", 0, "Blender World Space", "World space normal mapping, compatible with Blender render baking"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeShaderNormalMap", "storage");

	prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_space_items);
	RNA_def_property_ui_text(prop, "Space", "Space of the input normal");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "UV Map", "UV Map for tangent space maps");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "bNode", NULL);
}

static void def_sh_tangent(StructRNA *srna)
{
	static EnumPropertyItem prop_direction_type_items[] = {
		{SHD_TANGENT_RADIAL, "RADIAL", 0, "Radial", "Radial tangent around the X, Y or Z axis"},
		{SHD_TANGENT_UVMAP, "UV_MAP", 0, "UV Map", "Tangent from UV map"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_axis_items[] = {
		{SHD_TANGENT_AXIS_X, "X", 0, "X", "X axis"},
		{SHD_TANGENT_AXIS_Y, "Y", 0, "Y", "Y axis"},
		{SHD_TANGENT_AXIS_Z, "Z", 0, "Z", "Z axis"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeShaderTangent", "storage");

	prop = RNA_def_property(srna, "direction_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_direction_type_items);
	RNA_def_property_ui_text(prop, "Direction", "Method to use for the tangent");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Axis for radial tangents");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "uv_map", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "UV Map", "UV Map for tangent generated from UV");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "bNode", NULL);
}


static void def_sh_subsurface(StructRNA *srna)
{
	static EnumPropertyItem prop_subsurface_falloff_items[] = {
		{SHD_SUBSURFACE_CUBIC, "CUBIC", 0, "Cubic", "Simple cubic falloff function"},
		{SHD_SUBSURFACE_GAUSSIAN, "GAUSSIAN", 0, "Gaussian", "Normal distribution, multiple can be combined to fit more complex profiles"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, prop_subsurface_falloff_items);
	RNA_def_property_ui_text(prop, "Falloff", "Function to determine how much light nearby points contribute based on their distance to the shading point");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNodeSubsurface_update");
}

static void def_sh_script(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "script", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
	RNA_def_property_ui_text(prop, "Script", "Internal shader script to define the shader");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNodeScript_update");
	
	RNA_def_struct_sdna_from(srna, "NodeShaderScript", "storage");
	
	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File Path", "Shader script path");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_ShaderNodeScript_update");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ShaderNodeScript_mode_set", NULL);
	RNA_def_property_enum_items(prop, node_script_mode_items);
	RNA_def_property_ui_text(prop, "Script Source", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "use_auto_update", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_SCRIPT_AUTO_UPDATE);
	RNA_def_property_ui_text(prop, "Auto Update",
	                         "Automatically update the shader when the .osl file changes (external scripts only)");
	
	prop = RNA_def_property(srna, "bytecode", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ShaderNodeScript_bytecode_get", "rna_ShaderNodeScript_bytecode_length",
	                              "rna_ShaderNodeScript_bytecode_set");
	RNA_def_property_ui_text(prop, "Bytecode", "Compile bytecode for shader script node");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "bytecode_hash", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Bytecode Hash", "Hash of compile bytecode, for quick equality checking");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	/* needs to be reset to avoid bad pointer type in API functions below */
	RNA_def_struct_sdna_from(srna, "bNode", NULL);
	
	/* API functions */

#if 0	/* XXX TODO use general node api for this */
	func = RNA_def_function(srna, "find_socket", "rna_ShaderNodeScript_find_socket");
	RNA_def_function_ui_description(func, "Find a socket by name");
	parm = RNA_def_string(func, "name", NULL, 0, "Socket name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/*parm =*/ RNA_def_boolean(func, "is_output", false, "Output", "Whether the socket is an output");
	parm = RNA_def_pointer(func, "result", "NodeSocket", "", "");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "add_socket", "rna_ShaderNodeScript_add_socket");
	RNA_def_function_ui_description(func, "Add a socket socket");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_enum(func, "type", node_socket_type_items, SOCK_FLOAT, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/*parm =*/ RNA_def_boolean(func, "is_output", false, "Output", "Whether the socket is an output");
	parm = RNA_def_pointer(func, "result", "NodeSocket", "", "");
	RNA_def_function_return(func, parm);
	
	func = RNA_def_function(srna, "remove_socket", "rna_ShaderNodeScript_remove_socket");
	RNA_def_function_ui_description(func, "Remove a socket socket");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "sock", "NodeSocket", "Socket", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
#endif
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
	
	prop = RNA_def_property(srna, "premul", PROP_FLOAT, PROP_FACTOR);
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

	/* duplicated in def_cmp_bokehblur */
	prop = RNA_def_property(srna, "use_variable_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_NODEFLAG_BLUR_VARIABLE_SIZE);
	RNA_def_property_ui_text(prop, "Variable Size", "Support variable blur per-pixel when using an image for size input");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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

static void def_cmp_map_range(StructRNA *srna)
{
	PropertyRNA *prop;
	
	prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 1);
	RNA_def_property_ui_text(prop, "Clamp", "Clamp result of the node to 0..1 range");
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
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
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
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Image_Node_update_id");

	prop = RNA_def_property(srna, "use_straight_alpha_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT);
	RNA_def_property_ui_text(prop, "Straight Alpha Output", "Put Node output buffer to straight alpha instead of premultiplied");
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
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Scene", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, prop_scene_layer_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Node_scene_layer_itemf");
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
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
	
	prop = RNA_def_property(srna, "path", PROP_STRING, PROP_NONE);
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
static void rna_def_cmp_output_file_slots_api(BlenderRNA *brna, PropertyRNA *cprop, const char *struct_name)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, struct_name);
	srna = RNA_def_struct(brna, struct_name, NULL);
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_ui_text(srna, "File Output Slots", "Collection of File Output node slots");

	func = RNA_def_function(srna, "new", "rna_NodeOutputFile_slots_new");
	RNA_def_function_ui_description(func, "Add a file slot to this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
	RNA_def_function_return(func, parm);

	/* NB: methods below can use the standard node socket API functions,
	 * included here for completeness.
	 */

	func = RNA_def_function(srna, "remove", "rna_Node_socket_remove");
	RNA_def_function_ui_description(func, "Remove a file slot from this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "The socket to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", "rna_Node_inputs_clear");
	RNA_def_function_ui_description(func, "Remove all file slots from this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);

	func = RNA_def_function(srna, "move", "rna_Node_inputs_move");
	RNA_def_function_ui_description(func, "Move a file slot to another position");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_int(func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the socket to move", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the socket", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
static void def_cmp_output_file(BlenderRNA *brna, StructRNA *srna)
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
	rna_def_cmp_output_file_slots_api(brna, prop, "CompositorNodeOutputFileFileSlots");
	
	prop = RNA_def_property(srna, "layer_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_NodeOutputFile_slots_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end",
	                                  "rna_NodeOutputFile_slot_layer_get", NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "NodeOutputFileSlotLayer");
	RNA_def_property_ui_text(prop, "EXR Layer Slots", "");
	rna_def_cmp_output_file_slots_api(brna, prop, "CompositorNodeOutputFileLayerSlots");
}

static void def_cmp_dilate_erode(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem mode_items[] = {
	    {CMP_NODE_DILATEERODE_STEP,             "STEP",      0, "Step",      ""},
	    {CMP_NODE_DILATEERODE_DISTANCE_THRESH,  "THRESHOLD", 0, "Threshold", ""},
	    {CMP_NODE_DILATEERODE_DISTANCE,         "DISTANCE",  0, "Distance",  ""},
	    {CMP_NODE_DILATEERODE_DISTANCE_FEATHER, "FEATHER",   0, "Feather",  ""},
	    {0, NULL, 0, NULL, NULL}
	};
	
	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_ui_text(prop, "Mode", "Growing/shrinking mode");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_range(prop, -5000, 5000);
	RNA_def_property_ui_range(prop, -100, 100, 0, -1);
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
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_inpaint(StructRNA *srna)
{
	PropertyRNA *prop;

#if 0
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);

	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of inpaint algorithm");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
#endif
	
	prop = RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_text(prop, "Distance", "Distance to inpaint (number of iterations)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_despeckle(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Threshold", "Threshold for detecting pixels to despeckle");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "threshold_neighbor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom4");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Neighbor", "Threshold for the number of neighbor pixels that must match");
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
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_CompositorNodeScale_update");

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
	
	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_sampler_type_items);
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

	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Node_scene_set", NULL, NULL);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Scene", "Scene from which to select the active camera (render scene if undefined)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeDefocus", "storage");

	prop = RNA_def_property(srna, "bokeh", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bktype");
	RNA_def_property_enum_items(prop, bokeh_items);
	RNA_def_property_ui_text(prop, "Bokeh Type", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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
		{0, "STRAIGHT_TO_PREMUL", 0, "Straight to Premul", ""},
		{1, "PREMUL_TO_STRAIGHT", 0, "Premul to Straight", ""},
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
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");
	
	prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gamma");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Gamma", "Correction for Midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");
	
	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gain");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Gain", "Correction for Highlights");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_lgg");
	
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 3);
	RNA_def_property_ui_text(prop, "Offset", "Correction for Shadows");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");
	
	prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "power");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_range(prop, 0.f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Power", "Correction for Midtones");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");
	
	prop = RNA_def_property(srna, "slope", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "slope");
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_array_default(prop, default_1);
	RNA_def_property_range(prop, 0.f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
	RNA_def_property_ui_text(prop, "Slope", "Correction for Highlights");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeColorBalance_update_cdl");
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

	prop = RNA_def_property(srna, "use_antialias_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "custom2", 0);
	RNA_def_property_ui_text(prop, "Anti-Alias Z", "Anti-alias the z-buffer to try to avoid artifacts, mostly useful for Blender renders");
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

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_sampler_type_items);
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

	static EnumPropertyItem aspect_type_items[] = {
		{0, "SCENE",   0, "Scene Size",   ""},
		{CMP_NODEFLAG_MASK_FIXED, "FIXED",   0, "Fixed",   "Use pixel size for the buffer"},
		{CMP_NODEFLAG_MASK_FIXED_SCENE, "FIXED_SCENE",   0, "Fixed/Scene", "Pixel size scaled by scene percentage"},
		{0, NULL, 0, NULL, NULL}
	};

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

	prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_NODEFLAG_MASK_MOTION_BLUR);
	RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled motion blur of the mask");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_range(prop, 1, CMP_NODE_MASK_MBLUR_SAMPLES_MAX);
	RNA_def_property_ui_text(prop, "Samples", "Number of motion blur samples");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Shutter", "Exposure for motion blur as a factor of FPS");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");


	prop = RNA_def_property(srna, "size_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, aspect_type_items);
	RNA_def_property_ui_text(prop, "Size Source", "Where to get the mask size from for aspect/size information");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");


	RNA_def_struct_sdna_from(srna, "NodeMask", "storage");

	prop = RNA_def_property(srna, "size_x", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "X", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "size_y", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Y", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void dev_cmd_transform(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, node_sampler_type_items);
	RNA_def_property_ui_text(prop, "Filter", "Method to use to filter transform");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

/* -- Compositor Nodes ------------------------------------------------------ */

static EnumPropertyItem node_masktype_items[] = {
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

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, DEG2RADF(-1800.0f), DEG2RADF(1800.0f));
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
	RNA_def_property_ui_text(prop, "X", "X position of the middle of the ellipse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "y");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -1.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Y", "Y position of the middle of the ellipse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_float_default(prop, 0.3f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Width", "Width of the ellipse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_float_default(prop, 0.2f);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Height", "Height of the ellipse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "rotation");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, DEG2RADF(-1800.0f), DEG2RADF(1800.0f));
	RNA_def_property_ui_text(prop, "Rotation", "Rotation angle of the ellipse");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_bokehblur(StructRNA *srna)
{
	PropertyRNA *prop;

	/* duplicated in def_cmp_blur */
	prop = RNA_def_property(srna, "use_variable_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", CMP_NODEFLAG_BLUR_VARIABLE_SIZE);
	RNA_def_property_ui_text(prop, "Variable Size",
	                         "Support variable blur per-pixel when using an image for size input");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

#if 0
	prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "custom3");
	RNA_def_property_range(prop, 0.0f, 128.0f);
	RNA_def_property_ui_text(prop, "fStop",
	                         "Amount of focal blur, 128=infinity=perfect focus, half the value doubles "
	                         "the blur radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
#endif

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

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_float_default(prop, 0.0f);
	RNA_def_property_range(prop, DEG2RADF(-720.0f), DEG2RADF(720.0f));
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
	RNA_def_property_boolean_default(prop, true);
	RNA_def_property_ui_text(prop, "Red", "Red channel active");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "green", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 2);
	RNA_def_property_boolean_default(prop, true);
	RNA_def_property_ui_text(prop, "Green", "Green channel active");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "blue", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "custom1", 4);
	RNA_def_property_boolean_default(prop, true);
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

	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "custom2", CMP_NODE_OUTPUT_IGNORE_ALPHA);
	RNA_def_property_ui_text(prop, "Use Alpha", "Colors are treated alpha premultiplied, or colors output straight (alpha gets set to 1)");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_composite(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "use_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "custom2", CMP_NODE_OUTPUT_IGNORE_ALPHA);
	RNA_def_property_ui_text(prop, "Use Alpha", "Colors are treated alpha premultiplied, or colors output straight (alpha gets set to 1)");
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
	RNA_def_property_ui_text(prop, "Despill Factor", "Factor of despilling screen color from image");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "despill_balance", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "despill_balance");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Despill Balance", "Balance between non-key colors used to detect amount of key color to be removed");
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
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

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
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
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

	static EnumPropertyItem position_items[] = {
		{CMP_TRACKPOS_ABSOLUTE, "ABSOLUTE", 0,
		 "Absolute",  "Output absolute position of a marker"},
		{CMP_TRACKPOS_RELATIVE_START, "RELATIVE_START", 0,
		 "Relative Start",  "Output position of a marker relative to first marker of a track"},
		{CMP_TRACKPOS_RELATIVE_FRAME, "RELATIVE_FRAME", 0,
		 "Relative Frame",  "Output position of a marker relative to marker at given frame number"},
		{CMP_TRACKPOS_ABSOLUTE_FRAME, "ABSOLUTE_FRAME", 0,
		 "Absolute Frame",  "Output absolute position of a marker at given frame number"},
		{0, NULL, 0, NULL, NULL}
	};

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "position", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "custom1");
	RNA_def_property_enum_items(prop, position_items);
	RNA_def_property_ui_text(prop, "Position", "Which marker position to use for output");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "frame_relative", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "custom2");
	RNA_def_property_ui_text(prop, "Frame", "Frame to be used for relative position");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodeTrackPosData", "storage");

	prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "tracking_object");
	RNA_def_property_ui_text(prop, "Tracking Object", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "track_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "track_name");
	RNA_def_property_ui_text(prop, "Track", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_translate(StructRNA *srna)
{
	static EnumPropertyItem translate_items[] = {
		{CMP_NODE_WRAP_NONE, "NONE",  0, "None",       "No wrapping on X and Y"},
		{CMP_NODE_WRAP_X,    "XAXIS", 0, "X Axis",     "Wrap all pixels on the X axis"},
		{CMP_NODE_WRAP_Y,    "YAXIS", 0, "Y Axis",     "Wrap all pixels on the Y axis"},
		{CMP_NODE_WRAP_XY,   "BOTH",  0, "Both Axes",  "Wrap all pixels on both axes"},
		{0, NULL, 0, NULL, NULL}
	};

	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeTranslateData", "storage");

	prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "relative", 1);
	RNA_def_property_ui_text(prop, "Relative", "Use relative (percent) values to define blur radius");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "wrap_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "wrap_axis");
	RNA_def_property_enum_items(prop, translate_items);
	RNA_def_property_ui_text(prop, "Wrapping", "Wrap image on a specific axis");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_planetrackdeform(StructRNA *srna)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "id");
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Clip", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	RNA_def_struct_sdna_from(srna, "NodePlaneTrackDeformData", "storage");

	prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "tracking_object");
	RNA_def_property_ui_text(prop, "Tracking Object", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "plane_track_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "plane_track_name");
	RNA_def_property_ui_text(prop, "Plane Track", "");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_sunbeams(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_sdna_from(srna, "NodeSunBeams", "storage");

	prop = RNA_def_property(srna, "source", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "source");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Source", "Source point of rays as a factor of the image width & height");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

	prop = RNA_def_property(srna, "ray_length", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "ray_length");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Ray Length", "Length of rays as a factor of the image size");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
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

static void rna_def_shader_node(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "ShaderNode", "NodeInternal");
	RNA_def_struct_ui_text(srna, "Shader Node", "Material shader node");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_register_funcs(srna, "rna_ShaderNode_register", "rna_Node_unregister", NULL);
}

static void rna_def_compositor_node(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "CompositorNode", "NodeInternal");
	RNA_def_struct_ui_text(srna, "Compositor Node", "");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_register_funcs(srna, "rna_CompositorNode_register", "rna_Node_unregister", NULL);
	
	/* compositor node need_exec flag */
	func = RNA_def_function(srna, "tag_need_exec", "rna_CompositorNode_tag_need_exec");
	RNA_def_function_ui_description(func, "Tag the node for compositor update");
}

static void rna_def_texture_node(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "TextureNode", "NodeInternal");
	RNA_def_struct_ui_text(srna, "Texture Node", "");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_register_funcs(srna, "rna_TextureNode_register", "rna_Node_unregister", NULL);
}

/* -------------------------------------------------------------------------- */

static void rna_def_node_socket(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;
	
	static float default_draw_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	
	srna = RNA_def_struct(brna, "NodeSocket", NULL);
	RNA_def_struct_ui_text(srna, "Node Socket", "Input or output socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	RNA_def_struct_refine_func(srna, "rna_NodeSocket_refine");
	RNA_def_struct_ui_icon(srna, ICON_PLUG);
	RNA_def_struct_path_func(srna, "rna_NodeSocket_path");
	RNA_def_struct_register_funcs(srna, "rna_NodeSocket_register", "rna_NodeSocket_unregister", NULL);
	RNA_def_struct_idprops_func(srna, "rna_NodeSocket_idprops");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Socket name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

	prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "identifier");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

	prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_output_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Output", "True if the socket is an output, otherwise input");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SOCK_HIDDEN);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_NodeSocket_hide_set");
	RNA_def_property_ui_text(prop, "Hide", "Hide the socket");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SOCK_UNAVAIL);
	RNA_def_property_ui_text(prop, "Enabled", "Enable the socket");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "link_limit", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "limit");
	RNA_def_property_int_funcs(prop, NULL, "rna_NodeSocket_link_limit_set", NULL);
	RNA_def_property_range(prop, 1, 0xFFF);
	RNA_def_property_ui_text(prop, "Link Limit", "Max number of links allowed for this socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "is_linked", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SOCK_IN_USE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Linked", "True if the socket is connected");

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SOCK_COLLAPSED);
	RNA_def_property_ui_text(prop, "Expanded", "Socket links are expanded in the user interface");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "hide_value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SOCK_HIDE_VALUE);
	RNA_def_property_ui_text(prop, "Hide Value", "Hide the socket input value");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "node", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, "rna_NodeSocket_node_get", NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Node", "Node owning this socket");

	/* NB: the type property is used by standard sockets.
	 * Ideally should be defined only for the registered subclass,
	 * but to use the existing DNA is added in the base type here.
	 * Future socket types can ignore or override this if needed.
	 */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, node_socket_type_items);
	RNA_def_property_enum_default(prop, SOCK_FLOAT);
	RNA_def_property_ui_text(prop, "Type", "Data type");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocket_update");

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", "");

	/* draw socket */
	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw socket");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "Node");
	RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_property(func, "text", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(parm, "Text", "Text label to draw alongside properties");
	// RNA_def_property_string_default(parm, "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "draw_color", NULL);
	RNA_def_function_ui_description(func, "Color of the socket icon");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "Node");
	RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_float_array(func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
	RNA_def_function_output(func, parm);
}

static void rna_def_node_socket_interface(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	PropertyRNA *parm;
	FunctionRNA *func;
	
	static float default_draw_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	
	srna = RNA_def_struct(brna, "NodeSocketInterface", NULL);
	RNA_def_struct_ui_text(srna, "Node Socket Template", "Parameters to define node sockets");
	/* XXX Using bNodeSocket DNA for templates is a compatibility hack.
	 * This allows to keep the inputs/outputs lists in bNodeTree working for earlier versions
	 * and at the same time use them for socket templates in groups.
	 */
	RNA_def_struct_sdna(srna, "bNodeSocket");
	RNA_def_struct_refine_func(srna, "rna_NodeSocketInterface_refine");
	RNA_def_struct_path_func(srna, "rna_NodeSocketInterface_path");
	RNA_def_struct_idprops_func(srna, "rna_NodeSocketInterface_idprops");
	RNA_def_struct_register_funcs(srna, "rna_NodeSocketInterface_register", "rna_NodeSocketInterface_unregister", NULL);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Socket name");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");

	prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "identifier");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Identifier", "Unique identifier for mapping sockets");

	prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_NodeSocket_is_output_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Output", "True if the socket is an output, otherwise input");

	/* registration */
	prop = RNA_def_property(srna, "bl_socket_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", "");

	func = RNA_def_function(srna, "draw", NULL);
	RNA_def_function_ui_description(func, "Draw template settings");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "draw_color", NULL);
	RNA_def_function_ui_description(func, "Color of the socket icon");
	RNA_def_function_flag(func, FUNC_REGISTER);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_float_array(func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
	RNA_def_function_output(func, parm);

	func = RNA_def_function(srna, "register_properties", NULL);
	RNA_def_function_ui_description(func, "Define RNA properties of a socket");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "data_rna_type", "Struct", "Data RNA Type", "RNA type for special socket properties");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "init_socket", NULL);
	RNA_def_function_ui_description(func, "Initialize a node socket instance");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the socket to initialize");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Socket to initialize");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_string(func, "data_path", NULL, 0, "Data Path", "Path to specialized socket data");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "from_socket", NULL);
	RNA_def_function_ui_description(func, "Setup template parameters from an existing socket");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "node", "Node", "Node", "Node of the original socket");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "Socket", "Original socket");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
}

static void rna_def_node_socket_float(BlenderRNA *brna, const char *idname, const char *interface_idname, PropertySubType subtype)
{
	StructRNA *srna;
	PropertyRNA *prop;
	float value_default;
	
	/* choose sensible common default based on subtype */
	switch (subtype) {
		case PROP_FACTOR:
			value_default = 1.0f;
			break;
		case PROP_PERCENTAGE:
			value_default = 100.0f;
			break;
		default:
			value_default = 0.0f;
			break;
	}
	
	srna = RNA_def_struct(brna, idname, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Float Node Socket", "Floating point number socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_float_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Float Node Socket Interface", "Floating point number socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueFloat", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_float_default(prop, value_default);
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_float_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_int(BlenderRNA *brna, const char *identifier, const char *interface_idname, PropertySubType subtype)
{
	StructRNA *srna;
	PropertyRNA *prop;
	int value_default;
	
	/* choose sensible common default based on subtype */
	switch (subtype) {
		case PROP_FACTOR:
			value_default = 1;
			break;
		case PROP_PERCENTAGE:
			value_default = 100;
			break;
		default:
			value_default = 0;
			break;
	}
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Integer Node Socket", "Integer number socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
	RNA_def_property_int_sdna(prop, NULL, "value");
	RNA_def_property_int_default(prop, value_default);
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_int_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Integer Node Socket Interface", "Integer number socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");

	RNA_def_struct_sdna_from(srna, "bNodeSocketValueInt", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_INT, subtype);
	RNA_def_property_int_sdna(prop, NULL, "value");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_int_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "min_value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "max_value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_bool(BlenderRNA *brna, const char *identifier, const char *interface_idname)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Boolean Node Socket", "Boolean value socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "value", 1);
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Boolean Node Socket Interface", "Boolean value socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueBoolean", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "value", 1);
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_vector(BlenderRNA *brna, const char *identifier, const char *interface_idname, PropertySubType subtype)
{
	StructRNA *srna;
	PropertyRNA *prop;
	const float *value_default;
	
	/* choose sensible common default based on subtype */
	switch (subtype) {
		case PROP_DIRECTION: {
			static const float default_direction[3] = {0.0f, 0.0f, 1.0f};
			value_default = default_direction;
			break;
		}
		default: {
			static const float default_vector[3] = {0.0f, 0.0f, 0.0f};
			value_default = default_vector;
			break;
		}
	}
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Vector Node Socket", "3D vector socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_float_array_default(prop, value_default);
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_vector_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Vector Node Socket Interface", "3D vector socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueVector", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, subtype);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_NodeSocketStandard_vector_range");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "min_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "min");
	RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	prop = RNA_def_property(srna, "max_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "max");
	RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_color(BlenderRNA *brna, const char *identifier, const char *interface_idname)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Color Node Socket", "RGBA color socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Color Node Socket Interface", "RGBA color socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueRGBA", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_string(BlenderRNA *brna, const char *identifier, const char *interface_idname)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "String Node Socket", "String socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketStandard_value_update");
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "String Node Socket Interface", "String socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocketValueString", "default_value");
	
	prop = RNA_def_property(srna, "default_value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Default Value", "Input value used for unconnected socket");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeSocketInterface_update");
	
	RNA_def_struct_sdna_from(srna, "bNodeSocket", NULL);
}

static void rna_def_node_socket_shader(BlenderRNA *brna, const char *identifier, const char *interface_idname)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Shader Node Socket", "Shader socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	/* socket interface */
	srna = RNA_def_struct(brna, interface_idname, "NodeSocketInterfaceStandard");
	RNA_def_struct_ui_text(srna, "Shader Node Socket Interface", "Shader socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_virtual(BlenderRNA *brna, const char *identifier)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, identifier, "NodeSocketStandard");
	RNA_def_struct_ui_text(srna, "Virtual Node Socket", "Virtual socket of a node");
	RNA_def_struct_sdna(srna, "bNodeSocket");
}

static void rna_def_node_socket_standard_types(BlenderRNA *brna)
{
	/* XXX Workaround: Registered functions are not exposed in python by bpy,
	 * it expects them to be registered from python and use the native implementation.
	 * However, the standard socket types below are not registering these functions from python,
	 * so in order to call them in py scripts we need to overload and replace them with plain C callbacks.
	 * These types provide a usable basis for socket types defined in C.
	 */
	
	StructRNA *srna;
	PropertyRNA *parm, *prop;
	FunctionRNA *func;
	
	static float default_draw_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	
	srna = RNA_def_struct(brna, "NodeSocketStandard", "NodeSocket");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	/* draw socket */
	func = RNA_def_function(srna, "draw", "rna_NodeSocketStandard_draw");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Draw socket");
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "Node");
	RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_property(func, "text", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(parm, "Text", "Text label to draw alongside properties");
	// RNA_def_property_string_default(parm, "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "draw_color", "rna_NodeSocketStandard_draw_color");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Color of the socket icon");
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "node", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "Node");
	RNA_def_property_ui_text(parm, "Node", "Node the socket belongs to");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	parm = RNA_def_float_array(func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
	RNA_def_function_output(func, parm);
	
	
	srna = RNA_def_struct(brna, "NodeSocketInterfaceStandard", "NodeSocketInterface");
	RNA_def_struct_sdna(srna, "bNodeSocket");
	
	/* for easier type comparison in python */
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeinfo->type");
	RNA_def_property_enum_items(prop, node_socket_type_items);
	RNA_def_property_enum_default(prop, SOCK_FLOAT);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Data type");
	
	func = RNA_def_function(srna, "draw", "rna_NodeSocketInterfaceStandard_draw");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Draw template settings");
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "draw_color", "rna_NodeSocketInterfaceStandard_draw_color");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Color of the socket icon");
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_float_array(func, "color", 4, default_draw_color, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
	RNA_def_function_output(func, parm);


	/* XXX These types should eventually be registered at runtime.
	 * Then use the nodeStaticSocketType and nodeStaticSocketInterfaceType functions
	 * to get the idname strings from int type and subtype (see node_socket.c, register_standard_node_socket_types).
	 */
	
	rna_def_node_socket_float(brna, "NodeSocketFloat", "NodeSocketInterfaceFloat", PROP_NONE);
	rna_def_node_socket_float(brna, "NodeSocketFloatUnsigned", "NodeSocketInterfaceFloatUnsigned", PROP_UNSIGNED);
	rna_def_node_socket_float(brna, "NodeSocketFloatPercentage", "NodeSocketInterfaceFloatPercentage", PROP_PERCENTAGE);
	rna_def_node_socket_float(brna, "NodeSocketFloatFactor", "NodeSocketInterfaceFloatFactor", PROP_FACTOR);
	rna_def_node_socket_float(brna, "NodeSocketFloatAngle", "NodeSocketInterfaceFloatAngle", PROP_ANGLE);
	rna_def_node_socket_float(brna, "NodeSocketFloatTime", "NodeSocketInterfaceFloatTime", PROP_TIME);

	rna_def_node_socket_int(brna, "NodeSocketInt", "NodeSocketInterfaceInt", PROP_NONE);
	rna_def_node_socket_int(brna, "NodeSocketIntUnsigned", "NodeSocketInterfaceIntUnsigned", PROP_UNSIGNED);
	rna_def_node_socket_int(brna, "NodeSocketIntPercentage", "NodeSocketInterfaceIntPercentage", PROP_PERCENTAGE);
	rna_def_node_socket_int(brna, "NodeSocketIntFactor", "NodeSocketInterfaceIntFactor", PROP_FACTOR);

	rna_def_node_socket_bool(brna, "NodeSocketBool", "NodeSocketInterfaceBool");

	rna_def_node_socket_vector(brna, "NodeSocketVector", "NodeSocketInterfaceVector", PROP_NONE);
	rna_def_node_socket_vector(brna, "NodeSocketVectorTranslation", "NodeSocketInterfaceVectorTranslation", PROP_TRANSLATION);
	rna_def_node_socket_vector(brna, "NodeSocketVectorDirection", "NodeSocketInterfaceVectorDirection", PROP_DIRECTION);
	rna_def_node_socket_vector(brna, "NodeSocketVectorVelocity", "NodeSocketInterfaceVectorVelocity", PROP_VELOCITY);
	rna_def_node_socket_vector(brna, "NodeSocketVectorAcceleration", "NodeSocketInterfaceVectorAcceleration", PROP_ACCELERATION);
	rna_def_node_socket_vector(brna, "NodeSocketVectorEuler", "NodeSocketInterfaceVectorEuler", PROP_EULER);
	rna_def_node_socket_vector(brna, "NodeSocketVectorXYZ", "NodeSocketInterfaceVectorXYZ", PROP_XYZ);

	rna_def_node_socket_color(brna, "NodeSocketColor", "NodeSocketInterfaceColor");

	rna_def_node_socket_string(brna, "NodeSocketString", "NodeSocketInterfaceString");

	rna_def_node_socket_shader(brna, "NodeSocketShader", "NodeSocketInterfaceShader");

	rna_def_node_socket_virtual(brna, "NodeSocketVirtual");
}

static void rna_def_internal_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop, *parm;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "NodeInternalSocketTemplate", NULL);
	RNA_def_struct_ui_text(srna, "Socket Template", "Type and default value of a node socket");
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_NodeInternalSocketTemplate_name_get", "rna_NodeInternalSocketTemplate_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Name of the socket");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_NodeInternalSocketTemplate_identifier_get", "rna_NodeInternalSocketTemplate_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Identifier of the socket");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_NodeInternalSocketTemplate_type_get", NULL, NULL);
	RNA_def_property_enum_items(prop, node_socket_type_items);
	RNA_def_property_ui_text(prop, "Type", "Data type of the socket");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	/* XXX Workaround: Registered functions are not exposed in python by bpy,
	 * it expects them to be registered from python and use the native implementation.
	 * However, the standard node types are not registering these functions from python,
	 * so in order to call them in py scripts we need to overload and replace them with plain C callbacks.
	 * This type provides a usable basis for node types defined in C.
	 */
	
	srna = RNA_def_struct(brna, "NodeInternal", "Node");
	RNA_def_struct_sdna(srna, "bNode");
	
	/* poll */
	func = RNA_def_function(srna, "poll", "rna_NodeInternal_poll");
	RNA_def_function_ui_description(func, "If non-null output is returned, the node type can be added to the tree");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
	parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "poll_instance", "rna_NodeInternal_poll_instance");
	RNA_def_function_ui_description(func, "If non-null output is returned, the node can be added to the tree");
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
	parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	/* update */
	func = RNA_def_function(srna, "update", "rna_NodeInternal_update");
	RNA_def_function_ui_description(func, "Update on editor changes");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_ALLOW_WRITE);
	
	/* draw buttons */
	func = RNA_def_function(srna, "draw_buttons", "rna_NodeInternal_draw_buttons");
	RNA_def_function_ui_description(func, "Draw node buttons");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* draw buttons extended */
	func = RNA_def_function(srna, "draw_buttons_ext", "rna_NodeInternal_draw_buttons_ext");
	RNA_def_function_ui_description(func, "Draw node buttons in the sidebar");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
}

static void rna_def_node_sockets_api(BlenderRNA *brna, PropertyRNA *cprop, int in_out)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;
	const char *structtype = (in_out == SOCK_IN ? "NodeInputs" : "NodeOutputs");
	const char *uiname =  (in_out == SOCK_IN ? "Node Inputs" : "Node Outputs");
	const char *newfunc = (in_out == SOCK_IN ? "rna_Node_inputs_new" : "rna_Node_outputs_new");
	const char *clearfunc = (in_out == SOCK_IN ? "rna_Node_inputs_clear" : "rna_Node_outputs_clear");
	const char *movefunc = (in_out == SOCK_IN ? "rna_Node_inputs_move" : "rna_Node_outputs_move");

	RNA_def_property_srna(cprop, structtype);
	srna = RNA_def_struct(brna, structtype, NULL);
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_ui_text(srna, uiname, "Collection of Node Sockets");

	func = RNA_def_function(srna, "new", newfunc);
	RNA_def_function_ui_description(func, "Add a socket to this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "type", NULL, MAX_NAME, "Type", "Data type");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_string(func, "identifier", NULL, MAX_NAME, "Identifier", "Unique socket identifier");
	/* return value */
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "New socket");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Node_socket_remove");
	RNA_def_function_ui_description(func, "Remove a socket from this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "The socket to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", clearfunc);
	RNA_def_function_ui_description(func, "Remove all sockets from this node");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);

	func = RNA_def_function(srna, "move", movefunc);
	RNA_def_function_ui_description(func, "Move a socket to another position");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_int(func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the socket to move", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the socket", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	static EnumPropertyItem dummy_static_type_items[] = {
		{NODE_CUSTOM, "CUSTOM", 0, "Custom", "Custom Node"},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "Node", NULL);
	RNA_def_struct_ui_text(srna, "Node", "Node in a node tree");
	RNA_def_struct_sdna(srna, "bNode");
	RNA_def_struct_ui_icon(srna, ICON_NODE);
	RNA_def_struct_refine_func(srna, "rna_Node_refine");
	RNA_def_struct_path_func(srna, "rna_Node_path");
	RNA_def_struct_register_funcs(srna, "rna_Node_register", "rna_Node_unregister", NULL);
	RNA_def_struct_idprops_func(srna, "rna_Node_idprops");
	
	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, dummy_static_type_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_node_static_type_itemf");
	RNA_def_property_enum_default(prop, NODE_CUSTOM);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "Node type (deprecated, use bl_static_type or bl_idname for the actual identifier string)");
	
	prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "locx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, -100000.0f, 100000.0f);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, NC_NODE, "rna_Node_update");
	
	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_Node_width_range");
	RNA_def_property_ui_text(prop, "Width", "Width of the node");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);
	
	prop = RNA_def_property(srna, "width_hidden", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "miniwidth");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_Node_width_range");
	RNA_def_property_ui_text(prop, "Width Hidden", "Width of the node in hidden state");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);
	
	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_Node_height_range");
	RNA_def_property_ui_text(prop, "Height", "Height of the node");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);
	
	prop = RNA_def_property(srna, "dimensions", PROP_FLOAT, PROP_XYZ_LENGTH);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_Node_dimensions_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Dimensions", "Absolute bounding box dimensions of the node");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique node identifier");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Node_name_set");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
	
	prop = RNA_def_property(srna, "label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "label");
	RNA_def_property_ui_text(prop, "Label", "Optional custom node label");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);
	
	prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "inputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Inputs", "");
	rna_def_node_sockets_api(brna, prop, SOCK_IN);
	
	prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "outputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_ui_text(prop, "Outputs", "");
	rna_def_node_sockets_api(brna, prop, SOCK_OUT);

	prop = RNA_def_property(srna, "internal_links", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "internal_links", NULL);
	RNA_def_property_struct_type(prop, "NodeLink");
	RNA_def_property_ui_text(prop, "Internal Links", "Internal input-to-output connections for muting");

	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "parent");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Node_parent_set", NULL, "rna_Node_parent_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Node");
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
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Node_select_set");
	RNA_def_property_ui_text(prop, "Select", "Node selection state");
	RNA_def_property_update(prop, NC_NODE | NA_SELECTED, NULL);

	prop = RNA_def_property(srna, "show_options", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_OPTIONS);
	RNA_def_property_ui_text(prop, "Show Options", "");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "show_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_PREVIEW);
	RNA_def_property_ui_text(prop, "Show Preview", "");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_HIDDEN);
	RNA_def_property_ui_text(prop, "Hide", "");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, NULL);

	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_MUTED);
	RNA_def_property_ui_text(prop, "Mute", "");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	prop = RNA_def_property(srna, "show_texture", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_ACTIVE_TEXTURE);
	RNA_def_property_ui_text(prop, "Show Texture", "Draw node in viewport textured draw mode");
	RNA_def_property_update(prop, 0, "rna_Node_update");

	/* generic property update function */
	func = RNA_def_function(srna, "socket_value_update", "rna_Node_socket_value_update");
	RNA_def_function_ui_description(func, "Update after property changes");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "is_registered_node_type", "rna_Node_is_registered_node_type");
	RNA_def_function_ui_description(func, "True if a registered node type");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	parm = RNA_def_boolean(func, "result", false, "Result", "");
	RNA_def_function_return(func, parm);

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", "");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->ui_name");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label", "The node label");

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->ui_description");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	
	prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeinfo->ui_icon");
	RNA_def_property_enum_items(prop, node_icon_items);
	RNA_def_property_enum_default(prop, ICON_NODE);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_ui_text(prop, "Icon", "The node icon");

	prop = RNA_def_property(srna, "bl_static_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeinfo->type");
	RNA_def_property_enum_items(prop, dummy_static_type_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_node_static_type_itemf");
	RNA_def_property_enum_default(prop, NODE_CUSTOM);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	RNA_def_property_ui_text(prop, "Static Type", "Node type (deprecated, use with care)");

	/* type-based size properties */
	prop = RNA_def_property(srna, "bl_width_default", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->width");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_width_min", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->minwidth");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_width_max", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->maxwidth");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_height_default", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->height");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_height_min", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->minheight");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_height_max", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "typeinfo->minheight");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "If non-null output is returned, the node type can be added to the tree");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
	parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "poll_instance", NULL);
	RNA_def_function_ui_description(func, "If non-null output is returned, the node can be added to the tree");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));
	parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	/* update */
	func = RNA_def_function(srna, "update", NULL);
	RNA_def_function_ui_description(func, "Update on editor changes");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	
	/* init */
	func = RNA_def_function(srna, "init", NULL);
	RNA_def_function_ui_description(func, "Initialize a new instance of this node");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* copy */
	func = RNA_def_function(srna, "copy", NULL);
	RNA_def_function_ui_description(func, "Initialize a new instance of this node from an existing node");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	parm = RNA_def_pointer(func, "node", "Node", "Node", "Existing node to copy");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* free */
	func = RNA_def_function(srna, "free", NULL);
	RNA_def_function_ui_description(func, "Clean up node on removal");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

	/* draw buttons */
	func = RNA_def_function(srna, "draw_buttons", NULL);
	RNA_def_function_ui_description(func, "Draw node buttons");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* draw buttons extended */
	func = RNA_def_function(srna, "draw_buttons_ext", NULL);
	RNA_def_function_ui_description(func, "Draw node buttons in the sidebar");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_property(func, "layout", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "UILayout");
	RNA_def_property_ui_text(parm, "Layout", "Layout in the UI");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* dynamic label */
	func = RNA_def_function(srna, "draw_label", NULL);
	RNA_def_function_ui_description(func, "Returns a dynamic label string");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_string(func, "label", NULL, MAX_NAME, "Label", "");
	RNA_def_property_flag(parm, PROP_THICK_WRAP); /* needed for string return value */
	RNA_def_function_output(func, parm);
}

static void rna_def_node_link(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NodeLink", NULL);
	RNA_def_struct_ui_text(srna, "NodeLink", "Link between nodes in a node tree");
	RNA_def_struct_sdna(srna, "bNodeLink");
	RNA_def_struct_ui_icon(srna, ICON_NODE);

	prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NODE_LINK_VALID);
	RNA_def_struct_ui_text(srna, "Valid", "Link is valid");
	RNA_def_property_update(prop, NC_NODE | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "from_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fromnode");
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "From node", "");

	prop = RNA_def_property(srna, "to_node", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tonode");
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "To node", "");

	prop = RNA_def_property(srna, "from_socket", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "fromsock");
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "From socket", "");

	prop = RNA_def_property(srna, "to_socket", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "tosock");
	RNA_def_property_struct_type(prop, "NodeSocket");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "To socket", "");

	prop = RNA_def_property(srna, "is_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_NodeLink_is_hidden_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Hidden", "Link is hidden due to invisible sockets");
}

static void rna_def_nodetree_nodes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *parm, *prop;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "Nodes");
	srna = RNA_def_struct(brna, "Nodes", NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, "Nodes", "Collection of Nodes");

	func = RNA_def_function(srna, "new", "rna_NodeTree_node_new");
	RNA_def_function_ui_description(func, "Add a node to this node tree");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	/* XXX warning note should eventually be removed,
	 * added this here to avoid frequent confusion with API changes from "type" to "bl_idname"
	 */
	parm = RNA_def_string(func, "type", NULL, MAX_NAME, "Type", "Type of node to add (Warning: should be same as node.bl_idname, not node.type!)");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_pointer(func, "node", "Node", "", "New node");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_node_remove");
	RNA_def_function_ui_description(func, "Remove a node from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "node", "Node", "", "The node to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_node_clear");
	RNA_def_function_ui_description(func, "Remove all nodes from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_pointer_funcs(prop, "rna_NodeTree_active_node_get", "rna_NodeTree_active_node_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Node", "Active node in this tree");
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);
}

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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "output", "NodeSocket", "", "The output socket");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	RNA_def_boolean(func, "verify_limits", true, "Verify Limits", "Remove existing links if connection limit is exceeded");
	/* return */
	parm = RNA_def_pointer(func, "link", "NodeLink", "", "New node link");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_link_remove");
	RNA_def_function_ui_description(func, "remove a node link from the node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "link", "NodeLink", "", "The node link to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	func = RNA_def_function(srna, "clear", "rna_NodeTree_link_clear");
	RNA_def_function_ui_description(func, "remove all node links from the node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
}

static void rna_def_node_tree_sockets_api(BlenderRNA *brna, PropertyRNA *cprop, int in_out)
{
	StructRNA *srna;
	PropertyRNA *parm;
	FunctionRNA *func;
	const char *structtype = (in_out == SOCK_IN ? "NodeTreeInputs" : "NodeTreeOutputs");
	const char *uiname =  (in_out == SOCK_IN ? "Node Tree Inputs" : "Node Tree Outputs");
	const char *newfunc = (in_out == SOCK_IN ? "rna_NodeTree_inputs_new" : "rna_NodeTree_outputs_new");
	const char *clearfunc = (in_out == SOCK_IN ? "rna_NodeTree_inputs_clear" : "rna_NodeTree_outputs_clear");
	const char *movefunc = (in_out == SOCK_IN ? "rna_NodeTree_inputs_move" : "rna_NodeTree_outputs_move");

	RNA_def_property_srna(cprop, structtype);
	srna = RNA_def_struct(brna, structtype, NULL);
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_text(srna, uiname, "Collection of Node Tree Sockets");

	func = RNA_def_function(srna, "new", newfunc);
	RNA_def_function_ui_description(func, "Add a socket to this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_string(func, "type", NULL, MAX_NAME, "Type", "Data type");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "name", NULL, MAX_NAME, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_pointer(func, "socket", "NodeSocketInterface", "", "New socket");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_NodeTree_socket_remove");
	RNA_def_function_ui_description(func, "Remove a socket from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "socket", "NodeSocketInterface", "", "The socket to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "clear", clearfunc);
	RNA_def_function_ui_description(func, "Remove all sockets from this node tree");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);

	func = RNA_def_function(srna, "move", movefunc);
	RNA_def_function_ui_description(func, "Move a socket to another position");
	parm = RNA_def_int(func, "from_index", -1, 0, INT_MAX, "From Index", "Index of the socket to move", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "to_index", -1, 0, INT_MAX, "To Index", "Target index for the socket", 0, 10000);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void rna_def_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem static_type_items[] = {
		{NTREE_SHADER,      "SHADER",       ICON_MATERIAL,      "Shader",       "Shader nodes"},
		{NTREE_TEXTURE,     "TEXTURE",      ICON_TEXTURE,       "Texture",      "Texture nodes"},
		{NTREE_COMPOSIT,    "COMPOSITING",  ICON_RENDERLAYERS,  "Compositing",  "Compositing nodes"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "NodeTree", "ID");
	RNA_def_struct_ui_text(srna, "Node Tree",
	                       "Node tree consisting of linked nodes used for shading, textures and compositing");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_NODETREE);
	RNA_def_struct_refine_func(srna, "rna_NodeTree_refine");
	RNA_def_struct_register_funcs(srna, "rna_NodeTree_register", "rna_NodeTree_unregister", NULL);

	prop = RNA_def_property(srna, "view_center", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_sdna(prop, NULL, "view_center");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	/* AnimData */
	rna_def_animdata_common(srna);

	/* Nodes Collection */
	prop = RNA_def_property(srna, "nodes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "nodes", NULL);
	RNA_def_property_struct_type(prop, "Node");
	RNA_def_property_ui_text(prop, "Nodes", "");
	rna_def_nodetree_nodes_api(brna, prop);

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
	RNA_def_property_enum_items(prop, static_type_items);
	RNA_def_property_ui_text(prop, "Type", "Node Tree type (deprecated, bl_idname is the actual node tree type identifier)");

	prop = RNA_def_property(srna, "inputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "inputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocketInterface");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Inputs", "Node tree inputs");
	rna_def_node_tree_sockets_api(brna, prop, SOCK_IN);

	prop = RNA_def_property(srna, "active_input", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_NodeTree_active_input_get", "rna_NodeTree_active_input_set", NULL);
	RNA_def_property_ui_text(prop, "Active Input", "Index of the active input");
	RNA_def_property_update(prop, NC_NODE, NULL);

	prop = RNA_def_property(srna, "outputs", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "outputs", NULL);
	RNA_def_property_struct_type(prop, "NodeSocketInterface");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Outputs", "Node tree outputs");
	rna_def_node_tree_sockets_api(brna, prop, SOCK_OUT);

	prop = RNA_def_property(srna, "active_output", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_NodeTree_active_output_get", "rna_NodeTree_active_output_set", NULL);
	RNA_def_property_ui_text(prop, "Active Output", "Index of the active output");
	RNA_def_property_update(prop, NC_NODE, NULL);

	/* exposed as a function for runtime interface type properties */
	func = RNA_def_function(srna, "interface_update", "rna_NodeTree_interface_update");
	RNA_def_function_ui_description(func, "Updated node group interface");
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	/* registration */
	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "ID Name", "");

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->ui_name");
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Label", "The node tree label");

	prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_TRANSLATION);
	RNA_def_property_string_sdna(prop, NULL, "typeinfo->ui_description");
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
	
	prop = RNA_def_property(srna, "bl_icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeinfo->ui_icon");
	RNA_def_property_enum_items(prop, node_icon_items);
	RNA_def_property_enum_default(prop, ICON_NODETREE);
	RNA_def_property_flag(prop, PROP_REGISTER);
	RNA_def_property_ui_text(prop, "Icon", "The node tree icon");

	/* poll */
	func = RNA_def_function(srna, "poll", NULL);
	RNA_def_function_ui_description(func, "Check visibility in the editor");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	RNA_def_function_return(func, RNA_def_boolean(func, "visible", false, "", ""));

	/* update */
	func = RNA_def_function(srna, "update", NULL);
	RNA_def_function_ui_description(func, "Update on editor changes");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);

	/* get a node tree from context */
	func = RNA_def_function(srna, "get_from_context", NULL);
	RNA_def_function_ui_description(func, "Get a node tree from the context");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_REGISTER_OPTIONAL);
	parm = RNA_def_pointer(func, "context", "Context", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "result_1", "NodeTree", "Node Tree", "Active node tree from context");
	RNA_def_function_output(func, parm);
	parm = RNA_def_pointer(func, "result_2", "ID", "Owner ID", "ID data block that owns the node tree");
	RNA_def_function_output(func, parm);
	parm = RNA_def_pointer(func, "result_3", "ID", "From ID", "Original ID data block selected from the context");
	RNA_def_function_output(func, parm);
}

static void rna_def_composite_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CompositorNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Compositor Node Tree", "Node tree consisting of linked nodes used for compositing");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_RENDERLAYERS);

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

	prop = RNA_def_property(srna, "use_groupnode_buffer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NTREE_COM_GROUPNODE_BUFFER);
	RNA_def_property_ui_text(prop, "Buffer Groups", "Enable buffering of group nodes");

	prop = RNA_def_property(srna, "use_two_pass", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NTREE_TWO_PASS);
	RNA_def_property_ui_text(prop, "Two Pass", "Use two pass execution during editing: first calculate fast nodes, "
	                                           "second pass calculate all nodes");

	prop = RNA_def_property(srna, "use_viewer_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", NTREE_VIEWER_BORDER);
	RNA_def_property_ui_text(prop, "Viewer Border", "Use boundaries for viewer nodes and composite backdrop");
	RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, "rna_NodeTree_update");
}

static void rna_def_shader_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "ShaderNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Shader Node Tree",
	                       "Node tree consisting of linked nodes used for materials (and other shading datablocks)");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_MATERIAL);
}

static void rna_def_texture_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "TextureNodeTree", "NodeTree");
	RNA_def_struct_ui_text(srna, "Texture Node Tree", "Node tree consisting of linked nodes used for textures");
	RNA_def_struct_sdna(srna, "bNodeTree");
	RNA_def_struct_ui_icon(srna, ICON_TEXTURE);
}

static StructRNA *define_specific_node(BlenderRNA *brna, const char *struct_name, const char *base_name,
                                       const char *ui_name, const char *ui_desc, void (*def_func)(StructRNA *))
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;
	
	/* XXX hack, want to avoid "NodeInternal" prefix, so use "Node" in NOD_static_types.h and replace here */
	if (STREQ(base_name, "Node"))
		base_name = "NodeInternal";
	
	srna = RNA_def_struct(brna, struct_name, base_name);
	RNA_def_struct_ui_text(srna, ui_name, ui_desc);
	RNA_def_struct_sdna(srna, "bNode");

	func = RNA_def_function(srna, "is_registered_node_type", "rna_Node_is_registered_node_type");
	RNA_def_function_ui_description(func, "True if a registered node type");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	parm = RNA_def_boolean(func, "result", false, "Result", "");
	RNA_def_function_return(func, parm);

	/* Exposes the socket template type lists in RNA for use in scripts
	 * Only used in the C nodes and not exposed in the base class to keep the namespace clean for pynodes.
	 */
	func = RNA_def_function(srna, "input_template", "rna_NodeInternal_input_template");
	RNA_def_function_ui_description(func, "Input socket template");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	parm = RNA_def_property(func, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(parm, "Index", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_property(func, "result", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "NodeInternalSocketTemplate");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "output_template", "rna_NodeInternal_output_template");
	RNA_def_function_ui_description(func, "Output socket template");
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_SELF_TYPE);
	parm = RNA_def_property(func, "index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(parm, "Index", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_property(func, "result", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(parm, "NodeInternalSocketTemplate");
	RNA_def_property_flag(parm, PROP_RNAPTR);
	RNA_def_function_return(func, parm);

	if (def_func)
		def_func(srna);

	return srna;
}

static void rna_def_node_instance_hash(BlenderRNA *brna)
{
	StructRNA *srna;

	srna = RNA_def_struct(brna, "NodeInstanceHash", NULL);
	RNA_def_struct_ui_text(srna, "Node Instance Hash", "Hash table containing node instance data");

	/* XXX This type is a stub for now, only used to store instance hash in the context.
	 * Eventually could use a StructRNA pointer to define a specific data type
	 * and expose lookup functions.
	 */
}

void RNA_def_nodetree(BlenderRNA *brna)
{
	StructRNA *srna;
	
	rna_def_node_socket(brna);
	rna_def_node_socket_interface(brna);
	
	rna_def_node(brna);
	rna_def_node_link(brna);
	
	rna_def_internal_node(brna);
	rna_def_shader_node(brna);
	rna_def_compositor_node(brna);
	rna_def_texture_node(brna);
	
	rna_def_nodetree(brna);
	
	rna_def_node_socket_standard_types(brna);
	
	rna_def_composite_nodetree(brna);
	rna_def_shader_nodetree(brna);
	rna_def_texture_nodetree(brna);
	
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
	{ \
		srna = define_specific_node(brna, #Category #StructName, #Category, UIName, UIDesc, DefFunc); \
		if (ID == CMP_NODE_OUTPUT_FILE) { \
			/* needs brna argument, can't use NOD_static_types.h */ \
			def_cmp_output_file(brna, srna); \
		} \
	}
	
	/* hack, don't want to add include path to RNA just for this, since in the future RNA types
	 * for nodes should be defined locally at runtime anyway ...
	 */
#include "../../nodes/NOD_static_types.h"
	
	/* Node group types need to be defined for shader, compositor, texture nodes individually.
	 * Cannot use the static types header for this, since they share the same int id.
	 */
	define_specific_node(brna, "ShaderNodeGroup", "ShaderNode", "Group", "", def_group);
	define_specific_node(brna, "CompositorNodeGroup", "CompositorNode", "Group", "", def_group);
	define_specific_node(brna, "TextureNodeGroup", "TextureNode", "Group", "", def_group);
	def_custom_group(brna);
	
	/* special socket types */
	rna_def_cmp_output_file_slot_file(brna);
	rna_def_cmp_output_file_slot_layer(brna);
	
	rna_def_node_instance_hash(brna);
}

/* clean up macro definition */
#undef NODE_DEFINE_SUBTYPES

#endif
