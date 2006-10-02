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

void verse_send_t_language_set(VNodeID node_id, const char *language)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 96);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_t_language_set(node_id = %u language = %s );\n", node_id, language);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], language, 512);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_t_language_set(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_t_language_set)(void *user_data, VNodeID node_id, const char *language);
	VNodeID node_id;
	char language[512];
	
	func_t_language_set = v_fs_get_user_func(96);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], language, 512, buffer_length - buffer_pos);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_t_language_set(node_id = %u language = %s ); callback = %p\n", node_id, language, v_fs_get_user_func(96));
#endif
	if(func_t_language_set != NULL)
		func_t_language_set(v_fs_get_user_data(96), node_id, language);

	return buffer_pos;
}

void verse_send_t_buffer_create(VNodeID node_id, VBufferID buffer_id, const char *name)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 97);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_t_buffer_create(node_id = %u buffer_id = %u name = %s );\n", node_id, buffer_id, name);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], buffer_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], name, 16);
	if(node_id == (uint32) ~0u || buffer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_t_buffer_destroy(VNodeID node_id, VBufferID buffer_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 97);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_t_buffer_destroy(node_id = %u buffer_id = %u );\n", node_id, buffer_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], buffer_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 16);
	if(node_id == (uint32) ~0u || buffer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_t_buffer_create(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_t_buffer_create)(void *user_data, VNodeID node_id, VBufferID buffer_id, const char *name);
	VNodeID node_id;
	VBufferID buffer_id;
	char name[16];
	
	func_t_buffer_create = v_fs_get_user_func(97);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &buffer_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 16, buffer_length - buffer_pos);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(name[0] == 0)
		printf("receive: verse_send_t_buffer_destroy(node_id = %u buffer_id = %u ); callback = %p\n", node_id, buffer_id, v_fs_get_alias_user_func(97));
	else
		printf("receive: verse_send_t_buffer_create(node_id = %u buffer_id = %u name = %s ); callback = %p\n", node_id, buffer_id, name, v_fs_get_user_func(97));
#endif
	if(name[0] == 0)
	{
		void (* alias_t_buffer_destroy)(void *user_data, VNodeID node_id, VBufferID buffer_id);
		alias_t_buffer_destroy = v_fs_get_alias_user_func(97);
		if(alias_t_buffer_destroy != NULL)
			alias_t_buffer_destroy(v_fs_get_alias_user_data(97), node_id, buffer_id);
		return buffer_pos;
	}
	if(func_t_buffer_create != NULL)
		func_t_buffer_create(v_fs_get_user_data(97), node_id, buffer_id, name);

	return buffer_pos;
}

void verse_send_t_buffer_subscribe(VNodeID node_id, VBufferID buffer_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 98);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_t_buffer_subscribe(node_id = %u buffer_id = %u );\n", node_id, buffer_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], buffer_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);
	if(node_id == (uint32) ~0u || buffer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_t_buffer_unsubscribe(VNodeID node_id, VBufferID buffer_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 98);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_t_buffer_unsubscribe(node_id = %u buffer_id = %u );\n", node_id, buffer_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], buffer_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);
	if(node_id == (uint32) ~0u || buffer_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_t_buffer_subscribe(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_t_buffer_subscribe)(void *user_data, VNodeID node_id, VBufferID buffer_id);
	VNodeID node_id;
	VBufferID buffer_id;
	uint8	alias_bool;

	func_t_buffer_subscribe = v_fs_get_user_func(98);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &buffer_id);
	if(buffer_length < buffer_pos + 1)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(!alias_bool)
		printf("receive: verse_send_t_buffer_unsubscribe(node_id = %u buffer_id = %u ); callback = %p\n", node_id, buffer_id, v_fs_get_alias_user_func(98));
	else
		printf("receive: verse_send_t_buffer_subscribe(node_id = %u buffer_id = %u ); callback = %p\n", node_id, buffer_id, v_fs_get_user_func(98));
#endif
	if(!alias_bool)
	{
		void (* alias_t_buffer_unsubscribe)(void *user_data, VNodeID node_id, VBufferID buffer_id);
		alias_t_buffer_unsubscribe = v_fs_get_alias_user_func(98);
		if(alias_t_buffer_unsubscribe != NULL)
			alias_t_buffer_unsubscribe(v_fs_get_alias_user_data(98), node_id, buffer_id);
		return buffer_pos;
	}
	if(func_t_buffer_subscribe != NULL)
		func_t_buffer_subscribe(v_fs_get_user_data(98), node_id, buffer_id);

	return buffer_pos;
}

#endif

