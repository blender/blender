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
	void				**data;
	unsigned int		length;
	char				name[16];
	VNABlockType		type;
	real64			frequency;
	VSSubscriptionList  *subscribers;
} VSNLayer;

typedef struct {
	char				name[16];
	VSSubscriptionList  *subscribers;
} VSNStream;

typedef struct{
	VSNodeHead		head;
	VSNLayer		*buffers;
	unsigned int	buffer_count;
	VSNStream		*streams;
	unsigned int	stream_count;
} VSNodeAudio;

VSNodeAudio * vs_a_create_node(unsigned int owner)
{
	VSNodeAudio *node;
	char name[48];
	unsigned int i;

	node = malloc(sizeof *node);
	vs_add_new_node(&node->head, V_NT_AUDIO);
	sprintf(name, "Audio_Node_%u", node->head.id);
	create_node_head(&node->head, name, owner);
	node->buffer_count = 16;
	node->buffers = malloc((sizeof *node->buffers) * node->buffer_count);
	for(i = 0; i < node->buffer_count; i++)
		node->buffers[i].name[0] = 0;
	node->stream_count = 16;
	node->streams = malloc((sizeof *node->streams) * node->stream_count);
	for(i = 0; i < node->stream_count; i++)
		node->streams[i].name[0] = 0;

	return node;
}

void vs_a_destroy_node(VSNodeAudio *node)
{
	unsigned int i, j;
	destroy_node_head(&node->head);
	
	for(i = 0; i < node->buffer_count; i++)
	{
		if(node->buffers[i].name[0] != 0)
		{
			for(j = 0; j < node->buffers[i].length; j++)
				if(node->buffers[i].data[j] != NULL)
					free(node->buffers[i].data[j]);
			free(node->buffers[i].data);
		}
	}
	free(node->buffers);
	free(node->streams);
	free(node);
}

void vs_a_subscribe(VSNodeAudio *node)
{
	unsigned int i;
	if(node == NULL)
		return;
	for(i = 0; i < node->buffer_count; i++)
		if(node->buffers[i].name[0] != 0)
			verse_send_a_buffer_create(node->head.id, i, node->buffers[i].name, node->buffers[i].type,
						  node->buffers[i].frequency);
	for(i = 0; i < node->stream_count; i++)
		if(node->streams[i].name[0] != 0)
			verse_send_a_stream_create(node->head.id, i, node->streams[i].name);
}

void vs_a_unsubscribe(VSNodeAudio *node)
{
	unsigned int i;
	for(i = 0; i < node->buffer_count; i++)
		if(node->buffers[i].name[0] != 0)	
			vs_remove_subscriptor(node->buffers[i].subscribers);
	for(i = 0; i < node->stream_count; i++)
		if(node->streams[i].name[0] != 0)	
			vs_remove_subscriptor(node->streams[i].subscribers);
}

static void callback_send_a_stream_create(void *user, VNodeID node_id, VLayerID stream_id, const char *name)
{
	VSNodeAudio *node;
	unsigned int i, j, count;

	node = (VSNodeAudio *) vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;

	for(i = 0; i < node->stream_count; i++)
	{
		if(stream_id != i)
		{
			for(j = 0; name[j] == node->streams[i].name[j] && name[j] != 0; j++);
			if(name[j] == node->streams[i].name[j])
				return;
		}
	}
	if(stream_id >= node->stream_count || node->streams[stream_id].name[0] == 0)
	{
		for(stream_id = 0; stream_id < node->stream_count && node->streams[stream_id].name[0] != 0; stream_id++);
		if(stream_id == node->stream_count)
		{
			stream_id = node->stream_count;
			node->stream_count += 16;
			node->streams = realloc(node->streams, (sizeof *node->streams) * node->stream_count);
			for(i = stream_id; i < node->stream_count; i++)
				node->streams[i].name[0] = 0;		
		}
		node->streams[stream_id].subscribers = vs_create_subscription_list();
	}
	for(i = 0; name[i] != 0 && i < 15; i++)
		node->streams[stream_id].name[i] = name[i];
	node->streams[stream_id].name[i] = 0;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_a_stream_create(node_id, stream_id, name);
	}
	vs_reset_subscript_session();
}

static void callback_send_a_stream_destroy(void *user, VNodeID node_id, VLayerID stream_id)
{
	VSNodeAudio *node;
	unsigned int i, count;

	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL || stream_id >= node->stream_count || node->streams[stream_id].name[0] == 0)
		return;
	vs_remove_subscriptor(node->streams[stream_id].subscribers);
	node->streams[stream_id].name[0] = 0;
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_a_stream_destroy(node_id, stream_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_a_buffer_create(void *user, VNodeID node_id, VBufferID buffer_id, const char *name,
					 VNABlockType type, real64 frequency)
{
	VSNodeAudio *node;
	unsigned int i, j, count;

	if(frequency < 0.0)
		return;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;

	for(i = 0; i < node->buffer_count; i++)
	{
		if(buffer_id != i)
		{
			for(j = 0; name[j] == node->buffers[i].name[j] && name[j] != 0; j++);
			if(name[j] == node->buffers[i].name[j])
				return;
		}
	}

	if(buffer_id < node->buffer_count && node->buffers[buffer_id].name[0] != 0 && type != node->buffers[buffer_id].type)
	{
		free(node->buffers[buffer_id].data);
		vs_destroy_subscription_list(node->buffers[buffer_id].subscribers);
		node->buffers[buffer_id].name[0] = 0;
	}

	if(buffer_id >= node->buffer_count || node->buffers[buffer_id].name[0] == 0)
	{
		for(buffer_id = 0; buffer_id < node->buffer_count && node->buffers[buffer_id].name[0] != 0; buffer_id++);
		if(buffer_id == node->buffer_count)
		{
			buffer_id = node->buffer_count;
			node->buffer_count += 16;
			node->buffers = realloc(node->buffers, (sizeof *node->buffers) * node->buffer_count);
			for(i = buffer_id; i < node->buffer_count; i++)
				node->buffers[i].name[0] = 0;		
		}
		node->buffers[buffer_id].subscribers = vs_create_subscription_list();
		node->buffers[buffer_id].type = type;
		node->buffers[buffer_id].frequency = frequency;
		node->buffers[buffer_id].length = 64;
		node->buffers[buffer_id].data = malloc(sizeof(*node->buffers[buffer_id].data) * node->buffers[buffer_id].length);
		for(i = 0; i < node->buffers[buffer_id].length; i++)
			node->buffers[buffer_id].data[i] = NULL;	
	}
	for(i = 0; name[i] != 0 && i < 15; i++)
		node->buffers[buffer_id].name[i] = name[i];
	node->buffers[buffer_id].name[i] = 0;

	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_a_buffer_create(node_id, buffer_id, name, type, frequency);
	}
	vs_reset_subscript_session();
}

static void callback_send_a_buffer_destroy(void *user, VNodeID node_id, VBufferID buffer_id)
{
	VSNodeAudio *node;
	unsigned int i, count;

	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL || buffer_id >= node->buffer_count || node->buffers[buffer_id].name[0] == 0)
		return;
	vs_remove_subscriptor(node->buffers[buffer_id].subscribers);
	node->buffers[buffer_id].name[0] = 0;
	free(node->buffers[buffer_id].data);
	count =	vs_get_subscript_count(node->head.subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->head.subscribers, i);
		verse_send_a_buffer_destroy(node_id, buffer_id);
	}
	vs_reset_subscript_session();
}

static void callback_send_a_buffer_subscribe(void *user, VNodeID node_id, VLayerID buffer_id)
{
	VSNodeAudio *node;
	unsigned int i;

	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->buffer_count <= buffer_id)
		return;
	if(node->buffers[buffer_id].name[0] == 0)
		return;
	vs_add_new_subscriptor(node->buffers[buffer_id].subscribers);
	for(i = 0; i < node->buffers[buffer_id].length; i++)
	{
		if(node->buffers[buffer_id].data[i] != NULL)
			verse_send_a_block_set(node_id, buffer_id, i, node->buffers[buffer_id].type, node->buffers[buffer_id].data[i]);
	}
}

static void callback_send_a_buffer_unsubscribe(void *user, VNodeID node_id, VLayerID buffer_id)
{
	VSNodeAudio *node;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->buffer_count <= buffer_id)
		return;
	if(node->buffers[buffer_id].name[0] == 0)
		return;
	vs_remove_subscriptor(node->buffers[buffer_id].subscribers);
}

static void callback_send_a_block_set(void *user, VNodeID node_id, VLayerID buffer_id, uint32 block_index,
				      VNABlockType type, const VNABlock *data)
{
	static const size_t	blocksize[] = {
		VN_A_BLOCK_SIZE_INT8   * sizeof (int8),
		VN_A_BLOCK_SIZE_INT16  * sizeof (int16),
		VN_A_BLOCK_SIZE_INT24  * 3 * sizeof (int8),
		VN_A_BLOCK_SIZE_INT32  * sizeof (int32),
		VN_A_BLOCK_SIZE_REAL32 * sizeof (real32),
		VN_A_BLOCK_SIZE_REAL64 * sizeof (real64) 
	};
	VSNodeAudio *node;
	unsigned int i, count;

	if(type > VN_A_BLOCK_REAL64)	/* Protect blocksize array. */
		return;

	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->buffers[buffer_id].name[0] == 0)
		return;
	if(type != node->buffers[buffer_id].type)		/* Disregard attempts to set data of wrong type. */
		return;
	if(block_index > node->buffers[buffer_id].length)
	{
		node->buffers[buffer_id].data = realloc(node->buffers[buffer_id].data,
						      (sizeof *node->buffers[buffer_id].data) * (block_index + 64));
		for(i = node->buffers[buffer_id].length; i < block_index + 64; i++)
			node->buffers[buffer_id].data[i] = NULL;
		node->buffers[buffer_id].length = block_index + 64;
	}

	if(node->buffers[buffer_id].data[block_index] == NULL)
		node->buffers[buffer_id].data[block_index] = malloc(blocksize[type]);
	if(node->buffers[buffer_id].data[block_index] != NULL)
	{
		memcpy(node->buffers[buffer_id].data[block_index], data, blocksize[type]);
		count =	vs_get_subscript_count(node->buffers[buffer_id].subscribers);
		for(i = 0; i < count; i++)
		{
			vs_set_subscript_session(node->buffers[buffer_id].subscribers, i);
			verse_send_a_block_set(node_id, buffer_id, block_index, type, data);
		}
		vs_reset_subscript_session();
	}
}

static void callback_send_a_block_clear(void *user, VNodeID node_id, VLayerID buffer_id, uint32 id)
{
	VSNodeAudio *node;
	unsigned int i, count;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->buffer_count <= buffer_id)
		return;
	if(node->buffers[buffer_id].name[0] == 0)
		return;
	if(id >= node->buffers[buffer_id].length)
		return;
	if(node->buffers[buffer_id].data[id] == NULL)
		return;
	free(node->buffers[buffer_id].data[id]);
	node->buffers[buffer_id].data[id] = NULL;
	count =	vs_get_subscript_count(node->buffers[buffer_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->buffers[buffer_id].subscribers, i);
		verse_send_a_block_clear(node_id, buffer_id, id);
	}
	vs_reset_subscript_session();
}

static void callback_send_a_stream_subscribe(void *user, VNodeID node_id, VLayerID stream_id)
{
	VSNodeAudio *node;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->stream_count <= stream_id)
		return;
	if(node->streams[stream_id].name[0] == 0)
		return;
	vs_add_new_subscriptor(node->streams[stream_id].subscribers);
}

static void callback_send_a_stream_unsubscribe(void *user, VNodeID node_id, VLayerID stream_id)
{
	VSNodeAudio *node;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->stream_count <= stream_id)
		return;
	if(node->streams[stream_id].name[0] == 0)
		return;
	vs_remove_subscriptor(node->streams[stream_id].subscribers);
}

static void callback_send_a_stream(void *user, VNodeID node_id, VLayerID stream_id, uint32 time_s, uint32 time_f,
				   VNABlockType type, real64 frequency, const VNABlock *data)
{
	VSNodeAudio *node;
	unsigned int i, count;

	if(frequency < 0)
		return;
	node = (VSNodeAudio *)vs_get_node(node_id, V_NT_AUDIO);
	if(node == NULL)
		return;
	if(node->stream_count <= stream_id)
		return;
	if(node->streams[stream_id].name[0] == 0)
		return;
	count =	vs_get_subscript_count(node->streams[stream_id].subscribers);
	for(i = 0; i < count; i++)
	{
		vs_set_subscript_session(node->streams[stream_id].subscribers, i);
		verse_send_a_stream(node_id, stream_id, time_s, time_f, type, frequency, data);
	}
	vs_reset_subscript_session();
}

void vs_a_callback_init(void)
{
	verse_callback_set(verse_send_a_buffer_create,		callback_send_a_buffer_create,		NULL);
	verse_callback_set(verse_send_a_buffer_destroy,		callback_send_a_buffer_destroy,		NULL);
	verse_callback_set(verse_send_a_buffer_subscribe,	callback_send_a_buffer_subscribe,	NULL);
	verse_callback_set(verse_send_a_buffer_unsubscribe,	callback_send_a_buffer_unsubscribe,	NULL);
	verse_callback_set(verse_send_a_block_set,		callback_send_a_block_set,		NULL);
	verse_callback_set(verse_send_a_block_clear,		callback_send_a_block_clear,		NULL);
	verse_callback_set(verse_send_a_stream_create,		callback_send_a_stream_create,		NULL);
	verse_callback_set(verse_send_a_stream_destroy,		callback_send_a_stream_destroy,		NULL);
	verse_callback_set(verse_send_a_stream_subscribe,	callback_send_a_stream_subscribe,	NULL);
	verse_callback_set(verse_send_a_stream_unsubscribe,	callback_send_a_stream_unsubscribe,	NULL);
	verse_callback_set(verse_send_a_stream,			callback_send_a_stream,			NULL);
}

#endif
