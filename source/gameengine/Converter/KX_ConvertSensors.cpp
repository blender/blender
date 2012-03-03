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
 * Conversion of Blender data blocks to KX sensor system
 */

/** \file gameengine/Converter/KX_ConvertSensors.cpp
 *  \ingroup bgeconv
 */


#include <stdio.h>

#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning (disable : 4786)
#endif //WIN32

#include "wm_event_types.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_ConvertSensors.h"

/* This little block needed for linking to Blender... */
#if defined(WIN32) && !defined(FREE_WINDOWS)
#include "BLI_winstuff.h"
#endif

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_sensor_types.h"
#include "DNA_actuator_types.h" /* for SENS_ALL_KEYS ? this define is
probably misplaced */
/* end of blender include block */

#include "RAS_IPolygonMaterial.h"
// Sensors
#include "KX_GameObject.h"
#include "RAS_MeshObject.h"
#include "SCA_KeyboardSensor.h"
#include "SCA_MouseSensor.h"
#include "SCA_AlwaysSensor.h"
#include "KX_TouchSensor.h"
#include "KX_NearSensor.h"
#include "KX_RadarSensor.h"
#include "KX_MouseFocusSensor.h"
#include "KX_ArmatureSensor.h"
#include "SCA_JoystickSensor.h"
#include "KX_NetworkMessageSensor.h"
#include "SCA_ActuatorSensor.h"
#include "SCA_DelaySensor.h"


#include "SCA_PropertySensor.h"
#include "SCA_RandomSensor.h"
#include "KX_RaySensor.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
#include "KX_BlenderInputDevice.h"
#include "KX_Scene.h"
#include "IntValue.h"
#include "KX_BlenderKeyboardDevice.h"
#include "KX_BlenderGL.h"
#include "RAS_ICanvas.h"
#include "PHY_IPhysicsEnvironment.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"
#include "BL_BlenderDataConversion.h"

void BL_ConvertSensors(struct Object* blenderobject,
					   class KX_GameObject* gameobj,
					   SCA_LogicManager* logicmgr,
					   KX_Scene* kxscene,
					   KX_KetsjiEngine* kxengine,
					   int activeLayerBitInfo,
					   bool isInActiveLayer,
					   RAS_ICanvas* canvas,
					   KX_BlenderSceneConverter* converter
					   )
{

	int executePriority = 0;
	int uniqueint = 0;
	int count = 0;
	bSensor* sens = (bSensor*)blenderobject->sensors.first;
	bool pos_pulsemode = false;
	bool neg_pulsemode = false;
	int frequency = 0;
	bool invert = false;
	bool level = false;
	bool tap = false;
	
	while (sens)
	{
		sens = sens->next;
		count++;
	}
	gameobj->ReserveSensor(count);
	sens = (bSensor*)blenderobject->sensors.first;
	while(sens)
	{
		SCA_ISensor* gamesensor=NULL;
		/* All sensors have a pulse toggle, frequency, and invert field.     */
		/* These are extracted here, and set when the sensor is added to the */
		/* list.                                                             */
		pos_pulsemode = (sens->pulse & SENS_PULSE_REPEAT)!=0;
		neg_pulsemode = (sens->pulse & SENS_NEG_PULSE_MODE)!=0;
		
		frequency = sens->freq;
		invert    = !(sens->invert == 0);
		level     = !(sens->level == 0);
		tap       = !(sens->tap == 0);

		switch (sens->type)
		{
		case  SENS_ALWAYS:
			{
				
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
				if (eventmgr)
				{
					gamesensor = new SCA_AlwaysSensor(eventmgr, gameobj);
				}
				
				break;
			}
			
		case  SENS_DELAY:
			{
				// we can reuse the Always event manager for the delay sensor
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
				if (eventmgr)
				{
					bDelaySensor* delaysensor = (bDelaySensor*)sens->data;
					gamesensor = new SCA_DelaySensor(eventmgr, 
						gameobj,
						delaysensor->delay,
						delaysensor->duration,
						(delaysensor->flag & SENS_DELAY_REPEAT) != 0);
				}
				break;
			}

		case SENS_COLLISION:
			{
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::TOUCH_EVENTMGR);
				if (eventmgr)
				{
					// collision sensor can sense both materials and properties. 
					
					bool bFindMaterial = false, bTouchPulse = false;
					
					bCollisionSensor* blendertouchsensor = (bCollisionSensor*)sens->data;
					
					bFindMaterial = (blendertouchsensor->mode & SENS_COLLISION_MATERIAL);
					bTouchPulse = (blendertouchsensor->mode & SENS_COLLISION_PULSE);
					
					
					STR_String touchPropOrMatName = ( bFindMaterial ? 
						blendertouchsensor->materialName:
					(blendertouchsensor->name ? blendertouchsensor->name: ""));
					
					
					if (gameobj->GetPhysicsController())
					{	
						gamesensor = new KX_TouchSensor(eventmgr,
							gameobj,
							bFindMaterial,
							bTouchPulse,
							touchPropOrMatName);
					}
					
				}
				
				break;
			}
		case SENS_TOUCH:
			{
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::TOUCH_EVENTMGR);
				if (eventmgr)
				{
					STR_String touchpropertyname;
					bTouchSensor* blendertouchsensor = (bTouchSensor*)sens->data;
					
					if (blendertouchsensor->ma)
					{
						touchpropertyname = (char*) (blendertouchsensor->ma->id.name+2);
					}
					bool bFindMaterial = true;
					if (gameobj->GetPhysicsController())
					{	
						gamesensor = new KX_TouchSensor(eventmgr,
							gameobj,
							bFindMaterial,
							false,
							touchpropertyname);
					}
				}
				break;
			}
		case SENS_MESSAGE:
			{
				KX_NetworkEventManager* eventmgr = (KX_NetworkEventManager*)
					logicmgr->FindEventManager(SCA_EventManager::NETWORK_EVENTMGR);
				if (eventmgr) {
					bMessageSensor* msgSens = (bMessageSensor*) sens->data;	
					
					/* Get our NetworkScene */
					NG_NetworkScene *NetworkScene = kxscene->GetNetworkScene();
					/* filter on the incoming subjects, might be empty */
					STR_String subject = (msgSens->subject
						? (char*)msgSens->subject
						: "");
					
					gamesensor = new KX_NetworkMessageSensor(
						eventmgr,		// our eventmanager
						NetworkScene,	// our NetworkScene
						gameobj,		// the sensor controlling object
						subject);		// subject to filter on
				}
				break;
			}
		case SENS_NEAR:
			{
				
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::TOUCH_EVENTMGR);
				if (eventmgr)
				{
					STR_String nearpropertyname;	
					bNearSensor* blendernearsensor = (bNearSensor*)sens->data;
					if (blendernearsensor->name)
					{
						// only objects that own this property will be taken into account
						nearpropertyname = (char*) blendernearsensor->name;
					}
					
					//DT_ShapeHandle shape	=	DT_Sphere(0.0);
					
					// this sumoObject is not deleted by a gameobj, so delete it ourself
					// later (memleaks)!
					float radius = blendernearsensor->dist;
					PHY__Vector3 pos;
					const MT_Vector3& wpos = gameobj->NodeGetWorldPosition();
					pos[0] = (float)wpos[0];
					pos[1] = (float)wpos[1];
					pos[2] = (float)wpos[2];
					pos[3] = 0.f;
					bool bFindMaterial = false;
					PHY_IPhysicsController* physCtrl = kxscene->GetPhysicsEnvironment()->CreateSphereController(radius,pos);

					//will be done in KX_TouchEventManager::RegisterSensor()  
					//if (isInActiveLayer)
					//	kxscene->GetPhysicsEnvironment()->addSensor(physCtrl);

						

					gamesensor = new KX_NearSensor(eventmgr,gameobj,
						blendernearsensor->dist,
						blendernearsensor->resetdist,
						bFindMaterial,
						nearpropertyname,
						physCtrl);
					
				}
				break;
			}
			
			
		case SENS_KEYBOARD:
			{
				/* temporary input device, for converting the code for the keyboard sensor */
				
				bKeyboardSensor* blenderkeybdsensor = (bKeyboardSensor*)sens->data;
				SCA_KeyboardManager* eventmgr = (SCA_KeyboardManager*) logicmgr->FindEventManager(SCA_EventManager::KEYBOARD_EVENTMGR);
				if (eventmgr)
				{
					gamesensor = new SCA_KeyboardSensor(eventmgr,
						ConvertKeyCode(blenderkeybdsensor->key),
						ConvertKeyCode(blenderkeybdsensor->qual),
						ConvertKeyCode(blenderkeybdsensor->qual2),
						(blenderkeybdsensor->type == SENS_ALL_KEYS),
						blenderkeybdsensor->targetName,
						blenderkeybdsensor->toggleName,
						gameobj); //			blenderkeybdsensor->pad);
					
				} 
				
				break;
			}
		case SENS_MOUSE:
			{
				int keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_NODEF;			
				int trackfocus = 0;
				bMouseSensor *bmouse = (bMouseSensor *)sens->data;
				
				/* There are two main types of mouse sensors. If there is
				* no focus-related behavior requested, we can make do
				* with a basic sensor. This cuts down memory usage and
				* gives a slight performance gain. */
				
				SCA_MouseManager *eventmgr 
					= (SCA_MouseManager*) logicmgr->FindEventManager(SCA_EventManager::MOUSE_EVENTMGR);
				if (eventmgr) {
					
					/* Determine key mode. There is at most one active mode. */
					switch (bmouse->type) {
					case BL_SENS_MOUSE_LEFT_BUTTON:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_LEFTBUTTON;
						break;
					case BL_SENS_MOUSE_MIDDLE_BUTTON:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_MIDDLEBUTTON;
						break;
					case BL_SENS_MOUSE_RIGHT_BUTTON:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_RIGHTBUTTON;
						break;
					case BL_SENS_MOUSE_WHEEL_UP:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_WHEELUP;
						break;
					case BL_SENS_MOUSE_WHEEL_DOWN:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_WHEELDOWN;
						break;
					case BL_SENS_MOUSE_MOVEMENT:
						keytype = SCA_MouseSensor::KX_MOUSESENSORMODE_MOVEMENT;
						break;
					case BL_SENS_MOUSE_MOUSEOVER:
						trackfocus = 1;
						break;
					case BL_SENS_MOUSE_MOUSEOVER_ANY:
						trackfocus = 2;
						break;

					default:
						; /* error */
					}
					
					/* initial mouse position */				 
					int startx  = canvas->GetWidth()/2;
					int starty = canvas->GetHeight()/2;
					
					if (!trackfocus) {
						/* plain, simple mouse sensor */
						gamesensor = new SCA_MouseSensor(eventmgr,
							startx,starty,
							keytype,
							gameobj);
					} else {
						/* give us a focus-aware sensor */
						gamesensor = new KX_MouseFocusSensor(eventmgr,
							startx,
							starty,
							keytype,
							trackfocus,
							(bmouse->flag & SENS_MOUSE_FOCUS_PULSE) ? true:false,
							kxscene,
							kxengine,
							gameobj); 
					}
				} else {
					//				cout << "\n Could't find mouse event manager..."; - should throw an error here... 
				}
				break;
			}
		case SENS_PROPERTY:
			{
				bPropertySensor* blenderpropsensor = (bPropertySensor*) sens->data;
				SCA_EventManager* eventmgr 
					= logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
				if (eventmgr)
				{
					STR_String propname=blenderpropsensor->name;
					STR_String propval=blenderpropsensor->value;
					STR_String propmaxval=blenderpropsensor->maxvalue;
					
					SCA_PropertySensor::KX_PROPSENSOR_TYPE 
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_NODEF;
					
					/* Better do an explicit conversion here! (was implicit      */
					/* before...)                                                */
					switch(blenderpropsensor->type) {
					case SENS_PROP_EQUAL:
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_EQUAL;
						break;
					case SENS_PROP_NEQUAL:
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_NOTEQUAL;
						break;
					case SENS_PROP_INTERVAL:
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_INTERVAL;
						break;
					case SENS_PROP_CHANGED:
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_CHANGED;
						break;
					case SENS_PROP_EXPRESSION:
						propchecktype = SCA_PropertySensor::KX_PROPSENSOR_EXPRESSION;
						/* error */
						break;
					default:
						; /* error */
					}
					gamesensor = new SCA_PropertySensor(eventmgr,gameobj,propname,propval,propmaxval,propchecktype);
				}
				
				break;
			}
		case SENS_ACTUATOR:
			{
				bActuatorSensor* blenderactsensor = (bActuatorSensor*) sens->data;
				// we will reuse the property event manager, there is nothing special with this sensor
				SCA_EventManager* eventmgr 
					= logicmgr->FindEventManager(SCA_EventManager::ACTUATOR_EVENTMGR);
				if (eventmgr)
				{
					STR_String propname=blenderactsensor->name;
					gamesensor = new SCA_ActuatorSensor(eventmgr,gameobj,propname);
				}
				break;
			}
			
		case SENS_ARMATURE:
			{
				bArmatureSensor* blenderarmsensor = (bArmatureSensor*) sens->data;
				// we will reuse the property event manager, there is nothing special with this sensor
				SCA_EventManager* eventmgr 
					= logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
				if (eventmgr)
				{
					STR_String bonename=blenderarmsensor->posechannel;
					STR_String constraintname=blenderarmsensor->constraint;
					gamesensor = new KX_ArmatureSensor(eventmgr,gameobj,bonename,constraintname, blenderarmsensor->type, blenderarmsensor->value);
				}
				break;
			}

		case SENS_RADAR:
			{
				
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::TOUCH_EVENTMGR);
				if (eventmgr)
				{
					STR_String radarpropertyname;
					STR_String touchpropertyname;
					bRadarSensor* blenderradarsensor = (bRadarSensor*) sens->data;
					
					int radaraxis = blenderradarsensor->axis;
					
					if (blenderradarsensor->name)
					{
						// only objects that own this property will be taken into account
						radarpropertyname = (char*) blenderradarsensor->name;
					}
					
					MT_Scalar coneheight = blenderradarsensor->range;
					
					// janco: the angle was doubled, so should I divide the factor in 2
					// or the blenderradarsensor->angle?
					// nzc: the angle is the opening angle. We need to init with 
					// the axis-hull angle,so /2.0.
					MT_Scalar factor = tan(MT_radians((blenderradarsensor->angle)/2.0));
					//MT_Scalar coneradius = coneheight * (factor / 2);
					MT_Scalar coneradius = coneheight * factor;
					
					
					// this sumoObject is not deleted by a gameobj, so delete it ourself
					// later (memleaks)!
					MT_Scalar smallmargin = 0.0;
					MT_Scalar largemargin = 0.0;
					
					bool bFindMaterial = false;
					PHY_IPhysicsController* ctrl = kxscene->GetPhysicsEnvironment()->CreateConeController((float)coneradius, (float)coneheight);

					gamesensor = new KX_RadarSensor(
						eventmgr,
						gameobj,
						ctrl,
						coneradius,
						coneheight,
						radaraxis,
						smallmargin,
						largemargin,
						bFindMaterial,
						radarpropertyname);
						
				}
			
				break;
			}
		case SENS_RAY:
			{
				bRaySensor* blenderraysensor = (bRaySensor*) sens->data;
				
				//blenderradarsensor->angle;
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
				if (eventmgr)
				{
					bool bFindMaterial = (blenderraysensor->mode & SENS_COLLISION_MATERIAL);
					bool bXRay = (blenderraysensor->mode & SENS_RAY_XRAY);
					
					STR_String checkname = (bFindMaterial? blenderraysensor->matname : blenderraysensor->propname);

					// don't want to get rays of length 0.0 or so
					double distance = (blenderraysensor->range < 0.01 ? 0.01 : blenderraysensor->range );
					int axis = blenderraysensor->axisflag;

					
					gamesensor = new KX_RaySensor(eventmgr,
												  gameobj,
												  checkname,
												  bFindMaterial,
												  bXRay,
												  distance,
												  axis,
												  kxscene);

				}
				break;
			}
			
		case SENS_RANDOM:
			{
				bRandomSensor* blenderrndsensor = (bRandomSensor*) sens->data;
				// some files didn't write randomsensor, avoid crash now for NULL ptr's
				if (blenderrndsensor)
				{
					SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::BASIC_EVENTMGR);
					if (eventmgr)
					{
						int randomSeed = blenderrndsensor->seed;
						if (randomSeed == 0)
						{
							randomSeed = (int)(kxengine->GetRealTime()*100000.0);
							randomSeed ^= (intptr_t)blenderrndsensor;
						}
						gamesensor = new SCA_RandomSensor(eventmgr, gameobj, randomSeed);
					}
				}
				break;
			}
		case SENS_JOYSTICK:
			{
				int joysticktype = SCA_JoystickSensor::KX_JOYSENSORMODE_NODEF;
				
				bJoystickSensor* bjoy = (bJoystickSensor*) sens->data;
				
				SCA_JoystickManager *eventmgr 
					= (SCA_JoystickManager*) logicmgr->FindEventManager(SCA_EventManager::JOY_EVENTMGR);
				if (eventmgr) 
				{
					int axis	=0;
					int axisf	=0;
					int button	=0;
					int hat		=0; 
					int hatf	=0;
					int prec	=0;
					
					switch(bjoy->type)
					{
					case SENS_JOY_AXIS:
						axis	= bjoy->axis;
						axisf	= bjoy->axisf;
						prec	= bjoy->precision;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_AXIS;
						break;
					case SENS_JOY_BUTTON:
						button	= bjoy->button;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_BUTTON;
						break;
					case SENS_JOY_HAT:
						hat		= bjoy->hat;
						hatf	= bjoy->hatf;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_HAT;
						break;
					case SENS_JOY_AXIS_SINGLE:
						axis	= bjoy->axis_single;
						prec	= bjoy->precision;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_AXIS_SINGLE;
						break;
					default:
						printf("Error: bad case statement\n");
						break;
					}
					gamesensor = new SCA_JoystickSensor(
						eventmgr,
						gameobj,
						bjoy->joyindex,
						joysticktype,
						axis,axisf,
						prec,
						button,
						hat,hatf,
						(bjoy->flag & SENS_JOY_ANY_EVENT));
				} 
				else
				{
					printf("Error there was a problem finding the event manager\n");
				}

				break;
			}
		default:
			{
			}
		}

		if (gamesensor)
		{
			gamesensor->SetExecutePriority(executePriority++);
			STR_String uniquename = sens->name;
			uniquename += "#SENS#";
			uniqueint++;
			CIntValue* uniqueval = new CIntValue(uniqueint);
			uniquename += uniqueval->GetText();
			uniqueval->Release();
			
			/* Conversion succeeded, so we can set the generic props here.   */
			gamesensor->SetPulseMode(pos_pulsemode, 
									 neg_pulsemode, 
									 frequency);
			gamesensor->SetInvert(invert);
			gamesensor->SetLevel(level);
			gamesensor->SetTap(tap);
			gamesensor->SetName(sens->name);			
			
			gameobj->AddSensor(gamesensor);
			
			// only register to manager if it's in an active layer
			// Make registration dynamic: only when sensor is activated
			//if (isInActiveLayer)
			//	gamesensor->RegisterToManager();
			
			gamesensor->ReserveController(sens->totlinks);
			for (int i=0;i<sens->totlinks;i++)
			{
				bController* linkedcont = (bController*) sens->links[i];
				if (linkedcont) {
					SCA_IController* gamecont = converter->FindGameController(linkedcont);

					if (gamecont) {
						logicmgr->RegisterToSensor(gamecont,gamesensor);
					} else {
						printf(
							"Warning, sensor \"%s\" could not find its controller "
							"(link %d of %d) from object \"%s\"\n"
							"\tthere has been an error converting the blender controller for the game engine,"
							"logic may be incorrect\n", sens->name, i+1, sens->totlinks, blenderobject->id.name+2);
					}
				} else {
					printf(
						"Warning, sensor \"%s\" has lost a link to a controller "
						"(link %d of %d) from object \"%s\"\n"
						"\tpossible causes are partially appended objects or an error reading the file,"
						"logic may be incorrect\n", sens->name, i+1, sens->totlinks, blenderobject->id.name+2);
				}
			}
			// special case: Keyboard sensor with no link
			// this combination is usually used for key logging. 
			if (sens->type == SENS_KEYBOARD && sens->totlinks == 0) {
				// Force the registration so that the sensor runs
				gamesensor->IncLink();
			}
				
			// done with gamesensor
			gamesensor->Release();
			
		}
		sens=sens->next;
	}
}

