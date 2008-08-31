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
 * Conversion of Blender data blocks to KX sensor system
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "KX_BlenderSceneConverter.h"
#include "KX_ConvertSensors.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
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

// this map is Blender specific: a conversion between blender and ketsji enums
std::map<int,SCA_IInputDevice::KX_EnumInputs> gReverseKeyTranslateTable;


void BL_ConvertSensors(struct Object* blenderobject,
					   class KX_GameObject* gameobj,
					   SCA_LogicManager* logicmgr,
					   KX_Scene* kxscene,
					   SCA_IInputDevice* keydev,
					   int & executePriority,
					   int activeLayerBitInfo,
					   bool isInActiveLayer,
					   RAS_ICanvas* canvas,
					   KX_BlenderSceneConverter* converter
					   )
{
	
	
	
	/* The reverse table. In order to not confuse ourselves, we      */
	/* immediately convert all events that come in to KX codes.      */
	gReverseKeyTranslateTable[LEFTMOUSE			] =	SCA_IInputDevice::KX_LEFTMOUSE;
	gReverseKeyTranslateTable[MIDDLEMOUSE		] =	SCA_IInputDevice::KX_MIDDLEMOUSE;
	gReverseKeyTranslateTable[RIGHTMOUSE		] =	SCA_IInputDevice::KX_RIGHTMOUSE;
	gReverseKeyTranslateTable[WHEELUPMOUSE		] =	SCA_IInputDevice::KX_WHEELUPMOUSE;
	gReverseKeyTranslateTable[WHEELDOWNMOUSE	] =	SCA_IInputDevice::KX_WHEELDOWNMOUSE;
	gReverseKeyTranslateTable[MOUSEX			] = SCA_IInputDevice::KX_MOUSEX;
	gReverseKeyTranslateTable[MOUSEY			] =	SCA_IInputDevice::KX_MOUSEY;
	
	// TIMERS                                                                                                  
	
	gReverseKeyTranslateTable[TIMER0			] = SCA_IInputDevice::KX_TIMER0;                  
	gReverseKeyTranslateTable[TIMER1			] = SCA_IInputDevice::KX_TIMER1;                  
	gReverseKeyTranslateTable[TIMER2			] = SCA_IInputDevice::KX_TIMER2;                  
	gReverseKeyTranslateTable[TIMER3			] = SCA_IInputDevice::KX_TIMER3;                  
	
	// SYSTEM                                                                                                  
	
	gReverseKeyTranslateTable[KEYBD				] = SCA_IInputDevice::KX_KEYBD;                  
	gReverseKeyTranslateTable[RAWKEYBD			] = SCA_IInputDevice::KX_RAWKEYBD;                  
	gReverseKeyTranslateTable[REDRAW			] = SCA_IInputDevice::KX_REDRAW;                  
	gReverseKeyTranslateTable[INPUTCHANGE		] = SCA_IInputDevice::KX_INPUTCHANGE;                  
	gReverseKeyTranslateTable[QFULL				] = SCA_IInputDevice::KX_QFULL;                  
	gReverseKeyTranslateTable[WINFREEZE			] = SCA_IInputDevice::KX_WINFREEZE;                  
	gReverseKeyTranslateTable[WINTHAW			] = SCA_IInputDevice::KX_WINTHAW;                  
	gReverseKeyTranslateTable[WINCLOSE			] = SCA_IInputDevice::KX_WINCLOSE;                  
	gReverseKeyTranslateTable[WINQUIT			] = SCA_IInputDevice::KX_WINQUIT;                  
	gReverseKeyTranslateTable[Q_FIRSTTIME		] = SCA_IInputDevice::KX_Q_FIRSTTIME;                  
	
	// standard keyboard                                                                                       
	
	gReverseKeyTranslateTable[AKEY				] = SCA_IInputDevice::KX_AKEY;                  
	gReverseKeyTranslateTable[BKEY				] = SCA_IInputDevice::KX_BKEY;                  
	gReverseKeyTranslateTable[CKEY				] = SCA_IInputDevice::KX_CKEY;                  
	gReverseKeyTranslateTable[DKEY				] = SCA_IInputDevice::KX_DKEY;                  
	gReverseKeyTranslateTable[EKEY				] = SCA_IInputDevice::KX_EKEY;                  
	gReverseKeyTranslateTable[FKEY				] = SCA_IInputDevice::KX_FKEY;                  
	gReverseKeyTranslateTable[GKEY				] = SCA_IInputDevice::KX_GKEY;                  
	gReverseKeyTranslateTable[HKEY				] = SCA_IInputDevice::KX_HKEY;                  
	gReverseKeyTranslateTable[IKEY				] = SCA_IInputDevice::KX_IKEY;                  
	gReverseKeyTranslateTable[JKEY				] = SCA_IInputDevice::KX_JKEY;                  
	gReverseKeyTranslateTable[KKEY				] = SCA_IInputDevice::KX_KKEY;                  
	gReverseKeyTranslateTable[LKEY				] = SCA_IInputDevice::KX_LKEY;                  
	gReverseKeyTranslateTable[MKEY				] = SCA_IInputDevice::KX_MKEY;                  
	gReverseKeyTranslateTable[NKEY				] = SCA_IInputDevice::KX_NKEY;                  
	gReverseKeyTranslateTable[OKEY				] = SCA_IInputDevice::KX_OKEY;                  
	gReverseKeyTranslateTable[PKEY				] = SCA_IInputDevice::KX_PKEY;                  
	gReverseKeyTranslateTable[QKEY				] = SCA_IInputDevice::KX_QKEY;                  
	gReverseKeyTranslateTable[RKEY				] = SCA_IInputDevice::KX_RKEY;                  
	gReverseKeyTranslateTable[SKEY				] = SCA_IInputDevice::KX_SKEY;                  
	gReverseKeyTranslateTable[TKEY				] = SCA_IInputDevice::KX_TKEY;                  
	gReverseKeyTranslateTable[UKEY				] = SCA_IInputDevice::KX_UKEY;                  
	gReverseKeyTranslateTable[VKEY				] = SCA_IInputDevice::KX_VKEY;                  
	gReverseKeyTranslateTable[WKEY				] = SCA_IInputDevice::KX_WKEY;                  
	gReverseKeyTranslateTable[XKEY				] = SCA_IInputDevice::KX_XKEY;                  
	gReverseKeyTranslateTable[YKEY				] = SCA_IInputDevice::KX_YKEY;                  
	gReverseKeyTranslateTable[ZKEY				] = SCA_IInputDevice::KX_ZKEY;                  
	
	gReverseKeyTranslateTable[ZEROKEY			] = SCA_IInputDevice::KX_ZEROKEY;                  
	gReverseKeyTranslateTable[ONEKEY			] = SCA_IInputDevice::KX_ONEKEY;                  
	gReverseKeyTranslateTable[TWOKEY			] = SCA_IInputDevice::KX_TWOKEY;                  
	gReverseKeyTranslateTable[THREEKEY			] = SCA_IInputDevice::KX_THREEKEY;                  
	gReverseKeyTranslateTable[FOURKEY			] = SCA_IInputDevice::KX_FOURKEY;                  
	gReverseKeyTranslateTable[FIVEKEY			] = SCA_IInputDevice::KX_FIVEKEY;                  
	gReverseKeyTranslateTable[SIXKEY			] = SCA_IInputDevice::KX_SIXKEY;                  
	gReverseKeyTranslateTable[SEVENKEY			] = SCA_IInputDevice::KX_SEVENKEY;                  
	gReverseKeyTranslateTable[EIGHTKEY			] = SCA_IInputDevice::KX_EIGHTKEY;                  
	gReverseKeyTranslateTable[NINEKEY			] = SCA_IInputDevice::KX_NINEKEY;                  
	
	gReverseKeyTranslateTable[CAPSLOCKKEY		] = SCA_IInputDevice::KX_CAPSLOCKKEY;                  
	
	gReverseKeyTranslateTable[LEFTCTRLKEY		] = SCA_IInputDevice::KX_LEFTCTRLKEY;                  
	gReverseKeyTranslateTable[LEFTALTKEY		] = SCA_IInputDevice::KX_LEFTALTKEY;                  
	gReverseKeyTranslateTable[RIGHTALTKEY		] = SCA_IInputDevice::KX_RIGHTALTKEY;                  
	gReverseKeyTranslateTable[RIGHTCTRLKEY		] = SCA_IInputDevice::KX_RIGHTCTRLKEY;                  
	gReverseKeyTranslateTable[RIGHTSHIFTKEY		] = SCA_IInputDevice::KX_RIGHTSHIFTKEY;                  
	gReverseKeyTranslateTable[LEFTSHIFTKEY		] = SCA_IInputDevice::KX_LEFTSHIFTKEY;                  
	
	gReverseKeyTranslateTable[ESCKEY			] = SCA_IInputDevice::KX_ESCKEY;                  
	gReverseKeyTranslateTable[TABKEY			] = SCA_IInputDevice::KX_TABKEY;                  
	gReverseKeyTranslateTable[RETKEY			] = SCA_IInputDevice::KX_RETKEY;                  
	gReverseKeyTranslateTable[SPACEKEY			] = SCA_IInputDevice::KX_SPACEKEY;                  
	gReverseKeyTranslateTable[LINEFEEDKEY		] = SCA_IInputDevice::KX_LINEFEEDKEY;                  
	gReverseKeyTranslateTable[BACKSPACEKEY		] = SCA_IInputDevice::KX_BACKSPACEKEY;                  
	gReverseKeyTranslateTable[DELKEY			] = SCA_IInputDevice::KX_DELKEY;                  
	gReverseKeyTranslateTable[SEMICOLONKEY		] = SCA_IInputDevice::KX_SEMICOLONKEY;                  
	gReverseKeyTranslateTable[PERIODKEY			] = SCA_IInputDevice::KX_PERIODKEY;                  
	gReverseKeyTranslateTable[COMMAKEY			] = SCA_IInputDevice::KX_COMMAKEY;                  
	gReverseKeyTranslateTable[QUOTEKEY			] = SCA_IInputDevice::KX_QUOTEKEY;                  
	gReverseKeyTranslateTable[ACCENTGRAVEKEY	] = SCA_IInputDevice::KX_ACCENTGRAVEKEY;                  
	gReverseKeyTranslateTable[MINUSKEY			] = SCA_IInputDevice::KX_MINUSKEY;                  
	gReverseKeyTranslateTable[SLASHKEY			] = SCA_IInputDevice::KX_SLASHKEY;                  
	gReverseKeyTranslateTable[BACKSLASHKEY		] = SCA_IInputDevice::KX_BACKSLASHKEY;                  
	gReverseKeyTranslateTable[EQUALKEY			] = SCA_IInputDevice::KX_EQUALKEY;                  
	gReverseKeyTranslateTable[LEFTBRACKETKEY	] = SCA_IInputDevice::KX_LEFTBRACKETKEY;                  
	gReverseKeyTranslateTable[RIGHTBRACKETKEY	] = SCA_IInputDevice::KX_RIGHTBRACKETKEY;                  
	
	gReverseKeyTranslateTable[LEFTARROWKEY		] = SCA_IInputDevice::KX_LEFTARROWKEY;                  
	gReverseKeyTranslateTable[DOWNARROWKEY		] = SCA_IInputDevice::KX_DOWNARROWKEY;                  
	gReverseKeyTranslateTable[RIGHTARROWKEY		] = SCA_IInputDevice::KX_RIGHTARROWKEY;                  
	gReverseKeyTranslateTable[UPARROWKEY		] = SCA_IInputDevice::KX_UPARROWKEY;                  
	
	gReverseKeyTranslateTable[PAD2				] = SCA_IInputDevice::KX_PAD2;                  
	gReverseKeyTranslateTable[PAD4				] = SCA_IInputDevice::KX_PAD4;                  
	gReverseKeyTranslateTable[PAD6				] = SCA_IInputDevice::KX_PAD6;                  
	gReverseKeyTranslateTable[PAD8				] = SCA_IInputDevice::KX_PAD8;                  
	
	gReverseKeyTranslateTable[PAD1				] = SCA_IInputDevice::KX_PAD1;                  
	gReverseKeyTranslateTable[PAD3				] = SCA_IInputDevice::KX_PAD3;                  
	gReverseKeyTranslateTable[PAD5				] = SCA_IInputDevice::KX_PAD5;                  
	gReverseKeyTranslateTable[PAD7				] = SCA_IInputDevice::KX_PAD7;                  
	gReverseKeyTranslateTable[PAD9				] = SCA_IInputDevice::KX_PAD9;                  
	
	gReverseKeyTranslateTable[PADPERIOD			] = SCA_IInputDevice::KX_PADPERIOD;                  
	gReverseKeyTranslateTable[PADSLASHKEY		] = SCA_IInputDevice::KX_PADSLASHKEY;                  
	gReverseKeyTranslateTable[PADASTERKEY		] = SCA_IInputDevice::KX_PADASTERKEY;                  
	
	gReverseKeyTranslateTable[PAD0				] = SCA_IInputDevice::KX_PAD0;                  
	gReverseKeyTranslateTable[PADMINUS			] = SCA_IInputDevice::KX_PADMINUS;                  
	gReverseKeyTranslateTable[PADENTER			] = SCA_IInputDevice::KX_PADENTER;                  
	gReverseKeyTranslateTable[PADPLUSKEY		] = SCA_IInputDevice::KX_PADPLUSKEY;                  
	
	
	gReverseKeyTranslateTable[F1KEY				] = SCA_IInputDevice::KX_F1KEY;                  
	gReverseKeyTranslateTable[F2KEY				] = SCA_IInputDevice::KX_F2KEY;                  
	gReverseKeyTranslateTable[F3KEY				] = SCA_IInputDevice::KX_F3KEY;                  
	gReverseKeyTranslateTable[F4KEY				] = SCA_IInputDevice::KX_F4KEY;                  
	gReverseKeyTranslateTable[F5KEY				] = SCA_IInputDevice::KX_F5KEY;                  
	gReverseKeyTranslateTable[F6KEY				] = SCA_IInputDevice::KX_F6KEY;                  
	gReverseKeyTranslateTable[F7KEY				] = SCA_IInputDevice::KX_F7KEY;                  
	gReverseKeyTranslateTable[F8KEY				] = SCA_IInputDevice::KX_F8KEY;                  
	gReverseKeyTranslateTable[F9KEY				] = SCA_IInputDevice::KX_F9KEY;                  
	gReverseKeyTranslateTable[F10KEY			] = SCA_IInputDevice::KX_F10KEY;                  
	gReverseKeyTranslateTable[F11KEY			] = SCA_IInputDevice::KX_F11KEY;                  
	gReverseKeyTranslateTable[F12KEY			] = SCA_IInputDevice::KX_F12KEY;                  
	
	gReverseKeyTranslateTable[PAUSEKEY			] = SCA_IInputDevice::KX_PAUSEKEY;                  
	gReverseKeyTranslateTable[INSERTKEY			] = SCA_IInputDevice::KX_INSERTKEY;                  
	gReverseKeyTranslateTable[HOMEKEY			] = SCA_IInputDevice::KX_HOMEKEY;                  
	gReverseKeyTranslateTable[PAGEUPKEY			] = SCA_IInputDevice::KX_PAGEUPKEY;                  
	gReverseKeyTranslateTable[PAGEDOWNKEY		] = SCA_IInputDevice::KX_PAGEDOWNKEY;                  
	gReverseKeyTranslateTable[ENDKEY			] = SCA_IInputDevice::KX_ENDKEY;
	
	int uniqueint = 0;
	bSensor* sens = (bSensor*)blenderobject->sensors.first;
	bool pos_pulsemode = false;
	bool neg_pulsemode = false;
	int frequency = 0;
	bool invert = false;
	bool level = false;
	
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

		switch (sens->type)
		{
		case  SENS_ALWAYS:
			{
				
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::ALWAYS_EVENTMGR);
				if (eventmgr)
				{
					gamesensor = new SCA_AlwaysSensor(eventmgr, gameobj);
				}
				
				break;
			}
			
		case  SENS_DELAY:
			{
				// we can reuse the Always event manager for the delay sensor
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::ALWAYS_EVENTMGR);
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
					
					bool bFindMaterial = false;
					
					bCollisionSensor* blendertouchsensor = (bCollisionSensor*)sens->data;
					
					bFindMaterial = (blendertouchsensor->mode 
						& SENS_COLLISION_MATERIAL);
					
					
					STR_String touchPropOrMatName = ( bFindMaterial ? 
						blendertouchsensor->materialName:
					(blendertouchsensor->name ? blendertouchsensor->name: ""));
					
					
					if (gameobj->GetPhysicsController())
					{	
						gamesensor = new KX_TouchSensor(eventmgr,
							gameobj,
							bFindMaterial,
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
					pos[0] = wpos[0];
					pos[1] = wpos[1];
					pos[2] = wpos[2];
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
						nearpropertyname,kxscene,
						physCtrl
						);
					
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
						gReverseKeyTranslateTable[blenderkeybdsensor->key],
						blenderkeybdsensor->qual,
						blenderkeybdsensor->qual2,
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
				* no focus-related behaviour requested, we can make do
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
							canvas,
							kxscene,
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
					= logicmgr->FindEventManager(SCA_EventManager::PROPERTY_EVENTMGR);
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
					PHY_IPhysicsController* ctrl = kxscene->GetPhysicsEnvironment()->CreateConeController(coneradius,coneheight);

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
						radarpropertyname,
						kxscene);
						
				}
			
				break;
			}
		case SENS_RAY:
			{
				bRaySensor* blenderraysensor = (bRaySensor*) sens->data;
				
				//blenderradarsensor->angle;
				SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::RAY_EVENTMGR);
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
					SCA_EventManager* eventmgr = logicmgr->FindEventManager(SCA_EventManager::RANDOM_EVENTMGR);
					if (eventmgr)
					{
						int randomSeed = blenderrndsensor->seed;
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
					int buttonf =0; 
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
						buttonf	= bjoy->buttonf;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_BUTTON;
						break;
					case SENS_JOY_HAT:
						hat		= bjoy->hat;
						hatf	= bjoy->hatf;
						joysticktype  = SCA_JoystickSensor::KX_JOYSENSORMODE_HAT;
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
						button,buttonf,
						hat,hatf);
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
			gamesensor->SetName(STR_String(sens->name));			
			
			gameobj->AddSensor(gamesensor);
			
			// only register to manager if it's in an active layer
			// Make registration dynamic: only when sensor is activated
			//if (isInActiveLayer)
			//	gamesensor->RegisterToManager();
			
			
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
			// done with gamesensor
			gamesensor->Release();
			
		}
		sens=sens->next;
	}
}

