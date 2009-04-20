/*
**
*/

#include <stdio.h>
#include <stdlib.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "vs_server.h"

typedef struct {
	VNMFragmentType	type;
	VMatFrag	frag;
} VSMatFrag;

typedef struct{
	VSNodeHead	head;
	VSMatFrag	*frag;
	unsigned int	frag_count;
} VSNodeMaterial;

VSNodeMaterial * vs_m_create_node(unsigned int owner)
{
	VSNodeMaterial *node;
	char name[48];
	node = malloc(sizeof *node);
	vs_add_new_node(&node->head, V_NT_MATERIAL);
	sprintf(name, "Material_Node_%u", node->head.id);
	create_node_head(&node->head, name, owner);
	node->frag = NULL;
	node->frag_count = 0;
	return node;
}

void vs_m_destroy_node(VSNodeMaterial *node)
{
	destroy_node_head(&node->head);
	free(node->frag);
	free(node);
}

void vs_m_subscribe(VSNodeMaterial *node)
{
	uint16 i;
	for(i = 0; i < node->frag_count; i++)
		if(node->frag[i].type <= VN_M_FT_OUTPUT)
			verse_send_m_fragment_create(node->head.id, (uint16)i, (uint8)node->frag[i].type, &node->frag[i].frag);
}

void vs_m_unsubscribe(VSNodeMaterial *node)
{
}

static void callback_send_m_fragment_create(void *user, VNodeID node_id, VNMFragmentID frag_id, uint8 type, VMatFrag *fragment)
{
	unsigned int count;
	uint16 i;
	VSNodeMaterial *node;
	node = (VSNodeMaterial *)vs_get_node(node_id, V_NT_MATERIAL);
	if(node == NULL)
		return;
	if(node->frag_count + 32 < frag_id)
		frag_id = (uint16)-1;
	if(frag_id == (uint16) ~0u)
		for(frag_id = 0; frag_id < node->frag_count && node->frag[frag_id].type < VN_M_FT_OUTPUT + 1; frag_id++)
			;
	if(frag_id >= node->frag_count)
	{
		node->frag = realloc(node->frag, (sizeof *node->frag) * (node->frag_count + 32));
		for(i = node->frag_count; i < (node->frag_count + 32); i++)
			node->frag[i].type = 255;
		node->frag_count += 32;
	}	
	node->frag[frag_id].type = type;
	node->frag[frag_id].frag = *fragment;

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_m_fragment_create(node_id, frag_id, type, fragment);
	}
	vs_reset_subscript_session();
}

static void callback_send_m_fragment_destroy(void *user, VNodeID node_id, VNMFragmentID frag_id)
{
	unsigned int count, i;
	VSNodeMaterial *node;
	node = (VSNodeMaterial *)vs_get_node(node_id, V_NT_MATERIAL);
	printf("callback_send_m_fragment_destroy %p\n", node);
	if(node == NULL)
		return;
	if(node->frag_count <= frag_id || node->frag[frag_id].type > VN_M_FT_OUTPUT)
		return;
	node->frag[frag_id].type = 255;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_m_fragment_destroy(node_id, frag_id);
	}
	vs_reset_subscript_session();
}

void vs_m_callback_init(void)
{
	verse_callback_set(verse_send_m_fragment_create, callback_send_m_fragment_create, NULL);
	verse_callback_set(verse_send_m_fragment_destroy, callback_send_m_fragment_destroy, NULL);
}

#endif
