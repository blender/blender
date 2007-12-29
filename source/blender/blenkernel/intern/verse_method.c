/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * Contributor(s): Nathan Letwory. 
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef WITH_VERSE

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"
#include "DNA_text_types.h"

#include "BLI_dynamiclist.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BIF_verse.h"

#include "BKE_bad_level_calls.h"
#include "BKE_library.h"
#include "BKE_text.h"
#include "BKE_verse.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "verse.h"

/* helper struct for creating method descriptions */
typedef struct VMethodInfo {
	const char *name;
	uint8 param_count;
	const VNOParamType param_type[4];
	const char *param_name[4];
	uint16 id;
} VMethodInfo;

#ifdef VERSECHAT
/* array with methods for verse chat */
static VMethodInfo vmethod_info[] = {
	{ "join", 1, { VN_O_METHOD_PTYPE_STRING }, { "channel"}},
	{ "leave", 1, { VN_O_METHOD_PTYPE_STRING }, { "channel"}},
	{ "hear", 3, { VN_O_METHOD_PTYPE_STRING, VN_O_METHOD_PTYPE_STRING, VN_O_METHOD_PTYPE_STRING }, { "channel", "from", "msg"}}
};
#endif

/* lookup a method group based on its name */
struct VMethodGroup *lookup_vmethodgroup_name(ListBase *lb, const char *name) {
	struct VMethodGroup *vmg;

	for(vmg= lb->first; vmg; vmg= vmg->next)
		if(strcmp(vmg->name,name)==0) break;
	
	return vmg;
}

/* lookup a method group based on its group_id */
struct VMethodGroup *lookup_vmethodgroup(ListBase *lb, uint16 group_id) {
	struct VMethodGroup *vmg;

	for(vmg= lb->first; vmg; vmg= vmg->next)
		if(vmg->group_id==group_id) break;
	
	return vmg;
}

/* lookup a method based on its name */
struct VMethod *lookup_vmethod_name(ListBase *lb, const char *name) {
	struct VMethod *vm;
	for(vm= lb->first; vm; vm= vm->next)
		if(strcmp(vm->name,name)==0) break;

	return vm;
}

/* lookup a method based on its method_id */
struct VMethod *lookup_vmethod(ListBase *lb, uint8 method_id) {
	struct VMethod *vm;
	for(vm= lb->first; vm; vm= vm->next)
		if(vm->id==method_id) break;

	return vm;
}

#ifdef VERSECHAT
/*
 * send say command
 */
void send_say(const char *chan, const char *utter)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	VNOPackedParams *utterpack;
	VNOParam args[2];
	
	vnode= (VNode *)(session->nodes.lb.first);

	for( ; vnode; vnode= vnode->next) {
		if(strcmp(vnode->name, "tawksrv")==0) {
			vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk");
			if(!vmg) break;
			vm= lookup_vmethod_name(&(vmg->methods), "say");
			if(!vm) break;
			args[0].vstring= (char *)chan;
			args[1].vstring= (char *)utter;
			if((utterpack= verse_method_call_pack(vm->param_count, vm->param_type, args))!=NULL) {
				verse_send_o_method_call(vnode->id, vmg->group_id, vm->id, vnode->session->avatar, utterpack);
			}
			break;
		}

	}
}

/*
 * send logout command
 */
void send_logout(VNode *vnode)
{
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	VNOPackedParams *pack;

	vnode->chat_flag = CHAT_LOGGED;
	vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk");
	if(!vmg) return;
	vm= lookup_vmethod_name(&(vmg->methods), "logout");
	if(!vm) return;

	if((pack= verse_method_call_pack(vm->param_count, vm->param_type, NULL))!=NULL) {
		verse_send_o_method_call(vnode->id, vmg->group_id, vm->id, vnode->session->avatar, pack);
	}
	vnode->chat_flag = CHAT_NOTLOGGED;
}

/*
 * send join command
 */
void send_join(VNode *vnode, const char *chan)
{
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	VNOPackedParams *join;
	VNOParam channel[1];

	vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk");
	if(!vmg) return;
	vm= lookup_vmethod_name(&(vmg->methods), "join");
	if(!vm) return;

	channel[0].vstring= (char *)chan;
	if((join= verse_method_call_pack(vm->param_count, vm->param_type, channel))!=NULL) {
		verse_send_o_method_call(vnode->id, vmg->group_id, vm->id, vnode->session->avatar, join);
	}
}

/*
 * send leave command
 */
void send_leave(VNode *vnode, const char *chan)
{
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	VNOPackedParams *leave;
	VNOParam channel[1];

	vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk");
	if(!vmg) return;
	vm= lookup_vmethod_name(&(vmg->methods), "leave");
	if(!vm) return;

	channel[0].vstring= (char *)chan;
	if((leave= verse_method_call_pack(vm->param_count, vm->param_type, channel))!=NULL) {
		verse_send_o_method_call(vnode->id, vmg->group_id, vm->id, vnode->session->avatar, leave);
	}
}

/*
 * send login command
 */
void send_login(VNode *vnode)
{
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	VNOPackedParams *login;
	VNOParam param[1];

	vnode->chat_flag = CHAT_LOGGED;
	vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk");
	if(!vmg) return;
	vm= lookup_vmethod_name(&(vmg->methods), "login");
	if(!vm) return;

	param[0].vstring= U.verseuser;

	if((login= verse_method_call_pack(vm->param_count, vm->param_type, param))!=NULL) {
		verse_send_o_method_call(vnode->id, vmg->group_id, vm->id, vnode->session->avatar, login);
	}
	vnode->chat_flag = CHAT_LOGGED;

	vnode= lookup_vnode(vnode->session, vnode->session->avatar);
	vmg= lookup_vmethodgroup_name(&(vnode->methodgroups), "tawk-client");
	if(!vmg)
		verse_send_o_method_group_create(vnode->session->avatar, ~0, "tawk-client");
}
#endif

/*
 * Free a VMethod
 */
void free_verse_method(VMethod *vm) {
	if(!vm) return;

	MEM_freeN(vm->param_type);
}

/*
 * Free methods for VMethodGroup
 */
void free_verse_methodgroup(VMethodGroup *vmg)
{
	struct VMethod *vm, *tmpvm;

	if(!vmg) return;

	vm= vmg->methods.first;
	while(vm) {
		tmpvm=vm->next;
		free_verse_method(vm);
		vm= tmpvm;
	}
	BLI_freelistN(&(vmg->methods));
}

/* callback for method group creation */
static void cb_o_method_group_create(
		void *user_data,
		VNodeID node_id,
		uint16 group_id,
		const char *name)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;

	if(!session) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	vmg = lookup_vmethodgroup(&(vnode->methodgroups), group_id);
	
	/* create method group holder in node node_id */
	if(!vmg) {
		vmg= MEM_mallocN(sizeof(VMethodGroup), "VMethodGroup");
		vmg->group_id = group_id;
		vmg->methods.first = vmg->methods.last = NULL;
		BLI_addtail(&(vnode->methodgroups), vmg);
		printf("new method group with name %s (group_id %d) for node %u created\n", name, group_id, node_id);
	}

	/* this ensures name of an existing group gets updated, in case it is changed */
	BLI_strncpy(vmg->name, (char *)name, 16);

	/* subscribe to method group */
	verse_send_o_method_group_subscribe(node_id, group_id);

#ifdef VERSECHAT
	/* if this is our own method group, register our methods */
	if(node_id==session->avatar) {
		verse_send_o_method_create(node_id, group_id, (uint8)~0u, vmethod_info[0].name,
				vmethod_info[0].param_count,
				(VNOParamType *)vmethod_info[0].param_type,
				(const char **)vmethod_info[0].param_name);
		b_verse_update();
		verse_send_o_method_create(node_id, group_id, (uint8)~0u, vmethod_info[1].name,
				vmethod_info[1].param_count,
				(VNOParamType *)vmethod_info[1].param_type,
				(const char **)vmethod_info[1].param_name);
		b_verse_update();
		verse_send_o_method_create(node_id, group_id, (uint8)~0u, vmethod_info[2].name,
				vmethod_info[2].param_count,
				(VNOParamType *)vmethod_info[2].param_type,
				(const char **)vmethod_info[2].param_name);
		b_verse_update();
	}
#endif
}

/* callback for method group destruction */
static void cb_o_method_group_destroy(
		void *user_data,
		VNodeID node_id,
		uint16 group_id,
		const char *name)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;
	struct VMethod *vm;

	printf("method group %d destroyed\n", group_id);

	if(!session) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	for(vmg= vnode->methodgroups.first; vmg; vmg= vmg->next)
		if(vmg->group_id==group_id) break;

	if(!vmg) return; /* method group doesn't exist? */

	vmg->group_id = 0;
	vmg->name[0] = '\0';
	vm= vmg->methods.first;
	while(vm) {
		/* free vm */
		
	}

	/* TODO: unsubscribe from method group */
	BLI_remlink(&(vnode->methodgroups),vmg);
	MEM_freeN(vmg);
}

/* callback for method creation */
static void cb_o_method_create(
		void *user_data,
		VNodeID node_id,
		uint16 group_id,
		uint16 method_id,
		const char *name,
		uint8 param_count,
		const VNOParamType *param_type,
		const char *param_name[])
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	unsigned int size;
	unsigned int i;
	char *put;

	if(!session) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);

	vmg= lookup_vmethodgroup((&vnode->methodgroups), group_id);

	if(!vmg) return;

	vm= lookup_vmethod((&vmg->methods), method_id);

	if(!vm) {
		vm= MEM_mallocN(sizeof(VMethod), "VMethod");
		vm->id= method_id;
		vm->param_count= param_count;
		size= param_count* (sizeof(*vm->param_type) + sizeof(*vm->param_name));
		for(i= 0; i <param_count; i++) {
			size+=strlen(param_name[i])+1;
		}
		vm->param_type= MEM_mallocN(size, "param_type and param_name");
		memcpy(vm->param_type, param_type, sizeof(VNOParamType)*param_count);
		vm->param_name= (char **)(vm->param_type + param_count);
		put= (char *)(vm->param_name + param_count);
		for(i= 0; i < param_count; i++) {
			vm->param_name[i]= put;
			strcpy(put, param_name[i]);
			put += strlen(param_name[i]) + 1;
		}

		BLI_addtail(&(vmg->methods), vm);
#ifdef VERSECHAT
		if(strcmp(vmethod_info[0].name, name)==0) {
			vmethod_info[0].id = method_id;
		}
#endif
		printf("method %s in group %d of node %u created\n", name, group_id, node_id);
	}

	BLI_strncpy(vm->name, (char *)name, 500);
}

/* callback for method destruction */
static void cb_o_method_destroy(
		void *user_data,
		VNodeID node_id,
		uint16 group_id,
		uint16 method_id,
		const char *name,
		uint8 param_count,
		const VNOParamType *param_type,
		const char *param_name[])
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;
	struct VMethod *vm;

	if(!session) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	for(vmg= vnode->methodgroups.first; vmg; vmg= vmg->next)
		if(vmg->group_id==group_id) break;

	if(!vmg) return; /* method group doesn't exist? */

	for(vm= vmg->methods.first; vm; vm= vm->next)
		if(vm->id==method_id) break;

	if(!vm) return;

	BLI_remlink(&(vmg->methods), vm);
	MEM_freeN(vm->param_type);
	MEM_freeN(vm);
}

/* callback for method calls */
static void cb_o_method_call(void *user_data, VNodeID node_id, uint8 group_id, uint8 method_id, VNodeID sender, VNOPackedParams *params)
{
	struct VerseSession *session = (VerseSession*)current_verse_session();
	struct VNode *vnode;
	struct VMethodGroup *vmg;
	struct VMethod *vm;
	Text *text;
	int method_idx= -1;

	VNOParam arg[3];

	if(!session) return;

	if(session->avatar!=node_id) return;

	vnode = BLI_dlist_find_link(&(session->nodes), (unsigned int)node_id);
	vmg= lookup_vmethodgroup(&(vnode->methodgroups), group_id);
	if(!vmg) return;

	vm= lookup_vmethod(&(vmg->methods), method_id);
	if(!vm) return;
#ifdef VERSECHAT
	if(strcmp(vm->name, "join")==0) method_idx=0;
	if(strcmp(vm->name, "leave")==0) method_idx=1;
	if(strcmp(vm->name, "hear")==0) method_idx=2;
	if(method_idx>-1)
		verse_method_call_unpack(params, vmethod_info[method_idx].param_count, vmethod_info[method_idx].param_type, arg);

	switch(method_idx) {
		case 0:
			printf("Joining channel %s\n",arg[0].vstring);
			text=add_empty_text();
			text->flags |= TXT_ISCHAT;
			rename_id(&(text->id), arg[0].vstring);
			break;
		case 1:
			printf("Leaving channel %s\n",arg[0].vstring);
			break;
		case 2:
			{
				ListBase lb = G.main->text;
				ID *id= (ID *)lb.first;
				char showstr[1024];
				showstr[0]='\0';
				text = NULL;
				sprintf(showstr, "%s: %s\n", arg[1].vstring, arg[2].vstring);
				for(; id; id= id->next) {
					if(strcmp(id->name+2, arg[0].vstring)==0 && strcmp(arg[0].vstring, "#server")!=0) {
						text = (Text *)id;
						break;
					}
				}
				if(text) {
					txt_insert_buf(text, showstr);
					txt_move_eof(text, 0);
// XXX					allqueue(REDRAWCHAT, 0);
				} else {
					printf("%s> %s: %s\n",arg[0].vstring, arg[1].vstring, arg[2].vstring);
				}
			}
			break;
	}
#endif
}

void set_method_callbacks(void)
{
	/* create and destroy method groups */
	verse_callback_set(verse_send_o_method_group_create, cb_o_method_group_create, NULL);
	verse_callback_set(verse_send_o_method_group_destroy, cb_o_method_group_destroy, NULL);

	/* create and destroy methods */
	verse_callback_set(verse_send_o_method_create, cb_o_method_create, NULL);
	verse_callback_set(verse_send_o_method_destroy, cb_o_method_destroy, NULL);

	/* call methods */
	verse_callback_set(verse_send_o_method_call, cb_o_method_call, NULL);
}

#endif
