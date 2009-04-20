/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "vs_server.h"

#define VS_NODE_STORAGE_CHUNK_SIZE 16

static struct {
	VSNodeHead **nodes;
	unsigned int node_length;
	unsigned int node_allocated;
	VSSubscriptionList *list[V_NT_NUM_TYPES];
} VSNodeStorage;

extern void	callback_send_tag_group_create(void *user, VNodeID node_id, uint16 group_id, const char *name);

void vs_init_node_storage(void)
{
	unsigned int i;
	VSNodeStorage.nodes = malloc((sizeof *VSNodeStorage.nodes) * VS_NODE_STORAGE_CHUNK_SIZE);
	VSNodeStorage.nodes[0] = NULL;
	VSNodeStorage.node_length = 0;
	VSNodeStorage.node_allocated = VS_NODE_STORAGE_CHUNK_SIZE;
	for(i = 0; i < V_NT_NUM_TYPES; i++)
		VSNodeStorage.list[i] = vs_create_subscription_list();
}

unsigned int vs_add_new_node(VSNodeHead *node, VNodeType type)
{
	unsigned int i, j;
	for(i = 0; i < VSNodeStorage.node_length && VSNodeStorage.nodes[i] != NULL; i++);
	if(i >= VSNodeStorage.node_allocated)
	{
		j = VSNodeStorage.node_allocated;
		VSNodeStorage.node_allocated += VS_NODE_STORAGE_CHUNK_SIZE;
		VSNodeStorage.nodes = realloc(VSNodeStorage.nodes, (sizeof *VSNodeStorage.nodes) * VSNodeStorage.node_allocated);
		while(j < VSNodeStorage.node_allocated)
			VSNodeStorage.nodes[j++] = NULL;
	}
	VSNodeStorage.nodes[i] = node;
	if(i >= VSNodeStorage.node_length)
		VSNodeStorage.node_length = i + 1;
	node->id = i;
	node->type = type;

	return node->id;
}

VSNodeHead *vs_get_node(unsigned int node_id, VNodeType type)
{
	if(VSNodeStorage.node_length > node_id)
	{
		VSNodeHead	*node = VSNodeStorage.nodes[node_id];

		if(node != NULL && node->type == type)
			return node;
	}
	return NULL;
}

VSNodeHead *vs_get_node_head(unsigned int node_id)
{
	if(VSNodeStorage.node_length > node_id)
		return VSNodeStorage.nodes[node_id];
	return NULL;
}

extern VSNodeHead *vs_o_create_node(unsigned int owner);
extern VSNodeHead *vs_g_create_node(unsigned int owner);
extern VSNodeHead *vs_m_create_node(unsigned int owner);
extern VSNodeHead *vs_b_create_node(unsigned int owner);
extern VSNodeHead *vs_t_create_node(unsigned int owner);
extern VSNodeHead *vs_c_create_node(unsigned int owner);
extern VSNodeHead *vs_p_create_node(unsigned int owner);
extern VSNodeHead *vs_a_create_node(unsigned int owner);

extern void vs_o_destroy_node(VSNodeHead *node);
extern void vs_g_destroy_node(VSNodeHead *node);
extern void vs_m_destroy_node(VSNodeHead *node);
extern void vs_b_destroy_node(VSNodeHead *node);
extern void vs_t_destroy_node(VSNodeHead *node);
extern void vs_c_destroy_node(VSNodeHead *node);
extern void vs_p_destroy_node(VSNodeHead *node);
extern void vs_a_destroy_node(VSNodeHead *node);


VNodeID vs_node_create(VNodeID owner_id, unsigned int type)
{
	unsigned int count, i;
	VSNodeHead *node;
	VNodeID node_id;

	printf("vs_node_create(%u, %u)\n", owner_id, type);
	switch(type)
	{
		case V_NT_OBJECT :
			node = vs_o_create_node(owner_id);
		break;
		case V_NT_GEOMETRY :
			node = vs_g_create_node(owner_id);
		break;
		case V_NT_MATERIAL :
			node = vs_m_create_node(owner_id);
		break;
		case V_NT_BITMAP :
			node = vs_b_create_node(owner_id);
		break;
		case V_NT_TEXT :
			node = vs_t_create_node(owner_id);
		break;
		case V_NT_CURVE :
			node = vs_c_create_node(owner_id);
		break;
		case V_NT_AUDIO :
			node = vs_a_create_node(owner_id);
		break;
		default:
			fprintf(stderr, "Can't create node of unknown type %u\n", type);
			return 0U;
	}
	node_id = node->id;

	count =	vs_get_subscript_count(VSNodeStorage.list[type]);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(VSNodeStorage.list[type], i);
		if(owner_id != verse_session_get_avatar())
			verse_send_node_create(node_id, type, VN_OWNER_OTHER);
		else
			verse_send_node_create(node_id, type, VN_OWNER_MINE);
	}
	if(count != 0)
		vs_reset_subscript_session();
	return node_id;
}

/* Initialize an object node into being an avatar. */
void vs_avatar_init(VNodeID id, const char *name)
{
	callback_send_tag_group_create(NULL, id, (short) ~0u, "avatar");
	/* FIXME: Populate the group, too. */
}

void vs_reset_owner(VNodeID owner_id)
{
	unsigned int i;

	for(i = 0; i < VSNodeStorage.node_length; i++)
		if(VSNodeStorage.nodes[i] != NULL)
			if(VSNodeStorage.nodes[i]->owner == owner_id)
				VSNodeStorage.nodes[i]->owner = ~0;
}

static void callback_send_node_create(void *user_data, VNodeID node_id, uint8 type, VNodeOwner owner_id)
{
	vs_node_create(vs_get_avatar(), type);
}

void callback_send_node_destroy(void *user_data, VNodeID node_id)
{
	unsigned int count, i;
	VSNodeHead *node;
	VNodeType type;
	node = vs_get_node_head(node_id);
	if(node == NULL)
		return;
	VSNodeStorage.nodes[node_id] = NULL;
	type = node->type;
	switch(type)
	{
		case V_NT_OBJECT :
			vs_o_destroy_node(node);
		break;
		case V_NT_GEOMETRY :
			vs_g_destroy_node(node);
		break;
		case V_NT_MATERIAL :
			vs_m_destroy_node(node);
		break;
		case V_NT_BITMAP :
			vs_b_destroy_node(node);
		break;
		case V_NT_TEXT :
			vs_t_destroy_node(node);
		break;
		case V_NT_CURVE :
			vs_c_destroy_node(node);
		break;
		case V_NT_AUDIO :
			vs_c_destroy_node(node);
		break;
		default:
			fprintf(stderr, __FILE__ " Can't handle node_destroy for type %d--not implemented", type);
		return;
	}
	count =	vs_get_subscript_count(VSNodeStorage.list[type]);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(VSNodeStorage.list[type], i);
		verse_send_node_destroy(node_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_node_index_subscribe(void *user_data, uint32 mask)
{
	unsigned int i, j, pow = 1;

	for(i = 0; i < V_NT_NUM_TYPES; i++, pow <<= 1)
	{
		if((mask & pow) != 0)
		{
			for(j = 0; j < VSNodeStorage.node_length; j++)
			{
				if(VSNodeStorage.nodes[j] != NULL && VSNodeStorage.nodes[j]->type == (VNodeType)i)
				{
					if(VSNodeStorage.nodes[j]->owner == verse_session_get_avatar())
						verse_send_node_create(VSNodeStorage.nodes[j]->id, i, VN_OWNER_MINE);
					else
						verse_send_node_create(VSNodeStorage.nodes[j]->id, i, VN_OWNER_OTHER);
				}
			}
			vs_add_new_subscriptor(VSNodeStorage.list[i]);
		}
		else
			vs_remove_subscriptor(VSNodeStorage.list[i]);
	}
}

void init_callback_node_storage(void)
{
	verse_callback_set(verse_send_node_index_subscribe,	callback_send_node_index_subscribe,  NULL);
	verse_callback_set(verse_send_node_create,		callback_send_node_create,  NULL);
	verse_callback_set(verse_send_node_destroy,		callback_send_node_destroy,  NULL);
}

#endif
