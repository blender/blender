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
 * Convert Blender actuators for use in the GameEngine
 */

/** \file gameengine/Converter/KX_ConvertActuators.cpp
 *  \ingroup bgeconv
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include <math.h>

#include "MEM_guardedalloc.h"

#include "KX_BlenderSceneConverter.h"
#include "KX_ConvertActuators.h"

#ifdef WITH_AUDASPACE
#  include AUD_SOUND_H
#endif

// Actuators
//SCA logiclibrary native logicbricks
#include "SCA_PropertyActuator.h"
#include "SCA_LogicManager.h"
#include "SCA_RandomActuator.h"
#include "SCA_2DFilterActuator.h"

// Ketsji specific logicbricks
#include "KX_SceneActuator.h"
#include "KX_SoundActuator.h"
#include "KX_ObjectActuator.h"
#include "KX_TrackToActuator.h"
#include "KX_ConstraintActuator.h"
#include "KX_CameraActuator.h"
#include "KX_GameActuator.h"
#include "KX_StateActuator.h"
#include "KX_VisibilityActuator.h"
#include "KX_SCA_AddObjectActuator.h"
#include "KX_SCA_EndObjectActuator.h"
#include "KX_SCA_ReplaceMeshActuator.h"
#include "KX_ParentActuator.h"
#include "KX_SCA_DynamicActuator.h"
#include "KX_SteeringActuator.h"
#include "KX_MouseActuator.h"

#include "KX_Scene.h"
#include "KX_KetsjiEngine.h"

#include "EXP_IntValue.h"
#include "KX_GameObject.h"

/* This little block needed for linking to Blender... */
#include "BKE_text.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_path_util.h"

#include "KX_NetworkMessageActuator.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_object_types.h"
#include "DNA_sound_types.h"
#include "DNA_scene_types.h"
#include "DNA_actuator_types.h"
#include "DNA_packedFile_types.h"
#include "BL_ActionActuator.h"
#include "BL_ShapeActionActuator.h"
#include "BL_ArmatureActuator.h"
#include "RNA_access.h"
#include "BL_Action.h"
/* end of blender include block */

#include "BL_BlenderDataConversion.h"

/** 
 * KX_flt_trunc needed to round 'almost' zero values to zero, else velocities etc. are incorrectly set
 */

BLI_INLINE float KX_flt_trunc(const float x)
{
	return ( x < 0.0001f && x > -0.0001f ) ? 0.0f : x;
}

void BL_ConvertActuators(const char* maggiename,
                         struct Object* blenderobject,
                         KX_GameObject* gameobj,
                         SCA_LogicManager* logicmgr,
                         KX_Scene* scene,
                         KX_KetsjiEngine* ketsjiEngine,
                         int activeLayerBitInfo,
                         bool isInActiveLayer,
                         KX_BlenderSceneConverter* converter
                         )
{
	
	int uniqueint = 0;
	int actcount = 0;
	int executePriority = 0;
	bActuator* bact = (bActuator*) blenderobject->actuators.first;
	while (bact)
	{
		actcount++;
		bact = bact->next;
	}
	gameobj->ReserveActuator(actcount);
	bact = (bActuator*) blenderobject->actuators.first;
	while (bact)
	{
		STR_String uniquename = bact->name;
		STR_String& objectname = gameobj->GetName();
		
		SCA_IActuator* baseact = NULL;
		switch (bact->type)
		{
		case ACT_OBJECT:
			{
				bObjectActuator* obact = (bObjectActuator*) bact->data;
				KX_GameObject* obref = NULL;
				MT_Vector3 forcevec(KX_flt_trunc(obact->forceloc[0]),
				                    KX_flt_trunc(obact->forceloc[1]),
				                    KX_flt_trunc(obact->forceloc[2]));
				MT_Vector3 torquevec(obact->forcerot[0],
				                     obact->forcerot[1],
				                     obact->forcerot[2]);
				MT_Vector3 dlocvec(KX_flt_trunc(obact->dloc[0]),
				                   KX_flt_trunc(obact->dloc[1]),
				                   KX_flt_trunc(obact->dloc[2]));
				MT_Vector3 drotvec(KX_flt_trunc(obact->drot[0]),
				                   obact->drot[1],obact->drot[2]);
				MT_Vector3 linvelvec(KX_flt_trunc(obact->linearvelocity[0]),
				                     KX_flt_trunc(obact->linearvelocity[1]),
				                     KX_flt_trunc(obact->linearvelocity[2]));
				MT_Vector3 angvelvec(KX_flt_trunc(obact->angularvelocity[0]),
				                     KX_flt_trunc(obact->angularvelocity[1]),
				                     KX_flt_trunc(obact->angularvelocity[2]));
				short damping = obact->damping;

				/* Blender uses a bit vector internally for the local-flags. In */
				/* KX, we have four bools. The compiler should be smart enough  */
				/* to do the right thing. We need to explicitly convert here!   */
				
				KX_LocalFlags bitLocalFlag;
				
				bitLocalFlag.Force = bool((obact->flag & ACT_FORCE_LOCAL)!=0);
				bitLocalFlag.Torque = bool((obact->flag & ACT_TORQUE_LOCAL) !=0);//rlocal;
				bitLocalFlag.DLoc = bool((obact->flag & ACT_DLOC_LOCAL)!=0);
				bitLocalFlag.DRot = bool((obact->flag & ACT_DROT_LOCAL)!=0);
				bitLocalFlag.LinearVelocity = bool((obact->flag & ACT_LIN_VEL_LOCAL)!=0);
				bitLocalFlag.AngularVelocity = bool((obact->flag & ACT_ANG_VEL_LOCAL)!=0);
				bitLocalFlag.ServoControl = bool(obact->type == ACT_OBJECT_SERVO);
				bitLocalFlag.CharacterMotion = bool(obact->type == ACT_OBJECT_CHARACTER);
				bitLocalFlag.CharacterJump = bool((obact->flag & ACT_CHAR_JUMP)!=0);
				bitLocalFlag.AddOrSetLinV = bool((obact->flag & ACT_ADD_LIN_VEL)!=0);
				bitLocalFlag.AddOrSetCharLoc = bool((obact->flag & ACT_ADD_CHAR_LOC)!=0);
				if (obact->reference && bitLocalFlag.ServoControl)
				{
					obref = converter->FindGameObject(obact->reference);
				}
				
				KX_ObjectActuator* tmpbaseact = new KX_ObjectActuator(
				            gameobj,
				            obref,
				            forcevec.getValue(),
				            torquevec.getValue(),
				            dlocvec.getValue(),
				            drotvec.getValue(),
				            linvelvec.getValue(),
				            angvelvec.getValue(),
				            damping,
				            bitLocalFlag);
				baseact = tmpbaseact;
				break;
			}
		case ACT_ACTION:
			{
				bActionActuator* actact = (bActionActuator*) bact->data;
				STR_String propname = actact->name;
				STR_String propframe = actact->frameProp;

				short ipo_flags = 0;

				// Convert flags
				if (actact->flag & ACT_IPOFORCE) ipo_flags |= BL_Action::ACT_IPOFLAG_FORCE;
				if (actact->flag & ACT_IPOLOCAL) ipo_flags |= BL_Action::ACT_IPOFLAG_LOCAL;
				if (actact->flag & ACT_IPOADD) ipo_flags |= BL_Action::ACT_IPOFLAG_ADD;
				if (actact->flag & ACT_IPOCHILD) ipo_flags |= BL_Action::ACT_IPOFLAG_CHILD;
					
				BL_ActionActuator* tmpbaseact = new BL_ActionActuator(
				            gameobj,
				            propname,
				            propframe,
				            actact->sta,
				            actact->end,
				            actact->act,
				            actact->type, // + 1, because Blender starts to count at zero,
				            actact->blend_mode,
				            actact->blendin,
				            actact->priority,
				            actact->layer,
				            actact->layer_weight,
				            ipo_flags,
				            actact->end_reset,
				            actact->stridelength
				            // Ketsji at 1, because zero is reserved for "NoDef"
				            );
				baseact= tmpbaseact;
				break;
			}
		case ACT_SHAPEACTION:
			{
				if (blenderobject->type==OB_MESH) {
					bActionActuator* actact = (bActionActuator*) bact->data;
					STR_String propname = actact->name;
					STR_String propframe = actact->frameProp;
					
					BL_ShapeActionActuator* tmpbaseact = new BL_ShapeActionActuator(
					            gameobj,
					            propname,
					            propframe,
					            actact->sta,
					            actact->end,
					            actact->act,
					            actact->type, // + 1, because Blender starts to count at zero,
					            actact->blendin,
					            actact->priority,
					            actact->stridelength);
					// Ketsji at 1, because zero is reserved for "NoDef"
					baseact= tmpbaseact;
					break;
				}
				else
					printf ("Discarded shape action actuator from non-mesh object [%s]\n", blenderobject->id.name+2);
			}
		case ACT_LAMP:
			{
				break;
			}
		case ACT_CAMERA:
			{
				bCameraActuator *camact = (bCameraActuator *) bact->data;
				if (camact->ob) {
					KX_GameObject *tmpgob = converter->FindGameObject(camact->ob);
					
					/* visifac, fac and axis are not copied from the struct...   */ 
					/* that's some internal state...                             */
					KX_CameraActuator *tmpcamact = new KX_CameraActuator(
					            gameobj,
					            tmpgob,
					            camact->height,
					            camact->min,
					            camact->max,
					            camact->axis,
					            camact->damping);
					baseact = tmpcamact;
				}
				break;
			}
		case ACT_MESSAGE:
			{
				bMessageActuator *msgAct = (bMessageActuator *) bact->data;
				
				/* Get the name of the properties that objects must own that
				 * we're sending to, if present
				 */
				STR_String toPropName = msgAct->toPropName;
				
				/* Get the Message Subject to send.
				 */
				STR_String subject = msgAct->subject;
				
				/* Get the bodyType
				 */
				int bodyType = msgAct->bodyType;

				/* Get the body (text message or property name whose value
				 * we'll be sending, might be empty
				 */
				const STR_String body = msgAct->body;
				
				KX_NetworkMessageActuator *tmpmsgact = new KX_NetworkMessageActuator(
				            gameobj,					// actuator controlling object
				            scene->GetNetworkScene(),	// needed for replication
				            toPropName,
				            subject,
				            bodyType,
				            body);
				baseact = tmpmsgact;
				break;
			}
		case ACT_MATERIAL:
			{
				break;
			}
		case ACT_SOUND:
			{
				bSoundActuator* soundact = (bSoundActuator*) bact->data;
				/* get type, and possibly a start and end frame */
				KX_SoundActuator::KX_SOUNDACT_TYPE
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_NODEF;
				
				switch (soundact->type) {
				case ACT_SND_PLAY_STOP_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_PLAYSTOP;
					break;
				case ACT_SND_PLAY_END_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_PLAYEND;
					break;
				case ACT_SND_LOOP_STOP_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_LOOPSTOP;
					break;
				case ACT_SND_LOOP_END_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_LOOPEND;
					break;
				case ACT_SND_LOOP_BIDIRECTIONAL_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL;
					break;
				case ACT_SND_LOOP_BIDIRECTIONAL_STOP_SOUND:
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP;
					break;
					
				default:
					/* This is an error!!! */
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_NODEF;
				}
				
				if (soundActuatorType != KX_SoundActuator::KX_SOUNDACT_NODEF) 
				{
					bSound* sound = soundact->sound;
					bool is3d = soundact->flag & ACT_SND_3D_SOUND ? true : false;
					AUD_Sound* snd_sound = NULL;
					KX_3DSoundSettings settings;
					settings.cone_inner_angle = RAD2DEGF(soundact->sound3D.cone_inner_angle);
					settings.cone_outer_angle = RAD2DEGF(soundact->sound3D.cone_outer_angle);
					settings.cone_outer_gain = soundact->sound3D.cone_outer_gain;
					settings.max_distance = soundact->sound3D.max_distance;
					settings.max_gain = soundact->sound3D.max_gain;
					settings.min_gain = soundact->sound3D.min_gain;
					settings.reference_distance = soundact->sound3D.reference_distance;
					settings.rolloff_factor = soundact->sound3D.rolloff_factor;

					if (!sound)
					{
						std::cout <<	"WARNING: Sound actuator \"" << bact->name <<
										"\" from object \"" <<  blenderobject->id.name+2 <<
										"\" has no sound datablock." << std::endl;
					}
					else
					{
						snd_sound = sound->playback_handle;

						// if sound shall be 3D but isn't mono, we have to make it mono!
						if (is3d)
						{
							snd_sound = AUD_Sound_rechannel(snd_sound, AUD_CHANNELS_MONO);
						}
					}
					KX_SoundActuator* tmpsoundact =
						new KX_SoundActuator(gameobj,
						snd_sound,
						soundact->volume,
						(float)(expf((soundact->pitch / 12.0f) * (float)M_LN2)),
						is3d,
						settings,
						soundActuatorType);

					// if we made it mono, we have to free it
					if (sound && snd_sound && snd_sound != sound->playback_handle) {
						AUD_Sound_free(snd_sound);
					}

					tmpsoundact->SetName(bact->name);
					baseact = tmpsoundact;
				}
				break;
			}
		case ACT_PROPERTY:
			{
				bPropertyActuator* propact = (bPropertyActuator*) bact->data;
				SCA_IObject* destinationObj = NULL;
				
				/*
				 * here the destinationobject is searched. problem with multiple scenes: other scenes
				 * have not been converted yet, so the destobj will not be found, so the prop will
				 * not be copied.
				 * possible solutions:
				 * - convert everything when possible and not realtime only when needed.
				 * - let the object-with-property report itself to the act when converted
				 */
				if (propact->ob)
					destinationObj = converter->FindGameObject(propact->ob);
				
				SCA_PropertyActuator* tmppropact = new SCA_PropertyActuator(
				            gameobj,
				            destinationObj,
				            propact->name,
				            propact->value,
				            propact->type + 1); // + 1 because Ketsji Logic starts
				// with 0 for KX_ACT_PROP_NODEF
				baseact = tmppropact;
				break;
			}
		case ACT_EDIT_OBJECT:
			{
				bEditObjectActuator *editobact 
					= (bEditObjectActuator *) bact->data;
				/* There are four different kinds of 'edit object' thingies  */
				/* The alternative to this lengthy conversion is packing     */
				/* several actuators in one, which is not very nice design.. */
				switch (editobact->type) {
				case ACT_EDOB_ADD_OBJECT: 
					{
						
						// does the 'original' for replication exists, and 
						// is it in a non-active layer ?
						SCA_IObject* originalval = NULL;
						if (editobact->ob)
						{
							if (editobact->ob->lay & activeLayerBitInfo)
							{
								fprintf(stderr, "Warning, object \"%s\" from AddObject actuator \"%s\" is not in a hidden layer.\n", objectname.Ptr(), uniquename.Ptr());
							}
							else {
								originalval = converter->FindGameObject(editobact->ob);
							}
						}
						
						KX_SCA_AddObjectActuator* tmpaddact = new KX_SCA_AddObjectActuator(
						            gameobj,
						            originalval,
						            editobact->time,
						            scene,
						            editobact->linVelocity,
						            (editobact->localflag & ACT_EDOB_LOCAL_LINV) != 0,
						            editobact->angVelocity,
						            (editobact->localflag & ACT_EDOB_LOCAL_ANGV) != 0);

								//editobact->ob to gameobj
								baseact = tmpaddact;
					}
					break;
				case ACT_EDOB_END_OBJECT:
					{
						KX_SCA_EndObjectActuator* tmpendact 
							= new KX_SCA_EndObjectActuator(gameobj,scene);
						baseact = tmpendact;
					}
					break;
				case ACT_EDOB_REPLACE_MESH:
					{
						RAS_MeshObject *tmpmesh = converter->FindGameMesh(editobact->me);

						if (!tmpmesh) {
							std::cout << "Warning: object \"" << objectname <<
							"\" from ReplaceMesh actuator \"" << uniquename <<
							"\" uses a mesh not owned by an object in scene \"" <<
							scene->GetName() << "\"." << std::endl;
						}

						KX_SCA_ReplaceMeshActuator* tmpreplaceact = new KX_SCA_ReplaceMeshActuator(
						            gameobj,
						            tmpmesh,
						            scene,
						            (editobact->flag & ACT_EDOB_REPLACE_MESH_NOGFX) == 0,
						            (editobact->flag & ACT_EDOB_REPLACE_MESH_PHYS) != 0);

						baseact = tmpreplaceact;
					}
					break;
				case ACT_EDOB_TRACK_TO:
					{
						SCA_IObject* originalval = NULL;
						if (editobact->ob)
							originalval = converter->FindGameObject(editobact->ob);
							
						KX_TrackToActuator* tmptrackact = new KX_TrackToActuator(
						            gameobj,
						            originalval,
						            editobact->time,
						            editobact->flag,
						            editobact->trackflag,
						            editobact->upflag);
						baseact = tmptrackact;
						break;
					}
				case ACT_EDOB_DYNAMICS:
					{
						KX_SCA_DynamicActuator* tmpdynact = new KX_SCA_DynamicActuator(
						            gameobj,
						            editobact->dyn_operation,
						            editobact->mass);
						baseact = tmpdynact;
					}
				}
				break;
			}
		case ACT_CONSTRAINT:
			{
				float min = 0.0, max = 0.0;
				char *prop = NULL;
				KX_ConstraintActuator::KX_CONSTRAINTTYPE locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_NODEF;
				bConstraintActuator *conact 
					= (bConstraintActuator*) bact->data;
				/* convert settings... degrees in the ui become radians  */ 
				/* internally                                            */ 
				if (conact->type == ACT_CONST_TYPE_ORI) {
					min = conact->minloc[0];
					max = conact->maxloc[0];
					switch (conact->mode) {
					case ACT_CONST_DIRPX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIX;
						break;
					case ACT_CONST_DIRPY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIY;
						break;
					case ACT_CONST_DIRPZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIZ;
						break;
					}
				} else if (conact->type == ACT_CONST_TYPE_DIST) {
					switch (conact->mode) {
					case ACT_CONST_DIRPX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPX;
						min = conact->minloc[0];
						max = conact->maxloc[0];
						break;
					case ACT_CONST_DIRPY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPY;
						min = conact->minloc[1];
						max = conact->maxloc[1];
						break;
					case ACT_CONST_DIRPZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPZ;
						min = conact->minloc[2];
						max = conact->maxloc[2];
						break;
					case ACT_CONST_DIRNX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNX;
						min = conact->minloc[0];
						max = conact->maxloc[0];
						break;
					case ACT_CONST_DIRNY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNY;
						min = conact->minloc[1];
						max = conact->maxloc[1];
						break;
					case ACT_CONST_DIRNZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNZ;
						min = conact->minloc[2];
						max = conact->maxloc[2];
						break;
					}
					prop = conact->matprop;
				} else if (conact->type == ACT_CONST_TYPE_FH) {
					switch (conact->mode) {
					case ACT_CONST_DIRPX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPX;
						min = conact->minloc[0];
						max = conact->maxloc[0];
						break;
					case ACT_CONST_DIRPY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPY;
						min = conact->minloc[1];
						max = conact->maxloc[1];
						break;
					case ACT_CONST_DIRPZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPZ;
						min = conact->minloc[2];
						max = conact->maxloc[2];
						break;
					case ACT_CONST_DIRNX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNX;
						min = conact->minloc[0];
						max = conact->maxloc[0];
						break;
					case ACT_CONST_DIRNY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNY;
						min = conact->minloc[1];
						max = conact->maxloc[1];
						break;
					case ACT_CONST_DIRNZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNZ;
						min = conact->minloc[2];
						max = conact->maxloc[2];
						break;
					}
					prop = conact->matprop;
				} else {
					switch (conact->flag) {
					case ACT_CONST_LOCX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCX; 
						min = conact->minloc[0];
						max = conact->maxloc[0];
						break;
					case ACT_CONST_LOCY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCY; 
						min = conact->minloc[1];
						max = conact->maxloc[1];
						break;
					case ACT_CONST_LOCZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCZ;
						min = conact->minloc[2];
						max = conact->maxloc[2];
						break;
					case ACT_CONST_ROTX:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTX;
						min = conact->minrot[0] * (float)MT_RADS_PER_DEG;
						max = conact->maxrot[0] * (float)MT_RADS_PER_DEG;
						break;
					case ACT_CONST_ROTY:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTY;
						min = conact->minrot[1] * (float)MT_RADS_PER_DEG;
						max = conact->maxrot[1] * (float)MT_RADS_PER_DEG;
						break;
					case ACT_CONST_ROTZ:
						locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTZ;
						min = conact->minrot[2] * (float)MT_RADS_PER_DEG;
						max = conact->maxrot[2] * (float)MT_RADS_PER_DEG;
						break;
					default:
						; /* error */ 
					}
				}
				KX_ConstraintActuator *tmpconact = new KX_ConstraintActuator(
				            gameobj,
				            conact->damp,
				            conact->rotdamp,
				            min,
				            max,
				            conact->maxrot,
				            locrot,
				            conact->time,
				            conact->flag,
				            prop);
				baseact = tmpconact;
				break;
			}
		case ACT_GROUP:
			{
				// deprecated
			}
			break;
		case ACT_SCENE:
			{
				bSceneActuator *sceneact = (bSceneActuator *) bact->data;
				STR_String nextSceneName("");
				
				KX_SceneActuator* tmpsceneact;
				int mode = KX_SceneActuator::KX_SCENE_NODEF;
				KX_Camera *cam = NULL;
				//KX_Scene* scene = NULL;
				switch (sceneact->type)
				{
				case ACT_SCENE_RESUME:
				case ACT_SCENE_SUSPEND:
				case ACT_SCENE_ADD_FRONT:
				case ACT_SCENE_ADD_BACK:
				case ACT_SCENE_REMOVE:
				case ACT_SCENE_SET:
					{
						switch (sceneact->type)
						{
						case ACT_SCENE_RESUME:
							mode = KX_SceneActuator::KX_SCENE_RESUME;
							break;
						case ACT_SCENE_SUSPEND:
							mode = KX_SceneActuator::KX_SCENE_SUSPEND;
							break;
						case ACT_SCENE_ADD_FRONT:
							mode = KX_SceneActuator::KX_SCENE_ADD_FRONT_SCENE;
							break;
						case ACT_SCENE_ADD_BACK:
							mode = KX_SceneActuator::KX_SCENE_ADD_BACK_SCENE;
							break;
						case ACT_SCENE_REMOVE:
							mode = KX_SceneActuator::KX_SCENE_REMOVE_SCENE;
							break;
						case ACT_SCENE_SET:
						default:
							mode = KX_SceneActuator::KX_SCENE_SET_SCENE;
							break;
						};
						
						if (sceneact->scene) {
							nextSceneName = sceneact->scene->id.name + 2;
						}
						
						break;
					}
				case ACT_SCENE_CAMERA:
					mode = KX_SceneActuator::KX_SCENE_SET_CAMERA;
					if (sceneact->camera)
					{
						KX_GameObject *tmp = converter->FindGameObject(sceneact->camera);
						if (tmp && tmp->GetGameObjectType() == SCA_IObject::OBJ_CAMERA)
							cam = (KX_Camera*)tmp;
					}
					break;
				case ACT_SCENE_RESTART:
					{
						
						mode =  KX_SceneActuator::KX_SCENE_RESTART;
						break;
					}
				default:
					; /* flag error */
				}
				tmpsceneact = new KX_SceneActuator(
				            gameobj,
				            mode,
				            scene,
				            ketsjiEngine,
				            nextSceneName,
				            cam);
				baseact = tmpsceneact;
				break;
			}
		case ACT_GAME:
			{
				bGameActuator *gameact = (bGameActuator *) bact->data;
				KX_GameActuator* tmpgameact;
				STR_String filename = maggiename;
				STR_String loadinganimationname = "";
				int mode = KX_GameActuator::KX_GAME_NODEF;
				switch (gameact->type)
				{
				case ACT_GAME_LOAD:
					{
						mode = KX_GameActuator::KX_GAME_LOAD;
						filename = gameact->filename;
						loadinganimationname = gameact->loadaniname;
						break;
					}
				case ACT_GAME_START:
					{
						mode = KX_GameActuator::KX_GAME_START;
						filename = gameact->filename;
						loadinganimationname = gameact->loadaniname;
						break;
					}
				case ACT_GAME_RESTART:
					{
						mode = KX_GameActuator::KX_GAME_RESTART;
						break;
					}
				case ACT_GAME_QUIT:
					{
						mode = KX_GameActuator::KX_GAME_QUIT;
						break;
					}
				case ACT_GAME_SAVECFG:
					{
						mode = KX_GameActuator::KX_GAME_SAVECFG;
						break;
					}
				case ACT_GAME_LOADCFG:
					{
						mode = KX_GameActuator::KX_GAME_LOADCFG;
						break;
					}
					case ACT_GAME_SCREENSHOT:
					{
						mode = KX_GameActuator::KX_GAME_SCREENSHOT;
						filename = gameact->filename;
						break;
					}
				default:
					; /* flag error */
				}
				tmpgameact = new KX_GameActuator(
				            gameobj,
				            mode,
				            filename,
				            loadinganimationname,
				            scene,
				            ketsjiEngine);
				baseact = tmpgameact;

				break;
			}
		case ACT_RANDOM:
			{
				bRandomActuator *randAct 
					= (bRandomActuator *) bact->data;
				
				unsigned long seedArg = randAct->seed;
				if (seedArg == 0)
				{
					seedArg = (int)(ketsjiEngine->GetRealTime()*100000.0);
					seedArg ^= (intptr_t)randAct;
				}
				SCA_RandomActuator::KX_RANDOMACT_MODE modeArg 
					= SCA_RandomActuator::KX_RANDOMACT_NODEF;
				SCA_RandomActuator *tmprandomact;
				float paraArg1 = 0.0;
				float paraArg2 = 0.0;
				
				switch (randAct->distribution) {
				case ACT_RANDOM_BOOL_CONST:
					modeArg = SCA_RandomActuator::KX_RANDOMACT_BOOL_CONST;
					paraArg1 = (float) randAct->int_arg_1;
					break;
				case ACT_RANDOM_BOOL_UNIFORM:
					modeArg = SCA_RandomActuator::KX_RANDOMACT_BOOL_UNIFORM;
					break;
				case ACT_RANDOM_BOOL_BERNOUILLI:
					paraArg1 = randAct->float_arg_1;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_BOOL_BERNOUILLI;
					break;
				case ACT_RANDOM_INT_CONST:
					modeArg = SCA_RandomActuator::KX_RANDOMACT_INT_CONST;
					paraArg1 = (float) randAct->int_arg_1;
					break;
				case ACT_RANDOM_INT_UNIFORM:
					paraArg1 = (float) randAct->int_arg_1;
					paraArg2 = (float) randAct->int_arg_2;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_INT_UNIFORM;
					break;
				case ACT_RANDOM_INT_POISSON:
					paraArg1 = randAct->float_arg_1;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_INT_POISSON;
					break;
				case ACT_RANDOM_FLOAT_CONST:
					paraArg1 = randAct->float_arg_1;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_FLOAT_CONST;
					break;
				case ACT_RANDOM_FLOAT_UNIFORM:
					paraArg1 = randAct->float_arg_1;
					paraArg2 = randAct->float_arg_2;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_FLOAT_UNIFORM;
					break;
				case ACT_RANDOM_FLOAT_NORMAL:
					paraArg1 = randAct->float_arg_1;
					paraArg2 = randAct->float_arg_2;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_FLOAT_NORMAL;
					break;
				case ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL:
					paraArg1 = randAct->float_arg_1;
					modeArg = SCA_RandomActuator::KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL;
					break;
				default:
					; /* error */
				}
				tmprandomact = new SCA_RandomActuator(
				            gameobj,
				            seedArg,
				            modeArg,
				            paraArg1,
				            paraArg2,
				            randAct->propname);
				baseact = tmprandomact;
			}
			break;

		case ACT_VISIBILITY:
		{
			bVisibilityActuator *vis_act = (bVisibilityActuator *) bact->data;
			KX_VisibilityActuator * tmp_vis_act = NULL;
			bool v = ((vis_act->flag & ACT_VISIBILITY_INVISIBLE) != 0);
			bool o = ((vis_act->flag & ACT_VISIBILITY_OCCLUSION) != 0);
			bool recursive = ((vis_act->flag & ACT_VISIBILITY_RECURSIVE) != 0);

			tmp_vis_act = new KX_VisibilityActuator(gameobj, !v, o, recursive);
			
			baseact = tmp_vis_act;
		}
		break;

		case ACT_STATE:
		{
			bStateActuator *sta_act = (bStateActuator *) bact->data;
			KX_StateActuator * tmp_sta_act = NULL;

			tmp_sta_act = 
				new KX_StateActuator(gameobj, sta_act->type, sta_act->mask);
			
			baseact = tmp_sta_act;
		}
		break;

		case ACT_2DFILTER:
		{
			bTwoDFilterActuator *_2dfilter = (bTwoDFilterActuator*) bact->data;
			SCA_2DFilterActuator *tmp = NULL;

			RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode;
			switch (_2dfilter->type) {
				case ACT_2DFILTER_MOTIONBLUR:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_MOTIONBLUR;
					break;
				case ACT_2DFILTER_BLUR:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_BLUR;
					break;
				case ACT_2DFILTER_SHARPEN:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_SHARPEN;
					break;
				case ACT_2DFILTER_DILATION:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_DILATION;
					break;
				case ACT_2DFILTER_EROSION:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_EROSION;
					break;
				case ACT_2DFILTER_LAPLACIAN:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_LAPLACIAN;
					break;
				case ACT_2DFILTER_SOBEL:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_SOBEL;
					break;
				case ACT_2DFILTER_PREWITT:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_PREWITT;
					break;
				case ACT_2DFILTER_GRAYSCALE:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_GRAYSCALE;
					break;
				case ACT_2DFILTER_SEPIA:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_SEPIA;
					break;
				case ACT_2DFILTER_INVERT:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_INVERT;
					break;
				case ACT_2DFILTER_CUSTOMFILTER:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_CUSTOMFILTER;
					break;
				case ACT_2DFILTER_NOFILTER:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_NOFILTER;
					break;
				case ACT_2DFILTER_DISABLED:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_DISABLED;
					break;
				case ACT_2DFILTER_ENABLED:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_ENABLED;
					break;
				default:
					filtermode = RAS_2DFilterManager::RAS_2DFILTER_NOFILTER;
					break;
			}

			tmp = new SCA_2DFilterActuator(gameobj, filtermode,  _2dfilter->flag,
			                               _2dfilter->float_arg, _2dfilter->int_arg,
			                               ketsjiEngine->GetRasterizer(), scene);

			if (_2dfilter->text)
			{
				char *buf;
				// this is some blender specific code
				buf = txt_to_buf(_2dfilter->text);
				if (buf)
				{
					tmp->SetShaderText(buf);
					MEM_freeN(buf);
				}
			}

			baseact = tmp;

		}
		break;
		case ACT_PARENT:
			{
				bParentActuator *parAct = (bParentActuator *) bact->data;
				int mode = KX_ParentActuator::KX_PARENT_NODEF;
				bool addToCompound = true;
				bool ghost = true;
				KX_GameObject *tmpgob = NULL;

				switch (parAct->type) {
					case ACT_PARENT_SET:
						mode = KX_ParentActuator::KX_PARENT_SET;
						tmpgob = converter->FindGameObject(parAct->ob);
						addToCompound = !(parAct->flag & ACT_PARENT_COMPOUND);
						ghost = !(parAct->flag & ACT_PARENT_GHOST);
						break;
					case ACT_PARENT_REMOVE:
						mode = KX_ParentActuator::KX_PARENT_REMOVE;
						tmpgob = NULL;
						break;
				}
	
				KX_ParentActuator *tmpparact
					= new KX_ParentActuator(gameobj,
					mode,
					addToCompound,
					ghost,
					tmpgob);
				baseact = tmpparact;
				break;
			}
		
		case ACT_ARMATURE:
			{
				bArmatureActuator* armAct = (bArmatureActuator*) bact->data;
				KX_GameObject *tmpgob = converter->FindGameObject(armAct->target);
				KX_GameObject *subgob = converter->FindGameObject(armAct->subtarget);
				BL_ArmatureActuator* tmparmact = new BL_ArmatureActuator(
				            gameobj,
				            armAct->type,
				            armAct->posechannel,
				            armAct->constraint,
				            tmpgob,
				            subgob,
				            armAct->weight,
				            armAct->influence);
				baseact = tmparmact;
				break;
			}
		case ACT_STEERING:
			{
				bSteeringActuator *stAct = (bSteeringActuator *) bact->data;
				KX_GameObject *navmeshob = NULL;
				if (stAct->navmesh)
				{
					PointerRNA settings_ptr;
					RNA_pointer_create((ID *)stAct->navmesh, &RNA_GameObjectSettings, stAct->navmesh, &settings_ptr);
					if (RNA_enum_get(&settings_ptr, "physics_type") == OB_BODY_TYPE_NAVMESH)
						navmeshob = converter->FindGameObject(stAct->navmesh);
				}
				KX_GameObject *targetob = converter->FindGameObject(stAct->target);

				int mode = KX_SteeringActuator::KX_STEERING_NODEF;
				switch (stAct->type) {
				case ACT_STEERING_SEEK:
					mode = KX_SteeringActuator::KX_STEERING_SEEK;
					break;
				case ACT_STEERING_FLEE:
					mode = KX_SteeringActuator::KX_STEERING_FLEE;
					break;
				case ACT_STEERING_PATHFOLLOWING:
					mode = KX_SteeringActuator::KX_STEERING_PATHFOLLOWING;
					break;
				}

				bool selfTerminated = (stAct->flag & ACT_STEERING_SELFTERMINATED) !=0;
				bool enableVisualization = (stAct->flag & ACT_STEERING_ENABLEVISUALIZATION) !=0;
				short facingMode = (stAct->flag & ACT_STEERING_AUTOMATICFACING) ? stAct->facingaxis : 0;
				bool normalup = (stAct->flag & ACT_STEERING_NORMALUP) !=0;
				bool lockzvel = (stAct->flag & ACT_STEERING_LOCKZVEL) !=0;
				KX_SteeringActuator *tmpstact
					= new KX_SteeringActuator(gameobj, mode, targetob, navmeshob,stAct->dist, 
					stAct->velocity, stAct->acceleration, stAct->turnspeed, 
					selfTerminated, stAct->updateTime,
					scene->GetObstacleSimulation(), facingMode, normalup, enableVisualization, lockzvel);
				baseact = tmpstact;
				break;
			}
		case ACT_MOUSE:
			{
				bMouseActuator* mouAct = (bMouseActuator*) bact->data;
				int mode = KX_MouseActuator::KX_ACT_MOUSE_NODEF;

				switch (mouAct->type) {
					case ACT_MOUSE_VISIBILITY:
					{
						mode = KX_MouseActuator::KX_ACT_MOUSE_VISIBILITY;
						break;
					}
					case ACT_MOUSE_LOOK:
					{
						mode = KX_MouseActuator::KX_ACT_MOUSE_LOOK;
						break;
					}
				}

				bool visible = (mouAct->flag & ACT_MOUSE_VISIBLE) != 0;
				bool use_axis[2] = {(mouAct->flag & ACT_MOUSE_USE_AXIS_X) != 0, (mouAct->flag & ACT_MOUSE_USE_AXIS_Y) != 0};
				bool reset[2] = {(mouAct->flag & ACT_MOUSE_RESET_X) != 0, (mouAct->flag & ACT_MOUSE_RESET_Y) != 0};
				bool local[2] = {(mouAct->flag & ACT_MOUSE_LOCAL_X) != 0, (mouAct->flag & ACT_MOUSE_LOCAL_Y) != 0};

				SCA_MouseManager* eventmgr = (SCA_MouseManager*) logicmgr->FindEventManager(SCA_EventManager::MOUSE_EVENTMGR);
				if (eventmgr) {
					KX_MouseActuator* tmpbaseact = new KX_MouseActuator(gameobj,
																		ketsjiEngine,
																		eventmgr,
																		mode,
																		visible,
																		use_axis,
																		mouAct->threshold,
																		reset,
																		mouAct->object_axis,
																		local,
																		mouAct->sensitivity,
																		mouAct->limit_x,
																		mouAct->limit_y);
					baseact = tmpbaseact;
				} else {
					//cout << "\n Couldn't find mouse event manager..."; - should throw an error here...
				}
				break;
			}
		default:
			; /* generate some error */
		}
		
		if (baseact && !(bact->flag & ACT_DEACTIVATE))
		{
			baseact->SetExecutePriority(executePriority++);
			uniquename += "#ACT#";
			uniqueint++;
			CIntValue* uniqueval = new CIntValue(uniqueint);
			uniquename += uniqueval->GetText();
			uniqueval->Release();
			baseact->SetName(bact->name);
			baseact->SetLogicManager(logicmgr);
			//gameobj->SetProperty(uniquename,baseact);
			gameobj->AddActuator(baseact);
			
			converter->RegisterGameActuator(baseact, bact);
			// done with baseact, release it
			baseact->Release();
		}
		else if (baseact)
			baseact->Release();
		
		bact = bact->next;
	}
}


