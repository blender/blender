/**
 * $Id$
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

#include "KX_ConvertProperties.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_object_types.h"
#include "DNA_property_types.h"
/* end of blender include block */

#include "Value.h"
#include "VectorValue.h"
#include "BoolValue.h"
#include "StringValue.h"
#include "FloatValue.h"
#include "KX_GameObject.h"
//#include "ListValue.h"
#include "IntValue.h"
#include "SCA_TimeEventManager.h"
#include "SCA_IScene.h"


void BL_ConvertProperties(Object* object,KX_GameObject* gameobj,SCA_TimeEventManager* timemgr,SCA_IScene* scene, bool isInActiveLayer)
{
	
	bProperty* prop = (bProperty*)object->prop.first;
	CValue* propval;	
	bool show_debug_info;
	while(prop)
	{
	
		propval = NULL;
		show_debug_info = bool (prop->flag & PROP_DEBUG);

		switch(prop->type) {
		case PROP_BOOL:
		{
			propval = new CBoolValue((bool)(prop->data != 0));
			gameobj->SetProperty(prop->name,propval);
			//promp->poin= &prop->data;
			break;
		}
		case PROP_INT:
		{
			propval = new CIntValue((int)prop->data);
			gameobj->SetProperty(prop->name,propval);
			break;
		}
		case PROP_FLOAT:
		{
			//prop->poin= &prop->data;
			float floatprop = *((float*)&prop->data);
			propval = new CFloatValue(floatprop);
			gameobj->SetProperty(prop->name,propval);
		}
		break;
		case PROP_STRING:
		{
			//prop->poin= callocN(MAX_PROPSTRING, "property string");
			propval = new CStringValue((char*)prop->poin,"");
			gameobj->SetProperty(prop->name,propval);
			break;
		}
		case PROP_TIME:
		{
			float floatprop = *((float*)&prop->data);

			CValue* timeval = new CFloatValue(floatprop);
			// set a subproperty called 'timer' so that 
			// we can register the replica of this property 
			// at the time a game object is replicated (AddObjectActuator triggers this)
			CValue *bval = new CBoolValue(true);
			timeval->SetProperty("timer",bval);
			bval->Release();
			if (isInActiveLayer)
			{
				timemgr->AddTimeProperty(timeval);
			}
			
			propval = timeval;
			gameobj->SetProperty(prop->name,timeval);

		}
		default:
		{
			// todo make an assert etc.
		}
		}
		
		if (propval)
		{
			if (show_debug_info)
			{
				scene->AddDebugProperty(gameobj,STR_String(prop->name));
			}
			// done with propval, release it
			propval->Release();
		}

		prop = prop->next;
	}
	// check if state needs to be debugged
	if (object->scaflag & OB_DEBUGSTATE)
	{
		//  reserve name for object state
		scene->AddDebugProperty(gameobj,STR_String("__state__"));
	}
}
