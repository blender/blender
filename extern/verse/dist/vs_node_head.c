/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_cmd_gen.h"

#if !defined(V_GENERATE_FUNC_MODE)

#include "verse.h"
#include "v_util.h"
#include "vs_server.h"

typedef struct {
	VNTag		tag;
	VNTagType	type;
	char		tag_name[16];
} VSTag;

typedef struct {
	VSTag			*tags;
	unsigned int		tag_count;
	char			group_name[16];
	VSSubscriptionList	*subscribers;
} VSTagGroup;

void create_node_head(VSNodeHead *node, const char *name, unsigned int owner)
{
	size_t	len;

	len = strlen(name) + 1;
	node->name = malloc(len);
	v_strlcpy(node->name, name, len);
	node->owner = owner;
	node->tag_groups = NULL;
	node->group_count = 0;
	node->subscribers = vs_create_subscription_list();
}

void destroy_node_head(VSNodeHead *node)
{
	unsigned int i, j;
	if(node->name != NULL)
		free(node->name);
	if(node->tag_groups != NULL)
	{
		for(i = 0; i < node->group_count; i++)
		{
			for(j = 0; j < ((VSTagGroup *)node->tag_groups)[i].tag_count; j++)
			{
				if(((VSTagGroup *)node->tag_groups)[i].tags[j].type == VN_TAG_STRING)
					free(((VSTagGroup *)node->tag_groups)[i].tags[j].tag.vstring);
				if(((VSTagGroup *)node->tag_groups)[i].tags[j].type == VN_TAG_BLOB)
					free(((VSTagGroup *)node->tag_groups)[i].tags[j].tag.vblob.blob);
			}
			if(((VSTagGroup *)node->tag_groups)[i].tags != NULL)
				free(((VSTagGroup *)node->tag_groups)[i].tags);
		}
		if(node->tag_groups != NULL)
			free(node->tag_groups);
	}
}

 void callback_send_tag_group_create(void *user, VNodeID node_id, uint16 group_id, const char *name)
{
	VSNodeHead *node;
	unsigned int count, i, j, element;

	if((node = vs_get_node_head(node_id)) == 0)
		return;
	if(name[0] == 0)
		return;

	for(i = 0; i < node->group_count; i++) /* see if a tag group with this name alredy exists*/
	{
		if(((VSTagGroup *)node->tag_groups)[i].group_name[0] != 0)
		{
			for(j = 0; name[j] == ((VSTagGroup *)node->tag_groups)[i].group_name[j] && name[j] != 0; j++);
			if(name[j] == ((VSTagGroup *)node->tag_groups)[i].group_name[j])
				return;
		}
	}
	if(group_id < node->group_count && ((VSTagGroup *)node->tag_groups)[group_id].group_name[0] != 0) /* rename existing group */
	{
		element = group_id;
	}else /* create new game group */
	{
		for(element = 0; element < node->group_count && ((VSTagGroup *)node->tag_groups)[element].group_name[0] != 0; element++);
		if(element == node->group_count)
		{
			node->tag_groups = realloc(node->tag_groups, sizeof(VSTagGroup) * (node->group_count + 16));
			for(i = node->group_count; i < node->group_count + 16U; i++)
			{
				((VSTagGroup *)node->tag_groups)[i].group_name[0] = 0;
				((VSTagGroup *)node->tag_groups)[i].tags = NULL;
				((VSTagGroup *)node->tag_groups)[i].tag_count = 0;
				((VSTagGroup *)node->tag_groups)[i].subscribers = NULL;
			}
			node->group_count += 16;
		}
		((VSTagGroup *)node->tag_groups)[element].subscribers = vs_create_subscription_list();
	}
	v_strlcpy(((VSTagGroup *)node->tag_groups)[element].group_name, name,
		  sizeof ((VSTagGroup *)node->tag_groups)[element].group_name);

	count =	vs_get_subscript_count(node->subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->subscribers, i);
		verse_send_tag_group_create(node_id, element, name);
	}
	vs_reset_subscript_session();
}

static void callback_send_tag_group_destroy(void *user, VNodeID node_id, uint16 group_id)
{
	VSNodeHead *node;
	unsigned int count, i;
	if((node = vs_get_node_head(node_id)) == 0)
		return;

	if(node->group_count <= group_id || ((VSTagGroup *)node->tag_groups)[group_id].group_name[0] == 0)
		return;
	
	vs_destroy_subscription_list(((VSTagGroup *)node->tag_groups)[group_id].subscribers);
	for(i = 0; i < ((VSTagGroup *)node->tag_groups)[group_id].tag_count; i++)
	{
		if(((VSTagGroup *)node->tag_groups)[group_id].tags[i].type == VN_TAG_STRING)
			free(((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag.vstring);
		if(((VSTagGroup *)node->tag_groups)[group_id].tags[i].type == VN_TAG_BLOB)
			free(((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag.vblob.blob);
	}
	if(((VSTagGroup *)node->tag_groups)[group_id].tags != NULL)
		free(((VSTagGroup *)node->tag_groups)[group_id].tags);
	((VSTagGroup *)node->tag_groups)[group_id].group_name[0] = 0;
	((VSTagGroup *)node->tag_groups)[group_id].tags = NULL;
	((VSTagGroup *)node->tag_groups)[group_id].tag_count = 0;

	count =	vs_get_subscript_count(node->subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->subscribers, i);
		verse_send_tag_group_destroy(node_id, group_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_tag_group_subscribe(void *user, VNodeID node_id, uint16 group_id)
{
	VSNodeHead *node;
	unsigned int i;
	if((node = vs_get_node_head(node_id)) == 0)
		return;

	if(group_id < node->group_count && ((VSTagGroup *)node->tag_groups)[group_id].group_name[0] != 0)
	{
		vs_add_new_subscriptor(((VSTagGroup *)node->tag_groups)[group_id].subscribers);
		for(i = 0; i < ((VSTagGroup *)node->tag_groups)[group_id].tag_count; i++)
		{
			if(((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name[0] != 0)
			{
				verse_send_tag_create(node_id, group_id, (uint16)i, ((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name, ((VSTagGroup *)node->tag_groups)[group_id].tags[i].type, &((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag);
			}
		}
	}
}

static void callback_send_tag_group_unsubscribe(void *user, VNodeID node_id, uint16 group_id)
{
	VSNodeHead *node;
	if((node = vs_get_node_head(node_id)) == 0)
		return;
	if(group_id < node->group_count && ((VSTagGroup *)node->tag_groups)[group_id].group_name[0] != 0)
		vs_remove_subscriptor(((VSTagGroup *)node->tag_groups)[group_id].subscribers);
}
 
static void callback_send_tag_create(void *user, VNodeID node_id, uint16 group_id, uint16 tag_id, char *name, uint8 type, void *tag)
{
	VSNodeHead *node;
	VSTag *t = NULL;
	unsigned int i, count;

	if((node = vs_get_node_head(node_id)) == 0)
		return;
	if(group_id >= node->group_count || ((VSTagGroup *)node->tag_groups)[group_id].group_name[0] == 0)
		return;

/*	for(i = 0; i < ((VSTagGroup *)node->tag_groups)[group_id].tag_count; i++)
	{
		if(((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name != NULL && i != tag_id)
		{
			for(j = 0; name[j] == ((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name[j] && name[j] != 0; j++);
			if(name[j] == ((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name[j])
				return;
		}
	}*/
	if(tag_id < ((VSTagGroup *)node->tag_groups)[group_id].tag_count && ((VSTagGroup *)node->tag_groups)[group_id].tags[tag_id].tag_name[0] != 0)
		;
	else
	{
		for(tag_id = 0; tag_id < ((VSTagGroup *)node->tag_groups)[group_id].tag_count && ((VSTagGroup *)node->tag_groups)[group_id].tags[tag_id].tag_name[0] != 0; tag_id++)
			;
		if(tag_id == ((VSTagGroup *)node->tag_groups)[group_id].tag_count)
		{
			((VSTagGroup *)node->tag_groups)[group_id].tags = realloc(((VSTagGroup *)node->tag_groups)[group_id].tags, sizeof(VSTag) * (((VSTagGroup *)node->tag_groups)[group_id].tag_count + 16));
			for(i = tag_id; i < ((VSTagGroup *)node->tag_groups)[group_id].tag_count + 16; i++)
				((VSTagGroup *)node->tag_groups)[group_id].tags[i].tag_name[0] = 0;
			((VSTagGroup *)node->tag_groups)[group_id].tag_count += 16;
		}
	}
	t = &((VSTagGroup *)node->tag_groups)[group_id].tags[tag_id];
	if(t->tag_name[0] != '\0')	/* Old tag being re-set? */
	{
		if(t->type == VN_TAG_STRING)
			free(t->tag.vstring);
		else if(t->type == VN_TAG_BLOB)
			free(t->tag.vblob.blob);
	}
	t->type = type;
	v_strlcpy(t->tag_name, name, sizeof t->tag_name);
	switch(type)
	{
		case VN_TAG_BOOLEAN :
			t->tag.vboolean = ((VNTag *)tag)->vboolean;
		break;
		case VN_TAG_UINT32 :
			t->tag.vuint32 = ((VNTag *)tag)->vuint32;
		break;
		case VN_TAG_REAL64 :
			t->tag.vreal64 = ((VNTag *)tag)->vreal64;
		break;
		case VN_TAG_STRING :
			i = strlen(((VNTag *) tag)->vstring);
			t->tag.vstring = malloc(i + 1);
			strcpy(t->tag.vstring, ((VNTag *) tag)->vstring);
		break;
		case VN_TAG_REAL64_VEC3 :
			t->tag.vreal64_vec3[0] = ((VNTag *)tag)->vreal64_vec3[0];
			t->tag.vreal64_vec3[1] = ((VNTag *)tag)->vreal64_vec3[1];
			t->tag.vreal64_vec3[2] = ((VNTag *)tag)->vreal64_vec3[2];
		break;
		case VN_TAG_LINK :
			t->tag.vlink = ((VNTag *)tag)->vlink;
		break;
		case VN_TAG_ANIMATION :
			t->tag.vanimation.curve = ((VNTag *)tag)->vanimation.curve;
			t->tag.vanimation.start = ((VNTag *)tag)->vanimation.start;
			t->tag.vanimation.end = ((VNTag *)tag)->vanimation.end;
		break;
		case VN_TAG_BLOB :
			t->tag.vblob.blob = malloc(((VNTag *)tag)->vblob.size);
			t->tag.vblob.size = ((VNTag *)tag)->vblob.size;
			memcpy(t->tag.vblob.blob, ((VNTag *)tag)->vblob.blob, ((VNTag *)tag)->vblob.size);
		break;
	}

	count =	vs_get_subscript_count(((VSTagGroup *) node->tag_groups)[group_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(((VSTagGroup *) node->tag_groups)[group_id].subscribers, i);
		verse_send_tag_create(node_id, group_id, tag_id, name, type, tag);
	}
	vs_reset_subscript_session();
}

static void callback_send_tag_destroy(void *user, VNodeID node_id, uint16 group_id, uint16 tag_id)
{
	VSNodeHead *node;
	unsigned int count, i;
	if((node = vs_get_node_head(node_id)) == 0)
		return;

	count =	vs_get_subscript_count(((VSTagGroup *) node->tag_groups)[group_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(((VSTagGroup *) node->tag_groups)[group_id].subscribers, i);
		verse_send_tag_destroy(node_id, group_id, tag_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_node_name_set(void *user, VNodeID node_id, char *name)
{
	VSNodeHead *node;
	unsigned int count, i;
	size_t	len;

	if((node = vs_get_node_head(node_id)) == 0)
		return;
	len = strlen(name);
	if(len == 0)
		return;
	free(node->name);
	len++;
	node->name = malloc(len);
	v_strlcpy(node->name, name, len);
	count =	vs_get_subscript_count(node->subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->subscribers, i);
		verse_send_node_name_set(node_id, name);
	}
	vs_reset_subscript_session();
}

extern void vs_o_subscribe(VSNodeHead *node);
extern void vs_g_subscribe(VSNodeHead *node);
extern void vs_m_subscribe(VSNodeHead *node);
extern void vs_b_subscribe(VSNodeHead *node);
extern void vs_t_subscribe(VSNodeHead *node);
extern void vs_c_subscribe(VSNodeHead *node);
extern void vs_a_subscribe(VSNodeHead *node);

static void callback_send_node_subscribe(void *user, VNodeID node_id)
{
	unsigned int i;
	VSNodeHead *node;

	if((node = vs_get_node_head(node_id)) == NULL)
		return;
	switch(node->type)
	{
		case V_NT_OBJECT :
			vs_o_subscribe(node);
			break;
		case V_NT_GEOMETRY :
			vs_g_subscribe(node);
			break;
		case V_NT_MATERIAL :
			vs_m_subscribe(node);
			break;
		case V_NT_BITMAP :
			vs_b_subscribe(node);
			break;
		case V_NT_TEXT:
			vs_t_subscribe(node);
			break;
		case V_NT_CURVE:
			vs_c_subscribe(node);
			break;
		case V_NT_AUDIO:
			vs_a_subscribe(node);
			break;
		default:
			fprintf(stderr, "Not subscribing to node type %d\n", node->type);
	}
	verse_send_node_name_set(node->id, node->name);
	for(i = 0; i < node->group_count; i++)
		if(((VSTagGroup *)node->tag_groups)[i].group_name[0] != 0)
			verse_send_tag_group_create(node->id, (uint16)i, ((VSTagGroup *)node->tag_groups)[i].group_name);
	vs_add_new_subscriptor(node->subscribers);
}

extern void vs_o_unsubscribe(VSNodeHead *node);
extern void vs_g_unsubscribe(VSNodeHead *node);
extern void vs_m_unsubscribe(VSNodeHead *node);
extern void vs_b_unsubscribe(VSNodeHead *node);
extern void vs_t_unsubscribe(VSNodeHead *node);
extern void vs_c_unsubscribe(VSNodeHead *node);
extern void vs_a_unsubscribe(VSNodeHead *node);

static void callback_send_node_unsubscribe(void *user, VNodeID node_id)
{
	VSNodeHead *node;

	if((node = vs_get_node_head(node_id)) == NULL)
		return;
	vs_remove_subscriptor(node->subscribers);

	switch(node->type)
	{
		case V_NT_OBJECT :
			vs_o_unsubscribe(node);
			break;
		case V_NT_GEOMETRY :
			vs_g_unsubscribe(node);
			break;
		case V_NT_MATERIAL :
			vs_m_unsubscribe(node);
			break;
		case V_NT_BITMAP :
			vs_b_unsubscribe(node);
			break;
		case V_NT_TEXT:
			vs_t_unsubscribe(node);
			break;
		case V_NT_CURVE:
			vs_c_unsubscribe(node);
			break;
		case V_NT_AUDIO:
			vs_a_unsubscribe(node);
			break;
		default:
			fprintf(stderr, "Not unsubscribing from node type %d\n", node->type);
	}
}

void vs_h_callback_init(void)
{
	verse_callback_set(verse_send_tag_group_create, callback_send_tag_group_create, NULL);
	verse_callback_set(verse_send_tag_group_destroy, callback_send_tag_group_destroy, NULL);
	verse_callback_set(verse_send_tag_group_subscribe, callback_send_tag_group_subscribe, NULL);
	verse_callback_set(verse_send_tag_group_unsubscribe, callback_send_tag_group_unsubscribe, NULL);
	verse_callback_set(verse_send_tag_create, callback_send_tag_create, NULL);
	verse_callback_set(verse_send_tag_destroy, callback_send_tag_destroy, NULL);
	verse_callback_set(verse_send_node_name_set, callback_send_node_name_set, NULL);
	verse_callback_set(verse_send_node_subscribe, callback_send_node_subscribe, NULL);
	verse_callback_set(verse_send_node_unsubscribe, callback_send_node_unsubscribe, NULL);
}

#endif
