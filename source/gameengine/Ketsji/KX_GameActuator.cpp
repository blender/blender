/**
* global game stuff
*
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

#include "SCA_IActuator.h"
#include "KX_GameActuator.h"
//#include <iostream>
#include "KX_Scene.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h" /* for config load/saving */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_GameActuator::KX_GameActuator(SCA_IObject *gameobj, 
								   int mode,
								   const STR_String& filename,
								   const STR_String& loadinganimationname,
								   SCA_IScene* scene,
								   KX_KetsjiEngine* ketsjiengine)
								   : SCA_IActuator(gameobj, KX_ACT_GAME)
{
	m_mode = mode;
	m_filename = filename;
	m_loadinganimationname = loadinganimationname;
	m_scene = scene;
	m_ketsjiengine = ketsjiengine;
} /* End of constructor */



KX_GameActuator::~KX_GameActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */



CValue* KX_GameActuator::GetReplica()
{
	KX_GameActuator* replica = new KX_GameActuator(*this);
	replica->ProcessReplica();
	
	return replica;
}



bool KX_GameActuator::Update()
{
	// bool result = false;	 /*unused*/
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	switch (m_mode)
	{
	case KX_GAME_LOAD:
	case KX_GAME_START:
		{
			if (m_ketsjiengine)
			{
				STR_String exitstring = "start other game";
				m_ketsjiengine->RequestExit(KX_EXIT_REQUEST_START_OTHER_GAME);
				m_ketsjiengine->SetNameNextGame(m_filename);
				m_scene->AddDebugProperty((this)->GetParent(), exitstring);
			}

			break;
		}
	case KX_GAME_RESTART:
		{
			if (m_ketsjiengine)
			{
				STR_String exitstring = "restarting game";
				m_ketsjiengine->RequestExit(KX_EXIT_REQUEST_RESTART_GAME);
				m_ketsjiengine->SetNameNextGame(m_filename);
				m_scene->AddDebugProperty((this)->GetParent(), exitstring);
			}
			break;
		}
	case KX_GAME_QUIT:
		{
			if (m_ketsjiengine)
			{
				STR_String exitstring = "quiting game";
				m_ketsjiengine->RequestExit(KX_EXIT_REQUEST_QUIT_GAME);
				m_scene->AddDebugProperty((this)->GetParent(), exitstring);
			}
			break;
		}
	case KX_GAME_SAVECFG:
		{
#ifndef DISABLE_PYTHON
			if (m_ketsjiengine)
			{
				char mashal_path[512];
				char *marshal_buffer = NULL;
				unsigned int marshal_length;
				FILE *fp = NULL;
				
				pathGamePythonConfig(mashal_path);
				marshal_length = saveGamePythonConfig(&marshal_buffer);
				
				if (marshal_length && marshal_buffer) {
					fp = fopen(mashal_path, "wb");
					if (fp) {
						if (fwrite(marshal_buffer, 1, marshal_length, fp) != marshal_length) {
							printf("Warning: could not write marshal data\n");
						}
						fclose(fp);
					} else {
						printf("Warning: could not open marshal file\n");
					}
				} else {
					printf("Warning: could not create marshal buffer\n");
				}
				if (marshal_buffer)
					delete [] marshal_buffer;
			}
			break;
#endif // DISABLE_PYTHON
		}
	case KX_GAME_LOADCFG:
		{
#ifndef DISABLE_PYTHON
			if (m_ketsjiengine)
			{
				char mashal_path[512];
				char *marshal_buffer;
				int marshal_length;
				FILE *fp = NULL;
				int result;
				
				pathGamePythonConfig(mashal_path);
				
				fp = fopen(mashal_path, "rb");
				if (fp) {
					// obtain file size:
					fseek (fp , 0 , SEEK_END);
					marshal_length = ftell(fp);
					rewind(fp);
					
					marshal_buffer = (char*) malloc (sizeof(char)*marshal_length);
					
					result = fread (marshal_buffer, 1, marshal_length, fp);
					
					if (result == marshal_length) {
						loadGamePythonConfig(marshal_buffer, marshal_length);
					} else {
						printf("warning: could not read all of '%s'\n", mashal_path);
					}
					
					free(marshal_buffer);
					fclose(fp);
				} else {
					printf("warning: could not open '%s'\n", mashal_path);
				}
			}
			break;
#endif // DISABLE_PYTHON
		}
	default:
		; /* do nothing? this is an internal error !!! */
	}
	
	return false;
}


#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_GameActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_GameActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_GameActuator::Methods[] =
{
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_GameActuator::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("fileName",0,100,false,KX_GameActuator,m_filename),
	KX_PYATTRIBUTE_INT_RW("mode", KX_GAME_NODEF+1, KX_GAME_MAX-1, true, KX_GameActuator, m_mode),
	{ NULL }	//Sentinel
};

#endif // DISABLE_PYTHON
