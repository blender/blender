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
 */

/** \file gameengine/Converter/KX_ConvertProperties.cpp
 *  \ingroup bgeconv
 */


#include "KX_ConvertProperties.h"


#include "DNA_object_types.h"
#include "DNA_property_types.h"
/* end of blender include block */


#include "Value.h"
#include "VectorValue.h"
#include "BoolValue.h"
#include "StringValue.h"
#include "FloatValue.h"
#include "KX_GameObject.h"
#include "IntValue.h"
#include "SCA_TimeEventManager.h"
#include "SCA_IScene.h"

#include "KX_FontObject.h"
#include "DNA_curve_types.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

extern "C" {
	#include "BKE_property.h"
}

/* prototype */
void BL_ConvertTextProperty(Object* object, KX_FontObject* fontobj,SCA_TimeEventManager* timemgr,SCA_IScene* scene, bool isInActiveLayer);

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
			case GPROP_BOOL:
			{
				propval = new CBoolValue((bool)(prop->data != 0));
				gameobj->SetProperty(prop->name,propval);
				//promp->poin= &prop->data;
				break;
			}
			case GPROP_INT:
			{
				propval = new CIntValue((int)prop->data);
				gameobj->SetProperty(prop->name,propval);
				break;
			}
			case GPROP_FLOAT:
			{
				//prop->poin= &prop->data;
				float floatprop = *((float*)&prop->data);
				propval = new CFloatValue(floatprop);
				gameobj->SetProperty(prop->name,propval);
			}
			break;
			case GPROP_STRING:
			{
				//prop->poin= callocN(MAX_PROPSTRING, "property string");
				propval = new CStringValue((char*)prop->poin,"");
				gameobj->SetProperty(prop->name,propval);
				break;
			}
			case GPROP_TIME:
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
		
#ifdef WITH_PYTHON
		/* Warn if we double up on attributes, this isn't quite right since it wont find inherited attributes however there arnt many */
		for (PyAttributeDef *attrdef = KX_GameObject::Attributes; attrdef->m_name; attrdef++) {
			if (strcmp(prop->name, attrdef->m_name)==0) {
				printf("Warning! user defined property name \"%s\" is also a python attribute for object \"%s\"\n\tUse ob[\"%s\"] syntax to avoid conflict\n", prop->name, object->id.name+2, prop->name);
				break;
			}
		}
		for (PyMethodDef *methdef = KX_GameObject::Methods; methdef->ml_name; methdef++) {
			if (strcmp(prop->name, methdef->ml_name)==0) {
				printf("Warning! user defined property name \"%s\" is also a python method for object \"%s\"\n\tUse ob[\"%s\"] syntax to avoid conflict\n", prop->name, object->id.name+2, prop->name);
				break;
			}
		}
		/* end warning check */
#endif // WITH_PYTHON

		prop = prop->next;
	}
	// check if state needs to be debugged
	if (object->scaflag & OB_DEBUGSTATE)
	{
		//  reserve name for object state
		scene->AddDebugProperty(gameobj,STR_String("__state__"));
	}

	/* Font Objects need to 'copy' the Font Object data body to ["Text"] */
	if (object->type == OB_FONT)
	{
		BL_ConvertTextProperty(object, (KX_FontObject *)gameobj, timemgr, scene, isInActiveLayer);
	}
}

void BL_ConvertTextProperty(Object* object, KX_FontObject* fontobj,SCA_TimeEventManager* timemgr,SCA_IScene* scene, bool isInActiveLayer)
{
	CValue* tprop = fontobj->GetProperty("Text");
	if (!tprop) return;
	bProperty* prop = get_ob_property(object, "Text");
	if (!prop) return;

	Curve *curve = static_cast<Curve *>(object->data);
	STR_String str = curve->str;
	CValue* propval = NULL;

	switch(prop->type) {
		case GPROP_BOOL:
		{
			int value = atoi(str);
			propval = new CBoolValue((bool)(value != 0));
			tprop->SetValue(propval);
			break;
		}
		case GPROP_INT:
		{
			int value = atoi(str);
			propval = new CIntValue(value);
			tprop->SetValue(propval);
			break;
		}
		case GPROP_FLOAT:
		{
			float floatprop = (float)atof(str);
			propval = new CFloatValue(floatprop);
			tprop->SetValue(propval);
			break;
		}
		case GPROP_STRING:
		{
			propval = new CStringValue(str, "");
			tprop->SetValue(propval);
			break;
		}
		case GPROP_TIME:
		{
			float floatprop = (float)atof(str);

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
			tprop->SetValue(timeval);
		}
		default:
		{
			// todo make an assert etc.
		}
	}

	if (propval) {
		propval->Release();
	}
}

