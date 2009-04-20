/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "vs_server.h"

typedef struct {
	VNBLayerType		type;
	char			name[16];
	void			*layer;
	VSSubscriptionList	*subscribers;
} VSNBLayers;

typedef struct {
	VSNodeHead	head;
	uint16		size[3];
	uint32		partial_tile_col, partial_tile_row;	/* These rows/columns are partial. ~0 for none. */
	VSNBLayers	*layers;
	unsigned int	layer_count;
} VSNodeBitmap;

static unsigned long	tile_counter = 0;

VSNodeBitmap * vs_b_create_node(unsigned int owner)
{
	VSNodeBitmap *node;
	char name[48];
	node = malloc(sizeof *node);
	vs_add_new_node(&node->head, V_NT_BITMAP);
	sprintf(name, "Bitmap_Node_%u", node->head.id);
	create_node_head(&node->head, name, owner);

	node->size[0] = 0;
	node->size[1] = 0;
	node->size[2] = 0;
	node->partial_tile_col = ~0;
	node->partial_tile_row = ~0;
	node->layers = NULL;
	node->layer_count = 0;

	return node;
}

void vs_b_destroy_node(VSNodeBitmap *node)
{
	unsigned int i;
	destroy_node_head(&node->head);
	if(node->layers != NULL)
	{
		for(i = 0; i < node->layer_count; i++)
			if(node->layers[i].layer != NULL)
				free(node->layers[i].layer);
		free(node->layers);
	}
	free(node);
}

void vs_b_subscribe(VSNodeBitmap *node)
{
	unsigned int i;
	verse_send_b_dimensions_set(node->head.id, node->size[0], node->size[1], node->size[2]);
	for(i = 0; i < node->layer_count; i++)
		if(node->layers[i].name[0] != 0)
			verse_send_b_layer_create(node->head.id, (uint16)i, node->layers[i].name, (uint8)node->layers[i].type);
}


void vs_b_unsubscribe(VSNodeBitmap *node)
{
	unsigned int i;
	for(i = 0; i < node->layer_count; i++)
		if(node->layers[i].name[0] != 0)
			vs_remove_subscriptor(node->layers[i].subscribers);
}

static void callback_send_b_dimensions_set(void *user, VNodeID node_id, uint16 width, uint16 height, uint16 depth)
{
	VSNodeBitmap *node;
	unsigned int i, j, k, count, tiles[2], read, write, end = 0;

	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
		return;
	tiles[0] = (width + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE;
	tiles[1] = (height + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE;
	node->size[0] = (node->size[0] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE;
	node->size[1] = (node->size[1] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE;
	if(node->size[0] > tiles[0])
		node->size[0] = tiles[0];
	if(node->size[1] > tiles[1])
		node->size[1] = tiles[1];
	if(node->size[2] > depth)
		node->size[2] = depth;

	for(i = 0; i < node->layer_count; i++)
	{
		if(node->layers[i].name[0] != 0)
		{
			switch(node->layers[i].type)
			{
				case VN_B_LAYER_UINT1 :
				{
					uint16 *array, *old;
					write = 0;
					old = node->layers[i].layer;
					array = node->layers[i].layer = malloc((sizeof *array) * tiles[0] * tiles[1] * depth);
					for(j = 0; j < node->size[2]; j++)
					{
						for(k = 0; k < node->size[1]; k++)
						{
							read = (j * node->size[1] * node->size[0] + k * node->size[0]);
						
							end = (j * tiles[1] * tiles[0] + k * tiles[0] + node->size[0]);
							while(write < end)
								array[write++] = old[read++];
					
							end = (j * tiles[1] * tiles[0] + (k + 1) * tiles[0]);
							while(write < end)
								array[write++] = 0;
						}
						end = ((j + 1) * tiles[1] * tiles[0]);
							while(write < end)
								array[write++] = 0;
					}
					k = depth * tiles[1] * tiles[0];
					while(write < end)
						array[write++] = 0;
				}
				break;
				case VN_B_LAYER_UINT8 :
				{
					uint8 *array, *old;
					write = 0;
					old = node->layers[i].layer;
					array = node->layers[i].layer = malloc((sizeof *array) * tiles[0] * tiles[1] * depth * VN_B_TILE_SIZE * VN_B_TILE_SIZE);
					for(j = 0; j < node->size[2]; j++)
					{
						for(k = 0; k < node->size[1]; k++)
						{
							read = (j * node->size[1] * node->size[0] + k * node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
						
							end = (j * tiles[1] * tiles[0] + k * tiles[0] + node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = old[read++];
					
							end = (j * tiles[1] * tiles[0] + (k + 1) * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
						}
						end = ((j + 1) * tiles[1] * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
					}
					end = depth * tiles[1] * tiles[0] * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
					while(write < end)
						array[write++] = 0;
				}
				break;
				case VN_B_LAYER_UINT16 :
				{
					uint16 *array, *old;
					write = 0;
					old = node->layers[i].layer;
					array = node->layers[i].layer = malloc((sizeof *array) * tiles[0] * tiles[1] * depth * VN_B_TILE_SIZE * VN_B_TILE_SIZE);
					for(j = 0; j < node->size[2]; j++)
					{
						for(k = 0; k < node->size[1]; k++)
						{
							read = (j * node->size[1] * node->size[0] + k * node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
						
							end = (j * tiles[1] * tiles[0] + k * tiles[0] + node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = old[read++];
					
							end = (j * tiles[1] * tiles[0] + (k + 1) * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
						}
						end = ((j + 1) * tiles[1] * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
					}
					end = depth * tiles[1] * tiles[0] * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
					while(write < end)
						array[write++] = 0;
				}
				break;
				case VN_B_LAYER_REAL32 :
				{
					real32 *array, *old;
					write = 0;
					old = node->layers[i].layer;
					array = node->layers[i].layer = malloc((sizeof *array) * tiles[0] * tiles[1] * depth * VN_B_TILE_SIZE * VN_B_TILE_SIZE);
					for(j = 0; j < node->size[2]; j++)
					{
						for(k = 0; k < node->size[1]; k++)
						{
							read = (j * node->size[1] * node->size[0] + k * node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
						
							end = (j * tiles[1] * tiles[0] + k * tiles[0] + node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = old[read++];
					
							end = (j * tiles[1] * tiles[0] + (k + 1) * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
						}
						end = ((j + 1) * tiles[1] * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
					}
					end = depth * tiles[1] * tiles[0] * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
					while(write < end)
						array[write++] = 0;
				}
				break;
				case VN_B_LAYER_REAL64 :
				{
					real64 *array, *old;
					write = 0;
					old = node->layers[i].layer;
					array = node->layers[i].layer = malloc((sizeof *array) * tiles[0] * tiles[1] * depth * VN_B_TILE_SIZE * VN_B_TILE_SIZE);
					for(j = 0; j < node->size[2]; j++)
					{
						for(k = 0; k < node->size[1]; k++)
						{
							read = (j * node->size[1] * node->size[0] + k * node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
						
							end = (j * tiles[1] * tiles[0] + k * tiles[0] + node->size[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = old[read++];
					
							end = (j * tiles[1] * tiles[0] + (k + 1) * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
						}
						end = ((j + 1) * tiles[1] * tiles[0]) * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
							while(write < end)
								array[write++] = 0;
					}
					end = depth * tiles[1] * tiles[0] * VN_B_TILE_SIZE * VN_B_TILE_SIZE;
					while(write < end)
						array[write++] = 0;
				}
				break;
			}
		}
	}

	node->size[0] = width;
	node->size[1] = height;
	node->size[2] = depth;
	node->partial_tile_col = (width  % VN_B_TILE_SIZE) != 0 ? (width  + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE - 1 : ~0;
	node->partial_tile_row = (height % VN_B_TILE_SIZE) != 0 ? (height + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE - 1 : ~0;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_b_dimensions_set(node_id, width, height, depth);
	}
	vs_reset_subscript_session();
}

static void callback_send_b_layer_create(void *user, VNodeID node_id, VLayerID layer_id, char *name, uint8 type)
{
	VSNodeBitmap *node;
	unsigned int i, count;

	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
		return;
	if(node->layer_count <= layer_id || node->layers[layer_id].name[0] == 0)
	{
		for(layer_id = 0; layer_id < node->layer_count && node->layers[layer_id].name[0] != 0; layer_id++)
			;
		if(layer_id == node->layer_count)
		{
			node->layers = realloc(node->layers, (sizeof *node->layers) * (node->layer_count + 16));
			for(i = node->layer_count; i < node->layer_count + 16; i++)
			{
				node->layers[i].layer = NULL;
				node->layers[i].type = 0;
				node->layers[i].name[0] = 0;
				node->layers[i].subscribers = NULL;
			}
			node->layer_count += 16;
		}
		node->layers[layer_id].subscribers = vs_create_subscription_list();
		node->layers[layer_id].type = type + 1;
	}

	if(node->layers[layer_id].type != type || node->layers[layer_id].name[0] == 0)
	{
		count = ((node->size[0] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE) * ((node->size[1] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE);
		count *= VN_B_TILE_SIZE * VN_B_TILE_SIZE * node->size[2];
		if(node->layers[layer_id].layer != NULL)
			free(node->layers[layer_id].layer);
		if(count != 0)
		{	
			switch(type)
			{
				case VN_B_LAYER_UINT1 :
					node->layers[layer_id].layer = malloc(sizeof(uint8) * count / 8);
					memset(node->layers[layer_id].layer, 0, count * sizeof(uint8) / 8);
				break;
				case VN_B_LAYER_UINT8 :
					node->layers[layer_id].layer = malloc(sizeof(uint8) * count);
					memset(node->layers[layer_id].layer, 0, count * sizeof(uint8));
				break;
				case VN_B_LAYER_UINT16 :
					node->layers[layer_id].layer = malloc(sizeof(uint16) * count);
					memset(node->layers[layer_id].layer, 0, count * sizeof(uint16));
				break;
				case VN_B_LAYER_REAL32 :
					node->layers[layer_id].layer = malloc(sizeof(real32) * count);
					memset(node->layers[layer_id].layer, 0, count * sizeof(real32));
				break;
				case VN_B_LAYER_REAL64 :
					node->layers[layer_id].layer = malloc(sizeof(real64) * count);
					memset(node->layers[layer_id].layer, 0, count * sizeof(real64));
				break;
			}
		}else
			node->layers[layer_id].layer = NULL;
	}
	node->layers[layer_id].type = type;
	for(i = 0; i < 15 && name[i] != 0; i++)
		node->layers[layer_id].name[i] = name[i];
	node->layers[layer_id].name[i] = 0;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_b_layer_create(node_id, layer_id, name, type);
	}
	vs_reset_subscript_session();
}

static void callback_send_b_layer_destroy(void *user, VNodeID node_id, VLayerID layer_id)
{
	VSNodeBitmap *node;
	unsigned int i, count;
	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
		return;
	if(layer_id >= node->layer_count || node->layers[layer_id].layer == NULL)
		return;
	if(node->layers[layer_id].layer != NULL)
		free(node->layers[layer_id].layer);
	node->layers[layer_id].layer = NULL;
	node->layers[layer_id].type = 0;
	node->layers[layer_id].name[0] = 0;
	vs_destroy_subscription_list(node->layers[layer_id].subscribers);
	node->layers[layer_id].subscribers = NULL;

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_b_layer_destroy(node_id, layer_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_b_layer_subscribe(void *user, VNodeID node_id, VLayerID layer_id, uint8 level)
{
	VSNodeBitmap	*node;
	const void *data;
	unsigned int i, j, k, tile[3];

	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
		return;
	if(layer_id >= node->layer_count || node->layers[layer_id].layer == NULL)
		return;
	if(vs_add_new_subscriptor(node->layers[layer_id].subscribers) == 0)
		return;
	tile[0] = ((node->size[0] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE);
	tile[1] = ((node->size[1] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE);
	tile[2] = node->size[2];
	data = node->layers[layer_id].layer;
	switch(node->layers[layer_id].type)
	{
	case VN_B_LAYER_UINT1:
		for(i = 0; i < tile[0]; i++)
			for(j = 0; j < tile[1]; j++)
				for(k = 0; k < tile[2]; k++)
					verse_send_b_tile_set(node_id, layer_id, i, j, k, VN_B_LAYER_UINT1, (VNBTile *) &((uint8*)data)[(tile[0] * tile[1] * k + j * tile[0] + i) * VN_B_TILE_SIZE * VN_B_TILE_SIZE / 8]);
	break;
	case VN_B_LAYER_UINT8 :
		for(i = 0; i < tile[0]; i++)
			for(j = 0; j < tile[1]; j++)
				for(k = 0; k < tile[2]; k++)
					verse_send_b_tile_set(node_id, layer_id, i, j, k, VN_B_LAYER_UINT8, (VNBTile *) &((uint8*)data)[(tile[0] * tile[1] * k + j * tile[0] + i) * VN_B_TILE_SIZE * VN_B_TILE_SIZE]);
	break;
	case VN_B_LAYER_UINT16 :
		for(i = 0; i < tile[0]; i++)
			for(j = 0; j < tile[1]; j++)
				for(k = 0; k < tile[2]; k++)
					verse_send_b_tile_set(node_id, layer_id, i, j, k, VN_B_LAYER_UINT16, (VNBTile *) &((uint16*)data)[(tile[0] * tile[1] * k + j * tile[0] + i) * VN_B_TILE_SIZE * VN_B_TILE_SIZE]);
	break;
	case VN_B_LAYER_REAL32 :
		for(i = 0; i < tile[0]; i++)
			for(j = 0; j < tile[1]; j++)
				for(k = 0; k < tile[2]; k++)
					verse_send_b_tile_set(node_id, layer_id, i, j, k, VN_B_LAYER_REAL32, (VNBTile *) &((real32*)data)[(tile[0] * tile[1] * k + j * tile[0] + i) * VN_B_TILE_SIZE * VN_B_TILE_SIZE]);
	break;
	case VN_B_LAYER_REAL64 :
		for(i = 0; i < tile[0]; i++)
			for(j = 0; j < tile[1]; j++)
				for(k = 0; k < tile[2]; k++)
					verse_send_b_tile_set(node_id, layer_id, i, j, k, VN_B_LAYER_REAL64, (VNBTile *) &((real64*)data)[(tile[0] * tile[1] * k + j * tile[0] + i) * VN_B_TILE_SIZE * VN_B_TILE_SIZE]);
	break;
	}
}

static void callback_send_b_layer_unsubscribe(void *user, VNodeID node_id, VLayerID layer_id)
{
	VSNodeBitmap *node;
	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
		return;
	if(layer_id >= node->layer_count || node->layers[layer_id].layer == NULL)
		return;
	vs_remove_subscriptor(node->layers[layer_id].subscribers);
}

/* Clear any pixels that lie outside the image, so we don't violoate specs when
 * sending (stored version of) the tile out to subscribers.
*/
static void clear_outside(const VSNodeBitmap *node, uint16 tile_x, uint16 tile_y, uint8 type, VNBTile *tile)
{
	int	x, y, bx, by, p;

	by = tile_y * VN_B_TILE_SIZE;
	for(y = p = 0; y < VN_B_TILE_SIZE; y++, by++)
	{
		bx = tile_x * VN_B_TILE_SIZE;
		for(x = 0; x < VN_B_TILE_SIZE; x++, bx++, p++)
		{
			/* Simply test current pixel against bitmap size. Not quick, but simple. */
			if(bx >= node->size[0] || by >= node->size[1])
			{
				switch(type)
				{
				case VN_B_LAYER_UINT1:
					tile->vuint1[y] &= ~(128 >> x);
					break;
				case VN_B_LAYER_UINT8:
					tile->vuint8[p] = 0;
					break;
				case VN_B_LAYER_UINT16:
					tile->vuint16[p] = 0;
					break;
				case VN_B_LAYER_REAL32:
					tile->vreal32[p] = 0.0f;
					break;
				case VN_B_LAYER_REAL64:
					tile->vreal64[p] = 0.0;
					break;
				}
			}
		}
	}
}

static void callback_send_b_tile_set(void *user, VNodeID node_id, VLayerID layer_id,
				     uint16 tile_x, uint16 tile_y, uint16 tile_z, uint8 type, VNBTile *data)
{
	VSNodeBitmap *node;
	unsigned int i, count, tile[3];

	if((node = (VSNodeBitmap *)vs_get_node(node_id, V_NT_BITMAP)) == NULL)
	{
		printf("got tile for unknown bitmpa node %u, aborting\n", node_id);
		return;
	}
	if(layer_id >= node->layer_count || node->layers[layer_id].layer == NULL || node->layers[layer_id].type != type)
	{
		printf("node %u got tile for bad layer %u (have %u) %p\n", node_id, layer_id, node->layer_count, layer_id < node->layer_count ? node->layers[layer_id].layer : NULL);
		return;
	}
	if(tile_x >= ((node->size[0] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE) || tile_y >= ((node->size[1] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE) || tile_z >= node->size[2])
	{
		printf("got tile that is outside image, at (%u,%u,%u)\n", tile_x, tile_y, tile_z);
		return;
	}

	tile_counter++;

	tile[0] = ((node->size[0] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE);
	tile[1] = ((node->size[1] + VN_B_TILE_SIZE - 1) / VN_B_TILE_SIZE);
	tile[2] = node->size[2];

	/* If tile is in a partial column or row, clear the "outside" pixels. */
	if((uint32) tile_x == node->partial_tile_col || (uint32) tile_y == node->partial_tile_row)
		clear_outside(node, tile_x, tile_y, type, data);

	switch(node->layers[layer_id].type)
	{
		case VN_B_LAYER_UINT1 :
		{
			uint8 *p;
			p = &((uint8 *)node->layers[layer_id].layer)[(tile[0] * tile[1] * tile_z + tile_y * tile[0] + tile_x) * VN_B_TILE_SIZE * VN_B_TILE_SIZE/8];
			memcpy(p, data->vuint1, VN_B_TILE_SIZE * sizeof(uint8));
		}
		break;
		case VN_B_LAYER_UINT8 :
		{
			uint8 *p;
			p = &((uint8 *)node->layers[layer_id].layer)[(tile[0] * tile[1] * tile_z + tile_y * tile[0] + tile_x) * VN_B_TILE_SIZE * VN_B_TILE_SIZE];
			memcpy(p, data->vuint8, VN_B_TILE_SIZE * VN_B_TILE_SIZE * sizeof(uint8));
		}
		break;
		case VN_B_LAYER_UINT16 :
		{
			uint16 *p;
			p = &((uint16 *)node->layers[layer_id].layer)[(tile[0] * tile[1] * tile_z + tile_y * tile[0] + tile_x) * VN_B_TILE_SIZE * VN_B_TILE_SIZE];
			memcpy(p, data->vuint16, VN_B_TILE_SIZE * VN_B_TILE_SIZE * sizeof(uint16));
		}
		break;
		case VN_B_LAYER_REAL32 :
		{
			real32 *p;
			p = &((real32 *)node->layers[layer_id].layer)[(tile[0] * tile[1] * tile_z + tile_y * tile[0] + tile_x) * VN_B_TILE_SIZE * VN_B_TILE_SIZE];
			memcpy(p, data->vreal32, VN_B_TILE_SIZE * VN_B_TILE_SIZE * sizeof(real32));
		}
		break;
		case VN_B_LAYER_REAL64 :
		{
			real64 *p;
			p = &((real64 *)node->layers[layer_id].layer)[(tile[0] * tile[1] * tile_z + tile_y * tile[0] + tile_x) * VN_B_TILE_SIZE * VN_B_TILE_SIZE];
			memcpy(p, data->vreal64, VN_B_TILE_SIZE * VN_B_TILE_SIZE * sizeof(real64));
		}
		break;
	}
	count =	vs_get_subscript_count(node->layers[layer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->layers[layer_id].subscribers, i);
		verse_send_b_tile_set(node_id, layer_id, tile_x, tile_y, tile_z, type, data);
	}
	vs_reset_subscript_session();
}

void vs_b_callback_init(void)
{
	verse_callback_set(verse_send_b_dimensions_set, callback_send_b_dimensions_set, NULL);
	verse_callback_set(verse_send_b_layer_create, callback_send_b_layer_create, NULL);
	verse_callback_set(verse_send_b_layer_destroy, callback_send_b_layer_destroy, NULL);
	verse_callback_set(verse_send_b_layer_subscribe, callback_send_b_layer_subscribe, NULL);
	verse_callback_set(verse_send_b_layer_unsubscribe, callback_send_b_layer_unsubscribe, NULL);
	verse_callback_set(verse_send_b_tile_set, callback_send_b_tile_set, NULL);
}

#endif
