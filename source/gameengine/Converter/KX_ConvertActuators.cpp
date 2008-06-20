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
* Convert Blender actuators for use in the GameEngine
*/

#ifdef WIN32
#pragma warning (disable : 4786) 
#endif //WIN32

#define BLENDER_HACK_DTIME 0.02

#include "MEM_guardedalloc.h"

#include "KX_BlenderSceneConverter.h"
#include "KX_ConvertActuators.h"

// Actuators
//SCA logiclibrary native logicbricks
#include "SCA_PropertyActuator.h"
#include "SCA_LogicManager.h"
#include "SCA_RandomActuator.h"
#include "SCA_2DFilterActuator.h"

// Ketsji specific logicbricks
#include "KX_SceneActuator.h"
#include "KX_IpoActuator.h"
#include "KX_SoundActuator.h"
#include "KX_CDActuator.h"
#include "KX_ObjectActuator.h"
#include "KX_TrackToActuator.h"
#include "KX_ConstraintActuator.h"
#include "KX_CameraActuator.h"
#include "KX_GameActuator.h"
#include "KX_VisibilityActuator.h"
#include "KX_SCA_AddObjectActuator.h"
#include "KX_SCA_EndObjectActuator.h"
#include "KX_SCA_ReplaceMeshActuator.h"
#include "KX_ParentActuator.h"

#include "KX_Scene.h"
#include "KX_KetsjiEngine.h"

#include "IntValue.h"
#include "KX_GameObject.h"

/* This little block needed for linking to Blender... */
#include "BKE_text.h"
#include "BLI_blenlib.h"

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
/* end of blender include block */

#include "BL_BlenderDataConversion.h"

/** 
KX_BLENDERTRUNC needed to round 'almost' zero values to zero, else velocities etc. are incorrectly set 
*/

#define KX_BLENDERTRUNC(x)  (( x < 0.0001 && x > -0.0001 )  ? 0.0 : x)

void BL_ConvertActuators(char* maggiename,
						 struct Object* blenderobject,
						 KX_GameObject* gameobj,
						 SCA_LogicManager* logicmgr,
						 KX_Scene* scene,
						 KX_KetsjiEngine* ketsjiEngine,
						 int & executePriority, 
						 int activeLayerBitInfo,
						 bool isInActiveLayer,
						 RAS_IRenderTools* rendertools,
						 KX_BlenderSceneConverter* converter
						 )
{
	
	int uniqueint = 0;
	bActuator* bact = (bActuator*) blenderobject->actuators.first;
	while(bact)
	{
		STR_String uniquename = bact->name;
		STR_String objectname = gameobj->GetName();
		
		SCA_IActuator* baseact = NULL;
		switch (bact->type)
		{
		case ACT_OBJECT:
			{
				bObjectActuator* obact = (bObjectActuator*) bact->data;
				MT_Vector3 forcevec(KX_BLENDERTRUNC(obact->forceloc[0]),
					KX_BLENDERTRUNC(obact->forceloc[1]),
					KX_BLENDERTRUNC(obact->forceloc[2]));
				MT_Vector3 torquevec(obact->forcerot[0],obact->forcerot[1],obact->forcerot[2]);
				MT_Vector3 dlocvec ( KX_BLENDERTRUNC(obact->dloc[0]),
					KX_BLENDERTRUNC(obact->dloc[1]),
					KX_BLENDERTRUNC(obact->dloc[2]));
				MT_Vector3 drotvec ( KX_BLENDERTRUNC(obact->drot[0]),obact->drot[1],obact->drot[2]);
				MT_Vector3 linvelvec ( KX_BLENDERTRUNC(obact->linearvelocity[0]),
					KX_BLENDERTRUNC(obact->linearvelocity[1]),
					KX_BLENDERTRUNC(obact->linearvelocity[2]));
				MT_Vector3 angvelvec ( KX_BLENDERTRUNC(obact->angularvelocity[0]),
					KX_BLENDERTRUNC(obact->angularvelocity[1]),
					KX_BLENDERTRUNC(obact->angularvelocity[2]));
				
				drotvec /=		BLENDER_HACK_DTIME;
				//drotvec /=		BLENDER_HACK_DTIME;
				drotvec *= MT_2_PI/360.0;
				//dlocvec /= BLENDER_HACK_DTIME;
				//linvelvec /=	BLENDER_HACK_DTIME;
				//angvelvec /=	BLENDER_HACK_DTIME;
				
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
				
				bitLocalFlag.AddOrSetLinV = bool((obact->flag & ACT_ADD_LIN_VEL)!=0);
				
				
				KX_ObjectActuator* tmpbaseact = new KX_ObjectActuator(gameobj,
					forcevec.getValue(),
					torquevec.getValue(),
					dlocvec.getValue(),
					drotvec.getValue(),
					linvelvec.getValue(),
					angvelvec.getValue(),
					bitLocalFlag
					);
				baseact = tmpbaseact;
				break;
			}
		case ACT_ACTION:
			{
				if (blenderobject->type==OB_ARMATURE){
					bActionActuator* actact = (bActionActuator*) bact->data;
					STR_String propname = (actact->name ? actact->name : "");
					
					BL_ActionActuator* tmpbaseact = new BL_ActionActuator(
						gameobj,
						propname,
						actact->sta,
						actact->end,
						actact->act,
						actact->type, // + 1, because Blender starts to count at zero,
						actact->blendin,
						actact->priority,
						actact->stridelength
						// Ketsji at 1, because zero is reserved for "NoDef"
						);
					baseact= tmpbaseact;
					break;
				}
				else
					printf ("Discarded action actuator from non-armature object [%s]\n", blenderobject->id.name+2);
			}
		case ACT_SHAPEACTION:
			{
				if (blenderobject->type==OB_MESH){
					bActionActuator* actact = (bActionActuator*) bact->data;
					STR_String propname = (actact->name ? actact->name : "");
					
					BL_ShapeActionActuator* tmpbaseact = new BL_ShapeActionActuator(
						gameobj,
						propname,
						actact->sta,
						actact->end,
						actact->act,
						actact->type, // + 1, because Blender starts to count at zero,
						actact->blendin,
						actact->priority,
						actact->stridelength
						// Ketsji at 1, because zero is reserved for "NoDef"
						);
					baseact= tmpbaseact;
					break;
				}
				else
					printf ("Discarded shape action actuator from non-mesh object [%s]\n", blenderobject->id.name+2);
			}
		case ACT_IPO:
			{
				bIpoActuator* ipoact = (bIpoActuator*) bact->data;
				bool ipochild = (ipoact->flag & ACT_IPOCHILD) !=0;
				STR_String propname = ( ipoact->name ? ipoact->name : "");
				// first bit?
				bool ipo_as_force = (ipoact->flag & ACT_IPOFORCE);
				bool force_local = (ipoact->flag & ACT_IPOFORCE_LOCAL);
				
				KX_IpoActuator* tmpbaseact = new KX_IpoActuator(
					gameobj,
					propname ,
					ipoact->sta,
					ipoact->end,
					ipochild,
					ipoact->type + 1, // + 1, because Blender starts to count at zero,
					// Ketsji at 1, because zero is reserved for "NoDef"
					ipo_as_force,
					force_local
					);
				baseact = tmpbaseact;
				break;
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
					KX_CameraActuator *tmpcamact
						= new KX_CameraActuator(gameobj,
						tmpgob,
						camact->height,
						camact->min,
						camact->max,
						camact->axis=='x');
					baseact = tmpcamact;
				}
				break;
			}
		case ACT_MESSAGE:
			{
				bMessageActuator *msgAct = (bMessageActuator *) bact->data;
				
				/**
				* Get the name of the properties that objects must own that
				* we're sending to, if present
				*/
				STR_String toPropName = (msgAct->toPropName
					? (char*) msgAct->toPropName
					: "");
					/**
					* Get the Message Subject to send.
				*/
				STR_String subject = (msgAct->subject
					? (char*) msgAct->subject
					: "");
				
					/**
					* Get the bodyType
				*/
				int bodyType = msgAct->bodyType;
				
				/**
				* Get the body (text message or property name whose value
				* we'll be sending, might be empty
				*/
				STR_String body = (msgAct->body
					? (char*) msgAct->body
					: "");
				
				KX_NetworkMessageActuator *tmpmsgact = 
					new KX_NetworkMessageActuator(
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
				short startFrame = soundact->sta, stopFrame = soundact->end;
				KX_SoundActuator::KX_SOUNDACT_TYPE 
					soundActuatorType = KX_SoundActuator::KX_SOUNDACT_NODEF;
				
				switch(soundact->type) {
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
					SND_SoundObject* sndobj = NULL;
					
					if (soundact->sound)
					{
						SND_Scene* soundscene = scene->GetSoundScene();
						STR_String samplename = soundact->sound->name;
						
						bool sampleisloaded = false;
						
						/* let's see if the sample was already loaded */
						if (soundscene->IsSampleLoaded(samplename))
						{
							sampleisloaded = true;
						}
						else
						{
							/* if not, make it so */
							PackedFile* pf = soundact->sound->newpackedfile;
							
							/* but we need a packed file then */
							if (pf)
							{
								if (soundscene->LoadSample(samplename, pf->data, pf->size) > -1)
									sampleisloaded = true;
							}
							/* or else load it from disk */
							else
							{
								/* but we need to convert the samplename into absolute pathname first */
								BLI_convertstringcode(soundact->sound->name, maggiename);
								samplename = soundact->sound->name;
								
								/* and now we can load it */
								if (soundscene->LoadSample(samplename, NULL, 0) > -1)
									sampleisloaded = true;
							}
						}
						
						if (sampleisloaded)
						{
							sndobj = new SND_SoundObject();
							sndobj->SetSampleName(samplename.Ptr());
							sndobj->SetObjectName(bact->name);
							sndobj->SetRollOffFactor(soundact->sound->attenuation);
							sndobj->SetGain(soundact->sound->volume);
							sndobj->SetPitch(exp((soundact->sound->pitch / 12.0) * log(2.0)));
							//							sndobj->SetLoopStart(soundact->sound->loopstart);
							//							sndobj->SetLoopStart(soundact->sound->loopend);
							if (soundact->sound->flags & SOUND_FLAGS_LOOP)
							{
								if (soundact->sound->flags & SOUND_FLAGS_BIDIRECTIONAL_LOOP)
									sndobj->SetLoopMode(SND_LOOP_BIDIRECTIONAL);
								else
									sndobj->SetLoopMode(SND_LOOP_NORMAL);
							}
							else
								sndobj->SetLoopMode(SND_LOOP_OFF);
							
							if (soundact->sound->flags & SOUND_FLAGS_PRIORITY)
								sndobj->SetHighPriority(true);
							else
								sndobj->SetHighPriority(false);
							
							if (soundact->sound->flags & SOUND_FLAGS_3D)
								sndobj->Set3D(true);
							else
								sndobj->Set3D(false);
							
							KX_SoundActuator* tmpsoundact = 
								new KX_SoundActuator(gameobj, 
								sndobj,
								scene->GetSoundScene(), // needed for replication!
								soundActuatorType,
								startFrame,
								stopFrame);
							
							tmpsoundact->SetName(bact->name);
							baseact = tmpsoundact;
							soundscene->AddObject(sndobj);
						} else {
							std::cout << "WARNING: Sound actuator " << bact->name << " failed to load sample." << std::endl;
						}
					}
				}
				break;
			}
		case ACT_CD:
			{
				bCDActuator* cdact = (bCDActuator*) bact->data;
				/* get type, and possibly a start and end frame */
				short startFrame = cdact->sta, stopFrame = cdact->end;
				KX_CDActuator::KX_CDACT_TYPE 
					cdActuatorType = KX_CDActuator::KX_CDACT_NODEF;
				
				switch(cdact->type)
				{
				case ACT_CD_PLAY_ALL:
					cdActuatorType = KX_CDActuator::KX_CDACT_PLAY_ALL;
					break;
				case ACT_CD_PLAY_TRACK:
					cdActuatorType = KX_CDActuator::KX_CDACT_PLAY_TRACK;
					break;
				case ACT_CD_LOOP_TRACK:
					cdActuatorType = KX_CDActuator::KX_CDACT_LOOP_TRACK;
					break;
				case ACT_CD_VOLUME:
					cdActuatorType = KX_CDActuator::KX_CDACT_VOLUME;
					break;
				case ACT_CD_STOP:
					cdActuatorType = KX_CDActuator::KX_CDACT_STOP;
					break;
				case ACT_CD_PAUSE:
					cdActuatorType = KX_CDActuator::KX_CDACT_PAUSE;
					break;
				case ACT_CD_RESUME:
					cdActuatorType = KX_CDActuator::KX_CDACT_RESUME;
					break;
					
				default:
					/* This is an error!!! */
					cdActuatorType = KX_CDActuator::KX_CDACT_NODEF;
				}
				
				if (cdActuatorType != KX_CDActuator::KX_CDACT_NODEF) 
				{
					SND_CDObject* pCD = SND_CDObject::Instance();
					
					if (pCD)
					{
						pCD->SetGain(cdact->volume);
						
						KX_CDActuator* tmpcdact = 
							new KX_CDActuator(gameobj, 
							scene->GetSoundScene(), // needed for replication!
							cdActuatorType,
							cdact->track,
							startFrame,
							stopFrame);
						
						tmpcdact->SetName(bact->name);
						baseact = tmpcdact;
					}
				}
				break;
			}
		case ACT_PROPERTY:
			{
				bPropertyActuator* propact = (bPropertyActuator*) bact->data;
				CValue* destinationObj = NULL;
				
				/*
				here the destinationobject is searched. problem with multiple scenes: other scenes
				have not been converted yet, so the destobj will not be found, so the prop will
				not be copied.
				possible solutions:
				- convert everything when possible and not realtime only when needed.
				- let the object-with-property report itself to the act when converted
				*/
				if (propact->ob)
					destinationObj = converter->FindGameObject(propact->ob);
				
				SCA_PropertyActuator* tmppropact = new SCA_PropertyActuator(
					gameobj,
					destinationObj,
					propact->name,
					propact->value,
					propact->type+1); // + 1 because Ketsji Logic starts
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
						CValue* originalval = NULL;
						if (editobact->ob && !(editobact->ob->lay & activeLayerBitInfo))
							originalval = converter->FindGameObject(editobact->ob);
						
						MT_Vector3 linvelvec ( KX_BLENDERTRUNC(editobact->linVelocity[0]),
							KX_BLENDERTRUNC(editobact->linVelocity[1]),
							KX_BLENDERTRUNC(editobact->linVelocity[2]));
							
						KX_SCA_AddObjectActuator* tmpaddact = 
							new KX_SCA_AddObjectActuator(
								gameobj, 
								originalval,
								editobact->time,
								scene,
								linvelvec.getValue(),
								editobact->localflag!=0
								);
								
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
						RAS_MeshObject *tmpmesh = NULL;
						if (editobact->me)
							tmpmesh = BL_ConvertMesh(
								editobact->me,
								blenderobject,
								rendertools,
								scene,
								converter
								);

						KX_SCA_ReplaceMeshActuator* tmpreplaceact
							= new KX_SCA_ReplaceMeshActuator(
								gameobj,
								tmpmesh,
								scene
								);
							
							baseact = tmpreplaceact;
					}
					break;
				case ACT_EDOB_TRACK_TO:
					{
						SCA_IObject* originalval = NULL;
						if (editobact->ob)
							originalval = converter->FindGameObject(editobact->ob);
							
						KX_TrackToActuator* tmptrackact 
							= new KX_TrackToActuator(gameobj, 
								originalval,
								editobact->time,
								editobact->flag,
								blenderobject->trackflag,
								blenderobject->upflag
								);
							baseact = tmptrackact;
					}
				}
				break;
			}
		case ACT_CONSTRAINT:
			{
				float min = 0.0, max = 0.0;
				KX_ConstraintActuator::KX_CONSTRAINTTYPE locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_NODEF;
				bConstraintActuator *conact 
					= (bConstraintActuator*) bact->data;
				/* convert settings... degrees in the ui become radians  */ 
				/* internally                                            */ 
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
					min = MT_2_PI * conact->minrot[0] / 360.0;
					max = MT_2_PI * conact->maxrot[0] / 360.0;
					break;
				case ACT_CONST_ROTY:
					locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTY;
					min = MT_2_PI * conact->minrot[1] / 360.0;
					max = MT_2_PI * conact->maxrot[1] / 360.0;
					break;
				case ACT_CONST_ROTZ:
					locrot = KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTZ;
					min = MT_2_PI * conact->minrot[2] / 360.0;
					max = MT_2_PI * conact->maxrot[2] / 360.0;
					break;
				default:
					; /* error */ 
				}
				KX_ConstraintActuator *tmpconact 
					= new KX_ConstraintActuator(gameobj,
					conact->damp,
					min,
					max,
					locrot);
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
						
						if (sceneact->scene)
						{
							nextSceneName = sceneact->scene->id.name + 2; // this '2' is necessary to remove prefix 'SC'
						}
						
						break;
					}
				case ACT_SCENE_CAMERA:
					mode = KX_SceneActuator::KX_SCENE_SET_CAMERA;
					if (sceneact->camera)
					{
						cam = (KX_Camera*) converter->FindGameObject(sceneact->camera);
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
				tmpsceneact = new KX_SceneActuator(gameobj,
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
				default:
					; /* flag error */
				}
					tmpgameact = new KX_GameActuator(gameobj,
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
				SCA_RandomActuator::KX_RANDOMACT_MODE modeArg 
					= SCA_RandomActuator::KX_RANDOMACT_NODEF;
				SCA_RandomActuator *tmprandomact;
				float paraArg1 = 0.0;
				float paraArg2 = 0.0;
				
				switch  (randAct->distribution) {
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
				tmprandomact = new SCA_RandomActuator(gameobj,
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

			tmp_vis_act = 
				new KX_VisibilityActuator(gameobj,
							  !v);
			
			baseact = tmp_vis_act;
		}
		break;
		
		case ACT_2DFILTER:
		{
			bTwoDFilterActuator *_2dfilter = (bTwoDFilterActuator*) bact->data;
            SCA_2DFilterActuator *tmp = NULL;

			RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode;
			switch(_2dfilter->type)
			{
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
            
			tmp = new SCA_2DFilterActuator(gameobj, filtermode, _2dfilter->flag,
				_2dfilter->float_arg,_2dfilter->int_arg,ketsjiEngine->GetRasterizer(),rendertools);

			if (_2dfilter->text)
			{
				char *buf;
				// this is some blender specific code
				buf = txt_to_buf(_2dfilter->text);
				if (buf)
				{
					tmp->SetShaderText(STR_String(buf));
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
				KX_GameObject *tmpgob;

				switch(parAct->type)
				{
					case ACT_PARENT_SET:
						mode = KX_ParentActuator::KX_PARENT_SET;
						tmpgob = converter->FindGameObject(parAct->ob);
						break;
					case ACT_PARENT_REMOVE:
						mode = KX_ParentActuator::KX_PARENT_REMOVE;
						tmpgob = NULL;
						break;
				}
	
				KX_ParentActuator *tmpparact
					= new KX_ParentActuator(gameobj,
					mode,
					tmpgob);
				baseact = tmpparact;
				break;
			}
		
		default:
			; /* generate some error */
		}
		
		if (baseact)
		{
			baseact->SetExecutePriority(executePriority++);
			uniquename += "#ACT#";
			uniqueint++;
			CIntValue* uniqueval = new CIntValue(uniqueint);
			uniquename += uniqueval->GetText();
			uniqueval->Release();
			baseact->SetName(STR_String(bact->name));
			//gameobj->SetProperty(uniquename,baseact);
			gameobj->AddActuator(baseact);
			
			converter->RegisterGameActuator(baseact, bact);
			// done with baseact, release it
			baseact->Release();
		}
		
		bact = bact->next;
	}
}


