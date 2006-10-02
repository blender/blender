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

void verse_send_o_transform_pos_real32(VNodeID node_id, uint32 time_s, uint32 time_f, const real32 *pos, const real32 *speed, const real32 *accelerate, const real32 *drag_normal, real32 drag)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 32);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_pos_real32(node_id = %u time_s = %u time_f = %u pos = %p speed = %p accelerate = %p drag_normal = %p drag = %f );\n", node_id, time_s, time_f, pos, speed, accelerate, drag_normal, drag);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_s);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_f);
	{
		unsigned char mask = 0;
		unsigned int cmd;
		cmd = buffer_pos++;
		buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[0]);
		buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[1]);
		buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], pos[2]);
		if(speed != NULL && (speed[0] > 0.0000001 || speed[0] < -0.0000001 || speed[1] > 0.0000001 || speed[1] < -0.0000001 || speed[2] > 0.0000001 || speed[2] < -0.0000001))
		{
			mask |= 1;
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[0]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[1]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], speed[2]);
		}
		if(accelerate != NULL && (accelerate[0] > 0.0000001 || accelerate[0] < -0.0000001 || accelerate[1] > 0.0000001 || accelerate[1] < -0.0000001 || accelerate[2] > 0.0000001 || accelerate[2] < -0.0000001))
		{
			mask |= 2;
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[0]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[1]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], accelerate[2]);
		}
		if(drag_normal != NULL && (drag > 0.0000001 || drag < -0.0000001) && (drag_normal[0] > 0.0000001 || drag_normal[0] < -0.0000001 || drag_normal[1] > 0.0000001 || drag_normal[1] < -0.0000001 || drag_normal[2] > 0.0000001 || drag_normal[2] < -0.0000001))
		{
			mask |= 4;
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[0]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[1]);
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag_normal[2]);
		}
		if(drag > 0.0000001 || drag < -0.0000001)
		{
			mask |= 8;
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);
		}
		vnp_raw_pack_uint8(&buf[cmd], mask);
	}if(FALSE)
	buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_pos_real32(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_pos_real32)(void *user_data, VNodeID node_id, uint32 time_s, uint32 time_f, const real32 *pos, const real32 *speed, const real32 *accelerate, const real32 *drag_normal, real32 drag);
	VNodeID node_id;
	uint32 time_s;
	uint32 time_f;
	const real32 *pos;
	const real32 *speed;
	const real32 *accelerate;
	const real32 *drag_normal;
	real32 drag;
	
	func_o_transform_pos_real32 = v_fs_get_user_func(32);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_s);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_f);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_pos_real32(node_id = %u time_s = %u time_f = %u drag = %f ); callback = %p\n", node_id, time_s, time_f, drag, v_fs_get_user_func(32));
#endif
	{
		float output[4][3];
		unsigned int i, j;
		char mask, pow = 1;
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);
		for(j = 0; j < 3; j++)
			buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &output[0][j]);
		for(i = 1; i < 4; i++)
		{
			if((mask & pow) != 0)
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &output[i][j]);
			else
				for(j = 0; j < 3; j++)
					output[i][j] = 0;
			pow *= 2;
		}
		if((mask & pow) != 0)
			buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &drag);
		else
			drag = 0.0f;
		if(func_o_transform_pos_real32 != NULL)
			func_o_transform_pos_real32(v_fs_get_user_data(32), node_id, time_s, time_f, &output[0][0], &output[1][0], &output[2][0], &output[3][0], drag);
		return buffer_pos;
	}

	if(func_o_transform_pos_real32 != NULL)
		func_o_transform_pos_real32(v_fs_get_user_data(32), node_id, time_s, time_f, pos, speed, accelerate, drag_normal, drag);

	return buffer_pos;
}

void verse_send_o_transform_rot_real32(VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat32 *rot, const VNQuat32 *speed, const VNQuat32 *accelerate, const VNQuat32 *drag_normal, real32 drag)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 33);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_rot_real32(node_id = %u time_s = %u time_f = %u rot = %p speed = %p accelerate = %p drag_normal = %p drag = %f );\n", node_id, time_s, time_f, rot, speed, accelerate, drag_normal, drag);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_s);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_f);
	{
		uint8 mask = 0;
		unsigned int maskpos;
		maskpos = buffer_pos++;		/* Remember location, and reserve a byte for the mask. */
		buffer_pos += vnp_pack_quat32(&buf[buffer_pos], rot);
		if(v_quat32_valid(speed))
		{
			mask |= 1;
			buffer_pos += vnp_pack_quat32(&buf[buffer_pos], speed);
		}
		if(v_quat32_valid(accelerate))
		{
			mask |= 2;
			buffer_pos += vnp_pack_quat32(&buf[buffer_pos], accelerate);
		}
		if(v_quat32_valid(drag_normal))
		{
			mask |= 4;
			buffer_pos += vnp_pack_quat32(&buf[buffer_pos], drag_normal);
		}
		if(drag > 0.0000001 || drag < -0.0000001)
		{
			mask |= 8;
			buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);
		}
		vnp_raw_pack_uint8(&buf[maskpos], mask);	/* Write the mask into start of command. */
	}
	if(FALSE)
	buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], drag);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_rot_real32(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_rot_real32)(void *user_data, VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat32 *rot, const VNQuat32 *speed, const VNQuat32 *accelerate, const VNQuat32 *drag_normal, real32 drag);
	VNodeID node_id;
	uint32 time_s;
	uint32 time_f;
	const VNQuat32 *rot;
	const VNQuat32 *speed;
	const VNQuat32 *accelerate;
	const VNQuat32 *drag_normal;
	real32 drag;
	
	func_o_transform_rot_real32 = v_fs_get_user_func(33);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_s);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_f);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_rot_real32(node_id = %u time_s = %u time_f = %u drag = %f ); callback = %p\n", node_id, time_s, time_f, drag, v_fs_get_user_func(33));
#endif
	{
		VNQuat32 trot, temp[3], *q[3];
		unsigned int i;
		uint8 mask, test;
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);
		buffer_pos += vnp_unpack_quat32(&buf[buffer_pos], &trot);
		for(i = 0, test = 1; i < sizeof temp / sizeof *temp; i++, test <<= 1)
		{
			if(mask & test)		/* Field present? */
			{
				buffer_pos += vnp_unpack_quat32(&buf[buffer_pos], &temp[i]);
				q[i] = &temp[i];
			}
			else
				q[i] = NULL;
		}
		if(mask & test)
			buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &drag);
		else
			drag = 0.0;
		if(func_o_transform_rot_real32 != NULL)
			func_o_transform_rot_real32(v_fs_get_user_data(33), node_id, time_s, time_f, &trot, q[0], q[1], q[2], drag);
		return buffer_pos;
	}

	if(func_o_transform_rot_real32 != NULL)
		func_o_transform_rot_real32(v_fs_get_user_data(33), node_id, time_s, time_f, rot, speed, accelerate, drag_normal, drag);

	return buffer_pos;
}

void verse_send_o_transform_scale_real32(VNodeID node_id, real32 scale_x, real32 scale_y, real32 scale_z)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_20);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 34);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_scale_real32(node_id = %u scale_x = %f scale_y = %f scale_z = %f );\n", node_id, scale_x, scale_y, scale_z);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], scale_x);
	buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], scale_y);
	buffer_pos += vnp_raw_pack_real32(&buf[buffer_pos], scale_z);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_scale_real32(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_scale_real32)(void *user_data, VNodeID node_id, real32 scale_x, real32 scale_y, real32 scale_z);
	VNodeID node_id;
	real32 scale_x;
	real32 scale_y;
	real32 scale_z;
	
	func_o_transform_scale_real32 = v_fs_get_user_func(34);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &scale_x);
	buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &scale_y);
	buffer_pos += vnp_raw_unpack_real32(&buf[buffer_pos], &scale_z);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_scale_real32(node_id = %u scale_x = %f scale_y = %f scale_z = %f ); callback = %p\n", node_id, scale_x, scale_y, scale_z, v_fs_get_user_func(34));
#endif
	if(func_o_transform_scale_real32 != NULL)
		func_o_transform_scale_real32(v_fs_get_user_data(34), node_id, scale_x, scale_y, scale_z);

	return buffer_pos;
}

void verse_send_o_transform_pos_real64(VNodeID node_id, uint32 time_s, uint32 time_f, const real64 *pos, const real64 *speed, const real64 *accelerate, const real64 *drag_normal, real64 drag)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 35);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_pos_real64(node_id = %u time_s = %u time_f = %u pos = %p speed = %p accelerate = %p drag_normal = %p drag = %f );\n", node_id, time_s, time_f, pos, speed, accelerate, drag_normal, drag);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_s);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_f);
	{
		unsigned char mask = 0;
		unsigned int cmd;
		cmd = buffer_pos++;
		buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[0]);
		buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[1]);
		buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[2]);
		if(speed != NULL && (speed[0] > 0.0000001 || speed[0] < -0.0000001 || speed[1] > 0.0000001 || speed[1] < -0.0000001 || speed[2] > 0.0000001 || speed[2] < -0.0000001))
		{
			mask |= 1;
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[0]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[1]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[2]);
		}
		if(accelerate != NULL && (accelerate[0] > 0.0000001 || accelerate[0] < -0.0000001 || accelerate[1] > 0.0000001 || accelerate[1] < -0.0000001 || accelerate[2] > 0.0000001 || accelerate[2] < -0.0000001))
		{
			mask |= 2;
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[0]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[1]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accelerate[2]);
		}
		if(drag_normal != NULL && (drag > 0.0000001 || drag < -0.0000001) && (drag_normal[0] > 0.0000001 || drag_normal[0] < -0.0000001 || drag_normal[1] > 0.0000001 || drag_normal[1] < -0.0000001 || drag_normal[2] > 0.0000001 || drag_normal[2] < -0.0000001))
		{
			mask |= 4;
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[0]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[1]);
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag_normal[2]);
		}
		if(drag > 0.0000001 || drag < -0.0000001)
		{
			mask |= 8;
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);
		}
		vnp_raw_pack_uint8(&buf[cmd], mask);
	}if(FALSE)
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_pos_real64(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_pos_real64)(void *user_data, VNodeID node_id, uint32 time_s, uint32 time_f, const real64 *pos, const real64 *speed, const real64 *accelerate, const real64 *drag_normal, real64 drag);
	VNodeID node_id;
	uint32 time_s;
	uint32 time_f;
	const real64 *pos;
	const real64 *speed;
	const real64 *accelerate;
	const real64 *drag_normal;
	real64 drag;
	
	func_o_transform_pos_real64 = v_fs_get_user_func(35);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_s);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_f);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_pos_real64(node_id = %u time_s = %u time_f = %u drag = %f ); callback = %p\n", node_id, time_s, time_f, drag, v_fs_get_user_func(35));
#endif
	{
		double output[4][3];
		unsigned int i, j;
		char mask, pow = 1;
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);
		for(j = 0; j < 3; j++)
			buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[0][j]);
		for(i = 1; i < 4; i++)
		{
			if((mask & pow) != 0)
				for(j = 0; j < 3; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[i][j]);
			else
				for(j = 0; j < 3; j++)
					output[i][j] = 0;
			pow *= 2;
		}
		if((mask & pow) != 0)
			buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &drag);
		else
			drag = 0.0;
		if(func_o_transform_pos_real64 != NULL)
			func_o_transform_pos_real64(v_fs_get_user_data(35), node_id, time_s, time_f, &output[0][0], &output[1][0], &output[2][0], &output[3][0], drag);
		return buffer_pos;
	}

	if(func_o_transform_pos_real64 != NULL)
		func_o_transform_pos_real64(v_fs_get_user_data(35), node_id, time_s, time_f, pos, speed, accelerate, drag_normal, drag);

	return buffer_pos;
}

void verse_send_o_transform_rot_real64(VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat64 *rot, const VNQuat64 *speed, const VNQuat64 *accelerate, const VNQuat64 *drag_normal, real64 drag)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 36);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_rot_real64(node_id = %u time_s = %u time_f = %u rot = %p speed = %p accelerate = %p drag_normal = %p drag = %f );\n", node_id, time_s, time_f, rot, speed, accelerate, drag_normal, drag);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_s);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_f);
	{
		uint8 mask = 0;
		unsigned int maskpos;
		maskpos = buffer_pos++;		/* Remember location, and reserve a byte for the mask. */
		buffer_pos += vnp_pack_quat64(&buf[buffer_pos], rot);
		if(v_quat64_valid(speed))
		{
			mask |= 1;
			buffer_pos += vnp_pack_quat64(&buf[buffer_pos], speed);
		}
		if(v_quat64_valid(accelerate))
		{
			mask |= 2;
			buffer_pos += vnp_pack_quat64(&buf[buffer_pos], accelerate);
		}
		if(v_quat64_valid(drag_normal))
		{
			mask |= 4;
			buffer_pos += vnp_pack_quat64(&buf[buffer_pos], drag_normal);
		}
		if(drag > 0.0000001 || drag < -0.0000001)
		{
			mask |= 8;
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);
		}
		vnp_raw_pack_uint8(&buf[maskpos], mask);	/* Write the mask into start of command. */
	}
	if(FALSE)
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], drag);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_rot_real64(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_rot_real64)(void *user_data, VNodeID node_id, uint32 time_s, uint32 time_f, const VNQuat64 *rot, const VNQuat64 *speed, const VNQuat64 *accelerate, const VNQuat64 *drag_normal, real64 drag);
	VNodeID node_id;
	uint32 time_s;
	uint32 time_f;
	const VNQuat64 *rot;
	const VNQuat64 *speed;
	const VNQuat64 *accelerate;
	const VNQuat64 *drag_normal;
	real64 drag;
	
	func_o_transform_rot_real64 = v_fs_get_user_func(36);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_s);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_f);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_rot_real64(node_id = %u time_s = %u time_f = %u drag = %f ); callback = %p\n", node_id, time_s, time_f, drag, v_fs_get_user_func(36));
#endif
	{
		VNQuat64 trot, temp[3], *q[3];
		unsigned int i;
		uint8 mask, test;
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);
		buffer_pos += vnp_unpack_quat64(&buf[buffer_pos], &trot);
		for(i = 0, test = 1; i < sizeof temp / sizeof *temp; i++, test <<= 1)
		{
			if(mask & test)		/* Field present? */
			{
				buffer_pos += vnp_unpack_quat64(&buf[buffer_pos], &temp[i]);
				q[i] = &temp[i];
			}
			else
				q[i] = NULL;
		}
		if(mask & test)
			buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &drag);
		else
			drag = 0.0;
		if(func_o_transform_rot_real64 != NULL)
			func_o_transform_rot_real64(v_fs_get_user_data(36), node_id, time_s, time_f, &trot, q[0], q[1], q[2], drag);
		return buffer_pos;
	}

	if(func_o_transform_rot_real64 != NULL)
		func_o_transform_rot_real64(v_fs_get_user_data(36), node_id, time_s, time_f, rot, speed, accelerate, drag_normal, drag);

	return buffer_pos;
}

void verse_send_o_transform_scale_real64(VNodeID node_id, real64 scale_x, real64 scale_y, real64 scale_z)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 37);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_scale_real64(node_id = %u scale_x = %f scale_y = %f scale_z = %f );\n", node_id, scale_x, scale_y, scale_z);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale_x);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale_y);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale_z);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_scale_real64(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_transform_scale_real64)(void *user_data, VNodeID node_id, real64 scale_x, real64 scale_y, real64 scale_z);
	VNodeID node_id;
	real64 scale_x;
	real64 scale_y;
	real64 scale_z;
	
	func_o_transform_scale_real64 = v_fs_get_user_func(37);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &scale_x);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &scale_y);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &scale_z);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_transform_scale_real64(node_id = %u scale_x = %f scale_y = %f scale_z = %f ); callback = %p\n", node_id, scale_x, scale_y, scale_z, v_fs_get_user_func(37));
#endif
	if(func_o_transform_scale_real64 != NULL)
		func_o_transform_scale_real64(v_fs_get_user_data(37), node_id, scale_x, scale_y, scale_z);

	return buffer_pos;
}

void verse_send_o_transform_subscribe(VNodeID node_id, VNRealFormat type)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 38);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_subscribe(node_id = %u type = %u );\n", node_id, type);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)type);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_o_transform_unsubscribe(VNodeID node_id, VNRealFormat type)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 38);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_transform_unsubscribe(node_id = %u type = %u );\n", node_id, type);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)type);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_transform_subscribe(const char *buf, size_t buffer_length)
{
	uint8 enum_temp;
	unsigned int buffer_pos = 0;
	void (* func_o_transform_subscribe)(void *user_data, VNodeID node_id, VNRealFormat type);
	VNodeID node_id;
	VNRealFormat type;
	uint8	alias_bool;

	func_o_transform_subscribe = v_fs_get_user_func(38);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &enum_temp);
	type = (VNRealFormat)enum_temp;
	if(buffer_length < buffer_pos + 1)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(!alias_bool)
		printf("receive: verse_send_o_transform_unsubscribe(node_id = %u type = %u ); callback = %p\n", node_id, type, v_fs_get_alias_user_func(38));
	else
		printf("receive: verse_send_o_transform_subscribe(node_id = %u type = %u ); callback = %p\n", node_id, type, v_fs_get_user_func(38));
#endif
	if(!alias_bool)
	{
		void (* alias_o_transform_unsubscribe)(void *user_data, VNodeID node_id, VNRealFormat type);
		alias_o_transform_unsubscribe = v_fs_get_alias_user_func(38);
		if(alias_o_transform_unsubscribe != NULL)
			alias_o_transform_unsubscribe(v_fs_get_alias_user_data(38), node_id, (VNRealFormat)type);
		return buffer_pos;
	}
	if(func_o_transform_subscribe != NULL)
		func_o_transform_subscribe(v_fs_get_user_data(38), node_id, (VNRealFormat) type);

	return buffer_pos;
}

void verse_send_o_light_set(VNodeID node_id, real64 light_r, real64 light_g, real64 light_b)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 39);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_light_set(node_id = %u light_r = %f light_g = %f light_b = %f );\n", node_id, light_r, light_g, light_b);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], light_r);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], light_g);
	buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], light_b);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_light_set(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_light_set)(void *user_data, VNodeID node_id, real64 light_r, real64 light_g, real64 light_b);
	VNodeID node_id;
	real64 light_r;
	real64 light_g;
	real64 light_b;
	
	func_o_light_set = v_fs_get_user_func(39);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &light_r);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &light_g);
	buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &light_b);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_light_set(node_id = %u light_r = %f light_g = %f light_b = %f ); callback = %p\n", node_id, light_r, light_g, light_b, v_fs_get_user_func(39));
#endif
	if(func_o_light_set != NULL)
		func_o_light_set(v_fs_get_user_data(39), node_id, light_r, light_g, light_b);

	return buffer_pos;
}

void verse_send_o_link_set(VNodeID node_id, uint16 link_id, VNodeID link, const char *label, uint32 target_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_80);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 40);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_link_set(node_id = %u link_id = %u link = %u label = %s target_id = %u );\n", node_id, link_id, link, label, target_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], link_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], link);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], label, 16);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], target_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);
	if(node_id == (uint32) ~0u || link_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_o_link_destroy(VNodeID node_id, uint16 link_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_80);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 40);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_link_destroy(node_id = %u link_id = %u );\n", node_id, link_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], link_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], -1);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 16);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], -1);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);
	if(node_id == (uint32) ~0u || link_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_link_set(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_link_set)(void *user_data, VNodeID node_id, uint16 link_id, VNodeID link, const char *label, uint32 target_id);
	VNodeID node_id;
	uint16 link_id;
	VNodeID link;
	char label[16];
	uint32 target_id;
	uint8	alias_bool;

	func_o_link_set = v_fs_get_user_func(40);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &link_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &link);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], label, 16, buffer_length - buffer_pos);
	if(buffer_length < 4 + buffer_pos)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &target_id);
	if(buffer_length < buffer_pos + 1)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(!alias_bool)
		printf("receive: verse_send_o_link_destroy(node_id = %u link_id = %u ); callback = %p\n", node_id, link_id, v_fs_get_alias_user_func(40));
	else
		printf("receive: verse_send_o_link_set(node_id = %u link_id = %u link = %u label = %s target_id = %u ); callback = %p\n", node_id, link_id, link, label, target_id, v_fs_get_user_func(40));
#endif
	if(!alias_bool)
	{
		void (* alias_o_link_destroy)(void *user_data, VNodeID node_id, uint16 link_id);
		alias_o_link_destroy = v_fs_get_alias_user_func(40);
		if(alias_o_link_destroy != NULL)
			alias_o_link_destroy(v_fs_get_alias_user_data(40), node_id, link_id);
		return buffer_pos;
	}
	if(func_o_link_set != NULL)
		func_o_link_set(v_fs_get_user_data(40), node_id, link_id, link, label, target_id);

	return buffer_pos;
}

void verse_send_o_method_group_create(VNodeID node_id, uint16 group_id, const char *name)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 41);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_group_create(node_id = %u group_id = %u name = %s );\n", node_id, group_id, name);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], name, 16);
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_o_method_group_destroy(VNodeID node_id, uint16 group_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_30);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 41);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_group_destroy(node_id = %u group_id = %u );\n", node_id, group_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 16);
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_method_group_create(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_method_group_create)(void *user_data, VNodeID node_id, uint16 group_id, const char *name);
	VNodeID node_id;
	uint16 group_id;
	char name[16];
	
	func_o_method_group_create = v_fs_get_user_func(41);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &group_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 16, buffer_length - buffer_pos);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(name[0] == 0)
		printf("receive: verse_send_o_method_group_destroy(node_id = %u group_id = %u ); callback = %p\n", node_id, group_id, v_fs_get_alias_user_func(41));
	else
		printf("receive: verse_send_o_method_group_create(node_id = %u group_id = %u name = %s ); callback = %p\n", node_id, group_id, name, v_fs_get_user_func(41));
#endif
	if(name[0] == 0)
	{
		void (* alias_o_method_group_destroy)(void *user_data, VNodeID node_id, uint16 group_id);
		alias_o_method_group_destroy = v_fs_get_alias_user_func(41);
		if(alias_o_method_group_destroy != NULL)
			alias_o_method_group_destroy(v_fs_get_alias_user_data(41), node_id, group_id);
		return buffer_pos;
	}
	if(func_o_method_group_create != NULL)
		func_o_method_group_create(v_fs_get_user_data(41), node_id, group_id, name);

	return buffer_pos;
}

void verse_send_o_method_group_subscribe(VNodeID node_id, uint16 group_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 42);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_group_subscribe(node_id = %u group_id = %u );\n", node_id, group_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], TRUE);
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_o_method_group_unsubscribe(VNodeID node_id, uint16 group_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 42);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_group_unsubscribe(node_id = %u group_id = %u );\n", node_id, group_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], FALSE);
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_method_group_subscribe(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_method_group_subscribe)(void *user_data, VNodeID node_id, uint16 group_id);
	VNodeID node_id;
	uint16 group_id;
	uint8	alias_bool;

	func_o_method_group_subscribe = v_fs_get_user_func(42);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &group_id);
	if(buffer_length < buffer_pos + 1)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &alias_bool);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(!alias_bool)
		printf("receive: verse_send_o_method_group_unsubscribe(node_id = %u group_id = %u ); callback = %p\n", node_id, group_id, v_fs_get_alias_user_func(42));
	else
		printf("receive: verse_send_o_method_group_subscribe(node_id = %u group_id = %u ); callback = %p\n", node_id, group_id, v_fs_get_user_func(42));
#endif
	if(!alias_bool)
	{
		void (* alias_o_method_group_unsubscribe)(void *user_data, VNodeID node_id, uint16 group_id);
		alias_o_method_group_unsubscribe = v_fs_get_alias_user_func(42);
		if(alias_o_method_group_unsubscribe != NULL)
			alias_o_method_group_unsubscribe(v_fs_get_alias_user_data(42), node_id, group_id);
		return buffer_pos;
	}
	if(func_o_method_group_subscribe != NULL)
		func_o_method_group_subscribe(v_fs_get_user_data(42), node_id, group_id);

	return buffer_pos;
}

void verse_send_o_method_create(VNodeID node_id, uint16 group_id, uint16 method_id, const char *name, uint8 param_count, const VNOParamType *param_types, const char * *param_names)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 43);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_create(node_id = %u group_id = %u method_id = %u name = %s param_count = %u param_types = %p param_names = %p );\n", node_id, group_id, method_id, name, param_count, param_types, param_names);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], method_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], name, 512);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], param_count);
	{
		unsigned int i, j, sum = 1;
		for(i = 0; i < param_count; i++)
		{
			sum += 3;
			for(j = 0; param_names[i][j] != 0; j++);
		}
		if(sum + buffer_pos > 1500)
			return;
		for(i = 0; i < param_count; i++)
		{
			buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], param_types[i]);
			buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], param_names[i], 1500 - buffer_pos);
		}
	}
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u || method_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 9);
	else
		v_cmd_buf_set_address_size(head, 9);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

void verse_send_o_method_destroy(VNodeID node_id, uint16 group_id, uint16 method_id)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 43);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_destroy(node_id = %u group_id = %u method_id = %u );\n", node_id, group_id, method_id);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], method_id);
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], NULL, 512);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], -1);
	if(node_id == (uint32) ~0u || group_id == (uint16) ~0u || method_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 9);
	else
		v_cmd_buf_set_address_size(head, 9);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_method_create(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_method_create)(void *user_data, VNodeID node_id, uint16 group_id, uint16 method_id, const char *name, uint8 param_count, const VNOParamType *param_types, const char * *param_names);
	VNodeID node_id;
	uint16 group_id;
	uint16 method_id;
	char name[512];
	uint8 param_count;
	const VNOParamType *param_types;
	const char * *param_names;
	
	func_o_method_create = v_fs_get_user_func(43);
	if(buffer_length < 8)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &group_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &method_id);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 512, buffer_length - buffer_pos);
	if(buffer_length < 1 + buffer_pos)
		return -1;
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &param_count);
#if defined V_PRINT_RECEIVE_COMMANDS
	if(name[0] == 0)
		printf("receive: verse_send_o_method_destroy(node_id = %u group_id = %u method_id = %u ); callback = %p\n", node_id, group_id, method_id, v_fs_get_alias_user_func(43));
	else
		printf("receive: verse_send_o_method_create(node_id = %u group_id = %u method_id = %u name = %s param_count = %u ); callback = %p\n", node_id, group_id, method_id, name, param_count, v_fs_get_user_func(43));
#endif
	if(param_count != 255)
	{
		unsigned int i, size, text = 0;
		VNOParamType types[256];
		uint8 t;
		char name_buf[1500], *names[256];
		for(i = 0; i < param_count; i++)
		{
			buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &t);
			types[i] = t;
			names[i] = &name_buf[text];
			size = vnp_raw_unpack_string(&buf[buffer_pos], names[i], 1500 - buffer_pos, buffer_length - buffer_pos);
			buffer_pos += size;
			text += size;
		}
		if(func_o_method_create != NULL)
			func_o_method_create(v_fs_get_user_data(43), node_id, group_id, method_id, name, param_count, types, (const char **) names);
		return buffer_pos;
	}

	if(name[0] == 0)
	{
		void (* alias_o_method_destroy)(void *user_data, VNodeID node_id, uint16 group_id, uint16 method_id);
		alias_o_method_destroy = v_fs_get_alias_user_func(43);
		if(alias_o_method_destroy != NULL)
			alias_o_method_destroy(v_fs_get_alias_user_data(43), node_id, group_id, method_id);
		return buffer_pos;
	}
	if(func_o_method_create != NULL)
		func_o_method_create(v_fs_get_user_data(43), node_id, group_id, method_id, name, param_count, param_types, param_names);

	return buffer_pos;
}

void verse_send_o_method_call(VNodeID node_id, uint16 group_id, uint16 method_id, VNodeID sender, const VNOPackedParams *params)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 44);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_method_call(node_id = %u group_id = %u method_id = %u sender = %u params = %p );\n", node_id, group_id, method_id, sender, params);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], group_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], method_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], sender);
	{
		unsigned int i;
		uint16 size;
		vnp_raw_unpack_uint16(params, &size);
		for(i = 0; i < size; i++)
			buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], ((uint8 *)params)[i]);
		free((void *) params);	/* Drop the const. */
	}
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_method_call(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_method_call)(void *user_data, VNodeID node_id, uint16 group_id, uint16 method_id, VNodeID sender, const VNOPackedParams *params);
	VNodeID node_id;
	uint16 group_id;
	uint16 method_id;
	VNodeID sender;
	const VNOPackedParams *params;
	
	func_o_method_call = v_fs_get_user_func(44);
	if(buffer_length < 12)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &group_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &method_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &sender);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_method_call(node_id = %u group_id = %u method_id = %u sender = %u ); callback = %p\n", node_id, group_id, method_id, sender, v_fs_get_user_func(44));
#endif
	{
		unsigned int i;
		uint8 par[1500];
		uint16 size;
		vnp_raw_unpack_uint16(&buf[buffer_pos], &size);
		for(i = 0; i < size; i++)
			buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &par[i]);
		if(func_o_method_call != NULL)
			func_o_method_call(v_fs_get_user_data(44), node_id, group_id, method_id, sender, par);
		return buffer_pos;
	}

	if(func_o_method_call != NULL)
		func_o_method_call(v_fs_get_user_data(44), node_id, group_id, method_id, sender, params);

	return buffer_pos;
}

void verse_send_o_anim_run(VNodeID node_id, uint16 link_id, uint32 time_s, uint32 time_f, uint8 dimensions, const real64 *pos, const real64 *speed, const real64 *accel, const real64 *scale, const real64 *scale_speed)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_1500);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 45);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_anim_run(node_id = %u link_id = %u time_s = %u time_f = %u dimensions = %u pos = %p speed = %p accel = %p scale = %p scale_speed = %p );\n", node_id, link_id, time_s, time_f, dimensions, pos, speed, accel, scale, scale_speed);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], link_id);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_s);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], time_f);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], dimensions);
	{
		unsigned char mask = 0;
		unsigned int cmd, i;
		cmd = buffer_pos++;
		if(dimensions > 4)
			dimensions = 4;
		for(i = 0; i < dimensions; i++)
			buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], pos[i]);
		if(speed != NULL)
		{
			mask |= 1;
			for(i = 0; i < dimensions; i++)
				buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], speed[i]);
		}
		if(accel != NULL)
		{
			mask |= 2;
			for(i = 0; i < dimensions; i++)
				buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], accel[i]);
		}
		if(scale != NULL)
		{
			mask |= 3;
			for(i = 0; i < dimensions; i++)
				buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale[i]);
		}
		if(scale_speed != NULL)
		{
			mask |= 4;
			for(i = 0; i < dimensions; i++)
				buffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], scale_speed[i]);
		}
		vnp_raw_pack_uint8(&buf[cmd], mask);
	}
	if(node_id == (uint32) ~0u || link_id == (uint16) ~0u)
		v_cmd_buf_set_unique_address_size(head, 7);
	else
		v_cmd_buf_set_address_size(head, 7);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_anim_run(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_anim_run)(void *user_data, VNodeID node_id, uint16 link_id, uint32 time_s, uint32 time_f, uint8 dimensions, const real64 *pos, const real64 *speed, const real64 *accel, const real64 *scale, const real64 *scale_speed);
	VNodeID node_id;
	uint16 link_id;
	uint32 time_s;
	uint32 time_f;
	uint8 dimensions;
	const real64 *pos;
	const real64 *speed;
	const real64 *accel;
	const real64 *scale;
	const real64 *scale_speed;
	
	func_o_anim_run = v_fs_get_user_func(45);
	if(buffer_length < 6)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &link_id);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_s);
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &time_f);
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &dimensions);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_anim_run(node_id = %u link_id = %u time_s = %u time_f = %u dimensions = %u ); callback = %p\n", node_id, link_id, time_s, time_f, dimensions, v_fs_get_user_func(45));
#endif
	{
		double output[5][4];
		unsigned int i, j;
		char mask, pow = 1;
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &mask);
		if(dimensions > 4)
			dimensions = 4;
		for(j = 0; j < dimensions; j++)
			buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[0][j]);
		for(i = 1; i < 5; i++)
		{
			if((mask & pow) != 0)
				for(j = 0; j < dimensions; j++)
					buffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &output[i][j]);
			else
				for(j = 0; j < dimensions; j++)
					output[i][j] = 0;
			pow *= 2;
		}
		if(func_o_anim_run != NULL)
			func_o_anim_run(v_fs_get_user_data(45), node_id, link_id, time_s, time_f, dimensions, &output[0][0], &output[1][0], &output[2][0], &output[3][0], &output[4][0]);
		return buffer_pos;
	}

	if(func_o_anim_run != NULL)
		func_o_anim_run(v_fs_get_user_data(45), node_id, link_id, time_s, time_f, dimensions, pos, speed, accel, scale, scale_speed);

	return buffer_pos;
}

void verse_send_o_hide(VNodeID node_id, uint8 hidden)
{
	uint8 *buf;
	unsigned int buffer_pos = 0;
	VCMDBufHead *head;
	head = v_cmd_buf_allocate(VCMDBS_10);/* Allocating the buffer */
	buf = ((VCMDBuffer10 *)head)->buf;

	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 46);	/* Pack the command. */
#if defined V_PRINT_SEND_COMMANDS
	printf("send: verse_send_o_hide(node_id = %u hidden = %u );\n", node_id, hidden);
#endif
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], node_id);
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], hidden);
	if(node_id == (uint32) ~0u)
		v_cmd_buf_set_unique_address_size(head, 5);
	else
		v_cmd_buf_set_address_size(head, 5);
	v_cmd_buf_set_size(head, buffer_pos);
	v_noq_send_buf(v_con_get_network_queue(), head);
}

unsigned int v_unpack_o_hide(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_o_hide)(void *user_data, VNodeID node_id, uint8 hidden);
	VNodeID node_id;
	uint8 hidden;
	
	func_o_hide = v_fs_get_user_func(46);
	if(buffer_length < 4)
		return -1;
	buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &node_id);
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &hidden);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_o_hide(node_id = %u hidden = %u ); callback = %p\n", node_id, hidden, v_fs_get_user_func(46));
#endif
	if(func_o_hide != NULL)
		func_o_hide(v_fs_get_user_data(46), node_id, hidden);

	return buffer_pos;
}

#endif

