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

#define VS_TEXT_CHUNK_SIZE 4096

typedef struct {
	char			name[16];
	char			*text;
	size_t			length;
	size_t			allocated;
	VSSubscriptionList	*subscribers;
} VSTextBuffer;

typedef struct {
	VSNodeHead	head;
	char		language[512];
	VSTextBuffer	*buffer;
	unsigned int	buffer_count;
} VSNodeText;

VSNodeText * vs_t_create_node(unsigned int owner)
{
	VSNodeText *node;
	char name[48];
	unsigned int i;
	node = malloc(sizeof *node);
	vs_add_new_node(&node->head, V_NT_TEXT);
	sprintf(name, "Text_Node_%u", node->head.id);
	create_node_head(&node->head, name, owner);
	node->language[0] = 0;
	node->buffer_count = 16;
	node->buffer = malloc((sizeof *node->buffer) * node->buffer_count);
	for(i = 0; i < node->buffer_count; i++)
		node->buffer[i].name[0] = 0;

	return node;
}

void vs_t_destroy_node(VSNodeText *node)
{
	unsigned int i;
	destroy_node_head(&node->head);
	for(i = 0; i < node->buffer_count; i++)
	{
		if(node->buffer[i].name[0] != 0)
		{
			free(node->buffer[i].text);
			vs_destroy_subscription_list(node->buffer[i].subscribers);
		}
	}
	free(node->buffer);
	free(node);
}

void vs_t_subscribe(VSNodeText *node)
{
	unsigned int i;
	verse_send_t_language_set(node->head.id, node->language);
	for(i = 0; i < node->buffer_count; i++)
		if(node->buffer[i].name[0] != 0)
			verse_send_t_buffer_create(node->head.id, i, node->buffer[i].name);
}

void vs_t_unsubscribe(VSNodeText *node)
{
	unsigned int i;
	for(i = 0; i < node->buffer_count; i++)
		if(node->buffer[i].name[0] != 0)
			vs_remove_subscriptor(node->buffer[i].subscribers);
}

static void callback_send_t_language_set(void *user, VNodeID node_id, char *language)
{
	VSNodeText *node;
	unsigned int i, count;
	node = (VSNodeText *)vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;
	for(i = 0; i < 511 && language[i]; i++)
		node->language[i] = language[i];
	node->language[i] = 0;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_t_language_set(node_id, language);
	}
	vs_reset_subscript_session();

}

static void callback_send_t_buffer_create(void *user, VNodeID node_id, VBufferID buffer_id, const char *name)
{
	VSNodeText *node;
	unsigned int i, count;
	node = (VSNodeText *)vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;

	if(buffer_id >= node->buffer_count || node->buffer[buffer_id].name[0] != 0)
	{
		for(buffer_id = 0; buffer_id < node->buffer_count && node->buffer[buffer_id].name[0] != 0; buffer_id++)
			;
		if(buffer_id == node->buffer_count)
		{
			node->buffer = realloc(node->buffer, (sizeof *node->buffer) * node->buffer_count);
			for(i = node->buffer_count; i < node->buffer_count + 16; i++)
				node->buffer[i].name[0] = 0;
			node->buffer_count = i; 
		}
	}

	if(node->buffer[buffer_id].name[0] == 0)
	{
		node->buffer[buffer_id].allocated = VS_TEXT_CHUNK_SIZE;
		node->buffer[buffer_id].text = malloc(node->buffer[buffer_id].allocated);
		node->buffer[buffer_id].length = 0;
		node->buffer[buffer_id].subscribers = vs_create_subscription_list();
	}
	for(i = 0; i < 15 && name[i] != 0; i++)
		node->buffer[buffer_id].name[i] = name[i];
	node->buffer[buffer_id].name[i] = 0;

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_t_buffer_create(node_id, buffer_id, name);
	}
	vs_reset_subscript_session();
}

void callback_send_t_buffer_destroy(void *user, VNodeID node_id, VBufferID buffer_id)
{
	VSNodeText *node;
	unsigned int i, count;
	node = (VSNodeText *)vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;

	if(buffer_id >= node->buffer_count || node->buffer[buffer_id].name[0] == 0)
		return;

	node->buffer[buffer_id].name[0] = 0;
	free(node->buffer[buffer_id].text);
	vs_destroy_subscription_list(node->buffer[buffer_id].subscribers);

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_t_buffer_destroy(node_id, buffer_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_t_buffer_subscribe(void *user, VNodeID node_id, VBufferID buffer_id)
{
	VSNodeText *node;
	unsigned int i;

	node = (VSNodeText *)vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;
	if(buffer_id >= node->buffer_count || node->buffer[buffer_id].name[0] == 0)
		return;
	if(vs_add_new_subscriptor(node->buffer[buffer_id].subscribers) == 0)
		return;
	for(i = 0; i < node->buffer[buffer_id].length; i += VN_T_MAX_TEXT_CMD_SIZE)
	{	
		if(i + VN_T_MAX_TEXT_CMD_SIZE > node->buffer[buffer_id].length)
			verse_send_t_text_set(node_id, buffer_id, i, node->buffer[buffer_id].length - i, &node->buffer[buffer_id].text[i]);
		else
			verse_send_t_text_set(node_id, buffer_id, i, VN_T_MAX_TEXT_CMD_SIZE, &node->buffer[buffer_id].text[i]);
	}
}

static void callback_send_t_buffer_unsubscribe(void *user, VNodeID node_id, VBufferID buffer_id)
{
	VSNodeText *node;
	node = (VSNodeText *)vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;
	if(buffer_id >= node->buffer_count || node->buffer[buffer_id].name[0] == 0)
		return;
	vs_remove_subscriptor(node->buffer[buffer_id].subscribers);
}

static void callback_send_t_text_set(void *user, VNodeID node_id, VBufferID buffer_id, uint32 pos, uint32 length, const char *text)
{
	VSNodeText *node;
	VSTextBuffer *tb;
	unsigned int i, count, text_length;
	char *buf;

	node = (VSNodeText *) vs_get_node(node_id, V_NT_TEXT);
	if(node == NULL)
		return;
	if(buffer_id >= node->buffer_count || node->buffer[buffer_id].name[0] == 0)
		return;
	tb = &node->buffer[buffer_id];

	text_length = strlen(text);

	/* Clamp position and length of deleted region. */
	if(pos > tb->length)
		pos = tb->length;
	if(pos + length > tb->length)
		length = tb->length - pos;

	buf = tb->text;

	if(tb->length + text_length - length > tb->allocated)
	{
		buf = realloc(buf, tb->length + text_length - length + VS_TEXT_CHUNK_SIZE);
		tb->allocated = tb->length + text_length - length + VS_TEXT_CHUNK_SIZE;
	}

	if(text_length < length)		/* Insert smaller than delete? */
	{
		memmove(buf + pos + text_length, buf + pos + length, tb->length - (pos + length));
		memcpy(buf + pos, text, text_length);
	}
	else					/* Insert is larger than delete. */
	{
		memmove(buf + pos + text_length, buf + pos + length, tb->length - pos);
		memcpy(buf + pos, text, text_length);
	}

	tb->length += (int) text_length - length;
	buf[tb->length] = '\0';

	/* Buffer very much larger than content? Then shrink it. */
	if(tb->allocated > VS_TEXT_CHUNK_SIZE * 8 && tb->allocated * 2 > tb->length)
	{
		buf = realloc(buf, tb->length + VS_TEXT_CHUNK_SIZE);
		tb->allocated = tb->length + VS_TEXT_CHUNK_SIZE;
	}

	tb->text = buf;

	count =	vs_get_subscript_count(tb->subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(tb->subscribers, i);
		verse_send_t_text_set(node_id, buffer_id, pos, length, text);
	}
	vs_reset_subscript_session();
}


void vs_t_callback_init(void)
{
	verse_callback_set(verse_send_t_language_set,		callback_send_t_language_set, NULL);
	verse_callback_set(verse_send_t_buffer_create,		callback_send_t_buffer_create, NULL);
	verse_callback_set(verse_send_t_buffer_destroy,		callback_send_t_buffer_destroy, NULL);
	verse_callback_set(verse_send_t_buffer_subscribe,	callback_send_t_buffer_subscribe, NULL);
	verse_callback_set(verse_send_t_buffer_unsubscribe,	callback_send_t_buffer_unsubscribe, NULL);
	verse_callback_set(verse_send_t_text_set,		callback_send_t_text_set, NULL);
}

#endif
