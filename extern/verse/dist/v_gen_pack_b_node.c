/*
** This is automatically generated source code -- do not edit.
** Changes are affected either by editing the corresponding protocol
** definition file (v_cmd_def_X.c where X=node type), or by editing
** the code generator itself, in v_cmd_gen.c.
*/

#include <stdlib.h>
#include <stdio.h>

#include "v_cmd_gen.h"
#if !defined(V_GENERATE_FUNC_MODE)
#include "verse.h"
#include "v_cmd_buf.h"
#include "v_network_out_que.h"
#include "v_network.h"
#include "v_connection.h"
#include "v_util.h"

void verse_send_b_dimensions_set(VNodeID node_id, uint16 width, uint16 height, uint16 depth)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_20);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 80);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_b_dimensions_set(node_id = %u width = %u height = %u depth = %u );\n", node_id, width, height, depth);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], width);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], height);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], depth);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_b_dimensions_set(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_b_dimensions_set)(void *user_data, VNodeID node_id, uint16 width, uint16 height, uint16 depth);
	VNodeID node_id;
	uint16 width;
	uint16 height;
	uint16 depth;
	
	func_b_dimensions_set = v_fs_get_user_func(80);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &width);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &height);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &depth);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_b_dimensions_set(node_id = %u width = %u height = %u depth = %u ); callback = %p\n", node_id, width, height, depth, v_fs_get_user_func(80));
#endif
	if(func_b_dimensions_set != NULL)
		func_b_dimensions_set(v_fs_get_user_data(80), node_id, width, height, depth);

	return buffer_pos;
}

void verse_send_b_layer_create(VNodeID node_id, VLayerID layer_id, const char *name, VNBLayerType type)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 81);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_b_layer_create(node_id = %u layer_id = %u name = %s type = %u );\n", node_id, layer_id, name, type);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], layer_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], name, 16);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)type);
	if(node_id == (uint32) ~0u || layer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_b_layer_destroy(VNodeID node_id, VLayerID layer_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 81);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_b_layer_destroy(node_id = %u layer_id = %u );\n", node_id, layer_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], layer_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 16);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)-1);
	if(node_id == (uint32) ~0u || layer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_b_layer_create(const char *buf, size_t buffer_length)
{
	uint8 enum_temp;
	unsigned int buffer_pos = 0;
	void (* func_b_layer_create)(void *user_data, VNodeID node_id, VLayerID layer_id, const char *name, VNBLayerType type);
	VNodeID node_id;
	VLayerID layer_id;
	char name[16];
	VNBLayerType type;
	
	func_b_layer_create = v_fs_get_user_func(81);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &layer_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 16, buffer_length - buffer_pos);
	if(buffer_length < 1 + buffer_pos)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &enum_temp);
	type = (VNBLayerType)enum_temp;
#if defined V_PRINT_RECEIVE_COMMANDS
	if(name[0] == 0)
		printf("receive: verse_send_b_layer_destroy(node_id = %u layer_id = %u ); callback = %p\n", node_id, layer_id, v_fs_get_alias_user_func(81));
	else
		printf("receive: verse_send_b_layer_create(node_id = %u layer_id = %u name = %s type = %u ); callback = %p\n", node_id, layer_id, name, type, v_fs_get_user_func(81));
#endif
	if(name[0] == 0)
	{
		void (* alias_b_layer_destroy)(void *user_data, VNodeID node_id, VLayerID layer_id);
		alias_b_layer_destroy = v_fs_get_alias_user_func(81);
		if(alias_b_layer_destroy != NULL)
			alias_b_layer_destroy(v_fs_get_alias_user_data(81), node_id, layer_id);
		return buffer_pos;
	}
	if(func_b_layer_create != NULL)
		func_b_layer_create(v_fs_get_user_data(81), node_id, layer_id, name, (VNBLayerType) type);

	return buffer_pos;
}

void verse_send_b_layer_subscribe(VNodeID node_id, VLayerID layer_id, uint8 level)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 82);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_b_layer_subscribe(node_id = %u layer_id = %u level = %u );\n", node_id, layer_id, level);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], layer_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], level);
	if(node_id == (uint32) ~0u || layer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_b_layer_unsubscribe(VNodeID node_id, VLayerID layer_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 82);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_b_layer_unsubscribe(node_id = %u layer_id = %u );\n", node_id, layer_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], layer_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], -1);
	if(node_id == (uint32) ~0u || layer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_b_layer_subscribe(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_b_layer_subscribe)(void *user_data, VNodeID node_id, VLayerID layer_id, uint8 level);
	VNodeID node_id;
	VLayerID layer_id;
	uint8 level;
	
	func_b_layer_subscribe = v_fs_get_user_func(82);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &layer_id);
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &level);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(level == 255)
		printf("receive: verse_send_b_layer_unsubscribe(node_id = %u layer_id = %u ); callback = %p\n", node_id, layer_id, v_fs_get_alias_user_func(82));
	else
		printf("receive: verse_send_b_layer_subscribe(node_id = %u layer_id = %u level = %u ); callback = %p\n", node_id, layer_id, level, v_fs_get_user_func(82));
#endif
	if(level == 255)
	{
		void (* alias_b_layer_unsubscribe)(void *user_data, VNodeID node_id, VLayerID layer_id);
		alias_b_layer_unsubscribe = v_fs_get_alias_user_func(82);
		if(alias_b_layer_unsubscribe != NULL)
			alias_b_layer_unsubscribe(v_fs_get_alias_user_data(82), node_id, layer_id);
		return buffer_pos;
	}
	if(func_b_layer_subscribe != NULL)
		func_b_layer_subscribe(v_fs_get_user_data(82), node_id, layer_id, level);

	return buffer_pos;
}

#endif

