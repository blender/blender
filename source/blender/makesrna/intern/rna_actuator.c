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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_actuator.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_access.h"

#include "rna_internal.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_actuator_types.h"
#include "DNA_scene_types.h" /* for MAXFRAME */

#include "WM_types.h"

#include "BLI_utildefines.h"

/* Always keep in alphabetical order */
EnumPropertyItem actuator_type_items[] = {
	{ACT_ACTION, "ACTION", 0, "Action", ""},
	{ACT_ARMATURE, "ARMATURE", 0, "Armature", ""},
	{ACT_CAMERA, "CAMERA", 0, "Camera", ""},
	{ACT_CONSTRAINT, "CONSTRAINT", 0, "Constraint", ""},
	{ACT_EDIT_OBJECT, "EDIT_OBJECT", 0, "Edit Object", ""},
	{ACT_2DFILTER, "FILTER_2D", 0, "Filter 2D", ""},
	{ACT_GAME, "GAME", 0, "Game", ""},
	{ACT_MESSAGE, "MESSAGE", 0, "Message", ""},
	{ACT_OBJECT, "MOTION", 0, "Motion", ""},
	{ACT_PARENT, "PARENT", 0, "Parent", ""},
	{ACT_PROPERTY, "PROPERTY", 0, "Property", ""},
	{ACT_RANDOM, "RANDOM", 0, "Random", ""},
	{ACT_SCENE, "SCENE", 0, "Scene", ""},
	{ACT_SOUND, "SOUND", 0, "Sound", ""},
	{ACT_STATE, "STATE", 0, "State", ""},
	{ACT_VISIBILITY, "VISIBILITY", 0, "Visibility", ""},
	{ACT_STEERING, "STEERING", 0, "Steering", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "BKE_sca.h"

static StructRNA* rna_Actuator_refine(struct PointerRNA *ptr)
{
	bActuator *actuator = (bActuator*)ptr->data;

	switch (actuator->type) {
		case ACT_ACTION:
			return &RNA_ActionActuator;
		case ACT_OBJECT:
			return &RNA_ObjectActuator;
		case ACT_CAMERA:
			return &RNA_CameraActuator;
		case ACT_SOUND:
			return &RNA_SoundActuator;
		case ACT_PROPERTY:
			return &RNA_PropertyActuator;
		case ACT_CONSTRAINT:
			return &RNA_ConstraintActuator;
		case ACT_EDIT_OBJECT:
			return &RNA_EditObjectActuator;
		case ACT_SCENE:
			return &RNA_SceneActuator;
		case ACT_RANDOM:
			return &RNA_RandomActuator;
		case ACT_MESSAGE:
			return &RNA_MessageActuator;
		case ACT_GAME:
			return &RNA_GameActuator;
		case ACT_VISIBILITY:
			return &RNA_VisibilityActuator;
		case ACT_2DFILTER:
			return &RNA_Filter2DActuator;
		case ACT_PARENT:
			return &RNA_ParentActuator;
		case ACT_STATE:
			return &RNA_StateActuator;
		case ACT_ARMATURE:
			return &RNA_ArmatureActuator;
		case ACT_STEERING:
			return &RNA_SteeringActuator;
		default:
			return &RNA_Actuator;
	}
}

void rna_Actuator_name_set(PointerRNA *ptr, const char *value)
{
	bActuator *act = (bActuator *)ptr->data;

	BLI_strncpy_utf8(act->name, value, sizeof(act->name));

	if (ptr->id.data) {
		Object *ob = (Object *)ptr->id.data;
		BLI_uniquename(&ob->actuators, act, "Actuator", '.', offsetof(bActuator, name), sizeof(act->name));
	}
}

static void rna_Actuator_type_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator *)ptr->data;

	if (value != act->type) {
		act->type = value;
		init_actuator(act);
	}
}

static void rna_ConstraintActuator_type_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator *)ptr->data;
	bConstraintActuator *ca = act->data;

	if (value != ca->type) {
		ca->type = value;
		switch (ca->type) {
		case ACT_CONST_TYPE_ORI:
			/* negative axis not supported in the orientation mode */
			if (ELEM3(ca->mode, ACT_CONST_DIRNX,ACT_CONST_DIRNY, ACT_CONST_DIRNZ))
				ca->mode = ACT_CONST_NONE;
			break;

		case ACT_CONST_TYPE_LOC:
		case ACT_CONST_TYPE_DIST:
		case ACT_CONST_TYPE_FH:
		default:
			break;
		}
	}
}

static float rna_ConstraintActuator_limitmin_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->flag & ACT_CONST_LOCX) fp = ca->minloc;
	else if (ca->flag & ACT_CONST_LOCY) fp = ca->minloc+1;
	else if (ca->flag & ACT_CONST_LOCZ) fp = ca->minloc+2;
	else if (ca->flag & ACT_CONST_ROTX) fp = ca->minrot;
	else if (ca->flag & ACT_CONST_ROTY) fp = ca->minrot+1;
	else fp = ca->minrot+2;

	return *fp;
}

static void rna_ConstraintActuator_limitmin_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->flag & ACT_CONST_LOCX) fp = ca->minloc;
	else if (ca->flag & ACT_CONST_LOCY) fp = ca->minloc+1;
	else if (ca->flag & ACT_CONST_LOCZ) fp = ca->minloc+2;
	else if (ca->flag & ACT_CONST_ROTX) fp = ca->minrot;
	else if (ca->flag & ACT_CONST_ROTY) fp = ca->minrot+1;
	else fp = ca->minrot+2;

	*fp = value;
}

static float rna_ConstraintActuator_limitmax_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->flag & ACT_CONST_LOCX) fp = ca->maxloc;
	else if (ca->flag & ACT_CONST_LOCY) fp = ca->maxloc+1;
	else if (ca->flag & ACT_CONST_LOCZ) fp = ca->maxloc+2;
	else if (ca->flag & ACT_CONST_ROTX) fp = ca->maxrot;
	else if (ca->flag & ACT_CONST_ROTY) fp = ca->maxrot+1;
	else fp = ca->maxrot+2;

	return *fp;
}

static void rna_ConstraintActuator_limitmax_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->flag & ACT_CONST_LOCX) fp = ca->maxloc;
	else if (ca->flag & ACT_CONST_LOCY) fp = ca->maxloc+1;
	else if (ca->flag & ACT_CONST_LOCZ) fp = ca->maxloc+2;
	else if (ca->flag & ACT_CONST_ROTX) fp = ca->maxrot;
	else if (ca->flag & ACT_CONST_ROTY) fp = ca->maxrot+1;
	else fp = ca->maxrot+2;

	*fp = value;
}

static float rna_ConstraintActuator_distance_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->minloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->minloc+1;
	else fp = ca->minloc+2;

	return *fp;
}

static void rna_ConstraintActuator_distance_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->minloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->minloc+1;
	else fp = ca->minloc+2;

	*fp = value;
}

static float rna_ConstraintActuator_range_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->maxloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->maxloc+1;
	else fp = ca->maxloc+2;

	return *fp;
}

static void rna_ConstraintActuator_range_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->maxloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->maxloc+1;
	else fp = ca->maxloc+2;

	*fp = value;
}

static float rna_ConstraintActuator_fhheight_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->minloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->minloc+1;
	else fp = ca->minloc+2;

	return *fp;
}

static void rna_ConstraintActuator_fhheight_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->minloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->minloc+1;
	else fp = ca->minloc+2;

	*fp = value;
}

static float rna_ConstraintActuator_spring_get(struct PointerRNA *ptr)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->maxloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->maxloc+1;
	else fp = ca->maxloc+2;

	return *fp;
}

static void rna_ConstraintActuator_spring_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;
	float *fp;

	if (ca->mode & (ACT_CONST_DIRPX|ACT_CONST_DIRNX)) fp = ca->maxloc;
	else if (ca->mode & (ACT_CONST_DIRPY|ACT_CONST_DIRNY)) fp = ca->maxloc+1;
	else fp = ca->maxloc+2;

	*fp = value;
}
/* ConstraintActuator uses the same property for Material and Property.
 * Therefore we need to clear the property when "use_material_detect" mode changes */
static void rna_Actuator_constraint_detect_material_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator*)ptr->data;
	bConstraintActuator *ca = act->data;

	short old_value = (ca->flag & ACT_CONST_MATERIAL? 1:0);

	if (old_value != value) {
		ca->flag ^= ACT_CONST_MATERIAL;
		ca->matprop[0] = '\0';
	}
}

static void rna_ActionActuator_add_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator *)ptr->data;
	bActionActuator *aa = act->data;

	if (value == 1) {
		aa->flag &= ~ACT_IPOFORCE;
		aa->flag |= ACT_IPOADD;
	}
	else {
		aa->flag &= ~ACT_IPOADD;
	}
}

static void rna_ActionActuator_force_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator *)ptr->data;
	bActionActuator *aa = act->data;

	if (value == 1) {
		aa->flag &= ~ACT_IPOADD;
		aa->flag |= ACT_IPOFORCE;
	}
	else {
		aa->flag &= ~ACT_IPOFORCE;
	}
}

static void rna_ObjectActuator_type_set(struct PointerRNA *ptr, int value)
{
	bActuator *act = (bActuator *)ptr->data;
	bObjectActuator *oa = act->data;
	if (value != oa->type) {
		oa->type = value;
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

static void rna_ObjectActuator_integralcoefficient_set(struct PointerRNA *ptr, float value)
{
	bActuator *act = (bActuator*)ptr->data;
	bObjectActuator *oa = act->data;
	
	oa->forcerot[1] = value;
	oa->forcerot[0] = 60.0f*oa->forcerot[1];
}

static void rna_StateActuator_state_set(PointerRNA *ptr, const int *values)
{
	bActuator *act = (bActuator*)ptr->data;
	bStateActuator *sa = act->data;

	int i, tot = 0;

	/* ensure we always have some state selected */
	for (i = 0; i<OB_MAX_STATES; i++)
		if (values[i])
			tot++;
	
	if (tot == 0)
		return;

	for (i = 0; i<OB_MAX_STATES; i++) {
		if (values[i]) sa->mask |= (1<<i);
		else sa->mask &= ~(1<<i);
	}
}

/* Always keep in alphabetical order */
EnumPropertyItem *rna_Actuator_type_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *free)
{
	EnumPropertyItem *item = NULL;
	Object *ob = NULL;
	int totitem = 0;
	
	if (ptr->type == &RNA_Actuator || RNA_struct_is_a(ptr->type, &RNA_Actuator)) {
		ob = (Object *)ptr->id.data;
	}
	else {
		/* can't use ob from ptr->id.data because that enum is also used by operators */
		ob = CTX_data_active_object(C);
	}
	
	if (ob != NULL) {
		if (ob->type == OB_ARMATURE) {
			RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_ARMATURE);
		}
	}
	
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_ACTION);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_CAMERA);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_CONSTRAINT);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_EDIT_OBJECT);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_2DFILTER);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_GAME);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_MESSAGE);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_OBJECT);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_PARENT);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_PROPERTY);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_RANDOM);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_SCENE);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_STEERING);

	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_SOUND);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_STATE);
	RNA_enum_items_add_value(&item, &totitem, actuator_type_items, ACT_VISIBILITY);
	
	RNA_enum_item_end(&item, &totitem);
	*free = 1;
	
	return item;
}

static void rna_Actuator_Armature_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	bActuator *act = (bActuator *)ptr->data;
	bArmatureActuator *aa = act->data;
	Object *ob = (Object *)ptr->id.data;

	char *posechannel = aa->posechannel;
	char *constraint = aa->constraint;

	/* check that bone exist in the active object */
	if (ob->type == OB_ARMATURE && ob->pose) {
		bPoseChannel *pchan;
		bPose *pose = ob->pose;
		for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
			if (!strcmp(pchan->name, posechannel)) {
				/* found it, now look for constraint channel */
				bConstraint *con;
				for (con = pchan->constraints.first; con; con = con->next) {
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

static void rna_SteeringActuator_navmesh_set(PointerRNA *ptr, PointerRNA value)
{
	bActuator *act = (bActuator*)ptr->data;
	bSteeringActuator *sa = (bSteeringActuator*) act->data;

	Object* obj = value.data;
	if (obj && obj->body_type == OB_BODY_TYPE_NAVMESH)
		sa->navmesh = obj;
	else
		sa->navmesh = NULL;
}

/* note: the following set functions exists only to avoid id refcounting */
static void rna_Actuator_editobject_mesh_set(PointerRNA *ptr, PointerRNA value)
{
	bActuator *act = (bActuator *)ptr->data;
	bEditObjectActuator *eoa = (bEditObjectActuator *) act->data;

	eoa->me = value.data;
}

static void rna_Actuator_action_action_set(PointerRNA *ptr, PointerRNA value)
{
	bActuator *act = (bActuator *)ptr->data;
	bActionActuator *aa = (bActionActuator *) act->data;

	aa->act = value.data;
}

#else

void rna_def_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Actuator", NULL);
	RNA_def_struct_ui_text(srna, "Actuator", "Actuator to apply actions in the game engine");
	RNA_def_struct_sdna(srna, "bActuator");
	RNA_def_struct_refine_func(srna, "rna_Actuator_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Actuator_name_set");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, actuator_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Actuator_type_set", "rna_Actuator_type_itemf");
	RNA_def_property_ui_text(prop, "Type", "");

	prop = RNA_def_property(srna, "pin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_PIN);
	RNA_def_property_ui_text(prop, "Pinned", "Display when not linked to a visible states controller");
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_SHOW);
	RNA_def_property_ui_text(prop, "Expanded", "Set actuator expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

	RNA_api_actuator(srna);
}

static void rna_def_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_ACTION_PLAY, "PLAY", 0, "Play", ""},
		{ACT_ACTION_PINGPONG, "PINGPONG", 0, "Ping Pong", ""},
		{ACT_ACTION_FLIPPER, "FLIPPER", 0, "Flipper", ""},
		{ACT_ACTION_LOOP_STOP, "LOOPSTOP", 0, "Loop Stop", ""},
		{ACT_ACTION_LOOP_END, "LOOPEND", 0, "Loop End", ""},
		{ACT_ACTION_FROM_PROP, "PROPERTY", 0, "Property", ""},
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
		{ACT_ACTION_MOTION, "MOTION", 0, "Displacement", ""},
#endif
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Action Actuator", "Actuator to control the object movement");
	RNA_def_struct_sdna_from(srna, "bActionActuator", "data");

	prop = RNA_def_property(srna, "play_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Action Type", "Action playback type");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act");
	RNA_def_property_struct_type(prop, "Action");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Action", "");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Actuator_action_action_set", NULL, NULL);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_continue_last_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "end_reset", 1);
	RNA_def_property_ui_text(prop, "Continue",
	                         "Restore last frame when switching on/off, otherwise play from the start each time");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Use this property to define the Action position");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sta");
	RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 100, 2);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "end");
	RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 100, 2);
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_blend_in", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "blendin");
	RNA_def_property_range(prop, 0, 32767);
	RNA_def_property_ui_text(prop, "Blendin", "Number of frames of motion blending");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "priority", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Priority",
	                         "Execution priority - lower numbers will override actions with higher numbers "
	                         "(with 2 or more actions at once, the overriding channels must be lower in the stack)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "layer", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 7); /* This should match BL_ActionManager::MAX_ACTION_LAYERS - 1 */
	RNA_def_property_ui_text(prop, "Layer", "The animation layer to play the action on");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "layer_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Layer Weight",
	                         "How much of the previous layer to blend into this one (0 = add mode)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "frameProp");
	RNA_def_property_ui_text(prop, "Frame Property", "Assign the action's current frame number to this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* booleans */
	prop = RNA_def_property(srna, "use_additive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOADD);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ActionActuator_add_set");
	RNA_def_property_ui_text(prop, "Add",
	                         "Action is added to the current loc/rot/scale in global or local coordinate according to "
	                         "Local flag");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_force", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOFORCE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_ActionActuator_force_set");
	RNA_def_property_ui_text(prop, "Force",
	                         "Apply Action as a global or local force depending on the local option "
	                         "(dynamic objects only)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "use_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOLOCAL);
	RNA_def_property_ui_text(prop, "L", "Let the Action act in local coordinates, used in Force and Add mode");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "apply_to_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOCHILD);
	RNA_def_property_ui_text(prop, "Child", "Update Action on all children Objects as well");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
	prop = RNA_def_property(srna, "stride_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stridelength");
	RNA_def_property_range(prop, 0.0, 2500.0);
	RNA_def_property_ui_text(prop, "Cycle", "Distance covered by a single cycle of the action");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
#endif
}

static void rna_def_object_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA* prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_OBJECT_NORMAL, "OBJECT_NORMAL", 0, "Simple Motion", ""},
		{ACT_OBJECT_SERVO, "OBJECT_SERVO", 0, "Servo Control", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ObjectActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Motion Actuator", "Actuator to control the object movement");
	RNA_def_struct_sdna_from(srna, "bObjectActuator", "data");


	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ObjectActuator_type_set", NULL);
	RNA_def_property_ui_text(prop, "Motion Type", "Specify the motion system");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "reference_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "reference");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Reference Object",
	                         "Reference object for velocity calculation, leave empty for world reference");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "damping", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 1);
	RNA_def_property_ui_text(prop, "Damping Frames", "Number of frames to reach the target velocity");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "proportional_coefficient", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "forcerot[0]");
	RNA_def_property_ui_range(prop, 0.0, 200.0, 10, 2);
	RNA_def_property_ui_text(prop, "Proportional Coefficient", "Typical value is 60x integral coefficient");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "integral_coefficient", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "forcerot[1]");
	RNA_def_property_ui_range(prop, 0.0, 3.0, 10, 2);
	RNA_def_property_float_funcs(prop, NULL, "rna_ObjectActuator_integralcoefficient_set", NULL);
	RNA_def_property_ui_text(prop, "Integral Coefficient",
	                         "Low value (0.01) for slow response, high value (0.5) for fast response");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "derivate_coefficient", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "forcerot[2]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 10, 2);
	RNA_def_property_ui_text(prop, "Derivate Coefficient", "Not required, high values can cause instability");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Servo Limit */
	prop = RNA_def_property(srna, "force_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dloc[0]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max", "Upper limit for X force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drot[0]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Min", "Lower limit for X force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dloc[1]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max", "Upper limit for Y force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drot[1]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Min", "Lower limit for Y force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force_max_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dloc[2]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max", "Upper limit for Z force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force_min_z", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drot[2]");
	RNA_def_property_ui_range(prop, -100.0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Min", "Lower limit for Z force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* floats 3 Arrays*/
	prop = RNA_def_property(srna, "offset_location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "dloc");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Loc", "Location");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "offset_rotation", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float_sdna(prop, NULL, "drot");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Rot", "Rotation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "force", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "forceloc");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Force", "Force");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "torque", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "forcerot");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Torque", "Torque");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "linear_velocity", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "linearvelocity");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Linear Velocity",
	                         "Linear velocity (in Servo mode it sets the target relative linear velocity, it will be "
	                         "achieved by automatic application of force - Null velocity is a valid target)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "angularvelocity");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Angular Velocity", "Angular velocity");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* booleans */
	prop = RNA_def_property(srna, "use_local_location", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_DLOC_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Location is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_DROT_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Rotation is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_force", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_FORCE_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Force is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_torque", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_TORQUE_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Torque is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_linear_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_LIN_VEL_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Velocity is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_angular_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_ANG_VEL_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Angular velocity is defined in local coordinates");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_add_linear_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_ADD_LIN_VEL);
	RNA_def_property_ui_text(prop, "Add", "Toggles between ADD and SET linV");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_servo_limit_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_SERVO_LIMIT_X);
	RNA_def_property_ui_text(prop, "X", "Set limit to force along the X axis");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_servo_limit_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_SERVO_LIMIT_Y);
	RNA_def_property_ui_text(prop, "Y", "Set limit to force along the Y axis");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_servo_limit_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_SERVO_LIMIT_Z);
	RNA_def_property_ui_text(prop, "Z", "Set limit to force along the Z axis");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_camera_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_axis_items[] = {
		{OB_POSX, "POS_X", 0, "+X", "Camera tries to get behind the X axis"},
		{OB_POSY, "POS_Y", 0, "+Y", "Camera tries to get behind the Y axis"},
		{OB_NEGX, "NEG_X", 0, "-X", "Camera tries to get behind the -X axis"},
		{OB_NEGY, "NEG_Y", 0, "-Y", "Camera tries to get behind the -Y axis"},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "CameraActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Camera Actuator", "");
	RNA_def_struct_sdna_from(srna, "bCameraActuator", "data");

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Camera Object", "Look at this Object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* floats */
	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 1, 2);
	RNA_def_property_ui_text(prop, "Height", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 1, 2);
	RNA_def_property_ui_text(prop, "Min", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "damping");
	RNA_def_property_range(prop, 0, 10.0);
	RNA_def_property_ui_range(prop, 0, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Damping", "Strength of the constraint that drives the camera behind the target");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* +x/+y/-x/-y */
	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis");
	RNA_def_property_enum_items(prop, prop_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Axis the Camera will try to get behind");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_sound_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_SND_PLAY_STOP_SOUND, "PLAYSTOP", 0, "Play Stop", ""},
		{ACT_SND_PLAY_END_SOUND, "PLAYEND", 0, "Play End", ""},
		{ACT_SND_LOOP_STOP_SOUND, "LOOPSTOP", 0, "Loop Stop", ""},
		{ACT_SND_LOOP_END_SOUND, "LOOPEND", 0, "Loop End", ""},
		{ACT_SND_LOOP_BIDIRECTIONAL_SOUND, "LOOPBIDIRECTIONAL", 0, "Loop Bidirectional", ""},
		{ACT_SND_LOOP_BIDIRECTIONAL_STOP_SOUND, "LOOPBIDIRECTIONALSTOP", 0, "Loop Bidirectional Stop", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "SoundActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Sound Actuator", "Actuator to handle sound");
	RNA_def_struct_sdna_from(srna, "bSoundActuator", "data");

	prop = RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sound");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_struct_ui_text(srna, "Sound", "Sound file");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Play Mode", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
	RNA_def_property_range(prop, 0.0, 2.0);
	RNA_def_property_ui_text(prop, "Volume", "Initial volume of the sound");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, -12.0, 12.0, 1, 2);
	RNA_def_property_ui_text(prop, "Pitch", "Pitch of the sound");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* floats - 3D Parameters */
	prop = RNA_def_property(srna, "gain_3d_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.min_gain");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Minimum Gain", "The minimum gain of the sound, no matter how far it is away");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "gain_3d_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.max_gain");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Maximum Gain", "The maximum gain of the sound, no matter how near it is");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distance_3d_reference", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.reference_distance");
	RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 1, 2);
	RNA_def_property_ui_text(prop, "Reference Distance", "The distance where the sound has a gain of 1.0");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "distance_3d_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.max_distance");
	RNA_def_property_ui_range(prop, 0.0, FLT_MAX, 1, 2);
	RNA_def_property_ui_text(prop, "Maximum Distance", "The maximum distance at which you can hear the sound");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "rolloff_factor_3d", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.rolloff_factor");
	RNA_def_property_ui_range(prop, 0.0, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Rolloff", "The influence factor on volume depending on distance");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "cone_outer_gain_3d", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.cone_outer_gain");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Cone Outer Gain",
	                         "The gain outside the outer cone (the gain in the outer cone will be interpolated "
	                         "between this value and the normal gain in the inner cone)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "cone_outer_angle_3d", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.cone_outer_angle");
	RNA_def_property_ui_range(prop, 0.0, 360.0, 1, 2);
	RNA_def_property_ui_text(prop, "Cone Outer Angle", "The angle of the outer cone");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "cone_inner_angle_3d", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sound3D.cone_inner_angle");
	RNA_def_property_ui_range(prop, 0.0, 360.0, 1, 2);
	RNA_def_property_ui_text(prop, "Cone Inner Angle", "The angle of the inner cone");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* booleans */
	prop = RNA_def_property(srna, "use_sound_3d", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_SND_3D_SOUND);
	RNA_def_property_ui_text(prop, "3D Sound", "Enable/Disable 3D Sound");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_property_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_PROP_ASSIGN, "ASSIGN", 0, "Assign", ""},
		{ACT_PROP_ADD, "ADD", 0, "Add", ""},
		{ACT_PROP_COPY, "COPY", 0, "Copy", ""},
		{ACT_PROP_TOGGLE, "TOGGLE", 0, "Toggle", "For bool/int/float/timer properties only"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "PropertyActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Property Actuator", "Actuator to handle properties");
	RNA_def_struct_sdna_from(srna, "bPropertyActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "The name of the property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "value", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Value", "The name of the property or the value to use (use \"\" around strings)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Copy Mode */
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Copy from this Object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX add even magic'er property lookup (need to look for the property list of the target object) */
	prop = RNA_def_property(srna, "object_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Property Name", "Copy this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_constraint_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_CONST_TYPE_LOC, "LOC", 0, "Location Constraint", ""},
		{ACT_CONST_TYPE_DIST, "DIST", 0, "Distance Constraint", ""},
		{ACT_CONST_TYPE_ORI, "ORI", 0, "Orientation Constraint", ""},
		{ACT_CONST_TYPE_FH, "FH", 0, "Force Field Constraint", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_limit_items[] = {
		{ACT_CONST_NONE, "NONE", 0, "None", ""},
		{ACT_CONST_LOCX, "LOCX", 0, "Loc X", ""},
		{ACT_CONST_LOCY, "LOCY", 0, "Loc Y", ""},
		{ACT_CONST_LOCZ, "LOCZ", 0, "Loc Z", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_direction_items[] = {
		{ACT_CONST_NONE, "NONE", 0, "None", ""},
		{ACT_CONST_DIRPX, "DIRPX", 0, "X axis", ""},
		{ACT_CONST_DIRPY, "DIRPY", 0, "Y axis", ""},
		{ACT_CONST_DIRPZ, "DIRPZ", 0, "Z axis", ""},
		{ACT_CONST_DIRNX, "DIRNX", 0, "-X axis", ""},
		{ACT_CONST_DIRNY, "DIRNY", 0, "-Y axis", ""},
		{ACT_CONST_DIRNZ, "DIRNZ", 0, "-Z axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_direction_pos_items[] = {
		{ACT_CONST_NONE, "NONE", 0, "None", ""},
		{ACT_CONST_DIRPX, "DIRPX", 0, "X axis", ""},
		{ACT_CONST_DIRPY, "DIRPY", 0, "Y axis", ""},
		{ACT_CONST_DIRPZ, "DIRPZ", 0, "Z axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ConstraintActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Constraint Actuator", "Actuator to handle Constraints");
	RNA_def_struct_sdna_from(srna, "bConstraintActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ConstraintActuator_type_set", NULL);
	RNA_def_property_ui_text(prop, "Constraints Mode", "The type of the constraint");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "limit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_limit_items);
	RNA_def_property_ui_text(prop, "Limit", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_direction_items);
	RNA_def_property_ui_text(prop, "Direction", "Direction of the ray");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "direction_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_direction_items);
	RNA_def_property_ui_text(prop, "Direction", "Select the axis to be aligned along the reference direction");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_CONST_TYPE_LOC */
	prop = RNA_def_property(srna, "limit_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_limitmin_get",
	                             "rna_ConstraintActuator_limitmin_set", NULL);
	RNA_def_property_ui_range(prop, -2000.f, 2000.f, 1, 2);
	RNA_def_property_ui_text(prop, "Min", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "limit_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_limitmax_get",
	                             "rna_ConstraintActuator_limitmax_set", NULL);
	RNA_def_property_ui_range(prop, -2000.f, 2000.f, 1, 2);
	RNA_def_property_ui_text(prop, "Max", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "damping", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "damp");
	RNA_def_property_ui_range(prop, 0, 100, 1, 1);
	RNA_def_property_ui_text(prop, "Damping", "Damping factor: time constant (in frame) of low pass filter");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_CONST_TYPE_DIST */
	prop = RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_range_get", "rna_ConstraintActuator_range_set", NULL);
	RNA_def_property_ui_range(prop, 0.f, 2000.f, 1, 2);
	RNA_def_property_ui_text(prop, "Range", "Maximum length of ray");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_distance_get",
	                             "rna_ConstraintActuator_distance_set", NULL);
	RNA_def_property_ui_range(prop, -2000.f, 2000.f, 1, 2);
	RNA_def_property_ui_text(prop, "Distance", "Keep this distance to target");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX to use a pointer or add a material lookup */
	prop = RNA_def_property(srna, "material", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "matprop");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material", "Ray detects only Objects with this material");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX add magic property lookup */
	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "matprop");
	RNA_def_property_ui_text(prop, "Property", "Ray detects only Objects with this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "time", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 2);
	RNA_def_property_ui_text(prop, "Time", "Maximum activation time in frame, 0 for unlimited");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "damping_rotation", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rotdamp");
	RNA_def_property_ui_range(prop, 0, 100, 1, 1);
	RNA_def_property_ui_text(prop, "RotDamp", "Use a different damping for orientation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_CONST_TYPE_ORI */
	prop = RNA_def_property(srna, "direction_axis_pos", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_direction_pos_items);
	RNA_def_property_ui_text(prop, "Direction", "Select the axis to be aligned along the reference direction");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "rotation_max", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "maxrot");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -2000.0, 2000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Reference Direction", "Reference Direction");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX TODO - use radians internally then change to PROP_ANGLE */
	prop = RNA_def_property(srna, "angle_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "minloc[0]");
	RNA_def_property_range(prop, 0.0, 180.0);
	RNA_def_property_ui_text(prop, "Min Angle",
	                         "Minimum angle (in degree) to maintain with target direction "
	                         "(no correction is done if angle with target direction is between min and max)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX TODO - use radians internally then change to PROP_ANGLE */
	prop = RNA_def_property(srna, "angle_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxloc[0]");
	RNA_def_property_range(prop, 0.0, 180.0);
	RNA_def_property_ui_text(prop, "Max Angle",
	                         "Maximum angle (in degree) allowed with target direction "
	                         "(no correction is done if angle with target direction is between min and max)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_CONST_TYPE_FH */
	prop = RNA_def_property(srna, "fh_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_fhheight_get",
	                             "rna_ConstraintActuator_fhheight_set", NULL);
	RNA_def_property_ui_range(prop, 0.01, 2000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Distance", "Height of the force field area");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "fh_force", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ConstraintActuator_spring_get", "rna_ConstraintActuator_spring_set", NULL);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 2);
	RNA_def_property_ui_text(prop, "Force", "Spring force within the force field area");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "fh_damping", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "maxrot[0]");
	RNA_def_property_ui_range(prop, 0.0, 1.0, 10, 2);
	RNA_def_property_ui_text(prop, "Damping", "Damping factor of the force field spring");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* booleans */
	prop = RNA_def_property(srna, "use_force_distance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_DISTANCE);
	RNA_def_property_ui_text(prop, "Force Distance", "Force distance of object to point of impact of ray");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_LOCAL);
	RNA_def_property_ui_text(prop, "L", "Set ray along object's axis or global axis");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_NORMAL);
	RNA_def_property_ui_text(prop, "N",
	                         "Set object axis along (local axis) or parallel (global axis) to the normal at "
	                         "hit position");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_persistent", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_PERMANENT);
	RNA_def_property_ui_text(prop, "PER", "Persistent actuator: stays active even if ray does not reach target");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX to use an enum instead of a flag if possible */
	prop = RNA_def_property(srna, "use_material_detect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_MATERIAL);
	RNA_def_property_ui_text(prop, "M/P", "Detect material instead of property");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Actuator_constraint_detect_material_set");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_fh_paralel_axis", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_DOROTFH);
	RNA_def_property_ui_text(prop, "Rot Fh", "Keep object axis parallel to normal");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_fh_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_CONST_NORMAL);
	RNA_def_property_ui_text(prop, "N", "Add a horizontal spring force on slopes");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_edit_object_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_dyn_items[] = {
		{ACT_EDOB_RESTORE_DYN, "RESTOREDYN", 0, "Restore Dynamics", ""},
		{ACT_EDOB_SUSPEND_DYN, "SUSPENDDYN", 0, "Suspend Dynamics", ""},
		{ACT_EDOB_ENABLE_RB, "ENABLERIGIDBODY", 0, "Enable Rigid Body", ""},
		{ACT_EDOB_DISABLE_RB, "DISABLERIGIDBODY", 0, "Disable Rigid Body", ""},
		{ACT_EDOB_SET_MASS, "SETMASS", 0, "Set Mass", ""},
		{0, NULL, 0, NULL, NULL} };

	static EnumPropertyItem prop_type_items[] = {
	{ACT_EDOB_ADD_OBJECT, "ADDOBJECT", 0, "Add Object", ""},
	{ACT_EDOB_END_OBJECT, "ENDOBJECT", 0, "End Object", ""},
	{ACT_EDOB_REPLACE_MESH, "REPLACEMESH", 0, "Replace Mesh", ""},
	{ACT_EDOB_TRACK_TO, "TRACKTO", 0, "Track to", ""},
	{ACT_EDOB_DYNAMICS, "DYNAMICS", 0, "Dynamics", ""},
	{0, NULL, 0, NULL, NULL} };

	srna = RNA_def_struct(brna, "EditObjectActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Edit Object Actuator", "Actuator used to edit objects");
	RNA_def_struct_sdna_from(srna, "bEditObjectActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Edit Object", "The mode of the actuator");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "dynamic_operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dyn_operation");
	RNA_def_property_enum_items(prop, prop_dyn_items);
	RNA_def_property_ui_text(prop, "Dynamic Operation", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Add this Object and all its children (can't be on a visible layer)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "track_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Track to this Object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Mesh");
	RNA_def_property_pointer_sdna(prop, NULL, "me");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mesh",
	                         "Replace the existing, when left blank 'Phys' will remake the existing physics mesh");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Actuator_editobject_mesh_set", NULL, NULL);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "time", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 2000, 1, 1);
	RNA_def_property_ui_text(prop, "Time", "Duration the new Object lives or the track takes");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 10000, 1, 2);
	RNA_def_property_ui_text(prop, "Mass", "The mass of the object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* floats 3 Arrays*/
	prop = RNA_def_property(srna, "linear_velocity", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "linVelocity");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -100.0, 100.0, 10, 2);
	RNA_def_property_ui_text(prop, "Linear Velocity", "Velocity upon creation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "angVelocity");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 2);
	RNA_def_property_ui_text(prop, "Angular Velocity", "Angular velocity upon creation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* booleans */
	prop = RNA_def_property(srna, "use_local_linear_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "localflag", ACT_EDOB_LOCAL_LINV);
	RNA_def_property_ui_text(prop, "L", "Apply the transformation locally");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_local_angular_velocity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "localflag", ACT_EDOB_LOCAL_ANGV);
	RNA_def_property_ui_text(prop, "L", "Apply the rotation locally");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_replace_display_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ACT_EDOB_REPLACE_MESH_NOGFX);
	RNA_def_property_ui_text(prop, "Gfx", "Replace the display mesh");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_replace_physics_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_EDOB_REPLACE_MESH_PHYS);
	RNA_def_property_ui_text(prop, "Phys",
	                         "Replace the physics mesh (triangle bounds only - compound shapes not supported)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_3d_tracking", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_TRACK_3D);
	RNA_def_property_ui_text(prop, "3D", "Enable 3D tracking");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_scene_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_SCENE_RESTART, "RESTART", 0, "Restart", ""},
		{ACT_SCENE_SET, "SET", 0, "Set Scene", ""},
		{ACT_SCENE_CAMERA, "CAMERA", 0, "Set Camera", ""},
		{ACT_SCENE_ADD_FRONT, "ADDFRONT", 0, "Add Overlay Scene", ""},
		{ACT_SCENE_ADD_BACK, "ADDBACK", 0, "Add Background Scene", ""},
		{ACT_SCENE_REMOVE, "REMOVE", 0, "Remove Scene", ""},
		{ACT_SCENE_SUSPEND, "SUSPEND", 0, "Suspend Scene", ""},
		{ACT_SCENE_RESUME, "RESUME", 0, "Resume Scene", ""},
		{0, NULL, 0, NULL, NULL}};
		
	srna = RNA_def_struct(brna, "SceneActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Scene Actuator", "");
	RNA_def_struct_sdna_from(srna, "bSceneActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Mode", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/*XXX filter only camera objects */
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Camera Object", "Set this Camera (leave empty to refer to self object)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Scene", "Scene to be added/removed/paused/resumed");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* XXX no need for those tooltips. to remove soon
	 * Originally we had different 'scene' tooltips for different values of 'type'.
	 * They were:
	 * ACT_SCENE_RESTART	""
	 * ACT_SCENE_CAMERA	""
	 * ACT_SCENE_SET		"Set this Scene"
	 * ACT_SCENE_ADD_FRONT	"Add an Overlay Scene"
	 * ACT_SCENE_ADD_BACK	"Add a Background Scene"
	 * ACT_SCENE_REMOVE	"Remove a Scene"
	 * ACT_SCENE_SUSPEND	"Pause a Scene"
	 * ACT_SCENE_RESUME	"Unpause a Scene"
	 *
	 * It can be done in the ui script if still needed.
	 */
	
}

static void rna_def_random_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_distribution_items[] = {
		{ACT_RANDOM_BOOL_CONST, "BOOL_CONSTANT", 0, "Bool Constant", ""},
		{ACT_RANDOM_BOOL_UNIFORM, "BOOL_UNIFORM", 0, "Bool Uniform", ""},
		{ACT_RANDOM_BOOL_BERNOUILLI, "BOOL_BERNOUILLI", 0, "Bool Bernoulli", ""},
		{ACT_RANDOM_INT_CONST, "INT_CONSTANT", 0, "Int Constant", ""},
		{ACT_RANDOM_INT_UNIFORM, "INT_UNIFORM", 0, "Int Uniform", ""},
		{ACT_RANDOM_INT_POISSON, "INT_POISSON", 0, "Int Poisson", ""},
		{ACT_RANDOM_FLOAT_CONST, "FLOAT_CONSTANT", 0, "Float Constant", ""},
		{ACT_RANDOM_FLOAT_UNIFORM, "FLOAT_UNIFORM", 0, "Float Uniform", ""},
		{ACT_RANDOM_FLOAT_NORMAL, "FLOAT_NORMAL", 0, "Float Normal", ""},
		{ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL, "FLOAT_NEGATIVE_EXPONENTIAL", 0, "Float Neg. Exp.", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "RandomActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Random Actuator", "");
	RNA_def_struct_sdna_from(srna, "bRandomActuator", "data");

	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 1);
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "Seed",
	                         "Initial seed of the random generator, use Python for more freedom "
	                         "(choose 0 for not random)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "propname");
	RNA_def_property_ui_text(prop, "Property", "Assign the random value to this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_distribution_items);
	RNA_def_property_ui_text(prop, "Distribution", "Choose the type of distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* arguments for the distribution */
	/* int_arg_1, int_arg_2, float_arg_1, float_arg_2 */

	/* ACT_RANDOM_BOOL_CONST */
	prop = RNA_def_property(srna, "use_always_true", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "int_arg_1", 1);
	RNA_def_property_ui_text(prop, "Always True", "Always false or always true");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_BOOL_UNIFORM */
	/* label => "Choose between true and false, 50% chance each" */

	/* ACT_RANDOM_BOOL_BERNOUILLI */
	prop = RNA_def_property(srna, "chance", PROP_FLOAT, PROP_PERCENTAGE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Chance", "Pick a number between 0 and 1, success if it's below this value");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_CONST */
	prop = RNA_def_property(srna, "int_value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_1");
	RNA_def_property_ui_range(prop, -1000, 1000, 1, 1);
	RNA_def_property_ui_text(prop, "Value", "Always return this number");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_UNIFORM */
	prop = RNA_def_property(srna, "int_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_1");
	RNA_def_property_range(prop, -1000, 1000);
	RNA_def_property_ui_text(prop, "Min", "Choose a number from a range: lower boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "int_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_2");
	RNA_def_property_range(prop, -1000, 1000);
	RNA_def_property_ui_text(prop, "Max", "Choose a number from a range: upper boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_POISSON */
	prop = RNA_def_property(srna, "int_mean", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.01, 100.0);
	RNA_def_property_ui_text(prop, "Mean", "Expected mean value of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_CONST */
	prop = RNA_def_property(srna, "float_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Value", "Always return this number");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_UNIFORM */
	prop = RNA_def_property(srna, "float_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Min", "Choose a number from a range: lower boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "float_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_2");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Max", "Choose a number from a range: upper boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_NORMAL */
	prop = RNA_def_property(srna, "float_mean", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Mean", "A normal distribution: mean of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "standard_derivation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_2");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "SD", "A normal distribution: standard deviation of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL */
	prop = RNA_def_property(srna, "half_life_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Half-Life Time", "Negative exponential dropoff");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_message_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_body_type_items[] = {
		{ACT_MESG_MESG, "TEXT", 0, "Text", ""},
		{ACT_MESG_PROP, "PROPERTY", 0, "Property", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MessageActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Message Actuator", "");
	RNA_def_struct_sdna_from(srna, "bMessageActuator", "data");

	prop = RNA_def_property(srna, "to_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "toPropName");
	RNA_def_property_ui_text(prop, "To",
	                         "Optional, send message to objects with this name only, or empty to broadcast");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "subject", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Subject", "Optional, message subject (this is what can be filtered on)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "body_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bodyType");
	RNA_def_property_enum_items(prop, prop_body_type_items);
	RNA_def_property_ui_text(prop, "Body", "Toggle message type: either Text or a PropertyName");

	/* ACT_MESG_MESG */
	prop = RNA_def_property(srna, "body_message", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "body");
	RNA_def_property_ui_text(prop, "Body", "Optional, message body Text");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* ACT_MESG_PROP */
	prop = RNA_def_property(srna, "body_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "body");
	RNA_def_property_ui_text(prop, "Prop Name", "The message body will be set by the Property Value");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_game_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
/*		{ACT_GAME_LOAD, "LOAD", 0, "Load Game", ""}, */
/*		{ACT_GAME_START, "START", 0, "Start Loaded Game", ""},	 */
/*		keeping the load/start hacky for compatibility with 2.49 */
/*		ideally we could use ACT_GAME_START again and do a do_version() */

		{ACT_GAME_LOAD, "START", 0, "Start Game From File", ""},
		{ACT_GAME_RESTART, "RESTART", 0, "Restart Game", ""},
		{ACT_GAME_QUIT, "QUIT", 0, "Quit Game", ""},
		{ACT_GAME_SAVECFG, "SAVECFG", 0, "Save bge.logic.globalDict", ""},
		{ACT_GAME_LOADCFG, "LOADCFG", 0, "Load bge.logic.globalDict", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "GameActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Game Actuator", "");
	RNA_def_struct_sdna_from(srna, "bGameActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Game", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_GAME_LOAD */
	prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File",
	                         "Load this blend file, use the \"//\" prefix for a path relative to the current "
	                         "blend file");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	/*XXX to do: an operator that calls file_browse with relative_path on and blender filtering active */
}

static void rna_def_visibility_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "VisibilityActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Visibility Actuator", "Actuator to set visibility and occlusion of the object");
	RNA_def_struct_sdna_from(srna, "bVisibilityActuator", "data");

	prop = RNA_def_property(srna, "use_visible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ACT_VISIBILITY_INVISIBLE);
	RNA_def_property_ui_text(prop, "Visible",
	                         "Set the objects visible (initialized from the object render restriction toggle in "
	                         "physics button)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_occlusion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_VISIBILITY_OCCLUSION);
	RNA_def_property_ui_text(prop, "Occlusion",
	                         "Set the object to occlude objects behind it (initialized from the object type in "
	                         "physics button)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "apply_to_children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_VISIBILITY_RECURSIVE);
	RNA_def_property_ui_text(prop, "Children",
	                         "Set all the children of this object to the same visibility/occlusion recursively");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_twodfilter_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_2DFILTER_ENABLED, "ENABLE", 0, "Enable Filter", ""},
		{ACT_2DFILTER_DISABLED, "DISABLE", 0, "Disable Filter", ""},
		{ACT_2DFILTER_NOFILTER, "REMOVE", 0, "Remove Filter", ""},
		{ACT_2DFILTER_MOTIONBLUR, "MOTIONBLUR", 0, "Motion Blur", ""},
		{ACT_2DFILTER_BLUR, "BLUR", 0, "Blur", ""},
		{ACT_2DFILTER_SHARPEN, "SHARPEN", 0, "Sharpen", ""},
		{ACT_2DFILTER_DILATION, "DILATION", 0, "Dilation", ""},
		{ACT_2DFILTER_EROSION, "EROSION", 0, "Erosion", ""},
		{ACT_2DFILTER_LAPLACIAN, "LAPLACIAN", 0, "Laplacian", ""},
		{ACT_2DFILTER_SOBEL, "SOBEL", 0, "Sobel", ""},
		{ACT_2DFILTER_PREWITT, "PREWITT", 0, "Prewitt", ""},
		{ACT_2DFILTER_GRAYSCALE, "GRAYSCALE", 0, "Gray Scale", ""},
		{ACT_2DFILTER_SEPIA, "SEPIA", 0, "Sepia", ""},
		{ACT_2DFILTER_INVERT, "INVERT", 0, "Invert", ""},
		{ACT_2DFILTER_CUSTOMFILTER, "CUSTOMFILTER", 0, "Custom Filter", ""},
/*		{ACT_2DFILTER_NUMBER_OF_FILTERS, "", 0, "Do not use it. Sentinel", ""}, */
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "Filter2DActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Filter 2D Actuator", "Actuator to apply screen graphic effects");
	RNA_def_struct_sdna_from(srna, "bTwoDFilterActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Filter 2D Type", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "glsl_shader", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "text");
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Script", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "filter_pass", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg");
	RNA_def_property_ui_text(prop, "Pass Number", "Set filter order");
	RNA_def_property_range(prop, 0, 99); /*MAX_RENDER_PASS-1 */
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "motion_blur_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg");
	RNA_def_property_ui_text(prop, "Value", "Motion blur factor");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* booleans */
	prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", 1);
	RNA_def_property_ui_text(prop, "Enable", "Enable/Disable Motion Blur");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_parent_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{ACT_PARENT_SET, "SETPARENT", 0, "Set Parent", ""},
		{ACT_PARENT_REMOVE, "REMOVEPARENT", 0, "Remove Parent", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ParentActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Parent Actuator", "");
	RNA_def_struct_sdna_from(srna, "bParentActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Scene", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent Object", "Set this object as parent");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* booleans */
	prop = RNA_def_property(srna, "use_compound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ACT_PARENT_COMPOUND);
	RNA_def_property_ui_text(prop, "Compound",
	                         "Add this object shape to the parent shape "
	                         "(only if the parent shape is already compound)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_ghost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ACT_PARENT_GHOST);
	RNA_def_property_ui_text(prop, "Ghost", "Make this object ghost while parented");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_shape_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_ACTION_PLAY, "PLAY", 0, "Play", ""},
		{ACT_ACTION_PINGPONG, "PINGPONG", 0, "Ping Pong", ""},
		{ACT_ACTION_FLIPPER, "FLIPPER", 0, "Flipper", ""},
		{ACT_ACTION_LOOP_STOP, "LOOPSTOP", 0, "Loop Stop", ""},
		{ACT_ACTION_LOOP_END, "LOOPEND", 0, "Loop End", ""},
		{ACT_ACTION_FROM_PROP, "PROPERTY", 0, "Property", ""},
#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
		{ACT_ACTION_MOTION, "MOTION", 0, "Displacement", ""},
#endif
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ShapeActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Shape Action Actuator", "Actuator to control shape key animations");
	RNA_def_struct_sdna_from(srna, "bActionActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Action Type", "Action playback type");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "act");
	RNA_def_property_struct_type(prop, "Action");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Action", "");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Actuator_action_action_set", NULL, NULL);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_continue_last_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "end_reset", 1);
	RNA_def_property_ui_text(prop, "Continue",
	                         "Restore last frame when switching on/off, otherwise play from the start each time");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Use this property to define the Action position");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sta");
	RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 100, 2);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "end");
	RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 100, 2);
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_blend_in", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "blendin");
	RNA_def_property_range(prop, 0, 32767);
	RNA_def_property_ui_text(prop, "Blendin", "Number of frames of motion blending");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "priority", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Priority",
	                         "Execution priority - lower numbers will override actions with higher numbers "
	                         "(with 2 or more actions at once, the overriding channels must be lower in the stack)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "frame_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "frameProp");
	RNA_def_property_ui_text(prop, "Frame Property", "Assign the action's current frame number to this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

#ifdef __NLA_ACTION_BY_MOTION_ACTUATOR
	prop = RNA_def_property(srna, "stride_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stridelength");
	RNA_def_property_range(prop, 0.0, 2500.0);
	RNA_def_property_ui_text(prop, "Cycle", "Distance covered by a single cycle of the action");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
#endif
}

static void rna_def_state_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_STATE_SET, "SET", 0, "Set State", ""},
		{ACT_STATE_ADD, "ADD", 0, "Add State", ""},
		{ACT_STATE_REMOVE, "REMOVE", 0, "Remove State", ""},
		{ACT_STATE_CHANGE, "CHANGE", 0, "Change State", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna = RNA_def_struct(brna, "StateActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "State Actuator", "Actuator to handle states");
	RNA_def_struct_sdna_from(srna, "bStateActuator", "data");

	prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Operation", "Select the bit operation on object state mask");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "states", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "mask", 1);
	RNA_def_property_array(prop, OB_MAX_STATES);
	RNA_def_property_ui_text(prop, "State", "");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_StateActuator_state_set");
}

static void rna_def_armature_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA* prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_ARM_RUN, "RUN", 0, "Run Armature", ""},
		{ACT_ARM_ENABLE, "ENABLE", 0, "Enable", ""},
		{ACT_ARM_DISABLE, "DISABLE", 0, "Disable", ""},
		{ACT_ARM_SETTARGET, "SETTARGET", 0, "Set Target", ""},
		{ACT_ARM_SETWEIGHT, "SETWEIGHT", 0, "Set Weight", ""},
		{ACT_ARM_SETINFLUENCE, "SETINFLUENCE", 0, "Set Influence", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "ArmatureActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Armature Actuator", "");
	RNA_def_struct_sdna_from(srna, "bArmatureActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Constraint Type", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "bone", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "posechannel");
	RNA_def_property_ui_text(prop, "Bone", "Bone on which the constraint is defined");
	RNA_def_property_update(prop, NC_LOGIC, "rna_Actuator_Armature_update");

	prop = RNA_def_property(srna, "constraint", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "constraint");
	RNA_def_property_ui_text(prop, "Constraint", "Name of the constraint to control");
	RNA_def_property_update(prop, NC_LOGIC, "rna_Actuator_Armature_update");

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Set this object as the target of the constraint");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "secondary_target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "subtarget");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Secondary Target",
	                         "Set this object as the secondary target of the constraint "
	                         "(only IK polar target at the moment)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Weight", "Weight of this constraint");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "influence");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Influence", "Influence of this constraint");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_steering_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] = {
		{ACT_STEERING_SEEK, "SEEK", 0, "Seek", ""},
		{ACT_STEERING_FLEE, "FLEE", 0, "Flee", ""},
		{ACT_STEERING_PATHFOLLOWING, "PATHFOLLOWING", 0, "Path following", ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem facingaxis_items[] = {
		{1, "X", 0, "X", ""},
		{2, "Y", 0, "Y", ""},
		{3, "Z", 0, "Z", ""},
		{4, "NEG_X", 0, "-X", ""},
		{5, "NEG_Y", 0, "-Y", ""},
		{6, "NEG_Z", 0, "-Z", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "SteeringActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Steering Actuator", "");
	RNA_def_struct_sdna_from(srna, "bSteeringActuator", "data");

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Behavior", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "velocity");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Velocity", "Velocity magnitude");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "acceleration", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "acceleration");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Acceleration", "Max acceleration");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "turn_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "turnspeed");
	RNA_def_property_range(prop, 0.0, 720.0);
	RNA_def_property_ui_text(prop, "Turn Speed", "Max turn speed");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Dist", "Relax distance");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target Object", "Target object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "self_terminated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_STEERING_SELFTERMINATED);
	RNA_def_property_ui_text(prop, "Self Terminated", "Terminate when target is reached");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "show_visualization", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_STEERING_ENABLEVISUALIZATION);
	RNA_def_property_ui_text(prop, "Visualize", "Enable debug visualization");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "update_period", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "updateTime");
	RNA_def_property_ui_range(prop, -1, 100000, 1, 1);
	RNA_def_property_ui_text(prop, "Update period", "Path update period");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "navmesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "navmesh");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Navigation Mesh Object", "Navigation mesh");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SteeringActuator_navmesh_set", NULL, NULL);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "facing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_STEERING_AUTOMATICFACING);
	RNA_def_property_ui_text(prop, "Facing", "Enable automatic facing");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "facing_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "facingaxis");
	RNA_def_property_enum_items(prop, facingaxis_items);
	RNA_def_property_ui_text(prop, "Axis", "Axis for automatic facing");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "normal_up", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_STEERING_NORMALUP);
	RNA_def_property_ui_text(prop, "N", "Use normal of the navmesh to set \"UP\" vector");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

void RNA_def_actuator(BlenderRNA *brna)
{
	rna_def_actuator(brna);

	rna_def_action_actuator(brna);
	rna_def_object_actuator(brna);
	rna_def_camera_actuator(brna);
	rna_def_sound_actuator(brna);
	rna_def_property_actuator(brna);
	rna_def_constraint_actuator(brna);
	rna_def_edit_object_actuator(brna);
	rna_def_scene_actuator(brna);
	rna_def_random_actuator(brna);
	rna_def_message_actuator(brna);
	rna_def_game_actuator(brna);
	rna_def_visibility_actuator(brna);
	rna_def_twodfilter_actuator(brna);
	rna_def_parent_actuator(brna);
	rna_def_shape_action_actuator(brna);
	rna_def_state_actuator(brna);
	rna_def_armature_actuator(brna);
	rna_def_steering_actuator(brna);
}

#endif
