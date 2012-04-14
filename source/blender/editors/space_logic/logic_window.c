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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_logic/logic_window.c
 *  \ingroup splogic
 */


#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <float.h>

#include "DNA_actuator_types.h"
#include "DNA_controller_types.h"
#include "DNA_property_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sensor_types.h"
#include "DNA_constraint_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_sca.h"

#include "ED_util.h"

#include "WM_types.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "RNA_access.h"

/* XXX BAD BAD */
#include "../interface/interface_intern.h"

#include "logic_intern.h"


#define MAX_RENDER_PASS   100
#define B_REDR		1
#define B_IDNAME	2

#define B_ADD_SENS		2703
#define B_CHANGE_SENS		2704
#define B_DEL_SENS		2705

#define B_ADD_CONT		2706
#define B_CHANGE_CONT		2707
#define B_DEL_CONT		2708

#define B_ADD_ACT		2709
#define B_CHANGE_ACT		2710
#define B_DEL_ACT		2711

#define B_SOUNDACT_BROWSE	2712

#define B_SETSECTOR		2713
#define B_SETPROP		2714
#define B_SETACTOR		2715
#define B_SETMAINACTOR		2716
#define B_SETDYNA		2717
#define B_SET_STATE_BIT	2718
#define B_INIT_STATE_BIT	2719

/* proto */
static ID **get_selected_and_linked_obs(bContext *C, short *count, short scavisflag);

static int vergname(const void *v1, const void *v2)
{
	char **x1, **x2;
	
	x1= (char **)v1;
	x2= (char **)v2;
	
	return strcmp(*x1, *x2);
}

void make_unique_prop_names(bContext *C, char *str)
{
	Object *ob;
	bProperty *prop;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	ID **idar;
	short a, obcount, propcount=0, nr;
	char **names;
	
	/* this function is called by a Button, and gives the current
	 * stringpointer as an argument, this is the one that can change
	 */
	
	idar= get_selected_and_linked_obs(C, &obcount, BUTS_SENS_SEL|BUTS_SENS_ACT|BUTS_ACT_SEL|BUTS_ACT_ACT|BUTS_CONT_SEL|BUTS_CONT_ACT);
	
	/* for each object, make properties and sca names unique */
	
	/* count total names */
	for (a=0; a<obcount; a++) {
		ob= (Object *)idar[a];
		propcount+= BLI_countlist(&ob->prop);
		propcount+= BLI_countlist(&ob->sensors);
		propcount+= BLI_countlist(&ob->controllers);
		propcount+= BLI_countlist(&ob->actuators);
	}	
	if (propcount==0) {
		if (idar) MEM_freeN(idar);
		return;
	}
	
	/* make names array for sorting */
	names= MEM_callocN(propcount*sizeof(void *), "names");
	
	/* count total names */
	nr= 0;
	for (a=0; a<obcount; a++) {
		ob= (Object *)idar[a];
		prop= ob->prop.first;
		while (prop) {
			names[nr++]= prop->name;
			prop= prop->next;
		}
		sens= ob->sensors.first;
		while (sens) {
			names[nr++]= sens->name;
			sens= sens->next;
		}
		cont= ob->controllers.first;
		while (cont) {
			names[nr++]= cont->name;
			cont= cont->next;
		}
		act= ob->actuators.first;
		while (act) {
			names[nr++]= act->name;
			act= act->next;
		}
	}
	
	qsort(names, propcount, sizeof(void *), vergname);
	
	/* now we check for double names, and change them */
	
	for (nr=0; nr<propcount; nr++) {
		if (names[nr]!=str && strcmp( names[nr], str )==0 ) {
			BLI_newname(str, +1);
		}
	}
	
	MEM_freeN(idar);
	MEM_freeN(names);
}

static void make_unique_prop_names_cb(bContext *C, void *strv, void *UNUSED(redraw_view3d_flagv))
{
	char *str= strv;
//	int redraw_view3d_flag= GET_INT_FROM_POINTER(redraw_view3d_flagv);
	
	make_unique_prop_names(C, str);
}


static void old_sca_move_sensor(bContext *C, void *datav, void *move_up)
{
	/* deprecated, no longer using it (moved to sca.c) */
	Scene *scene= CTX_data_scene(C);
	bSensor *sens_to_delete= datav;
	int val;
	Base *base;
	bSensor *sens, *tmp;
	
	// val= pupmenu("Move up%x1|Move down %x2");
	val = move_up ? 1:2;
	
	if (val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while (base) {
		
			sens= base->object->sensors.first;
			while (sens) {
				if (sens == sens_to_delete) break;
				sens= sens->next;
			}
			
			if (sens) {
				if ( val==1 && sens->prev) {
					for (tmp=sens->prev; tmp; tmp=tmp->prev) {
						if (tmp->flag & SENS_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->sensors, sens);
						BLI_insertlinkbefore(&base->object->sensors, tmp, sens);
					}
				}
				else if ( val==2 && sens->next) {
					for (tmp=sens->next; tmp; tmp=tmp->next) {
						if (tmp->flag & SENS_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->sensors, sens);
						BLI_insertlink(&base->object->sensors, tmp, sens);
					}
				}
				ED_undo_push(C, "Move sensor");
				break;
			}
			
			base= base->next;
		}
	}
}

static void old_sca_move_controller(bContext *C, void *datav, void *move_up)
{
	/* deprecated, no longer using it (moved to sca.c) */
	Scene *scene= CTX_data_scene(C);
	bController *controller_to_del= datav;
	int val;
	Base *base;
	bController *cont, *tmp;
	
	//val= pupmenu("Move up%x1|Move down %x2");
	val = move_up ? 1:2;
	
	if (val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while (base) {
		
			cont= base->object->controllers.first;
			while (cont) {
				if (cont == controller_to_del) break;
				cont= cont->next;
			}
			
			if (cont) {
				if ( val==1 && cont->prev) {
					/* locate the controller that has the same state mask but is earlier in the list */
					tmp = cont->prev;
					while (tmp) {
						if (tmp->state_mask & cont->state_mask)
							break;
						tmp = tmp->prev;
					}
					if (tmp) {
						BLI_remlink(&base->object->controllers, cont);
						BLI_insertlinkbefore(&base->object->controllers, tmp, cont);
					}
				}
				else if ( val==2 && cont->next) {
					tmp = cont->next;
					while (tmp) {
						if (tmp->state_mask & cont->state_mask)
							break;
						tmp = tmp->next;
					}
					BLI_remlink(&base->object->controllers, cont);
					BLI_insertlink(&base->object->controllers, tmp, cont);
				}
				ED_undo_push(C, "Move controller");
				break;
			}
			
			base= base->next;
		}
	}
}

static void old_sca_move_actuator(bContext *C, void *datav, void *move_up)
{
	/* deprecated, no longer using it (moved to sca.c) */
	Scene *scene= CTX_data_scene(C);
	bActuator *actuator_to_move= datav;
	int val;
	Base *base;
	bActuator *act, *tmp;
	
	//val= pupmenu("Move up%x1|Move down %x2");
	val = move_up ? 1:2;
	
	if (val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while (base) {
		
			act= base->object->actuators.first;
			while (act) {
				if (act == actuator_to_move) break;
				act= act->next;
			}
			
			if (act) {
				if ( val==1 && act->prev) {
					/* locate the first visible actuators before this one */
					for (tmp = act->prev; tmp; tmp=tmp->prev) {
						if (tmp->flag & ACT_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->actuators, act);
						BLI_insertlinkbefore(&base->object->actuators, tmp, act);
					}
				}
				else if ( val==2 && act->next) {
					for (tmp=act->next; tmp; tmp=tmp->next) {
						if (tmp->flag & ACT_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->actuators, act);
						BLI_insertlink(&base->object->actuators, tmp, act);
					}
				}
				ED_undo_push(C, "Move actuator");
				break;
			}
			
			base= base->next;
		}
	}
}

static void do_logic_buts(bContext *C, void *UNUSED(arg), int event)
{
	Main *bmain= CTX_data_main(C);
	bSensor *sens;
	bController *cont;
	bActuator *act;
	Object *ob;
	int didit, bit;
	
	ob= CTX_data_active_object(C);
	if (ob==NULL) return;
	
	switch(event) {

	case B_SETPROP:
		/* check for inconsistent types */
		ob->gameflag &= ~(OB_SECTOR|OB_MAINACTOR|OB_DYNAMIC|OB_ACTOR);
		break;

	case B_SETACTOR:
	case B_SETDYNA:
	case B_SETMAINACTOR:
		ob->gameflag &= ~(OB_SECTOR|OB_PROP);
		break;
	
	case B_ADD_SENS:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			if (ob->scaflag & OB_ADDSENS) {
				ob->scaflag &= ~OB_ADDSENS;
				sens= new_sensor(SENS_ALWAYS);
				BLI_addtail(&(ob->sensors), sens);
				make_unique_prop_names(C, sens->name);
				ob->scaflag |= OB_SHOWSENS;
			}
		}
		
		ED_undo_push(C, "Add sensor");
		break;

	case B_CHANGE_SENS:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			sens= ob->sensors.first;
			while (sens) {
				if (sens->type != sens->otype) {
					init_sensor(sens);
					sens->otype= sens->type;
					break;
				}
				sens= sens->next;
			}
		}
		break;
	
	case B_DEL_SENS:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			sens= ob->sensors.first;
			while (sens) {
				if (sens->flag & SENS_DEL) {
					BLI_remlink(&(ob->sensors), sens);
					free_sensor(sens);
					break;
				}
				sens= sens->next;
			}
		}
		ED_undo_push(C, "Delete sensor");
		break;
	
	case B_ADD_CONT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			if (ob->scaflag & OB_ADDCONT) {
				ob->scaflag &= ~OB_ADDCONT;
				cont= new_controller(CONT_LOGIC_AND);
				make_unique_prop_names(C, cont->name);
				ob->scaflag |= OB_SHOWCONT;
				BLI_addtail(&(ob->controllers), cont);
				/* set the controller state mask from the current object state.
				 * A controller is always in a single state, so select the lowest bit set
				 * from the object state */
				for (bit=0; bit<32; bit++) {
					if (ob->state & (1<<bit))
						break;
				}
				cont->state_mask = (1<<bit);
				if (cont->state_mask == 0) {
					/* shouldn't happen, object state is never 0 */
					cont->state_mask = 1;
				}
			}
		}
		ED_undo_push(C, "Add controller");
		break;

	case B_SET_STATE_BIT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			if (ob->scaflag & OB_ALLSTATE) {
				ob->scaflag &= ~OB_ALLSTATE;
				ob->state = 0x3FFFFFFF;
			}
		}
		break;

	case B_INIT_STATE_BIT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			if (ob->scaflag & OB_INITSTBIT) {
				ob->scaflag &= ~OB_INITSTBIT;
				ob->state = ob->init_state;
				if (!ob->state)
					ob->state = 1;
			}
		}
		break;

	case B_CHANGE_CONT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			cont= ob->controllers.first;
			while (cont) {
				if (cont->type != cont->otype) {
					init_controller(cont);
					cont->otype= cont->type;
					break;
				}
				cont= cont->next;
			}
		}
		break;
	

	case B_DEL_CONT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			cont= ob->controllers.first;
			while (cont) {
				if (cont->flag & CONT_DEL) {
					BLI_remlink(&(ob->controllers), cont);
					unlink_controller(cont);
					free_controller(cont);
					break;
				}
				cont= cont->next;
			}
		}
		ED_undo_push(C, "Delete controller");
		break;

	case B_ADD_ACT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			if (ob->scaflag & OB_ADDACT) {
				ob->scaflag &= ~OB_ADDACT;
				act= new_actuator(ACT_OBJECT);
				make_unique_prop_names(C, act->name);
				BLI_addtail(&(ob->actuators), act);
				ob->scaflag |= OB_SHOWACT;
			}
		}
		ED_undo_push(C, "Add actuator");
		break;

	case B_CHANGE_ACT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while (act) {
				if (act->type != act->otype) {
					init_actuator(act);
					act->otype= act->type;
					break;
				}
				act= act->next;
			}
		}
		break;

	case B_DEL_ACT:
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while (act) {
				if (act->flag & ACT_DEL) {
					BLI_remlink(&(ob->actuators), act);
					unlink_actuator(act);
					free_actuator(act);
					break;
				}
				act= act->next;
			}
		}
		ED_undo_push(C, "Delete actuator");
		break;
	
	case B_SOUNDACT_BROWSE:
		/* since we don't know which... */
		didit= 0;
		for (ob=bmain->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while (act)
			{
				if (act->type==ACT_SOUND)
				{
					bSoundActuator *sa= act->data;
					if (sa->sndnr)
					{
						ID *sound= bmain->sound.first;
						int nr= 1;

						if (sa->sndnr == -2) {
// XXX							activate_databrowse((ID *)bmain->sound.first, ID_SO, 0, B_SOUNDACT_BROWSE,
//											&sa->sndnr, do_logic_buts);
							break;
						}

						while (sound)
						{
							if (nr==sa->sndnr)
								break;
							nr++;
							sound= sound->next;
						}
						
						if (sa->sound)
							((ID *)sa->sound)->us--;
						
						sa->sound= (struct bSound *)sound;
						
						if (sound)
							sound->us++;
						
						sa->sndnr= 0;
						didit= 1;
					}
				}
				act= act->next;
			}
			if (didit)
				break;
		}

		break;
	}
}


static const char *sensor_name(int type)
{
	switch (type) {
	case SENS_ALWAYS:
		return "Always";
	case SENS_TOUCH:
		return "Touch";
	case SENS_NEAR:
		return "Near";
	case SENS_KEYBOARD:
		return "Keyboard";
	case SENS_PROPERTY:
		return "Property";
	case SENS_ARMATURE:
		return "Armature";
	case SENS_ACTUATOR:
		return "Actuator";
	case SENS_DELAY:
		return "Delay";
	case SENS_MOUSE:
		return "Mouse";
	case SENS_COLLISION:
		return "Collision";
	case SENS_RADAR:
		return "Radar";
	case SENS_RANDOM:
		return "Random";
	case SENS_RAY:
		return "Ray";
	case SENS_MESSAGE:
		return "Message";
	case SENS_JOYSTICK:
		return "Joystick";
	}
	return "unknown";
}

static const char *sensor_pup(void)
{
	/* the number needs to match defines in DNA_sensor_types.h */
	return "Sensors %t|Always %x0|Delay %x13|Keyboard %x3|Mouse %x5|"
		"Touch %x1|Collision %x6|Near %x2|Radar %x7|"
		"Property %x4|Random %x8|Ray %x9|Message %x10|Joystick %x11|Actuator %x12|Armature %x14";
}

static const char *controller_name(int type)
{
	switch (type) {
	case CONT_LOGIC_AND:
		return "And";
	case CONT_LOGIC_OR:
		return "Or";
	case CONT_LOGIC_NAND:
		return "Nand";
	case CONT_LOGIC_NOR:
		return "Nor";
	case CONT_LOGIC_XOR:
		return "Xor";
	case CONT_LOGIC_XNOR:
		return "Xnor";
	case CONT_EXPRESSION:
		return "Expression";
	case CONT_PYTHON:
		return "Python";
	}
	return "unknown";
}

static const char *controller_pup(void)
{
	return "Controllers   %t|AND %x0|OR %x1|XOR %x6|NAND %x4|NOR %x5|XNOR %x7|Expression %x2|Python %x3";
}

static const char *actuator_name(int type)
{
	switch (type) {
	case ACT_SHAPEACTION:
		return "Shape Action";
	case ACT_ACTION:
		return "Action";
	case ACT_OBJECT:
		return "Motion";
	case ACT_IPO:
		return "F-Curve";
	case ACT_LAMP:
		return "Lamp";
	case ACT_CAMERA:
		return "Camera";
	case ACT_MATERIAL:
		return "Material";
	case ACT_SOUND:
		return "Sound";
	case ACT_PROPERTY:
		return "Property";
	case ACT_EDIT_OBJECT:
		return "Edit Object";
	case ACT_CONSTRAINT:
		return "Constraint";
	case ACT_SCENE:
		return "Scene";
	case ACT_GROUP:
		return "Group";
	case ACT_RANDOM:
		return "Random";
	case ACT_MESSAGE:
		return "Message";
	case ACT_GAME:
		return "Game";
	case ACT_VISIBILITY:
		return "Visibility";
	case ACT_2DFILTER:
		return "Filter 2D";
	case ACT_PARENT:
		return "Parent";
	case ACT_STATE:
		return "State";
	case ACT_ARMATURE:
		return "Armature";
	case ACT_STEERING:
		return "Steering";		
	}
	return "unknown";
}




static const char *actuator_pup(Object *owner)
{
	switch (owner->type)
	{
	case OB_ARMATURE:
		return "Actuators  %t|Action %x15|Armature %x23|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
						"|Scene %x11|Random %x13|Message %x14|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
		break;

	case OB_MESH:
		return "Actuators  %t|Shape Action %x21|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
						"|Scene %x11|Random %x13|Message %x14|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
		break;

	default:
		return "Actuators  %t|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
						"|Scene %x11|Random %x13|Message %x14|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
	}
}



static void set_sca_ob(Object *ob)
{
	bController *cont;
	bActuator *act;

	cont= ob->controllers.first;
	while (cont) {
		cont->mynew= (bController *)ob;
		cont= cont->next;
	}
	act= ob->actuators.first;
	while (act) {
		act->mynew= (bActuator *)ob;
		act= act->next;
	}
}

static ID **get_selected_and_linked_obs(bContext *C, short *count, short scavisflag)
{
	Base *base;
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob, *obt, *obact= CTX_data_active_object(C);
	ID **idar;
	bSensor *sens;
	bController *cont;
	unsigned int lay;
	int a, nr, doit;
	
	/* we need a sorted object list */
	/* set scavisflags flags in Objects to indicate these should be evaluated */
	/* also hide ob pointers in ->new entries of controllerss/actuators */
	
	*count= 0;
	
	if (scene==NULL) return NULL;
	
	ob= bmain->object.first;
	while (ob) {
		ob->scavisflag= 0;
		set_sca_ob(ob);
		ob= ob->id.next;
	}
	
	/* XXX here it checked 3d lay */
	lay= scene->lay;
	
	base= FIRSTBASE;
	while (base) {
		if (base->lay & lay) {
			if (base->flag & SELECT) {
				if (scavisflag & BUTS_SENS_SEL) base->object->scavisflag |= OB_VIS_SENS;
				if (scavisflag & BUTS_CONT_SEL) base->object->scavisflag |= OB_VIS_CONT;
				if (scavisflag & BUTS_ACT_SEL) base->object->scavisflag |= OB_VIS_ACT;
			}
		}
		base= base->next;
	}

	if (obact) {
		if (scavisflag & BUTS_SENS_ACT) obact->scavisflag |= OB_VIS_SENS;
		if (scavisflag & BUTS_CONT_ACT) obact->scavisflag |= OB_VIS_CONT;
		if (scavisflag & BUTS_ACT_ACT) obact->scavisflag |= OB_VIS_ACT;
	}
	
	/* BUTS_XXX_STATE are similar to BUTS_XXX_LINK for selecting the object */
	if (scavisflag & (BUTS_SENS_LINK|BUTS_CONT_LINK|BUTS_ACT_LINK|BUTS_SENS_STATE|BUTS_ACT_STATE)) {
		doit= 1;
		while (doit) {
			doit= 0;
			
			ob= bmain->object.first;
			while (ob) {
			
				/* 1st case: select sensor when controller selected */
				if ((scavisflag & (BUTS_SENS_LINK|BUTS_SENS_STATE)) && (ob->scavisflag & OB_VIS_SENS)==0) {
					sens= ob->sensors.first;
					while (sens) {
						for (a=0; a<sens->totlinks; a++) {
							if (sens->links[a]) {
								obt= (Object *)sens->links[a]->mynew;
								if (obt && (obt->scavisflag & OB_VIS_CONT)) {
									doit= 1;
									ob->scavisflag |= OB_VIS_SENS;
									break;
								}
							}
						}
						if (doit) break;
						sens= sens->next;
					}
				}
				
				/* 2nd case: select cont when act selected */
				if ((scavisflag & BUTS_CONT_LINK)  && (ob->scavisflag & OB_VIS_CONT)==0) {
					cont= ob->controllers.first;
					while (cont) {
						for (a=0; a<cont->totlinks; a++) {
							if (cont->links[a]) {
								obt= (Object *)cont->links[a]->mynew;
								if (obt && (obt->scavisflag & OB_VIS_ACT)) {
									doit= 1;
									ob->scavisflag |= OB_VIS_CONT;
									break;
								}
							}
						}
						if (doit) break;
						cont= cont->next;
					}
				}
				
				/* 3rd case: select controller when sensor selected */
				if ((scavisflag & BUTS_CONT_LINK) && (ob->scavisflag & OB_VIS_SENS)) {
					sens= ob->sensors.first;
					while (sens) {
						for (a=0; a<sens->totlinks; a++) {
							if (sens->links[a]) {
								obt= (Object *)sens->links[a]->mynew;
								if (obt && (obt->scavisflag & OB_VIS_CONT)==0) {
									doit= 1;
									obt->scavisflag |= OB_VIS_CONT;
								}
							}
						}
						sens= sens->next;
					}
				}
				
				/* 4th case: select actuator when controller selected */
				if ( (scavisflag & (BUTS_ACT_LINK|BUTS_ACT_STATE))  && (ob->scavisflag & OB_VIS_CONT)) {
					cont= ob->controllers.first;
					while (cont) {
						for (a=0; a<cont->totlinks; a++) {
							if (cont->links[a]) {
								obt= (Object *)cont->links[a]->mynew;
								if (obt && (obt->scavisflag & OB_VIS_ACT)==0) {
									doit= 1;
									obt->scavisflag |= OB_VIS_ACT;
								}
							}
						}
						cont= cont->next;
					}
					
				}
				ob= ob->id.next;
			}
		}
	} 
	
	/* now we count */
	ob= bmain->object.first;
	while (ob) {
		if ( ob->scavisflag ) (*count)++;
		ob= ob->id.next;
	}

	if (*count==0) return NULL;
	if (*count>24) *count= 24;		/* temporal */
	
	idar= MEM_callocN( (*count)*sizeof(void *), "idar");
	
	ob= bmain->object.first;
	nr= 0;

	/* make the active object always the first one of the list */
	if (obact) {
		idar[0]= (ID *)obact;
		nr++;
	}

	while (ob) {
		if ( (ob->scavisflag) && (ob != obact)) {
			idar[nr]= (ID *)ob;
			nr++;
		}
		if (nr>=24) break;
		ob= ob->id.next;
	}
	
	/* just to be sure... these were set in set_sca_done_ob() */
	clear_sca_new_poins();
	
	return idar;
}


static int get_col_sensor(int type)
{
	/* XXX themecolors not here */
	
	switch(type) {
	case SENS_ALWAYS:		return TH_PANEL;
	case SENS_DELAY:		return TH_PANEL;
	case SENS_TOUCH:		return TH_PANEL;
	case SENS_COLLISION:	return TH_PANEL;
	case SENS_NEAR:			return TH_PANEL; 
	case SENS_KEYBOARD:		return TH_PANEL;
	case SENS_PROPERTY:		return TH_PANEL;
	case SENS_ARMATURE:		return TH_PANEL;
	case SENS_ACTUATOR:		return TH_PANEL;
	case SENS_MOUSE:		return TH_PANEL;
	case SENS_RADAR:		return TH_PANEL;
	case SENS_RANDOM:		return TH_PANEL;
	case SENS_RAY:			return TH_PANEL;
	case SENS_MESSAGE:		return TH_PANEL;
	case SENS_JOYSTICK:		return TH_PANEL;
	default:				return TH_PANEL;
	}
}
static void set_col_sensor(int type, int medium)
{
	int col= get_col_sensor(type);
	UI_ThemeColorShade(col, medium?30:0);
}


static void verify_logicbutton_func(bContext *UNUSED(C), void *data1, void *data2)
{
	bSensor *sens= (bSensor*)data1;
	
	if (sens->level && sens->tap) {
		if (data2 == &(sens->level))
			sens->tap= 0;
		else
			sens->level= 0;
	}
}

static void test_scriptpoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->text, name, offsetof(ID, name) + 2);
}

static void test_actionpoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->action, name, offsetof(ID, name) + 2);
	if (*idpp)
		id_us_plus(*idpp);
}


static void test_obpoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->object, name, offsetof(ID, name) + 2);
	if (*idpp)
		id_lib_extern(*idpp);	/* checks lib data, sets correct flag for saving then */
}

static void test_meshpoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->mesh, name, offsetof(ID, name) + 2);
	if (*idpp)
		id_us_plus(*idpp);
}

static void test_matpoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->mat, name, offsetof(ID, name) + 2);
	if (*idpp)
		id_us_plus(*idpp);
}

static void test_scenepoin_but(struct bContext *C, const char *name, ID **idpp)
{
	*idpp= BLI_findstring(&CTX_data_main(C)->scene, name, offsetof(ID, name) + 2);
	if (*idpp)
		id_us_plus(*idpp);
}

static void test_keyboard_event(struct bContext *UNUSED(C), void *arg_ks, void *UNUSED(arg))
{
	bKeyboardSensor *ks= (bKeyboardSensor*)arg_ks;
	
	if (!ISKEYBOARD(ks->key))
		ks->key= 0;
	if (!ISKEYBOARD(ks->qual))
		ks->qual= 0;
	if (!ISKEYBOARD(ks->qual2))
		ks->qual2= 0;
}

/**
 * Draws a toggle for pulse mode, a frequency field and a toggle to invert
 * the value of this sensor. Operates on the shared data block of sensors.
 */
static void draw_default_sensor_header(bSensor *sens,
								uiBlock *block,
								short x,
								short y,
								short w) 
{
	uiBut *but;
	
	/* Pulsing and frequency */
	uiBlockBeginAlign(block);
	uiDefIconButBitS(block, TOG, SENS_PULSE_REPEAT, 1, ICON_DOTSUP,
			 (short)(x + 10 + 0.0 * (w - 20)), (short)(y - 21), (short)(0.1 * (w - 20)), 19,
			 &sens->pulse, 0.0, 0.0, 0, 0,
			 "Activate TRUE level triggering (pulse mode)");

	uiDefIconButBitS(block, TOG, SENS_NEG_PULSE_MODE, 1, ICON_DOTSDOWN,
			 (short)(x + 10 + 0.1 * (w-20)), (short)(y - 21), (short)(0.1 * (w-20)), 19,
			 &sens->pulse, 0.0, 0.0, 0, 0,
			 "Activate FALSE level triggering (pulse mode)");
	uiDefButS(block, NUM, 1, "f:",
			 (short)(x + 10 + 0.2 * (w-20)), (short)(y - 21), (short)(0.275 * (w-20)), 19,
			 &sens->freq, 0.0, 10000.0, 0, 0,
			 "Delay between repeated pulses (in logic tics, 0 = no delay)");
	uiBlockEndAlign(block);
	
	/* value or shift? */
	uiBlockBeginAlign(block);
	but = uiDefButS(block, TOG, 1, "Level",
			 (short)(x + 10 + 0.5 * (w-20)), (short)(y - 21), (short)(0.20 * (w-20)), 19,
			 &sens->level, 0.0, 0.0, 0, 0,
			 "Level detector, trigger controllers of new states (only applicable upon logic state transition)");
	uiButSetFunc(but, verify_logicbutton_func, sens, &(sens->level));
	but = uiDefButS(block, TOG, 1, "Tap",
			 (short)(x + 10 + 0.702 * (w-20)), (short)(y - 21), (short)(0.12 * (w-20)), 19,
			 &sens->tap, 0.0, 0.0, 0, 0,
			 "Trigger controllers only for an instant, even while the sensor remains true");
	uiButSetFunc(but, verify_logicbutton_func, sens, &(sens->tap));
	uiBlockEndAlign(block);
	
	uiDefButS(block, TOG, 1, "Inv",
			 (short)(x + 10 + 0.85 * (w-20)), (short)(y - 21), (short)(0.15 * (w-20)), 19,
			 &sens->invert, 0.0, 0.0, 0, 0,
			 "Invert the level (output) of this sensor");
}

static void get_armature_bone_constraint(Object *ob, const char *posechannel, const char *constraint_name, bConstraint **constraint)
{
	/* check that bone exist in the active object */
	if (ob->type == OB_ARMATURE && ob->pose) {
		bPoseChannel *pchan= get_pose_channel(ob->pose, posechannel);
		if (pchan) {
			bConstraint *con= BLI_findstring(&pchan->constraints, constraint_name, offsetof(bConstraint, name));
			if (con) {
				*constraint= con;
			}
		}
	}
	/* didn't find any */
}
static void check_armature_bone_constraint(Object *ob, char *posechannel, char *constraint)
{
	/* check that bone exist in the active object */
	if (ob->type == OB_ARMATURE && ob->pose) {
		bPoseChannel *pchan;
		bPose *pose = ob->pose;
		for (pchan=pose->chanbase.first; pchan; pchan=pchan->next) {
			if (!strcmp(pchan->name, posechannel)) {
				/* found it, now look for constraint channel */
				bConstraint *con;
				for (con=pchan->constraints.first; con; con=con->next) {
					if (!strcmp(con->name, constraint)) {
						/* found it, all ok */
						return;						
					}
				}
				/* didn't find constraint, make empty */
				constraint[0] = 0;
				return;
			}
		}
	}
	/* didn't find any */
	posechannel[0] = 0;
	constraint[0] = 0;
}

static void check_armature_sensor(bContext *C, void *arg1_but, void *arg2_sens)
{
	bArmatureSensor *sens = arg2_sens;
	uiBut *but = arg1_but;
	Object *ob= CTX_data_active_object(C);

	/* check that bone exist in the active object */
	but->retval = B_REDR;
	check_armature_bone_constraint(ob, sens->posechannel, sens->constraint);
}

static short draw_sensorbuttons(Object *ob, bSensor *sens, uiBlock *block, short xco, short yco, short width)
{
	bNearSensor      *ns           = NULL;
	bTouchSensor     *ts           = NULL;
	bKeyboardSensor  *ks           = NULL;
	bPropertySensor  *ps           = NULL;
	bArmatureSensor  *arm          = NULL;
	bMouseSensor     *ms           = NULL;
	bCollisionSensor *cs           = NULL;
	bRadarSensor     *rs           = NULL;
	bRandomSensor    *randomSensor = NULL;
	bRaySensor       *raySens      = NULL;
	bMessageSensor   *mes          = NULL;
	bJoystickSensor	 *joy		   = NULL;
	bActuatorSensor  *as           = NULL;
	bDelaySensor     *ds		   = NULL;
	uiBut *but;
	short ysize;
	const char *str;
	
	/* yco is at the top of the rect, draw downwards */
	
	set_col_sensor(sens->type, 0);
	
	switch (sens->type)
	{
	case SENS_ALWAYS:
		{
			ysize= 24;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			yco-= ysize;
			
			break;
		}
	case SENS_TOUCH:
		{
			ysize= 48; 
			
			glRects(xco, yco-ysize, xco+width, yco); 
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			ts= sens->data; 
			
			// uiDefBut(block, TEX, 1, "Property:",	xco,yco-22,width, 19, &ts->name, 0, MAX_NAME, 0, 0, "Only look for Objects with this property");
			uiDefIDPoinBut(block, test_matpoin_but, ID_MA, 1, "MA:",(short)(xco + 10),(short)(yco-44), (short)(width - 20), 19, &ts->ma,  "Only look for floors with this Material"); 
			// uiDefButF(block, NUM, 1, "Margin:",	xco+width/2,yco-44,width/2, 19, &ts->dist, 0.0, 10.0, 100, 0, "Extra margin (distance) for larger sensitivity"); 
			yco-= ysize; 
			break; 
		}
	case SENS_COLLISION:
		{
			ysize= 48;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			cs= sens->data;
			
			/* The collision sensor will become a generic collision (i.e. it     */
			/* absorb the old touch sensor).                                     */

			uiDefButBitS(block, TOG, SENS_COLLISION_PULSE, B_REDR, "Pulse",(short)(xco + 10),(short)(yco - 44),
				(short)(0.20 * (width-20)), 19, &cs->mode, 0.0, 0.0, 0, 0,
				"Changes to the set of colliding objects generated pulses");
			
			uiDefButBitS(block, TOG, SENS_COLLISION_MATERIAL, B_REDR, "M/P",(short)(xco + 10 + (0.20 * (width-20))),(short)(yco - 44),
				(short)(0.20 * (width-20)), 19, &cs->mode, 0.0, 0.0, 0, 0,
				"Toggle collision on material or property");
			
			if (cs->mode & SENS_COLLISION_MATERIAL) {
				uiDefBut(block, TEX, 1, "Material:", (short)(xco + 10 + 0.40 * (width-20)),
					(short)(yco-44), (short)(0.6*(width-20)), 19, &cs->materialName, 0, MAX_NAME, 0, 0,
					"Only look for Objects with this material");
			}
			else {
				uiDefBut(block, TEX, 1, "Property:", (short)(xco + 10 + 0.40 * (width-20)), (short)(yco-44),
					(short)(0.6*(width-20)), 19, &cs->name, 0, MAX_NAME, 0, 0,
					"Only look for Objects with this property");
			}
	
			/*  		uiDefButS(block, NUM, 1, "Damp:",	xco+10+width-90,yco-24, 70, 19, &cs->damp, 0, 250, 0, 0, "For 'damp' time don't detect another collision"); */
			
			yco-= ysize;
			break;
		}
	case SENS_NEAR:
		{
			ysize= 72;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			ns= sens->data;
			
			uiDefBut(block, TEX, 1, "Property:",(short)(10+xco),(short)(yco-44), (short)(width-20), 19,
				&ns->name, 0, MAX_NAME, 0, 0, "Only look for Objects with this property");
			uiDefButF(block, NUM, 1, "Dist",(short)(10+xco),(short)(yco-68),(short)((width-22)/2), 19,
				&ns->dist, 0.0, 1000.0, 1000, 0, "Trigger distance");
			uiDefButF(block, NUM, 1, "Reset",(short)(10+xco+(width-22)/2), (short)(yco-68), (short)((width-22)/2), 19,
				&ns->resetdist, 0.0, 1000.0, 1000, 0, "Reset distance"); 
			yco-= ysize;
			break;
		}
	case SENS_RADAR:
		{
			ysize= 72; 
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			rs= sens->data;
			
			uiDefBut(block, TEX, 1, "Prop:",
					 (short)(10+xco),(short)(yco-44), (short)(0.7 * (width-20)), 19,
					 &rs->name, 0, MAX_NAME, 0, 0,
					 "Only look for Objects with this property");

			str = "Type %t|+X axis %x0|+Y axis %x1|+Z axis %x2|-X axis %x3|-Y axis %x4|-Z axis %x5"; 
			uiDefButS(block, MENU, B_REDR, str,
				(short)(10+xco+0.7 * (width-20)), (short)(yco-44), (short)(0.3 * (width-22)), 19,
				&rs->axis, 2.0, 31, 0, 0,
				"Specify along which axis the radar cone is cast");
				
			uiDefButF(block, NUM, 1, "Ang:",
					 (short)(10+xco), (short)(yco-68), (short)((width-20)/2), 19,
					 &rs->angle, 0.0, 179.9, 10, 0,
					 "Opening angle of the radar cone");
			uiDefButF(block, NUM, 1, "Dist:",
					 (short)(xco+10 + (width-20)/2), (short)(yco-68), (short)((width-20)/2), 19,
					 &rs->range, 0.01, 10000.0, 100, 0,
					 "Depth of the radar cone");
			yco-= ysize;
			break;
		}
	case SENS_KEYBOARD:
		{
			ks= sens->data;
			
			/* 5 lines: 120 height */
			ysize= (ks->type&1) ? 96:120;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			/* header line */
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			/* part of line 1 */
			uiDefBut(block, LABEL, 0, "Key",	  xco, yco-44, 40, 19, NULL, 0, 0, 0, 0, "");
			uiDefButBitS(block, TOG, 1, B_REDR, "All keys",	  xco+40+(width/2), yco-44, (width/2)-50, 19,
				&ks->type, 0, 0, 0, 0, "");
			
			
			if ((ks->type&1)==0) { /* is All Keys option off? */
				/* line 2: hotkey and allkeys toggle */
				but = uiDefKeyevtButS(block, 0, "", xco+40, yco-44, (width)/2, 19, &ks->key, "Key code");
				uiButSetFunc(but, test_keyboard_event, ks, NULL);
				
				/* line 3: two key modifyers (qual1, qual2) */
				uiDefBut(block, LABEL, 0, "Hold",	  xco, yco-68, 40, 19, NULL, 0, 0, 0, 0, "");
				but = uiDefKeyevtButS(block, 0, "", xco+40, yco-68, (width-50)/2, 19, &ks->qual, "Modifier key code");
				uiButSetFunc(but, test_keyboard_event, ks, NULL);
				but = uiDefKeyevtButS(block, 0, "", xco+40+(width-50)/2, yco-68, (width-50)/2, 19, &ks->qual2, "Second Modifier key code");
				uiButSetFunc(but, test_keyboard_event, ks, NULL);
			}
			
			/* line 4: toggle property for string logging mode */
			uiDefBut(block, TEX, 1, "LogToggle: ",
				xco+10, yco-((ks->type&1) ? 68:92), (width-20), 19,
				ks->toggleName, 0, MAX_NAME, 0, 0,
				"Property that indicates whether to log "
				"keystrokes as a string");
			
			/* line 5: target property for string logging mode */
			uiDefBut(block, TEX, 1, "Target: ",
				xco+10, yco-((ks->type&1) ? 92:116), (width-20), 19,
				ks->targetName, 0, MAX_NAME, 0, 0,
				"Property that receives the keystrokes in case "
				"a string is logged");
			
			yco-= ysize;
			break;
		}
	case SENS_PROPERTY:
		{
			ysize= 96;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize,
				(float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			ps= sens->data;
			
			str= "Type %t|Equal %x0|Not Equal %x1|Interval %x2|Changed %x3"; 
			/* str= "Type %t|Equal %x0|Not Equal %x1"; */
			uiDefButI(block, MENU, B_REDR, str,			xco+30,yco-44,width-60, 19,
				&ps->type, 0, 31, 0, 0, "Type");
			
			if (ps->type != SENS_PROP_EXPRESSION)
			{
				uiDefBut(block, TEX, 1, "Prop: ",			xco+30,yco-68,width-60, 19,
					ps->name, 0, MAX_NAME, 0, 0,  "Property name");
			}
			
			if (ps->type == SENS_PROP_INTERVAL)
			{
				uiDefBut(block, TEX, 1, "Min: ",		xco,yco-92,width/2, 19,
					ps->value, 0, MAX_NAME, 0, 0, "check for min value");
				uiDefBut(block, TEX, 1, "Max: ",		xco+width/2,yco-92,width/2, 19,
					ps->maxvalue, 0, MAX_NAME, 0, 0, "check for max value");
			}
			else if (ps->type == SENS_PROP_CHANGED) {
				/* pass */
			}
			else {
				uiDefBut(block, TEX, 1, "Value: ",		xco+30,yco-92,width-60, 19,
				         ps->value, 0, MAX_NAME, 0, 0, "check for value");
			}
			
			yco-= ysize;
			break;
		}
	case SENS_ARMATURE:
		{
			ysize= 70;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize,
				(float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			arm= sens->data;

			if (ob->type == OB_ARMATURE) {
				uiBlockBeginAlign(block);
				but = uiDefBut(block, TEX, 1, "Bone: ",
						(xco+10), (yco-44), (width-20)/2, 19,
						arm->posechannel, 0, MAX_NAME, 0, 0,
						"Bone on which you want to check a constraint");
				uiButSetFunc(but, check_armature_sensor, but, arm);
				but = uiDefBut(block, TEX, 1, "Cons: ",
						(xco+10)+(width-20)/2, (yco-44), (width-20)/2, 19,
						arm->constraint, 0, MAX_NAME, 0, 0,
						"Name of the constraint you want to control");
				uiButSetFunc(but, check_armature_sensor, but, arm);
				uiBlockEndAlign(block);

				str= "Type %t|State changed %x0|Lin error below %x1|Lin error above %x2|Rot error below %x3|Rot error above %x4"; 

				uiDefButI(block, MENU, B_REDR, str,			xco+10,yco-66,0.4*(width-20), 19,
					&arm->type, 0, 31, 0, 0, "Type");
			
				if (arm->type != SENS_ARM_STATE_CHANGED)
				{
					uiDefButF(block, NUM, 1, "Value: ",		xco+10+0.4*(width-20),yco-66,0.6*(width-20), 19,
					&arm->value, -10000.0, 10000.0, 100, 0, "Test the error against this value");
				}
			}
			yco-= ysize;
			break;
		}
	case SENS_ACTUATOR:
		{
			ysize= 48;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize,
				(float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			as= sens->data;
			
			uiDefBut(block, TEX, 1, "Act: ",			xco+30,yco-44,width-60, 19,
					as->name, 0, MAX_NAME, 0, 0,  "Actuator name, actuator active state modifications will be detected");
			yco-= ysize;
			break;
		}
	case SENS_DELAY:
		{
			ysize= 48;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize,
				(float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			ds = sens->data;
			
			uiDefButS(block, NUM, 0, "Delay",(short)(10+xco),(short)(yco-44),(short)((width-22)*0.4+10), 19,
				&ds->delay, 0.0, 5000.0, 0, 0, "Delay in number of logic tics before the positive trigger (default 60 per second)");
			uiDefButS(block, NUM, 0, "Dur",(short)(10+xco+(width-22)*0.4+10),(short)(yco-44),(short)((width-22)*0.4-10), 19,
				&ds->duration, 0.0, 5000.0, 0, 0, "If >0, delay in number of logic tics before the negative trigger following the positive trigger");
			uiDefButBitS(block, TOG, SENS_DELAY_REPEAT, 0, "REP",(short)(xco + 10 + (width-22)*0.8),(short)(yco - 44),
				(short)(0.20 * (width-22)), 19, &ds->flag, 0.0, 0.0, 0, 0,
				"Toggle repeat option. If selected, the sensor restarts after Delay+Dur logic tics");
			yco-= ysize;
			break;
		}
	case SENS_MOUSE:
		{
			ms= sens->data;
			/* Two lines: 48 pixels high. */
			ysize = 48;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			/* line 1: header */
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			/* Line 2: type selection. The number are a bit mangled to get
			 * proper compatibility with older .blend files. */

			/* Any sensor type default is 0 but the ms enum starts in 1.
			 * Therefore the mouse sensor is initialized to 1 in sca.c */
			str= "Type %t|Left button %x1|Middle button %x2|"
				"Right button %x4|Wheel Up %x5|Wheel Down %x6|Movement %x8|Mouse over %x16|Mouse over any%x32"; 
			uiDefButS(block, MENU, B_REDR, str, xco+10, yco-44, (width*0.8f)-20, 19,
				&ms->type, 0, 31, 0, 0,
				"Specify the type of event this mouse sensor should trigger on");
			
			if (ms->type==32) {
				uiDefButBitS(block, TOG, SENS_MOUSE_FOCUS_PULSE, B_REDR, "Pulse",(short)(xco + 10) + (width*0.8f)-20,(short)(yco - 44),
					(short)(0.20 * (width-20)), 19, &ms->flag, 0.0, 0.0, 0, 0,
					"Moving the mouse over a different object generates a pulse");	
			}
			
			yco-= ysize;
			break;
		}
	case SENS_RANDOM:
		{
			ysize = 48;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			randomSensor = sens->data;
			/* some files were wrongly written, avoid crash now */
			if (randomSensor)
			{
				uiDefButI(block, NUM, 1, "Seed: ",		xco+10,yco-44,(width-20), 19,
					&randomSensor->seed, 0, 1000, 0, 0,
					"Initial seed of the generator. (Choose 0 for not random)");
			}
			yco-= ysize;
			break;
		}
	case SENS_RAY:
		{
			ysize = 72;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			draw_default_sensor_header(sens, block, xco, yco, width);
			raySens = sens->data;
			
			/* 1. property or material */
			uiDefButBitS(block, TOG, SENS_COLLISION_MATERIAL, B_REDR, "M/P",
				xco + 10,yco - 44, 0.20 * (width-20), 19,
				&raySens->mode, 0.0, 0.0, 0, 0,
				"Toggle collision on material or property");
			
			if (raySens->mode & SENS_COLLISION_MATERIAL) {
				uiDefBut(block, TEX, 1, "Material:", xco + 10 + 0.20 * (width-20), yco-44, 0.8*(width-20), 19,
					&raySens->matname, 0, MAX_NAME, 0, 0,
					"Only look for Objects with this material");
			}
			else {
				uiDefBut(block, TEX, 1, "Property:", xco + 10 + 0.20 * (width-20), yco-44, 0.8*(width-20), 19,
					&raySens->propname, 0, MAX_NAME, 0, 0,
					"Only look for Objects with this property");
			}

			/* X-Ray option */
			uiDefButBitS(block, TOG, SENS_RAY_XRAY, 1, "X",
				xco + 10,yco - 68, 0.10 * (width-20), 19,
				&raySens->mode, 0.0, 0.0, 0, 0,
				"Toggle X-Ray option (see through objects that don't have the property)");
			/* 2. sensing range */
			uiDefButF(block, NUM, 1, "Range", xco+10 + 0.10 * (width-20), yco-68, 0.5 * (width-20), 19,
				&raySens->range, 0.01, 10000.0, 100, 0,
				"Sense objects no farther than this distance");
			
			/* 3. axis choice */
			str = "Type %t|+ X axis %x1|+ Y axis %x0|+ Z axis %x2|- X axis %x3|- Y axis %x4|- Z axis %x5"; 
			uiDefButI(block, MENU, B_REDR, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
				&raySens->axisflag, 2.0, 31, 0, 0,
				"Specify along which axis the ray is cast");
			
			yco-= ysize;		
			break;
		}
	case SENS_MESSAGE:
		{
			mes = sens->data;
			ysize = 2 * 24; /* total number of lines * 24 pixels/line */
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize,
				(float)xco+width, (float)yco, 1);
			
			/* line 1: header line */
			draw_default_sensor_header(sens, block, xco, yco, width);
			
			/* line 2: Subject filter */
			uiDefBut(block, TEX, 1, "Subject: ",
				(xco+10), (yco-44), (width-20), 19,
				mes->subject, 0, MAX_NAME, 0, 0,
				"Optional subject filter: only accept messages with this subject"
				", or empty for all");
			
			yco -= ysize;
			break;
		}
		case SENS_JOYSTICK:
		{

			ysize =  72;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			/* line 1: header */
			draw_default_sensor_header(sens, block, xco, yco, width);

			joy= sens->data;

			uiDefButC(block, NUM, 1, "Index:", xco+10, yco-44, 0.33 * (width-20), 19,
			&joy->joyindex, 0, SENS_JOY_MAXINDEX-1, 100, 0,
			"Specify which joystick to use");			

			str= "Type %t|Button %x0|Axis %x1|Single Axis %x3|Hat%x2"; 
			uiDefButC(block, MENU, B_REDR, str, xco+87, yco-44, 0.26 * (width-20), 19,
				&joy->type, 0, 31, 0, 0,
				"The type of event this joystick sensor is triggered on");
			
			if (joy->type != SENS_JOY_AXIS_SINGLE) {
				if (joy->flag & SENS_JOY_ANY_EVENT) {
					switch (joy->type) {
					case SENS_JOY_AXIS:	
						str = "All Axis Events";
						break;
					case SENS_JOY_BUTTON:	
						str = "All Button Events";
						break;
					default:
						str = "All Hat Events";
						break;
					}
				}
				else {
					str = "All";
				}
				
				uiDefButBitS(block, TOG, SENS_JOY_ANY_EVENT, B_REDR, str,
					xco+10 + 0.475 * (width-20), yco-68, ((joy->flag & SENS_JOY_ANY_EVENT) ? 0.525 : 0.12) * (width-20), 19,
					&joy->flag, 0, 0, 0, 0,
					"Triggered by all events on this joysticks current type (axis/button/hat)");
			}
			if (joy->type == SENS_JOY_BUTTON)
			{
				if ((joy->flag & SENS_JOY_ANY_EVENT)==0) {
					uiDefButI(block, NUM, 1, "Number:", xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
					&joy->button, 0, 18, 100, 0,
					"Specify which button to use");
				}
			}
			else if (joy->type == SENS_JOY_AXIS) {
				uiDefButS(block, NUM, 1, "Number:", xco+10, yco-68, 0.46 * (width-20), 19,
				&joy->axis, 1, 8.0, 100, 0,
				"Specify which axis pair to use, 1 is useually the main direction input");

				uiDefButI(block, NUM, 1, "Threshold:", xco+10 + 0.6 * (width-20),yco-44, 0.4 * (width-20), 19,
				&joy->precision, 0, 32768.0, 100, 0,
				"Specify the precision of the axis");

				if ((joy->flag & SENS_JOY_ANY_EVENT)==0) {
					str = "Type %t|Up Axis %x1 |Down Axis %x3|Left Axis %x2|Right Axis %x0"; 
					uiDefButI(block, MENU, B_REDR, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
					&joy->axisf, 2.0, 31, 0, 0,
					"The direction of the axis, use 'All Events' to receive events on any direction");
				}
			}
			else if (joy->type == SENS_JOY_HAT) {
				uiDefButI(block, NUM, 1, "Number:", xco+10, yco-68, 0.46 * (width-20), 19,
				&joy->hat, 1, 4.0, 100, 0,
				"Specify which hat to use");
				
				if ((joy->flag & SENS_JOY_ANY_EVENT)==0) {
					str = "Direction%t|Up%x1|Down%x4|Left%x8|Right%x2|%l|Up/Right%x3|Down/Left%x12|Up/Left%x9|Down/Right%x6"; 
					uiDefButI(block, MENU, 0, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
					&joy->hatf, 2.0, 31, 0, 0,
					"The direction of the hat, use 'All Events' to receive events on any direction");
				}
			}
			else { /* (joy->type == SENS_JOY_AXIS_SINGLE)*/
				uiDefButS(block, NUM, 1, "Number:", xco+10, yco-68, 0.46 * (width-20), 19,
				&joy->axis_single, 1, 16.0, 100, 0,
				"Specify a single axis (verticle/horizontal/other) to detect");
				
				uiDefButI(block, NUM, 1, "Threshold:", xco+10 + 0.6 * (width-20),yco-44, 0.4 * (width-20), 19,
				&joy->precision, 0, 32768.0, 100, 0,
				"Specify the precision of the axis");
			}
			yco-= ysize;
			break;
		}
	}
	
	return yco-4;
}



static short draw_controllerbuttons(bController *cont, uiBlock *block, short xco, short yco, short width)
{
	bExpressionCont *ec;
	bPythonCont *pc;
	short ysize;
	
	switch (cont->type) {
	case CONT_EXPRESSION:
		ysize= 28;

		UI_ThemeColor(TH_PANEL);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		/* uiDefBut(block, LABEL, 1, "Not yet...", xco,yco-24,80, 19, NULL, 0, 0, 0, 0, ""); */
		ec= cont->data;	
		/* uiDefBut(block, BUT, 1, "Variables", xco,yco-24,80, 19, NULL, 0, 0, 0, 0, "Available variables for expression"); */
		uiDefBut(block, TEX, 1, "Exp:",		xco + 10 , yco-21, width-20, 19,
				 ec->str, 0, sizeof(ec->str), 0, 0,
				 "Expression"); 
		
		yco-= ysize;
		break;
	case CONT_PYTHON:
		ysize= 28;
		
		if (cont->data==NULL) init_controller(cont);
		pc= cont->data;
		
		UI_ThemeColor(TH_PANEL);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

	
		uiBlockBeginAlign(block);
		uiDefButI(block, MENU, B_REDR, "Execution Method%t|Script%x0|Module%x1", xco+4,yco-23, 66, 19, &pc->mode, 0, 0, 0, 0, "Python script type (textblock or module - faster)");
		if (pc->mode==0)
			uiDefIDPoinBut(block, test_scriptpoin_but, ID_TXT, 1, "", xco+70,yco-23,width-74, 19, &pc->text, "Blender textblock to run as a script");
		else {
			uiDefBut(block, TEX, 1, "", xco+70,yco-23,(width-70)-25, 19, pc->module, 0, sizeof(pc->module), 0, 0, "Module name and function to run e.g. \"someModule.main\". Internal texts and external python files can be used");
			uiDefButBitI(block, TOG, CONT_PY_DEBUG, B_REDR, "D", (xco+width)-25, yco-23, 19, 19, &pc->flag, 0, 0, 0, 0, "Continuously reload the module from disk for editing external modules without restarting");
		}
		uiBlockEndAlign(block);
		
		yco-= ysize;
		break;
		
	default:
		ysize= 4;

		UI_ThemeColor(TH_PANEL);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		yco-= ysize;
	}
	
	return yco;
}

static int get_col_actuator(int type)
{
	switch(type) {
	case ACT_ACTION:		return TH_PANEL;
	case ACT_SHAPEACTION:	return TH_PANEL;
	case ACT_OBJECT:		return TH_PANEL;
	case ACT_IPO:			return TH_PANEL;
	case ACT_PROPERTY:		return TH_PANEL;
	case ACT_SOUND:			return TH_PANEL;
	case ACT_CAMERA: 		return TH_PANEL;
	case ACT_EDIT_OBJECT: 		return TH_PANEL;
	case ACT_GROUP:			return TH_PANEL;
	case ACT_RANDOM:		return TH_PANEL;
	case ACT_SCENE:			return TH_PANEL;
	case ACT_MESSAGE:		return TH_PANEL;
	case ACT_GAME:			return TH_PANEL;
	case ACT_VISIBILITY:		return TH_PANEL;
	case ACT_CONSTRAINT:		return TH_PANEL;
	case ACT_STATE:			return TH_PANEL;
	case ACT_ARMATURE:			return TH_PANEL;
	case ACT_STEERING:		return TH_PANEL;
	default:				return TH_PANEL;
	}
}
static void set_col_actuator(int item, int medium) 
{
	int col= get_col_actuator(item);
	UI_ThemeColorShade(col, medium?30:10);
	
}

static void change_object_actuator(bContext *UNUSED(C), void *act, void *UNUSED(arg))
{
	bObjectActuator *oa = act;

	if (oa->type != oa->otype) {
		switch (oa->type) {
		case ACT_OBJECT_NORMAL:
			memset(oa, 0, sizeof(bObjectActuator));
			oa->flag = ACT_FORCE_LOCAL|ACT_TORQUE_LOCAL|ACT_DLOC_LOCAL|ACT_DROT_LOCAL;
			oa->type = ACT_OBJECT_NORMAL;
			break;

		case ACT_OBJECT_SERVO:
			memset(oa, 0, sizeof(bObjectActuator));
			oa->flag = ACT_LIN_VEL_LOCAL;
			oa->type = ACT_OBJECT_SERVO;
			oa->forcerot[0] = 30.0f;
			oa->forcerot[1] = 0.5f;
			oa->forcerot[2] = 0.0f;
			break;
		}
	}
}

static void change_ipo_actuator(bContext *UNUSED(C), void *arg1_but, void *arg2_ia)
{
	bIpoActuator *ia = arg2_ia;
	uiBut *but = arg1_but;

	if (but->retval & ACT_IPOFORCE)
		ia->flag &= ~ACT_IPOADD;
	else if (but->retval & ACT_IPOADD)
		ia->flag &= ~ACT_IPOFORCE;
	but->retval = B_REDR;
}

static void update_object_actuator_PID(bContext *UNUSED(C), void *act, void *UNUSED(arg))
{
	bObjectActuator *oa = act;
	oa->forcerot[0] = 60.0f*oa->forcerot[1];
}

static char *get_state_name(Object *ob, short bit)
{
	bController *cont;
	unsigned int mask;

	mask = (1<<bit);
	cont = ob->controllers.first;
	while (cont) {
		if (cont->state_mask & mask) {
			return cont->name;
		}
		cont = cont->next;
	}
	return (char*)"";
}

static void check_state_mask(bContext *C, void *arg1_but, void *arg2_mask)
{
	wmWindow *win= CTX_wm_window(C);
	int shift= win->eventstate->shift;
	unsigned int *cont_mask = arg2_mask;
	uiBut *but = arg1_but;

	if (*cont_mask == 0 || !(shift))
		*cont_mask = (1<<but->retval);
	but->retval = B_REDR;
}

static void check_armature_actuator(bContext *C, void *arg1_but, void *arg2_act)
{
	bArmatureActuator *act = arg2_act;
	uiBut *but = arg1_but;
	Object *ob= CTX_data_active_object(C);

	/* check that bone exist in the active object */
	but->retval = B_REDR;
	check_armature_bone_constraint(ob, act->posechannel, act->constraint);
}


static short draw_actuatorbuttons(Main *bmain, Object *ob, bActuator *act, uiBlock *block, short xco, short yco, short width)
{
	bSoundActuator      *sa      = NULL;
	bObjectActuator     *oa      = NULL;
	bIpoActuator        *ia      = NULL;
	bPropertyActuator   *pa      = NULL;
	bCameraActuator     *ca      = NULL;
	bEditObjectActuator *eoa     = NULL;
	bConstraintActuator *coa     = NULL;
	bSceneActuator      *sca     = NULL;
	bGroupActuator      *ga      = NULL;
	bRandomActuator     *randAct = NULL;
	bMessageActuator    *ma      = NULL;
	bActionActuator	    *aa	     = NULL;
	bGameActuator	    *gma     = NULL;
	bVisibilityActuator *visAct  = NULL;
	bTwoDFilterActuator	*tdfa	 = NULL;
	bParentActuator     *parAct  = NULL;
	bStateActuator		*staAct  = NULL;
	bArmatureActuator   *armAct  = NULL;
	
	float *fp;
	short ysize = 0, wval;
	const char *str;
	int myline, stbit;
	uiBut *but;


	/* yco is at the top of the rect, draw downwards */
	set_col_actuator(act->type, 0);
	
	switch (act->type)
	{
	case ACT_OBJECT:
		{
			oa = act->data;
			wval = (width-100)/3;
			if (oa->type == ACT_OBJECT_NORMAL)
			{
				if (ob->gameflag & OB_DYNAMIC) {
					ysize= 175;
				}
				else {
					ysize= 72;
				}

				glRects(xco, yco-ysize, xco+width, yco);
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, 0, "Loc",	xco, yco-45, 45, 19, NULL, 0, 0, 0, 0, "Sets the location");
				uiDefButF(block, NUM, 0, "",		xco+45, yco-45, wval, 19, oa->dloc, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-45, wval, 19, oa->dloc+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-45, wval, 19, oa->dloc+2, -10000.0, 10000.0, 10, 0, "");
				uiBlockEndAlign(block);
				
				uiDefBut(block, LABEL, 0, "Rot",	xco, yco-64, 45, 19, NULL, 0, 0, 0, 0, "Sets the rotation");
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, 0, "",		xco+45, yco-64, wval, 19, oa->drot, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-64, wval, 19, oa->drot+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-64, wval, 19, oa->drot+2, -10000.0, 10000.0, 10, 0, "");
				uiBlockEndAlign(block);

				uiDefButBitS(block, TOG, ACT_DLOC_LOCAL, 0, "L",		xco+45+3*wval, yco-45, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
				uiDefButBitS(block, TOG, ACT_DROT_LOCAL, 0, "L",		xco+45+3*wval, yco-64, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
	
				if ( ob->gameflag & OB_DYNAMIC )
				{
					uiDefBut(block, LABEL, 0, "Force",	xco, yco-87, 55, 19, NULL, 0, 0, 0, 0, "Sets the force");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, 0, "",		xco+45, yco-87, wval, 19, oa->forceloc, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-87, wval, 19, oa->forceloc+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-87, wval, 19, oa->forceloc+2, -10000.0, 10000.0, 10, 0, "");
					uiBlockEndAlign(block);

					uiDefBut(block, LABEL, 0, "Torque", xco, yco-106, 55, 19, NULL, 0, 0, 0, 0, "Sets the torque");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, 0, "",		xco+45, yco-106, wval, 19, oa->forcerot, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-106, wval, 19, oa->forcerot+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-106, wval, 19, oa->forcerot+2, -10000.0, 10000.0, 10, 0, "");				
					uiBlockEndAlign(block);
				}
				
				if ( ob->gameflag & OB_DYNAMIC )
				{
					uiDefBut(block, LABEL, 0, "LinV",	xco, yco-129, 45, 19, NULL, 0, 0, 0, 0, "Sets the linear velocity");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, 0, "",		xco+45, yco-129, wval, 19, oa->linearvelocity, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-129, wval, 19, oa->linearvelocity+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-129, wval, 19, oa->linearvelocity+2, -10000.0, 10000.0, 10, 0, "");
					uiBlockEndAlign(block);
				
					uiDefBut(block, LABEL, 0, "AngV",	xco, yco-148, 45, 19, NULL, 0, 0, 0, 0, "Sets the angular velocity");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, 0, "",		xco+45, yco-148, wval, 19, oa->angularvelocity, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-148, wval, 19, oa->angularvelocity+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-148, wval, 19, oa->angularvelocity+2, -10000.0, 10000.0, 10, 0, "");
					uiBlockEndAlign(block);
				
					uiDefBut(block, LABEL, 0, "Damp",	xco, yco-171, 45, 19, NULL, 0, 0, 0, 0, "Number of frames to reach the target velocity");
					uiDefButS(block, NUM, 0, "",		xco+45, yco-171, wval, 19, &oa->damping, 0.0, 1000.0, 100, 0, "");

					uiDefButBitS(block, TOG, ACT_FORCE_LOCAL, 0, "L",		xco+45+3*wval, yco-87, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_TORQUE_LOCAL, 0, "L",		xco+45+3*wval, yco-106, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_LIN_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-129, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_ANG_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-148, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
				
					uiDefButBitS(block, TOG, ACT_ADD_LIN_VEL, 0, "use_additive",xco+45+3*wval+15, yco-129, 35, 19, &oa->flag, 0.0, 0.0, 0, 0, "Toggles between ADD and SET linV");
				}				
			}
			else if (oa->type == ACT_OBJECT_SERVO)
			{
				ysize= 195;
				
				glRects(xco, yco-ysize, xco+width, yco);
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
				
				uiDefBut(block, LABEL, 0, "Ref",	xco, yco-45, 45, 19, NULL, 0, 0, 0, 0, "");
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+45, yco-45, wval*3, 19, &(oa->reference), "Reference object for velocity calculation, leave empty for world reference");
				uiDefBut(block, LABEL, 0, "linV",	xco, yco-68, 45, 19, NULL, 0, 0, 0, 0, "Sets the target relative linear velocity, it will be achieved by automatic application of force. Null velocity is a valid target");
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, 0, "",		xco+45, yco-68, wval, 19, oa->linearvelocity, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-68, wval, 19, oa->linearvelocity+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-68, wval, 19, oa->linearvelocity+2, -10000.0, 10000.0, 10, 0, "");
				uiBlockEndAlign(block);
				uiDefButBitS(block, TOG, ACT_LIN_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-68, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Velocity is defined in local coordinates");

				uiDefBut(block, LABEL, 0, "Limit",	xco, yco-91, 45, 19, NULL, 0, 0, 0, 0, "Select if the force needs to be limited along certain axis (local or global depending on LinV Local flag)");
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_X, B_REDR, "X",		xco+45, yco-91, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the X axis");
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_Y, B_REDR, "Y",		xco+45+wval, yco-91, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the Y axis");
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_Z, B_REDR, "Z",		xco+45+2*wval, yco-91, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the Z axis");
				uiBlockEndAlign(block);
				uiDefBut(block, LABEL, 0, "Max",	xco, yco-110, 45, 19, NULL, 0, 0, 0, 0, "Set the upper limit for force");
				uiDefBut(block, LABEL, 0, "Min",	xco, yco-129, 45, 19, NULL, 0, 0, 0, 0, "Set the lower limit for force");
				if (oa->flag & ACT_SERVO_LIMIT_X) {
					uiDefButF(block, NUM, 0, "",		xco+45, yco-110, wval, 19, oa->dloc, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-129, wval, 19, oa->drot, -10000.0, 10000.0, 10, 0, "");
				}
				if (oa->flag & ACT_SERVO_LIMIT_Y) {
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-110, wval, 19, oa->dloc+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-129, wval, 19, oa->drot+1, -10000.0, 10000.0, 10, 0, "");
				}
				if (oa->flag & ACT_SERVO_LIMIT_Z) {
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-110, wval, 19, oa->dloc+2, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-129, wval, 19, oa->drot+2, -10000.0, 10000.0, 10, 0, "");
				}
				uiDefBut(block, LABEL, 0, "Servo",	xco, yco-152, 45, 19, NULL, 0, 0, 0, 0, "Coefficients of the PID servo controller");
				uiDefButF(block, NUMSLI, B_REDR, "P: ",		xco+45, yco-152, wval*3, 19, oa->forcerot, 0.00, 200.0, 100, 0, "Proportional coefficient, typical value is 60x Integral coefficient");
				uiDefBut(block, LABEL, 0, "Slow",	xco, yco-171, 45, 19, NULL, 0, 0, 0, 0, "Low value of I coefficient correspond to slow response");
				but = uiDefButF(block, NUMSLI, B_REDR, " I : ",		xco+45, yco-171, wval*3, 19, oa->forcerot+1, 0.0, 3.0, 1, 0, "Integral coefficient, low value (0.01) for slow response, high value (0.5) for fast response");
				uiButSetFunc(but, update_object_actuator_PID, oa, NULL);
				uiDefBut(block, LABEL, 0, "Fast",	xco+45+3*wval, yco-171, 45, 19, NULL, 0, 0, 0, 0, "High value of I coefficient correspond to fast response");
				uiDefButF(block, NUMSLI, B_REDR, "D: ",		xco+45, yco-190, wval*3, 19, oa->forcerot+2, -100.0, 100.0, 100, 0, "Derivate coefficient, not required, high values can cause instability");
			}
			str= "Motion Type %t|Simple motion %x0|Servo Control %x1";
			but = uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-23, (width-80), 19, &oa->type, 0.0, 0.0, 0, 0, "");
			oa->otype = oa->type;
			uiButSetFunc(but, change_object_actuator, oa, NULL);
			yco-= ysize;
			break;
		}
	case ACT_ACTION:
	case ACT_SHAPEACTION:
		{
			/* DrawAct */
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
			ysize = 112;
#else
			ysize= 92;
#endif
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			aa = act->data;
			wval = (width-60)/3;
			
			//		str= "Action types   %t|Play %x0|Ping Pong %x1|Flipper %x2|Loop Stop %x3|Loop End %x4|Property %x6";
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
			str= "Action types   %t|Play %x0|Flipper %x2|Loop Stop %x3|Loop End %x4|Property %x6|Displacement %x7";
#else
			str= "Action types   %t|Play %x0|Flipper %x2|Loop Stop %x3|Loop End %x4|Property %x6";
#endif
			uiDefButS(block, MENU, B_REDR, str, xco+10, yco-24, width/3, 19, &aa->type, 0.0, 0.0, 0.0, 0.0, "Action playback type");
			uiDefIDPoinBut(block, test_actionpoin_but, ID_AC, 1, "AC: ", xco+10+ (width/3), yco-24, ((width/3)*2) - (20 + 60), 19, &aa->act, "Action name");
			
			uiDefButBitS(block, TOGN, 1, 0, "Continue", xco+((width/3)*2)+20, yco-24, 60, 19,
					 &aa->end_reset, 0.0, 0.0, 0, 0, "Restore last frame when switching on/off, otherwise play from the start each time");
			
			
			if (aa->type == ACT_ACTION_FROM_PROP) {
				uiDefBut(block, TEX, 0, "Prop: ",xco+10, yco-44, width-20, 19, aa->name, 0.0, MAX_NAME, 0, 0, "Use this property to define the Action position");
			}
			else {
				uiDefButF(block, NUM, 0, "Sta: ",xco+10, yco-44, (width-20)/2, 19, &aa->sta, 1.0, MAXFRAMEF, 0, 0, "Start frame");
				uiDefButF(block, NUM, 0, "End: ",xco+10+(width-20)/2, yco-44, (width-20)/2, 19, &aa->end, 1.0, MAXFRAMEF, 0, 0, "End frame");
			}
						
			uiDefButS(block, NUM, 0, "Blendin: ", xco+10, yco-64, (width-20)/2, 19, &aa->blendin, 0.0, 32767, 0.0, 0.0, "Number of frames of motion blending");
			uiDefButS(block, NUM, 0, "Priority: ", xco+10+(width-20)/2, yco-64, (width-20)/2, 19, &aa->priority, 0.0, 100.0, 0.0, 0.0, "Execution priority - lower numbers will override actions with higher numbers, With 2 or more actions at once, the overriding channels must be lower in the stack");
			
			uiDefBut(block, TEX, 0, "FrameProp: ",xco+10, yco-84, width-20, 19, aa->frameProp, 0.0, MAX_NAME, 0, 0, "Assign the action's current frame number to this property");

			
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
			if (aa->type == ACT_ACTION_MOTION) {
				uiDefButF(block, NUM, 0, "Cycle: ",xco+30, yco-84, (width-60)/2, 19, &aa->stridelength, 0.0, 2500.0, 0, 0, "Distance covered by a single cycle of the action");
			}
#endif
			
			
			
			yco-=ysize;
			break;
		}
	case ACT_IPO:
		{
			ia= act->data;
			
			ysize= 72;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str = "Ipo types   %t|Play %x0|Ping Pong %x1|Flipper %x2|Loop Stop %x3|Loop End %x4|Property %x6";
			
			uiDefButS(block, MENU, B_REDR, str,		xco+10, yco-24, (width-20)/2, 19, &ia->type, 0, 0, 0, 0, "");

			but = uiDefButBitS(block, TOG, ACT_IPOFORCE, ACT_IPOFORCE, 
				"Force", xco+10+(width-20)/2, yco-24, (width-20)/4-10, 19, 
				&ia->flag, 0, 0, 0, 0, 
				"Apply Ipo as a global or local force depending on the local option (dynamic objects only)"); 
			uiButSetFunc(but, change_ipo_actuator, but, ia);

			but = uiDefButBitS(block, TOG, ACT_IPOADD, ACT_IPOADD, 
				"Add", xco+3*(width-20)/4, yco-24, (width-20)/4-10, 19, 
				&ia->flag, 0, 0, 0, 0, 
				"Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag"); 
			uiButSetFunc(but, change_ipo_actuator, but, ia);
			
			/* Only show the do-force-local toggle if force is requested */
			if (ia->flag & (ACT_IPOFORCE|ACT_IPOADD)) {
				uiDefButBitS(block, TOG, ACT_IPOLOCAL, 0, 
					"L", xco+width-30, yco-24, 20, 19, 
					&ia->flag, 0, 0, 0, 0, 
					"Let the ipo acts in local coordinates, used in Force and Add mode"); 
			}

			if (ia->type==ACT_IPO_FROM_PROP) {
				uiDefBut(block, TEX, 0, 
					"Prop: ",		xco+10, yco-44, width-80, 19, 
					ia->name, 0.0, MAX_NAME, 0, 0,
					"Use this property to define the Ipo position");
			}
			else {
				uiDefButF(block, NUM, 0, 
					"Sta",		xco+10, yco-44, (width-80)/2, 19, 
					&ia->sta, 1.0, MAXFRAMEF, 0, 0, 
					"Start frame");
				uiDefButF(block, NUM, 0, 
					"End",		xco+10+(width-80)/2, yco-44, (width-80)/2, 19, 
					&ia->end, 1.0, MAXFRAMEF, 0, 0, 
					"End frame");
			}
			uiDefButBitS(block, TOG, ACT_IPOCHILD,  B_REDR, 
				"Child",	xco+10+(width-80), yco-44, 60, 19, 
				&ia->flag, 0, 0, 0, 0, 
				"Update IPO on all children Objects as well");
			uiDefBut(block, TEX, 0, 
				"FrameProp: ",		xco+10, yco-64, width-20, 19, 
				ia->frameProp, 0.0, MAX_NAME, 0, 0,
				"Assign the action's current frame number to this property");

			yco-= ysize;
			break;
		}
	case ACT_PROPERTY:
		{
			ysize= 68;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			pa= act->data;
			
			str= "Type%t|Assign%x0|Add %x1|Copy %x2|Toggle (bool/int/float/timer)%x3";
			uiDefButI(block, MENU, B_REDR, str,		xco+30,yco-24,width-60, 19, &pa->type, 0, 31, 0, 0, "Type");
			
			uiDefBut(block, TEX, 1, "Prop: ",		xco+30,yco-44,width-60, 19, pa->name, 0, MAX_NAME, 0, 0, "Property name");
			
			
			if (pa->type==ACT_PROP_TOGGLE) {
				/* no ui */
				ysize -= 22;
			}
			else if (pa->type==ACT_PROP_COPY) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",	xco+10, yco-64, (width-20)/2, 19, &(pa->ob), "Copy from this Object");
				uiDefBut(block, TEX, 1, "Prop: ",		xco+10+(width-20)/2, yco-64, (width-20)/2, 19, pa->value, 0, MAX_NAME, 0, 0, "Copy this property");
			}
			else {
				uiDefBut(block, TEX, 1, "Value: ",		xco+30,yco-64,width-60, 19, pa->value, 0, MAX_NAME, 0, 0, "change with this value, use \"\" around strings");
			}
			yco-= ysize;
			
			break;
		}
	case ACT_SOUND:
		{
			sa = act->data;
			sa->sndnr = 0;
			
			if (sa->flag & ACT_SND_3D_SOUND)
				ysize = 180;
			else
				ysize = 92;

			wval = (width-20)/2;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			if (bmain->sound.first) {
				IDnames_to_pupstring(&str, "Sound files", NULL, &(bmain->sound), (ID *)sa->sound, &(sa->sndnr));
				/* reset this value, it is for handling the event */
				sa->sndnr = 0;
				uiDefButS(block, MENU, B_SOUNDACT_BROWSE, str, xco+10,yco-22,20,19, &(sa->sndnr), 0, 0, 0, 0, "");	
				uiDefButO(block, BUT, "sound.open", 0, "Load Sound", xco+wval+10, yco-22, wval, 19,
				          "Load a sound file (remember to set caching on for small sounds that are played often)");

				if (sa->sound) {
					char dummy_str[] = "Sound mode %t|Play Stop %x0|Play End %x1|Loop Stop %x2|"
					                   "Loop End %x3|Loop Ping Pong Stop %x5|Loop Ping Pong %x4";
					uiDefBut(block, TEX, B_IDNAME, "SO:",xco+30,yco-22,wval-20,19,
					         ((ID *)sa->sound)->name+2, 0.0, MAX_ID_NAME-2, 0, 0, "");
					uiDefButS(block, MENU, 1, dummy_str,xco+10,yco-44,width-20, 19,
					          &sa->type, 0.0, 0.0, 0, 0, "");
					uiDefButF(block, NUM, 0, "Volume:", xco+10,yco-66,wval, 19, &sa->volume,
					          0.0, 1.0, 0, 0, "Sets the volume of this sound");
					uiDefButF(block, NUM, 0, "Pitch:",xco+wval+10,yco-66,wval, 19, &sa->pitch,-12.0,
					          12.0, 0, 0, "Sets the pitch of this sound");
					uiDefButS(block, TOG | BIT, 0, "3D Sound", xco+10, yco-88, width-20, 19,
					          &sa->flag, 0.0, 1.0, 0.0, 0.0, "Plays the sound positioned in 3D space");
					if (sa->flag & ACT_SND_3D_SOUND) {
						uiDefButF(block, NUM, 0, "Minimum Gain: ", xco+10, yco-110, wval, 19,
						          &sa->sound3D.min_gain, 0.0, 1.0, 0.0, 0.0,
						          "The minimum gain of the sound, no matter how far it is away");
						uiDefButF(block, NUM, 0, "Maximum Gain: ", xco+10, yco-132, wval, 19,
						          &sa->sound3D.max_gain, 0.0, 1.0, 0.0, 0.0,
						          "The maximum gain of the sound, no matter how near it is");
						uiDefButF(block, NUM, 0, "Reference Distance: ", xco+10, yco-154, wval, 19,
						          &sa->sound3D.reference_distance, 0.0, FLT_MAX, 0.0, 0.0,
						          "The reference distance is the distance where the sound has a gain of 1.0");
						uiDefButF(block, NUM, 0, "Maximum Distance: ", xco+10, yco-176, wval, 19,
						          &sa->sound3D.max_distance, 0.0, FLT_MAX, 0.0, 0.0,
						          "The maximum distance at which you can hear the sound");
						uiDefButF(block, NUM, 0, "Rolloff: ", xco+wval+10, yco-110, wval, 19,
						          &sa->sound3D.rolloff_factor, 0.0, 5.0, 0.0, 0.0,
						          "The rolloff factor defines the influence factor on volume depending on distance");
						uiDefButF(block, NUM, 0, "Cone Outer Gain: ", xco+wval+10, yco-132, wval, 19,
						          &sa->sound3D.cone_outer_gain, 0.0, 1.0, 0.0, 0.0,
						          "The gain outside the outer cone. The gain in the outer cone will be "
						          "interpolated between this value and the normal gain in the inner cone");
						uiDefButF(block, NUM, 0, "Cone Outer Angle: ", xco+wval+10, yco-154, wval,
						          19, &sa->sound3D.cone_outer_angle, 0.0, 360.0, 0.0, 0.0,
						          "The angle of the outer cone");
						uiDefButF(block, NUM, 0, "Cone Inner Angle: ", xco+wval+10, yco-176, wval,
						          19, &sa->sound3D.cone_inner_angle, 0.0, 360.0, 0.0, 0.0,
						          "The angle of the inner cone");
					}
				}
				MEM_freeN((void *)str);
			} 
			else {
				uiDefButO(block, BUT, "sound.open", 0, "Load Sound", xco+10, yco-22, width-20, 19, "Load a sound file");
			}
					
			yco-= ysize;
			
			break;
		}
	case ACT_CAMERA:

		ysize= 48;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		ca= act->data;

		uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+10, yco-24, (width-20)/2, 19, &(ca->ob), "Look at this Object");
		uiDefButF(block, NUM, 0, "Height:",	xco+10+(width-20)/2, yco-24, (width-20)/2, 19, &ca->height, 0.0, 20.0, 0, 0, "");
		
		uiDefButF(block, NUM, 0, "Min:",	xco+10, yco-44, (width-60)/2, 19, &ca->min, 0.0, 20.0, 0, 0, "");
		
		if (ca->axis==0) ca->axis= 'x';
		uiDefButS(block, ROW, 0, "X",	xco+10+(width-60)/2, yco-44, 20, 19, &ca->axis, 4.0, (float)'x', 0, 0, "Camera tries to get behind the X axis");
		uiDefButS(block, ROW, 0, "Y",	xco+30+(width-60)/2, yco-44, 20, 19, &ca->axis, 4.0, (float)'y', 0, 0, "Camera tries to get behind the Y axis");
		
		uiDefButF(block, NUM, 0, "Max:",	xco+20+(width)/2, yco-44, (width-60)/2, 19, &ca->max, 0.0, 20.0, 0, 0, "");

		yco-= ysize;

		break;

	case ACT_EDIT_OBJECT:
		
		eoa= act->data;

		if (eoa->type==ACT_EDOB_ADD_OBJECT) {
			ysize = 92;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+10, yco-44, (width-20)/2, 19, &(eoa->ob), "Add this Object and all its children (cant be on an visible layer)");
			uiDefButI(block, NUM, 0, "Time:",	xco+10+(width-20)/2, yco-44, (width-20)/2, 19, &eoa->time, 0.0, 2000.0, 0, 0, "Duration the new Object lives");

			wval= (width-60)/3;
			uiDefBut(block, LABEL, 0, "linV",	xco,           yco-68,   45, 19,
					 NULL, 0, 0, 0, 0,
					 "Velocity upon creation");
			uiDefButF(block, NUM, 0, "",		xco+45,        yco-68, wval, 19,
					 eoa->linVelocity, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, x component");
			uiDefButF(block, NUM, 0, "",		xco+45+wval,   yco-68, wval, 19,
					 eoa->linVelocity+1, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, y component");
			uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-68, wval, 19,
					 eoa->linVelocity+2, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, z component");
			uiDefButBitS(block, TOG, ACT_EDOB_LOCAL_LINV, 0, "L", xco+45+3*wval, yco-68, 15, 19,
					 &eoa->localflag, 0.0, 0.0, 0, 0,
					 "Apply the transformation locally");
			
			
			uiDefBut(block, LABEL, 0, "AngV",	xco,           yco-90,   45, 19,
					 NULL, 0, 0, 0, 0,
					 "Angular velocity upon creation");
			uiDefButF(block, NUM, 0, "",		xco+45,        yco-90, wval, 19,
					 eoa->angVelocity, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, x component");
			uiDefButF(block, NUM, 0, "",		xco+45+wval,   yco-90, wval, 19,
					 eoa->angVelocity+1, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, y component");
			uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-90, wval, 19,
					 eoa->angVelocity+2, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, z component");
			uiDefButBitS(block, TOG, ACT_EDOB_LOCAL_ANGV, 0, "L", xco+45+3*wval, yco-90, 15, 19,
					 &eoa->localflag, 0.0, 0.0, 0, 0,
					 "Apply the rotation locally");
					 

		}
		else if (eoa->type==ACT_EDOB_END_OBJECT) {
			ysize= 28;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		}
		else if (eoa->type==ACT_EDOB_REPLACE_MESH) {
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
	 
			uiDefIDPoinBut(block, test_meshpoin_but, ID_ME, 1, "ME:",		xco+40, yco-44, (width-80)/2, 19, &(eoa->me), "replace the existing, when left blank 'Phys' will remake the existing physics mesh");
			
			uiDefButBitS(block, TOGN, ACT_EDOB_REPLACE_MESH_NOGFX, 0, "Gfx",	xco+40 + (width-80)/2, yco-44, (width-80)/4, 19, &eoa->flag, 0, 0, 0, 0, "Replace the display mesh");
			uiDefButBitS(block, TOG, ACT_EDOB_REPLACE_MESH_PHYS, 0, "Phys",	xco+40 + (width-80)/2 +(width-80)/4, yco-44, (width-80)/4, 19, &eoa->flag, 0, 0, 0, 0, "Replace the physics mesh (triangle bounds only. compound shapes not supported)");
		}
		else if (eoa->type==ACT_EDOB_TRACK_TO) {
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
	 
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+10, yco-44, (width-20)/2, 19, &(eoa->ob), "Track to this Object");
			uiDefButI(block, NUM, 0, "Time:",	xco+10+(width-20)/2, yco-44, (width-20)/2-40, 19, &eoa->time, 0.0, 2000.0, 0, 0, "Duration the tracking takes");
			uiDefButS(block, TOG, 0, "3D",	xco+width-50, yco-44, 40, 19, &eoa->flag, 0.0, 0.0, 0, 0, "Enable 3D tracking");
		}
		else if (eoa->type==ACT_EDOB_DYNAMICS) {
			ysize= 69;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Dynamic Operation %t|Restore Dynamics %x0|Suspend Dynamics %x1|Enable Rigid Body %x2|Disable Rigid Body %x3|Set Mass %x4";
			uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-44, (width-80), 19,  &(eoa->dyn_operation), 0.0, 0.0, 0, 0, "");
			if (eoa->dyn_operation==4) {
				uiDefButF(block, NUM, 0, "",		xco+40, yco-63, width-80, 19,
					 &eoa->mass, 0.0, 10000.0, 10, 0,
					 "Mass for object");
			}
		}
		str= "Edit Object %t|Add Object %x0|End Object %x1|Replace Mesh %x2|Track to %x3|Dynamics %x4";
		uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-24, (width-80), 19, &eoa->type, 0.0, 0.0, 0, 0, "");

		yco-= ysize;

		break;

	case ACT_CONSTRAINT:
		coa= act->data;
	
		if (coa->type == ACT_CONST_TYPE_LOC) {
			ysize= 69;

			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
	/*  		str= "Limit %t|None %x0|Loc X %x1|Loc Y %x2|Loc Z %x4|Rot X %x8|Rot Y %x16|Rot Z %x32"; */
	/*			coa->flag &= ~(63); */
			str= "Limit %t|None %x0|Loc X %x1|Loc Y %x2|Loc Z %x4";
			coa->flag &= 7;
			coa->time = 0;
			uiDefButS(block, MENU, 1, str,		xco+10, yco-65, 70, 19, &coa->flag, 0.0, 0.0, 0, 0, "");
		
			uiDefButS(block, NUM,		0, "damp",	xco+10, yco-45, 70, 19, &coa->damp, 0.0, 100.0, 0, 0, "Damping factor: time constant (in frame) of low pass filter");
			uiDefBut(block, LABEL,			0, "Min",	xco+80, yco-45, (width-90)/2, 19, NULL, 0.0, 0.0, 0, 0, "");
			uiDefBut(block, LABEL,			0, "Max",	xco+80+(width-90)/2, yco-45, (width-90)/2, 19, NULL, 0.0, 0.0, 0, 0, "");

			if (coa->flag & ACT_CONST_LOCX) fp= coa->minloc;
			else if (coa->flag & ACT_CONST_LOCY) fp= coa->minloc+1;
			else if (coa->flag & ACT_CONST_LOCZ) fp= coa->minloc+2;
			else if (coa->flag & ACT_CONST_ROTX) fp= coa->minrot;
			else if (coa->flag & ACT_CONST_ROTY) fp= coa->minrot+1;
			else fp= coa->minrot+2;
			
			uiDefButF(block, NUM, 0, "",		xco+80, yco-65, (width-90)/2, 19, fp, -2000.0, 2000.0, 10, 0, "");
			uiDefButF(block, NUM, 0, "",		xco+80+(width-90)/2, yco-65, (width-90)/2, 19, fp+3, -2000.0, 2000.0, 10, 0, "");
		}
		else if (coa->type == ACT_CONST_TYPE_DIST) {
			ysize= 106;

			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Direction %t|None %x0|X axis %x1|Y axis %x2|Z axis %x4|-X axis %x8|-Y axis %x16|-Z axis %x32";
			uiDefButS(block, MENU, B_REDR, str,		xco+10, yco-65, 70, 19, &coa->mode, 0.0, 0.0, 0, 0, "Set the direction of the ray");
		
			uiDefButS(block, NUM,		0, "damp",	xco+10, yco-45, 70, 19, &coa->damp, 0.0, 100.0, 0, 0, "Damping factor: time constant (in frame) of low pass filter");
			uiDefBut(block, LABEL,			0, "Range",	xco+80, yco-45, (width-115)/2, 19, NULL, 0.0, 0.0, 0, 0, "Set the maximum length of ray");
			uiDefButBitS(block, TOG, ACT_CONST_DISTANCE, B_REDR, "Dist",	xco+80+(width-115)/2, yco-45, (width-115)/2, 19, &coa->flag, 0.0, 0.0, 0, 0, "Force distance of object to point of impact of ray");
			uiDefButBitS(block, TOG, ACT_CONST_LOCAL, 0, "L", xco+80+(width-115), yco-45, 25, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Set ray along object's axis or global axis");

			if (coa->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp= coa->minloc;
			else if (coa->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp= coa->minloc+1;
			else fp= coa->minloc+2;

			uiDefButF(block, NUM, 0, "",		xco+80, yco-65, (width-115)/2, 19, fp+3, 0.0, 2000.0, 10, 0, "Maximum length of ray");
			if (coa->flag & ACT_CONST_DISTANCE)
				uiDefButF(block, NUM, 0, "",		xco+80+(width-115)/2, yco-65, (width-115)/2, 19, fp, -2000.0, 2000.0, 10, 0, "Keep this distance to target");
			uiDefButBitS(block, TOG, ACT_CONST_NORMAL, 0, "N", xco+80+(width-115), yco-65, 25, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Set object axis along (local axis) or parallel (global axis) to the normal at hit position");
			uiDefButBitS(block, TOG, ACT_CONST_MATERIAL, B_REDR, "M/P", xco+10, yco-84, 40, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Detect material instead of property");
			if (coa->flag & ACT_CONST_MATERIAL) {
				uiDefBut(block, TEX, 1, "Material:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, MAX_NAME, 0, 0,
					"Ray detects only Objects with this material");
			}
			else {
				uiDefBut(block, TEX, 1, "Property:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, MAX_NAME, 0, 0,
					"Ray detect only Objects with this property");
			}
			uiDefButBitS(block, TOG, ACT_CONST_PERMANENT, 0, "PER", xco+10, yco-103, 40, 19,
				&coa->flag, 0.0, 0.0, 0, 0, "Persistent actuator: stays active even if ray does not reach target");
			uiDefButS(block, NUM, 0, "time", xco+50, yco-103, (width-60)/2, 19, &(coa->time), 0.0, 1000.0, 0, 0, "Maximum activation time in frame, 0 for unlimited");
			uiDefButS(block, NUM, 0, "rotDamp", xco+50+(width-60)/2, yco-103, (width-60)/2, 19, &(coa->rotdamp), 0.0, 100.0, 0, 0, "Use a different damping for orientation");
		}
		else if (coa->type == ACT_CONST_TYPE_ORI) {
			ysize= 87;

			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Direction %t|None %x0|X axis %x1|Y axis %x2|Z axis %x4";
			uiDefButS(block, MENU, B_REDR, str,		xco+10, yco-65, 70, 19, &coa->mode, 0.0, 0.0, 0, 0, "Select the axis to be aligned along the reference direction");
		
			uiDefButS(block, NUM,		0, "damp",	xco+10, yco-45, 70, 19, &coa->damp, 0.0, 100.0, 0, 0, "Damping factor: time constant (in frame) of low pass filter");
			uiDefBut(block, LABEL,			0, "X",	xco+80, yco-45, (width-115)/3, 19, NULL, 0.0, 0.0, 0, 0, "");
			uiDefBut(block, LABEL,			0, "Y",	xco+80+(width-115)/3, yco-45, (width-115)/3, 19, NULL, 0.0, 0.0, 0, 0, "");
			uiDefBut(block, LABEL,			0, "Z",	xco+80+2*(width-115)/3, yco-45, (width-115)/3, 19, NULL, 0.0, 0.0, 0, 0, "");

			uiDefButF(block, NUM, 0, "",		xco+80, yco-65, (width-115)/3, 19, &coa->maxrot[0], -2000.0, 2000.0, 10, 0, "X component of reference direction");
			uiDefButF(block, NUM, 0, "",		xco+80+(width-115)/3, yco-65, (width-115)/3, 19, &coa->maxrot[1], -2000.0, 2000.0, 10, 0, "Y component of reference direction");
			uiDefButF(block, NUM, 0, "",		xco+80+2*(width-115)/3, yco-65, (width-115)/3, 19, &coa->maxrot[2], -2000.0, 2000.0, 10, 0, "Z component of reference direction");

			uiDefButS(block, NUM, 0, "time", xco+10, yco-84, 70, 19, &(coa->time), 0.0, 1000.0, 0, 0, "Maximum activation time in frame, 0 for unlimited");
			uiDefButF(block, NUM, 0, "min", xco+80, yco-84, (width-115)/2, 19, &(coa->minloc[0]), 0.0, 180.0, 10, 1, "Minimum angle (in degree) to maintain with target direction. No correction is done if angle with target direction is between min and max");
			uiDefButF(block, NUM, 0, "max", xco+80+(width-115)/2, yco-84, (width-115)/2, 19, &(coa->maxloc[0]), 0.0, 180.0, 10, 1, "Maximum angle (in degree) allowed with target direction. No correction is done if angle with target direction is between min and max");
		}
		else if (coa->type == ACT_CONST_TYPE_FH) {
			ysize= 106;

			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Direction %t|None %x0|X axis %x1|Y axis %x2|Z axis %x4|-X axis %x8|-Y axis %x16|-Z axis %x32";
			uiDefButS(block, MENU, B_REDR, str,		xco+10, yco-65, 70, 19, &coa->mode, 0.0, 0.0, 0, 0, "Set the direction of the ray (in world coordinate)");

			if (coa->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp= coa->minloc;
			else if (coa->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp= coa->minloc+1;
			else fp= coa->minloc+2;

			uiDefButF(block, NUM,		0, "damp",	xco+10, yco-45, (width-70)/2, 19, &coa->maxrot[0], 0.0, 1.0, 1, 0, "Damping factor of the Fh spring force");
			uiDefButF(block, NUM,		0, "dist",	xco+10+(width-70)/2, yco-45, (width-70)/2, 19, fp, 0.010, 2000.0, 10, 0, "Height of the Fh area");
			uiDefButBitS(block, TOG, ACT_CONST_DOROTFH, 0, "Rot Fh",	xco+10+(width-70), yco-45, 50, 19, &coa->flag, 0.0, 0.0, 0, 0, "Keep object axis parallel to normal");

			uiDefButF(block, NUMSLI, 0, "Fh ",		xco+80, yco-65, (width-115), 19, fp+3, 0.0, 1.0, 0, 0, "Spring force within the Fh area");
			uiDefButBitS(block, TOG, ACT_CONST_NORMAL, 0, "N", xco+80+(width-115), yco-65, 25, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Add a horizontal spring force on slopes");
			uiDefButBitS(block, TOG, ACT_CONST_MATERIAL, B_REDR, "M/P", xco+10, yco-84, 40, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Detect material instead of property");
			if (coa->flag & ACT_CONST_MATERIAL) {
				uiDefBut(block, TEX, 1, "Material:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, MAX_NAME, 0, 0,
					"Ray detects only Objects with this material");
			}
			else {
				uiDefBut(block, TEX, 1, "Property:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, MAX_NAME, 0, 0,
					"Ray detect only Objects with this property");
			}
			uiDefButBitS(block, TOG, ACT_CONST_PERMANENT, 0, "PER", xco+10, yco-103, 40, 19,
				&coa->flag, 0.0, 0.0, 0, 0, "Persistent actuator: stays active even if ray does not reach target");
			uiDefButS(block, NUM, 0, "time", xco+50, yco-103, 90, 19, &(coa->time), 0.0, 1000.0, 0, 0, "Maximum activation time in frame, 0 for unlimited");
			uiDefButF(block, NUM, 0, "rotDamp", xco+140, yco-103, (width-150), 19, &coa->maxrot[1], 0.0, 1.0, 1, 0, "Use a different damping for rotation");
		}
		str= "Constraint Type %t|Location %x0|Distance %x1|Orientation %x2|Force field %x3";
		but = uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-23, (width-80), 19, &coa->type, 0.0, 0.0, 0, 0, "");
		yco-= ysize;
		break;

	case ACT_SCENE:
		sca= act->data;
		
		if (sca->type==ACT_SCENE_RESTART) {
			ysize= 28;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		}
		else if (sca->type==ACT_SCENE_CAMERA) {

			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+40, yco-44, (width-80), 19, &(sca->camera), "Set this Camera. Leave empty to refer to self object");
		}
		else if (sca->type==ACT_SCENE_SET) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Set this Scene");
		}
		else if (sca->type==ACT_SCENE_ADD_FRONT) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Add an Overlay Scene");
		}
		else if (sca->type==ACT_SCENE_ADD_BACK) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Add a Background Scene");
		}
		else if (sca->type==ACT_SCENE_REMOVE) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Remove a Scene");
		}
		else if (sca->type==ACT_SCENE_SUSPEND) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Pause a Scene");
		}
		else if (sca->type==ACT_SCENE_RESUME) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Unpause a Scene");
		}

		str= "Scene %t|Restart %x0|Set Scene %x1|Set Camera %x2|Add OverlayScene %x3|Add BackgroundScene %x4|Remove Scene %x5|Suspend Scene %x6|Resume Scene %x7";
		uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-24, (width-80), 19, &sca->type, 0.0, 0.0, 0, 0, ""); 

		  yco-= ysize; 
		  break; 
	case ACT_GAME:
		{
			gma = act->data;
			if (gma->type == ACT_GAME_LOAD) {
				//ysize = 68;
				ysize = 48;
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
				   uiDefBut(block, TEX, 1, "File: ", xco+10, yco-44,width-20,19, &(gma->filename), 0, sizeof(gma->filename), 0, 0, "Load this blend file, use the \"//\" prefix for a path relative to the current blend file");
//				uiDefBut(block, TEX, 1, "Anim: ", xco+10, yco-64,width-20,19, &(gma->loadaniname), 0, sizeof(gma->loadaniname), 0, 0, "Use this loadinganimation");
			}
#if 0
			else if (gma->type == ACT_GAME_START) {
				ysize = 68; 
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

				   uiDefBut(block, TEX, 1, "File: ", xco+10, yco-44,width-20,19, &(gma->filename), 0, sizeof(gma->filename), 0, 0, "Load this file");
				uiDefBut(block, TEX, 1, "Anim: ", xco+10, yco-64,width-20,19, &(gma->loadaniname), 0, sizeof(gma->loadaniname), 0, 0, "Use this loadinganimation");
			}
#endif
			else if (ELEM4(gma->type, ACT_GAME_RESTART, ACT_GAME_QUIT, ACT_GAME_SAVECFG, ACT_GAME_LOADCFG)) {
				ysize = 28; 
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
			}

			//str = "Scene %t|Load game%x0|Start loaded game%x1|Restart this game%x2|Quit this game %x3";
			str = "Scene %t|Start new game%x0|Restart this game%x2|Quit this game %x3|Save bge.logic.globalDict %x4|Load bge.logic.globalDict %x5";
			uiDefButS(block, MENU, B_REDR, str, xco+40, yco-24, (width-80), 19, &gma->type, 0.0, 0.0, 0, 0, ""); 
			
			yco -= ysize; 
			break; 
		}
	case ACT_GROUP:
		ga= act->data;

		ysize= 52;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		str= "GroupKey types   %t|Set Key %x6|Play %x0|Ping Pong %x1|Flipper %x2|Loop Stop %x3|Loop End %x4|Property %x5";

		uiDefButS(block, MENU, 1, str,			xco+20, yco-24, width-40, 19, &ga->type, 0, 0, 0, 0, "");
		if (ga->type==ACT_GROUP_SET) {
			uiDefBut(block, TEX, 0, "Key: ",		xco+20, yco-44, (width-10)/2, 19, ga->name, 0.0, MAX_NAME, 0, 0, "This name defines groupkey to be set");
			uiDefButI(block, NUM, 0, "Frame:",	xco+20+(width-10)/2, yco-44, (width-70)/2, 19, &ga->sta, 0.0, 2500.0, 0, 0, "Set this frame");
		}
		else if (ga->type==ACT_GROUP_FROM_PROP) {
			uiDefBut(block, TEX, 0, "Prop: ",		xco+20, yco-44, width-40, 19, ga->name, 0.0, MAX_NAME, 0, 0, "Use this property to define the Group position");
		}
		else {
			uiDefButI(block, NUM, 0, "State",		xco+20, yco-44, (width-40)/2, 19, &ga->sta, 0.0, 2500.0, 0, 0, "Start frame");
			uiDefButI(block, NUM, 0, "End",		xco+20+(width-40)/2, yco-44, (width-40)/2, 19, &ga->end, 0.0, 2500.0, 0, 0, "End frame");
		}
		yco-= ysize;
		break;

	case ACT_VISIBILITY:
		ysize = 24;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco,
			 (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		visAct = act->data;

		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOGN, ACT_VISIBILITY_INVISIBLE, B_REDR,
			  "Visible",
			  xco + 10, yco - 20, (width - 20)/3, 19, &visAct->flag,
			  0.0, 0.0, 0, 0,
			  "Set the objects visible. Initialized from the objects render restriction toggle (access in the outliner)");
		uiDefButBitI(block, TOG, ACT_VISIBILITY_OCCLUSION, B_REDR,
			  "Occlusion",
			  xco + 10 + ((width - 20)/3), yco - 20, (width - 20)/3, 19, &visAct->flag,
			  0.0, 0.0, 0, 0,
			  "Set the object to occlude objects behind it. Initialized from the object type in physics button");
		uiBlockEndAlign(block);
		
		uiDefButBitI(block, TOG, ACT_VISIBILITY_RECURSIVE, 0,
			  "Children",
			  xco + 10 + (((width - 20)/3)*2)+10, yco - 20, ((width - 20)/3)-10, 19, &visAct->flag,
			  0.0, 0.0, 0, 0,
			  "Sets all the children of this object to the same visibility/occlusion recursively");

		yco-= ysize;

		break;
		
	case ACT_STATE:
		ysize = 34;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco,
			 (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		staAct = act->data;

		str= "Operation %t|Cpy %x0|Add %x1|Sub %x2|Inv %x3";

		uiDefButI(block, MENU, B_REDR, str,
			  xco + 10, yco - 24, 65, 19, &staAct->type,
			  0.0, 0.0, 0, 0,
			  "Select the bit operation on object state mask");

		for (wval=0; wval<15; wval+=5) {
			uiBlockBeginAlign(block);
			for (stbit=0; stbit<5; stbit++) {
				but = uiDefButBitI(block,  TOG, 1<<(stbit+wval), stbit+wval, "",	(short)(xco+85+12*stbit+13*wval), yco-17, 12, 12, (int *)&(staAct->mask), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+wval)));
				uiButSetFunc(but, check_state_mask, but, &(staAct->mask));
			}
			for (stbit=0; stbit<5; stbit++) {
				but = uiDefButBitI(block, TOG, 1<<(stbit+wval+15), stbit+wval+15, "",	(short)(xco+85+12*stbit+13*wval), yco-29, 12, 12, (int *)&(staAct->mask), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+wval+15)));
				uiButSetFunc(but, check_state_mask, but, &(staAct->mask));
			}
		}
		uiBlockEndAlign(block);

		yco-= ysize;

		break;

	case ACT_RANDOM:
		ysize  = 69;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco,
				  (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		randAct = act->data;

		/* 1. seed */
		uiDefButI(block, NUM, 1, "Seed: ",		(xco+10),yco-24, 0.4 *(width-20), 19,
				 &randAct->seed, 0, 1000, 0, 0,
				 "Initial seed of the random generator. Use Python for more freedom. "
				 " (Choose 0 for not random)");

		/* 2. distribution type */
		/* One pick per distribution. These numbers MUST match the #defines  */
		/* in game.h !!!                                                     */
		str= "Distribution %t|Bool Constant %x0|Bool Uniform %x1"
			"|Bool Bernoulli %x2|Int Constant %x3|Int Uniform %x4"
			"|Int Poisson %x5|Float Constant %x6|Float Uniform %x7"
			"|Float Normal %x8|Float Neg. Exp. %x9";
		uiDefButI(block, MENU, B_REDR, str, (xco+10) + 0.4 * (width-20), yco-24, 0.6 * (width-20), 19,
				 &randAct->distribution, 0.0, 0.0, 0, 0,
				 "Choose the type of distribution");

		/* 3. property */
		uiDefBut(block, TEX, 1, "Property:", (xco+10), yco-44, (width-20), 19,
				 &randAct->propname, 0, MAX_NAME, 0, 0,
				 "Assign the random value to this property"); 

		/*4. and 5. arguments for the distribution*/
		switch (randAct->distribution) {
		case ACT_RANDOM_BOOL_CONST:
			uiDefButBitI(block, TOG, 1, 1, "Always true", (xco+10), yco-64, (width-20), 19,
					 &randAct->int_arg_1, 2.0, 1, 0, 0,
					 "Always false or always true");			
			break;
		case ACT_RANDOM_BOOL_UNIFORM:
			uiDefBut(block, LABEL, 0, "     Do a 50-50 pick",	(xco+10), yco-64, (width-20), 19,
					 NULL, 0, 0, 0, 0,
					 "Choose between true and false, 50% chance each");
			break;
		case ACT_RANDOM_BOOL_BERNOUILLI:
			uiDefButF(block, NUM, 1, "Chance", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.0, 1.0, 0, 0,
					 "Pick a number between 0 and 1. Success if you stay "
					 "below this value");			
			break;
		case ACT_RANDOM_INT_CONST:
			uiDefButI(block, NUM, 1, "Value: ",		(xco+10), yco-64, (width-20), 19,
					 &randAct->int_arg_1, -1000, 1000, 0, 0,
					 "Always return this number");
			break;
		case ACT_RANDOM_INT_UNIFORM:
			uiDefButI(block, NUM, 1, "Min: ",		(xco+10), yco-64, (width-20)/2, 19,
					 &randAct->int_arg_1, -1000, 1000, 0, 0,
					 "Choose a number from a range. "
					 "Lower boundary of the range");
			uiDefButI(block, NUM, 1, "Max: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->int_arg_2, -1000, 1000, 0, 0,
					 "Choose a number from a range. "
					 "Upper boundary of the range");
			break;
		case ACT_RANDOM_INT_POISSON:
			uiDefButF(block, NUM, 1, "Mean: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.01, 100.0, 0, 0,
					 "Expected mean value of the distribution");						
			break;
		case ACT_RANDOM_FLOAT_CONST:
			uiDefButF(block, NUM, 1, "Value: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.0, 1.0, 0, 0,
					 "Always return this number");
			break;
		case ACT_RANDOM_FLOAT_UNIFORM:
			uiDefButF(block, NUM, 1, "Min: ",		(xco+10), yco-64, (width-20)/2, 19,
					 &randAct->float_arg_1, -10000.0, 10000.0, 0, 0,
					 "Choose a number from a range"
					 "Lower boundary of the range");
			uiDefButF(block, NUM, 1, "Max: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->float_arg_2, -10000.0, 10000.0, 0, 0,
					 "Choose a number from a range"
					 "Upper boundary of the range");
			break;
		case ACT_RANDOM_FLOAT_NORMAL:
			uiDefButF(block, NUM, 1, "Mean: ",		(xco+10), yco-64, (width-20)/2, 19,
					 &randAct->float_arg_1, -10000.0, 10000.0, 0, 0,
					 "A normal distribution. Mean of the distribution");
			uiDefButF(block, NUM, 1, "SD: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->float_arg_2, 0.0, 10000.0, 0, 0,
					 "A normal distribution. Standard deviation of the "
					 "distribution");
			break;
		case ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL:
			uiDefButF(block, NUM, 1, "Half-life time: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.001, 10000.0, 0, 0,
					 "Negative exponential dropoff");
			break;
		default:
			; /* don't know what this distro is... can be useful for testing */
			/* though :)                                                     */
		}

		yco-= ysize;
		break;
	case ACT_MESSAGE:
		ma = act->data;

		ysize = 4 + (3 * 24); /* footer + number of lines * 24 pixels/line */
	
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco,	    (float)yco-ysize,
				 (float)xco+width,  (float)yco, 1);

		myline=1;

		/* line 1: To */
		uiDefBut(block, TEX, 1, "To: ",
			(xco+10), (yco-(myline++*24)), (width-20), 19,
			&ma->toPropName, 0, MAX_NAME, 0, 0,
			"Optional send message to objects with this name only, or empty to broadcast");

		/* line 2: Message Subject */
		uiDefBut(block, TEX, 1, "Subject: ",
		(xco+10), (yco-(myline++*24)), (width-20), 19,
		&ma->subject, 0, MAX_NAME, 0, 0,
		"Optional message subject. This is what can be filtered on");

		/* line 3: Text/Property */
		uiDefButBitS(block, TOG, 1, B_REDR, "T/P",
			(xco+10),(yco-(myline*24)), (0.20 * (width-20)), 19,
			&ma->bodyType, 0.0, 0.0, 0, 0,
			"Toggle message type: either Text or a PropertyName");

		if (ma->bodyType == ACT_MESG_MESG) {
			/* line 3: Message Body */
			uiDefBut(block, TEX, 1, "Body: ",
			(xco+10+(0.20*(width-20))),(yco-(myline++*24)),(0.8*(width-20)),19,
			&ma->body, 0, MAX_NAME, 0, 0,
			"Optional message body Text");
		}
		else {
			/* line 3: Property body (set by property) */
			uiDefBut(block, TEX, 1, "Propname: ",
			(xco+10+(0.20*(width-20))),(yco-(myline++*24)),(0.8*(width-20)),19,
			&ma->body, 0, MAX_NAME, 0, 0,
			"The message body will be set by the Property Value");
		}
		
		yco -= ysize;
		break;
	case ACT_2DFILTER:
		tdfa = act->data;

		ysize = 50;
		if (tdfa->type == ACT_2DFILTER_CUSTOMFILTER) {
			ysize +=20;
		}
		glRects( xco, yco-ysize, xco+width, yco ); 
		uiEmboss( (float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1 );

		switch(tdfa->type)
		{
			case ACT_2DFILTER_MOTIONBLUR:
				if (!tdfa->flag) {
					uiDefButS(block, TOG, B_REDR, "D",	xco+30,yco-44,19, 19, &tdfa->flag, 0.0, 0.0, 0.0, 0.0, "Disable Motion Blur");
					uiDefButF(block, NUM, B_REDR, "Value:", xco+52,yco-44,width-82,19,&tdfa->float_arg,0.0,1.0,0.0,0.0,"Set motion blur value");
				}
				else {
					uiDefButS(block, TOG, B_REDR, "Disabled",	xco+30,yco-44,width-60, 19, &tdfa->flag, 0.0, 0.0, 0.0, 0.0, "Enable Motion Blur");
				}
				break;
			case ACT_2DFILTER_BLUR:
			case ACT_2DFILTER_SHARPEN:
			case ACT_2DFILTER_DILATION:
			case ACT_2DFILTER_EROSION:
			case ACT_2DFILTER_LAPLACIAN:
			case ACT_2DFILTER_SOBEL:
			case ACT_2DFILTER_PREWITT:
			case ACT_2DFILTER_GRAYSCALE:
			case ACT_2DFILTER_SEPIA:
			case ACT_2DFILTER_INVERT:
			case ACT_2DFILTER_NOFILTER:
			case ACT_2DFILTER_DISABLED:
			case ACT_2DFILTER_ENABLED:
				uiDefButI(block, NUM, B_REDR, "Pass Number:", xco+30,yco-44,width-60,19,&tdfa->int_arg,0.0,MAX_RENDER_PASS-1,0.0,0.0,"Set filter order");
				break;
			case ACT_2DFILTER_CUSTOMFILTER:
				uiDefButI(block, NUM, B_REDR, "Pass Number:", xco+30,yco-44,width-60,19,&tdfa->int_arg,0.0,MAX_RENDER_PASS-1,0.0,0.0,"Set filter order");
				uiDefIDPoinBut(block, test_scriptpoin_but, ID_SCRIPT, 1, "Script: ", xco+30,yco-64,width-60, 19, &tdfa->text, "");
				break;
		}
		
		str= "2D Filter   %t|Motion Blur   %x1|Blur %x2|Sharpen %x3|Dilation %x4|Erosion %x5|"
				"Laplacian %x6|Sobel %x7|Prewitt %x8|Gray Scale %x9|Sepia %x10|Invert %x11|Custom Filter %x12|"
				"Enable Filter %x-2|Disable Filter %x-1|Remove Filter %x0|";
		uiDefButS(block, MENU, B_REDR, str,	xco+30,yco-24,width-60, 19, &tdfa->type, 0.0, 0.0, 0.0, 0.0, "2D filter type");
		
		yco -= ysize;
		break;
	case ACT_PARENT:
		parAct = act->data;

		if (parAct->type==ACT_PARENT_SET) {
			
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+95, yco-24, (width-100), 19, &(parAct->ob), "Set this object as parent");
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOGN, ACT_PARENT_COMPOUND, B_REDR,
			"Compound",
			xco + 5, yco - 44, (width - 10)/2, 19, &parAct->flag,
			0.0, 0.0, 0, 0,
			"Add this object shape to the parent shape (only if the parent shape is already compound)");
			uiDefButBitS(block, TOGN, ACT_PARENT_GHOST, B_REDR,
			"Ghost",
			xco + 5 + ((width - 10)/2), yco - 44, (width - 10)/2, 19, &parAct->flag,
			0.0, 0.0, 0, 0,
			"Make this object ghost while parented (only if not compound)");
			uiBlockEndAlign(block);
		}
		else if (parAct->type==ACT_PARENT_REMOVE) {

			ysize= 28;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		}

		str= "Parent %t|Set Parent %x0|Remove Parent %x1";
		uiDefButI(block, MENU, B_REDR, str,		xco+5, yco-24, parAct->type==1?(width-80):90, 19, &parAct->type, 0.0, 0.0, 0, 0, ""); 

		yco-= ysize;
		break;
	case ACT_ARMATURE:
		armAct = act->data;

		if (ob->type == OB_ARMATURE) {
			str= "Constraint %t|Run armature %x0|Enable %x1|Disable %x2|Set target %x3|Set weight %x4";
			uiDefButI(block, MENU, B_REDR, str,		xco+5, yco-24, (width-10)*0.35, 19, &armAct->type, 0.0, 0.0, 0, 0, ""); 

			switch (armAct->type) {
			case ACT_ARM_RUN:
				ysize = 28;
				break;
			default:
				uiBlockBeginAlign(block);
				but = uiDefBut(block, TEX, 1, "Bone: ",
						(xco+5), (yco-44), (width-10)/2, 19,
						armAct->posechannel, 0, MAX_NAME, 0, 0,
						"Bone on which the constraint is defined");
				uiButSetFunc(but, check_armature_actuator, but, armAct);
				but = uiDefBut(block, TEX, 1, "Cons: ",
						(xco+5)+(width-10)/2, (yco-44), (width-10)/2, 19,
						armAct->constraint, 0, MAX_NAME, 0, 0,
						"Name of the constraint you want to control");
				uiButSetFunc(but, check_armature_actuator, but, armAct);
				uiBlockEndAlign(block);
				ysize = 48;
				switch (armAct->type) {
				case ACT_ARM_SETTARGET:
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "Target: ",		xco+5, yco-64, (width-10), 19, &(armAct->target), "Set this object as the target of the constraint"); 
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "Secondary Target: ",		xco+5, yco-84, (width-10), 19, &(armAct->subtarget), "Set this object as the secondary target of the constraint (only IK polar target at the moment)"); 
					ysize += 40;
					break;
				case ACT_ARM_SETWEIGHT:
					uiDefButF(block, NUM, B_REDR, "Weight:", xco+5+(width-10)*0.35,yco-24,(width-10)*0.65,19,&armAct->weight,0.0,1.0,0.0,0.0,"Set weight of this constraint");
					break;
				}
			}
		  }
		glRects(xco, yco-ysize, xco+width, yco); 
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
		yco-= ysize;
		break;

	 default:
		ysize= 4;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		yco-= ysize;
		break;
	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	return yco-4;
}

static void do_sensor_menu(bContext *C, void *UNUSED(arg), int event)
{	
	SpaceLogic *slogic= CTX_wm_space_logic(C);
	ID **idar;
	Object *ob;
	bSensor *sens;
	short count, a;
	
	idar= get_selected_and_linked_obs(C, &count, slogic->scaflag);
	
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if (event==0 || event==2) ob->scaflag |= OB_SHOWSENS;
		else if (event==1) ob->scaflag &= ~OB_SHOWSENS;
	}
		
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		sens= ob->sensors.first;
		while (sens) {
			if (event==2) sens->flag |= SENS_SHOW;
			else if (event==3) sens->flag &= ~SENS_SHOW;
			sens= sens->next;
		}
	}

	if (idar) MEM_freeN(idar);
}

static uiBlock *sensor_menu(bContext *C, ARegion *ar, void *UNUSED(arg))
{
	uiBlock *block;
	int yco=0;
	
	block= uiBeginBlock(C, ar, __func__, UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_sensor_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",	0, (short)(yco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Sensors",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Sensors",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);
	
	return block;
}

static void do_controller_menu(bContext *C, void *UNUSED(arg), int event)
{	
	SpaceLogic *slogic= CTX_wm_space_logic(C);
	ID **idar;
	Object *ob;
	bController *cont;
	short count, a;
	
	idar= get_selected_and_linked_obs(C, &count, slogic->scaflag);
	
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if (event==0 || event==2) ob->scaflag |= OB_SHOWCONT;
		else if (event==1) ob->scaflag &= ~OB_SHOWCONT;
	}

	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		cont= ob->controllers.first;
		while (cont) {
			if (event==2) cont->flag |= CONT_SHOW;
			else if (event==3) cont->flag &= ~CONT_SHOW;
			cont= cont->next;
		}
	}

	if (idar) MEM_freeN(idar);
}

static uiBlock *controller_menu(bContext *C, ARegion *ar, void *UNUSED(arg))
{
	uiBlock *block;
	int yco=0;
	
	block= uiBeginBlock(C, ar, __func__, UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_controller_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0,(short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",					0, (short)(yco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Controllers",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 2, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Controllers",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 3, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);
	
	return block;
}

static void do_actuator_menu(bContext *C, void *UNUSED(arg), int event)
{	
	SpaceLogic *slogic= CTX_wm_space_logic(C);
	ID **idar;
	Object *ob;
	bActuator *act;
	short count, a;
	
	idar= get_selected_and_linked_obs(C, &count, slogic->scaflag);
	
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if (event==0 || event==2) ob->scaflag |= OB_SHOWACT;
		else if (event==1) ob->scaflag &= ~OB_SHOWACT;
	}

	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		act= ob->actuators.first;
		while (act) {
			if (event==2) act->flag |= ACT_SHOW;
			else if (event==3) act->flag &= ~ACT_SHOW;
			act= act->next;
		}
	}

	if (idar) MEM_freeN(idar);
}

static uiBlock *actuator_menu(bContext *C, ARegion *ar, void *UNUSED(arg))
{
	uiBlock *block;
	int xco=0;
	
	block= uiBeginBlock(C, ar, __func__, UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_actuator_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",	0, (short)(xco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Actuators",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Actuators",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);
	
	return block;
}



static void check_controller_state_mask(bContext *UNUSED(C), void *arg1_but, void *arg2_mask)
{
	unsigned int *cont_mask = arg2_mask;
	uiBut *but = arg1_but;
	
	/* a controller is always in a single state */
	*cont_mask = (1<<but->retval);
	but->retval = B_REDR;
}

static int first_bit(unsigned int mask)
{
	int bit;

	for (bit=0; bit<32; bit++) {
		if (mask & (1<<bit))
			return bit;
	}
	return -1;
}

static uiBlock *controller_state_mask_menu(bContext *C, ARegion *ar, void *arg_cont)
{
	uiBlock *block;
	uiBut *but;
	bController *cont = arg_cont;

	short yco = 12, xco = 0, stbit, offset;

	block= uiBeginBlock(C, ar, __func__, UI_EMBOSS);

	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-5, -5, 200, 34, NULL, 0, 0, 0, 0, "");
	
	for (offset=0; offset<15; offset+=5) {
		uiBlockBeginAlign(block);
		for (stbit=0; stbit<5; stbit++) {
			but = uiDefButBitI(block, TOG, (1<<(stbit+offset)), (stbit+offset), "",	(short)(xco+12*stbit+13*offset), yco, 12, 12, (int *)&(cont->state_mask), 0, 0, 0, 0, "");
			uiButSetFunc(but, check_controller_state_mask, but, &(cont->state_mask));
		}
		for (stbit=0; stbit<5; stbit++) {
			but = uiDefButBitI(block, TOG, (1<<(stbit+offset+15)), (stbit+offset+15), "",	(short)(xco+12*stbit+13*offset), yco-12, 12, 12, (int *)&(cont->state_mask), 0, 0, 0, 0, "");
			uiButSetFunc(but, check_controller_state_mask, but, &(cont->state_mask));
		}
	}
	uiBlockEndAlign(block);

	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);

	return block;
}

static void do_object_state_menu(bContext *UNUSED(C), void *arg, int event)
{	
	Object *ob = arg;

	switch (event) {
	case 0:
		ob->state = 0x3FFFFFFF;
		break;
	case 1:
		ob->state = ob->init_state;
		if (!ob->state)
			ob->state = 1;
		break;
	case 2:
		ob->init_state = ob->state;
		break;
	}
}

static uiBlock *object_state_mask_menu(bContext *C, ARegion *ar, void *arg_obj)
{
	uiBlock *block;
	short xco = 0;

	block= uiBeginBlock(C, ar, __func__, UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_object_state_menu, arg_obj);
	
	uiDefBut(block, BUTM, 1, "Set all bits",		0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Recall init state",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",					0, (short)(xco-=6),	 160, 6,  NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Store init state",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_TOP);
	uiEndBlock(C, block);
	
	return block;
}

static int is_sensor_linked(uiBlock *block, bSensor *sens)
{
	bController *cont;
	int i;

	for (i=0; i<sens->totlinks; i++) {
		cont = sens->links[i];
		if (uiFindInlink(block, cont) != NULL)
			return 1;
	}
	return 0;
}

/* Sensors code */

static void draw_sensor_header(uiLayout *layout, PointerRNA *ptr, PointerRNA *logic_ptr)
{
	uiLayout *box, *row, *sub;
	bSensor *sens= (bSensor *)ptr->data;
	
	box= uiLayoutBox(layout);
	row= uiLayoutRow(box, 0);
	
	uiItemR(row, ptr, "show_expanded", UI_ITEM_R_NO_BG, "", ICON_NONE);
	if (RNA_boolean_get(ptr, "show_expanded")) {
		uiItemR(row, ptr, "type", 0, "", ICON_NONE);
		uiItemR(row, ptr, "name", 0, "", ICON_NONE);
	}
	else {
		uiItemL(row, sensor_name(sens->type), ICON_NONE);
		uiItemL(row, sens->name, ICON_NONE);
	}

	sub= uiLayoutRow(row, 0);
	uiLayoutSetActive(sub, ((RNA_boolean_get(logic_ptr, "show_sensors_active_states")
							&& RNA_boolean_get(ptr, "show_expanded")) || RNA_boolean_get(ptr, "pin")));
	uiItemR(sub, ptr, "pin", UI_ITEM_R_NO_BG, "", ICON_NONE);

	if (RNA_boolean_get(ptr, "show_expanded")==0) {
		sub= uiLayoutRow(row, 1);
		uiItemEnumO(sub, "LOGIC_OT_sensor_move", "", ICON_TRIA_UP, "direction", 1); // up
		uiItemEnumO(sub, "LOGIC_OT_sensor_move", "", ICON_TRIA_DOWN, "direction", 2); // down
	}

	uiItemO(row, "", ICON_X, "LOGIC_OT_sensor_remove");
}

static void draw_sensor_internal_header(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *box, *split, *sub, *row;

	box= uiLayoutBox(layout);
	split = uiLayoutSplit(box, 0.45, 0);
	
	row= uiLayoutRow(split, 1);
	uiItemR(row, ptr, "use_pulse_true_level", 0, "", ICON_DOTSUP);
	uiItemR(row, ptr, "use_pulse_false_level", 0, "", ICON_DOTSDOWN);

	sub=uiLayoutRow(row, 0);
	uiLayoutSetActive(sub, (RNA_boolean_get(ptr, "use_pulse_true_level")
							|| RNA_boolean_get(ptr, "use_pulse_false_level")));
	uiItemR(sub, ptr, "frequency", 0, "Freq", ICON_NONE);
	
	row= uiLayoutRow(split, 1);
	uiItemR(row, ptr, "use_level", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	uiItemR(row, ptr, "use_tap", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	
	uiItemR(split, ptr, "invert", UI_ITEM_R_TOGGLE, "Invert", ICON_NONE);
}
/* sensors in alphabetical order */

static void draw_sensor_actuator(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);
	uiItemPointerR(layout, ptr, "actuator", &settings_ptr, "actuators", NULL, ICON_LOGIC);
}

static void draw_sensor_armature(uiLayout *layout, PointerRNA *ptr)
{
	bSensor *sens = (bSensor*)ptr->data;
	bArmatureSensor *as = (bArmatureSensor *) sens->data;
	Object *ob = (Object *)ptr->id.data;
	PointerRNA pose_ptr, pchan_ptr;
	PropertyRNA *bones_prop= NULL;
	uiLayout *row;

	if (ob->type != OB_ARMATURE) {
		uiItemL(layout, "Sensor only available for armatures", ICON_NONE);
		return;
	}

	if (ob->pose) {
		RNA_pointer_create((ID *)ob, &RNA_Pose, ob->pose, &pose_ptr);
		bones_prop = RNA_struct_find_property(&pose_ptr, "bones");
	}

	if (&pose_ptr.data) {
		uiItemPointerR(layout, ptr, "bone", &pose_ptr, "bones", NULL, ICON_BONE_DATA);

		if (RNA_property_collection_lookup_string(&pose_ptr, bones_prop, as->posechannel, &pchan_ptr))
			uiItemPointerR(layout, ptr, "constraint", &pchan_ptr, "constraints", NULL, ICON_CONSTRAINT_BONE);
	}
	row = uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "test_type", 0, NULL, ICON_NONE);
	if (RNA_enum_get(ptr, "test_type") != SENS_ARM_STATE_CHANGED)
		uiItemR(row, ptr, "value", 0, NULL, ICON_NONE);
}

static void draw_sensor_collision(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *row, *split;
	PointerRNA main_ptr;

	RNA_main_pointer_create(CTX_data_main(C), &main_ptr);

	split = uiLayoutSplit(layout, 0.3, 0);
	row = uiLayoutRow(split, 1);
	uiItemR(row, ptr, "use_pulse", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	uiItemR(row, ptr, "use_material", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

	switch (RNA_boolean_get(ptr, "use_material")) {
		case SENS_COLLISION_PROPERTY:
			uiItemR(split, ptr, "property", 0, NULL, ICON_NONE);
			break;
		case SENS_COLLISION_MATERIAL:
			uiItemPointerR(split, ptr, "material", &main_ptr, "materials", NULL, ICON_MATERIAL_DATA);
			break;
	}
}

static void draw_sensor_delay(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;
	
	row= uiLayoutRow(layout, 0);

	uiItemR(row, ptr, "delay", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "duration", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "use_repeat", 0, NULL, ICON_NONE);
}

static void draw_sensor_joystick(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *col, *row;

	uiItemR(layout, ptr, "joystick_index", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "event_type", 0, NULL, ICON_NONE);

	switch (RNA_enum_get(ptr, "event_type")) {
		case SENS_JOY_BUTTON:
			uiItemR(layout, ptr, "use_all_events", 0, NULL, ICON_NONE);

			col = uiLayoutColumn(layout, 0);
			uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_all_events")==0);
			uiItemR(col, ptr, "button_number", 0, NULL, ICON_NONE);
			break;
		case SENS_JOY_AXIS:
			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "axis_number", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "axis_threshold", 0, NULL, ICON_NONE);

			uiItemR(layout, ptr, "use_all_events", 0, NULL, ICON_NONE);
			col = uiLayoutColumn(layout, 0);
			uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_all_events")==0);
			uiItemR(col, ptr, "axis_direction", 0, NULL, ICON_NONE);
			break;
		case SENS_JOY_HAT:
			uiItemR(layout, ptr, "hat_number", 0, NULL, ICON_NONE);
			uiItemR(layout, ptr, "use_all_events", 0, NULL, ICON_NONE);

			col = uiLayoutColumn(layout, 0);
			uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_all_events")==0);
			uiItemR(col, ptr, "hat_direction", 0, NULL, ICON_NONE);
			break;
		case SENS_JOY_AXIS_SINGLE:
			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "single_axis_number", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "axis_threshold", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_sensor_keyboard(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;
	uiLayout *row, *col;

	row = uiLayoutRow(layout, 0);
	uiItemL(row, "Key:", ICON_NONE);
	col = uiLayoutColumn(row, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_all_keys")==0);
	uiItemR(col, ptr, "key", UI_ITEM_R_EVENT, "", ICON_NONE);
	col = uiLayoutColumn(row, 0);
	uiItemR(col, ptr, "use_all_keys", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_all_keys")==0);
	row = uiLayoutRow(col, 0);
	uiItemL(row, "First Modifier:", ICON_NONE);
	uiItemR(row, ptr, "modifier_key_1", UI_ITEM_R_EVENT, "", ICON_NONE);
	
	row = uiLayoutRow(col, 0);
	uiItemL(row, "Second Modifier:", ICON_NONE);
	uiItemR(row, ptr, "modifier_key_2", UI_ITEM_R_EVENT, "", ICON_NONE);

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);
	uiItemPointerR(layout, ptr, "log", &settings_ptr, "properties", NULL, ICON_NONE);
	uiItemPointerR(layout, ptr, "target", &settings_ptr, "properties", NULL, ICON_NONE);
}

static void draw_sensor_message(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "subject", 0, NULL, ICON_NONE);
}

static void draw_sensor_mouse(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mouse_event", 0, NULL, ICON_NONE);
}

static void draw_sensor_near(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;

	uiItemR(layout, ptr, "property", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "distance", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "reset_distance", 0, NULL, ICON_NONE);
}

static void draw_sensor_property(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;

	uiLayout *row;
	uiItemR(layout, ptr, "evaluation_type", 0, NULL, ICON_NONE);

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);
	uiItemPointerR(layout, ptr, "property", &settings_ptr, "properties", NULL, ICON_NONE);

	switch (RNA_enum_get(ptr, "evaluation_type")) {
		case SENS_PROP_INTERVAL:
			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "value_min", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "value_max", 0, NULL, ICON_NONE);
			break;
		case SENS_PROP_EQUAL:
			uiItemR(layout, ptr, "value", 0, NULL, ICON_NONE);
			break;
		case SENS_PROP_NEQUAL:
			uiItemR(layout, ptr, "value", 0, NULL, ICON_NONE);
			break;
		case SENS_PROP_CHANGED:
			break;
	}
}

static void draw_sensor_radar(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;

	uiItemR(layout, ptr, "property", 0, NULL, ICON_NONE);
	uiItemR(layout, ptr, "axis", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "angle", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "distance", 0, NULL, ICON_NONE);
}

static void draw_sensor_random(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "seed", 0, NULL, ICON_NONE);
}

static void draw_sensor_ray(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *split, *row;
	PointerRNA main_ptr;

	RNA_main_pointer_create(CTX_data_main(C), &main_ptr);
	split= uiLayoutSplit(layout, 0.3, 0);
	uiItemR(split, ptr, "ray_type", 0, "", ICON_NONE);
	switch (RNA_enum_get(ptr, "ray_type")) {
		case SENS_RAY_PROPERTY:
			uiItemR(split, ptr, "property", 0, "", ICON_NONE);
			break;
		case SENS_RAY_MATERIAL:
			uiItemPointerR(split, ptr, "material", &main_ptr, "materials", "", ICON_MATERIAL_DATA);
			break;
	}

	split= uiLayoutSplit(layout, 0.3, 0);
	uiItemR(split, ptr, "axis", 0, "", ICON_NONE);
	row= uiLayoutRow(split, 0);	
	uiItemR(row, ptr, "range", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "use_x_ray", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
}

static void draw_sensor_touch(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "material", 0, NULL, ICON_NONE);
}

static void draw_brick_sensor(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *box;
	
	if (!RNA_boolean_get(ptr, "show_expanded"))
		return;

	draw_sensor_internal_header(layout, ptr);
	
	box = uiLayoutBox(layout);

	switch (RNA_enum_get(ptr, "type")) {

		case SENS_ACTUATOR:
			draw_sensor_actuator(box, ptr);
			break;
		case SENS_ALWAYS:
			break;
		case SENS_ARMATURE:
			draw_sensor_armature(box, ptr);
			break;
		case SENS_COLLISION:
			draw_sensor_collision(box, ptr, C);
			break;
		case SENS_DELAY:
			draw_sensor_delay(box, ptr);
			break;
		case SENS_JOYSTICK:
			draw_sensor_joystick(box, ptr);
			break;
		case SENS_KEYBOARD:
			draw_sensor_keyboard(box, ptr);
			break;
		case SENS_MESSAGE:
			draw_sensor_message(box, ptr);
			break;
		case SENS_MOUSE:
			draw_sensor_mouse(box, ptr);
			break;
		case SENS_NEAR:
			draw_sensor_near(box, ptr);
			break;
		case SENS_PROPERTY:
			draw_sensor_property(box, ptr);
			break;
		case SENS_RADAR:
			draw_sensor_radar(box, ptr);
			break;
		case SENS_RANDOM:
			draw_sensor_random(box, ptr);
			break;
		case SENS_RAY:
			draw_sensor_ray(box, ptr, C);
			break;
		case SENS_TOUCH:
			draw_sensor_touch(box, ptr);
			break;
	}
}

/* Controller code */
static void draw_controller_header(uiLayout *layout, PointerRNA *ptr, int xco, int width, int yco)
{
	uiLayout *box, *row, *sub;
	bController *cont= (bController *)ptr->data;

	char state[3];
	BLI_snprintf(state, sizeof(state), "%d", RNA_int_get(ptr, "states"));
	
	box= uiLayoutBox(layout);
	row= uiLayoutRow(box, 0);
	
	uiItemR(row, ptr, "show_expanded", UI_ITEM_R_NO_BG, "", ICON_NONE);
	if (RNA_boolean_get(ptr, "show_expanded")) {
		uiItemR(row, ptr, "type", 0, "", ICON_NONE);
		uiItemR(row, ptr, "name", 0, "", ICON_NONE);
		/* XXX provisory for Blender 2.50Beta */
		uiDefBlockBut(uiLayoutGetBlock(layout), controller_state_mask_menu, cont, state, (short)(xco+width-44), yco, 22+22, UI_UNIT_Y, "Set controller state index (from 1 to 30)");
	}
	else {
		uiItemL(row, controller_name(cont->type), ICON_NONE);
		uiItemL(row, cont->name, ICON_NONE);
		uiItemL(row, state, ICON_NONE);
	}

	uiItemR(row, ptr, "use_priority", 0, "", ICON_NONE);

	if (RNA_boolean_get(ptr, "show_expanded")==0) {
		sub= uiLayoutRow(row, 1);
		uiItemEnumO(sub, "LOGIC_OT_controller_move", "", ICON_TRIA_UP, "direction", 1); // up
		uiItemEnumO(sub, "LOGIC_OT_controller_move", "", ICON_TRIA_DOWN, "direction", 2); // down
	}
	uiItemO(row, "", ICON_X, "LOGIC_OT_controller_remove");
}

static void draw_controller_expression(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "expression", 0, "", ICON_NONE);
}

static void draw_controller_python(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *split, *sub;

	split = uiLayoutSplit(layout, 0.3, 1);
	uiItemR(split, ptr, "mode", 0, "", ICON_NONE);
	if (RNA_enum_get(ptr, "mode") == CONT_PY_SCRIPT) {
		uiItemR(split, ptr, "text", 0, "", ICON_NONE);
	}
	else {
		sub = uiLayoutSplit(split, 0.8, 0);
		uiItemR(sub, ptr, "module", 0, "", ICON_NONE);
		uiItemR(sub, ptr, "use_debug", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	}
}

static void draw_controller_state(uiLayout *UNUSED(layout), PointerRNA *UNUSED(ptr))
{

}

static void draw_brick_controller(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *box;
	
	if (!RNA_boolean_get(ptr, "show_expanded"))
		return;
	
	box = uiLayoutBox(layout);

	draw_controller_state(box, ptr);
	
	switch (RNA_enum_get(ptr, "type")) {
		case CONT_LOGIC_AND:
			break;
		case CONT_LOGIC_OR:
			break;
		case CONT_EXPRESSION:
			draw_controller_expression(box, ptr);
			break;
		case CONT_PYTHON:
			draw_controller_python(box, ptr);
			break;
		case CONT_LOGIC_NAND:
			break;
		case CONT_LOGIC_NOR:
			break;
		case CONT_LOGIC_XOR:
			break;
		case CONT_LOGIC_XNOR:
			break;
	}
}

/* Actuator code */
static void draw_actuator_header(uiLayout *layout, PointerRNA *ptr, PointerRNA *logic_ptr)
{
	uiLayout *box, *row, *sub;
	bActuator *act= (bActuator *)ptr->data;
	
	box= uiLayoutBox(layout);
	row= uiLayoutRow(box, 0);
	
	uiItemR(row, ptr, "show_expanded", UI_ITEM_R_NO_BG, "", ICON_NONE);
	if (RNA_boolean_get(ptr, "show_expanded")) {
		uiItemR(row, ptr, "type", 0, "", ICON_NONE);
		uiItemR(row, ptr, "name", 0, "", ICON_NONE);
	}
	else {
		uiItemL(row, actuator_name(act->type), ICON_NONE);
		uiItemL(row, act->name, ICON_NONE);
	}

	sub= uiLayoutRow(row, 0);
	uiLayoutSetActive(sub, ((RNA_boolean_get(logic_ptr, "show_actuators_active_states")
							&& RNA_boolean_get(ptr, "show_expanded")) || RNA_boolean_get(ptr, "pin")));
	uiItemR(sub, ptr, "pin", UI_ITEM_R_NO_BG, "", ICON_NONE);

	if (RNA_boolean_get(ptr, "show_expanded")==0) {
		sub= uiLayoutRow(row, 1);
		uiItemEnumO(sub, "LOGIC_OT_actuator_move", "", ICON_TRIA_UP, "direction", 1); // up
		uiItemEnumO(sub, "LOGIC_OT_actuator_move", "", ICON_TRIA_DOWN, "direction", 2); // down
	}
	uiItemO(row, "", ICON_X, "LOGIC_OT_actuator_remove");
}

static void draw_actuator_action(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;
	uiLayout *row, *sub;

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "play_mode", 0, "", ICON_NONE);

	sub= uiLayoutRow(row, 1);
	uiItemR(sub, ptr, "use_force", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
	uiItemR(sub, ptr, "use_additive", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

	row = uiLayoutColumn(sub, 0);
	uiLayoutSetActive(row, (RNA_boolean_get(ptr, "use_additive") || RNA_boolean_get(ptr, "use_force")));
	uiItemR(row, ptr, "use_local", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "action", 0, "", ICON_NONE);
	uiItemR(row, ptr, "use_continue_last_frame", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	if ((RNA_enum_get(ptr, "play_mode") == ACT_ACTION_FROM_PROP))
		uiItemPointerR(row, ptr, "property", &settings_ptr, "properties", NULL, ICON_NONE);

	else {
		uiItemR(row, ptr, "frame_start", 0, NULL, ICON_NONE);
		uiItemR(row, ptr, "frame_end", 0, NULL, ICON_NONE);
	}

	uiItemR(row, ptr, "apply_to_children", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "frame_blend_in", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "priority", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "layer", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "layer_weight", 0, NULL, ICON_NONE);

	uiItemPointerR(layout, ptr, "frame_property", &settings_ptr, "properties", NULL, ICON_NONE);

#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
	uiItemR(layout, "stride_length", 0, NULL, ICON_NONE);
#endif
}

static void draw_actuator_armature(uiLayout *layout, PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bArmatureActuator *aa = (bArmatureActuator *) act->data;
	Object *ob = (Object *)ptr->id.data;
	bConstraint *constraint = NULL;
	PointerRNA pose_ptr, pchan_ptr;
	PropertyRNA *bones_prop = NULL;

	if (ob->type != OB_ARMATURE) {
		uiItemL(layout, "Actuator only available for armatures", ICON_NONE);
		return;
	}
	
	if (ob->pose) {
		RNA_pointer_create((ID *)ob, &RNA_Pose, ob->pose, &pose_ptr);
		bones_prop = RNA_struct_find_property(&pose_ptr, "bones");
	}
	
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	
	switch (RNA_enum_get(ptr, "mode"))
	{
		case ACT_ARM_RUN:
			break;
		case ACT_ARM_ENABLE:
		case ACT_ARM_DISABLE:
			if (ob->pose) {
				uiItemPointerR(layout, ptr, "bone", &pose_ptr, "bones", NULL, ICON_BONE_DATA);

				if (RNA_property_collection_lookup_string(&pose_ptr, bones_prop, aa->posechannel, &pchan_ptr))
					uiItemPointerR(layout, ptr, "constraint", &pchan_ptr, "constraints", NULL, ICON_CONSTRAINT_BONE);
			}
			break;
		case ACT_ARM_SETTARGET:
			if (ob->pose) {
				uiItemPointerR(layout, ptr, "bone", &pose_ptr, "bones", NULL, ICON_BONE_DATA);
				
				if (RNA_property_collection_lookup_string(&pose_ptr, bones_prop, aa->posechannel, &pchan_ptr))
					uiItemPointerR(layout, ptr, "constraint", &pchan_ptr, "constraints", NULL, ICON_CONSTRAINT_BONE);
			}

			uiItemR(layout, ptr, "target", 0, NULL, ICON_NONE);

			/* show second target only if the constraint supports it */
			get_armature_bone_constraint(ob, aa->posechannel, aa->constraint, &constraint);
			if (constraint && constraint->type == CONSTRAINT_TYPE_KINEMATIC) {
				uiItemR(layout, ptr, "secondary_target", 0, NULL, ICON_NONE);
			}
			break;
		case ACT_ARM_SETWEIGHT:
			if (ob->pose) {
				uiItemPointerR(layout, ptr, "bone", &pose_ptr, "bones", NULL, ICON_BONE_DATA);
				
				if (RNA_property_collection_lookup_string(&pose_ptr, bones_prop, aa->posechannel, &pchan_ptr))
					uiItemPointerR(layout, ptr, "constraint", &pchan_ptr, "constraints", NULL, ICON_CONSTRAINT_BONE);
			}

			uiItemR(layout, ptr, "weight", 0, NULL, ICON_NONE);
			break;
		case ACT_ARM_SETINFLUENCE:
			if (ob->pose) {
				uiItemPointerR(layout, ptr, "bone", &pose_ptr, "bones", NULL, ICON_BONE_DATA);
				
				if (RNA_property_collection_lookup_string(&pose_ptr, bones_prop, aa->posechannel, &pchan_ptr))
					uiItemPointerR(layout, ptr, "constraint", &pchan_ptr, "constraints", NULL, ICON_CONSTRAINT_BONE);
			}

			uiItemR(layout, ptr, "influence", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_camera(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;
	uiItemR(layout, ptr, "object", 0, NULL, ICON_NONE);

	row = uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "height", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "axis", 0, NULL, ICON_NONE);

	row = uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "min", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "max", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "damping", 0, NULL, ICON_NONE);
}

static void draw_actuator_constraint(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *row, *col, *sub, *split;
	PointerRNA main_ptr;

	RNA_main_pointer_create(CTX_data_main(C), &main_ptr);

	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	switch (RNA_enum_get(ptr, "mode"))
	{
		case ACT_CONST_TYPE_LOC:
			uiItemR(layout, ptr, "limit", 0, NULL, ICON_NONE);

			row = uiLayoutRow(layout, 1);
			uiItemR(row, ptr, "limit_min", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "limit_max", 0, NULL, ICON_NONE);

			uiItemR(layout, ptr, "damping", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			break;

		case ACT_CONST_TYPE_DIST:
			split = uiLayoutSplit(layout, 0.8, 0);
			uiItemR(split, ptr, "direction", 0, NULL, ICON_NONE);
			row = uiLayoutRow(split, 1);
			uiItemR(row, ptr, "use_local", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			uiItemR(row, ptr, "use_normal", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			row = uiLayoutRow(layout, 0);
			col = uiLayoutColumn(row, 1);
			uiItemL(col, "Range:", ICON_NONE);
			uiItemR(col, ptr, "range", 0, "", ICON_NONE);

			col = uiLayoutColumn(row, 1);
			uiItemR(col, ptr, "use_force_distance", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			sub = uiLayoutColumn(col, 0);
			uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_force_distance")==1);
			uiItemR(sub, ptr, "distance", 0, "", ICON_NONE);

			uiItemR(layout, ptr, "damping", UI_ITEM_R_SLIDER , NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.15, 0);
			uiItemR(split, ptr, "use_material_detect", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			if (RNA_boolean_get(ptr, "use_material_detect"))
				uiItemPointerR(split, ptr, "material", &main_ptr, "materials", NULL, ICON_MATERIAL_DATA);
			else
				uiItemR(split, ptr, "property", 0, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.15, 0);
			uiItemR(split, ptr, "use_persistent", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			row = uiLayoutRow(split, 1);
			uiItemR(row, ptr, "time", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "damping_rotation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			break;

		case ACT_CONST_TYPE_ORI:
			uiItemR(layout, ptr, "direction_axis_pos", 0, NULL, ICON_NONE);

			row=uiLayoutRow(layout, 1);
			uiItemR(row, ptr, "damping", UI_ITEM_R_SLIDER , NULL, ICON_NONE);
			uiItemR(row, ptr, "time", 0, NULL, ICON_NONE);

			row=uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "rotation_max", 0, NULL, ICON_NONE);

			row=uiLayoutRow(layout, 1);
			uiItemR(row, ptr, "angle_min", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "angle_max", 0, NULL, ICON_NONE);
			break;

		case ACT_CONST_TYPE_FH:
			split=uiLayoutSplit(layout, 0.75, 0);
			row= uiLayoutRow(split, 0);
			uiItemR(row, ptr, "fh_damping", UI_ITEM_R_SLIDER , NULL, ICON_NONE);

			uiItemR(row, ptr, "fh_height", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_fh_paralel_axis", UI_ITEM_R_TOGGLE , NULL, ICON_NONE);

			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "direction_axis", 0, NULL, ICON_NONE);
			split = uiLayoutSplit(row, 0.9, 0);
			uiItemR(split, ptr, "fh_force", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_fh_normal", UI_ITEM_R_TOGGLE , NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.15, 0);
			uiItemR(split, ptr, "use_material_detect", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			if (RNA_boolean_get(ptr, "use_material_detect"))
				uiItemPointerR(split, ptr, "material", &main_ptr, "materials", NULL, ICON_MATERIAL_DATA);
			else
				uiItemR(split, ptr, "property", 0, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.15, 0);
			uiItemR(split, ptr, "use_persistent", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "time", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "damping_rotation", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_edit_object(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	uiLayout *row, *split, *sub;
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

	switch (RNA_enum_get(ptr, "mode"))
	{
		case ACT_EDOB_ADD_OBJECT:
			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "object", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "time", 0, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.9, 0);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "linear_velocity", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_local_linear_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.9, 0);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "angular_velocity", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_local_angular_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			break;
		case ACT_EDOB_END_OBJECT:
			break;
		case ACT_EDOB_REPLACE_MESH:
			if (ob->type != OB_MESH) {
				uiItemL(layout, "Mode only available for mesh objects", ICON_NONE);
				break;
			}
			split = uiLayoutSplit(layout, 0.6, 0);
			uiItemR(split, ptr, "mesh", 0, NULL, ICON_NONE);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "use_replace_display_mesh", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			uiItemR(row, ptr, "use_replace_physics_mesh", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			break;
		case ACT_EDOB_TRACK_TO:
			split = uiLayoutSplit(layout, 0.5, 0);
			uiItemR(split, ptr, "track_object", 0, NULL, ICON_NONE);
			sub = uiLayoutSplit(split, 0.7, 0);
			uiItemR(sub, ptr, "time", 0, NULL, ICON_NONE);
			uiItemR(sub, ptr, "use_3d_tracking", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			break;
		case ACT_EDOB_DYNAMICS:
			if (ob->type != OB_MESH) {
				uiItemL(layout, "Mode only available for mesh objects", ICON_NONE);
				break;
			}
			uiItemR(layout, ptr, "dynamic_operation", 0, NULL, ICON_NONE);
			if (RNA_enum_get(ptr, "dynamic_operation") == ACT_EDOB_SET_MASS)
				uiItemR(layout, ptr, "mass", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_filter_2d(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row, *split;

	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	switch (RNA_enum_get(ptr, "mode"))
	{
		case ACT_2DFILTER_CUSTOMFILTER:
			uiItemR(layout, ptr, "filter_pass", 0, NULL, ICON_NONE);
			uiItemR(layout, ptr, "glsl_shader", 0, NULL, ICON_NONE);
			break;
		case ACT_2DFILTER_MOTIONBLUR:
			split=uiLayoutSplit(layout, 0.75, 1);
			row= uiLayoutRow(split, 0);
			uiLayoutSetActive(row, RNA_boolean_get(ptr, "use_motion_blur")==1);
			uiItemR(row, ptr, "motion_blur_factor", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_motion_blur", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			break;
		default: // all other 2D Filters
			uiItemR(layout, ptr, "filter_pass", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_game(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	if (RNA_enum_get(ptr, "mode") == ACT_GAME_LOAD)
		uiItemR(layout, ptr, "filename", 0, NULL, ICON_NONE);
}

static void draw_actuator_message(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	Object *ob;
	PointerRNA main_ptr, settings_ptr;
	uiLayout *row;

	RNA_main_pointer_create(CTX_data_main(C), &main_ptr);

	ob = (Object *)ptr->id.data;
	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	uiItemPointerR(layout, ptr, "to_property", &main_ptr, "objects", NULL, ICON_OBJECT_DATA);
	uiItemR(layout, ptr, "subject", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 1);
	uiItemR(row, ptr, "body_type", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "body_type") == ACT_MESG_MESG)
		uiItemR(row, ptr, "body_message", 0, "", ICON_NONE);
	else // mode == ACT_MESG_PROP
		uiItemPointerR(row, ptr, "body_property", &settings_ptr, "properties", "", ICON_NONE);
}

static void draw_actuator_motion(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	PointerRNA settings_ptr;
	uiLayout *split, *row, *col, *sub;
	int physics_type;

	ob = (Object *)ptr->id.data;	
	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);
	physics_type = RNA_enum_get(&settings_ptr, "physics_type");
	
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	
	switch (RNA_enum_get(ptr, "mode")) {
		case ACT_OBJECT_NORMAL:
			split = uiLayoutSplit(layout, 0.9, 0);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "offset_location", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_local_location", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.9, 0);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "offset_rotation", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_local_rotation", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			
			if (ELEM3(physics_type, OB_BODY_TYPE_DYNAMIC, OB_BODY_TYPE_RIGID, OB_BODY_TYPE_SOFT)) {			
				uiItemL(layout, "Dynamic Object Settings:", ICON_NONE);
				split = uiLayoutSplit(layout, 0.9, 0);
				row = uiLayoutRow(split, 0);
				uiItemR(row, ptr, "force", 0, NULL, ICON_NONE);
				uiItemR(split, ptr, "use_local_force", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

				split = uiLayoutSplit(layout, 0.9, 0);
				row = uiLayoutRow(split, 0);
				uiItemR(row, ptr, "torque", 0, NULL, ICON_NONE);
				uiItemR(split, ptr, "use_local_torque", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

				split = uiLayoutSplit(layout, 0.9, 0);
				row = uiLayoutRow(split, 0);
				uiItemR(row, ptr, "linear_velocity", 0, NULL, ICON_NONE);
				row = uiLayoutRow(split, 1);
				uiItemR(row, ptr, "use_local_linear_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
				uiItemR(row, ptr, "use_add_linear_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

				split = uiLayoutSplit(layout, 0.9, 0);
				row = uiLayoutRow(split, 0);
				uiItemR(row, ptr, "angular_velocity", 0, NULL, ICON_NONE);
				uiItemR(split, ptr, "use_local_angular_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

				uiItemR(layout, ptr, "damping", 0, NULL, ICON_NONE);
			}
			break;
		case ACT_OBJECT_SERVO:
			uiItemR(layout, ptr, "reference_object", 0, NULL, ICON_NONE);

			split = uiLayoutSplit(layout, 0.9, 0);
			row = uiLayoutRow(split, 0);
			uiItemR(row, ptr, "linear_velocity", 0, NULL, ICON_NONE);
			uiItemR(split, ptr, "use_local_linear_velocity", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

			row = uiLayoutRow(layout, 0);
			col = uiLayoutColumn(row, 0);
			uiItemR(col, ptr, "use_servo_limit_x", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			sub = uiLayoutColumn(col, 1);
			uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_servo_limit_x")==1);
			uiItemR(sub, ptr, "force_max_x", 0, NULL, ICON_NONE);
			uiItemR(sub, ptr, "force_min_x", 0, NULL, ICON_NONE);

			col = uiLayoutColumn(row, 0);
			uiItemR(col, ptr, "use_servo_limit_y", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			sub = uiLayoutColumn(col, 1);
			uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_servo_limit_y")==1);
			uiItemR(sub, ptr, "force_max_y", 0, NULL, ICON_NONE);
			uiItemR(sub, ptr, "force_min_y", 0, NULL, ICON_NONE);

			col = uiLayoutColumn(row, 0);
			uiItemR(col, ptr, "use_servo_limit_z", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			sub = uiLayoutColumn(col, 1);
			uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_servo_limit_z")==1);
			uiItemR(sub, ptr, "force_max_z", 0, NULL, ICON_NONE);
			uiItemR(sub, ptr, "force_min_z", 0, NULL, ICON_NONE);

			//XXXACTUATOR missing labels from original 2.49 ui (e.g. Servo, Min, Max, Fast)
			//Layout designers willing to help on that, please compare with 2.49 ui
			// (since the old code is going to be deleted ... soon)

			col = uiLayoutColumn(layout, 1);
			uiItemR(col, ptr, "proportional_coefficient", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			uiItemR(col, ptr, "integral_coefficient", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			uiItemR(col, ptr, "derivate_coefficient", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_parent(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row, *sub;

	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

	if (RNA_enum_get(ptr, "mode") == ACT_PARENT_SET) {
		uiItemR(layout, ptr, "object", 0, NULL, ICON_NONE);

		row = uiLayoutRow(layout, 0);
		uiItemR(row, ptr, "use_compound", 0, NULL, ICON_NONE);
		sub= uiLayoutRow(row, 0);
		uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_compound")==1);
		uiItemR(sub, ptr, "use_ghost", 0, NULL, ICON_NONE);
	}
}

static void draw_actuator_property(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	bActuator *act = (bActuator *)ptr->data;
	bPropertyActuator *pa = (bPropertyActuator *) act->data;
	Object *ob_from= pa->ob;
	PointerRNA settings_ptr, obj_settings_ptr;

	uiLayout *row, *sub;

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);
	uiItemPointerR(layout, ptr, "property", &settings_ptr, "properties", NULL, ICON_NONE);

	switch(RNA_enum_get(ptr, "mode"))
	{
		case ACT_PROP_TOGGLE:
			break;
		case ACT_PROP_ADD:
			uiItemR(layout, ptr, "value", 0, NULL, ICON_NONE);
			break;
		case ACT_PROP_ASSIGN:
			uiItemR(layout, ptr, "value", 0, NULL, ICON_NONE);
			break;
		case ACT_PROP_COPY:
			row = uiLayoutRow(layout, 0);
			uiItemR(row, ptr, "object", 0, NULL, ICON_NONE);
			if (ob_from) {
				RNA_pointer_create((ID *)ob_from, &RNA_GameObjectSettings, ob_from, &obj_settings_ptr);
				uiItemPointerR(row, ptr, "object_property", &obj_settings_ptr, "properties", NULL, ICON_NONE);
			}
			else {
				sub= uiLayoutRow(row, 0);
				uiLayoutSetActive(sub, 0);
				uiItemR(sub, ptr, "object_property", 0, NULL, ICON_NONE);
			}
			break;
	}
}

static void draw_actuator_random(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob;
	PointerRNA settings_ptr;
	uiLayout *row;

	ob = (Object *)ptr->id.data;
	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	row = uiLayoutRow(layout, 0);

	uiItemR(row, ptr, "seed", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "distribution", 0, NULL, ICON_NONE);

	row = uiLayoutRow(layout, 0);
	uiItemPointerR(row, ptr, "property", &settings_ptr, "properties", NULL, ICON_NONE);

	row = uiLayoutRow(layout, 0);

	switch (RNA_enum_get(ptr, "distribution")) {
		case ACT_RANDOM_BOOL_CONST:
			uiItemR(row, ptr, "use_always_true", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_BOOL_UNIFORM:
			uiItemL(row, "Choose between true and false, 50% chance each", ICON_NONE);
			break;

		case ACT_RANDOM_BOOL_BERNOUILLI:
			uiItemR(row, ptr, "chance", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_INT_CONST:
			uiItemR(row, ptr, "int_value", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_INT_UNIFORM:
			uiItemR(row, ptr, "int_min", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "int_max", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_INT_POISSON:
			uiItemR(row, ptr, "int_mean", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_FLOAT_CONST:
			uiItemR(row, ptr, "float_value", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_FLOAT_UNIFORM:
			uiItemR(row, ptr, "float_min", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "float_max", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_FLOAT_NORMAL:
			uiItemR(row, ptr, "float_mean", 0, NULL, ICON_NONE);
			uiItemR(row, ptr, "standard_derivation", 0, NULL, ICON_NONE);
			break;

		case ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL:
			uiItemR(row, ptr, "half_life_time", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_scene(uiLayout *layout, PointerRNA *ptr)
{
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

	switch (RNA_enum_get(ptr, "mode")) {
		case ACT_SCENE_CAMERA:
			uiItemR(layout, ptr, "camera", 0, NULL, ICON_NONE);
			break;
		case ACT_SCENE_RESTART:
			break;
		default: // ACT_SCENE_SET|ACT_SCENE_ADD_FRONT|ACT_SCENE_ADD_BACK|ACT_SCENE_REMOVE|ACT_SCENE_SUSPEND|ACT_SCENE_RESUME
			uiItemR(layout, ptr, "scene", 0, NULL, ICON_NONE);
			break;
	}
}

static void draw_actuator_shape_action(uiLayout *layout, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;
	uiLayout *row;

	if (ob->type != OB_MESH) {
		uiItemL(layout, "Actuator only available for mesh objects", ICON_NONE);
		return;
	}

	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "mode", 0, "", ICON_NONE);
	uiItemR(row, ptr, "action", 0, "", ICON_NONE);
	uiItemR(row, ptr, "use_continue_last_frame", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	if ((RNA_enum_get(ptr, "mode") == ACT_ACTION_FROM_PROP))
		uiItemPointerR(row, ptr, "property", &settings_ptr, "properties", NULL, ICON_NONE);

	else {
		uiItemR(row, ptr, "frame_start", 0, NULL, ICON_NONE);
		uiItemR(row, ptr, "frame_end", 0, NULL, ICON_NONE);
	}

	row= uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "frame_blend_in", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "priority", 0, NULL, ICON_NONE);

	row= uiLayoutRow(layout, 0);
	uiItemPointerR(row, ptr, "frame_property", &settings_ptr, "properties", NULL, ICON_NONE);

#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
	uiItemR(row, "stride_length", 0, NULL, ICON_NONE);
#endif
}

static void draw_actuator_sound(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *row, *col;

	uiTemplateID(layout, C, ptr, "sound", NULL, "SOUND_OT_open", NULL);
	if (!RNA_pointer_get(ptr, "sound").data) {
		uiItemL(layout, "Select a sound from the list or load a new one", ICON_NONE);
		return;
	}
	uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

	row = uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "volume", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "pitch", 0, NULL, ICON_NONE);

	uiItemR(layout, ptr, "use_sound_3d", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, 0);
	uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_sound_3d")==1);

	row = uiLayoutRow(col, 0);
	uiItemR(row, ptr, "gain_3d_min", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "gain_3d_max", 0, NULL, ICON_NONE);

	row = uiLayoutRow(col, 0);
	uiItemR(row, ptr, "distance_3d_reference", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "distance_3d_max", 0, NULL, ICON_NONE);

	row = uiLayoutRow(col, 0);
	uiItemR(row, ptr, "rolloff_factor_3d", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "cone_outer_gain_3d", 0, NULL, ICON_NONE);

	row = uiLayoutRow(col, 0);
	uiItemR(row, ptr, "cone_outer_angle_3d", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "cone_inner_angle_3d", 0, NULL, ICON_NONE);
}

static void draw_actuator_state(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *split;
	Object *ob = (Object *)ptr->id.data;
	PointerRNA settings_ptr;
	RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

	split = uiLayoutSplit(layout, 0.35, 0);
	uiItemR(split, ptr, "operation", 0, NULL, ICON_NONE);

	uiTemplateLayers(split, ptr, "states", &settings_ptr, "used_states", 0);
}

static void draw_actuator_visibility(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;
	row = uiLayoutRow(layout, 0);

	uiItemR(row, ptr, "use_visible", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "use_occlusion", 0, NULL, ICON_NONE);
	uiItemR(row, ptr, "apply_to_children", 0, NULL, ICON_NONE);
}

static void draw_actuator_steering(uiLayout *layout, PointerRNA *ptr)
{
	uiLayout *row;
	uiLayout *col;

	uiItemR(layout, ptr, "mode", 0, NULL, 0);
	uiItemR(layout, ptr, "target", 0, NULL, 0);
	uiItemR(layout, ptr, "navmesh", 0, NULL, 0);	

	row = uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "distance", 0, NULL, 0);
	uiItemR(row, ptr, "velocity", 0, NULL, 0);
	row = uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "acceleration", 0, NULL, 0);
	uiItemR(row, ptr, "turn_speed", 0, NULL, 0);

	row = uiLayoutRow(layout, 0);
	col = uiLayoutColumn(row, 0);
	uiItemR(col, ptr, "facing", 0, NULL, 0);
	col = uiLayoutColumn(row, 0);
	uiItemR(col, ptr, "facing_axis", 0, NULL, 0);
	if (!RNA_boolean_get(ptr, "facing")) {
		uiLayoutSetActive(col, 0);
	}
	col = uiLayoutColumn(row, 0);
	uiItemR(col, ptr, "normal_up", 0, NULL, 0);
	if (!RNA_pointer_get(ptr, "navmesh").data) {
		uiLayoutSetActive(col, 0);
	}

	row = uiLayoutRow(layout, 0);
	uiItemR(row, ptr, "self_terminated", 0, NULL, 0);
	if (RNA_enum_get(ptr, "mode")==ACT_STEERING_PATHFOLLOWING) {
		uiItemR(row, ptr, "update_period", 0, NULL, 0);	
		row = uiLayoutRow(layout, 0);
	}
	uiItemR(row, ptr, "show_visualization", 0, NULL, 0);	
}

static void draw_brick_actuator(uiLayout *layout, PointerRNA *ptr, bContext *C)
{
	uiLayout *box;
	
	if (!RNA_boolean_get(ptr, "show_expanded"))
		return;
	
	box = uiLayoutBox(layout);
	
	switch (RNA_enum_get(ptr, "type")) {
		case ACT_ACTION:
			draw_actuator_action(box, ptr);
			break;
		case ACT_ARMATURE:
			draw_actuator_armature(box, ptr);
			break;
		case ACT_CAMERA:
			draw_actuator_camera(box, ptr);
			break;
		case ACT_CONSTRAINT:
			draw_actuator_constraint(box, ptr, C);
			break;
		case ACT_EDIT_OBJECT:
			draw_actuator_edit_object(box, ptr);
			break;
		case ACT_2DFILTER:
			draw_actuator_filter_2d(box, ptr);
			break;
		case ACT_GAME:
			draw_actuator_game(box, ptr);
			break;
		case ACT_MESSAGE:
			draw_actuator_message(box, ptr, C);
			break;
		case ACT_OBJECT:
			draw_actuator_motion(box, ptr);
			break;
		case ACT_PARENT:
			draw_actuator_parent(box, ptr);
			break;
		case ACT_PROPERTY:
			draw_actuator_property(box, ptr);
			break;
		case ACT_RANDOM:
			draw_actuator_random(box, ptr);
			break;
		case ACT_SCENE:
			draw_actuator_scene(box, ptr);
			break;
		case ACT_SHAPEACTION:
			draw_actuator_shape_action(box, ptr);
			break;
		case ACT_SOUND:
			draw_actuator_sound(box, ptr, C);
			break;
		case ACT_STATE:
			draw_actuator_state(box, ptr);
			break;
		case ACT_VISIBILITY:
			draw_actuator_visibility(box, ptr);
			break;
		case ACT_STEERING:
			draw_actuator_steering(box, ptr);
	}
}

static void logic_buttons_new(bContext *C, ARegion *ar)
{
	SpaceLogic *slogic= CTX_wm_space_logic(C);
	Object *ob= CTX_data_active_object(C);
	Object *act_ob= ob;
	ID **idar;
	
	PointerRNA logic_ptr, settings_ptr;
	
	uiLayout *layout, *row, *box;
	uiBlock *block;
	uiBut *but;
	char uiblockstr[32];
	short a, count;
	int xco, yco, width;
	
	if (ob==NULL) return;
	
	RNA_pointer_create(NULL, &RNA_SpaceLogicEditor, slogic, &logic_ptr);
	idar= get_selected_and_linked_obs(C, &count, slogic->scaflag);
	
	BLI_snprintf(uiblockstr, sizeof(uiblockstr), "buttonswin %p", (void *)ar);
	block= uiBeginBlock(C, ar, uiblockstr, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_logic_buts, NULL);
	
	/* loop over all objects and set visible/linked flags for the logic bricks */
	for (a=0; a<count; a++) {
		bActuator *act;
		bSensor *sens;
		bController *cont;
		int iact;
		short flag;

		ob= (Object *)idar[a];
		
		/* clean ACT_LINKED and ACT_VISIBLE of all potentially visible actuators so that we can determine which is actually linked/visible */
		act = ob->actuators.first;
		while (act) {
			act->flag &= ~(ACT_LINKED|ACT_VISIBLE);
			act = act->next;
		}
		/* same for sensors */
		sens= ob->sensors.first;
		while (sens) {
			sens->flag &= ~(SENS_VISIBLE);
			sens = sens->next;
		}

		/* mark the linked and visible actuators */
		cont= ob->controllers.first;
		while (cont) {
			flag = ACT_LINKED;

			/* this controller is visible, mark all its actuator */
			if ((ob->scaflag & OB_ALLSTATE) || (ob->state & cont->state_mask))
				flag |= ACT_VISIBLE;

			for (iact=0; iact<cont->totlinks; iact++) {
				act = cont->links[iact];
				if (act)
					act->flag |= flag;
			}
			cont = cont->next;
		}
	}
	
	/* ****************** Controllers ****************** */
	
	xco= 420; yco= 170; width= 300;
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, xco, yco, width, 20, UI_GetStyle());
	row = uiLayoutRow(layout, 1);
	
	uiDefBlockBut(block, controller_menu, NULL, "Controllers", xco-10, yco, 300, UI_UNIT_Y, "");		/* replace this with uiLayout stuff later */
	
	uiItemR(row, &logic_ptr, "show_controllers_selected_objects", 0, "Sel", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_controllers_active_object", 0, "Act", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_controllers_linked_controller", 0, "Link", ICON_NONE);

	for (a=0; a<count; a++) {
		bController *cont;
		PointerRNA ptr;
		uiLayout *split, *subsplit, *col;

		
		ob= (Object *)idar[a];

		/* only draw the controller common header if "use_visible" */
		if ( (ob->scavisflag & OB_VIS_CONT) == 0) continue;
	
		/* Drawing the Controller Header common to all Selected Objects */

		RNA_pointer_create((ID *)ob, &RNA_GameObjectSettings, ob, &settings_ptr);

		split= uiLayoutSplit(layout, 0.05, 0);
		uiItemR(split, &settings_ptr, "show_state_panel", UI_ITEM_R_NO_BG, "", ICON_DISCLOSURE_TRI_RIGHT);

		row = uiLayoutRow(split, 1);
		uiDefButBitS(block, TOG, OB_SHOWCONT, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide controllers");
		if (ob == act_ob)
			uiItemMenuEnumO(row, "LOGIC_OT_controller_add", "type", "Add Controller", ICON_NONE);

		if (RNA_boolean_get(&settings_ptr, "show_state_panel")) {

			box= uiLayoutBox(layout);
			split= uiLayoutSplit(box, 0.2, 0);

			col= uiLayoutColumn(split, 0);
			uiItemL(col, "Visible", ICON_NONE);
			uiItemL(col, "Initial", ICON_NONE);

			subsplit= uiLayoutSplit(split, 0.85, 0);
			col= uiLayoutColumn(subsplit, 0);
			row= uiLayoutRow(col, 0);
			uiLayoutSetActive(row, RNA_boolean_get(&settings_ptr, "use_all_states")==0);
			uiTemplateLayers(row, &settings_ptr, "states_visible", &settings_ptr, "used_states", 0);
			row= uiLayoutRow(col, 0);
			uiTemplateLayers(row, &settings_ptr, "states_initial", &settings_ptr, "used_states", 0);

			col= uiLayoutColumn(subsplit, 0);
			uiItemR(col, &settings_ptr, "use_all_states", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);
			uiItemR(col, &settings_ptr, "show_debug_state", 0, "", ICON_NONE);
		}

		/* End of Drawing the Controller Header common to all Selected Objects */

		if ((ob->scaflag & OB_SHOWCONT) == 0) continue;
		

		uiItemS(layout);
		
		for (cont= ob->controllers.first; cont; cont=cont->next) {
			RNA_pointer_create((ID *)ob, &RNA_Controller, cont, &ptr);
			
			if (!(ob->scaflag & OB_ALLSTATE) && !(ob->state & cont->state_mask))
				continue;
			
			/* use two nested splits to align inlinks/links properly */
			split = uiLayoutSplit(layout, 0.05, 0);
			
			/* put inlink button to the left */
			col = uiLayoutColumn(split, 0);
			uiLayoutSetAlignment(col, UI_LAYOUT_ALIGN_LEFT);
			uiDefIconBut(block, INLINK, 0, ICON_INLINK, 0, 0, UI_UNIT_X, UI_UNIT_Y, cont, LINK_CONTROLLER, 0, 0, 0, "");
			
			//col = uiLayoutColumn(split, 1);
			/* nested split for middle and right columns */
			subsplit = uiLayoutSplit(split, 0.95, 0);
			
			col = uiLayoutColumn(subsplit, 1);
			uiLayoutSetContextPointer(col, "controller", &ptr);
			
			/* should make UI template for controller header.. function will do for now */
//			draw_controller_header(col, &ptr);
			draw_controller_header(col, &ptr, xco, width, yco); //provisory for 2.50 beta

			/* draw the brick contents */
			draw_brick_controller(col, &ptr);
			
			
			/* put link button to the right */
			col = uiLayoutColumn(subsplit, 0);
			uiLayoutSetAlignment(col, UI_LAYOUT_ALIGN_LEFT);
			but = uiDefIconBut(block, LINK, 0, ICON_LINK, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
			uiSetButLink(but, NULL, (void ***)&(cont->links), &cont->totlinks, LINK_CONTROLLER, LINK_ACTUATOR);
		}
	}
	uiBlockLayoutResolve(block, NULL, &yco);	/* stores final height in yco */
	
	
	/* ****************** Sensors ****************** */
	
	xco= 10; yco= 170; width= 340;
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, xco, yco, width, 20, UI_GetStyle());
	row = uiLayoutRow(layout, 1);
	
	uiDefBlockBut(block, sensor_menu, NULL, "Sensors", xco-10, yco, 300, UI_UNIT_Y, "");		/* replace this with uiLayout stuff later */
	
	uiItemR(row, &logic_ptr, "show_sensors_selected_objects", 0, "Sel", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_sensors_active_object", 0, "Act", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_sensors_linked_controller", 0, "Link", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_sensors_active_states", 0, "State", ICON_NONE);
	
	for (a=0; a<count; a++) {
		bSensor *sens;
		PointerRNA ptr;
		
		ob= (Object *)idar[a];

		/* only draw the sensor common header if "use_visible" */
		if ((ob->scavisflag & OB_VIS_SENS) == 0) continue;

		row = uiLayoutRow(layout, 1);
		uiDefButBitS(block, TOG, OB_SHOWSENS, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide sensors");
		if (ob == act_ob)
			uiItemMenuEnumO(row, "LOGIC_OT_sensor_add", "type", "Add Sensor", ICON_NONE);
		
		if ((ob->scaflag & OB_SHOWSENS) == 0) continue;
		
		uiItemS(layout);
		
		for (sens= ob->sensors.first; sens; sens=sens->next) {
			RNA_pointer_create((ID *)ob, &RNA_Sensor, sens, &ptr);
			
			if ((ob->scaflag & OB_ALLSTATE) ||
				!(slogic->scaflag & BUTS_SENS_STATE) ||
				(sens->totlinks == 0) ||											/* always display sensor without links so that is can be edited */
				(sens->flag & SENS_PIN && slogic->scaflag & BUTS_SENS_STATE) ||	/* states can hide some sensors, pinned sensors ignore the visible state */
				(is_sensor_linked(block, sens))
				)
			{	// gotta check if the current state is visible or not
				uiLayout *split, *col;
				
				/* make as visible, for move operator */
				sens->flag |= SENS_VISIBLE;

				split = uiLayoutSplit(layout, 0.95, 0);
				col = uiLayoutColumn(split, 1);
				uiLayoutSetContextPointer(col, "sensor", &ptr);
				
				/* should make UI template for sensor header.. function will do for now */
				draw_sensor_header(col, &ptr, &logic_ptr);
				
				/* draw the brick contents */
				draw_brick_sensor(col, &ptr, C);
				
				/* put link button to the right */
				col = uiLayoutColumn(split, 0);
				/* use old-school uiButtons for links for now */
				but = uiDefIconBut(block, LINK, 0, ICON_LINK, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
				uiSetButLink(but, NULL, (void ***)&(sens->links), &sens->totlinks, LINK_SENSOR, LINK_CONTROLLER);
			}
		}
	}
	uiBlockLayoutResolve(block, NULL, &yco);	/* stores final height in yco */
	
	/* ****************** Actuators ****************** */
	
	xco= 800; yco= 170; width= 340;
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, xco, yco, width, 20, UI_GetStyle());
	row = uiLayoutRow(layout, 1);
	
	uiDefBlockBut(block, actuator_menu, NULL, "Actuators", xco-10, yco, 300, UI_UNIT_Y, "");		/* replace this with uiLayout stuff later */
	
	uiItemR(row, &logic_ptr, "show_actuators_selected_objects", 0, "Sel", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_actuators_active_object", 0, "Act", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_actuators_linked_controller", 0, "Link", ICON_NONE);
	uiItemR(row, &logic_ptr, "show_actuators_active_states", 0, "State", ICON_NONE);
	
	for (a=0; a<count; a++) {
		bActuator *act;
		PointerRNA ptr;
		
		ob= (Object *)idar[a];

		/* only draw the actuator common header if "use_visible" */
		if ( (ob->scavisflag & OB_VIS_ACT) == 0) continue;

		row = uiLayoutRow(layout, 1);
		uiDefButBitS(block, TOG, OB_SHOWACT, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide actuators");
		if (ob == act_ob)
			uiItemMenuEnumO(row, "LOGIC_OT_actuator_add", "type", "Add Actuator", ICON_NONE);

		if ((ob->scaflag & OB_SHOWACT) == 0) continue;
		
		uiItemS(layout);
		
		for (act= ob->actuators.first; act; act=act->next) {
			
			RNA_pointer_create((ID *)ob, &RNA_Actuator, act, &ptr);
			
			if ((ob->scaflag & OB_ALLSTATE) ||
				!(slogic->scaflag & BUTS_ACT_STATE) ||
				!(act->flag & ACT_LINKED) ||		/* always display actuators without links so that is can be edited */
				(act->flag & ACT_VISIBLE) ||		/* this actuator has visible connection, display it */
				(act->flag & ACT_PIN && slogic->scaflag & BUTS_ACT_STATE)	/* states can hide some sensors, pinned sensors ignore the visible state */
				)
			{	// gotta check if the current state is visible or not
				uiLayout *split, *col;
				
				/* make as visible, for move operator */
				act->flag |= ACT_VISIBLE;

				split = uiLayoutSplit(layout, 0.05, 0);
				
				/* put inlink button to the left */
				col = uiLayoutColumn(split, 0);
				uiDefIconBut(block, INLINK, 0, ICON_INLINK, 0, 0, UI_UNIT_X, UI_UNIT_Y, act, LINK_ACTUATOR, 0, 0, 0, "");

				col = uiLayoutColumn(split, 1);
				uiLayoutSetContextPointer(col, "actuator", &ptr);
				
				/* should make UI template for actuator header.. function will do for now */
				draw_actuator_header(col, &ptr, &logic_ptr);
				
				/* draw the brick contents */
				draw_brick_actuator(col, &ptr, C);
				
			}
		}
	}
	uiBlockLayoutResolve(block, NULL, &yco);	/* stores final height in yco */
	
	
	uiComposeLinks(block);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
	
	if (idar) MEM_freeN(idar);
}

void logic_buttons(bContext *C, ARegion *ar)
{
	Main *bmain= CTX_data_main(C);
	SpaceLogic *slogic= CTX_wm_space_logic(C);
	Object *ob= CTX_data_active_object(C);
	ID **idar;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	uiBlock *block;
	uiBut *but;
	PointerRNA logic_ptr;
	int a, iact, stbit, offset;
	int xco, yco, width, ycoo;
	short count;
	char numstr[32];
	/* pin is a bool used for actuator and sensor drawing with states
	 * pin so changing states dosnt hide the logic brick */
	char pin;

	if (G.rt == 0) {
		logic_buttons_new(C, ar);
		return;
	}
	
	if (ob==NULL) return;
//	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	BLI_snprintf(numstr, sizeof(numstr), "buttonswin %p", (void *)ar);
	block= uiBeginBlock(C, ar, numstr, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_logic_buts, NULL);

	RNA_pointer_create(NULL, &RNA_SpaceLogicEditor, slogic, &logic_ptr);
	
	idar= get_selected_and_linked_obs(C, &count, slogic->scaflag);

	/* clean ACT_LINKED and ACT_VISIBLE of all potentially visible actuators so that 
	 * we can determine which is actually linked/visible */
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
		act= ob->actuators.first;
		while (act) {
			act->flag &= ~(ACT_LINKED|ACT_VISIBLE);
			act = act->next;
		}
		/* same for sensors */
		sens= ob->sensors.first;
		while (sens) {
			sens->flag &= ~(SENS_VISIBLE);
			sens = sens->next;
		}
	}
		
	/* start with the controller because we need to know which one is visible */
	/* ******************************* */
	xco= 400; yco= 170; width= 300;

	uiDefBlockBut(block, controller_menu, NULL, "Controllers", xco-10, yco+35, 100, UI_UNIT_Y, "");
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_CONT_SEL,  B_REDR, "Sel", xco+110, yco+35, (width-100)/3, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_CONT_ACT, B_REDR, "Act", xco+110+(width-100)/3, yco+35, (width-100)/3, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_CONT_LINK, B_REDR, "Link", xco+110+2*(width-100)/3, yco+35, (width-100)/3, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show linked Objects to Sensor/Actuator");
	uiBlockEndAlign(block);
	
	for (a=0; a<count; a++) {
		unsigned int controller_state_mask = 0; /* store a bitmask for states that are used */
		
		ob= (Object *)idar[a];
//		uiClearButLock();
//		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		if ( (ob->scavisflag & OB_VIS_CONT) == 0) continue;

		/* presume it is only objects for now */
		uiBlockBeginAlign(block);
//		if (ob->controllers.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWCONT, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Active Object name");
//		if (ob->controllers.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDCONT, B_ADD_CONT, "Add",(short)(xco+width-40), yco, 50, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Add a new Controller");
		uiBlockEndAlign(block);
		yco-=20;
		
		/* mark all actuators linked to these controllers */
		/* note that some of these actuators could be from objects that are not in the display list.
		 * It's ok because those actuators will not be displayed here */
		cont= ob->controllers.first;
		while (cont) {
			for (iact=0; iact<cont->totlinks; iact++) {
				act = cont->links[iact];
				if (act)
					act->flag |= ACT_LINKED;
			}
			controller_state_mask |= cont->state_mask;
			cont = cont->next;
		}

		if (ob->scaflag & OB_SHOWCONT) {

			/* first show the state */
			uiDefBlockBut(block, object_state_mask_menu, ob, "State", (short)(xco-10), (short)(yco-10), 36, UI_UNIT_Y, "Object state menu: store and retrieve initial state");

			if (!ob->state)
				ob->state = 1;
			for (offset=0; offset<15; offset+=5) {
				uiBlockBeginAlign(block);
				for (stbit=0; stbit<5; stbit++) {
					but = uiDefButBitI(block, controller_state_mask&(1<<(stbit+offset)) ? BUT_TOGDUAL:TOG, 1<<(stbit+offset), stbit+offset, "",	(short)(xco+31+12*stbit+13*offset), yco, 12, 12, (int *)&(ob->state), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+offset)));
					uiButSetFunc(but, check_state_mask, but, &(ob->state));
				}
				for (stbit=0; stbit<5; stbit++) {
					but = uiDefButBitI(block, controller_state_mask&(1<<(stbit+offset+15)) ? BUT_TOGDUAL:TOG, 1<<(stbit+offset+15), stbit+offset+15, "",	(short)(xco+31+12*stbit+13*offset), yco-12, 12, 12, (int *)&(ob->state), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+offset+15)));
					uiButSetFunc(but, check_state_mask, but, &(ob->state));
				}
			}
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, OB_ALLSTATE, B_SET_STATE_BIT, "All",(short)(xco+226), yco-10, 22, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Set all state bits");
			uiDefButBitS(block, TOG, OB_INITSTBIT, B_INIT_STATE_BIT, "Ini",(short)(xco+248), yco-10, 22, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Set the initial state");
			uiDefButBitS(block, TOG, OB_DEBUGSTATE, 0, "D",(short)(xco+270), yco-10, 15, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Print state debug info");
			uiBlockEndAlign(block);

			yco-=35;
		
			/* display only the controllers that match the current state */
			offset = 0;
			for (stbit=0; stbit<32; stbit++) {
				if (!(ob->state & (1<<stbit)))
					continue;
				/* add a separation between controllers of different states */
				if (offset) {
					offset = 0;
					yco -= 6;
				}
				cont= ob->controllers.first;
				while (cont) {
					if (cont->state_mask & (1<<stbit)) {
						/* this controller is visible, mark all its actuator */
						for (iact=0; iact<cont->totlinks; iact++) {
							act = cont->links[iact];
							if (act)
								act->flag |= ACT_VISIBLE;
						}
						uiDefIconButBitS(block, TOG, CONT_DEL, B_DEL_CONT, ICON_X,	xco, yco, 22, UI_UNIT_Y, &cont->flag, 0, 0, 0, 0, "Delete Controller");
						uiDefIconButBitS(block, ICONTOG, CONT_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, UI_UNIT_Y, &cont->flag, 0, 0, 0, 0, "Controller settings");
						uiDefIconButBitS(block, TOG, CONT_PRIO, B_REDR, ICON_BOOKMARKS, (short)(xco+width-66), yco, 22, UI_UNIT_Y, &cont->flag, 0, 0, 0, 0, "Mark controller for execution before all non-marked controllers (good for startup scripts)");

						sprintf(numstr, "%d", first_bit(cont->state_mask)+1);
						uiDefBlockBut(block, controller_state_mask_menu, cont, numstr, (short)(xco+width-44), yco, 22, UI_UNIT_Y, "Set controller state index (from 1 to 30)");
				
						if (cont->flag & CONT_SHOW) {
							cont->otype= cont->type;
							uiDefButS(block, MENU, B_CHANGE_CONT, controller_pup(),(short)(xco+22), yco, 70, UI_UNIT_Y, &cont->type, 0, 0, 0, 0, "Controller type");
							but = uiDefBut(block, TEX, 1, "", (short)(xco+92), yco, (short)(width-158), UI_UNIT_Y, cont->name, 0, MAX_NAME, 0, 0, "Controller name");
							uiButSetFunc(but, make_unique_prop_names_cb, cont->name, (void*) 0);
				
							ycoo= yco;
							yco= draw_controllerbuttons(cont, block, xco, yco, width);
							if (yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
						}
						else {
							cpack(0x999999);
							glRecti(xco+22, yco, xco+width-22,yco+19);
							but = uiDefBut(block, LABEL, 0, controller_name(cont->type), (short)(xco+22), yco, 70, UI_UNIT_Y, cont, 0, 0, 0, 0, "Controller type");
							//uiButSetFunc(but, old_sca_move_controller, cont, NULL);
							but = uiDefBut(block, LABEL, 0, cont->name,(short)(xco+92), yco,(short)(width-158), UI_UNIT_Y, cont, 0, 0, 0, 0, "Controller name");
							//uiButSetFunc(but, old_sca_move_controller, cont, NULL);

							uiBlockBeginAlign(block);
							but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_UP, (short)(xco+width-(110+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick up");
							uiButSetFunc(but, old_sca_move_controller, cont, (void *)TRUE);
							but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_DOWN, (short)(xco+width-(88+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick down");
							uiButSetFunc(but, old_sca_move_controller, cont, (void *)FALSE);
							uiBlockEndAlign(block);

							ycoo= yco;
						}
				
						but = uiDefIconBut(block, LINK, 0, ICON_LINK,	(short)(xco+width), ycoo, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
						uiSetButLink(but, NULL, (void ***)&(cont->links), &cont->totlinks, LINK_CONTROLLER, LINK_ACTUATOR);
				
						uiDefIconBut(block, INLINK, 0, ICON_INLINK,(short)(xco-19), ycoo, UI_UNIT_X, UI_UNIT_Y, cont, LINK_CONTROLLER, 0, 0, 0, "");
						/* offset is >0 if at least one controller was displayed */
						offset++;
						yco-=20;
					}
					cont= cont->next;
				}

			}
			yco-= 6;
		}
	}

	/* ******************************* */
	xco= 10; yco= 170; width= 300;

	uiDefBlockBut(block, sensor_menu, NULL, "Sensors", xco-10, yco+35, 70, UI_UNIT_Y, "");
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_SENS_SEL, B_REDR, "Sel", xco+80, yco+35, (width-70)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_SENS_ACT, B_REDR, "Act", xco+80+(width-70)/4, yco+35, (width-70)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_SENS_LINK, B_REDR, "Link", xco+80+2*(width-70)/4, yco+35, (width-70)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show linked Objects to Controller");
	uiDefButBitS(block, TOG, BUTS_SENS_STATE, B_REDR, "State", xco+80+3*(width-70)/4, yco+35, (width-70)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show only sensors connected to active states");
	uiBlockEndAlign(block);
	
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
//		uiClearButLock();
//		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		
		if ( (ob->scavisflag & OB_VIS_SENS) == 0) continue;
		
		/* presume it is only objects for now */
		uiBlockBeginAlign(block);
//		if (ob->sensors.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWSENS, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide sensors");
//		if (ob->sensors.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDSENS, B_ADD_SENS, "Add",(short)(xco+width-40), yco, 50, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Add a new Sensor");
		uiBlockEndAlign(block);
		yco-=20;
		
		if (ob->scaflag & OB_SHOWSENS) {
			
			sens= ob->sensors.first;
			while (sens) {
				if (!(slogic->scaflag & BUTS_SENS_STATE) ||
					 (sens->totlinks == 0) ||		/* always display sensor without links so that is can be edited */
					 (sens->flag & SENS_PIN && slogic->scaflag & BUTS_SENS_STATE) || /* states can hide some sensors, pinned sensors ignore the visible state */
					 (is_sensor_linked(block, sens))
				) {
					/* should we draw the pin? - for now always draw when there is a state */
					pin = (slogic->scaflag & BUTS_SENS_STATE && (sens->flag & SENS_SHOW || sens->flag & SENS_PIN)) ? 1 : 0;
					
					sens->flag |= SENS_VISIBLE;
					uiDefIconButBitS(block, TOG, SENS_DEL, B_DEL_SENS, ICON_X,	xco, yco, 22, UI_UNIT_Y, &sens->flag, 0, 0, 0, 0, "Delete Sensor");
					if (pin)
						uiDefIconButBitS(block, ICONTOG, SENS_PIN, B_REDR, ICON_PINNED, (short)(xco+width-44), yco, 22, UI_UNIT_Y, &sens->flag, 0, 0, 0, 0, "Display when not linked to a visible states controller");
					
					uiDefIconButBitS(block, ICONTOG, SENS_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, UI_UNIT_Y, &sens->flag, 0, 0, 0, 0, "Sensor settings");

					ycoo= yco;
					if (sens->flag & SENS_SHOW) {
						uiDefButS(block, MENU, B_CHANGE_SENS, sensor_pup(),	(short)(xco+22), yco, 80, UI_UNIT_Y, &sens->type, 0, 0, 0, 0, "Sensor type");
						but = uiDefBut(block, TEX, 1, "", (short)(xco+102), yco, (short)(width-(pin?146:124)), UI_UNIT_Y, sens->name, 0, MAX_NAME, 0, 0, "Sensor name");
						uiButSetFunc(but, make_unique_prop_names_cb, sens->name, (void*) 0);

						sens->otype= sens->type;
						yco= draw_sensorbuttons(ob, sens, block, xco, yco, width);
						if (yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
					}
					else {
						set_col_sensor(sens->type, 1);
						glRecti(xco+22, yco, xco+width-22,yco+19);
						but = uiDefBut(block, LABEL, 0, sensor_name(sens->type),	(short)(xco+22), yco, 80, UI_UNIT_Y, sens, 0, 0, 0, 0, "");
						//uiButSetFunc(but, old_sca_move_sensor, sens, NULL);
						but = uiDefBut(block, LABEL, 0, sens->name, (short)(xco+102), yco, (short)(width-(pin?146:124)), UI_UNIT_Y, sens, 0, MAX_NAME, 0, 0, "");
						//uiButSetFunc(but, old_sca_move_sensor, sens, NULL);

						uiBlockBeginAlign(block);
						but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_UP, (short)(xco+width-(66+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick up");
						uiButSetFunc(but, old_sca_move_sensor, sens, (void *)TRUE);
						but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_DOWN, (short)(xco+width-(44+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick down");
						uiButSetFunc(but, old_sca_move_sensor, sens, (void *)FALSE);
						uiBlockEndAlign(block);
					}

					but = uiDefIconBut(block, LINK, 0, ICON_LINK,	(short)(xco+width), ycoo, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
					uiSetButLink(but, NULL, (void ***)&(sens->links), &sens->totlinks, LINK_SENSOR, LINK_CONTROLLER);

					yco-=20;
				}
				sens= sens->next;
			}
			yco-= 6;
		}
	}
	/* ******************************* */
	xco= 800; yco= 170; width= 300;
	uiDefBlockBut(block, actuator_menu, NULL, "Actuators", xco-10, yco+35, 90, UI_UNIT_Y, "");

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_ACT_SEL, B_REDR, "Sel", xco+110, yco+35, (width-100)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_ACT_ACT, B_REDR, "Act", xco+110+(width-100)/4, yco+35, (width-100)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_ACT_LINK, B_REDR, "Link", xco+110+2*(width-100)/4, yco+35, (width-100)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show linked Objects to Controller");
	uiDefButBitS(block, TOG, BUTS_ACT_STATE, B_REDR, "State", xco+110+3*(width-100)/4, yco+35, (width-100)/4, UI_UNIT_Y, &slogic->scaflag, 0, 0, 0, 0, "Show only actuators connected to active states");
	uiBlockEndAlign(block);
	for (a=0; a<count; a++) {
		ob= (Object *)idar[a];
//		uiClearButLock();
//		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		if ( (ob->scavisflag & OB_VIS_ACT) == 0) continue;

		/* presume it is only objects for now */
		uiBlockBeginAlign(block);
//		if (ob->actuators.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWACT, B_REDR, ob->id.name+2,(short)(xco-10), yco,(short)(width-30), UI_UNIT_Y, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide actuators");
//		if (ob->actuators.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDACT, B_ADD_ACT, "Add",(short)(xco+width-40), yco, 50, UI_UNIT_Y, &ob->scaflag, 0, 0, 0, 0, "Add a new Actuator");
		uiBlockEndAlign(block);
		yco-=20;
		
		if (ob->scaflag & OB_SHOWACT) {
			
			act= ob->actuators.first;
			while (act) {
				if (!(slogic->scaflag & BUTS_ACT_STATE) ||
					!(act->flag & ACT_LINKED) ||		/* always display actuators without links so that is can be edited */
					 (act->flag & ACT_VISIBLE) ||		/* this actuator has visible connection, display it */
					 (act->flag & ACT_PIN && slogic->scaflag & BUTS_ACT_STATE)) {
					
					pin = (slogic->scaflag & BUTS_ACT_STATE && (act->flag & SENS_SHOW || act->flag & SENS_PIN)) ? 1 : 0;
					
					act->flag |= ACT_VISIBLE;	/* mark the actuator as visible to help implementing the up/down action */
					uiDefIconButBitS(block, TOG, ACT_DEL, B_DEL_ACT, ICON_X,	xco, yco, 22, UI_UNIT_Y, &act->flag, 0, 0, 0, 0, "Delete Actuator");
					if (pin)
						uiDefIconButBitS(block, ICONTOG, ACT_PIN, B_REDR, ICON_PINNED, (short)(xco+width-44), yco, 22, UI_UNIT_Y, &act->flag, 0, 0, 0, 0, "Display when not linked to a visible states controller");
					uiDefIconButBitS(block, ICONTOG, ACT_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, UI_UNIT_Y, &act->flag, 0, 0, 0, 0, "Display the actuator");
					
					if (act->flag & ACT_SHOW) {
						act->otype= act->type;
						uiDefButS(block, MENU, B_CHANGE_ACT, actuator_pup(ob),	(short)(xco+22), yco, 90, UI_UNIT_Y, &act->type, 0, 0, 0, 0, "Actuator type");
						but = uiDefBut(block, TEX, 1, "", (short)(xco+112), yco, (short)(width-(pin?156:134)), UI_UNIT_Y, act->name, 0, MAX_NAME, 0, 0, "Actuator name");
						uiButSetFunc(but, make_unique_prop_names_cb, act->name, (void*) 0);

						ycoo= yco;
						yco= draw_actuatorbuttons(bmain, ob, act, block, xco, yco, width);
						if (yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
					}
					else {
						set_col_actuator(act->type, 1);
						glRecti((short)(xco+22), yco, (short)(xco+width-22),(short)(yco+19));
						/* but= */ uiDefBut(block, LABEL, 0, actuator_name(act->type), (short)(xco+22), yco, 90, UI_UNIT_Y, act, 0, 0, 0, 0, "Actuator type");
						// uiButSetFunc(but, old_sca_move_actuator, act, NULL);
						/* but= */ uiDefBut(block, LABEL, 0, act->name, (short)(xco+112), yco, (short)(width-(pin?156:134)), UI_UNIT_Y, act, 0, 0, 0, 0, "Actuator name");
						// uiButSetFunc(but, old_sca_move_actuator, act, NULL);

						uiBlockBeginAlign(block);
						but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_UP, (short)(xco+width-(66+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick up");
						uiButSetFunc(but, old_sca_move_actuator, act, (void *)TRUE);
						but = uiDefIconBut(block, BUT, B_REDR, ICON_TRIA_DOWN, (short)(xco+width-(44+5)), yco, 22, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Move this logic brick down");
						uiButSetFunc(but, old_sca_move_actuator, act, (void *)FALSE);
						uiBlockEndAlign(block);

						ycoo= yco;
					}

					uiDefIconBut(block, INLINK, 0, ICON_INLINK,(short)(xco-19), ycoo, UI_UNIT_X, UI_UNIT_Y, act, LINK_ACTUATOR, 0, 0, 0, "");

					yco-=20;
				}
				act= act->next;
			}
			yco-= 6;
		}
	}

	uiComposeLinks(block);
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);

	if (idar) MEM_freeN(idar);
}






