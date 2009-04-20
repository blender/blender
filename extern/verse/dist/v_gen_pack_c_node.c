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

void verse_send_c_curve_create(VNodeID node_id, VLayerID curve_id, const char *name, uint8 dimensions)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 128);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_c_curve_create(node_id = %u curve_id = %u name = %s dimensions = %u );\n", node_id, curve_id, name, dimensions);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], curve_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], name, 16);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], dimensions);
	if(node_id == (uint32) ~0u || curve_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_c_curve_destroy(VNodeID node_id, VLayerID curve_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 128);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_c_curve_destroy(node_id = %u curve_id = %u );\n", node_id, curve_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], curve_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 16);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], -1);
	if(node_id == (uint32) ~0u || curve_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_c_curve_create(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_c_curve_create)(void *user_data, VNodeID node_id, VLayerID curve_id, const char *name, uint8 dimensions);
	VNodeID node_id;
	VLayerID curve_id;
	char name[16];
	uint8 dimensions;
	
	func_c_curve_create = v_fs_get_user_func(128);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &curve_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 16, buffer_length - buffer_pos);
	if(buffer_length < 1 + buffer_pos)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &dimensions);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(name[0] == 0)
		printf("receive: verse_send_c_curve_destroy(node_id = %u curve_id = %u ); callback = %p\n", node_id, curve_id, v_fs_get_alias_user_func(128));
	else
		printf("receive: verse_send_c_curve_create(node_id = %u curve_id = %u name = %s dimensions = %u ); callback = %p\n", node_id, curve_id, name, dimensions, v_fs_get_user_func(128));
#endif
	if(name[0] == 0)
	{
		void (* alias_c_curve_destroy)(void *user_data, VNodeID node_id, VLayerID curve_id);
		alias_c_curve_destroy = v_fs_get_alias_user_func(128);
		if(alias_c_curve_destroy != NULL)
			alias_c_curve_destroy(v_fs_get_alias_user_data(128), node_id, curve_id);
		return buffer_pos;
	}
	if(func_c_curve_create != NULL)
		func_c_curve_create(v_fs_get_user_data(128), node_id, curve_id, name, dimensions);

	return buffer_pos;
}

void verse_send_c_curve_subscribe(VNodeID node_id, VLayerID curve_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 129);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_c_curve_subscribe(node_id = %u curve_id = %u );\n", node_id, curve_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], curve_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);
	if(node_id == (uint32) ~0u || curve_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_c_curve_unsubscribe(VNodeID node_id, VLayerID curve_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 129);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_c_curve_unsubscribe(node_id = %u curve_id = %u );\n", node_id, curve_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], curve_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);
	if(node_id == (uint32) ~0u || curve_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_c_curve_subscribe(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_c_curve_subscribe)(void *user_data, VNodeID node_id, VLayerID curve_id);
	VNodeID node_id;
	VLayerID curve_id;
	uint8	alias_bool;

	func_c_curve_subscribe = v_fs_get_user_func(129);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &curve_id);
	if(buffer_length < buffer_pos + 1)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(!alias_bool)
		printf("receive: verse_send_c_curve_unsubscribe(node_id = %u curve_id = %u ); callback = %p\n", node_id, curve_id, v_fs_get_alias_user_func(129));
	else
		printf("receive: verse_send_c_curve_subscribe(node_id = %u curve_id = %u ); callback = %p\n", node_id, curve_id, v_fs_get_user_func(129));
#endif
	if(!alias_bool)
	{
		void (* alias_c_curve_unsubscribe)(void *user_data, VNodeID node_id, VLayerID curve_id);
		alias_c_curve_unsubscribe = v_fs_get_alias_user_func(129);
		if(alias_c_curve_unsubscribe != NULL)
			alias_c_curve_unsubscribe(v_fs_get_alias_user_data(129), node_id, curve_id);
		return buffer_pos;
	}
	if(func_c_curve_subscribe != NULL)
		func_c_curve_subscribe(v_fs_get_user_data(129), node_id, curve_id);

	return buffer_pos;
}

#endif

