/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * these all are linked to objects (listbase)
 * all data is 'direct data', not Blender lib data.
 */

/** \file blender/blenkernel/intern/sca.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_controller_types.h"
#include "DNA_sensor_types.h"
#include "DNA_actuator_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_sca.h"

/* ******************* SENSORS ************************ */

void free_sensor(bSensor *sens)
{
	if (sens->links) MEM_freeN(sens->links);
	if (sens->data) MEM_freeN(sens->data);
	MEM_freeN(sens);
	
}

void free_sensors(ListBase *lb)
{
	bSensor *sens;
	
	while ((sens= lb->first)) {
		BLI_remlink(lb, sens);
		free_sensor(sens);
	}
}

bSensor *copy_sensor(bSensor *sens)
{
	bSensor *sensn;
	
	sensn= MEM_dupallocN(sens);
	sensn->flag |= SENS_NEW;
	if (sens->data) {
		sensn->data= MEM_dupallocN(sens->data);
	}

	if (sens->links) sensn->links= MEM_dupallocN(sens->links);
	
	return sensn;
}

void copy_sensors(ListBase *lbn, ListBase *lbo)
{
	bSensor *sens, *sensn;
	
	lbn->first= lbn->last= NULL;
	sens= lbo->first;
	while (sens) {
		sensn= copy_sensor(sens);
		BLI_addtail(lbn, sensn);
		sens= sens->next;
	}
}

void init_sensor(bSensor *sens)
{
	/* also use when sensor changes type */
	bNearSensor *ns;
	bMouseSensor *ms;
	bJoystickSensor *js;
	bRaySensor *rs;
	
	if (sens->data) MEM_freeN(sens->data);
	sens->data= NULL;
	sens->pulse = 0;
	
	switch (sens->type) {
	case SENS_ALWAYS:
		sens->pulse = 0;
		break;
	case SENS_TOUCH:
		sens->data= MEM_callocN(sizeof(bTouchSensor), "touchsens");
		break;
	case SENS_NEAR:
		ns=sens->data= MEM_callocN(sizeof(bNearSensor), "nearsens");
		ns->dist= 1.0;
		ns->resetdist= 2.0;
		break;
	case SENS_KEYBOARD:
		sens->data= MEM_callocN(sizeof(bKeyboardSensor), "keysens");
		break;
	case SENS_PROPERTY:
		sens->data= MEM_callocN(sizeof(bPropertySensor), "propsens");
		break;
	case SENS_ARMATURE:
		sens->data= MEM_callocN(sizeof(bArmatureSensor), "armsens");
		break;
	case SENS_ACTUATOR:
		sens->data= MEM_callocN(sizeof(bActuatorSensor), "actsens");
		break;
	case SENS_DELAY:
		sens->data= MEM_callocN(sizeof(bDelaySensor), "delaysens");
		break;
	case SENS_MOUSE:
		ms=sens->data= MEM_callocN(sizeof(bMouseSensor), "mousesens");
		ms->type= 1; // LEFTMOUSE workaround because Mouse Sensor types enum starts in 1
		break;
	case SENS_COLLISION:
		sens->data= MEM_callocN(sizeof(bCollisionSensor), "colsens");
		break;
	case SENS_RADAR:
		sens->data= MEM_callocN(sizeof(bRadarSensor), "radarsens");
		break;
	case SENS_RANDOM:
		sens->data= MEM_callocN(sizeof(bRandomSensor), "randomsens");
		break;
	case SENS_RAY:
		sens->data= MEM_callocN(sizeof(bRaySensor), "raysens");
		rs = sens->data;
		rs->range = 0.01f;
		break;
	case SENS_MESSAGE:
		sens->data= MEM_callocN(sizeof(bMessageSensor), "messagesens");
		break;
	case SENS_JOYSTICK:
		sens->data= MEM_callocN(sizeof(bJoystickSensor), "joysticksens");
		js= sens->data;
		js->hatf = SENS_JOY_HAT_UP;
		js->axis = 1;
		js->hat = 1;
		break;
	default:
		; /* this is very severe... I cannot make any memory for this        */
		/* logic brick...                                                    */
	}
}

bSensor *new_sensor(int type)
{
	bSensor *sens;

	sens= MEM_callocN(sizeof(bSensor), "Sensor");
	sens->type= type;
	sens->flag= SENS_SHOW;
	
	init_sensor(sens);
	
	strcpy(sens->name, "sensor");
// XXX	make_unique_prop_names(sens->name);
	
	return sens;
}

/* ******************* CONTROLLERS ************************ */

void unlink_controller(bController *cont)
{
	bSensor *sens;
	Object *ob;
	
	/* check for controller pointers in sensors */
	ob= G.main->object.first;
	while (ob) {
		sens= ob->sensors.first;
		while (sens) {
			unlink_logicbricks((void **)&cont, (void ***)&(sens->links), &sens->totlinks);
			sens= sens->next;
		}
		ob= ob->id.next;
	}
}

void unlink_controllers(ListBase *lb)
{
	bController *cont;
	
	for (cont= lb->first; cont; cont= cont->next)
		unlink_controller(cont);	
}

void free_controller(bController *cont)
{
	if (cont->links) MEM_freeN(cont->links);

	/* the controller itself */
	if (cont->data) MEM_freeN(cont->data);
	MEM_freeN(cont);
	
}

void free_controllers(ListBase *lb)
{
	bController *cont;
	
	while ((cont= lb->first)) {
		BLI_remlink(lb, cont);
		if (cont->slinks) MEM_freeN(cont->slinks);
		free_controller(cont);
	}
}

bController *copy_controller(bController *cont)
{
	bController *contn;
	
	cont->mynew=contn= MEM_dupallocN(cont);
	contn->flag |= CONT_NEW;
	if (cont->data) {
		contn->data= MEM_dupallocN(cont->data);
	}

	if (cont->links) contn->links= MEM_dupallocN(cont->links);
	contn->slinks= NULL;
	contn->totslinks= 0;
	
	return contn;
}

void copy_controllers(ListBase *lbn, ListBase *lbo)
{
	bController *cont, *contn;
	
	lbn->first= lbn->last= NULL;
	cont= lbo->first;
	while (cont) {
		contn= copy_controller(cont);
		BLI_addtail(lbn, contn);
		cont= cont->next;
	}
}

void init_controller(bController *cont)
{
	/* also use when controller changes type, leave actuators... */
	
	if (cont->data) MEM_freeN(cont->data);
	cont->data= NULL;
	
	switch (cont->type) {
	case CONT_EXPRESSION:
		cont->data= MEM_callocN(sizeof(bExpressionCont), "expcont");
		break;
	case CONT_PYTHON:
		cont->data= MEM_callocN(sizeof(bPythonCont), "pycont");
		break;
	}
}

bController *new_controller(int type)
{
	bController *cont;

	cont= MEM_callocN(sizeof(bController), "Controller");
	cont->type= type;
	cont->flag= CONT_SHOW;

	init_controller(cont);
	
	strcpy(cont->name, "cont");
// XXX	make_unique_prop_names(cont->name);
	
	return cont;
}

/* ******************* ACTUATORS ************************ */

void unlink_actuator(bActuator *act)
{
	bController *cont;
	Object *ob;
	
	/* check for actuator pointers in controllers */
	ob= G.main->object.first;
	while (ob) {
		cont= ob->controllers.first;
		while (cont) {
			unlink_logicbricks((void **)&act, (void ***)&(cont->links), &cont->totlinks);
			cont= cont->next;
		}
		ob= ob->id.next;
	}
}

void unlink_actuators(ListBase *lb)
{
	bActuator *act;
	
	for (act= lb->first; act; act= act->next)
		unlink_actuator(act);
}

void free_actuator(bActuator *act)
{
	bSoundActuator *sa;

	if (act->data) {
		switch (act->type) {
		case ACT_SOUND:
			sa = (bSoundActuator *) act->data;
			if (sa->sound)
				id_us_min((ID *) sa->sound);
			break;
		}

		MEM_freeN(act->data);
	}
	MEM_freeN(act);
}

void free_actuators(ListBase *lb)
{
	bActuator *act;
	
	while ((act= lb->first)) {
		BLI_remlink(lb, act);
		free_actuator(act);
	}
}

bActuator *copy_actuator(bActuator *act)
{
	bActuator *actn;
	bSoundActuator *sa;
	
	act->mynew=actn= MEM_dupallocN(act);
	actn->flag |= ACT_NEW;
	if (act->data) {
		actn->data= MEM_dupallocN(act->data);
	}
	
	switch (act->type) {
		case ACT_SOUND:
			sa= (bSoundActuator *)act->data;
			if (sa->sound)
				id_us_plus((ID *) sa->sound);
			break;
	}
	return actn;
}

void copy_actuators(ListBase *lbn, ListBase *lbo)
{
	bActuator *act, *actn;
	
	lbn->first= lbn->last= NULL;
	act= lbo->first;
	while (act) {
		actn= copy_actuator(act);
		BLI_addtail(lbn, actn);
		act= act->next;
	}
}

void init_actuator(bActuator *act)
{
	/* also use when actuator changes type */
	bCameraActuator *ca;
	bObjectActuator *oa;
	bRandomActuator *ra;
	bSoundActuator *sa;
	bSteeringActuator *sta;
	bArmatureActuator *arma;
	
	if (act->data) MEM_freeN(act->data);
	act->data= NULL;
	
	switch (act->type) {
	case ACT_ACTION:
	case ACT_SHAPEACTION:
		act->data= MEM_callocN(sizeof(bActionActuator), "actionact");
		break;
	case ACT_SOUND:
		sa = act->data= MEM_callocN(sizeof(bSoundActuator), "soundact");
		sa->volume = 1.0f;
		sa->sound3D.rolloff_factor = 1.0f;
		sa->sound3D.reference_distance = 1.0f;
		sa->sound3D.max_gain = 1.0f;
		sa->sound3D.cone_inner_angle = 360.0f;
		sa->sound3D.cone_outer_angle = 360.0f;
		sa->sound3D.max_distance = FLT_MAX;
		break;
	case ACT_OBJECT:
		act->data= MEM_callocN(sizeof(bObjectActuator), "objectact");
		oa= act->data;
		oa->flag= 15;
		break;
	case ACT_IPO:
		act->data= MEM_callocN(sizeof(bIpoActuator), "ipoact");
		break;
	case ACT_PROPERTY:
		act->data= MEM_callocN(sizeof(bPropertyActuator), "propact");
		break;
	case ACT_CAMERA:
		act->data= MEM_callocN(sizeof(bCameraActuator), "camact");
		ca = act->data;
		ca->axis = OB_POSX;
		ca->damping = 1.0/32.0;
		break;
	case ACT_EDIT_OBJECT:
		act->data= MEM_callocN(sizeof(bEditObjectActuator), "editobact");
		break;
	case ACT_CONSTRAINT:
		act->data= MEM_callocN(sizeof(bConstraintActuator), "cons act");
		break;
	case ACT_SCENE:
		act->data= MEM_callocN(sizeof(bSceneActuator), "scene act");
		break;
	case ACT_GROUP:
		act->data= MEM_callocN(sizeof(bGroupActuator), "group act");
		break;
	case ACT_RANDOM:
		act->data= MEM_callocN(sizeof(bRandomActuator), "random act");
		ra=act->data;
		ra->float_arg_1 = 0.1f;
		break;
	case ACT_MESSAGE:
		act->data= MEM_callocN(sizeof(bMessageActuator), "message act");
		break;
	case ACT_GAME:
		act->data= MEM_callocN(sizeof(bGameActuator), "game act");
		break;
	case ACT_VISIBILITY:
		act->data= MEM_callocN(sizeof(bVisibilityActuator), "visibility act");
		break;
	case ACT_2DFILTER:
		act->data = MEM_callocN(sizeof( bTwoDFilterActuator ), "2d filter act");
		break;
	case ACT_PARENT:
		act->data = MEM_callocN(sizeof( bParentActuator ), "parent act");
		break;
	case ACT_STATE:
		act->data = MEM_callocN(sizeof( bStateActuator ), "state act");
		break;
	case ACT_ARMATURE:
		act->data = MEM_callocN(sizeof( bArmatureActuator ), "armature act");
		arma = act->data;
		arma->influence = 1.f;
		break;
	case ACT_STEERING:
		act->data = MEM_callocN(sizeof( bSteeringActuator), "steering act");
		sta = act->data;
		sta->acceleration = 3.f;
		sta->turnspeed = 120.f;
		sta->dist = 1.f;
		sta->velocity= 3.f;
		sta->flag = ACT_STEERING_AUTOMATICFACING;
		sta->facingaxis = 1;
		break;
	default:
		; /* this is very severe... I cannot make any memory for this        */
		/* logic brick...                                                    */
	}
}

bActuator *new_actuator(int type)
{
	bActuator *act;

	act= MEM_callocN(sizeof(bActuator), "Actuator");
	act->type= type;
	act->flag= ACT_SHOW;
	
	init_actuator(act);
	
	strcpy(act->name, "act");
// XXX	make_unique_prop_names(act->name);
	
	return act;
}

/* ******************** GENERAL ******************* */
void clear_sca_new_poins_ob(Object *ob)
{
	bSensor *sens;
	bController *cont;
	bActuator *act;
	
	sens= ob->sensors.first;
	while (sens) {
		sens->flag &= ~SENS_NEW;
		sens= sens->next;
	}
	cont= ob->controllers.first;
	while (cont) {
		cont->mynew= NULL;
		cont->flag &= ~CONT_NEW;
		cont= cont->next;
	}
	act= ob->actuators.first;
	while (act) {
		act->mynew= NULL;
		act->flag &= ~ACT_NEW;
		act= act->next;
	}
}

void clear_sca_new_poins(void)
{
	Object *ob;
	
	ob= G.main->object.first;
	while (ob) {
		clear_sca_new_poins_ob(ob);
		ob= ob->id.next;	
	}
}

void set_sca_new_poins_ob(Object *ob)
{
	bSensor *sens;
	bController *cont;
	bActuator *act;
	int a;
	
	sens= ob->sensors.first;
	while (sens) {
		if (sens->flag & SENS_NEW) {
			for (a=0; a<sens->totlinks; a++) {
				if (sens->links[a] && sens->links[a]->mynew)
					sens->links[a]= sens->links[a]->mynew;
			}
		}
		sens= sens->next;
	}

	cont= ob->controllers.first;
	while (cont) {
		if (cont->flag & CONT_NEW) {
			for (a=0; a<cont->totlinks; a++) {
				if ( cont->links[a] && cont->links[a]->mynew)
					cont->links[a]= cont->links[a]->mynew;
			}
		}
		cont= cont->next;
	}
	
	
	act= ob->actuators.first;
	while (act) {
		if (act->flag & ACT_NEW) {
			if (act->type==ACT_EDIT_OBJECT) {
				bEditObjectActuator *eoa= act->data;
				ID_NEW(eoa->ob);
			}
			else if (act->type==ACT_SCENE) {
				bSceneActuator *sca= act->data;
				ID_NEW(sca->camera);
			}
			else if (act->type==ACT_CAMERA) {
				bCameraActuator *ca= act->data;
				ID_NEW(ca->ob);
			}
			else if (act->type==ACT_OBJECT) {
				bObjectActuator *oa= act->data;
				ID_NEW(oa->reference);
			}
			else if (act->type==ACT_MESSAGE) {
				bMessageActuator *ma= act->data;
				ID_NEW(ma->toObject);
			}
			else if (act->type==ACT_PARENT) {
				bParentActuator *para = act->data;
				ID_NEW(para->ob);
			}
			else if (act->type==ACT_ARMATURE) {
				bArmatureActuator *aa = act->data;
				ID_NEW(aa->target);
				ID_NEW(aa->subtarget);
			}
			else if (act->type==ACT_PROPERTY) {
				bPropertyActuator *pa= act->data;
				ID_NEW(pa->ob);
			}
			else if (act->type==ACT_STEERING) {
				bSteeringActuator *sta = act->data;
				ID_NEW(sta->navmesh);
				ID_NEW(sta->target);
			}
		}
		act= act->next;
	}
}


void set_sca_new_poins(void)
{
	Object *ob;
	
	ob= G.main->object.first;
	while (ob) {
		set_sca_new_poins_ob(ob);
		ob= ob->id.next;	
	}
}

void sca_remove_ob_poin(Object *obt, Object *ob)
{
	bSensor *sens;
	bMessageSensor *ms;
	bActuator *act;
	bCameraActuator *ca;
	bObjectActuator *oa;
	bSceneActuator *sa;
	bEditObjectActuator *eoa;
	bPropertyActuator *pa;
	bMessageActuator *ma;
	bParentActuator *para;
	bArmatureActuator *aa;
	bSteeringActuator *sta;


	sens= obt->sensors.first;
	while (sens) {
		switch (sens->type) {
		case SENS_MESSAGE:
			ms= sens->data;
			if (ms->fromObject==ob) ms->fromObject= NULL;
		}
		sens= sens->next;
	}

	act= obt->actuators.first;
	while (act) {
		switch (act->type) {
		case ACT_CAMERA:
			ca= act->data;
			if (ca->ob==ob) ca->ob= NULL;
			break;
		case ACT_OBJECT:
			oa= act->data;
			if (oa->reference==ob) oa->reference= NULL;
			break;
		case ACT_PROPERTY:
			pa= act->data;
			if (pa->ob==ob) pa->ob= NULL;
			break;
		case ACT_SCENE:
			sa= act->data;
			if (sa->camera==ob) sa->camera= NULL;
			break;
		case ACT_EDIT_OBJECT:
			eoa= act->data;
			if (eoa->ob==ob) eoa->ob= NULL;
			break;
		case ACT_MESSAGE:
			ma= act->data;
			if (ma->toObject==ob) ma->toObject= NULL;
			break;
		case ACT_PARENT:
			para = act->data;
			if (para->ob==ob) para->ob = NULL;
			break;
		case ACT_ARMATURE:
			aa = act->data;
			if (aa->target == ob) aa->target = NULL;
			if (aa->subtarget == ob) aa->subtarget = NULL;
			break;
		case ACT_STEERING:
			sta = act->data;
			if (sta->navmesh == ob) sta->navmesh = NULL;
			if (sta->target == ob) sta->target = NULL;
		}
		act= act->next;
	}	
}

/* ******************** INTERFACE ******************* */
void sca_move_sensor(bSensor *sens_to_move, Object *ob, int move_up)
{
	bSensor *sens, *tmp;

	int val;
	val = move_up ? 1:2;

	/* make sure this sensor belongs to this object */
	sens= ob->sensors.first;
	while (sens) {
		if (sens == sens_to_move) break;
		sens= sens->next;
	}
	if (!sens) return;

	/* move up */
	if ( val==1 && sens->prev) {
		for (tmp=sens->prev; tmp; tmp=tmp->prev) {
			if (tmp->flag & SENS_VISIBLE)
				break;
		}
		if (tmp) {
			BLI_remlink(&ob->sensors, sens);
			BLI_insertlinkbefore(&ob->sensors, tmp, sens);
		}
	}
	/* move down */
	else if ( val==2 && sens->next) {
		for (tmp=sens->next; tmp; tmp=tmp->next) {
			if (tmp->flag & SENS_VISIBLE)
				break;
		}
		if (tmp) {
			BLI_remlink(&ob->sensors, sens);
			BLI_insertlink(&ob->sensors, tmp, sens);
		}
	}
}

void sca_move_controller(bController *cont_to_move, Object *ob, int move_up)
{
	bController *cont, *tmp;

	int val;
	val = move_up ? 1:2;

	/* make sure this controller belongs to this object */
	cont= ob->controllers.first;
	while (cont) {
		if (cont == cont_to_move) break;
		cont= cont->next;
	}
	if (!cont) return;

	/* move up */
	if ( val==1 && cont->prev) {
		/* locate the controller that has the same state mask but is earlier in the list */
		tmp = cont->prev;
		while (tmp) {
			if (tmp->state_mask & cont->state_mask) 
				break;
			tmp = tmp->prev;
		}
		if (tmp) {
			BLI_remlink(&ob->controllers, cont);
			BLI_insertlinkbefore(&ob->controllers, tmp, cont);
		}
	}

	/* move down */
	else if ( val==2 && cont->next) {
		tmp = cont->next;
		while (tmp) {
			if (tmp->state_mask & cont->state_mask) 
				break;
			tmp = tmp->next;
		}
		BLI_remlink(&ob->controllers, cont);
		BLI_insertlink(&ob->controllers, tmp, cont);
	}
}

void sca_move_actuator(bActuator *act_to_move, Object *ob, int move_up)
{
	bActuator *act, *tmp;
	int val;

	val = move_up ? 1:2;

	/* make sure this actuator belongs to this object */
	act= ob->actuators.first;
	while (act) {
		if (act == act_to_move) break;
		act= act->next;
	}
	if (!act) return;

	/* move up */
	if ( val==1 && act->prev) {
		/* locate the first visible actuators before this one */
		for (tmp = act->prev; tmp; tmp=tmp->prev) {
			if (tmp->flag & ACT_VISIBLE)
				break;
		}
		if (tmp) {
			BLI_remlink(&ob->actuators, act);
			BLI_insertlinkbefore(&ob->actuators, tmp, act);
		}
	}
	/* move down */
	else if ( val==2 && act->next) {
		/* locate the first visible actuators after this one */
		for (tmp=act->next; tmp; tmp=tmp->next) {
			if (tmp->flag & ACT_VISIBLE)
				break;
		}
		if (tmp) {
			BLI_remlink(&ob->actuators, act);
			BLI_insertlink(&ob->actuators, tmp, act);
		}
	}
}

void link_logicbricks(void **poin, void ***ppoin, short *tot, short size)
{
	void **old_links= NULL;
	
	int ibrick;

	/* check if the bricks are already linked */
	for (ibrick=0; ibrick < *tot; ibrick++) {
		if ((*ppoin)[ibrick] == *poin)
			return;
	}

	if (*ppoin) {
		old_links= *ppoin;

		(*tot) ++;
		*ppoin = MEM_callocN((*tot)*size, "new link");
	
		for (ibrick=0; ibrick < *(tot) - 1; ibrick++) {
			(*ppoin)[ibrick] = old_links[ibrick];
		}
		(*ppoin)[ibrick] = *poin;

		if (old_links) MEM_freeN(old_links);
	}
	else {
		(*tot) = 1;
		*ppoin = MEM_callocN((*tot)*size, "new link");
		(*ppoin)[0] = *poin;
	}
}

void unlink_logicbricks(void **poin, void ***ppoin, short *tot)
{
	int ibrick, removed;

	removed= 0;
	for (ibrick=0; ibrick < *tot; ibrick++) {
		if (removed) (*ppoin)[ibrick - removed] = (*ppoin)[ibrick];
		else if ((*ppoin)[ibrick] == *poin) removed = 1;
	}

	if (removed) {
		(*tot) --;

		if (*tot == 0) {
			MEM_freeN(*ppoin);
			(*ppoin)= NULL;
		}
		return;
	}
}
