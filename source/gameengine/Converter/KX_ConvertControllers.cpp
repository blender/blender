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

#include "MEM_guardedalloc.h"

#include "KX_BlenderSceneConverter.h"
#include "KX_ConvertControllers.h"
#include "KX_Python.h"

// Controller
#include "SCA_ANDController.h"
#include "SCA_ORController.h"
#include "SCA_PythonController.h"
#include "SCA_ExpressionController.h"

#include "SCA_LogicManager.h"
#include "KX_GameObject.h"
#include "IntValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_object_types.h"
#include "DNA_controller_types.h"
#include "DNA_text_types.h"

#include "BKE_text.h"

#include "BLI_blenlib.h"

/* end of blender include block */


	static void
LinkControllerToActuators(
	SCA_IController *game_controller,
	bController* bcontr,	
	SCA_LogicManager* logicmgr,
	KX_BlenderSceneConverter* converter
) {
	// Iterate through the actuators of the game blender
	// controller and find the corresponding ketsji actuator.

	for (int i=0;i<bcontr->totlinks;i++)
	{
		bActuator* bact = (bActuator*) bcontr->links[i];
		SCA_IActuator *game_actuator = converter->FindGameActuator(bact);
		if (game_actuator) {
			logicmgr->RegisterToActuator(game_controller, game_actuator);
		}
	}
}


void BL_ConvertControllers(
	struct Object* blenderobject,
	class KX_GameObject* gameobj,
	SCA_LogicManager* logicmgr, 
	PyObject* pythondictionary,
	int &executePriority,
	int activeLayerBitInfo,
	bool isInActiveLayer,
	KX_BlenderSceneConverter* converter
) {
	int uniqueint=0;
	bController* bcontr = (bController*)blenderobject->controllers.first;
	while (bcontr)
	{
		SCA_IController* gamecontroller = NULL;
		switch(bcontr->type)
		{
			case CONT_LOGIC_AND:
			{
				gamecontroller = new SCA_ANDController(gameobj);
				LinkControllerToActuators(gamecontroller,bcontr,logicmgr,converter);
				break;
			}
			case CONT_LOGIC_OR:
			{
				gamecontroller = new SCA_ORController(gameobj);
				LinkControllerToActuators(gamecontroller,bcontr,logicmgr,converter);
				break;
			}
			case CONT_EXPRESSION:
			{
				bExpressionCont* bexpcont = (bExpressionCont*) bcontr->data;
				STR_String expressiontext = STR_String(bexpcont->str);
				if (expressiontext.Length() > 0)
				{
					gamecontroller = new SCA_ExpressionController(gameobj,expressiontext);
					LinkControllerToActuators(gamecontroller,bcontr,logicmgr,converter);

				}
				break;
			}
			case CONT_PYTHON:
			{
					
				// we should create a Python controller here
							
				SCA_PythonController* pyctrl = new SCA_PythonController(gameobj);
				gamecontroller = pyctrl;
					
				bPythonCont* pycont = (bPythonCont*) bcontr->data;
				pyctrl->SetDictionary(pythondictionary);
					
				if (pycont->text)
				{
					char *buf;
					// this is some blender specific code
					buf= txt_to_buf(pycont->text);
					if (buf)
					{
						pyctrl->SetScriptText(STR_String(buf));
						pyctrl->SetScriptName(pycont->text->id.name+2);
						MEM_freeN(buf);
					}
					
				}
					
				LinkControllerToActuators(gamecontroller,bcontr,logicmgr,converter);
				break;
			}
			default:
			{
				
			}
		}

		if (gamecontroller)
		{
			gamecontroller->SetExecutePriority(executePriority++);
			gamecontroller->SetState(bcontr->state_mask);
			STR_String uniquename = bcontr->name;
			uniquename += "#CONTR#";
			uniqueint++;
			CIntValue* uniqueval = new CIntValue(uniqueint);
			uniquename += uniqueval->GetText();
			uniqueval->Release();
			gamecontroller->SetName(uniquename);
			gameobj->AddController(gamecontroller);
			
			converter->RegisterGameController(gamecontroller, bcontr);
			//done with gamecontroller
			gamecontroller->Release();
		}
		
		bcontr = bcontr->next;
	}

}
