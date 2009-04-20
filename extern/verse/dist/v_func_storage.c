/*
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include "verse_header.h"
#include "v_pack.h"
#include "v_cmd_gen.h"
#include "v_connection.h"
#if !defined(V_GENERATE_FUNC_MODE)
#include "verse.h"
#include "v_cmd_buf.h"
#include "v_network_out_que.h"

#define V_FS_MAX_CMDS	256

extern void init_pack_and_unpack(void);

static struct {
	unsigned int	(*unpack_func[V_FS_MAX_CMDS])(const char *data, size_t length);
	void		*pack_func[V_FS_MAX_CMDS];
	void		*user_func[V_FS_MAX_CMDS];
	void		*user_data[V_FS_MAX_CMDS];
	void		*alias_pack_func[V_FS_MAX_CMDS];
	void		*alias_user_func[V_FS_MAX_CMDS];
	void		*alias_user_data[V_FS_MAX_CMDS];
	boolean		call;
} VCmdData;

static boolean v_fs_initialized = FALSE;

extern void verse_send_packet_ack(uint32 packet_id);
extern void callback_send_packet_ack(void *user, uint32 packet_id);
extern void verse_send_packet_nak(uint32 packet_id);
extern void callback_send_packet_nak(void *user, uint32 packet_id);

void v_fs_init(void)
{
	unsigned int i;
	if(v_fs_initialized)
		return;
	for(i = 0; i < V_FS_MAX_CMDS; i++)
	{
		VCmdData.unpack_func[i] = NULL;
		VCmdData.pack_func[i] = NULL;
		VCmdData.user_func[i] = NULL;
		VCmdData.user_data[i] = NULL;
		VCmdData.alias_pack_func[i] = NULL;
		VCmdData.alias_user_func[i] = NULL;
		VCmdData.alias_user_data[i] = NULL;
	}
	#if !defined(V_GENERATE_FUNC_MODE)
	init_pack_and_unpack();
	#endif
	for(i = 0; i < V_FS_MAX_CMDS && VCmdData.pack_func[i] != verse_send_packet_ack; i++);
	VCmdData.user_func[i] = callback_send_packet_ack;
	for(i = 0; i < V_FS_MAX_CMDS && VCmdData.pack_func[i] != verse_send_packet_nak; i++);
	VCmdData.user_func[i] = callback_send_packet_nak;

	v_fs_initialized = TRUE;
}


void v_fs_add_func(unsigned int cmd_id, unsigned int (*unpack_func)(const char *data, size_t length), void *pack_func, void *alias_func)
{
	VCmdData.unpack_func[cmd_id] = unpack_func;
	VCmdData.pack_func[cmd_id] = pack_func;
	VCmdData.alias_pack_func[cmd_id] = alias_func;
}

void *v_fs_get_user_func(unsigned int cmd_id)
{
/*	if(VCmdData.call)*/
		return VCmdData.user_func[cmd_id];
	return NULL;
}

void *v_fs_get_user_data(unsigned int cmd_id)
{
	return VCmdData.user_data[cmd_id];
}

void *v_fs_get_alias_user_func(unsigned int cmd_id)
{
/*	if(VCmdData.call)*/
		return VCmdData.alias_user_func[cmd_id];
	return NULL;
}

void *v_fs_get_alias_user_data(unsigned int cmd_id)
{
	return VCmdData.alias_user_data[cmd_id];
}

void verse_callback_set(void *command, void *callback, void *user)
{
	unsigned int i;
	if(!v_fs_initialized)
		v_fs_init();

	for(i = 0; i < V_FS_MAX_CMDS; i++)
	{
		if(VCmdData.pack_func[i] == command)
		{
			VCmdData.user_data[i] = user;
			VCmdData.user_func[i] = callback;
			return;
		}
		if(VCmdData.alias_pack_func[i] == command)
		{
			VCmdData.alias_user_data[i] = user;
			VCmdData.alias_user_func[i] = callback;
			return;
		}
	}
}

/* Do we accept incoming connections, i.e. are we a host implementation? */
boolean v_fs_func_accept_connections(void)
{
	return VCmdData.user_func[0] != NULL;
}

/* Inspect beginning of packet, looking for ACK or NAK commands. */
void v_fs_unpack_beginning(const uint8 *data, unsigned int length)
{
	uint32 id, i = 4;
	uint8 cmd_id;

	i += vnp_raw_unpack_uint8(&data[i], &cmd_id);
	while(i < length && (cmd_id == 7 || cmd_id == 8))
	{
		i += vnp_raw_unpack_uint32(&data[i], &id);
		if(cmd_id == 7)
			callback_send_packet_ack(NULL, id);
		else
			callback_send_packet_nak(NULL, id);
		i += vnp_raw_unpack_uint8(&data[i], &cmd_id);
	}
}

void v_fs_unpack(uint8 *data, unsigned int length)
{
	uint32 i, output, pack_id;
	uint8 cmd_id, last = 255;

	i = vnp_raw_unpack_uint32(data, &pack_id); /* each packet starts with a 32 bit id */
	vnp_raw_unpack_uint8(&data[i], &cmd_id);
	while(i < length && (cmd_id == 7 || cmd_id == 8))
	{
		i += 5;
		vnp_raw_unpack_uint8(&data[i], &cmd_id);
	}
	while(i < length)
	{
		i += vnp_raw_unpack_uint8(&data[i], &cmd_id);
		if(VCmdData.unpack_func[cmd_id] != NULL)
		{
			VCmdData.call = TRUE;
			output = VCmdData.unpack_func[cmd_id](&data[i], length - i);
			if(output == (unsigned int) -1)	/* Can this happen? Should be size_t or int, depending. */
			{
				printf("** Aborting decode, command %u unpacker returned failure\n", cmd_id);
/*				verse_send_packet_nak(pack_id);*/
				return;
			}
			last = cmd_id;
			i += output;
		}
		else	/* If unknown command byte was found, complain loudly and stop parsing packet. */
		{
			size_t	j;

			printf("\n** Unknown command ID %u (0x%02X) encountered--aborting packet decode len=%u pos=%u last=%u\n", cmd_id, cmd_id, length, i, last);
			printf(" decoded %u bytes: ", --i);
			for(j = 0; j < i; j++)
				printf("%02X ", data[j]);
			printf("\n (packet id=%u)", pack_id);
			printf(" remaining %u bytes: ", length - i);
			for(j = i; j < length; j++)
				printf("%02X ", data[j]);
			printf("\n");
/*			*(char *) 0 = 0;*/
			break;
		}
	}
/*	if(expected != NULL)
		verse_send_packet_ack(pack_id);*/
}

extern unsigned int v_unpack_connection(const char *data, size_t length);

#endif
