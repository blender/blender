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
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "mydevice.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BKE_global.h"
#include "BKE_blender.h"

#include "BIF_verse.h"
#include "BIF_space.h"
#include "BIF_interface.h"

extern ListBase session_list;
extern ListBase server_list;

/*
 * this function creates popup menu with all active VerseSessions
 * it return pointer at selected VerseSession, if no VerseSession
 * is selected, then NULL is returned
 */
VerseSession *session_menu(void)
{
	struct VerseSession *session;
	char session_number[10];
	short i=1, num=1;
	char session_address_list[1024];	/* pupmenu business */

	session_number[0] = '\0';
	session_address_list[0] = '\0';

	strcat(session_address_list, "Session list %t");

	session = session_list.first;

	while(session){
		strcat(session_address_list, "| ");
		strcat(session_address_list, session->address);
		strcat(session_address_list, " %x");
		sprintf(session_number, "%d", num);
		strcat(session_address_list, session_number);
		num++;
		session = session->next;
	}

	printf("session list: %s\n", session_address_list);
	num = pupmenu(session_address_list);

	if(num==-1) return NULL;

	session = session_list.first;

	while(session) {
		if(i==num) return session;
		i++;
		session = session->next;
	}

	return NULL;
}

/*
 * returns name of verse client (it is used as avatar's name)
 */
char *verse_client_name(void)
{
	char *client_name;
	char blender_version[5];
	short name_lenght = 14;

#ifndef WIN32
	char *hostname;
	hostname = getenv("HOSTNAME");
	if(hostname) name_lenght += strlen(hostname);
#endif

	client_name = (char*)MEM_mallocN(sizeof(char)*name_lenght, "verse client name");
	client_name[0] = '\0';

	strcat(client_name, "blender_");
	blender_version[0] = '\0';
	sprintf(blender_version, "%d", BLENDER_VERSION);
	strcat(client_name, blender_version);

#ifndef WIN32
	/* add at the end of the client name hostname */
	if(hostname) {
		strcat(client_name, ":");
		strcat(client_name, hostname);
	}
#endif

	return client_name;
}

/*===========================================================
 *
 *   functions executed after calling callback functions
 *
 ============================================================*/

/*
 * this function is called, when some tag was changed or new tag was created
 */
void post_tag_change(VTag *vtag)
{
	printf("\tnew tag %s was created or changed\n", vtag->name);
}

/*
 * this function is called, when verse taggroup was created
 */
void post_taggroup_create(VTagGroup *vtaggroup)
{
	printf("\tnew taggroup %s was created\n", vtaggroup->name);
}

/*
 * this function is called after creating of new VerseNode
 */
void post_node_create(VNode *vnode)
{
	struct VerseSession *session = vnode->session;

	if((session->flag & VERSE_AUTOSUBSCRIBE) && (vnode->owner_id != VN_OWNER_MINE)) {
		if(vnode->type == V_NT_OBJECT) {
			create_object_from_verse_node(vnode);
		}
		else if(vnode->type == V_NT_GEOMETRY) {
			create_mesh_from_geom_node(vnode);;
		}
	}

	allqueue(REDRAWOOPS, 0);
}

/*
 * this function is called after destroying of VerseNode
 */
void post_node_destroy(VNode *vnode)
{
	allqueue(REDRAWOOPS, 0);

	/* TODO: destroy bindings between vnode and blender data structures */
}

/*
 * this function is calles after renaming of VerseNode by verse_server
 */
void post_node_name_set(VNode *vnode)
{
	/* if VerseNode has coresponding blender data structure, then
	 * change ID name of data structure */
	if(vnode->type==V_NT_OBJECT) {
		struct Object *ob;
		ob = (Object*)((VObjectData*)vnode->data)->object;
		if(ob) {
			char *str;
			str = (char*)malloc(sizeof(char)*(strlen(vnode->name)+3));
			str[0] = '\0';
			strcat(str, "OB");
			strcat(str, vnode->name);
			strncpy(ob->id.name, str, 23);
			printf("\tob->id.name: %s\n", ob->id.name);
			free(str);
		}
	}
	else if(vnode->type==V_NT_GEOMETRY) {
		struct Mesh *me;

		me = (Mesh*)((VGeomData*)vnode->data)->mesh;
		if(me) {
			char *str;
			str = (char*)malloc(sizeof(char)*(strlen(vnode->name)+3));
			str[0] = '\0';
			strcat(str, "ME");
			strcat(str, vnode->name);
			strncpy(me->id.name, str, 23);
			printf("\tme->id.name: %s\n", me->id.name);
			free(str);
		}
	}

	allqueue(REDRAWALL, 0);
}

/*
 * this function is called after acception connection with verse server
 */
void post_connect_accept(VerseSession *session)
{
	VerseServer *server;
	
	G.f |= G_VERSE_CONNECTED;

	session->counter = 0;

	server = server_list.first;
	while(server) {
		if(strcmp(server->ip, session->address)==0) {
			server->flag = session->flag;
			break;
		}
		server = server->next;
	}

	allqueue(REDRAWOOPS, 0);
}

void post_server_add(void)
{
	allqueue(REDRAWOOPS, 0);
}

/*
 * this function is called, when connestion with verse server is ended/terminated/etc.
 */
void post_connect_terminated(VerseSession *session)
{
	VerseServer *server;
	server = server_list.first;
	while(server) {
		if(strcmp(server->ip, session->address)==0) {
			server->flag = 0;
			server->session=NULL;
			break;
		}
		server = server->next;
	}

	/* if it is last session, then no other will exist ... set Global flag */
	if((session->prev==NULL) && (session->next==NULL))
		G.f &= ~G_VERSE_CONNECTED;

	allqueue(REDRAWOOPS, 0);
}

/*
 * if connection wasn't accepted, then free VerseSession
 * and print warning message with popupmenu
 */
void post_connect_update(VerseSession *session)
{
	if(session->flag & VERSE_CONNECTING) {
		session->counter++;
		if(session->counter > MAX_UNCONNECTED_EVENTS) {
			char *str;
			/* popup menu*/
			str = malloc(sizeof(char)*(strlen(session->address)+35));
			str[0]='\0';
			strcat(str, "Error%t|No response from server: ");
			strcat(str, session->address);
			pupmenu(str);
			free(str);

			session->flag = 0;
			session->counter = 0;
			session->post_connect_terminated(session);
			free_verse_session(session);
		}
	}
}

#endif

