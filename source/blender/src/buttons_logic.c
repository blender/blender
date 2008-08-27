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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_material_types.h"
#include "DNA_sensor_types.h"
#include "DNA_actuator_types.h"
#include "DNA_controller_types.h"
#include "DNA_property_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_mesh_types.h"
#include "DNA_world_types.h"

#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sca.h"
#include "BKE_property.h"
#include "BKE_property.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_interface.h"
#include "BIF_butspace.h"
#include "BIF_screen.h"
#include "BIF_keyval.h"
#include "BIF_editsound.h"

#include "BIF_editsca.h"


#include "BDR_editcurve.h"
#include "BDR_editobject.h"
#include "BSE_headerbuttons.h"
#include "BSE_filesel.h"

#include "blendef.h"
#include "mydevice.h"
#include "nla.h"	/* For __NLA : Important, do not remove */
#include "butspace.h" // own module
#include "interface.h"

/* internals */
void buttons_enji(uiBlock *, Object *);
void buttons_ketsji(uiBlock *, Object *);
void buttons_bullet(uiBlock *, Object *);

/****/

static ID **get_selected_and_linked_obs(short *count, short scavisflag);
static char *actuator_pup(Object *owner);

/****/


static void del_property(void *selpropv, void *data2_unused)
{
	bProperty *prop, *selprop= selpropv;
	Object *ob;
	int a=0;
		
	ob= OBACT;
	if(ob==NULL) return;

	prop= ob->prop.first;
	while(prop) {
		if(prop==selprop) {
			if (strcmp(prop->name,"Text") == 0) {
				allqueue(REDRAWVIEW3D, 0);
			}
			BLI_remlink(&ob->prop, prop);
			free_property(prop);
			break;
		}
		a++;
		prop= prop->next;
	}
	BIF_undo_push("Delete property");
	allqueue(REDRAWBUTSLOGIC, 0);
	
}

static int vergname(const void *v1, const void *v2)
{
	char **x1, **x2;
	
	x1= (char **)v1;
	x2= (char **)v2;
	
	return strcmp(*x1, *x2);
}

void make_unique_prop_names(char *str)
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

	idar= get_selected_and_linked_obs(&obcount, BUTS_SENS_SEL|BUTS_SENS_ACT|BUTS_ACT_SEL|BUTS_ACT_ACT|BUTS_CONT_SEL|BUTS_CONT_ACT);
	
	/* for each object, make properties and sca names unique */
	
	/* count total names */
	for(a=0; a<obcount; a++) {
		ob= (Object *)idar[a];
		propcount+= BLI_countlist(&ob->prop);
		propcount+= BLI_countlist(&ob->sensors);
		propcount+= BLI_countlist(&ob->controllers);
		propcount+= BLI_countlist(&ob->actuators);
	}	
	if(propcount==0) {
		if(idar) MEM_freeN(idar);
		return;
	}
	
	/* make names array for sorting */
	names= MEM_callocN(propcount*sizeof(void *), "names");

	/* count total names */
	nr= 0;
	for(a=0; a<obcount; a++) {
		ob= (Object *)idar[a];
		prop= ob->prop.first;
		while(prop) {
			names[nr++]= prop->name;
			prop= prop->next;
		}
		sens= ob->sensors.first;
		while(sens) {
			names[nr++]= sens->name;
			sens= sens->next;
		}
		cont= ob->controllers.first;
		while(cont) {
			names[nr++]= cont->name;
			cont= cont->next;
		}
		act= ob->actuators.first;
		while(act) {
			names[nr++]= act->name;
			act= act->next;
		}
	}

	qsort(names, propcount, sizeof(void *), vergname);
	
	/* now we check for double names, and change them */
	
	for(nr=0; nr<propcount; nr++) {
		if(names[nr]!=str && strcmp( names[nr], str )==0 ) {
			BLI_newname(str, +1);
		}
	}
	
	MEM_freeN(idar);
	MEM_freeN(names);
}

static void make_unique_prop_names_cb(void *strv, void *redraw_view3d_flagv)
{
	char *str= strv;
	int redraw_view3d_flag= GET_INT_FROM_POINTER(redraw_view3d_flagv);

	make_unique_prop_names(str);
	if (redraw_view3d_flag) allqueue(REDRAWVIEW3D, 0);
}

static void sca_move_sensor(void *datav, void *data2_unused)
{
	bSensor *sens_to_delete= datav;
	int val;
	Base *base;
	bSensor *sens, *tmp;
	
	val= pupmenu("Move up%x1|Move down %x2");
	
	if(val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while(base) {
		
			sens= base->object->sensors.first;
			while(sens) {
				if(sens == sens_to_delete) break;
				sens= sens->next;
			}
			
			if(sens) {
				if( val==1 && sens->prev) {
					for (tmp=sens->prev; tmp; tmp=tmp->prev) {
						if (tmp->flag & SENS_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->sensors, sens);
						BLI_insertlinkbefore(&base->object->sensors, tmp, sens);
					}
				}
				else if( val==2 && sens->next) {
					for (tmp=sens->next; tmp; tmp=tmp->next) {
						if (tmp->flag & SENS_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->sensors, sens);
						BLI_insertlink(&base->object->sensors, tmp, sens);
					}
				}
				BIF_undo_push("Move sensor");
				allqueue(REDRAWBUTSLOGIC, 0);
				break;
			}
			
			base= base->next;
		}
	}
}

static void sca_move_controller(void *datav, void *data2_unused)
{
	bController *controller_to_del= datav;
	int val;
	Base *base;
	bController *cont, *tmp;
	
	val= pupmenu("Move up%x1|Move down %x2");
	
	if(val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while(base) {
		
			cont= base->object->controllers.first;
			while(cont) {
				if(cont == controller_to_del) break;
				cont= cont->next;
			}
			
			if(cont) {
				if( val==1 && cont->prev) {
					/* locate the controller that has the same state mask but is earlier in the list */
					tmp = cont->prev;
					while(tmp) {
						if(tmp->state_mask & cont->state_mask) 
							break;
						tmp = tmp->prev;
					}
					if (tmp) {
						BLI_remlink(&base->object->controllers, cont);
						BLI_insertlinkbefore(&base->object->controllers, tmp, cont);
					}
				}
				else if( val==2 && cont->next) {
					tmp = cont->next;
					while(tmp) {
						if(tmp->state_mask & cont->state_mask) 
							break;
						tmp = tmp->next;
					}
					BLI_remlink(&base->object->controllers, cont);
					BLI_insertlink(&base->object->controllers, tmp, cont);
				}
				BIF_undo_push("Move controller");
				allqueue(REDRAWBUTSLOGIC, 0);
				break;
			}
			
			base= base->next;
		}
	}
}

static void sca_move_actuator(void *datav, void *data2_unused)
{
	bActuator *actuator_to_move= datav;
	int val;
	Base *base;
	bActuator *act, *tmp;
	
	val= pupmenu("Move up%x1|Move down %x2");
	
	if(val>0) {
		/* now find out which object has this ... */
		base= FIRSTBASE;
		while(base) {
		
			act= base->object->actuators.first;
			while(act) {
				if(act == actuator_to_move) break;
				act= act->next;
			}
			
			if(act) {
				if( val==1 && act->prev) {
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
				else if( val==2 && act->next) {
					for (tmp=act->next; tmp; tmp=tmp->next) {
						if (tmp->flag & ACT_VISIBLE)
							break;
					}
					if (tmp) {
						BLI_remlink(&base->object->actuators, act);
						BLI_insertlink(&base->object->actuators, tmp, act);
					}
				}
				BIF_undo_push("Move actuator");
				allqueue(REDRAWBUTSLOGIC, 0);
				break;
			}
			
			base= base->next;
		}
	}
}

void do_logic_buts(unsigned short event)
{
	bProperty *prop;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	Object *ob;
	int didit, bit;
	
	ob= OBACT;
	if(ob==0) return;
	
	switch(event) {

	case B_SETSECTOR:
		/* check for inconsistant types */
		ob->gameflag &= ~(OB_PROP|OB_MAINACTOR|OB_DYNAMIC|OB_ACTOR);
		ob->dtx |= OB_BOUNDBOX;
		allqueue(REDRAWBUTSGAME, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
		
	case B_SETPROP:
		/* check for inconsistant types */
		ob->gameflag &= ~(OB_SECTOR|OB_MAINACTOR|OB_DYNAMIC|OB_ACTOR);
		allqueue(REDRAWBUTSGAME, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_SETACTOR:
	case B_SETDYNA:
	case B_SETMAINACTOR:
		ob->gameflag &= ~(OB_SECTOR|OB_PROP);
		allqueue(REDRAWBUTSGAME, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;


	case B_ADD_PROP:
		prop= new_property(PROP_FLOAT);
		make_unique_prop_names(prop->name);
		BLI_addtail(&ob->prop, prop);
		BIF_undo_push("Add property");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	
	case B_CHANGE_PROP:
		prop= ob->prop.first;
		while(prop) {
			if(prop->type!=prop->otype) {
				init_property(prop);
				if (strcmp(prop->name, "Text") == 0) {
					allqueue(REDRAWVIEW3D, 0);
				}
			}
			prop= prop->next;
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	
	case B_ADD_SENS:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			if(ob->scaflag & OB_ADDSENS) {
				ob->scaflag &= ~OB_ADDSENS;
				sens= new_sensor(SENS_ALWAYS);
				BLI_addtail(&(ob->sensors), sens);
				make_unique_prop_names(sens->name);
				ob->scaflag |= OB_SHOWSENS;
			}
		}
		
		BIF_undo_push("Add sensor");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_CHANGE_SENS:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			sens= ob->sensors.first;
			while(sens) {
				if(sens->type != sens->otype) {
					init_sensor(sens);
					sens->otype= sens->type;
					break;
				}
				sens= sens->next;
			}
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	
	case B_DEL_SENS:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			sens= ob->sensors.first;
			while(sens) {
				if(sens->flag & SENS_DEL) {
					BLI_remlink(&(ob->sensors), sens);
					free_sensor(sens);
					break;
				}
				sens= sens->next;
			}
		}
		BIF_undo_push("Delete sensor");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	
	case B_ADD_CONT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			if(ob->scaflag & OB_ADDCONT) {
				ob->scaflag &= ~OB_ADDCONT;
				cont= new_controller(CONT_LOGIC_AND);
				make_unique_prop_names(cont->name);
				ob->scaflag |= OB_SHOWCONT;
				BLI_addtail(&(ob->controllers), cont);
				/* set the controller state mask from the current object state.
				   A controller is always in a single state, so select the lowest bit set
				   from the object state */
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
		BIF_undo_push("Add controller");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_SET_STATE_BIT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			if(ob->scaflag & OB_SETSTBIT) {
				ob->scaflag &= ~OB_SETSTBIT;
				ob->state = 0x3FFFFFFF;
			}
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_INIT_STATE_BIT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			if(ob->scaflag & OB_INITSTBIT) {
				ob->scaflag &= ~OB_INITSTBIT;
				ob->state = ob->init_state;
				if (!ob->state)
					ob->state = 1;
			}
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_CHANGE_CONT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			cont= ob->controllers.first;
			while(cont) {
				if(cont->type != cont->otype) {
					init_controller(cont);
					cont->otype= cont->type;
					break;
				}
				cont= cont->next;
			}
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	

	case B_DEL_CONT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			cont= ob->controllers.first;
			while(cont) {
				if(cont->flag & CONT_DEL) {
					BLI_remlink(&(ob->controllers), cont);
					unlink_controller(cont);
					free_controller(cont);
					break;
				}
				cont= cont->next;
			}
		}
		BIF_undo_push("Delete controller");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_ADD_ACT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			if(ob->scaflag & OB_ADDACT) {
				ob->scaflag &= ~OB_ADDACT;
				act= new_actuator(ACT_OBJECT);
				make_unique_prop_names(act->name);
				BLI_addtail(&(ob->actuators), act);
				ob->scaflag |= OB_SHOWACT;
			}
		}
		BIF_undo_push("Add actuator");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_CHANGE_ACT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while(act) {
				if(act->type != act->otype) {
					init_actuator(act);
					act->otype= act->type;
					break;
				}
				act= act->next;
			}
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		break;

	case B_DEL_ACT:
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while(act) {
				if(act->flag & ACT_DEL) {
					BLI_remlink(&(ob->actuators), act);
					unlink_actuator(act);
					free_actuator(act);
					break;
				}
				act= act->next;
			}
		}
		BIF_undo_push("Delete actuator");
		allqueue(REDRAWBUTSLOGIC, 0);
		break;
	
	case B_SOUNDACT_BROWSE:
		/* since we don't know which... */
		didit= 0;
		for(ob=G.main->object.first; ob; ob=ob->id.next) {
			act= ob->actuators.first;
			while(act)
			{
				if(act->type==ACT_SOUND)
				{
					bSoundActuator *sa= act->data;
					if(sa->sndnr)
					{
						bSound *sound= G.main->sound.first;
						int nr= 1;

						if(sa->sndnr == -2) {
							activate_databrowse((ID *)G.main->sound.first, ID_SO, 0, B_SOUNDACT_BROWSE,
											&sa->sndnr, do_logic_buts);
							break;
						}

						while(sound)
						{
							if(nr==sa->sndnr)
								break;
							nr++;
							sound= sound->id.next;
						}
						
						if(sa->sound)
							sa->sound->id.us--;
						
						sa->sound= sound;
						
						if(sound)
							sound->id.us++;
						
						sa->sndnr= 0;
						didit= 1;
					}
				}
				act= act->next;
			}
			if(didit)
				break;
		}
		allqueue(REDRAWBUTSLOGIC, 0);
		allqueue(REDRAWSOUND, 0);

		break;
	}
}


static char *sensor_name(int type)
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

static char *sensor_pup(void)
{
	/* the number needs to match defines in game.h */
	return "Sensors %t|Always %x0|Delay %x13|Keyboard %x3|Mouse %x5|"
		"Touch %x1|Collision %x6|Near %x2|Radar %x7|"
		"Property %x4|Random %x8|Ray %x9|Message %x10|Joystick %x11|Actuator %x12";
}

static char *controller_name(int type)
{
	switch (type) {
	case CONT_LOGIC_AND:
		return "AND";
	case CONT_LOGIC_OR:
		return "OR";
	case CONT_LOGIC_NAND:
		return "NAND";
	case CONT_LOGIC_NOR:
		return "NOR";
	case CONT_LOGIC_XOR:
		return "XOR";
	case CONT_LOGIC_XNOR:
		return "XNOR";
	case CONT_EXPRESSION:
		return "Expression";
	case CONT_PYTHON:
		return "Python";
	}
	return "unknown";
}

static char *controller_pup(void)
{
	return "Controllers   %t|AND %x0|OR %x1|XOR %x6|NAND %x4|NOR %x5|XNOR %x7|Expression %x2|Python %x3";
}

static char *actuator_name(int type)
{
	switch (type) {
	case ACT_SHAPEACTION:
		return "Shape Action";
	case ACT_ACTION:
		return "Action";
	case ACT_OBJECT:
		return "Motion";
	case ACT_IPO:
		return "Ipo";
	case ACT_LAMP:
		return "Lamp";
	case ACT_CAMERA:
		return "Camera";
	case ACT_MATERIAL:
		return "Material";
	case ACT_SOUND:
		return "Sound";
	case ACT_CD:
		return "CD";
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
		return "2D Filter";
	case ACT_PARENT:
		return "Parent";
	case ACT_STATE:
		return "State";
	}
	return "unknown";
}




static char *actuator_pup(Object *owner)
{
	switch (owner->type)
	{
	case OB_ARMATURE:
		return "Actuators  %t|Action %x15|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
			"|Scene %x11|Random %x13|Message %x14|CD %x16|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
		break;

	case OB_MESH:
		return "Actuators  %t|Shape Action %x21|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
			"|Scene %x11|Random %x13|Message %x14|CD %x16|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
		break;

	default:
		return "Actuators  %t|Motion %x0|Constraint %x9|Ipo %x1"
			"|Camera %x3|Sound %x5|Property %x6|Edit Object %x10"
			"|Scene %x11|Random %x13|Message %x14|CD %x16|Game %x17"
			"|Visibility %x18|2D Filter %x19|Parent %x20|State %x22";
	}
}



static void set_sca_ob(Object *ob)
{
	bController *cont;
	bActuator *act;

	cont= ob->controllers.first;
	while(cont) {
		cont->mynew= (bController *)ob;
		cont= cont->next;
	}
	act= ob->actuators.first;
	while(act) {
		act->mynew= (bActuator *)ob;
		act= act->next;
	}
}

static ID **get_selected_and_linked_obs(short *count, short scavisflag)
{
	Base *base;
	Object *ob, *obt;
	ID **idar;
	bSensor *sens;
	bController *cont;
	unsigned int lay;
	int a, nr, doit;
	
	/* we need a sorted object list */
	/* set scavisflags flags in Objects to indicate these should be evaluated */
	/* also hide ob pointers in ->new entries of controllerss/actuators */
	
	*count= 0;
	
	if(G.scene==NULL) return NULL;
	
	ob= G.main->object.first;
	while(ob) {
		ob->scavisflag= 0;
		set_sca_ob(ob);
		ob= ob->id.next;
	}
	
	if(G.vd) lay= G.vd->lay;
	else lay= G.scene->lay;
	
	base= FIRSTBASE;
	while(base) {
		if(base->lay & lay) {
			if(base->flag & SELECT) {
				if(scavisflag & BUTS_SENS_SEL) base->object->scavisflag |= OB_VIS_SENS;
				if(scavisflag & BUTS_CONT_SEL) base->object->scavisflag |= OB_VIS_CONT;
				if(scavisflag & BUTS_ACT_SEL) base->object->scavisflag |= OB_VIS_ACT;
			}
		}
		base= base->next;
	}

	if(OBACT) {
		if(scavisflag & BUTS_SENS_ACT) OBACT->scavisflag |= OB_VIS_SENS;
		if(scavisflag & BUTS_CONT_ACT) OBACT->scavisflag |= OB_VIS_CONT;
		if(scavisflag & BUTS_ACT_ACT) OBACT->scavisflag |= OB_VIS_ACT;
	}
	
	/* BUTS_XXX_STATE are similar to BUTS_XXX_LINK for selecting the object */
	if(scavisflag & (BUTS_SENS_LINK|BUTS_CONT_LINK|BUTS_ACT_LINK|BUTS_SENS_STATE|BUTS_ACT_STATE)) {
		doit= 1;
		while(doit) {
			doit= 0;
			
			ob= G.main->object.first;
			while(ob) {
			
				/* 1st case: select sensor when controller selected */
				if((scavisflag & (BUTS_SENS_LINK|BUTS_SENS_STATE)) && (ob->scavisflag & OB_VIS_SENS)==0) {
					sens= ob->sensors.first;
					while(sens) {
						for(a=0; a<sens->totlinks; a++) {
							if(sens->links[a]) {
								obt= (Object *)sens->links[a]->mynew;
								if(obt && (obt->scavisflag & OB_VIS_CONT)) {
									doit= 1;
									ob->scavisflag |= OB_VIS_SENS;
									break;
								}
							}
						}
						if(doit) break;
						sens= sens->next;
					}
				}
				
				/* 2nd case: select cont when act selected */
				if((scavisflag & BUTS_CONT_LINK)  && (ob->scavisflag & OB_VIS_CONT)==0) {
					cont= ob->controllers.first;
					while(cont) {
						for(a=0; a<cont->totlinks; a++) {
							if(cont->links[a]) {
								obt= (Object *)cont->links[a]->mynew;
								if(obt && (obt->scavisflag & OB_VIS_ACT)) {
									doit= 1;
									ob->scavisflag |= OB_VIS_CONT;
									break;
								}
							}
						}
						if(doit) break;
						cont= cont->next;
					}
				}
				
				/* 3rd case: select controller when sensor selected */
				if((scavisflag & BUTS_CONT_LINK) && (ob->scavisflag & OB_VIS_SENS)) {
					sens= ob->sensors.first;
					while(sens) {
						for(a=0; a<sens->totlinks; a++) {
							if(sens->links[a]) {
								obt= (Object *)sens->links[a]->mynew;
								if(obt && (obt->scavisflag & OB_VIS_CONT)==0) {
									doit= 1;
									obt->scavisflag |= OB_VIS_CONT;
								}
							}
						}
						sens= sens->next;
					}
				}
				
				/* 4th case: select actuator when controller selected */
				if( (scavisflag & (BUTS_ACT_LINK|BUTS_ACT_STATE))  && (ob->scavisflag & OB_VIS_CONT)) {
					cont= ob->controllers.first;
					while(cont) {
						for(a=0; a<cont->totlinks; a++) {
							if(cont->links[a]) {
								obt= (Object *)cont->links[a]->mynew;
								if(obt && (obt->scavisflag & OB_VIS_ACT)==0) {
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
	ob= G.main->object.first;
	while(ob) {
		if( ob->scavisflag ) (*count)++;
		ob= ob->id.next;
	}

	if(*count==0) return NULL;
	if(*count>24) *count= 24;		/* temporal */
	
	idar= MEM_callocN( (*count)*sizeof(void *), "idar");
	
	ob= G.main->object.first;
	nr= 0;
	while(ob) {
		if( ob->scavisflag ) {
			idar[nr]= (ID *)ob;
			nr++;
		}
		if(nr>=24) break;
		ob= ob->id.next;
	}
	
	/* just to be sure... these were set in set_sca_done_ob() */
	clear_sca_new_poins();
	
	return idar;
}


static int get_col_sensor(int type)
{
	switch(type) {
	case SENS_ALWAYS:		return TH_BUT_ACTION;
	case SENS_DELAY:		return TH_BUT_ACTION;
	case SENS_TOUCH:		return TH_BUT_NEUTRAL;
	case SENS_COLLISION:	return TH_BUT_SETTING;
	case SENS_NEAR:			return TH_BUT_SETTING1; 
	case SENS_KEYBOARD:		return TH_BUT_SETTING2;
	case SENS_PROPERTY:		return TH_BUT_NUM;
	case SENS_ACTUATOR:		return TH_BUT_NUM;
	case SENS_MOUSE:		return TH_BUT_TEXTFIELD;
	case SENS_RADAR:		return TH_BUT_POPUP;
	case SENS_RANDOM:		return TH_BUT_NEUTRAL;
	case SENS_RAY:			return TH_BUT_SETTING1;
	case SENS_MESSAGE:		return TH_BUT_SETTING2;
	case SENS_JOYSTICK:		return TH_BUT_NEUTRAL;
	default:				return TH_BUT_NEUTRAL;
	}
}
static void set_col_sensor(int type, int medium)
{
	int col= get_col_sensor(type);
	BIF_ThemeColorShade(col, medium?30:0);
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
	/* Pulsing and frequency */
	uiDefIconButBitS(block, TOG, SENS_PULSE_REPEAT, 1, ICON_DOTSUP,
			 (short)(x + 10 + 0. * (w-20)), (short)(y - 19), (short)(0.15 * (w-20)), 19,
			 &sens->pulse, 0.0, 0.0, 0, 0,
			 "Activate TRUE level triggering (pulse mode)");

	uiDefIconButBitS(block, TOG, SENS_NEG_PULSE_MODE, 1, ICON_DOTSDOWN,
			 (short)(x + 10 + 0.15 * (w-20)), (short)(y - 19), (short)(0.15 * (w-20)), 19,
			 &sens->pulse, 0.0, 0.0, 0, 0,
			 "Activate FALSE level triggering (pulse mode)");
	uiDefButS(block, NUM, 1, "f:",
			 (short)(x + 10 + 0.3 * (w-20)), (short)(y - 19), (short)(0.275 * (w-20)), 19,
			 &sens->freq, 0.0, 10000.0, 0, 0,
			 "Delay between repeated pulses (in logic tics, 0 = no delay)");
	
	/* value or shift? */
	uiDefButS(block, TOG, 1, "Inv",
			 (short)(x + 10 + 0.85 * (w-20)), (short)(y - 19), (short)(0.15 * (w-20)), 19,
			 &sens->invert, 0.0, 0.0, 0, 0,
			 "Invert the level (output) of this sensor");
	uiDefButS(block, TOG, 1, "Lvl",
			 (short)(x + 10 + 0.70 * (w-20)), (short)(y - 19), (short)(0.15 * (w-20)), 19,
			 &sens->level, 0.0, 0.0, 0, 0,
			 "Level detector versus edge detector (only applicable in case of logic state transition)");
}

static short draw_sensorbuttons(bSensor *sens, uiBlock *block, short xco, short yco, short width,char* objectname)
{
	bNearSensor      *ns           = NULL;
	bTouchSensor     *ts           = NULL;
	bKeyboardSensor  *ks           = NULL;
	bPropertySensor  *ps           = NULL;
	bMouseSensor     *ms           = NULL;
	bCollisionSensor *cs           = NULL;
	bRadarSensor     *rs           = NULL;
	bRandomSensor    *randomSensor = NULL;
	bRaySensor       *raySens      = NULL;
	bMessageSensor   *mes          = NULL;
	bJoystickSensor	 *joy		   = NULL;
	bActuatorSensor  *as           = NULL;
	bDelaySensor     *ds		   = NULL;
	short ysize;
	char *str;
	
	/* yco is at the top of the rect, draw downwards */
	
	uiBlockSetEmboss(block, UI_EMBOSSM);
	
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
			
			/* uiDefBut(block, TEX, 1, "Property:",	xco,yco-22,width, 19, &ts->name, 0, 31, 0, 0, "Only look for Objects with this property"); */
			uiDefIDPoinBut(block, test_matpoin_but, ID_MA, 1, "MA:",(short)(xco + 10),(short)(yco-44), (short)(width - 20), 19, &ts->ma,  "Only look for floors with this Material"); 
			///* uiDefButF(block, NUM, 1, "Margin:",	xco+width/2,yco-44,width/2, 19, &ts->dist, 0.0, 10.0, 100, 0, "Extra margin (distance) for larger sensitivity"); 
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

			uiDefButBitS(block, TOG, SENS_COLLISION_MATERIAL, B_REDR, "M/P",(short)(xco + 10),(short)(yco - 44),
				(short)(0.20 * (width-20)), 19, &cs->mode, 0.0, 0.0, 0, 0,
				"Toggle collision on material or property.");
			
			if (cs->mode & SENS_COLLISION_MATERIAL) {
				uiDefBut(block, TEX, 1, "Material:", (short)(xco + 10 + 0.20 * (width-20)),
					(short)(yco-44), (short)(0.8*(width-20)), 19, &cs->materialName, 0, 31, 0, 0,
					"Only look for Objects with this material");
			} else {
				uiDefBut(block, TEX, 1, "Property:", (short)(xco + 10 + 0.20 * (width-20)), (short)(yco-44),
					(short)(0.8*(width-20)), 19, &cs->name, 0, 31, 0, 0,
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
				&ns->name, 0, 31, 0, 0, "Only look for Objects with this property");
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
					 &rs->name, 0, 31, 0, 0,
					 "Only look for Objects with this property");

			str = "Type %t|+X axis %x0|+Y axis %x1|+Z axis %x2|-X axis %x3|-Y axis %x4|-Z axis %x5"; 
			uiDefButS(block, MENU, B_REDR, str,
				(short)(10+xco+0.7 * (width-20)), (short)(yco-44), (short)(0.3 * (width-22)), 19,
				&rs->axis, 2.0, 31, 0, 0,
				"Specify along which axis the radar cone is cast.");
				
			uiDefButF(block, NUM, 1, "Ang:",
					 (short)(10+xco), (short)(yco-68), (short)((width-20)/2), 19,
					 &rs->angle, 0.0, 179.9, 10, 0,
					 "Opening angle of the radar cone.");
			uiDefButF(block, NUM, 1, "Dist:",
					 (short)(xco+10 + (width-20)/2), (short)(yco-68), (short)((width-20)/2), 19,
					 &rs->range, 0.01, 10000.0, 100, 0,
					 "Depth of the radar cone");
			yco-= ysize;
			break;
		}
	case SENS_KEYBOARD:
		{
			/* 5 lines: 120 height */
			ysize= 120;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			/* header line */
			draw_default_sensor_header(sens, block, xco, yco, width);
			ks= sens->data;
			
			/* line 2: hotkey and allkeys toggle */
			uiDefKeyevtButS(block, B_DIFF, "", xco+40, yco-44, (width)/2, 19, &ks->key, "Key code");
			
			/* line 3: two key modifyers (qual1, qual2) */
			uiDefKeyevtButS(block, B_DIFF, "", xco+40, yco-68, (width-50)/2, 19, &ks->qual, "Modifier key code");
			uiDefKeyevtButS(block, B_DIFF, "", xco+40+(width-50)/2, yco-68, (width-50)/2, 19, &ks->qual2, "Second Modifier key code");
			
			/* labels for line 1 and 2 */
			uiDefBut(block, LABEL, 0, "Key",	  xco, yco-44, 40, 19, NULL, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "Hold",	  xco, yco-68, 40, 19, NULL, 0, 0, 0, 0, "");
			
			/* part of line 1 */
			uiBlockSetCol(block, TH_BUT_SETTING2);
			uiDefButBitS(block, TOG, 1, 0, "All keys",	  xco+40+(width/2), yco-44, (width/2)-50, 19,
				&ks->type, 0, 0, 0, 0, "");
			
			/* line 4: toggle property for string logging mode */
			uiDefBut(block, TEX, 1, "LogToggle: ",
				xco+10, yco-92, (width-20), 19,
				ks->toggleName, 0, 31, 0, 0,
				"Property that indicates whether to log "
				"keystrokes as a string.");
			
			/* line 5: target property for string logging mode */
			uiDefBut(block, TEX, 1, "Target: ",
				xco+10, yco-116, (width-20), 19,
				ks->targetName, 0, 31, 0, 0,
				"Property that receives the keystrokes in case "
				"a string is logged.");
			
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
					ps->name, 0, 31, 0, 0,  "Property name");
			}
			
			if(ps->type == SENS_PROP_INTERVAL)
			{
				uiDefBut(block, TEX, 1, "Min: ",		xco,yco-92,width/2, 19,
					ps->value, 0, 31, 0, 0, "test for min value");
				uiDefBut(block, TEX, 1, "Max: ",		xco+width/2,yco-92,width/2, 19,
					ps->maxvalue, 0, 31, 0, 0, "test for max value");
			}
			else if(ps->type == SENS_PROP_CHANGED);
			else
			{
				uiDefBut(block, TEX, 1, "Value: ",		xco+30,yco-92,width-60, 19,
					ps->value, 0, 31, 0, 0, "test for value");
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
					as->name, 0, 31, 0, 0,  "Actuator name, actuator active state modifications will be detected");
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
				&ds->delay, 0.0, 5000.0, 0, 0, "Delay in number of frames before the positive trigger");
			uiDefButS(block, NUM, 0, "Dur",(short)(10+xco+(width-22)*0.4+10),(short)(yco-44),(short)((width-22)*0.4-10), 19,
				&ds->duration, 0.0, 5000.0, 0, 0, "If >0, delay in number of frames before the negative trigger following the positive trigger");
			uiDefButBitS(block, TOG, SENS_DELAY_REPEAT, 0, "REP",(short)(xco + 10 + (width-22)*0.8),(short)(yco - 44),
				(short)(0.20 * (width-22)), 19, &ds->flag, 0.0, 0.0, 0, 0,
				"Toggle repeat option. If selected, the sensor restarts after Delay+Dur frames");
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
			str= "Type %t|Left button %x1|Middle button %x2|"
				"Right button %x4|Wheel Up %x5|Wheel Down %x6|Movement %x8|Mouse over %x16|Mouse over any%x32"; 
			uiDefButS(block, MENU, B_REDR, str, xco+10, yco-44, width-20, 19,
				&ms->type, 0, 31, 0, 0,
				"Specify the type of event this mouse sensor should trigger on.");
			
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
				"Toggle collision on material or property.");
			
			if (raySens->mode & SENS_COLLISION_MATERIAL)
			{
				uiDefBut(block, TEX, 1, "Material:", xco + 10 + 0.20 * (width-20), yco-44, 0.8*(width-20), 19,
					&raySens->matname, 0, 31, 0, 0,
					"Only look for Objects with this material");
			}
			else
			{
				uiDefBut(block, TEX, 1, "Property:", xco + 10 + 0.20 * (width-20), yco-44, 0.8*(width-20), 19,
					&raySens->propname, 0, 31, 0, 0,
					"Only look for Objects with this property");
			}
			
			/* 2. sensing range */
			uiDefButF(block, NUM, 1, "Range", xco+10, yco-68, 0.6 * (width-20), 19,
				&raySens->range, 0.01, 10000.0, 100, 0,
				"Sense objects no farther than this distance");
			
			/* 3. axis choice */
			str = "Type %t|+ X axis %x1|+ Y axis %x0|+ Z axis %x2|- X axis %x3|- Y axis %x4|- Z axis %x5"; 
			uiDefButI(block, MENU, B_REDR, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
				&raySens->axisflag, 2.0, 31, 0, 0,
				"Specify along which axis the ray is cast.");
			
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
				mes->subject, 0, 31, 0, 0,
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
			

			str= "Type %t|Button %x0|Axis %x1|Hat%x2"; 
			uiDefButS(block, MENU, B_REDR, str, xco+10, yco-44, 0.6 * (width-20), 19,
				&joy->type, 0, 31, 0, 0,
				"The type of event this joystick sensor is triggered on.");
			
			if(joy->type == SENS_JOY_BUTTON)
			{
				uiDefButI(block, NUM, 1, "Number:", xco+10, yco-68, 0.6 * (width-20), 19,
				&joy->button, 0, 18, 100, 0,
				"Specify which button to use");
				
				str = "Type %t|Pressed %x0|Released %x1"; 
				uiDefButI(block, MENU, B_REDR, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
				&joy->buttonf, 2.0, 31, 0, 0,
				"Button pressed or released.");
			}
			else if(joy->type == SENS_JOY_AXIS)
			{
				uiDefButI(block, NUM, 1, "Number:", xco+10, yco-68, 0.6 * (width-20), 19,
				&joy->axis, 1, 2.0, 100, 0,
				"Specify which axis to use");

				uiDefButI(block, NUM, 1, "Threshold:", xco+10 + 0.6 * (width-20),yco-44, 0.4 * (width-20), 19,
				&joy->precision, 0, 32768.0, 100, 0,
				"Specify the precision of the axis");

				str = "Type %t|Up Axis %x1 |Down Axis %x3|Left Axis %x2|Right Axis %x0"; 
				uiDefButI(block, MENU, B_REDR, str, xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
				&joy->axisf, 2.0, 31, 0, 0,
				"The direction of the axis");
			}
			else
			{
				uiDefButI(block, NUM, 1, "Number:", xco+10, yco-68, 0.6 * (width-20), 19,
				&joy->hat, 1, 2.0, 100, 0,
				"Specify which hat to use");
				
				uiDefButI(block, NUM, 1, "Direction:", xco+10 + 0.6 * (width-20), yco-68, 0.4 * (width-20), 19,
				&joy->hatf, 0, 12, 100, 0,
				"Specify hat direction");
			}
			yco-= ysize;
			break;
		}
	}
	
	uiBlockSetEmboss(block, UI_EMBOSSM);
	uiBlockSetCol(block, TH_AUTO);
	
	return yco-4;
}



static short draw_controllerbuttons(bController *cont, uiBlock *block, short xco, short yco, short width)
{
	bExpressionCont *ec;
	bPythonCont *pc;
	short ysize;
	
	uiBlockSetEmboss(block, UI_EMBOSSM);
	
	switch (cont->type) {
	case CONT_EXPRESSION:
		ysize= 28;

		BIF_ThemeColor(TH_BUT_SETTING);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		/* uiDefBut(block, LABEL, 1, "Not yet...", xco,yco-24,80, 19, NULL, 0, 0, 0, 0, ""); */
		ec= cont->data;	
		/* uiDefBut(block, BUT, 1, "Variables", xco,yco-24,80, 19, NULL, 0, 0, 0, 0, "Available variables for expression"); */
		uiDefBut(block, TEX, 1, "Exp:",		xco + 10 , yco-21, width-20, 19,
				 ec->str, 0, 127, 0, 0,
				 "Expression"); 
		
		yco-= ysize;
		break;
	case CONT_PYTHON:
		ysize= 28;
		
		if(cont->data==NULL) init_controller(cont);
		pc= cont->data;
		
		BIF_ThemeColor(TH_BUT_SETTING1);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

		uiDefIDPoinBut(block, test_scriptpoin_but, ID_SCRIPT, 1, "Script: ", xco+45,yco-24,width-90, 19, &pc->text, "");
		
		yco-= ysize;
		break;
		
	default:
		ysize= 4;

		BIF_ThemeColor(TH_BUT_NEUTRAL);
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		yco-= ysize;
	}
	
	uiBlockSetEmboss(block, UI_EMBOSSM);
	uiBlockSetCol(block, TH_AUTO);

	return yco;
}

static int get_col_actuator(int type)
{
	switch(type) {
	case ACT_ACTION:		return TH_BUT_ACTION;
	case ACT_SHAPEACTION:	return TH_BUT_ACTION;
	case ACT_OBJECT:		return TH_BUT_NEUTRAL;
	case ACT_IPO:			return TH_BUT_SETTING;
	case ACT_PROPERTY:		return TH_BUT_SETTING1;
	case ACT_SOUND:			return TH_BUT_SETTING2;
	case ACT_CD:			return TH_BUT_NUM;
	case ACT_CAMERA: 		return TH_BUT_TEXTFIELD;
	case ACT_EDIT_OBJECT: 		return TH_BUT_POPUP;
	case ACT_GROUP:			return TH_BUT_ACTION;
	case ACT_RANDOM:		return TH_BUT_NEUTRAL;
	case ACT_SCENE:			return TH_BUT_SETTING;
	case ACT_MESSAGE:		return TH_BUT_SETTING1;
	case ACT_GAME:			return TH_BUT_SETTING2;
	case ACT_VISIBILITY:		return TH_BUT_NUM;
	case ACT_CONSTRAINT:		return TH_BUT_ACTION;
	case ACT_STATE:			return TH_BUT_SETTING2;
	default:				return TH_BUT_NEUTRAL;
	}
}
static void set_col_actuator(int item, int medium) 
{
	int col= get_col_actuator(item);
	BIF_ThemeColorShade(col, medium?30:10);
	
}

static void change_object_actuator(void *act, void *arg)
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

static void change_ipo_actuator(void *arg1_but, void *arg2_ia)
{
	bIpoActuator *ia = arg2_ia;
	uiBut *but = arg1_but;

	if (but->retval & ACT_IPOFORCE)
		ia->flag &= ~ACT_IPOADD;
	else if (but->retval & ACT_IPOADD)
		ia->flag &= ~ACT_IPOFORCE;
	but->retval = B_REDR;
}

void update_object_actuator_PID(void *act, void *arg)
{
	bObjectActuator *oa = act;
	oa->forcerot[0] = 60.0f*oa->forcerot[1];
}

char *get_state_name(Object *ob, short bit)
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

static short draw_actuatorbuttons(Object *ob, bActuator *act, uiBlock *block, short xco, short yco, short width)
{
	bSoundActuator      *sa      = NULL;
	bCDActuator			*cda	 = NULL;
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
	
	float *fp;
	short ysize = 0, wval;
	char *str;
	int myline, stbit;
	uiBut *but;

	/* yco is at the top of the rect, draw downwards */
	uiBlockSetEmboss(block, UI_EMBOSSM);
	set_col_actuator(act->type, 0);
	
	switch (act->type)
	{
	case ACT_OBJECT:
		{
			oa = act->data;
			wval = (width-100)/3;
			if (oa->type == ACT_OBJECT_NORMAL)
			{
				if ( ob->gameflag & OB_DYNAMIC )
				{
					ysize= 175;
				}
				else
				{
					ysize= 72;
				}

				glRects(xco, yco-ysize, xco+width, yco);
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
				
				uiDefBut(block, LABEL, 0, "Loc",	xco, yco-45, 45, 19, NULL, 0, 0, 0, 0, "Sets the location");
				uiDefButF(block, NUM, 0, "",		xco+45, yco-45, wval, 19, oa->dloc, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-45, wval, 19, oa->dloc+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-45, wval, 19, oa->dloc+2, -10000.0, 10000.0, 10, 0, "");
				
				uiDefBut(block, LABEL, 0, "Rot",	xco, yco-64, 45, 19, NULL, 0, 0, 0, 0, "Sets the rotation");
				uiDefButF(block, NUM, 0, "",		xco+45, yco-64, wval, 19, oa->drot, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-64, wval, 19, oa->drot+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-64, wval, 19, oa->drot+2, -10000.0, 10000.0, 10, 0, "");

				uiDefButBitS(block, TOG, ACT_DLOC_LOCAL, 0, "L",		xco+45+3*wval, yco-45, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
				uiDefButBitS(block, TOG, ACT_DROT_LOCAL, 0, "L",		xco+45+3*wval, yco-64, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
	
				if ( ob->gameflag & OB_DYNAMIC )
				{
					uiDefBut(block, LABEL, 0, "Force",	xco, yco-87, 55, 19, NULL, 0, 0, 0, 0, "Sets the force");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-87, wval, 19, oa->forceloc, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-87, wval, 19, oa->forceloc+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-87, wval, 19, oa->forceloc+2, -10000.0, 10000.0, 10, 0, "");
				
					uiDefBut(block, LABEL, 0, "Torque", xco, yco-106, 55, 19, NULL, 0, 0, 0, 0, "Sets the torque");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-106, wval, 19, oa->forcerot, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-106, wval, 19, oa->forcerot+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-106, wval, 19, oa->forcerot+2, -10000.0, 10000.0, 10, 0, "");				
				}
				
				if ( ob->gameflag & OB_DYNAMIC )
				{
					uiDefBut(block, LABEL, 0, "LinV",	xco, yco-129, 45, 19, NULL, 0, 0, 0, 0, "Sets the linear velocity");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-129, wval, 19, oa->linearvelocity, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-129, wval, 19, oa->linearvelocity+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-129, wval, 19, oa->linearvelocity+2, -10000.0, 10000.0, 10, 0, "");
				
					uiDefBut(block, LABEL, 0, "AngV",	xco, yco-148, 45, 19, NULL, 0, 0, 0, 0, "Sets the angular velocity");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-148, wval, 19, oa->angularvelocity, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-148, wval, 19, oa->angularvelocity+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-148, wval, 19, oa->angularvelocity+2, -10000.0, 10000.0, 10, 0, "");
				
					uiDefBut(block, LABEL, 0, "Damp",	xco, yco-171, 45, 19, NULL, 0, 0, 0, 0, "Number of frames to reach the target velocity");
					uiDefButS(block, NUM, 0, "",		xco+45, yco-171, wval, 19, &oa->damping, 0.0, 1000.0, 100, 0, "");

					uiDefButBitS(block, TOG, ACT_FORCE_LOCAL, 0, "L",		xco+45+3*wval, yco-87, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_TORQUE_LOCAL, 0, "L",		xco+45+3*wval, yco-106, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_LIN_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-129, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
					uiDefButBitS(block, TOG, ACT_ANG_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-148, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Local transformation");
				
					uiDefButBitS(block, TOG, ACT_ADD_LIN_VEL, 0, "add",xco+45+3*wval+15, yco-129, 35, 19, &oa->flag, 0.0, 0.0, 0, 0, "Toggles between ADD and SET linV");
				}				
			} else if (oa->type == ACT_OBJECT_SERVO)
			{
				ysize= 172;
				
				glRects(xco, yco-ysize, xco+width, yco);
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
				
				uiDefBut(block, LABEL, 0, "linV",	xco, yco-45, 45, 19, NULL, 0, 0, 0, 0, "Sets the target linear velocity, it will be achieve by automatic application of force. Null velocity is a valid target");
				uiDefButF(block, NUM, 0, "",		xco+45, yco-45, wval, 19, oa->linearvelocity, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-45, wval, 19, oa->linearvelocity+1, -10000.0, 10000.0, 10, 0, "");
				uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-45, wval, 19, oa->linearvelocity+2, -10000.0, 10000.0, 10, 0, "");
				uiDefButBitS(block, TOG, ACT_LIN_VEL_LOCAL, 0, "L",		xco+45+3*wval, yco-45, 15, 19, &oa->flag, 0.0, 0.0, 0, 0, "Velocity is defined in local coordinates");

				uiDefBut(block, LABEL, 0, "Limit",	xco, yco-68, 45, 19, NULL, 0, 0, 0, 0, "Select if the force need to be limited along certain axis (local or global depending on LinV Local flag)");
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_X, B_REDR, "X",		xco+45, yco-68, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the X axis");
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_Y, B_REDR, "Y",		xco+45+wval, yco-68, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the Y axis");
				uiDefButBitS(block, TOG, ACT_SERVO_LIMIT_Z, B_REDR, "Z",		xco+45+2*wval, yco-68, wval, 19, &oa->flag, 0.0, 0.0, 0, 0, "Set limit to force along the Z axis");
				uiDefBut(block, LABEL, 0, "Max",	xco, yco-87, 45, 19, NULL, 0, 0, 0, 0, "Set the upper limit for force");
				uiDefBut(block, LABEL, 0, "Min",	xco, yco-106, 45, 19, NULL, 0, 0, 0, 0, "Set the lower limit for force");
				if (oa->flag & ACT_SERVO_LIMIT_X) {
					uiDefButF(block, NUM, 0, "",		xco+45, yco-87, wval, 19, oa->dloc, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45, yco-106, wval, 19, oa->drot, -10000.0, 10000.0, 10, 0, "");
				}
				if (oa->flag & ACT_SERVO_LIMIT_Y) {
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-87, wval, 19, oa->dloc+1, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+wval, yco-106, wval, 19, oa->drot+1, -10000.0, 10000.0, 10, 0, "");
				}
				if (oa->flag & ACT_SERVO_LIMIT_Z) {
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-87, wval, 19, oa->dloc+2, -10000.0, 10000.0, 10, 0, "");
					uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-106, wval, 19, oa->drot+2, -10000.0, 10000.0, 10, 0, "");
				}
				uiDefBut(block, LABEL, 0, "Servo",	xco, yco-129, 45, 19, NULL, 0, 0, 0, 0, "Coefficients of the PID servo controller");
				uiDefButF(block, NUMSLI, B_REDR, "P: ",		xco+45, yco-129, wval*3, 19, oa->forcerot, 0.00, 200.0, 100, 0, "Proportional coefficient, typical value is 60x Integral coefficient");
				uiDefBut(block, LABEL, 0, "Slow",	xco, yco-148, 45, 19, NULL, 0, 0, 0, 0, "Low value of I coefficient correspond to slow response");
				but = uiDefButF(block, NUMSLI, B_REDR, " I : ",		xco+45, yco-148, wval*3, 19, oa->forcerot+1, 0.0, 3.0, 1, 0, "Integral coefficient, low value (0.01) for slow response, high value (0.5) for fast response");
				uiButSetFunc(but, update_object_actuator_PID, oa, NULL);
				uiDefBut(block, LABEL, 0, "Fast",	xco+45+3*wval, yco-148, 45, 19, NULL, 0, 0, 0, 0, "High value of I coefficient correspond to fast response");
				uiDefButF(block, NUMSLI, B_REDR, "D: ",		xco+45, yco-167, wval*3, 19, oa->forcerot+2, -100.0, 100.0, 100, 0, "Derivate coefficient, not required, high values can cause instability");
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
			
			
			if(aa->type == ACT_ACTION_FROM_PROP)
			{
				uiDefBut(block, TEX, 0, "Prop: ",xco+10, yco-44, width-20, 19, aa->name, 0.0, 31.0, 0, 0, "Use this property to define the Action position");
			}
			else
			{
				uiDefButI(block, NUM, 0, "Sta: ",xco+10, yco-44, (width-20)/2, 19, &aa->sta, 0.0, MAXFRAMEF, 0, 0, "Start frame");
				uiDefButI(block, NUM, 0, "End: ",xco+10+(width-20)/2, yco-44, (width-20)/2, 19, &aa->end, 0.0, MAXFRAMEF, 0, 0, "End frame");
			}
						
			uiDefButS(block, NUM, 0, "Blendin: ", xco+10, yco-64, (width-20)/2, 19, &aa->blendin, 0.0, 32767, 0.0, 0.0, "Number of frames of motion blending");
			uiDefButS(block, NUM, 0, "Priority: ", xco+10+(width-20)/2, yco-64, (width-20)/2, 19, &aa->priority, 0.0, 100.0, 0.0, 0.0, "Execution priority - lower numbers will override actions with higher numbers, With 2 or more actions at once, the overriding channels must be lower in the stack");
			
			uiDefBut(block, TEX, 0, "FrameProp: ",xco+10, yco-84, width-20, 19, aa->frameProp, 0.0, 31.0, 0, 0, "Assign this property this actions current frame number");			

			
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
			if(aa->type == ACT_ACTION_MOTION)
			{
				uiDefButF(block, NUM, 0, "Cycle: ",xco+30, yco-84, (width-60)/2, 19, &aa->stridelength, 0.0, 2500.0, 0, 0, "Distance covered by a single cycle of the action");
			}
#endif
			
			
			
			yco-=ysize;
			break;
		}
	case ACT_IPO:
		{
			ia= act->data;
			
			ysize= 52;
			
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
					"Let the ipo acts in local coordinates, used in Force and Add mode."); 
			}

			if(ia->type==ACT_IPO_FROM_PROP) {
				uiDefBut(block, TEX, 0, 
					"Prop: ",		xco+10, yco-44, width-80, 19, 
					ia->name, 0.0, 31.0, 0, 0, 
					"Use this property to define the Ipo position");
			}
			else {
				uiDefButI(block, NUM, 0, 
					"Sta",		xco+10, yco-44, (width-80)/2, 19, 
					&ia->sta, 0.0, MAXFRAMEF, 0, 0, 
					"Start frame, (subtract 1 to match blenders frame numbers)");
				uiDefButI(block, NUM, 0, 
					"End",		xco+10+(width-80)/2, yco-44, (width-80)/2, 19, 
					&ia->end, 0.0, MAXFRAMEF, 0, 0, 
					"End frame, (subtract 1 to match blenders frame numbers)");
			}
			uiDefButBitS(block, TOG, ACT_IPOCHILD,  B_REDR, 
				"Child",	xco+10+(width-80), yco-44, 60, 19, 
				&ia->flag, 0, 0, 0, 0, 
				"Update IPO on all children Objects as well");

			yco-= ysize;
			break;
		}
	case ACT_PROPERTY:
		{
			ysize= 68;
			
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			pa= act->data;
			
			str= "Type   %t|Assign   %x0|Add %x1|Copy %x2";
			uiDefButI(block, MENU, B_REDR, str,		xco+30,yco-24,width-60, 19, &pa->type, 0, 31, 0, 0, "Type");
			
			uiDefBut(block, TEX, 1, "Prop: ",		xco+30,yco-44,width-60, 19, pa->name, 0, 31, 0, 0, "Property name");
			
			if(pa->type==ACT_PROP_COPY) {
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",	xco+10, yco-64, (width-20)/2, 19, &(pa->ob), "Copy from this Object");
				uiDefBut(block, TEX, 1, "Prop: ",		xco+10+(width-20)/2, yco-64, (width-20)/2, 19, pa->value, 0, 31, 0, 0, "Copy this property");
			}
			else {
				uiDefBut(block, TEX, 1, "Value: ",		xco+30,yco-64,width-60, 19, pa->value, 0, 31, 0, 0, "change with this value");
			}
			yco-= ysize;
			
			break;
		}
    case ACT_SOUND:
		{
			ysize = 70;
			
			sa = act->data;
			sa->sndnr = 0;
			
			wval = (width-20)/2;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			if(G.main->sound.first) {
				IDnames_to_pupstring(&str, "Sound files", NULL, &(G.main->sound), (ID *)sa->sound, &(sa->sndnr));
				/* reset this value, it is for handling the event */
				sa->sndnr = 0;
				uiDefButS(block, MENU, B_SOUNDACT_BROWSE, str, xco+10,yco-22,20,19, &(sa->sndnr), 0, 0, 0, 0, "");	

				if(sa->sound) {
					char dummy_str[] = "Sound mode %t|Play Stop %x0|Play End %x1|Loop Stop %x2|Loop End %x3|Loop Ping Pong Stop %x5|Loop Ping Pong %x4";
					uiDefBut(block, TEX, B_IDNAME, "SO:",xco+30,yco-22,width-40,19, sa->sound->id.name+2,    0.0, 21.0, 0, 0, "");
					uiDefButS(block, MENU, 1, dummy_str,xco+10,yco-44,width-20, 19, &sa->type, 0.0, 0.0, 0, 0, "");
					uiDefButF(block, NUM, 0, "Volume:", xco+10,yco-66,wval, 19, &sa->sound->volume, 0.0,  1.0, 0, 0, "Sets the volume of this sound");
					uiDefButF(block, NUM, 0, "Pitch:",xco+wval+10,yco-66,wval, 19, &sa->sound->pitch,-12.0, 12.0, 0, 0, "Sets the pitch of this sound");
				}
				MEM_freeN(str);
			} 
			else {
				uiDefBut(block, LABEL, 0, "Use Sound window (F10) to load samples", xco, yco-24, width, 19, NULL, 0, 0, 0, 0, "");
			}
					
			yco-= ysize;
			
			break;
		}
	case ACT_CD:
		{
			char cd_type_str[] = "Sound mode %t|Play all tracks %x0|Play one track %x1|"
				"Volume %x3|Stop %x4|Pause %x5|Resume %x6";
			cda = act->data;

			if (cda) {
				if (cda->track == 0) {
					cda->track = 1;
					cda->volume = 1;
					cda->type = ACT_CD_PLAY_ALL;
				}
				
				if (cda->type == ACT_CD_PLAY_TRACK || cda->type == ACT_CD_LOOP_TRACK) {
					ysize = 48;
					glRects(xco, yco-ysize, xco+width, yco);
					uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
					uiDefButS(block, NUM, 0, "Track:", xco+10,yco-44,width-20, 19, &cda->track, 1, 99, 0, 0, "Select the track to be played");
				}
				else if (cda->type == ACT_CD_VOLUME) {
					ysize = 48;
					glRects(xco, yco-ysize, xco+width, yco);
					uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
					uiDefButF(block, NUM, 0, "Volume:", xco+10,yco-44,width-20, 19, &cda->volume, 0, 1, 0, 0, "Set the volume for CD playback");
				}
				else {
					ysize = 28;
					glRects(xco, yco-ysize, xco+width, yco);
					uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
				}
				uiDefButS(block, MENU, B_REDR, cd_type_str,xco+10,yco-22,width-20, 19, &cda->type, 0.0, 0.0, 0, 0, "");
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
		
 		if(ca->axis==0) ca->axis= 'x';
 		uiDefButS(block, ROW, 0, "X",	xco+10+(width-60)/2, yco-44, 20, 19, &ca->axis, 4.0, (float)'x', 0, 0, "Camera tries to get behind the X axis");
 		uiDefButS(block, ROW, 0, "Y",	xco+30+(width-60)/2, yco-44, 20, 19, &ca->axis, 4.0, (float)'y', 0, 0, "Camera tries to get behind the Y axis");
		
 		uiDefButF(block, NUM, 0, "Max:",	xco+20+(width)/2, yco-44, (width-60)/2, 19, &ca->max, 0.0, 20.0, 0, 0, "");

 		yco-= ysize;
        
         break;
		 		
	case ACT_EDIT_OBJECT:
		
		eoa= act->data;

		if(eoa->type==ACT_EDOB_ADD_OBJECT) {
			int wval; /* just a temp width */
			ysize = 92;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
	 
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+10, yco-44, (width-20)/2, 19, &(eoa->ob), "Add this Object and all its children (cant be on an visible layer)");
			uiDefButI(block, NUM, 0, "Time:",	xco+10+(width-20)/2, yco-44, (width-20)/2, 19, &eoa->time, 0.0, 2000.0, 0, 0, "Duration the new Object lives");

			wval= (width-60)/3;
			uiDefBut(block, LABEL, 0, "linV",	xco,           yco-68,   45, 19,
					 NULL, 0, 0, 0, 0,
					 "Velocity upon creation.");
			uiDefButF(block, NUM, 0, "",		xco+45,        yco-68, wval, 19,
					 eoa->linVelocity, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, x component.");
			uiDefButF(block, NUM, 0, "",		xco+45+wval,   yco-68, wval, 19,
					 eoa->linVelocity+1, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, y component.");
			uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-68, wval, 19,
					 eoa->linVelocity+2, -100.0, 100.0, 10, 0,
					 "Velocity upon creation, z component.");
			uiDefButBitS(block, TOG, ACT_EDOB_LOCAL_LINV, 0, "L", xco+45+3*wval, yco-68, 15, 19,
					 &eoa->localflag, 0.0, 0.0, 0, 0,
					 "Apply the transformation locally");
			
			
			uiDefBut(block, LABEL, 0, "AngV",	xco,           yco-90,   45, 19,
					 NULL, 0, 0, 0, 0,
					 "Angular velocity upon creation.");
			uiDefButF(block, NUM, 0, "",		xco+45,        yco-90, wval, 19,
					 eoa->angVelocity, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, x component.");
			uiDefButF(block, NUM, 0, "",		xco+45+wval,   yco-90, wval, 19,
					 eoa->angVelocity+1, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, y component.");
			uiDefButF(block, NUM, 0, "",		xco+45+2*wval, yco-90, wval, 19,
					 eoa->angVelocity+2, -10000.0, 10000.0, 10, 0,
					 "Angular velocity upon creation, z component.");
			uiDefButBitS(block, TOG, ACT_EDOB_LOCAL_ANGV, 0, "L", xco+45+3*wval, yco-90, 15, 19,
					 &eoa->localflag, 0.0, 0.0, 0, 0,
					 "Apply the rotation locally");
					 

		}
		else if(eoa->type==ACT_EDOB_END_OBJECT) {
			ysize= 28;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		}
		else if(eoa->type==ACT_EDOB_REPLACE_MESH) {
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
	 
			uiDefIDPoinBut(block, test_meshpoin_but, ID_ME, 1, "ME:",		xco+40, yco-44, (width-80), 19, &(eoa->me), "replace the existing mesh with this one");
		}
		else if(eoa->type==ACT_EDOB_TRACK_TO) {
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
	 
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+10, yco-44, (width-20)/2, 19, &(eoa->ob), "Track to this Object");
			uiDefButI(block, NUM, 0, "Time:",	xco+10+(width-20)/2, yco-44, (width-20)/2-40, 19, &eoa->time, 0.0, 2000.0, 0, 0, "Duration the tracking takes");
			uiDefButS(block, TOG, 0, "3D",	xco+width-50, yco-44, 40, 19, &eoa->flag, 0.0, 0.0, 0, 0, "Enable 3D tracking");
		}
		else if(eoa->type==ACT_EDOB_DYNAMICS) {
			ysize= 48;
			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Dynamic Operation %t|Restore Dynamics %x0|Suspend Dynamics %x1|Enable Rigid Body %x2|Disable Rigid Body %x3";
			uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-44, (width-80), 19,  &(eoa->dyn_operation), 0.0, 0.0, 0, 0, "");
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
			coa->flag &= ~(7);
			coa->time = 0;
			uiDefButS(block, MENU, 1, str,		xco+10, yco-65, 70, 19, &coa->flag, 0.0, 0.0, 0, 0, "");
		
			uiDefButS(block, NUM,		0, "damp",	xco+10, yco-45, 70, 19, &coa->damp, 0.0, 100.0, 0, 0, "Damping factor: time constant (in frame) of low pass filter");
			uiDefBut(block, LABEL,			0, "Min",	xco+80, yco-45, (width-90)/2, 19, NULL, 0.0, 0.0, 0, 0, "");
			uiDefBut(block, LABEL,			0, "Max",	xco+80+(width-90)/2, yco-45, (width-90)/2, 19, NULL, 0.0, 0.0, 0, 0, "");

			if(coa->flag & ACT_CONST_LOCX) fp= coa->minloc;
			else if(coa->flag & ACT_CONST_LOCY) fp= coa->minloc+1;
			else if(coa->flag & ACT_CONST_LOCZ) fp= coa->minloc+2;
			else if(coa->flag & ACT_CONST_ROTX) fp= coa->minrot;
			else if(coa->flag & ACT_CONST_ROTY) fp= coa->minrot+1;
			else fp= coa->minrot+2;
			
			uiDefButF(block, NUM, 0, "",		xco+80, yco-65, (width-90)/2, 19, fp, -2000.0, 2000.0, 10, 0, "");
			uiDefButF(block, NUM, 0, "",		xco+80+(width-90)/2, yco-65, (width-90)/2, 19, fp+3, -2000.0, 2000.0, 10, 0, "");
		} else if (coa->type == ACT_CONST_TYPE_DIST) {
			ysize= 106;

			glRects(xco, yco-ysize, xco+width, yco);
			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
			
			str= "Direction %t|None %x0|X axis %x1|Y axis %x2|Z axis %x4|-X axis %x8|-Y axis %x16|-Z axis %x32";
			uiDefButS(block, MENU, B_REDR, str,		xco+10, yco-65, 70, 19, &coa->mode, 0.0, 0.0, 0, 0, "Set the direction of the ray");
		
			uiDefButS(block, NUM,		0, "damp",	xco+10, yco-45, 70, 19, &coa->damp, 0.0, 100.0, 0, 0, "Damping factor: time constant (in frame) of low pass filter");
			uiDefBut(block, LABEL,			0, "Range",	xco+80, yco-45, (width-115)/2, 19, NULL, 0.0, 0.0, 0, 0, "Set the maximum length of ray");
			uiDefButBitS(block, TOG, ACT_CONST_DISTANCE, B_REDR, "Dist",	xco+80+(width-115)/2, yco-45, (width-115)/2, 19, &coa->flag, 0.0, 0.0, 0, 0, "Force distance of object to point of impact of ray");

			if(coa->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp= coa->minloc;
			else if(coa->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp= coa->minloc+1;
			else fp= coa->minloc+2;

			uiDefButF(block, NUM, 0, "",		xco+80, yco-65, (width-115)/2, 19, fp+3, 0.0, 2000.0, 10, 0, "Maximum length of ray");
			if (coa->flag & ACT_CONST_DISTANCE)
				uiDefButF(block, NUM, 0, "",		xco+80+(width-115)/2, yco-65, (width-115)/2, 19, fp, -2000.0, 2000.0, 10, 0, "Keep this distance to target");
			uiDefButBitS(block, TOG, ACT_CONST_NORMAL, 0, "N", xco+80+(width-115), yco-65, 25, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Set object axis along the normal at hit position");
			uiDefButBitS(block, TOG, ACT_CONST_MATERIAL, B_REDR, "M/P", xco+10, yco-84, 40, 19,
					 &coa->flag, 0.0, 0.0, 0, 0, "Detect material instead of property");
			if (coa->flag & ACT_CONST_MATERIAL)
			{
				uiDefBut(block, TEX, 1, "Material:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, 31, 0, 0,
					"Ray detects only Objects with this material");
			}
			else
			{
				uiDefBut(block, TEX, 1, "Property:", xco + 50, yco-84, (width-60), 19,
					coa->matprop, 0, 31, 0, 0,
					"Ray detect only Objects with this property");
			}
			uiDefButBitS(block, TOG, ACT_CONST_PERMANENT, 0, "PER", xco+10, yco-103, 40, 19,
				&coa->flag, 0.0, 0.0, 0, 0, "Persistent actuator: stays active even if ray does not reach target");
			uiDefButS(block, NUM, 0, "time", xco+50, yco-103, (width-60)/2, 19, &(coa->time), 0.0, 1000.0, 0, 0, "Maximum activation time in frame, 0 for unlimited");
			uiDefButS(block, NUM, 0, "rotDamp", xco+50+(width-60)/2, yco-103, (width-60)/2, 19, &(coa->rotdamp), 0.0, 100.0, 0, 0, "Use a different damping for orientation");
		} else if (coa->type == ACT_CONST_TYPE_ORI) {
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
		str= "Constraint Type %t|Location %x0|Distance %x1|Orientation %x2";
		but = uiDefButS(block, MENU, B_REDR, str,		xco+40, yco-23, (width-80), 19, &coa->type, 0.0, 0.0, 0, 0, "");
 		yco-= ysize;
        break;
 
	case ACT_SCENE:
  		sca= act->data; 
		
  		if(sca->type==ACT_SCENE_RESTART) { 
  			ysize= 28; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
  		} 
  		else if(sca->type==ACT_SCENE_CAMERA) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	 
  			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+40, yco-44, (width-80), 19, &(sca->camera), "Set this Camera. Leave empty to refer to self object"); 
  		} 
  		else if(sca->type==ACT_SCENE_SET) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	
  			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Set this Scene"); 
  		} 
		else if(sca->type==ACT_SCENE_ADD_FRONT) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	
  			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Add an Overlay Scene"); 
  		} 
		else if(sca->type==ACT_SCENE_ADD_BACK) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	
  			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Add a Background Scene"); 
  		} 
		else if(sca->type==ACT_SCENE_REMOVE) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	
  			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Remove a Scene");
  		} 
		else if(sca->type==ACT_SCENE_SUSPEND) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	
  			uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 1, "SCE:",		xco+40, yco-44, (width-80), 19, &(sca->scene), "Pause a Scene");
  		} 
		else if(sca->type==ACT_SCENE_RESUME) { 
			
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
			if (gma->type == ACT_GAME_LOAD)
			{
				//ysize = 68;
				ysize = 48;
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
		   		uiDefBut(block, TEX, 1, "File: ", xco+10, yco-44,width-20,19, &(gma->filename), 0, 63, 0, 0, "Load this file");
//				uiDefBut(block, TEX, 1, "Anim: ", xco+10, yco-64,width-20,19, &(gma->loadaniname), 0, 63, 0, 0, "Use this loadinganimation");
			}
/*			else if (gma->type == ACT_GAME_START)
			{
				ysize = 68; 
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);

		   		uiDefBut(block, TEX, 1, "File: ", xco+10, yco-44,width-20,19, &(gma->filename), 0, 63, 0, 0, "Load this file");
				uiDefBut(block, TEX, 1, "Anim: ", xco+10, yco-64,width-20,19, &(gma->loadaniname), 0, 63, 0, 0, "Use this loadinganimation");
			}
*/			else if (gma->type == ACT_GAME_RESTART)
			{
				ysize = 28; 
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
			}
			else if (gma->type == ACT_GAME_QUIT)
			{
				ysize = 28; 
				glRects(xco, yco-ysize, xco+width, yco); 
				uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
			}

			//str = "Scene %t|Load game%x0|Start loaded game%x1|Restart this game%x2|Quit this game %x3";
			str = "Scene %t|Start new game%x0|Restart this game%x2|Quit this game %x3";
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
		if(ga->type==ACT_GROUP_SET) {
			uiDefBut(block, TEX, 0, "Key: ",		xco+20, yco-44, (width-10)/2, 19, ga->name, 0.0, 31.0, 0, 0, "This name defines groupkey to be set");
			uiDefButI(block, NUM, 0, "Frame:",	xco+20+(width-10)/2, yco-44, (width-70)/2, 19, &ga->sta, 0.0, 2500.0, 0, 0, "Set this frame");
		}
		else if(ga->type==ACT_GROUP_FROM_PROP) {
			uiDefBut(block, TEX, 0, "Prop: ",		xco+20, yco-44, width-40, 19, ga->name, 0.0, 31.0, 0, 0, "Use this property to define the Group position");
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

		str= "Visibility %t|Visible %x0|Invisible %x1";

		uiDefButI(block, MENU, B_REDR, str,
			  xco + 10, yco - 24, width - 20, 19, &visAct->flag,
			  0.0, 0.0, 0, 0,
			  "Make the object invisible or visible.");
/*
		uiDefButBitI(block, TOG, ACT_VISIBILITY_INVISIBLE, 0,
			  "Invisible",
			  xco + 10, yco - 24, width - 20, 19, &visAct->flag,
			  0.0, 0.0, 0, 0,
			  "Make the object invisible or visible.");
*/
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
				uiDefButBitI(block, TOG, (1<<(stbit+wval)), 0, "",	(short)(xco+85+12*stbit+13*wval), yco-17, 12, 12, (int *)&(staAct->mask), 0, 0, 0, 0, get_state_name(ob, (short)(wval+stbit)));
			}
			for (stbit=0; stbit<5; stbit++) {
				uiDefButBitI(block, TOG, (1<<(stbit+wval+15)), 0, "",	(short)(xco+85+12*stbit+13*wval), yco-29, 12, 12, (int *)&(staAct->mask), 0, 0, 0, 0, get_state_name(ob, (short)(wval+stbit+15)));
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
				 &randAct->propname, 0, 31, 0, 0,
				 "Assign the random value to this property"); 

		/*4. and 5. arguments for the distribution*/
		switch (randAct->distribution) {
		case ACT_RANDOM_BOOL_CONST:
			uiDefButBitI(block, TOG, 1, 1, "Always true", (xco+10), yco-64, (width-20), 19,
					 &randAct->int_arg_1, 2.0, 1, 0, 0,
					 "Always false or always true");			
			break;
		case ACT_RANDOM_BOOL_UNIFORM:
			uiDefBut(block, LABEL, 0, "     Do a 50-50 pick.",	(xco+10), yco-64, (width-20), 19,
					 NULL, 0, 0, 0, 0,
					 "Choose between true and false, 50% chance each.");
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
					 "Lower boundary of the range.");
			uiDefButI(block, NUM, 1, "Max: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->int_arg_2, -1000, 1000, 0, 0,
					 "Choose a number from a range. "
					 "Upper boundary of the range.");
			break;
		case ACT_RANDOM_INT_POISSON:
			uiDefButF(block, NUM, 1, "Mean: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.01, 100.0, 0, 0,
					 "Expected mean value of the distribution.");						
			break;
		case ACT_RANDOM_FLOAT_CONST:
			uiDefButF(block, NUM, 1, "Value: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.0, 1.0, 0, 0,
					 "Always return this number");
			break;
		case ACT_RANDOM_FLOAT_UNIFORM:
			uiDefButF(block, NUM, 1, "Min: ",		(xco+10), yco-64, (width-20)/2, 19,
					 &randAct->float_arg_1, -10000.0, 10000.0, 0, 0,
					 "Choose a number from a range. "
					 "Lower boundary of the range.");
			uiDefButF(block, NUM, 1, "Max: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->float_arg_2, -10000.0, 10000.0, 0, 0,
					 "Choose a number from a range. "
					 "Upper boundary of the range.");
			break;
		case ACT_RANDOM_FLOAT_NORMAL:
			uiDefButF(block, NUM, 1, "Mean: ",		(xco+10), yco-64, (width-20)/2, 19,
					 &randAct->float_arg_1, -10000.0, 10000.0, 0, 0,
					 "A normal distribution. Mean of the distribution.");
			uiDefButF(block, NUM, 1, "SD: ",		(xco+10) + (width-20)/2, yco-64, (width-20)/2, 19,
					 &randAct->float_arg_2, 0.0, 10000.0, 0, 0,
					 "A normal distribution. Standard deviation of the "
					 "distribution.");
			break;
		case ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL:
			uiDefButF(block, NUM, 1, "Half-life time: ", (xco+10), yco-64, (width-20), 19,
					 &randAct->float_arg_1, 0.001, 10000.0, 0, 0,
					 "Negative exponential dropoff.");
			break;
		default:
			; /* don't know what this distro is... can be useful for testing */
			/* though :)                                                     */
		}

		yco-= ysize;
		break;
	case ACT_MESSAGE:
		ma = act->data;

#define MESSAGE_SENSOR_TO_FIELD_WORKS	/* Really?  Not really.  Don't remove this ifdef yet */

#ifdef MESSAGE_SENSOR_TO_FIELD_WORKS
		ysize = 4 + (3 * 24); /* footer + number of lines * 24 pixels/line */
#else
		ysize = 4 + (2 * 24); /* footer + number of lines * 24 pixels/line */
#endif
		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco,	    (float)yco-ysize,
				 (float)xco+width,  (float)yco, 1);

		myline=1;


#ifdef MESSAGE_SENSOR_TO_FIELD_WORKS
		/* line 1: To */
		uiDefBut(block, TEX, 1, "To: ",
			(xco+10), (yco-(myline++*24)), (width-20), 19,
			&ma->toPropName, 0, 31, 0, 0,
			"Optional send message to objects with this name only (Prefix name with OB)"
			", or empty to broadcast");

#endif

		/* line 2: Message Subject */
		uiDefBut(block, TEX, 1, "Subject: ",
		(xco+10), (yco-(myline++*24)), (width-20), 19,
		&ma->subject, 0, 31, 0, 0,
		"Optional message subject. This is what can be filtered on.");

		/* line 3: Text/Property */
		uiDefButBitS(block, TOG, 1, B_REDR, "T/P",
			(xco+10),(yco-(myline*24)), (0.20 * (width-20)), 19,
			&ma->bodyType, 0.0, 0.0, 0, 0,
			"Toggle message type: either Text or a PropertyName.");

		if (ma->bodyType == ACT_MESG_MESG)
		{
    		/* line 3: Message Body */
    		uiDefBut(block, TEX, 1, "Body: ",
    		(xco+10+(0.20*(width-20))),(yco-(myline++*24)),(0.8*(width-20)),19,
    		&ma->body, 0, 31, 0, 0,
    		"Optional message body Text");
		} else
		{
			/* line 3: Property body (set by property) */
			uiDefBut(block, TEX, 1, "Propname: ",
    		(xco+10+(0.20*(width-20))),(yco-(myline++*24)),(0.8*(width-20)),19,
			&ma->body, 0, 31, 0, 0,
			"The message body will be set by the Property Value");
		}
		
		yco -= ysize;
		break;
	case ACT_2DFILTER:
		tdfa = act->data;

		ysize = 50;
		if(tdfa->type == ACT_2DFILTER_CUSTOMFILTER)
		{
			ysize +=20;
		}
        glRects( xco, yco-ysize, xco+width, yco ); 
		uiEmboss( (float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1 );

		switch(tdfa->type)
		{
			case ACT_2DFILTER_MOTIONBLUR:
				if(!tdfa->flag)
				{
					uiDefButS(block, TOG, B_REDR, "D",	xco+30,yco-44,19, 19, &tdfa->flag, 0.0, 0.0, 0.0, 0.0, "Disable Motion Blur");
					uiDefButF(block, NUM, B_REDR, "Value:", xco+52,yco-44,width-82,19,&tdfa->float_arg,0.0,1.0,0.0,0.0,"Set motion blur value");
				}
				else
				{
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
				uiDefButI(block, NUM, B_REDR, "Pass Number:", xco+30,yco-44,width-60,19,&tdfa->int_arg,0.0,MAX_RENDER_PASS-1,0.0,0.0,"Set motion blur value");
				break;
			case ACT_2DFILTER_CUSTOMFILTER:
				uiDefButI(block, NUM, B_REDR, "Pass Number:", xco+30,yco-44,width-60,19,&tdfa->int_arg,0.0,MAX_RENDER_PASS-1,0.0,0.0,"Set motion blur value");
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

  		if(parAct->type==ACT_PARENT_SET) { 
			
  			ysize= 48; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
	 
  			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, 1, "OB:",		xco+40, yco-44, (width-80), 19, &(parAct->ob), "Set this object as parent"); 
  		}
  		else if(parAct->type==ACT_PARENT_REMOVE) { 
			
  			ysize= 28; 
  			glRects(xco, yco-ysize, xco+width, yco); 
  			uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1); 
 		}

		str= "Parent %t|Set Parent %x0|Remove Parent %x1";
		uiDefButI(block, MENU, B_REDR, str,		xco+40, yco-24, (width-80), 19, &parAct->type, 0.0, 0.0, 0, 0, ""); 

  		yco-= ysize; 
  		break; 
 	default:
		ysize= 4;

		glRects(xco, yco-ysize, xco+width, yco);
		uiEmboss((float)xco, (float)yco-ysize, (float)xco+width, (float)yco, 1);
		
		yco-= ysize;
		break;
	}

	uiBlockSetEmboss(block, UI_EMBOSSM);

	return yco-4;
}

static void do_sensor_menu(void *arg, int event)
{	
	ID **idar;
	Object *ob;
	bSensor *sens;
	short count, a;
	
	idar= get_selected_and_linked_obs(&count, G.buts->scaflag);
	
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if(event==0 || event==2) ob->scaflag |= OB_SHOWSENS;
		else if(event==1) ob->scaflag &= ~OB_SHOWSENS;
	}
		
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		sens= ob->sensors.first;
		while(sens) {
			if(event==2) sens->flag |= SENS_SHOW;
			else if(event==3) sens->flag &= ~SENS_SHOW;
			sens= sens->next;
		}
	}

	if(idar) MEM_freeN(idar);
	allqueue(REDRAWBUTSLOGIC, 0);
}

static uiBlock *sensor_menu(void *arg_unused)
{
	uiBlock *block;
	int yco=0;
	
	block= uiNewBlock(&curarea->uiblocks, "filemenu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, do_sensor_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",	0, (short)(yco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Sensors",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Sensors",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static void do_controller_menu(void *arg, int event)
{	
	ID **idar;
	Object *ob;
	bController *cont;
	short count, a;
	
	idar= get_selected_and_linked_obs(&count, G.buts->scaflag);
	
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if(event==0 || event==2) ob->scaflag |= OB_SHOWCONT;
		else if(event==1) ob->scaflag &= ~OB_SHOWCONT;
	}

	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		cont= ob->controllers.first;
		while(cont) {
			if(event==2) cont->flag |= CONT_SHOW;
			else if(event==3) cont->flag &= ~CONT_SHOW;
			cont= cont->next;
		}
	}

	if(idar) MEM_freeN(idar);
	allqueue(REDRAWBUTSLOGIC, 0);
}

static uiBlock *controller_menu(void *arg_unused)
{
	uiBlock *block;
	int yco=0;
	
	block= uiNewBlock(&curarea->uiblocks, "filemenu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, do_controller_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0,(short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",					0, (short)(yco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Controllers",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 2, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Controllers",	0, (short)(yco-=20), 160, 19, NULL, 0.0, 0.0, 3, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}

static void do_actuator_menu(void *arg, int event)
{	
	ID **idar;
	Object *ob;
	bActuator *act;
	short count, a;
	
	idar= get_selected_and_linked_obs(&count, G.buts->scaflag);
	
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		if(event==0 || event==2) ob->scaflag |= OB_SHOWACT;
		else if(event==1) ob->scaflag &= ~OB_SHOWACT;
	}

	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		act= ob->actuators.first;
		while(act) {
			if(event==2) act->flag |= ACT_SHOW;
			else if(event==3) act->flag &= ~ACT_SHOW;
			act= act->next;
		}
	}

	if(idar) MEM_freeN(idar);
	allqueue(REDRAWBUTSLOGIC, 0);
}

static uiBlock *actuator_menu(void *arg_unused)
{
	uiBlock *block;
	int xco=0;
	
	block= uiNewBlock(&curarea->uiblocks, "filemenu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, do_actuator_menu, NULL);
	
	uiDefBut(block, BUTM, 1, "Show Objects",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Hide Objects",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",	0, (short)(xco-=6), 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Show Actuators",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefBut(block, BUTM, 1, "Hide Actuators",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiBlockSetDirection(block, UI_TOP);
	
	return block;
}


void buttons_enji(uiBlock *block, Object *ob)
{
	uiDefButBitI(block, TOG, OB_SECTOR, B_SETSECTOR, "Sector",
			 10,205,65,19, &ob->gameflag, 0, 0, 0, 0, 
			 "All game elements should be in the Sector boundbox");
	uiDefButBitI(block, TOG, OB_PROP, B_SETPROP, "Prop",
			 75,205,65,19, &ob->gameflag, 0, 0, 0, 0, 
			 "An Object fixed within a sector");
	uiBlockSetCol(block, BUTPURPLE);
	uiDefButBitI(block, TOG, OB_ACTOR, B_SETACTOR, "Actor",
			 140,205,65,19, &ob->gameflag, 0, 0, 0, 0, 
			 "Objects that are evaluated by the engine ");
	if(ob->gameflag & OB_ACTOR) {	
		uiDefButBitI(block, TOG, OB_DYNAMIC, B_SETDYNA, "Dynamic",
				 205,205,75,19, &ob->gameflag, 0, 0, 0, 0, 
				 "Motion defined by laws of physics");
		uiDefButBitI(block, TOG, OB_MAINACTOR, B_SETMAINACTOR, "MainActor",
				 280,205,70,19, &ob->gameflag, 0, 0, 0, 0, "");
		
		if(ob->gameflag & OB_DYNAMIC) {
			uiDefButBitI(block, TOG, OB_DO_FH, B_DIFF, "Do Fh",
					 10,185,50,19, &ob->gameflag, 0, 0, 0, 0, 
					 "Use Fh settings in Materials");
			uiDefButBitI(block, TOG, OB_ROT_FH, B_DIFF, "Rot Fh",
					 60,185,50,19, &ob->gameflag, 0, 0, 0, 0, 
					 "Use face normal to rotate Object");
		
			uiBlockSetCol(block, BUTGREY);
			uiDefButF(block, NUM, B_DIFF, "Mass:",
					 110, 185, 120, 19, &ob->mass, 0.01, 100.0, 10, 0, 
					 "The mass of the Object");
			uiDefButF(block, NUM, REDRAWVIEW3D, "Size:",
					 230, 185, 120, 19, &ob->inertia, 0.01, 10.0, 10, 0, 
					 "Bounding sphere size");
			uiDefButF(block, NUM, B_DIFF, "Damp:",
					 10, 165, 100, 19, &ob->damping, 0.0, 1.0, 10, 0, 
					 "General movement damping");
			uiDefButF(block, NUM, B_DIFF, "RotDamp:",
					 110, 165, 120, 19, &ob->rdamping, 0.0, 1.0, 10, 0, 
					 "General rotation damping");
		}
	}

}

void buttons_ketsji(uiBlock *block, Object *ob)
{
	uiDefButBitI(block, TOG, OB_ACTOR, B_REDR, "Actor",
			  10,205,55,19, &ob->gameflag, 0, 0, 0, 0,
			  "Objects that are evaluated by the engine ");
	if(ob->gameflag & OB_ACTOR) {	
		uiDefButBitI(block, TOG, OB_GHOST, B_REDR, "Ghost", 65,205,55,19, 
				  &ob->gameflag, 0, 0, 0, 0, 
				  "Objects that don't restitute collisions (like a ghost)");
		uiDefButBitI(block, TOG, OB_DYNAMIC, B_REDR, "Dynamic", 120,205,70,19, 
				  &ob->gameflag, 0, 0, 0, 0, 
				  "Motion defined by laws of physics");
	
		if(ob->gameflag & OB_DYNAMIC) {
			uiDefButBitI(block, TOG, OB_RIGID_BODY, B_REDR, "Rigid Body", 190,205,80,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Enable rolling physics");
			uiDefButBitI(block, TOG, OB_COLLISION_RESPONSE, B_REDR, "No sleeping", 270,205,80,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Disable auto (de)activation");

			uiDefButBitI(block, TOG, OB_DO_FH, B_DIFF, "Do Fh", 10,185,50,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Use Fh settings in Materials");
			uiDefButBitI(block, TOG, OB_ROT_FH, B_DIFF, "Rot Fh", 60,185,50,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Use face normal to rotate Object");
			uiDefButF(block, NUM, B_DIFF, "Mass:", 110, 185, 80, 19, 
					  &ob->mass, 0.01, 10000.0, 10, 0, 
					  "The mass of the Object");
			uiDefButF(block, NUM, REDRAWVIEW3D, "Radius:", 190, 185, 80, 19, 
					  &ob->inertia, 0.01, 10.0, 10, 0, 
					  "Bounding sphere radius");
			uiDefButF(block, NUM, B_DIFF, "Form:", 270, 185, 80, 19, 
					  &ob->formfactor, 0.01, 100.0, 10, 0, 
					  "Form factor");

			uiDefButF(block, NUM, B_DIFF, "Damp:", 10, 165, 100, 19, 
					  &ob->damping, 0.0, 1.0, 10, 0, 
					  "General movement damping");
			uiDefButF(block, NUM, B_DIFF, "RotDamp:", 110, 165, 120, 19, 
					  &ob->rdamping, 0.0, 1.0, 10, 0, 
					  "General rotation damping");
			uiDefButBitI(block, TOG, OB_ANISOTROPIC_FRICTION, B_REDR, "Anisotropic", 
					  230, 165, 120, 19,
					  &ob->gameflag, 0.0, 1.0, 10, 0,
					  "Enable anisotropic friction");			
		}

		if (ob->gameflag & OB_ANISOTROPIC_FRICTION) {
			uiDefButF(block, NUM, B_DIFF, "x friction:", 10, 145, 114, 19,
					  &ob->anisotropicFriction[0], 0.0, 1.0, 10, 0,
					  "Relative friction coefficient in the x-direction.");
			uiDefButF(block, NUM, B_DIFF, "y friction:", 124, 145, 113, 19,
					  &ob->anisotropicFriction[1], 0.0, 1.0, 10, 0,
					  "Relative friction coefficient in the y-direction.");
			uiDefButF(block, NUM, B_DIFF, "z friction:", 237, 145, 113, 19,
					  &ob->anisotropicFriction[2], 0.0, 1.0, 10, 0,
					  "Relative friction coefficient in the z-direction.");
		}
	}

	if (!(ob->gameflag & OB_GHOST)) {
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, OB_BOUNDS, B_REDR, "Bounds", 10, 125, 75, 19,
				&ob->gameflag, 0, 0,0, 0,
				"Specify a bounds object for physics");
		if (ob->gameflag & OB_BOUNDS) {
			uiDefButS(block, MENU, REDRAWVIEW3D, "Boundary Display%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Convex Hull Polytope%x5|Static TriangleMesh %x4",
				85, 125, 160, 19, &ob->boundtype, 0, 0, 0, 0, "Selects the collision type");
			uiDefButBitI(block, TOG, OB_CHILD, B_REDR, "Compound", 250,125,100,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Add Children");
		}
		uiBlockEndAlign(block);
	}
}

void buttons_bullet(uiBlock *block, Object *ob)
{
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, OB_ACTOR, B_REDR, "Actor",
			  10,205,55,19, &ob->gameflag, 0, 0, 0, 0,
			  "Objects that are evaluated by the engine ");
	if(ob->gameflag & OB_ACTOR) {	
		uiDefButBitI(block, TOG, OB_GHOST, B_REDR, "Ghost", 65,205,55,19, 
				  &ob->gameflag, 0, 0, 0, 0, 
				  "Objects that don't restitute collisions (like a ghost)");
		uiDefButBitI(block, TOG, OB_DYNAMIC, B_REDR, "Dynamic", 120,205,70,19, 
				  &ob->gameflag, 0, 0, 0, 0, 
				  "Motion defined by laws of physics");
	
		if(ob->gameflag & OB_DYNAMIC) {
			uiDefButBitI(block, TOG, OB_RIGID_BODY, B_REDR, "Rigid Body", 190,205,80,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Enable rolling physics");
			uiDefButBitI(block, TOG, OB_COLLISION_RESPONSE, B_REDR, "No sleeping", 270,205,80,19, 
					  &ob->gameflag, 0, 0, 0, 0, 
					  "Disable auto (de)activation");

			uiDefButF(block, NUM, B_DIFF, "Mass:", 10, 185, 170, 19, 
					  &ob->mass, 0.01, 10000.0, 10, 2, 
					  "The mass of the Object");
			uiDefButF(block, NUM, REDRAWVIEW3D, "Radius:", 180, 185, 170, 19, 
					  &ob->inertia, 0.01, 10.0, 10, 2, 
					  "Bounding sphere radius");

			uiDefButF(block, NUMSLI, B_DIFF, "Damp ", 10, 165, 150, 19, 
					  &ob->damping, 0.0, 1.0, 10, 0, 
					  "General movement damping");
			uiDefButF(block, NUMSLI, B_DIFF, "RotDamp ", 160, 165, 190, 19, 
					  &ob->rdamping, 0.0, 1.0, 10, 0, 
					  "General rotation damping");
		}
	}
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, OB_BOUNDS, B_REDR, "Bounds", 10, 125, 75, 19,
		     &ob->gameflag, 0, 0,0, 0,
		     "Specify a bounds object for physics");
	if (ob->gameflag & OB_BOUNDS) {
		uiDefButS(block, MENU, REDRAWVIEW3D, "Boundary Display%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Convex Hull Polytope%x5|Static TriangleMesh %x4",
		  //almost ready to enable this one:			uiDefButS(block, MENU, REDRAWVIEW3D, "Boundary Display%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Convex Hull Polytope%x5|Static TriangleMesh %x4|Dynamic Mesh %x5|",
			  85, 125, 160, 19, &ob->boundtype, 0, 0, 0, 0, "Selects the collision type");
		uiDefButBitI(block, TOG, OB_CHILD, B_REDR, "Compound", 250,125,100,19, 
			     &ob->gameflag, 0, 0, 0, 0, 
			     "Add Children");
	}
	uiBlockEndAlign(block);
}

static void check_object_state(void *arg1_but, void *arg2_mask)
{
	unsigned int *cont_mask = arg2_mask;
	uiBut *but = arg1_but;

	if (*cont_mask == 0 || !(G.qual & LR_SHIFTKEY))
		*cont_mask = (1<<but->retval);
	but->retval = B_REDR;
}

static void check_controller_state_mask(void *arg1_but, void *arg2_mask)
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

static uiBlock *controller_state_mask_menu(void *arg_cont)
{
	uiBlock *block;
	uiBut *but;
	bController *cont = arg_cont;

	short yco = 12, xco = 0, stbit, offset;

	block= uiNewBlock(&curarea->uiblocks, "Controller state mask", UI_EMBOSS, UI_HELV, curarea->win);

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

	return block;
}

static void do_object_state_menu(void *arg, int event)
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
	allqueue(REDRAWBUTSLOGIC, 0);
}

static uiBlock *object_state_mask_menu(void *arg_obj)
{
	uiBlock *block;
	short xco = 0;

	block= uiNewBlock(&curarea->uiblocks, "obstatemenu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, do_object_state_menu, arg_obj);
	
	uiDefBut(block, BUTM, 1, "Set all bits",		0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Recall init state",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, SEPR, 0, "",					0, (short)(xco-=6),	 160, 6,  NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Store init state",	0, (short)(xco-=20), 160, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_TOP);
	return block;
}

static int is_sensor_linked(uiBlock *block, bSensor *sens)
{
	bController *cont;
	int i, count;

	for (count=0, i=0; i<sens->totlinks; i++) {
		cont = sens->links[i];
		if (uiFindInlink(block, cont) != NULL)
			return 1;
	}
	return 0;
}

/* never used, see CVS 1.134 for the code */
/*  static FreeCamera *new_freecamera(void) */

/* never used, see CVS 1.120 for the code */
/*  static uiBlock *freecamera_menu(void) */


void logic_buts(void)
{
	ID **idar;
	Object *ob;
	bProperty *prop;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	uiBlock *block;
	uiBut *but;
	World *wrld;
	int a, iact, stbit, offset;
	short xco, yco, count, width, ycoo;
	char *pupstr, name[32];

	wrld= G.scene->world;

	ob= OBACT;

	if(ob==0) return;
	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	sprintf(name, "buttonswin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, name, UI_EMBOSS, UI_HELV, curarea->win);
	
	uiBlockSetCol(block, TH_BUT_SETTING2);

	if(wrld) {
		switch(wrld->physicsEngine) {
		case WOPHY_ENJI:
			buttons_enji(block, ob);
			break;
		case WOPHY_BULLET:
			buttons_bullet(block, ob);
			break;
		default:
			buttons_ketsji(block, ob);
			break;
		}
	}
	else buttons_ketsji(block, ob);
	
	uiBlockSetCol(block, TH_AUTO);
	uiBlockBeginAlign(block);
	uiDefBut(block, BUT, B_ADD_PROP, "Add Property",		10, 90, 340, 24,
			 NULL, 0.0, 100.0, 100, 0,
			 "");
	
	pupstr= "Types %t|Bool %x0|Int %x1|Float %x2|String %x3|Timer %x5";
	
	a= 0;
	prop= ob->prop.first;
	while(prop) {
		but= uiDefBut(block, BUT, 1, "Del",		10, (short)(70-20*a), 40, 20, NULL, 0.0, 0.0, 1, (float)a, "");
		uiButSetFunc(but, del_property, prop, NULL);
		uiDefButS(block, MENU, B_CHANGE_PROP, pupstr,		50, (short)(70-20*a), 60, 20, &prop->type, 0, 0, 0, 0, "");
		but= uiDefBut(block, TEX, 1, "Name:",					110, (short)(70-20*a), 110, 20, prop->name, 0, 31, 0, 0, "");
		uiButSetFunc(but, make_unique_prop_names_cb, prop->name, (void*) 1);
		
		if(prop->type==PROP_BOOL) {
			uiDefButBitI(block, TOG, 1, B_REDR, "True",		220, (short)(70-20*a), 55, 20, &prop->data, 0, 0, 0, 0, "");
			uiDefButBitI(block, TOGN, 1, B_REDR, "False",	270, (short)(70-20*a), 55, 20, &prop->data, 0, 0, 0, 0, "");
		}
		else if(prop->type==PROP_INT) 
			uiDefButI(block, NUM, B_REDR, "",			220, (short)(70-20*a), 110, 20, &prop->data, -10000, 10000, 0, 0, "");
		else if(prop->type==PROP_FLOAT) 
			uiDefButF(block, NUM, B_REDR, "",			220, (short)(70-20*a), 110, 20, (float*) &prop->data, -10000, 10000, 100, 3, "");
		else if(prop->type==PROP_STRING) 
			uiDefBut(block, TEX, B_REDR, "",				220, (short)(70-20*a), 110, 20, prop->poin, 0, 127, 0, 0, "");
		else if(prop->type==PROP_TIME) 
			uiDefButF(block, NUM, B_REDR, "",			220, (short)(70-20*a), 110, 20, (float*) &prop->data, -10000, 10000, 100, 3, "");
		
		uiDefButBitS(block, TOG, PROP_DEBUG, B_REDR, "D",		330, (short)(70-20*a), 20, 20, &prop->flag, 0, 0, 0, 0, "Print Debug info");
		
		a++;
		prop= prop->next;
		
	}
	uiBlockEndAlign(block);

	uiClearButLock();

	idar= get_selected_and_linked_obs(&count, G.buts->scaflag);

	/* clean ACT_LINKED and ACT_VISIBLE of all potentially visible actuators so that 
	   we can determine which is actually linked/visible */
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		act= ob->actuators.first;
		while(act) {
			act->flag &= ~(ACT_LINKED|ACT_VISIBLE);
			act = act->next;
		}
		/* same for sensors */
		sens= ob->sensors.first;
		while(sens) {
			sens->flag &= ~(SENS_VISIBLE);
			sens = sens->next;
		}
	}
		
	/* start with the controller because we need to know which one is visible */
	/* ******************************* */
	xco= 695; yco= 170; width= 275;

	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiDefBlockBut(block, controller_menu, NULL, "Controllers", xco-10, yco+35, 100, 19, "");
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_CONT_SEL,  B_REDR, "Sel", xco+110, yco+35, (width-100)/3, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_CONT_ACT, B_REDR, "Act", xco+110+(width-100)/3, yco+35, (width-100)/3, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_CONT_LINK, B_REDR, "Link", xco+110+2*(width-100)/3, yco+35, (width-100)/3, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show linked Objects to Sensor/Actuator");
	uiBlockEndAlign(block);
	
	ob= OBACT;
	
	for(a=0; a<count; a++) {
		unsigned int controller_state_mask = 0; /* store a bitmask for states that are used */
		
		ob= (Object *)idar[a];
		uiClearButLock();
		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		if( (ob->scavisflag & OB_VIS_CONT) == 0) continue;

		/* presume it is only objects for now */
		uiBlockSetEmboss(block, UI_EMBOSS);
		uiBlockBeginAlign(block);
		if(ob->controllers.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWCONT, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), 19, &ob->scaflag, 0, 0, 0, 0, "Active Object name");
		if(ob->controllers.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDCONT, B_ADD_CONT, "Add",(short)(xco+width-40), yco, 50, 19, &ob->scaflag, 0, 0, 0, 0, "Add a new Controller");
		uiBlockEndAlign(block);
		yco-=17;
		
		/* mark all actuators linked to these controllers */
		/* note that some of these actuators could be from objects that are not in the display list.
		   It's ok because those actuators will not be displayed here */
		cont= ob->controllers.first;
		while(cont) {
			for (iact=0; iact<cont->totlinks; iact++) {
				act = cont->links[iact];
				if (act)
					act->flag |= ACT_LINKED;
			}
			controller_state_mask |= cont->state_mask;
			cont = cont->next;
		}

		if(ob->scaflag & OB_SHOWCONT) {

			/* first show the state */
			uiBlockSetEmboss(block, UI_EMBOSSP);
			uiDefBlockBut(block, object_state_mask_menu, ob, "State", (short)(xco-10), (short)(yco-10), 40, 19, "Object state menu: store and retrieve initial state");
			uiBlockSetEmboss(block, UI_EMBOSS);
			if (!ob->state)
				ob->state = 1;
			for (offset=0; offset<15; offset+=5) {
				uiBlockBeginAlign(block);
				for (stbit=0; stbit<5; stbit++) {
					but = uiDefButBitI(block, controller_state_mask&(1<<(stbit+offset)) ? BUT_TOGDUAL:TOG, 1<<(stbit+offset), stbit+offset, "",	(short)(xco+35+12*stbit+13*offset), yco, 12, 12, (int *)&(ob->state), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+offset)));
					uiButSetFunc(but, check_object_state, but, &(ob->state));
				}
				for (stbit=0; stbit<5; stbit++) {
					but = uiDefButBitI(block, controller_state_mask&(1<<(stbit+offset+15)) ? BUT_TOGDUAL:TOG, 1<<(stbit+offset+15), stbit+offset+15, "",	(short)(xco+35+12*stbit+13*offset), yco-12, 12, 12, (int *)&(ob->state), 0, 0, 0, 0, get_state_name(ob, (short)(stbit+offset+15)));
					uiButSetFunc(but, check_object_state, but, &(ob->state));
				}
			}
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, OB_SETSTBIT, B_SET_STATE_BIT, "All",(short)(xco+235), yco-10, 25, 19, &ob->scaflag, 0, 0, 0, 0, "Set all state bits");
			uiDefButBitS(block, TOG, OB_INITSTBIT, B_INIT_STATE_BIT, "Ini",(short)(xco+260), yco-10, 25, 19, &ob->scaflag, 0, 0, 0, 0, "Set the initial state");
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
				while(cont) {
					if (cont->state_mask & (1<<stbit)) {
						/* this controller is visible, mark all its actuator */
						for (iact=0; iact<cont->totlinks; iact++) {
							act = cont->links[iact];
							if (act)
								act->flag |= ACT_VISIBLE;
						}
						uiBlockSetEmboss(block, UI_EMBOSSM);
						uiDefIconButBitS(block, TOG, CONT_DEL, B_DEL_CONT, ICON_X,	xco, yco, 22, 19, &cont->flag, 0, 0, 0, 0, "Delete Controller");
						uiDefIconButBitS(block, ICONTOG, CONT_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, 19, &cont->flag, 0, 0, 0, 0, "Controller settings");
						uiBlockSetEmboss(block, UI_EMBOSSP);
						sprintf(name, "%d", first_bit(cont->state_mask)+1);
						uiDefBlockBut(block, controller_state_mask_menu, cont, name, (short)(xco+width-44), yco, 22, 19, "Set controller state mask");
						uiBlockSetEmboss(block, UI_EMBOSSM);
				
						if(cont->flag & CONT_SHOW) {
							cont->otype= cont->type;
							uiDefButS(block, MENU, B_CHANGE_CONT, controller_pup(),(short)(xco+22), yco, 100, 19, &cont->type, 0, 0, 0, 0, "Controller type");
							but= uiDefBut(block, TEX, 1, "", (short)(xco+122), yco, (short)(width-166), 19, cont->name, 0, 31, 0, 0, "Controller name");
							uiButSetFunc(but, make_unique_prop_names_cb, cont->name, (void*) 0);
				
							ycoo= yco;
							yco= draw_controllerbuttons(cont, block, xco, yco, width);
							if(yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
						}
						else {
							cpack(0x999999);
							glRecti(xco+22, yco, xco+width-22,yco+19);
							but= uiDefBut(block, LABEL, 0, controller_name(cont->type), (short)(xco+22), yco, 100, 19, cont, 0, 0, 0, 0, "Controller type");
							uiButSetFunc(but, sca_move_controller, cont, NULL);
							but= uiDefBut(block, LABEL, 0, cont->name,(short)(xco+122), yco,(short)(width-166), 19, cont, 0, 0, 0, 0, "Controller name");
							uiButSetFunc(but, sca_move_controller, cont, NULL);
							ycoo= yco;
						}
				
						but= uiDefIconBut(block, LINK, 0, ICON_LINK,	(short)(xco+width), ycoo, 19, 19, NULL, 0, 0, 0, 0, "");
						uiSetButLink(but, NULL, (void ***)&(cont->links), &cont->totlinks, LINK_CONTROLLER, LINK_ACTUATOR);
				
						uiDefIconBut(block, INLINK, 0, ICON_INLINK,(short)(xco-19), ycoo, 19, 19, cont, LINK_CONTROLLER, 0, 0, 0, "");
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
	xco= 375; yco= 170; width= 250;

	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiDefBlockBut(block, sensor_menu, NULL, "Sensors", xco-10, yco+35, 70, 19, "");
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_SENS_SEL, B_REDR, "Sel", xco+80, yco+35, (width-70)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_SENS_ACT, B_REDR, "Act", xco+80+(width-70)/4, yco+35, (width-70)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_SENS_LINK, B_REDR, "Link", xco+80+2*(width-70)/4, yco+35, (width-70)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show linked Objects to Controller");
	uiDefButBitS(block, TOG, BUTS_SENS_STATE, B_REDR, "State", xco+80+3*(width-70)/4, yco+35, (width-70)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show only sensors connected to active states");
	uiBlockEndAlign(block);
	
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		uiClearButLock();
		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		
		if( (ob->scavisflag & OB_VIS_SENS) == 0) continue;
		
		/* presume it is only objects for now */
		uiBlockSetEmboss(block, UI_EMBOSS);
		uiBlockBeginAlign(block);
		if(ob->sensors.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWSENS, B_REDR, ob->id.name+2,(short)(xco-10), yco, (short)(width-30), 19, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide sensors");
		if(ob->sensors.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDSENS, B_ADD_SENS, "Add",(short)(xco+width-40), yco, 50, 19, &ob->scaflag, 0, 0, 0, 0, "Add a new Sensor");
		uiBlockEndAlign(block);
		yco-=20;
		
		if(ob->scaflag & OB_SHOWSENS) {
			
			sens= ob->sensors.first;
			while(sens) {
				if (!(G.buts->scaflag & BUTS_SENS_STATE) ||
					sens->totlinks == 0 ||		/* always display sensor without links so that is can be edited */
					is_sensor_linked(block, sens)) {
					sens->flag |= SENS_VISIBLE;
					uiBlockSetEmboss(block, UI_EMBOSSM);
					uiDefIconButBitS(block, TOG, SENS_DEL, B_DEL_SENS, ICON_X,	xco, yco, 22, 19, &sens->flag, 0, 0, 0, 0, "Delete Sensor");
					uiDefIconButBitS(block, ICONTOG, SENS_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, 19, &sens->flag, 0, 0, 0, 0, "Sensor settings");

					ycoo= yco;
					if(sens->flag & SENS_SHOW)
					{
						uiDefButS(block, MENU, B_CHANGE_SENS, sensor_pup(),	(short)(xco+22), yco, 80, 19, &sens->type, 0, 0, 0, 0, "Sensor type");
						but= uiDefBut(block, TEX, 1, "", (short)(xco+102), yco, (short)(width-124), 19, sens->name, 0, 31, 0, 0, "Sensor name");
						uiButSetFunc(but, make_unique_prop_names_cb, sens->name, (void*) 0);

						sens->otype= sens->type;
						yco= draw_sensorbuttons(sens, block, xco, yco, width,ob->id.name);
						if(yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
					}
					else {
						set_col_sensor(sens->type, 1);
						glRecti(xco+22, yco, xco+width-22,yco+19);
						but= uiDefBut(block, LABEL, 0, sensor_name(sens->type),	(short)(xco+22), yco, 80, 19, sens, 0, 0, 0, 0, "");
						uiButSetFunc(but, sca_move_sensor, sens, NULL);
						but= uiDefBut(block, LABEL, 0, sens->name, (short)(xco+102), yco, (short)(width-124), 19, sens, 0, 31, 0, 0, "");
						uiButSetFunc(but, sca_move_sensor, sens, NULL);
					}

					but= uiDefIconBut(block, LINK, 0, ICON_LINK,	(short)(xco+width), ycoo, 19, 19, NULL, 0, 0, 0, 0, "");
					uiSetButLink(but, NULL, (void ***)&(sens->links), &sens->totlinks, LINK_SENSOR, LINK_CONTROLLER);

					yco-=20;
				}
				sens= sens->next;
			}
			yco-= 6;
		}
	}

	/* ******************************* */
	xco= 1040; yco= 170; width= 280;
	
	uiBlockSetEmboss(block, UI_EMBOSSP);
	uiDefBlockBut(block, actuator_menu, NULL, "Actuators", xco-10, yco+35, 90, 19, "");
	uiBlockSetEmboss(block, UI_EMBOSS);
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, BUTS_ACT_SEL, B_REDR, "Sel", xco+110, yco+35, (width-100)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show all selected Objects");
	uiDefButBitS(block, TOG, BUTS_ACT_ACT, B_REDR, "Act", xco+110+(width-100)/4, yco+35, (width-100)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show active Object");
	uiDefButBitS(block, TOG, BUTS_ACT_LINK, B_REDR, "Link", xco+110+2*(width-100)/4, yco+35, (width-100)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show linked Objects to Controller");
	uiDefButBitS(block, TOG, BUTS_ACT_STATE, B_REDR, "State", xco+110+3*(width-100)/4, yco+35, (width-100)/4, 19, &G.buts->scaflag, 0, 0, 0, 0, "Show only actuators connected to active states");
	uiBlockEndAlign(block);
	for(a=0; a<count; a++) {
		ob= (Object *)idar[a];
		uiClearButLock();
		uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
		if( (ob->scavisflag & OB_VIS_ACT) == 0) continue;

		/* presume it is only objects for now */
		uiBlockSetEmboss(block, UI_EMBOSS);
		uiBlockBeginAlign(block);
		if(ob->actuators.first) uiSetCurFont(block, UI_HELVB);
		uiDefButBitS(block, TOG, OB_SHOWACT, B_REDR, ob->id.name+2,(short)(xco-10), yco,(short)(width-30), 19, &ob->scaflag, 0, 31, 0, 0, "Object name, click to show/hide actuators");
		if(ob->actuators.first) uiSetCurFont(block, UI_HELV);
		uiDefButBitS(block, TOG, OB_ADDACT, B_ADD_ACT, "Add",(short)(xco+width-40), yco, 50, 19, &ob->scaflag, 0, 0, 0, 0, "Add a new Actuator");
		uiBlockEndAlign(block);
		yco-=20;
		
		if(ob->scaflag & OB_SHOWACT) {
			
			act= ob->actuators.first;
			while(act) {
				if (!(G.buts->scaflag & BUTS_ACT_STATE) ||
					!(act->flag & ACT_LINKED) ||		/* always display actuators without links so that is can be edited */
					(act->flag & ACT_VISIBLE)) {		/* this actuator has visible connection, display it */
					act->flag |= ACT_VISIBLE;	/* mark the actuator as visible to help implementing the up/down action */
					uiBlockSetEmboss(block, UI_EMBOSSM);
					uiDefIconButBitS(block, TOG, ACT_DEL, B_DEL_ACT, ICON_X,	xco, yco, 22, 19, &act->flag, 0, 0, 0, 0, "Delete Actuator");
					uiDefIconButBitS(block, ICONTOG, ACT_SHOW, B_REDR, ICON_RIGHTARROW, (short)(xco+width-22), yco, 22, 19, &act->flag, 0, 0, 0, 0, "Actuator settings");

					if(act->flag & ACT_SHOW) {
						act->otype= act->type;
						uiDefButS(block, MENU, B_CHANGE_ACT, actuator_pup(ob),	(short)(xco+22), yco, 90, 19, &act->type, 0, 0, 0, 0, "Actuator type");
						but= uiDefBut(block, TEX, 1, "", (short)(xco+112), yco, (short)(width-134), 19, act->name, 0, 31, 0, 0, "Actuator name");
						uiButSetFunc(but, make_unique_prop_names_cb, act->name, (void*) 0);

						ycoo= yco;
						yco= draw_actuatorbuttons(ob, act, block, xco, yco, width);
						if(yco-6 < ycoo) ycoo= (yco+ycoo-20)/2;
					}
					else {
						set_col_actuator(act->type, 1);
						glRecti((short)(xco+22), yco, (short)(xco+width-22),(short)(yco+19));
						but= uiDefBut(block, LABEL, 0, actuator_name(act->type), (short)(xco+22), yco, 90, 19, act, 0, 0, 0, 0, "Actuator type");
						uiButSetFunc(but, sca_move_actuator, act, NULL);
						but= uiDefBut(block, LABEL, 0, act->name, (short)(xco+112), yco, (short)(width-134), 19, act, 0, 0, 0, 0, "Actuator name");
						uiButSetFunc(but, sca_move_actuator, act, NULL);
						ycoo= yco;
					}

					uiDefIconBut(block, INLINK, 0, ICON_INLINK,(short)(xco-19), ycoo, 19, 19, act, LINK_ACTUATOR, 0, 0, 0, "");

					yco-=20;
				}
				act= act->next;
			}
			yco-= 6;
		}
	}

	uiComposeLinks(block);
	uiDrawBlock(block);

	if(idar) MEM_freeN(idar);
}


