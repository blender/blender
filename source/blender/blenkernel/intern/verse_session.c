/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Jiri Hnidek.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_VERSE

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"	/* temp */
#include "DNA_listBase.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_dynamiclist.h"
#include "BLI_blenlib.h"

//XXX #include "BIF_screen.h"
//XXX #include "BIF_verse.h"

#include "BKE_global.h"	
#include "BKE_verse.h"

struct ListBase session_list={NULL, NULL};
struct ListBase server_list={NULL, NULL};

static int cb_ping_registered = 0;

/* list of static function prototypes */
static void cb_connect_terminate(const char *address, const char *bye);
static void cb_connect_accept(void *user_data, uint32 avatar, void *address, void *connection, const uint8 *host_id);
static void set_all_callbacks(void);
static void free_verse_session_data(struct VerseSession *session);
static void add_verse_server(VMSServer *server);
static void check_connection_state(struct VerseServer *server);

static void check_connection_state(struct VerseServer *server)
{
	struct VerseSession *session;
	session = session_list.first;
	while(session) {
		if(strcmp(server->ip,session->address)==0) {
			server->flag = session->flag;
			return;
		}
		session = session->next;
	}
}
/*
 * add verse server to server_list. Prevents duplicate
 * entries
 */
static void add_verse_server(VMSServer *server)
{
	struct VerseServer *iter, *niter;
	VerseServer *newserver;
	const char *name = verse_ms_field_value(server, "DE");
	iter = server_list.first;

	while(iter) {
		niter = iter->next;
		if(strcmp(iter->ip, server->ip)==0) {
			return;
		}
		iter = niter;
	}

	newserver = (VerseServer *)MEM_mallocN(sizeof(VerseServer), "VerseServer");
	newserver->ip = (char *)MEM_mallocN(sizeof(char)*(strlen(server->ip)+1), "VerseServer ip");
	strcpy(newserver->ip, server->ip);

	if(name) {
		newserver->name = (char *)MEM_mallocN(sizeof(char)*(strlen(name)+strlen(newserver->ip)+4), "VerseServer name");
		strcpy(newserver->name, name);
		strcat(newserver->name, " (");
		strcat(newserver->name, newserver->ip);
		strcat(newserver->name, ")");
	}

	newserver->flag = 0;
	check_connection_state(newserver);

	printf("Adding new verse server: %s at %s\n", newserver->name, newserver->ip);

	BLI_addtail(&server_list, newserver);
	//XXX post_server_add();
}

/*
 * callback function for ping
 */
static void cb_ping(void *user, const char *address, const char *message)
{
	VMSServer	**servers = verse_ms_list_parse(message);
	if(servers != NULL)
	{
		int	i;

		for(i = 0; servers[i] != NULL; i++)
			add_verse_server(servers[i]);

		free(servers);
	}
}

/*
 * callback function for connection terminated
 */
static void cb_connect_terminate(const char *address, const char *bye)
{
	VerseSession *session = (VerseSession*)current_verse_session();

	if(!session) return;

	/* remove session from list of session */
	BLI_remlink(&session_list, session);
	/* do post connect operations */
	session->post_connect_terminated(session);
	/* free session data */
	free_verse_session_data(session);
	/* free session */
	MEM_freeN(session);
}

/*
 * callback function for accepted connection to verse server
 */
static void cb_connect_accept(
		void *user_data,
		uint32 avatar,
		void *address,
		void *connection,
		const uint8 *host_id)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VerseServer *server = server_list.first;
	uint32 i, mask=0;

	if(!session) return;

	session->flag |= VERSE_CONNECTED;
	session->flag &= ~VERSE_CONNECTING;

	while(server) {
		if(strcmp(session->address, server->ip)==0) {
			server->flag |= VERSE_CONNECTED;
			server->flag &= ~VERSE_CONNECTING;
			server->session = session;
			break;
		}
		server = server->next;
	}

	printf("\tBlender is connected to verse server: %s\n", (char*)address);
	printf("\tVerseSession->counter: %d\n", session->counter);

	session->avatar = avatar;

	session->post_connect_accept(session);

	for(i = 0; i < V_NT_NUM_TYPES; i++)
		mask = mask | (1 << i);
	verse_send_node_index_subscribe(mask);
	verse_send_node_subscribe(session->avatar); /* subscribe to avatar node, as well */

	/* create our own method group and method */
	/*verse_send_o_method_group_create(session->avatar, ~0, "tawk-client");*/
}

/*
 * set up all callbacks for sessions
 */
void set_verse_session_callbacks(void)
{
	/* connection */
	verse_callback_set(verse_send_connect_accept, cb_connect_accept, NULL);
	/* connection was terminated */
	verse_callback_set(verse_send_connect_terminate, cb_connect_terminate, NULL);

}

/*
 * set all callbacks used in Blender
 */
static void set_all_callbacks(void)
{
	/* set up all callbacks for sessions */
	set_verse_session_callbacks();

	/* set up callbacks for nodes */
	set_node_callbacks();

	/* set up all callbacks for object nodes */
	set_object_callbacks();

	/* set up all callbacks for geometry nodes */
	set_geometry_callbacks();

	/* set up all callbacks for bitmap nodes */
	set_bitmap_callbacks();
	
	/* set up all callbacks for method groups and methods */
	set_method_callbacks();
}

/*
 * this function sends and receive all packets for all sessions
 */
void b_verse_update(void)
{
	VerseSession *session, *next_session;

	session = session_list.first;
	while(session){
		next_session = session->next;
		verse_session_set(session->vsession);
		if((session->flag & VERSE_CONNECTED) || (session->flag & VERSE_CONNECTING)) {
			verse_callback_update(10);
			session->post_connect_update(session);
		}
		session = next_session;
	}
	if(cb_ping_registered>0) {
			verse_callback_update(10);
	}
}

/*
 * returns VerseSession coresponding to vsession pointer
 */
VerseSession *versesession_from_vsession(VSession *vsession)
{
	struct VerseSession *session;

	session = session_list.first;

	while(session) {
		if(session->vsession==vsession) return session;
		session = session->next;
	}
	
	return session;
}

/*
 * returns pointer at current VerseSession
 */
VerseSession *current_verse_session(void)
{
	struct VerseSession *session;
	VSession vsession = verse_session_get();

	session = session_list.first;

	while(session){
		if(session->vsession == vsession)
			return session;
		session = session->next;
	}

	printf("error: non-existing SESSION occured!\n");
	return NULL;
}

/*
 * free VerseSession
 */
static void free_verse_session_data(VerseSession *session)
{
	struct VNode *vnode;

	/* free data of all nodes */
	vnode = session->nodes.lb.first;
	while(vnode){
		free_verse_node_data(vnode);
		vnode = vnode->next;
	}

	/* free data of nodes waiting in queue */
	vnode = session->queue.first;
	while(vnode){
		free_verse_node_data(vnode);
		vnode = vnode->next;
	}

	/* free all VerseNodes */
	BLI_dlist_destroy(&(session->nodes));
	/* free all VerseNodes waiting in queque */
	BLI_freelistN(&(session->queue));

	/* free name of verse host for this session */
	MEM_freeN(session->address);
}

/*
 * free VerseSession
 */
void free_verse_session(VerseSession *session)
{
	/* remove session from session list*/
	BLI_remlink(&session_list, session);
	/* do post terminated operations */
	session->post_connect_terminated(session);
	/* free session data (nodes, layers) */
	free_verse_session_data(session);
	/* free session */
	MEM_freeN(session);
}

/*
 * create new verse session and return coresponding data structure
 */
VerseSession *create_verse_session(
		const char *name,
		const char *pass,
		const char *address,
		uint8 *expected_key)
{
	struct VerseSession *session;
	VSession *vsession;
	
	vsession = verse_send_connect(name, pass, address, expected_key);

	if(!vsession) return NULL;

	session = (VerseSession*)MEM_mallocN(sizeof(VerseSession), "VerseSession");

	session->flag = VERSE_CONNECTING;

	session->vsession = vsession;
	session->avatar = -1;

	session->address = (char*)MEM_mallocN(sizeof(char)*(strlen(address)+1),"session adress name");
	strcpy(session->address, address);

	session->connection = NULL;
	session->host_id = NULL;
	session->counter = 0;

	/* initialize dynamic list of nodes and node queue */
	BLI_dlist_init(&(session->nodes));
	session->queue.first = session->queue.last = NULL;

	/* set up all client dependent functions */
	//XXX session->post_connect_accept = post_connect_accept;
	//XXX session->post_connect_terminated = post_connect_terminated;
	//XXX session->post_connect_update = post_connect_update;

	//XXX post_server_add();

	return session;
}

/*
 * end verse session and free all session data
 */
void end_verse_session(VerseSession *session)
{
	/* send terminate command to verse server */
	verse_send_connect_terminate(session->address, "blender: bye bye");
	/* update callbacks */
	verse_callback_update(1000);
	/* send destroy session command to verse server */
	verse_session_destroy(session->vsession);
	/* set up flag of verse session */
	session->flag &= ~VERSE_CONNECTED;
	/* do post connect operations */
	session->post_connect_terminated(session);
	/* free structure of verse session */
	free_verse_session(session);
}

void free_all_servers(void)
{
	VerseServer *server, *nextserver;

	server = server_list.first;

	while(server) {
		nextserver = server->next;
		BLI_remlink(&server_list, server);
		MEM_freeN(server->name);
		MEM_freeN(server->ip);
		MEM_freeN(server);
		server = nextserver;
	}
	
	BLI_freelistN(&server_list);
}

/*
 * end connection to all verse hosts (servers) ... free all VerseSessions
 * free all VerseServers
 */
void end_all_verse_sessions(void)
{
	VerseSession *session,*nextsession;

	session = session_list.first;

	while(session) {
		nextsession= session->next;
		end_verse_session(session);
		/* end next session */
		session = nextsession;
	}

	BLI_freelistN(&session_list);

	free_all_servers();
}

/*
 * do a get from ms
 */
void b_verse_ms_get(void)
{
	if(cb_ping_registered==0) {
		/* handle ping messages (for master server) */
		verse_callback_set(verse_send_ping, cb_ping, NULL);
		//XXX add_screenhandler(G.curscreen, SCREEN_HANDLER_VERSE, 1);
		cb_ping_registered++;
	}
	free_all_servers();

	verse_ms_get_send(U.versemaster, VERSE_MS_FIELD_DESCRIPTION, NULL);
	verse_callback_update(10);
}

/*
 * connect to verse host, set up all callbacks, create session
 */
void b_verse_connect(char *address)
{
	VerseSession *session = NULL;

	/* if no session was created before, then set up all callbacks */
	if((session_list.first==NULL) && (session_list.last==NULL))
		set_all_callbacks();

	/* create new session */
	if(address)
		session = create_verse_session("Blender", "pass", address, NULL);

	if(session) {
		/* add new session to the list of sessions */
		BLI_addtail(&session_list, session);

		/* add verse handler if this is first session */
		if(session_list.first == session_list.last) {
			//XXX add_screenhandler(G.curscreen, SCREEN_HANDLER_VERSE, 1);
		}

	}
}

#endif
