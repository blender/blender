/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_cmd_buf.h"
#include "v_network_in_que.h"
#include "v_network_out_que.h"
#include "v_cmd_gen.h"
#include "v_connection.h"
#include "v_encryption.h"
#include "v_util.h"

#if !defined(V_GENERATE_FUNC_MODE)
#include "verse.h"

#define CONNECTION_CHUNK_SIZE	16
#define V_MAX_CONNECT_PACKET_SIZE	1500
#define V_CON_MAX_MICROSECOND_BETWEEN_SENDS	100
#define V_RE_CONNECTON_TIME_OUT 4
#define V_CONNECTON_TIME_OUT 30

typedef struct {
	VNetOutQueue	*out_queue;
	VNetInQueue		in_queue;
	VNetworkAddress	network_address;
	boolean			connected;
	unsigned int	avatar;
/*	unsigned int	packet_id;*/
	int32			timedelta_s;
	uint32			timedelta_f;
	boolean			destroy_flag;
	void			*ordered_storage;
	char			name[V_ENCRYPTION_LOGIN_KEY_SIZE / 2];
	char			pass[V_ENCRYPTION_LOGIN_KEY_SIZE / 2];
	VConnectStage	connect_stage;
	unsigned int	stage_atempts;
	uint8			key_my[V_ENCRYPTION_LOGIN_KEY_FULL_SIZE];
	uint8			key_other[V_ENCRYPTION_LOGIN_KEY_FULL_SIZE];
	uint8			key_data[V_ENCRYPTION_DATA_KEY_SIZE];
	uint8			*expected_key;
} VConnection;

static struct {
	VConnection		*con;
	unsigned int	con_count;
	unsigned int	current_connection;
	VNetworkAddress	*connect_address;
	void			*unified_func_storage;
	uint16			connect_port;
	unsigned int	pending_packets;
	uint8			host_id[V_ENCRYPTION_LOGIN_KEY_FULL_SIZE];
} VConData;

extern void cmd_buf_init(void);

void v_con_init(void) /* since verse doesnt have an init function this function is runned over an ove ard starts whit a check it it has run before */
{
	static boolean v_con_initialized = FALSE;

	if(v_con_initialized)
		return;
	cmd_buf_init();
	v_con_initialized = TRUE;
	VConData.con = malloc(CONNECTION_CHUNK_SIZE * sizeof *VConData.con);
	memset(VConData.con, 0, CONNECTION_CHUNK_SIZE * sizeof *VConData.con);	/* Clear the memory. */
	VConData.con_count = 0;
	VConData.pending_packets = 0;
/*	v_e_connect_create_key(&VConData.host_id[V_ENCRYPTION_LOGIN_PRIVATE_START],
		       &VConData.host_id[V_ENCRYPTION_LOGIN_PUBLIC_START],
		       &VConData.host_id[V_ENCRYPTION_LOGIN_N_START]);*/ /* default host id if none is set by user */
}

void verse_set_port(uint16 port)
{
	v_n_set_port(port);
}

void verse_host_id_create(uint8 *id)
{
	v_e_connect_create_key(&id[V_ENCRYPTION_LOGIN_PRIVATE_START],
			       &id[V_ENCRYPTION_LOGIN_PUBLIC_START], &id[V_ENCRYPTION_LOGIN_N_START]);
}

void verse_host_id_set(uint8 *id)
{
	memcpy(VConData.host_id, id, V_ENCRYPTION_LOGIN_KEY_FULL_SIZE);
}

extern void *v_fs_create_func_storage(void);
extern void *v_create_ordered_storage(void);
extern void v_destroy_ordered_storage(void *data);

void *v_con_connect(uint32 ip, uint16 port, VConnectStage stage) /* creates a new connection slot */
{
	v_con_init(); /* init connections, if not done yet */
	if((VConData.con_count - 1) % CONNECTION_CHUNK_SIZE == 0) /* do we need more slots for connections, then reallocate more space */
		VConData.con = realloc(VConData.con, (sizeof *VConData.con) * (VConData.con_count + CONNECTION_CHUNK_SIZE));
	VConData.con[VConData.con_count].out_queue = v_noq_create_network_queue(); /* create a out queue fo all out going commands */
	v_niq_clear(&VConData.con[VConData.con_count].in_queue); /* clear and init the que of incomming packets.*/
	VConData.con[VConData.con_count].connected = FALSE; /* not yet propperly connected and should not accept commands yet */
	VConData.con[VConData.con_count].network_address.ip = ip; /* ip address of other side */
	VConData.con[VConData.con_count].network_address.port = port; /* port used by other side */
	VConData.con[VConData.con_count].avatar = 0; /* no avatar set yet*/
/*	VConData.con[VConData.con_count].packet_id = 2;*/
	VConData.con[VConData.con_count].destroy_flag = FALSE; /* this is a flag that is set once the connection is about to be destroyed.*/
	VConData.con[VConData.con_count].ordered_storage = v_create_ordered_storage();
	VConData.con[VConData.con_count].name[0] = 0; /* nouser name set yet */
	VConData.con[VConData.con_count].pass[0] = 0; /* no password set yet */
	VConData.con[VConData.con_count].connect_stage = stage; /* this is the stage of the connection, it show if the connection is ready, the init state depends if this is a client or host */
	VConData.con[VConData.con_count].stage_atempts = 0; /* each stage in the connection prosess is atempted multiple times to avoid failiure if packets get lost*/
	VConData.con[VConData.con_count].timedelta_s = 0; /* number of seconds since last incomming packet to the connection*/
	VConData.con[VConData.con_count].timedelta_f = 0; /* number of fractions of a second since last incomming packet to the connection*/
	VConData.con[VConData.con_count].expected_key = NULL; /* expected hist id if this is a client */
	VConData.current_connection = VConData.con_count; /* set the new connection to the current*/
	++VConData.con_count; /* add one to the number of connections*/
	return VConData.con[VConData.current_connection].out_queue;
}

void verse_session_destroy(VSession session) /* a session can not be destroyed right away, because this function might be called inside a call back from the session it tryes tpo destroy, therfor it only markes it*/
{
	unsigned int i;
	for(i = 0; i < VConData.con_count && VConData.con[i].out_queue != session; i++);
	if(i < VConData.con_count)
	{
		VConData.con[i].destroy_flag = TRUE;
	}
}

void verse_session_set(VSession session) /* find a session and make it the current*/
{
	unsigned int i;
	for(i = 0; i < VConData.con_count && session != VConData.con[i].out_queue; i++);
	if(i < VConData.con_count)
		VConData.current_connection = i;
}

VSession verse_session_get(void)
{
	if(VConData.current_connection < VConData.con_count)
		return VConData.con[VConData.current_connection].out_queue;
	return NULL;
}

uint32 v_co_find_connection(uint32 ip, uint16 port) /* if a packet comes form a ip address what connection does it belong to? */
{
	unsigned int i;

	for(i = 0; i < VConData.con_count; i++)
	{
		if(ip == VConData.con[i].network_address.ip &&
		   port == VConData.con[i].network_address.port &&
		   VConData.con[i].destroy_flag == 0)
		{
			return i;
		}
	}
	return -1;
}

boolean v_co_switch_connection(uint32 ip, uint16 port) /* switches to the current connection to one ip address if it exists */
{
	unsigned int i;
	for(i = 0; i < VConData.con_count; i++)
	{
		if(ip == VConData.con[i].network_address.ip && port == VConData.con[i].network_address.port)
		{
			VConData.current_connection = i;
			return TRUE;
		}
	}
	return FALSE;
}

void v_con_inqueue_timer_update(void)
{
	if(VConData.current_connection < VConData.con_count)
	{
		v_niq_timer_update(&VConData.con[VConData.current_connection].in_queue);
	}
}

/*
extern void	v_fs_buf_unpack(const uint8 *data, unsigned int length);
extern void	v_fs_buf_store_pack(uint8 *data, unsigned int length);
extern boolean	v_fs_buf_unpack_stored(void);
*/
extern void v_unpack_connection(const char *buf, unsigned int buffer_length);

extern void	verse_send_packet_nak(uint32 packet_id);
extern void	v_callback_connect_terminate(const char *bye);
extern boolean	v_connect_unpack_ping(const char *buf, size_t buffer_length, uint32 ip, uint16 port);
extern void	v_ping_update(void);
void v_fs_unpack_beginning(uint8 *data, unsigned int length);

/* Main function that receives and distributes all incoming packets. */
boolean v_con_network_listen(void)
{
	VNetworkAddress address;
	uint8 buf[V_MAX_CONNECT_PACKET_SIZE], *store;
	int size = 0;
	unsigned int connection;
	uint32 packet_id;
	boolean ret = FALSE;

	v_con_init(); /* Init if needed. */
	connection = VConData.current_connection; /* Store current connection in a local variable so that we can restore it later. */
	size = v_n_receive_data(&address, buf, sizeof buf); /* Ask for incoming data from the network. */
	while(size != -1 && size != 0) /* Did we get any data? */
	{
		VConData.current_connection = v_co_find_connection(address.ip, address.port); /* Is there a connection matching the IP and port? */
		vnp_raw_unpack_uint32(buf, &packet_id); /* Unpack the ID of the packet. */
/*		printf("got packet ID %u, %d bytes, connection %u\n", packet_id, size, VConData.current_connection);*/
		if(VConData.current_connection < VConData.con_count &&
		   !(VConData.con[VConData.current_connection].connect_stage == V_CS_CONNECTED && packet_id == 0)) /* If this isn't a packet from an existing connection, disregard it. */
		{
			if(VConData.con[VConData.current_connection].connect_stage == V_CS_CONNECTED) /* Is this connection initialized? */
			{
				store = v_niq_store(&VConData.con[VConData.current_connection].in_queue, size, packet_id); /* Store the packet. */
				if(store != NULL)
				{
					VConData.pending_packets++; /* We now have one more packet pending unpack. */
					v_e_data_decrypt_packet(store, buf, size, VConData.con[VConData.current_connection].key_data); /* Decrypt the packet. */
					v_fs_unpack_beginning(store, size);
				}
			}
			else
			{
				v_unpack_connection(buf, size); /* This is an ongoing connecton-attempt. */
				v_niq_timer_update(&VConData.con[VConData.current_connection].in_queue);
			}
		}
		else if(v_connect_unpack_ping(buf, size, address.ip, address.port))	/* Ping handled. */
			;
		else if(v_fs_func_accept_connections()) /* Do we accept connection-attempts? */
		{
			if(VConData.current_connection >= VConData.con_count ||
			   V_RE_CONNECTON_TIME_OUT < v_niq_time_out(&VConData.con[VConData.current_connection].in_queue)) /* Is it a new client, or an old client that we haven't heard form in some time? */
			{
				if(VConData.current_connection < VConData.con_count)
				{
					VConData.con[VConData.current_connection].network_address.ip = 0;
					VConData.con[VConData.current_connection].destroy_flag = TRUE; /* Destroy old connection if there is one. */
				}
				v_con_connect(address.ip, address.port, V_CS_IDLE); /* Create a new connection. */
				v_unpack_connection(buf, size); /* Unpack the connection-attempt. */
			}
		}
		else
		{
			fprintf(stderr, __FILE__ ": Unhandled packet--dropping\n");
			if(VConData.con_count > 0)
			{
				fprintf(stderr, __FILE__ ": State: connections=%u, current=%u (stage %u), packet_id=%u\n",
					VConData.con_count,
					VConData.current_connection,
					(VConData.current_connection < VConData.con_count) ? VConData.con[VConData.current_connection].connect_stage : 0,
					packet_id);
			}
		}
		size = v_n_receive_data(&address, buf, sizeof buf); /* See if there are more incoming packets. */
		ret = TRUE;
	}
	VConData.current_connection = connection; /* Reset the current connection. */

	return ret;
}

extern void	v_update_connection_pending(boolean resend);

boolean v_con_callback_update(void)
{
	static unsigned int seconds;
	boolean	output = FALSE;
	size_t	size; 
	unsigned int connection, s;
	VNetInPacked	*p;

	v_n_get_current_time(&s, NULL);
	connection = VConData.current_connection;
	for(VConData.current_connection = 0; VConData.current_connection < VConData.con_count; VConData.current_connection++)
		if(VConData.con[VConData.current_connection].connect_stage != V_CS_CONNECTED)
			v_update_connection_pending(s != seconds);
	seconds = s;
	VConData.current_connection = connection;
	if(VConData.pending_packets == 0)
		return FALSE;
	if(VConData.con[VConData.current_connection].connect_stage == V_CS_CONNECTED)
	{
		while((p = v_niq_get(&VConData.con[VConData.current_connection].in_queue, &size)) != NULL)
		{
			VConData.pending_packets--;
			v_fs_unpack(p->data, size);
			v_niq_release(&VConData.con[VConData.current_connection].in_queue, p);
			output = TRUE;
		}
		v_con_network_listen();
	}
	return output;
}

void verse_callback_update(unsigned int microseconds)
{
	unsigned int connection, passed;

	v_ping_update();	/* Deliver any pending pings. */
	connection = VConData.current_connection;
	for(VConData.current_connection = 0; VConData.current_connection < VConData.con_count; VConData.current_connection++)
	{
		if(VConData.con[VConData.current_connection].connect_stage == V_CS_CONNECTED)
			v_noq_send_queue(VConData.con[VConData.current_connection].out_queue, &VConData.con[VConData.current_connection].network_address);
		if(VConData.con[VConData.current_connection].destroy_flag == TRUE)
		{
			v_noq_destroy_network_queue(VConData.con[VConData.current_connection].out_queue);
			VConData.pending_packets -= v_niq_free(&VConData.con[VConData.current_connection].in_queue);
			v_destroy_ordered_storage(VConData.con[VConData.current_connection].ordered_storage);
			if(VConData.con[VConData.current_connection].expected_key != NULL)
				free(VConData.con[VConData.current_connection].expected_key);
			if(VConData.con_count - 1 != VConData.current_connection)
				VConData.con[VConData.current_connection] = VConData.con[VConData.con_count - 1];
			VConData.con_count--;
			if(connection >= VConData.con_count)
				VConData.current_connection = 0;
			return;
		}
	}
	VConData.current_connection = connection;

	if(VConData.con_count > 0)
	{
/*		printf("checking timeout of stage %d connection %u\n",
		       VConData.con[VConData.current_connection].connect_stage, VConData.current_connection);
*/		if(V_CONNECTON_TIME_OUT < v_niq_time_out(&VConData.con[VConData.current_connection].in_queue))
		{
			if(VConData.con[VConData.current_connection].connect_stage != V_CS_CONNECTED)
			{
				VConData.con[VConData.current_connection].destroy_flag = TRUE;
			}
			else
				v_callback_connect_terminate("connection timed out");
		}
	}

	v_con_network_listen();
	if(VConData.con_count > 0)
		if(v_con_callback_update())
			return;
	for(passed = 0; passed < microseconds && VConData.pending_packets == 0;)
	{
		boolean	update;
		if(microseconds - passed > V_CON_MAX_MICROSECOND_BETWEEN_SENDS)	/* Still a long way to go? */
			passed += v_n_wait_for_incoming(V_CON_MAX_MICROSECOND_BETWEEN_SENDS);
		else
			passed += v_n_wait_for_incoming(microseconds - passed);
		do
		{
			update = v_con_network_listen();
			connection = VConData.current_connection;
			for(VConData.current_connection = 0; VConData.current_connection < VConData.con_count; VConData.current_connection++)
			{
				if(VConData.con[VConData.current_connection].connect_stage == V_CS_CONNECTED)
				{
					if(v_noq_send_queue(VConData.con[VConData.current_connection].out_queue, &VConData.con[VConData.current_connection].network_address))
						update = TRUE;
				}
			}
			VConData.current_connection = connection;
		} while(update);
	}
	if(VConData.con_count > 0)
		v_con_callback_update();
}

void v_con_set_name_pass(const char *name, const char *pass)
{
	v_strlcpy(VConData.con[VConData.current_connection].name, name, sizeof VConData.con[VConData.current_connection].name);
	v_strlcpy(VConData.con[VConData.current_connection].pass, pass, sizeof VConData.con[VConData.current_connection].pass);
}

const char * v_con_get_name(void)
{
	return VConData.con[VConData.current_connection].name;
}

const char * v_con_get_pass(void)
{
	return VConData.con[VConData.current_connection].pass;
}

void v_con_set_connect_stage(VConnectStage stage)
{
	VConData.con[VConData.current_connection].connect_stage = stage;
	VConData.con[VConData.current_connection].stage_atempts = 0;
}

VConnectStage v_con_get_connect_stage(void)
{
	return VConData.con[VConData.current_connection].connect_stage;
}

uint8 *v_con_get_my_key(void)
{
	return VConData.con[VConData.current_connection].key_my;
}

uint8 *v_con_get_other_key(void)
{
	return VConData.con[VConData.current_connection].key_other;
}

uint8 **v_con_get_expected_key(void)
{
	return &VConData.con[VConData.current_connection].expected_key;
}

uint8 * v_con_get_host_id(void)
{
	return VConData.host_id;
}

void v_con_set_data_key(const uint8 *key)
{
	memcpy(VConData.con[VConData.current_connection].key_data, key, V_ENCRYPTION_DATA_KEY_SIZE);
}

const uint8 * v_con_get_data_key(void)
{
	return VConData.con[VConData.current_connection].key_data;
}

void * v_con_get_network_queue(void)
{
	return VConData.con[VConData.current_connection].out_queue;
}

VNetworkAddress * v_con_get_network_address(void)
{
	return &VConData.con[VConData.current_connection].network_address;
}

void * v_con_get_ordered_storage(void)
{
	return VConData.con[VConData.current_connection].ordered_storage;
}

void v_con_set_avatar(uint32 avatar)
{
	VConData.con[VConData.current_connection].avatar = avatar;
}

uint32 verse_session_get_avatar(void)
{
	return VConData.con[VConData.current_connection].avatar;
}

void verse_session_get_time(uint32 *seconds, uint32 *fractions)
{
	uint32 s, f;
	v_n_get_current_time(&s, &f);
	if((uint32)~0 - f < VConData.con[VConData.current_connection].timedelta_f)
		s++;
	if(seconds != NULL) 
	{
		if(VConData.con[VConData.current_connection].timedelta_s < 0)
			*seconds = s - (uint32)(-VConData.con[VConData.current_connection].timedelta_s);
		else
			*seconds = s + VConData.con[VConData.current_connection].timedelta_s;
	}
	if(fractions != NULL)
		*fractions = f + VConData.con[VConData.current_connection].timedelta_f;
}

void v_con_set_time(uint32 seconds, uint32 fractions)
{
	uint32 s, f;
	v_n_get_current_time(&s, &f);

	if(f < fractions)
		s--;
	if (s < seconds)
		VConData.con[VConData.current_connection].timedelta_s = -(int)(seconds - s);
	else
		VConData.con[VConData.current_connection].timedelta_s = (int)(s - seconds);
	VConData.con[VConData.current_connection].timedelta_f = f - fractions;
}

#endif
