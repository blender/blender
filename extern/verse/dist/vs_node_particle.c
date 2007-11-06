/*
**
*/

#include <stdio.h>
#include <stdlib.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "vs_server.h"

/*
typedef struct {
	VSNodeHead head;
} VSNodeObject;

VSNodeObject *vs_o_create_node(unsigned int owner)
{
	VSNodeObject *node;
	node = malloc(sizeof *node);
	create_node_head(&node->head, name, owner);
	vs_add_new_node(&node->head, V_NT_OBJECT);
	return node;
}

void vs_o_destroy_node(VSNodeObject *node)
{
	destroy_node_head(&node->head);
	free(node);
}

void vs_o_subscribe(VSNodeObject *node)
{
}

static void callback_send_o_unsubscribe(void *user, VNodeID node_id)
{
	VSNodeObject *node;
	node = (VSNodeObject *)vs_get_node(node_id);
	if(node == NULL)
		return;
	vs_remove_subscriptor(node->head.subscribers);
}

void vs_o_callback_init(void)
{
}
*/
#endif
