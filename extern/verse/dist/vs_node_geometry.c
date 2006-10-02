/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "v_util.h"
#include "vs_server.h"

#define VS_G_LAYER_CHUNK 32

typedef struct {
	VNGLayerType	type;
	char		name[16];
	void		*layer;
	VSSubscriptionList *subscribers;
	VSSubscriptionList *subscribersd;
	union{
		uint32 integer;
		real64 real;
	} def;
} VSNGLayer;

typedef struct {
	char	weight[16];
	char	reference[16];
	uint16	parent;
	real64	pos_x;
	real64	pos_y;
	real64	pos_z;
	char	position_label[16];
	char	rotation_label[16];
	char	scale_label[16];
} VSNGBone;

typedef struct {
	VSNodeHead	head;
	VSNGLayer	*layer;
	uint16		layer_count;
	uint32		vertex_size;
	uint32		poly_size;
	uint32		vertex_hole;
	uint32		polygon_hole;	
	uint32		crease_vertex;
	char		crease_vertex_layer[16];
	uint32		crease_edge;
	char		crease_edge_layer[16];
	VSNGBone	*bones;
	uint32		bone_count;
} VSNodeGeometry;

VSNodeGeometry * vs_g_create_node(unsigned int owner)
{
	VSNodeGeometry *node;
	unsigned int i;
	char name[48];
	node = malloc(sizeof *node);
	vs_add_new_node(&node->head, V_NT_GEOMETRY);
	sprintf(name, "Geometry_Node_%u", node->head.id);
	create_node_head(&node->head, name, owner);

	node->bones = malloc((sizeof *node->bones) * 16);
	node->bone_count = 16;
	for(i = 0; i < node->bone_count; i++)
		node->bones[i].weight[0] = '\0';

	node->layer = malloc((sizeof *node->layer) * 16);
	node->layer_count = 16;
	node->vertex_size = VS_G_LAYER_CHUNK;
	node->poly_size = VS_G_LAYER_CHUNK;

	strcpy(node->layer[0].name, "vertex");
	node->layer[0].type = VN_G_LAYER_VERTEX_XYZ;
	node->layer[0].layer = malloc(sizeof(real64) * VS_G_LAYER_CHUNK * 3);
	for(i = 0; i < VS_G_LAYER_CHUNK * 3; i++)
		((real64 *)node->layer[0].layer)[i] = V_REAL64_MAX;
	node->layer[0].subscribers  = NULL;/*vs_create_subscription_list();*/
	node->layer[0].subscribersd = NULL;/*vs_create_subscription_list();*/
	node->layer[0].def.real = 0;

	strcpy(node->layer[1].name, "polygon");
	node->layer[1].type = VN_G_LAYER_POLYGON_CORNER_UINT32;
	node->layer[1].layer = malloc(sizeof(uint32) * VS_G_LAYER_CHUNK * 4);
	for(i = 0; i < VS_G_LAYER_CHUNK * 4; i++)
		((uint32 *)node->layer[1].layer)[i] = -1;
	node->layer[1].subscribers = NULL;/*vs_create_subscription_list();*/
	node->layer[1].subscribersd = NULL;
	node->layer[1].def.integer = 0;
	node->layer[1].def.real = 0.0;

	for(i = 2; i < 16; i++)
	{
		node->layer[i].type = -1;
		node->layer[i].name[0] = 0;
		node->layer[i].layer = NULL;
		node->layer[i].subscribers = NULL;
		node->layer[i].subscribersd = NULL;
		node->layer[i].def.real = 0;
	}
	node->crease_vertex = 0;
	node->crease_vertex_layer[0] = 0;
	node->crease_edge = 0;
	node->crease_edge_layer[0] = 0;
	node->vertex_hole = 0;
	node->polygon_hole = 0;
	return node;
}

void vs_g_destroy_node(VSNodeGeometry *node)
{
	destroy_node_head(&node->head);
	free(node);
}

void vs_g_subscribe(VSNodeGeometry *node)
{
	unsigned int i;
	for(i = 0; i < node->layer_count; i++)
	{
		if(node->layer[i].layer != NULL)
		{
			verse_send_g_layer_create(node->head.id, (uint16)i, node->layer[i].name, node->layer[i].type,
						  node->layer[i].def.integer, node->layer[i].def.real);
		}
	}
	verse_send_g_crease_set_vertex(node->head.id, node->crease_vertex_layer, node->crease_vertex);
	verse_send_g_crease_set_edge(node->head.id, node->crease_edge_layer, node->crease_edge);
	for(i = 0; i < node->bone_count; i++)
	{
		if(node->bones[i].weight[0] != 0)
			verse_send_g_bone_create(node->head.id, (uint16)i, node->bones[i].weight, node->bones[i].reference, node->bones[i].parent,
						 node->bones[i].pos_x, node->bones[i].pos_y, node->bones[i].pos_z, node->bones[i].position_label,
						 node->bones[i].rotation_label, node->bones[i].scale_label);
	}
}


void vs_g_unsubscribe(VSNodeGeometry *node)
{
	unsigned int i;
	for(i = 0; i < node->layer_count; i++) {
		if(node->layer[i].layer != NULL) {
			if(node->layer[i].subscribers)
				vs_remove_subscriptor(node->layer[i].subscribers);
			if(node->layer[i].subscribersd)
				vs_remove_subscriptor(node->layer[i].subscribersd);
		}
	}
}

static void callback_send_g_layer_create(void *user, VNodeID node_id, VLayerID layer_id, char *name, uint8 type, uint32 def_uint, real64 def_real)
{
	VSNodeGeometry *node;
	unsigned int i, j, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);

	if(node == NULL)
		return;
	if((type < VN_G_LAYER_POLYGON_CORNER_UINT32 && type > VN_G_LAYER_VERTEX_REAL) ||
	   (type > VN_G_LAYER_POLYGON_FACE_REAL))
		return;

	if(layer_id < 2)
		layer_id = -1;
	for(i = 0; i < node->layer_count; i++)
	{
		if(node->layer[i].layer != NULL && i != layer_id)
		{
			for(j = 0; name[j] == node->layer[i].name[j] && name[j] != 0; j++);
			if(name[j] == node->layer[i].name[j])
				return;
		}
	}
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL)
	{
		for(layer_id = 0; layer_id < node->layer_count && node->layer[layer_id].layer != NULL; layer_id++);
		if(layer_id == node->layer_count)
		{
			layer_id = node->layer_count;
			node->layer_count += 16;
			node->layer = realloc(node->layer, (sizeof *node->layer) * node->layer_count);
			for(i = layer_id; i < node->layer_count; i++)
			{
				node->layer[i].type = -1;
				node->layer[i].name[0] = 0;
				node->layer[i].layer = 0;
				node->layer[i].subscribers = NULL;
				node->layer[i].subscribersd = NULL;			
			}
		}
	}
	for(i = 0; i < 16; i++)
		node->layer[layer_id].name[i] = name[i];

	if(node->layer[layer_id].type != type)
	{
		if(node->layer[layer_id].subscribers) {
			vs_destroy_subscription_list(node->layer[layer_id].subscribers);
			node->layer[layer_id].subscribers = NULL;
		}
		if(node->layer[layer_id].subscribersd) {
			vs_destroy_subscription_list(node->layer[layer_id].subscribersd);
			node->layer[layer_id].subscribersd = NULL;
		}
		node->layer[layer_id].type = type;
		free(node->layer[layer_id].layer);
		switch(type)
		{
			case VN_G_LAYER_VERTEX_XYZ :
				node->layer[layer_id].layer = malloc(sizeof(real64) * node->vertex_size * 3);
				for(i = 0; i < node->vertex_size * 3; i++)
					((real64 *)node->layer[layer_id].layer)[i] = ((real64 *)node->layer[0].layer)[i];
			break;
			case VN_G_LAYER_VERTEX_UINT32 :
				node->layer[layer_id].layer = malloc(sizeof(uint32) * node->vertex_size);
				for(i = 0; i < node->vertex_size; i++)
					((uint32 *)node->layer[layer_id].layer)[i] = def_uint;
				node->layer[layer_id].def.integer = def_uint;
			break;
			case VN_G_LAYER_VERTEX_REAL :
				node->layer[layer_id].layer = malloc(sizeof(real64) * node->vertex_size);
				for(i = 0; i < node->vertex_size; i++)
					((real64 *)node->layer[layer_id].layer)[i] = def_real;
				node->layer[layer_id].def.real = def_real;
			break;
			case VN_G_LAYER_POLYGON_CORNER_UINT32 :
				node->layer[layer_id].layer = malloc(sizeof(uint32) * node->poly_size * 4);
				for(i = 0; i < node->poly_size * 4; i++)
					((uint32 *)node->layer[layer_id].layer)[i] = def_uint;
				node->layer[layer_id].def.integer = def_uint;
			break;
			case VN_G_LAYER_POLYGON_CORNER_REAL :
				node->layer[layer_id].layer = malloc(sizeof(real64) * node->poly_size * 4);
				for(i = 0; i < node->poly_size * 4; i++)
					((real64 *)node->layer[layer_id].layer)[i] = def_real;
				node->layer[layer_id].def.real = def_real;
			break;
			case VN_G_LAYER_POLYGON_FACE_UINT8 :
				node->layer[layer_id].layer = malloc(sizeof(uint8) * node->poly_size);
				for(i = 0; i < node->poly_size; i++)
					((uint8 *)node->layer[layer_id].layer)[i] = def_uint;
				node->layer[layer_id].def.integer = def_uint;
			break;
			case VN_G_LAYER_POLYGON_FACE_UINT32 :
				node->layer[layer_id].layer = malloc(sizeof(uint32) * node->poly_size);
				for(i = 0; i < node->poly_size; i++)
					((uint32 *)node->layer[layer_id].layer)[i] = def_uint;
				node->layer[layer_id].def.integer = def_uint;
			break;
			case VN_G_LAYER_POLYGON_FACE_REAL :
				node->layer[layer_id].layer = malloc(sizeof(real64) * node->poly_size);
				for(i = 0; i < node->poly_size; i++)
					((real64 *)node->layer[layer_id].layer)[i] = def_real;
				node->layer[layer_id].def.real = def_real;
			break;
		}
	}
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_layer_create(node_id, layer_id, name, type, def_uint, def_real);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_layer_destroy(void *user, VNodeID node_id, VLayerID layer_id)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || layer_id < 2)
		return;
	free(node->layer[layer_id].layer);
	node->layer[layer_id].layer = NULL;
	node->layer[layer_id].name[0] = 0;
	node->layer[layer_id].type = -1;
	if(node->layer[layer_id].subscribers) {
		vs_destroy_subscription_list(node->layer[layer_id].subscribers);
		node->layer[layer_id].subscribers = NULL;
	}
	if(node->layer[layer_id].subscribersd) {
		vs_destroy_subscription_list(node->layer[layer_id].subscribersd);
		node->layer[layer_id].subscribersd = NULL;
	}

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_layer_destroy(node_id, layer_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_layer_subscribe(void *user, VNodeID node_id, VLayerID layer_id, uint8 type)
{
	VSNodeGeometry		*node;
	VSNGLayer		*layer;
	VSSubscriptionList	**list = NULL;
	unsigned int i;
	
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;

	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL)
		return;
	/* Pick subscription list to add subscriber to. */
	layer = &node->layer[layer_id];
	if(type == VN_FORMAT_REAL64 && (layer->type == VN_G_LAYER_VERTEX_XYZ || layer->type == VN_G_LAYER_VERTEX_REAL ||
	    layer->type == VN_G_LAYER_POLYGON_CORNER_REAL || layer->type == VN_G_LAYER_POLYGON_FACE_REAL))
	{
		list = &node->layer[layer_id].subscribersd;
	}
	else
		list = &node->layer[layer_id].subscribers;

	/* Add new subscriptor to whichever list was chosen by precision-test above. Create list if necessary. */
	if(list == NULL)
		return;
	if(*list == NULL)
		*list = vs_create_subscription_list();
	vs_add_new_subscriptor(*list);

	switch(layer->type)
	{
		case VN_G_LAYER_VERTEX_XYZ :
			if(type == VN_FORMAT_REAL64)
			{
				for(i = 0; i < node->vertex_size; i++)
					if(((real64 *)node->layer[0].layer)[i * 3] != V_REAL64_MAX)
						verse_send_g_vertex_set_xyz_real64(node_id, layer_id, i, ((real64 *)layer->layer)[i * 3], ((real64 *)layer->layer)[i * 3 + 1], ((real64 *)layer->layer)[i * 3 + 2]);
			}else
			{
				for(i = 0; i < node->vertex_size; i++)
					if(((real64 *)node->layer[0].layer)[i * 3] != V_REAL64_MAX)
						verse_send_g_vertex_set_xyz_real32(node_id, layer_id, i, (float)((real64 *)layer->layer)[i * 3], (float)((real64 *)layer->layer)[i * 3 + 1], (float)((real64 *)layer->layer)[i * 3 + 2]);
			}
			break;
		case VN_G_LAYER_VERTEX_UINT32 :
			for(i = 0; i < node->vertex_size; i++)
				if(((real64 *)node->layer[0].layer)[i * 3] != V_REAL64_MAX && ((uint32 *)layer->layer)[i] != layer->def.integer)
					verse_send_g_vertex_set_uint32(node_id, layer_id, i, ((uint32 *)layer->layer)[i]);
		break;
		case VN_G_LAYER_VERTEX_REAL :
			if(type == VN_FORMAT_REAL64)
			{
				for(i = 0; i < node->vertex_size; i++)
					if(((real64 *)node->layer[0].layer)[i * 3] != V_REAL64_MAX && ((real64 *)layer->layer)[i] != layer->def.real)
						verse_send_g_vertex_set_real64(node_id, layer_id, i, ((real64 *)layer->layer)[i]);
			}else
			{
				for(i = 0; i < node->vertex_size; i++)
					if(((real64 *)node->layer[0].layer)[i * 3] != V_REAL64_MAX && ((real64 *)layer->layer)[i] != layer->def.real)
						verse_send_g_vertex_set_real32(node_id, layer_id, i, (float)((real64 *)layer->layer)[i]);
			}
		break;
		case VN_G_LAYER_POLYGON_CORNER_UINT32 :
			for(i = 0; i < node->poly_size; i++)
				if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && !(((uint32 *)layer->layer)[i * 4] == layer->def.integer && ((uint32 *)layer->layer)[i * 4 + 1] == layer->def.integer && ((uint32 *)layer->layer)[i * 4 + 2] == layer->def.integer && ((uint32 *)layer->layer)[i * 4 + 3] == layer->def.integer))
					verse_send_g_polygon_set_corner_uint32(node_id, layer_id, i, ((uint32 *)layer->layer)[i * 4], ((uint32 *)layer->layer)[i * 4 + 1], ((uint32 *)layer->layer)[i * 4 + 2], ((uint32 *)layer->layer)[i * 4 + 3]);
		break;
		case VN_G_LAYER_POLYGON_CORNER_REAL :
			if(type == VN_FORMAT_REAL64)
			{
				for(i = 0; i < node->poly_size; i++)
					if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && !(((real64 *)layer->layer)[i * 4] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 1] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 2] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 3] == layer->def.real))
						verse_send_g_polygon_set_corner_real64(node_id, layer_id, i, ((real64 *)layer->layer)[i * 4], ((real64 *)layer->layer)[i * 4 + 1], ((real64 *)layer->layer)[i * 4 + 2], ((real64 *)layer->layer)[i * 4 + 3]);
			}else
			{
				for(i = 0; i < node->poly_size; i++)
					if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && !(((real64 *)layer->layer)[i * 4] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 1] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 2] == layer->def.real && ((real64 *)layer->layer)[i * 4 + 3] == layer->def.real))
						verse_send_g_polygon_set_corner_real32(node_id, layer_id, i, (float)((real64 *)layer->layer)[i * 4], (float)((real64 *)layer->layer)[i * 4 + 1], (float)((real64 *)layer->layer)[i * 4 + 2], (float)((real64 *)layer->layer)[i * 4 + 3]);
			}
		break;
		case VN_G_LAYER_POLYGON_FACE_UINT8 :
			for(i = 0; i < node->poly_size; i++)
				if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && ((uint8 *)layer->layer)[i] != layer->def.integer)
					verse_send_g_polygon_set_face_uint8(node_id, layer_id, i, ((uint8 *)layer->layer)[i]);
		break;
		case VN_G_LAYER_POLYGON_FACE_UINT32 :
			for(i = 0; i < node->poly_size; i++)
				if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && ((uint32 *)layer->layer)[i] != layer->def.integer)
					verse_send_g_polygon_set_face_uint32(node_id, layer_id, i, ((uint32 *)layer->layer)[i]);
		break;
		case VN_G_LAYER_POLYGON_FACE_REAL :
			if(type == VN_FORMAT_REAL64)
			{
				for(i = 0; i < node->poly_size; i++)
					if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && ((real64 *)layer->layer)[i] != layer->def.real)
						verse_send_g_polygon_set_face_real64(node_id, layer_id, i, ((real64 *)layer->layer)[i]);
			}else
			{
				for(i = 0; i < node->poly_size; i++)
					if(((uint32 *)node->layer[1].layer)[i * 4] != (uint32) ~0u && ((real64 *)layer->layer)[i] != layer->def.real)
						verse_send_g_polygon_set_face_real32(node_id, layer_id, i, (float)((real64 *)layer->layer)[i]);
			}
		break;
	}
}

static void callback_send_g_layer_unsubscribe(void *user, VNodeID node_id, VLayerID layer_id)
{
	VSNodeGeometry *node;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL)
		return;
	if(node->layer[layer_id].subscribers)
		vs_remove_subscriptor(node->layer[layer_id].subscribers);
	if(node->layer[layer_id].subscribersd)
		vs_remove_subscriptor(node->layer[layer_id].subscribersd);
}


static unsigned int vs_g_extend_arrays(VSNodeGeometry *node, boolean vertex, boolean base_layer, unsigned int id)
{
	unsigned int i, j;

	if(base_layer && id == ~0u)
	{
		if(vertex)
		{
			while(node->vertex_hole < node->vertex_size && ((real64 *)node->layer[0].layer)[node->vertex_hole * 3] != V_REAL64_MAX)
				node->vertex_hole++;
			id = node->vertex_hole;
		}else
		{
			while(node->polygon_hole < node->poly_size && ((uint32 *)node->layer[1].layer)[node->polygon_hole * 4] != ~0u)
				node->polygon_hole++;
			id = node->polygon_hole;
		}
	}

	if(vertex)
	{
		if(node->vertex_size + 4096 < id)
			return -1;
		if(node->vertex_size > id)
			return id;
	}else
	{
		if(node->poly_size + 4096 < id)
			return -1;
		if(node->poly_size > id)
			return id;
	}

	for(i = 0; i < node->layer_count; i++)
	{
		if((vertex && node->layer[i].type < VN_G_LAYER_POLYGON_CORNER_UINT32) || (!vertex && node->layer[i].type >= VN_G_LAYER_POLYGON_CORNER_UINT32))
		{
			switch(node->layer[i].type)
			{
				case VN_G_LAYER_VERTEX_XYZ :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(real64) * (id + VS_G_LAYER_CHUNK) * 3);
					for(j = node->vertex_size * 3; j < (id + VS_G_LAYER_CHUNK) * 3; j++)
						((real64 *)node->layer[i].layer)[j] = V_REAL64_MAX;
				break;
				case VN_G_LAYER_VERTEX_UINT32 :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(uint32) * (id + VS_G_LAYER_CHUNK));
					for(j = node->vertex_size; j < (id + VS_G_LAYER_CHUNK); j++)
						((uint32 *)node->layer[i].layer)[j] = node->layer[i].def.integer;
				break;
				case VN_G_LAYER_VERTEX_REAL :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(real64) * (id + VS_G_LAYER_CHUNK));
					for(j = node->vertex_size; j < (id + VS_G_LAYER_CHUNK); j++)
						((real64 *)node->layer[i].layer)[j] = node->layer[i].def.real;
				break;
				case VN_G_LAYER_POLYGON_CORNER_UINT32 :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(uint32) * (id + VS_G_LAYER_CHUNK) * 4);
					for(j = node->poly_size * 4; j < (id + VS_G_LAYER_CHUNK) * 4; j++)
						((uint32 *)node->layer[i].layer)[j] = node->layer[i].def.integer;
				break;
				case VN_G_LAYER_POLYGON_CORNER_REAL :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(real64) * (id + VS_G_LAYER_CHUNK) * 4);
					for(j = node->poly_size * 4; j < (id + VS_G_LAYER_CHUNK) * 4; j++)
						((real64 *)node->layer[i].layer)[j] = node->layer[i].def.real;
				break;
				case VN_G_LAYER_POLYGON_FACE_UINT8 :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(uint8) * (id + VS_G_LAYER_CHUNK));
					for(j = node->poly_size; j < (id + VS_G_LAYER_CHUNK); j++)
						((uint8 *)node->layer[i].layer)[j] = node->layer[i].def.integer;
				break;
				case VN_G_LAYER_POLYGON_FACE_UINT32 :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(uint32) * (id + VS_G_LAYER_CHUNK));
					for(j = node->poly_size; j < (id + VS_G_LAYER_CHUNK); j++)
						((uint32 *)node->layer[i].layer)[j] = node->layer[i].def.integer;
				break;
				case VN_G_LAYER_POLYGON_FACE_REAL :
					node->layer[i].layer = realloc(node->layer[i].layer, sizeof(real64) * (id + VS_G_LAYER_CHUNK));
					for(j = node->poly_size; j < (id + VS_G_LAYER_CHUNK); j++)
						((real64 *)node->layer[i].layer)[j] = node->layer[i].def.real;
				break;
			}
		}
	}
	if(vertex)
		node->vertex_size = id + VS_G_LAYER_CHUNK;
	else
		node->poly_size = id + VS_G_LAYER_CHUNK;
	return id;
}


static void callback_send_g_vertex_set_xyz_real32(void *user, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, float x, float y, float z)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_VERTEX_XYZ)
		return;
	if((vertex_id = vs_g_extend_arrays(node, TRUE, layer_id == 0, vertex_id)) == ~0u)
		return;
	if(((real64 *)node->layer[0].layer)[vertex_id * 3] == V_REAL64_MAX)
	{
		for(i = 0; i < node->layer_count; i++)
		{
			if(node->layer[i].name[0] != 0 && node->layer[i].type == VN_G_LAYER_VERTEX_XYZ && node->layer[i].layer != NULL)
			{
				((real64 *)node->layer[i].layer)[vertex_id * 3] = x;
				((real64 *)node->layer[i].layer)[vertex_id * 3 + 1] = y;
				((real64 *)node->layer[i].layer)[vertex_id * 3 + 2] = z;
			}
		}
		layer_id = 0;
	}else
	{
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3] = x;
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3 + 1] = y;
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3 + 2] = z;
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_vertex_set_xyz_real64(node_id, layer_id, vertex_id, (real64)x, (real64)y, (real64)z);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_vertex_set_xyz_real32(node_id, layer_id, vertex_id, x, y, z);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_vertex_set_xyz_real64(void *user, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real64 x, real64 y, real64 z)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_VERTEX_XYZ)
		return;
	if((vertex_id = vs_g_extend_arrays(node, TRUE, layer_id == 0, vertex_id)) == ~0u)
		return;
	if(((real64 *)node->layer[0].layer)[vertex_id * 3] == V_REAL64_MAX)
	{
		for(i = 0; i < node->layer_count; i++)
		{
			if(node->layer[i].name[0] != 0 && node->layer[i].type == VN_G_LAYER_VERTEX_XYZ && node->layer[i].layer != NULL)
			{
				((real64 *)node->layer[i].layer)[vertex_id * 3] = x;
				((real64 *)node->layer[i].layer)[vertex_id * 3 + 1] = y;
				((real64 *)node->layer[i].layer)[vertex_id * 3 + 2] = z;
			}
		}
		layer_id = 0;
	}else
	{
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3] = x;
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3 + 1] = y;
		((real64 *)node->layer[layer_id].layer)[vertex_id * 3 + 2] = z;
	}
	count = vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_vertex_set_xyz_real64(node_id, layer_id, vertex_id, x, y, z);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_vertex_set_xyz_real32(node_id, layer_id, vertex_id, (float)x, (float)y, (float)z);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_vertex_delete_real(void *user, VNodeID node_id, uint32 vertex_id)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(vertex_id >= node->vertex_size)
		return;
	if(vertex_id < node->vertex_hole)
		node->vertex_hole = vertex_id;
	for(i = 0; i < node->layer_count; i++)
		if(node->layer[i].name[0] != 0 && node->layer[i].type == VN_G_LAYER_VERTEX_XYZ && node->layer[i].layer != NULL)
			((real64 *)node->layer[i].layer)[vertex_id * 3] = V_REAL64_MAX;
	count =	vs_get_subscript_count(node->layer[0].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[0].subscribers, i);
		verse_send_g_vertex_delete_real32(node_id, vertex_id);
	}
	count =	vs_get_subscript_count(node->layer[0].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[0].subscribersd, i);
		verse_send_g_vertex_delete_real64(node_id, vertex_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_vertex_set_uint32(void *user, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, uint32 value)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_VERTEX_UINT32)
		return;
	if((vertex_id = vs_g_extend_arrays(node, TRUE, FALSE, vertex_id)) == ~0u)
		return;
	((uint32 *)node->layer[layer_id].layer)[vertex_id] = value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_vertex_set_uint32(node_id, layer_id, vertex_id, value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_vertex_set_real64(void *user, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, real64 value)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_VERTEX_REAL)
		return;
	if((vertex_id = vs_g_extend_arrays(node, TRUE, FALSE, vertex_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[vertex_id] = value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_vertex_set_real32(node_id, layer_id, vertex_id, (float)value);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_vertex_set_real64(node_id, layer_id, vertex_id, value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_vertex_set_real32(void *user, VNodeID node_id, VLayerID layer_id, uint32 vertex_id, float value)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_VERTEX_REAL)
		return;
	if((vertex_id = vs_g_extend_arrays(node, TRUE, FALSE, vertex_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[vertex_id] = (real64)value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_vertex_set_real32(node_id, layer_id, vertex_id, value);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_vertex_set_real64(node_id, layer_id, vertex_id, (real64)value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_corner_uint32(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint32 v0, uint32 v1, uint32 v2, uint32 v3)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_CORNER_UINT32)
		return;
	if(layer_id == 1 && (v0 == v1 || v1 == v2 || v2 == v3 || v3 == v0 || v0 == v2 || v1 == v3))
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, layer_id == 1, polygon_id)) == ~0u)
		return;
	((uint32 *)node->layer[layer_id].layer)[polygon_id * 4] = v0;
	((uint32 *)node->layer[layer_id].layer)[polygon_id * 4 + 1] = v1;
	((uint32 *)node->layer[layer_id].layer)[polygon_id * 4 + 2] = v2;
	((uint32 *)node->layer[layer_id].layer)[polygon_id * 4 + 3] = v3;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_corner_uint32(node_id, layer_id, polygon_id, v0, v1, v2, v3);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_corner_real64(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real64 v0, real64 v1, real64 v2, real64 v3)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_CORNER_REAL)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4] = v0;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 1] = v1;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 2] = v2;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 3] = v3;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_corner_real32(node_id, layer_id, polygon_id, (float)v0, (float)v1, (float)v2, (float)v3);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_polygon_set_corner_real64(node_id, layer_id, polygon_id, v0, v1, v2, v3);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_corner_real32(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, float v0, float v1, float v2, float v3)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_CORNER_REAL)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4] = v0;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 1] = v1;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 2] = v2;
	((real64 *)node->layer[layer_id].layer)[polygon_id * 4 + 3] = v3;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_corner_real32(node_id, layer_id, polygon_id, v0, v1, v2, v3);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_polygon_set_corner_real64(node_id, layer_id, polygon_id, (real64)v0, (real64)v1, (real64)v2, (real64)v3);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_face_uint8(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint8 value)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_FACE_UINT8)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((uint8 *)node->layer[layer_id].layer)[polygon_id] = value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_face_uint8(node_id, layer_id, polygon_id, value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_face_uint32(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, uint32 value)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_FACE_UINT32)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((uint32 *)node->layer[layer_id].layer)[polygon_id] = value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_face_uint32(node_id, layer_id, polygon_id, value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_face_real64(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, real64 value)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_FACE_REAL)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[polygon_id] = value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_face_real32(node_id, layer_id, polygon_id, (float)value);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_polygon_set_face_real64(node_id, layer_id, polygon_id, value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_set_face_real32(void *user, VNodeID node_id, VLayerID layer_id, uint32 polygon_id, float value)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(layer_id >= node->layer_count || node->layer[layer_id].layer == NULL || node->layer[layer_id].type != VN_G_LAYER_POLYGON_FACE_REAL)
		return;
	if((polygon_id = vs_g_extend_arrays(node, FALSE, FALSE, polygon_id)) == ~0u)
		return;
	((real64 *)node->layer[layer_id].layer)[polygon_id] = (real64)value;
	count =	vs_get_subscript_count(node->layer[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribers, i);
		verse_send_g_polygon_set_face_real32(node_id, layer_id, polygon_id, value);
	}
	count =	vs_get_subscript_count(node->layer[layer_id].subscribersd);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[layer_id].subscribersd, i);
		verse_send_g_polygon_set_face_real64(node_id, layer_id, polygon_id, (real64)value);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_polygon_delete(void *user, VNodeID node_id, uint32 polygon_id)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;

	if(polygon_id >= node->poly_size || ((uint32 *)node->layer[1].layer)[polygon_id * 4] == ~0u)
		return;
	if(polygon_id < node->polygon_hole)
		node->polygon_hole = polygon_id;

	((uint32 *)node->layer[1].layer)[polygon_id * 4] = ~0u;
	count =	vs_get_subscript_count(node->layer[1].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layer[1].subscribers, i);
		verse_send_g_polygon_delete(node_id, polygon_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_crease_set_vertex(void *user, VNodeID node_id, const char *layer, uint32 def_crease)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	node->crease_vertex = def_crease;
	v_strlcpy(node->crease_vertex_layer, layer, sizeof node->crease_vertex_layer);
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_crease_set_vertex(node_id, layer, def_crease);
	}
	vs_reset_subscript_session();
}

static void callback_send_g_crease_set_edge(void *user, VNodeID node_id, const char *layer, uint32 def_crease)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	node->crease_edge = def_crease;
	v_strlcpy(node->crease_edge_layer, layer, sizeof node->crease_edge_layer);
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_crease_set_edge(node_id, layer, def_crease);
	}
	vs_reset_subscript_session();
}

void callback_send_g_bone_create(void *user, VNodeID node_id, uint16 bone_id, const char *weight,
				 const char *reference, uint16 parent,
				 real64 pos_x, real64 pos_y, real64 pos_z,
				 const char *position_label, const char *rotation_label, const char *scale_label)
{
	VSNodeGeometry *node;
	unsigned int i, count;

	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(bone_id >= node->bone_count || node->bones[bone_id].weight[0] == '\0')
	{
		/* Find free bone to re-use, if any. */
		for(bone_id = 0; bone_id < node->bone_count && node->bones[bone_id].weight[0] != '\0'; bone_id++)
			;
		if(bone_id == node->bone_count)
		{
			bone_id = node->bone_count;
			node->bone_count += 16;
			node->bones = realloc(node->bones, (sizeof *node->bones) * node->bone_count);
			for(i = bone_id; i < node->bone_count; i++)
				node->bones[i].weight[0] = '\0';
		}
	}
	v_strlcpy(node->bones[bone_id].weight, weight, sizeof node->bones[bone_id].weight);
	v_strlcpy(node->bones[bone_id].reference, reference, sizeof node->bones[bone_id].reference);
	node->bones[bone_id].parent = parent;
	node->bones[bone_id].pos_x = pos_x;
	node->bones[bone_id].pos_y = pos_y;
	node->bones[bone_id].pos_z = pos_z;
	v_strlcpy(node->bones[bone_id].position_label, position_label, sizeof node->bones[bone_id].position_label);
	v_strlcpy(node->bones[bone_id].rotation_label, rotation_label, sizeof node->bones[bone_id].rotation_label);
	v_strlcpy(node->bones[bone_id].scale_label,    scale_label,    sizeof node->bones[bone_id].scale_label);

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_bone_create(node_id, bone_id, weight, reference, parent, pos_x, pos_y, pos_z, position_label, rotation_label, scale_label);
	}
	vs_reset_subscript_session();
}

void callback_send_g_bone_destroy(void *user, VNodeID node_id, uint32 bone_id)
{
	VSNodeGeometry *node;
	unsigned int i, count;
	node = (VSNodeGeometry *)vs_get_node(node_id, V_NT_GEOMETRY);
	if(node == NULL)
		return;
	if(bone_id >= node->bone_count || node->bones[bone_id].weight[0] == 0)
		return;
	node->bones[bone_id].weight[0] = 0;

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_g_bone_destroy(node_id, bone_id);
	}
	vs_reset_subscript_session();
}

void vs_g_callback_init(void)
{
	verse_callback_set(verse_send_g_layer_create, callback_send_g_layer_create, NULL);
	verse_callback_set(verse_send_g_layer_destroy, callback_send_g_layer_destroy, NULL);
	verse_callback_set(verse_send_g_layer_subscribe, callback_send_g_layer_subscribe, NULL);
	verse_callback_set(verse_send_g_layer_unsubscribe, callback_send_g_layer_unsubscribe, NULL);
	verse_callback_set(verse_send_g_vertex_set_xyz_real32, callback_send_g_vertex_set_xyz_real32, NULL);
	verse_callback_set(verse_send_g_vertex_set_xyz_real64, callback_send_g_vertex_set_xyz_real64, NULL);
	verse_callback_set(verse_send_g_vertex_set_uint32, callback_send_g_vertex_set_uint32, NULL);
	verse_callback_set(verse_send_g_vertex_set_real32, callback_send_g_vertex_set_real32, NULL);
	verse_callback_set(verse_send_g_vertex_set_real64, callback_send_g_vertex_set_real64, NULL);
	verse_callback_set(verse_send_g_vertex_delete_real32, callback_send_g_vertex_delete_real, NULL);
	verse_callback_set(verse_send_g_vertex_delete_real64, callback_send_g_vertex_delete_real, NULL);
	verse_callback_set(verse_send_g_polygon_set_corner_uint32, callback_send_g_polygon_set_corner_uint32, NULL);
	verse_callback_set(verse_send_g_polygon_set_corner_real32, callback_send_g_polygon_set_corner_real32, NULL);
	verse_callback_set(verse_send_g_polygon_set_corner_real64, callback_send_g_polygon_set_corner_real64, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_uint8, callback_send_g_polygon_set_face_uint8, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_uint32, callback_send_g_polygon_set_face_uint32, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_real32, callback_send_g_polygon_set_face_real32, NULL);
	verse_callback_set(verse_send_g_polygon_set_face_real64, callback_send_g_polygon_set_face_real64, NULL);
	verse_callback_set(verse_send_g_polygon_delete, callback_send_g_polygon_delete, NULL);
	verse_callback_set(verse_send_g_crease_set_vertex, callback_send_g_crease_set_vertex, NULL);
	verse_callback_set(verse_send_g_crease_set_edge, callback_send_g_crease_set_edge, NULL);
	verse_callback_set(verse_send_g_bone_create, callback_send_g_bone_create, NULL);
	verse_callback_set(verse_send_g_bone_destroy, callback_send_g_bone_destroy, NULL);
}

#endif
