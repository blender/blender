/*
** A simple Verse server.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "v_cmd_gen.h"

#if !defined V_GENERATE_FUNC_MODE

#include "verse.h"
#include "v_network.h"
#include "v_util.h"
#include "vs_server.h"

extern VNodeID	vs_node_create(VNodeID owner_id, unsigned int type);
extern void	callback_send_node_destroy(void *user_data, VNodeID node_id);
extern void	vs_reset_owner(VNodeID owner_id);

static void callback_send_connect(void *user, const char *name, const char *pass, const char *address, const uint8 *host_id)
{
	VNodeID avatar;
	VSession *session;

	printf("Connecting '%s'\n", name);
	if(TRUE)
	{
		avatar = vs_node_create(~0, V_NT_OBJECT);
		session = verse_send_connect_accept(avatar, address, NULL);
		vs_add_new_connection(session, name, pass, avatar);
/*		vs_avatar_init(avatar, name);*/
	}
	else
	{
		verse_send_connect_terminate(address, "I'm sorry but you are not welcome here.");
	}
}

static void callback_send_connect_terminate(void *user, char *address, char *bye)
{
	printf("callback_send_connect_terminate\n");
	vs_reset_owner(vs_get_avatar());
	callback_send_node_destroy(NULL, vs_get_avatar());
	verse_session_destroy(vs_get_session());
	vs_remove_connection();
}

static void vs_load_host_id(const char *file_name)
{
	FILE	*f;
	uint8	id[V_HOST_ID_SIZE];
	size_t	got;

	/* Attempt to read key from given filename. Fails silently. */
	if((f = fopen(file_name, "rb")) != NULL)
	{
		if((got = fread(id, 1, sizeof id, f)) > 0)
		{
			printf("Loaded %u-bit host ID key successfully\n", 8 * (got / 3));
			verse_host_id_set(id);
		}
		fclose(f);
		if(got)
			return;
	}
	/* If file didn't open, or reading failed, generate a new key and write it out. */
	verse_host_id_create(id);
	verse_host_id_set(id);
	if((f = fopen(file_name, "wb")) != NULL)
	{
		if(fwrite(id, sizeof id, 1, f) != 1)
			fprintf(stderr, "Warning: Couldn't write host ID to \"%s\"\n", file_name);
		fclose(f);
	}
	else
		fprintf(stderr, "Warning: Couldn't open \"%s\" for host ID writing\n", file_name);
}

static void cb_sigint_handler(int sig)
{
	if(sig == SIGINT)
	{
		printf("Verse server terminating\n");
		exit(EXIT_SUCCESS);
	}
}

static void callback_send_ping(void *user, const char *address, const char *message)
{
	if(strncmp(message, "DESCRIBE", 8) == 0 && message[8] == ' ')
		vs_master_handle_describe(address, message + 9);
}

static void usage(void)
{
	printf("Verse server usage:\n");
	printf(" -h\t\t\tShow this usage information.\n");
	printf(" -ms\t\t\tRegisters the server with a master server at the address\n");
	printf(" \t\t\tgiven with the -ms:ip= option. Off by default.\n");
	printf(" -ms:ip=IP[:PORT]\tSet master server to register with. Implies -ms.\n");
	printf(" \t\t\tThe default address is <%s>.\n", vs_master_get_address());
	printf(" -ms:de=DESC\t\tSet description, sent to master server.\n");
	printf(" -ms:ta=TAGS\t\tSet tags, sent to master server.\n");
	printf(" -port=PORT\t\tSet port to use for incoming connections.\n");
	printf(" -version\t\tPrint version information and exit.\n");
}

int main(int argc, char **argv)
{
	uint32		i, seconds, fractions, port = VERSE_STD_CONNECT_PORT;

	signal(SIGINT, cb_sigint_handler);

	vs_master_set_address("master.uni-verse.org");		/* The default master address. */
	vs_master_set_enabled(FALSE);				/* Make sure master server support is disabled. */
	for(i = 1; i < (uint32) argc; i++)
	{
		if(strcmp(argv[i], "-h") == 0)
		{
			usage();
			return EXIT_SUCCESS;
		}
		else if(strcmp(argv[i], "-ms") == 0)
			vs_master_set_enabled(TRUE);
                else if(strncmp(argv[i], "-ms:ip=", 7) == 0)
		{
                        vs_master_set_address(argv[i] + 7);
			vs_master_set_enabled(TRUE);
		}
                else if(strncmp(argv[i], "-ms:de=", 7) == 0)
                        vs_master_set_desc(argv[i] + 7);
                else if(strncmp(argv[i], "-ms:ta=", 7) == 0)
			vs_master_set_tags(argv[i] + 7);
		else if(strncmp(argv[i], "-port=", 6) == 0)
			port = strtoul(argv[i] + 6, NULL, 0);
		else if(strcmp(argv[i], "-version") == 0)
		{
			printf("r%up%u%s\n", V_RELEASE_NUMBER, V_RELEASE_PATCH, V_RELEASE_LABEL);
			return EXIT_SUCCESS;
		}
		else
			fprintf(stderr, "Ignoring unknown argument \"%s\", try -h for help\n", argv[i]);
	}

	printf("Verse Server r%up%u%s by Eskil Steenberg <http://verse.blender.org/>\n", V_RELEASE_NUMBER, V_RELEASE_PATCH, V_RELEASE_LABEL);
	verse_set_port(port);	/* The Verse standard port. */
	printf(" Listening on port %d\n", port);

	/* Seed the random number generator. Still rather too weak for crypto, I guess. */
	v_n_get_current_time(&seconds, &fractions);
	srand(seconds ^ fractions);

	vs_load_host_id("host_id.rsa");
	vs_init_node_storage();
	vs_o_callback_init();
	vs_g_callback_init();
	vs_m_callback_init();
	vs_b_callback_init();
	vs_t_callback_init();
	vs_c_callback_init();
	vs_a_callback_init();
	vs_h_callback_init();
	init_callback_node_storage();
	verse_callback_set(verse_send_ping,		callback_send_ping, NULL);
	verse_callback_set(verse_send_connect,		callback_send_connect,		NULL);
	verse_callback_set(verse_send_connect_terminate, callback_send_connect_terminate, NULL);

	while(TRUE)
	{
		vs_set_next_session();
		verse_callback_update(1000000);
		vs_master_update();
	}
	return EXIT_SUCCESS;
}

#endif		/* V_GENERATE_FUNC_MODE */
