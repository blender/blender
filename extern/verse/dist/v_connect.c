/*
**
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "v_cmd_buf.h"
#include "v_network_out_que.h"
#include "v_network.h"
#include "v_connection.h"
#include "v_encryption.h"
#include "v_util.h"

extern void verse_send_packet_ack(uint32 packet_id);

static void v_send_hidden_connect_contact(void) /*  Stage 0: Clinets inital call to connect to host */
{
	uint8 buf[V_ENCRYPTION_LOGIN_KEY_HALF_SIZE + 4 + 1 + 1], *key;
	unsigned int i, buffer_pos = 0;
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], 0);/* Packing the packet id */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 0);/* Packing the command */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 0);/* Stage 0 */

	key = v_con_get_my_key();
	for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], key[V_ENCRYPTION_LOGIN_PUBLIC_START + i]);/* Packing the command */
	for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], key[V_ENCRYPTION_LOGIN_N_START + i]);/* Packing the command */
	
	v_n_send_data(v_con_get_network_address(), buf, buffer_pos);
}

static void v_send_hidden_connect_send_key(void) /*  Stage 1: Hosts reply to any atempt to connect */
{
	uint8 buf[V_ENCRYPTION_LOGIN_KEY_SIZE * 3 + 4 + 1 + 1 + 1 + 4 + 4], *host_id;
	unsigned int i, buffer_pos = 0, s, f;
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], 0);/* Packing the packet id */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 0);/* Packing the command */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 1);/* Stage 1 */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], V_RELEASE_NUMBER);/* version */
	v_n_get_current_time(&s, &f);
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], s);/* time, seconds */
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], f);/* time, fraction */
	host_id = v_con_get_host_id();
	for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], host_id[V_ENCRYPTION_LOGIN_PUBLIC_START + i]);
	for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], host_id[V_ENCRYPTION_LOGIN_N_START + i]);
	
	v_n_send_data(v_con_get_network_address(), buf, buffer_pos);
}

static void v_send_hidden_connect_login(void) /* Stage 2: clients sends encrypted name and password */
{
	uint8		buf[1500], *key, name_pass[V_ENCRYPTION_LOGIN_KEY_SIZE], encrypted_key[V_ENCRYPTION_LOGIN_KEY_SIZE];
	const char	*name, *pass;
	unsigned int	buffer_pos = 0, i;

	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], 1);/* Packing the packet id */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 0);/* Packing the command */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 2);/* Stage 2 */
	name = v_con_get_name();
	/* Pad data area with randomness. */
	for(i = 0; i < sizeof name_pass; i++)
		name_pass[i] = rand() >> 13;
	v_strlcpy(name_pass, name, V_ENCRYPTION_LOGIN_KEY_SIZE / 2);
	pass = v_con_get_pass();
	v_strlcpy(name_pass + V_ENCRYPTION_LOGIN_KEY_SIZE / 2, pass, V_ENCRYPTION_LOGIN_KEY_SIZE / 2);
	/* Make sure last (MSB) byte is clear, to guarantee that data < key for RSA math. */
	name_pass[sizeof name_pass - 1] = 0;
	key = v_con_get_other_key();
	v_e_connect_encrypt(encrypted_key, name_pass, &key[V_ENCRYPTION_LOGIN_PUBLIC_START], &key[V_ENCRYPTION_LOGIN_N_START]);

	for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], encrypted_key[i]);
	v_n_send_data(v_con_get_network_address(), buf, buffer_pos);
}

static void v_send_hidden_connect_accept(void) /* Host accepts Clients connectionatempt and sends over data encryption key */
{
	uint8 buf[1500], *client_key, encrypted[V_ENCRYPTION_DATA_KEY_SIZE];
	unsigned int i, buffer_pos = 0;

	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], 1);/* Packing the packet id */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 1);/* Packing the command */
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], verse_session_get_avatar());
	client_key = v_con_get_other_key();
	v_e_connect_encrypt(encrypted, v_con_get_data_key(), &client_key[V_ENCRYPTION_LOGIN_PUBLIC_START], &client_key[V_ENCRYPTION_LOGIN_N_START]);
	for(i = 0; i < V_ENCRYPTION_DATA_KEY_SIZE; i++)
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], encrypted[i]);
	v_n_send_data(v_con_get_network_address(), buf, buffer_pos);
}

static void v_send_hidden_connect_terminate(VNetworkAddress *address, unsigned int packet_id, const char *bye) /* Host accepts Clients connectionatempt and sends over data encryption key */
{
	uint8 buf[1500];
	unsigned int buffer_pos = 0;
	buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], packet_id);/* Packing the packet id */
	buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 2);/* Packing the command */
	buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], bye, 512); /* pack message */
	v_e_data_encrypt_command(buf, sizeof (uint32), buf + sizeof (uint32), buffer_pos, v_con_get_data_key());
	v_n_send_data(address, buf, buffer_pos);
}

VSession verse_send_connect(const char *name, const char *pass, const char *address, const uint8 *expected_key)
{
	uint8 *my_key, *key = NULL;
	unsigned int i;
	VNetworkAddress a; 
	VSession *session;
	if(v_n_set_network_address(&a, address))
	{
#if defined(V_PRINT_SEND_COMMANDS)
		char ip_string[32];
#endif
		session = v_con_connect(a.ip, a.port, V_CS_CONTACT);
#if defined(V_PRINT_SEND_COMMANDS)
		v_n_get_address_string(&a, ip_string);
		printf("send: %p = verse_send_connect(name = %s, pass = %s, address = %s (%s), expected_key = %p)\n", session, name, pass, address, ip_string, expected_key);
#endif
		v_con_set_name_pass(name, pass);
		if(expected_key != NULL)
		{
			key = malloc((sizeof *key) * V_ENCRYPTION_LOGIN_KEY_HALF_SIZE);
			for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_HALF_SIZE; i++)
				key[i] = expected_key[i];
			*v_con_get_expected_key() = key;
		}
		my_key = v_con_get_my_key();
		v_e_connect_create_key(&my_key[V_ENCRYPTION_LOGIN_PRIVATE_START], &my_key[V_ENCRYPTION_LOGIN_PUBLIC_START], &my_key[V_ENCRYPTION_LOGIN_N_START]);
		v_send_hidden_connect_contact();
		v_con_inqueue_timer_update();	/* Reset timer in connection's in queue, above takes a while. */
		return session;
	}
	else
	{
#if defined(V_PRINT_SEND_COMMANDS)
		printf("send: NULL = verse_send_connect(name = %s, pass = %s, address = %s (Unressolved DNS), key = %p);\n", name, pass, address, key);
#endif
		return NULL;
	}
}

void v_update_connection_pending(boolean resend)
{
	VSession (* func_connect)(void *user_data, const char *name, const char *pass, const char *address, const uint8 *key) = NULL;
	VSession (* func_connect_accept)(void *user_data, VNodeID avatar, char *address, uint8 *host_id);
	void (* func_connect_terminate)(void *user_data, char *address, const char *bye);
	char address_string[32];

	switch(v_con_get_connect_stage())
	{
	case V_CS_CONTACT :		/* client tries to contact host */
		if(resend)
			v_send_hidden_connect_contact();
		break;
	case V_CS_CONTACTED :		/* Host replies with challange */
		if(resend)
			v_send_hidden_connect_send_key();
		break;
	case V_CS_PENDING_ACCEPT :	/* Client sends login */
		if(resend)
			v_send_hidden_connect_login();
		break;
	case V_CS_PENDING_HOST_CALLBACK : /* Host got login waits for accept connect callback */
		v_con_set_connect_stage(V_CS_PENDING_DECISION);
		func_connect = v_fs_get_user_func(0);
		v_n_get_address_string(v_con_get_network_address(), address_string);
#if defined(V_PRINT_RECEIVE_COMMANDS)
		printf("receive: verse_send_connect(address = %s, name = %s, pass = %s, key = NULL); callback = %p\n", address_string, v_con_get_name(), v_con_get_pass(), func_connect);
#endif
		if(func_connect != 0)
			func_connect(v_fs_get_user_data(0), v_con_get_name(), v_con_get_pass(), address_string, NULL);
		break;
	case V_CS_PENDING_CLIENT_CALLBACK_ACCEPT : /* Host got login waits for accept connect callback */
		v_con_set_connect_stage(V_CS_CONNECTED);
		func_connect_accept = v_fs_get_user_func(1);
		v_n_get_address_string(v_con_get_network_address(), address_string);
#if defined(V_PRINT_RECEIVE_COMMANDS)
		printf("receive: func_connect_accept(avatar = %u, address = %s, name = %s, pass = %s, key = NULL); callback = %p\n",
		       verse_session_get_avatar(), address_string, v_con_get_name(), v_con_get_pass(), func_connect);
#endif
		if(func_connect_accept != 0)
			func_connect_accept(v_fs_get_user_data(1), verse_session_get_avatar(), address_string, NULL);
		break;
	case V_CS_PENDING_CLIENT_CALLBACK_TERMINATE : /* Host got login waits for accept connect callback */
		v_con_set_connect_stage(V_CS_CONNECTED);
		func_connect_terminate = v_fs_get_user_func(2);
		v_n_get_address_string(v_con_get_network_address(), address_string);
#if defined(V_PRINT_RECEIVE_COMMANDS)
		printf("receive: func_connect_terminate(address = %s, bye = %s); callback = %p\n", address_string, "no message", func_connect);
#endif
		if(func_connect_terminate != 0)
			func_connect_terminate(v_fs_get_user_data(2), address_string, "no message");
		break;
	default:
		;
	}	
}

void v_unpack_connection(const char *buf, unsigned int buffer_length) /* un packing all stages of connect command */
{
	unsigned int buffer_pos, i, pack_id;
	uint32 seconds, fractions, pre;
	uint8 /*key[V_ENCRYPTION_LOGIN_KEY_SIZE], */stage, cmd_id, version;

	if(buffer_length < 5)
		return;
	
	buffer_pos = vnp_raw_unpack_uint32(buf, &pack_id);
	buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &cmd_id);
	pre = v_con_get_connect_stage();
	if(cmd_id == 0)
	{
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &stage);
		printf(" Handling connection, stage %u\n", stage);
		if(stage == V_CS_IDLE && V_CS_IDLE == v_con_get_connect_stage()) /* reseved by host */
		{
			uint8 *other_key, *my_key;

			verse_send_packet_ack(pack_id);
			my_key = v_con_get_my_key();
			v_con_set_data_key(v_e_data_create_key());
			other_key = v_con_get_other_key();
			for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &other_key[V_ENCRYPTION_LOGIN_PUBLIC_START + i]);/* Packing the command */
			for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &other_key[V_ENCRYPTION_LOGIN_N_START + i]);/* Packing the command */
			v_con_set_connect_stage(V_CS_CONTACTED);
			v_send_hidden_connect_send_key();
			return; 
		}
		if(stage == V_CS_CONTACT && V_CS_CONTACT == v_con_get_connect_stage())
		{
			uint8 *other_key; /* *host_id, *my_key, a[V_ENCRYPTION_LOGIN_KEY_SIZE], b[V_ENCRYPTION_LOGIN_KEY_SIZE];*/
			verse_send_packet_ack(pack_id);
			buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &version);
			if(version != V_RELEASE_NUMBER)
			{
			/*	char error_message[128];
				func_connect_deny = v_fs_get_user_func(2);
				#if defined(V_PRINT_RECEIVE_COMMANDS)
				printf("receive: verse_send_connect_deny(Host is running version %u you are running version %u); callback = %p\n", (uint32)version, (uint32)V_RELEASE_NUMBER func_connect_deny);
				#endif
				if(func_connect_deny != NULL)
				{
					sprintf(error_message, "Host is running version %u you are running version %u", (uint32)version, (uint32)V_RELEASE_NUMBER);
					func_connect_deny(v_fs_get_user_data(2), error_message);
				}*/
				return;
			}

			buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &seconds);
			buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &fractions);
			v_con_set_time(seconds, fractions);

			other_key = v_con_get_other_key();
			for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &other_key[V_ENCRYPTION_LOGIN_PUBLIC_START + i]);
			for(i = 0; i < V_ENCRYPTION_DATA_KEY_SIZE; i++)
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &other_key[V_ENCRYPTION_LOGIN_N_START + i]);
	
			v_con_set_connect_stage(V_CS_PENDING_ACCEPT);
			v_send_hidden_connect_login();
			return; 
		}
#if 0
		for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_HALF_SIZE && encrypted_key[i] == 0; i++);
			if(i < 0)
			{
				other_key = v_con_get_my_key();
				v_e_connect_encrypt(decrypted_key, encrypted_key, &other_key[V_ENCRYPTION_LOGIN_PUBLIC_START + i], &other_key[V_ENCRYPTION_LOGIN_N_START + i]);
				for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_HALF_SIZE && my_key[V_ENCRYPTION_LOGIN_PUBLIC_START + i] == decrypted_key[i]; i++);
				if(i < 0) /* Host is not who it appers top be */
				{
					func_connect_deny = v_fs_get_user_func(2);
#if defined(V_PRINT_RECEIVE_COMMANDS)
					printf("receive: verse_send_connect_deny(Host failed identity check); callback = %p\n", func_connect_deny);
#endif
					if(func_connect_deny != NULL)
						func_connect_deny(v_fs_get_user_data(2), "Host failed identity check");
					return;
				}
			}
#endif
		if(stage == V_CS_CONTACTED && V_CS_CONTACTED == v_con_get_connect_stage()) /* reseved by host */
		{
			char *host_id, unpack[V_ENCRYPTION_LOGIN_KEY_SIZE], data[V_ENCRYPTION_LOGIN_KEY_SIZE];
			VNetworkAddress *address;
			verse_send_packet_ack(pack_id);
			address = v_con_get_network_address();
			for(i = 0; i < V_ENCRYPTION_LOGIN_KEY_SIZE; i++)
				buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &data[i]);
			host_id = v_con_get_host_id();
			v_e_connect_encrypt(unpack, data, &host_id[V_ENCRYPTION_LOGIN_PRIVATE_START], &host_id[V_ENCRYPTION_LOGIN_N_START]);
			v_con_set_name_pass(unpack, &unpack[V_ENCRYPTION_LOGIN_KEY_SIZE / 2]);
			v_con_set_connect_stage(V_CS_PENDING_HOST_CALLBACK);
			return; 
		}
	}
	if(cmd_id == 1 && V_CS_PENDING_ACCEPT == v_con_get_connect_stage()) /* reseved by client */
	{
		uint8 *my_key, key[V_ENCRYPTION_DATA_KEY_SIZE], decrypted[V_ENCRYPTION_DATA_KEY_SIZE];
		uint32 avatar;
		verse_send_packet_ack(pack_id);
		buffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &avatar);
		v_con_set_avatar(avatar);
		for(i = 0; i < V_ENCRYPTION_DATA_KEY_SIZE; i++)
			buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &key[i]);
		my_key = v_con_get_my_key();
		v_e_connect_encrypt(decrypted, key, &my_key[V_ENCRYPTION_LOGIN_PRIVATE_START], &my_key[V_ENCRYPTION_LOGIN_N_START]);
		v_con_set_data_key(decrypted);
		v_con_set_connect_stage(V_CS_PENDING_CLIENT_CALLBACK_ACCEPT);
		v_send_hidden_connect_send_key();
		return; 
	}
	if(cmd_id == 2 && V_CS_PENDING_ACCEPT == v_con_get_connect_stage()) /* reseved by client */
	{
		verse_send_packet_ack(pack_id);	
	/*	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], name, 512, buffer_length - buffer_pos);
	*/	v_con_set_connect_stage(V_CS_PENDING_CLIENT_CALLBACK_TERMINATE);
		return; 
	}
}

VSession verse_send_connect_accept(VNodeID avatar, const char *address, uint8 *host_id)
{
	VNetworkAddress a;
#if defined(V_PRINT_SEND_COMMANDS)
	printf("send: verse_send_connect_accept(avatar = %u, address = %s, host_id = NULL);\n", avatar, address);
#endif

	if(!v_n_set_network_address(&a, address))
		return NULL;
	if(v_co_switch_connection(a.ip, a.port))
	{
		if(v_con_get_connect_stage() != V_CS_PENDING_DECISION)
			return NULL;
		v_con_set_avatar(avatar);
		v_con_set_connect_stage(V_CS_CONNECTED);
		v_send_hidden_connect_accept();
		return v_con_get_network_queue();
	}
	return NULL;
}

void v_callback_connect_terminate(const char *bye)
{
	void (* func_connect_terminate)(void *user_data, char *address, const char *bye);
	char address_string[32];

	printf("terminate (%s)\n", bye);
	func_connect_terminate = v_fs_get_user_func(2);
	v_n_get_address_string(v_con_get_network_address(), address_string);
#if defined(V_PRINT_RECEIVE_COMMANDS)
	printf("receive: verse_send_connect_terminate(address = %s, bye = %s); callback = %p\n", address_string, bye, func_connect_terminate);
#endif
	if(func_connect_terminate != 0)
		func_connect_terminate(v_fs_get_user_data(2), address_string, bye);
}

void verse_send_connect_terminate(const char *address, const char *bye)
{
	VNetworkAddress a;
#if defined(V_PRINT_RECEIVE_COMMANDS)
	printf("send: verse_send_connect_terminate(address = %s, bye = %s);\n", address, bye);
#endif

	if(address == NULL)
		v_send_hidden_connect_terminate(v_con_get_network_address(), v_noq_get_next_out_packet_id(v_con_get_network_queue()), bye);
	else if(!v_n_set_network_address(&a, address))
		return;
	else if(v_co_switch_connection(a.ip, a.port))
		v_send_hidden_connect_terminate(v_con_get_network_address(), v_noq_get_next_out_packet_id(v_con_get_network_queue()), bye);

	if(v_con_get_connect_stage() != V_CS_PENDING_DECISION)
		verse_session_destroy(v_con_get_network_queue());
}

void verse_send_ping(const char *address, const char *message)
{
	VNetworkAddress a;
	if(v_n_set_network_address(&a, address))
	{
		unsigned int buffer_pos = 0;
		uint8 buf[1500];
		buffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], 0);/* Packing the Packet id */
		buffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], 5);/* Packing the command */
#if defined V_PRINT_SEND_COMMANDS
		printf("send: verse_send_ping(address = %s text = %s);\n", address, message);
#endif
		buffer_pos += vnp_raw_pack_string(&buf[buffer_pos], message, 1400);
		v_n_send_data(&a, buf, buffer_pos);
	}
#if defined V_PRINT_SEND_COMMANDS
	else
		printf("send: verse_send_ping(address = %s (FAULTY) message = %s);\n", address, message);
#endif
}

unsigned int v_unpack_ping(const char *buf, size_t buffer_length)
{
	unsigned int buffer_pos = 0;
	void (* func_ping)(void *user_data, const char *address, const char *text);
	char address[64];
	char message[1400];

	func_ping = v_fs_get_user_func(5);
	v_n_get_address_string(v_con_get_network_address(), address);
	buffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], message, 1400, buffer_length - buffer_pos);
#if defined V_PRINT_RECEIVE_COMMANDS
	printf("receive: verse_send_ping(address = %s message = %s ); callback = %p\n", address, message, v_fs_get_user_func(5));
#endif
	if(func_ping != NULL)
		func_ping(v_fs_get_user_data(5), address, message);
	return buffer_pos;
}

typedef struct {
	uint32	ip;
	uint16	port;
	char	message[1400];
	void	*next;
} VPingCommand;

static VPingCommand *v_ping_commands = NULL;

boolean v_connect_unpack_ping(const char *buf, size_t buffer_length, uint32 ip, uint16 port)
{
	if(buffer_length > 5)
	{
		unsigned int buffer_pos = 0;
		uint8 cmd_id;
		uint32 pack_id;

		buffer_pos = vnp_raw_unpack_uint32(&buf[buffer_pos], &pack_id);
		buffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &cmd_id);
		if(cmd_id == 5)
		{
			if(NULL != v_fs_get_user_func(5))
			{
				VPingCommand *pc;

				pc = malloc(sizeof *pc);
				pc->ip = ip;
				pc->port = port;
				vnp_raw_unpack_string(&buf[buffer_pos], pc->message,
						      sizeof pc->message, buffer_length - buffer_pos);
				pc->next = v_ping_commands;
				v_ping_commands = pc;
			}
			return TRUE;
		}
	}
	return FALSE;
}

void v_ping_update(void)
{
	VPingCommand *cp;
	void (* func_ping)(void *user_data, const char *address, const char *text);
	VNetworkAddress a;
	char address[64];
	func_ping = v_fs_get_user_func(5);

	while(v_ping_commands != NULL)
	{
		cp = v_ping_commands->next;
		a.ip = v_ping_commands->ip;
		a.port = v_ping_commands->port;
		v_n_get_address_string(&a, address);	
#if defined V_PRINT_RECEIVE_COMMANDS
		printf("receive: verse_send_ping(address = %s message = %s ); callback = %p\n", address, v_ping_commands->message, v_fs_get_user_func(5));
#endif
		if(func_ping != NULL)
			func_ping(v_fs_get_user_data(5), address, v_ping_commands->message);
		free(v_ping_commands);
		v_ping_commands = cp;
	}
}

#endif
