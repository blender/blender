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
 * Initialize Python thingies.
 */

#include "GL/glew.h"

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#ifndef DISABLE_PYTHON

extern "C" {
	#include "bpy_internal_import.h"  /* from the blender python api, but we want to import text too! */
	#include "mathutils.h" // Blender.Mathutils module copied here so the blenderlayer can use.
	#include "geometry.h" // Blender.Geometry module copied here so the blenderlayer can use.
	#include "bgl.h"
	#include "blf_api.h"

	#include "marshal.h" /* python header for loading/saving dicts */
}
#endif

#include "KX_PythonInit.h"

// directory header for py function getBlendFileList
#ifndef WIN32
  #include <dirent.h>
  #include <stdlib.h>
#else
  #include <io.h>
  #include "BLI_winstuff.h"
#endif

//python physics binding
#include "KX_PyConstraintBinding.h"

#include "KX_KetsjiEngine.h"
#include "KX_RadarSensor.h"
#include "KX_RaySensor.h"
#include "KX_ArmatureSensor.h"
#include "KX_SceneActuator.h"
#include "KX_GameActuator.h"
#include "KX_ParentActuator.h"
#include "KX_SCA_DynamicActuator.h"

#include "SCA_IInputDevice.h"
#include "SCA_PropertySensor.h"
#include "SCA_RandomActuator.h"
#include "SCA_KeyboardSensor.h" /* IsPrintable, ToCharacter */
#include "SCA_PythonKeyboard.h"
#include "SCA_PythonMouse.h"
#include "KX_ConstraintActuator.h"
#include "KX_IpoActuator.h"
#include "KX_SoundActuator.h"
#include "KX_StateActuator.h"
#include "BL_ActionActuator.h"
#include "BL_ArmatureObject.h"
#include "RAS_IRasterizer.h"
#include "RAS_ICanvas.h"
#include "RAS_BucketManager.h"
#include "RAS_2DFilterManager.h"
#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "ListValue.h"
#include "InputParser.h"
#include "KX_Scene.h"

#include "NG_NetworkScene.h" //Needed for sendMessage()

#include "BL_Shader.h"

#include "KX_PyMath.h"

#include "PyObjectPlus.h"

#include "KX_PythonInitTypes.h" 

/* we only need this to get a list of libraries from the main struct */
#include "DNA_ID.h"
#include "DNA_scene_types.h"

#include "PHY_IPhysicsEnvironment.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BLI_blenlib.h"
#include "GPU_material.h"
#include "MEM_guardedalloc.h"

/* for converting new scenes */
#include "KX_BlenderSceneConverter.h"
#include "KX_MeshProxy.h" /* for creating a new library of mesh objects */
extern "C" {
	#include "BLO_readfile.h"
}


#include "NG_NetworkScene.h" //Needed for sendMessage()

static void setSandbox(TPythonSecurityLevel level);

// 'local' copy of canvas ptr, for window height/width python scripts
static RAS_ICanvas* gp_Canvas = NULL;
static KX_Scene*	gp_KetsjiScene = NULL;
static KX_KetsjiEngine*	gp_KetsjiEngine = NULL;
static RAS_IRasterizer* gp_Rasterizer = NULL;
static char gp_GamePythonPath[FILE_MAXDIR + FILE_MAXFILE] = "";
static char gp_GamePythonPathOrig[FILE_MAXDIR + FILE_MAXFILE] = ""; // not super happy about this, but we need to remember the first loaded file for the global/dict load save

void KX_SetActiveScene(class KX_Scene* scene)
{
	gp_KetsjiScene = scene;
}

class KX_Scene* KX_GetActiveScene()
{
	return gp_KetsjiScene;
}

class KX_KetsjiEngine* KX_GetActiveEngine()
{
	return gp_KetsjiEngine;
}

/* why is this in python? */
void	KX_RasterizerDrawDebugLine(const MT_Vector3& from,const MT_Vector3& to,const MT_Vector3& color)
{
	if (gp_Rasterizer)
		gp_Rasterizer->DrawDebugLine(from,to,color);
}

#ifndef DISABLE_PYTHON

static PyObject *gp_OrigPythonSysPath= NULL;
static PyObject *gp_OrigPythonSysModules= NULL;

/* Macro for building the keyboard translation */
//#define KX_MACRO_addToDict(dict, name) PyDict_SetItemString(dict, #name, PyLong_FromSsize_t(SCA_IInputDevice::KX_##name))
#define KX_MACRO_addToDict(dict, name) PyDict_SetItemString(dict, #name, item=PyLong_FromSsize_t(name)); Py_DECREF(item)
/* For the defines for types from logic bricks, we do stuff explicitly... */
#define KX_MACRO_addTypesToDict(dict, name, name2) PyDict_SetItemString(dict, #name, item=PyLong_FromSsize_t(name2)); Py_DECREF(item)


// temporarily python stuff, will be put in another place later !
#include "KX_Python.h"
#include "SCA_PythonController.h"
// List of methods defined in the module

static PyObject* ErrorObject;
static const char *gPyGetRandomFloat_doc="getRandomFloat returns a random floating point value in the range [0..1]";

static PyObject* gPyGetRandomFloat(PyObject*)
{
	return PyFloat_FromDouble(MT_random());
}

static PyObject* gPySetGravity(PyObject*, PyObject* value)
{
	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	if (gp_KetsjiScene)
		gp_KetsjiScene->SetGravity(vec);
	
	Py_RETURN_NONE;
}

static char gPyExpandPath_doc[] =
"(path) - Converts a blender internal path into a proper file system path.\n\
path - the string path to convert.\n\n\
Use / as directory separator in path\n\
You can use '//' at the start of the string to define a relative path;\n\
Blender replaces that string by the directory of the startup .blend or runtime\n\
file to make a full path name (doesn't change during the game, even if you load\n\
other .blend).\n\
The function also converts the directory separator to the local file system format.";

static PyObject* gPyExpandPath(PyObject*, PyObject* args)
{
	char expanded[FILE_MAXDIR + FILE_MAXFILE];
	char* filename;
	
	if (!PyArg_ParseTuple(args,"s:ExpandPath",&filename))
		return NULL;

	BLI_strncpy(expanded, filename, FILE_MAXDIR + FILE_MAXFILE);
	BLI_path_abs(expanded, gp_GamePythonPath);
	return PyUnicode_FromString(expanded);
}

static char gPyStartGame_doc[] =
"startGame(blend)\n\
Loads the blend file";

static PyObject* gPyStartGame(PyObject*, PyObject* args)
{
	char* blendfile;

	if (!PyArg_ParseTuple(args, "s:startGame", &blendfile))
		return NULL;

	gp_KetsjiEngine->RequestExit(KX_EXIT_REQUEST_START_OTHER_GAME);
	gp_KetsjiEngine->SetNameNextGame(blendfile);

	Py_RETURN_NONE;
}

static char gPyEndGame_doc[] =
"endGame()\n\
Ends the current game";

static PyObject* gPyEndGame(PyObject*)
{
	gp_KetsjiEngine->RequestExit(KX_EXIT_REQUEST_QUIT_GAME);

	//printf("%s\n", gp_GamePythonPath);

	Py_RETURN_NONE;
}

static char gPyRestartGame_doc[] =
"restartGame()\n\
Restarts the current game by reloading the .blend file";

static PyObject* gPyRestartGame(PyObject*)
{
	gp_KetsjiEngine->RequestExit(KX_EXIT_REQUEST_RESTART_GAME);
	gp_KetsjiEngine->SetNameNextGame(gp_GamePythonPath);

	Py_RETURN_NONE;
}

static char gPySaveGlobalDict_doc[] =
"saveGlobalDict()\n\
Saves GameLogic.globalDict to a file";

static PyObject* gPySaveGlobalDict(PyObject*)
{
	char marshal_path[512];
	char *marshal_buffer = NULL;
	unsigned int marshal_length;
	FILE *fp = NULL;

	pathGamePythonConfig(marshal_path);
	marshal_length = saveGamePythonConfig(&marshal_buffer);

	if (marshal_length && marshal_buffer)
	{
		fp = fopen(marshal_path, "wb");

		if (fp)
		{
			if (fwrite(marshal_buffer, 1, marshal_length, fp) != marshal_length)
				printf("Warning: could not write marshal data\n");

			fclose(fp);
		} else {
			printf("Warning: could not open marshal file\n");
		}
	} else {
		printf("Warning: could not create marshal buffer\n");
	}

	if (marshal_buffer)
		delete [] marshal_buffer;

	Py_RETURN_NONE;
}

static char gPyLoadGlobalDict_doc[] =
"LoadGlobalDict()\n\
Loads GameLogic.globalDict from a file";

static PyObject* gPyLoadGlobalDict(PyObject*)
{
	char marshal_path[512];
	char *marshal_buffer = NULL;
	unsigned int marshal_length;
	FILE *fp = NULL;
	int result;

	pathGamePythonConfig(marshal_path);

	fp = fopen(marshal_path, "rb");

	if (fp) {
		// obtain file size:
		fseek (fp, 0, SEEK_END);
		marshal_length = ftell(fp);
		rewind(fp);

		marshal_buffer = (char*)malloc (sizeof(char)*marshal_length);

		result = fread(marshal_buffer, 1, marshal_length, fp);

		if (result == marshal_length) {
			loadGamePythonConfig(marshal_buffer, marshal_length);
		} else {
			printf("Warning: could not read all of '%s'\n", marshal_path);
		}

		free(marshal_buffer);
		fclose(fp);
	} else {
		printf("Warning: could not open '%s'\n", marshal_path);
	}

	Py_RETURN_NONE;
}

static char gPySendMessage_doc[] = 
"sendMessage(subject, [body, to, from])\n\
sends a message in same manner as a message actuator\
subject = Subject of the message\
body = Message body\
to = Name of object to send the message to\
from = Name of object to send the string from";

static PyObject* gPySendMessage(PyObject*, PyObject* args)
{
	char* subject;
	char* body = (char *)"";
	char* to = (char *)"";
	char* from = (char *)"";

	if (!PyArg_ParseTuple(args, "s|sss:sendMessage", &subject, &body, &to, &from))
		return NULL;

	gp_KetsjiScene->GetNetworkScene()->SendMessage(to, from, subject, body);

	Py_RETURN_NONE;
}

// this gets a pointer to an array filled with floats
static PyObject* gPyGetSpectrum(PyObject*)
{
	PyObject* resultlist = PyList_New(512);

        for (int index = 0; index < 512; index++)
        {
                PyList_SET_ITEM(resultlist, index, PyFloat_FromDouble(0.0));
        }

	return resultlist;
}


static PyObject* gPyStopDSP(PyObject*, PyObject* args)
{
        PyErr_SetString(PyExc_RuntimeError, "no audio device available");
        return NULL;
	
	Py_RETURN_NONE;
}

static PyObject* gPySetLogicTicRate(PyObject*, PyObject* args)
{
	float ticrate;
	if (!PyArg_ParseTuple(args, "f:setLogicTicRate", &ticrate))
		return NULL;
	
	KX_KetsjiEngine::SetTicRate(ticrate);
	Py_RETURN_NONE;
}

static PyObject* gPyGetLogicTicRate(PyObject*)
{
	return PyFloat_FromDouble(KX_KetsjiEngine::GetTicRate());
}

static PyObject* gPySetMaxLogicFrame(PyObject*, PyObject* args)
{
	int frame;
	if (!PyArg_ParseTuple(args, "i:setMaxLogicFrame", &frame))
		return NULL;
	
	KX_KetsjiEngine::SetMaxLogicFrame(frame);
	Py_RETURN_NONE;
}

static PyObject* gPyGetMaxLogicFrame(PyObject*)
{
	return PyLong_FromSsize_t(KX_KetsjiEngine::GetMaxLogicFrame());
}

static PyObject* gPySetMaxPhysicsFrame(PyObject*, PyObject* args)
{
	int frame;
	if (!PyArg_ParseTuple(args, "i:setMaxPhysicsFrame", &frame))
		return NULL;
	
	KX_KetsjiEngine::SetMaxPhysicsFrame(frame);
	Py_RETURN_NONE;
}

static PyObject* gPyGetMaxPhysicsFrame(PyObject*)
{
	return PyLong_FromSsize_t(KX_KetsjiEngine::GetMaxPhysicsFrame());
}

static PyObject* gPySetPhysicsTicRate(PyObject*, PyObject* args)
{
	float ticrate;
	if (!PyArg_ParseTuple(args, "f:setPhysicsTicRate", &ticrate))
		return NULL;
	
	PHY_GetActiveEnvironment()->setFixedTimeStep(true,ticrate);
	Py_RETURN_NONE;
}
#if 0 // unused
static PyObject* gPySetPhysicsDebug(PyObject*, PyObject* args)
{
	int debugMode;
	if (!PyArg_ParseTuple(args, "i:setPhysicsDebug", &debugMode))
		return NULL;
	
	PHY_GetActiveEnvironment()->setDebugMode(debugMode);
	Py_RETURN_NONE;
}
#endif


static PyObject* gPyGetPhysicsTicRate(PyObject*)
{
	return PyFloat_FromDouble(PHY_GetActiveEnvironment()->getFixedTimeStep());
}

static PyObject* gPyGetAverageFrameRate(PyObject*)
{
	return PyFloat_FromDouble(KX_KetsjiEngine::GetAverageFrameRate());
}

static PyObject* gPyGetBlendFileList(PyObject*, PyObject* args)
{
	char cpath[sizeof(gp_GamePythonPath)];
	char *searchpath = NULL;
	PyObject* list, *value;
	
    DIR *dp;
    struct dirent *dirp;
	
	if (!PyArg_ParseTuple(args, "|s:getBlendFileList", &searchpath))
		return NULL;
	
	list = PyList_New(0);
	
	if (searchpath) {
		BLI_strncpy(cpath, searchpath, FILE_MAXDIR + FILE_MAXFILE);
		BLI_path_abs(cpath, gp_GamePythonPath);
	} else {
		/* Get the dir only */
		BLI_split_dirfile(gp_GamePythonPath, cpath, NULL);
	}
	
    if((dp  = opendir(cpath)) == NULL) {
		/* todo, show the errno, this shouldnt happen anyway if the blendfile is readable */
		fprintf(stderr, "Could not read directoty (%s) failed, code %d (%s)\n", cpath, errno, strerror(errno));
		return list;
    }
	
    while ((dirp = readdir(dp)) != NULL) {
		if (BLI_testextensie(dirp->d_name, ".blend")) {
			value = PyUnicode_FromString(dirp->d_name);
			PyList_Append(list, value);
			Py_DECREF(value);
		}
    }
	
    closedir(dp);
    return list;
}

static char gPyAddScene_doc[] = 
"addScene(name, [overlay])\n\
adds a scene to the game engine\n\
name = Name of the scene\n\
overlay = Overlay or underlay";
static PyObject* gPyAddScene(PyObject*, PyObject* args)
{
	char* name;
	int overlay = 1;
	
	if (!PyArg_ParseTuple(args, "s|i:addScene", &name , &overlay))
		return NULL;
	
	gp_KetsjiEngine->ConvertAndAddScene(name, (overlay != 0));

	Py_RETURN_NONE;
}

static const char *gPyGetCurrentScene_doc =
"getCurrentScene()\n"
"Gets a reference to the current scene.\n";
static PyObject* gPyGetCurrentScene(PyObject* self)
{
	return gp_KetsjiScene->GetProxy();
}

static const char *gPyGetSceneList_doc =
"getSceneList()\n"
"Return a list of converted scenes.\n";
static PyObject* gPyGetSceneList(PyObject* self)
{
	KX_KetsjiEngine* m_engine = KX_GetActiveEngine();
	PyObject* list;
	KX_SceneList* scenes = m_engine->CurrentScenes();
	int numScenes = scenes->size();
	int i;
	
	list = PyList_New(numScenes);
	
	for (i=0;i<numScenes;i++)
	{
		KX_Scene* scene = scenes->at(i);
		PyList_SET_ITEM(list, i, scene->GetProxy());
	}

	return list;
}

static PyObject *pyPrintStats(PyObject *,PyObject *,PyObject *)
{
	gp_KetsjiScene->GetSceneConverter()->PrintStats();
	Py_RETURN_NONE;
}

static PyObject *pyPrintExt(PyObject *,PyObject *,PyObject *)
{
#define pprint(x) std::cout << x << std::endl;
	bool count=0;
	bool support=0;
	pprint("Supported Extensions...");
	pprint(" GL_ARB_shader_objects supported?       "<< (GLEW_ARB_shader_objects?"yes.":"no."));
	count = 1;

	support= GLEW_ARB_vertex_shader;
	pprint(" GL_ARB_vertex_shader supported?        "<< (support?"yes.":"no."));
	count = 1;
	if(support){
		pprint(" ----------Details----------");
		int max=0;
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, (GLint*)&max);
		pprint("  Max uniform components." << max);

		glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, (GLint*)&max);
		pprint("  Max varying floats." << max);

		glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, (GLint*)&max);
		pprint("  Max vertex texture units." << max);
	
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, (GLint*)&max);
		pprint("  Max combined texture units." << max);
		pprint("");
	}

	support=GLEW_ARB_fragment_shader;
	pprint(" GL_ARB_fragment_shader supported?      "<< (support?"yes.":"no."));
	count = 1;
	if(support){
		pprint(" ----------Details----------");
		int max=0;
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, (GLint*)&max);
		pprint("  Max uniform components." << max);
		pprint("");
	}

	support = GLEW_ARB_texture_cube_map;
	pprint(" GL_ARB_texture_cube_map supported?     "<< (support?"yes.":"no."));
	count = 1;
	if(support){
		pprint(" ----------Details----------");
		int size=0;
		glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, (GLint*)&size);
		pprint("  Max cubemap size." << size);
		pprint("");
	}

	support = GLEW_ARB_multitexture;
	count = 1;
	pprint(" GL_ARB_multitexture supported?         "<< (support?"yes.":"no."));
	if(support){
		pprint(" ----------Details----------");
		int units=0;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&units);
		pprint("  Max texture units available.  " << units);
		pprint("");
	}

	pprint(" GL_ARB_texture_env_combine supported?  "<< (GLEW_ARB_texture_env_combine?"yes.":"no."));
	count = 1;

	if(!count)
		pprint("No extenstions are used in this build");

	Py_RETURN_NONE;
}

static PyObject *gLibLoad(PyObject*, PyObject* args)
{
	KX_Scene *kx_scene= gp_KetsjiScene;
	char *path;
	char *group;
	char *err_str= NULL;
	
	if (!PyArg_ParseTuple(args,"ss:LibLoad",&path, &group))
		return NULL;

	if(kx_scene->GetSceneConverter()->LinkBlendFile(path, group, kx_scene, &err_str)) {
		Py_RETURN_TRUE;
	}
	
	if(err_str) {
		PyErr_SetString(PyExc_ValueError, err_str);
		return NULL;
	}
	
	Py_RETURN_FALSE;
}

static PyObject *gLibNew(PyObject*, PyObject* args)
{
	KX_Scene *kx_scene= gp_KetsjiScene;
	char *path;
	char *group;
	char *name;
	PyObject *names;
	int idcode;

	if (!PyArg_ParseTuple(args,"ssO!:LibNew",&path, &group, &PyList_Type, &names))
		return NULL;
	
	if(kx_scene->GetSceneConverter()->GetMainDynamicPath(path))
	{
		PyErr_SetString(PyExc_KeyError, "the name of the path given exists");
		return NULL;
	}
	
	idcode= BLO_idcode_from_name(group);
	if(idcode==0) {
		PyErr_Format(PyExc_ValueError, "invalid group given \"%s\"", group);
		return NULL;
	}
	
	Main *maggie= (Main *)MEM_callocN( sizeof(Main), "BgeMain");
	kx_scene->GetSceneConverter()->GetMainDynamic().push_back(maggie);
	strncpy(maggie->name, path, sizeof(maggie->name)-1);
	
	/* Copy the object into main */
	if(idcode==ID_ME) {
		PyObject *ret= PyList_New(0);
		PyObject *item;
		for(int i= 0; i < PyList_GET_SIZE(names); i++) {
			name= _PyUnicode_AsString(PyList_GET_ITEM(names, i));
			if(name) {
				RAS_MeshObject *meshobj= kx_scene->GetSceneConverter()->ConvertMeshSpecial(kx_scene, maggie, name);
				if(meshobj) {
					KX_MeshProxy* meshproxy = new KX_MeshProxy(meshobj);
					item= meshproxy->NewProxy(true);
					PyList_Append(ret, item);
					Py_DECREF(item);
				}
			}
			else {
				PyErr_Clear(); /* wasnt a string, ignore for now */
			}
		}
		
		return ret;
	}
	else {
		PyErr_Format(PyExc_ValueError, "only \"Mesh\" group currently supported");
		return NULL;
	}
	
	Py_RETURN_NONE;
}

static PyObject *gLibFree(PyObject*, PyObject* args)
{
	KX_Scene *kx_scene= gp_KetsjiScene;
	char *path;

	if (!PyArg_ParseTuple(args,"s:LibFree",&path))
		return NULL;

	if (kx_scene->GetSceneConverter()->FreeBlendFile(path))
	{
		Py_RETURN_TRUE;
	}
	else {
		Py_RETURN_FALSE;
	}
}

static PyObject *gLibList(PyObject*, PyObject* args)
{
	vector<Main*> &dynMaggie = gp_KetsjiScene->GetSceneConverter()->GetMainDynamic();
	int i= 0;
	PyObject *list= PyList_New(dynMaggie.size());
	
	for (vector<Main*>::iterator it=dynMaggie.begin(); !(it==dynMaggie.end()); it++)
	{
		PyList_SET_ITEM(list, i++, PyUnicode_FromString( (*it)->name) );
	}
	
	return list;
}

static struct PyMethodDef game_methods[] = {
	{"expandPath", (PyCFunction)gPyExpandPath, METH_VARARGS, (const char *)gPyExpandPath_doc},
	{"startGame", (PyCFunction)gPyStartGame, METH_VARARGS, (const char *)gPyStartGame_doc},
	{"endGame", (PyCFunction)gPyEndGame, METH_NOARGS, (const char *)gPyEndGame_doc},
	{"restartGame", (PyCFunction)gPyRestartGame, METH_NOARGS, (const char *)gPyRestartGame_doc},
	{"saveGlobalDict", (PyCFunction)gPySaveGlobalDict, METH_NOARGS, (const char *)gPySaveGlobalDict_doc},
	{"loadGlobalDict", (PyCFunction)gPyLoadGlobalDict, METH_NOARGS, (const char *)gPyLoadGlobalDict_doc},
	{"sendMessage", (PyCFunction)gPySendMessage, METH_VARARGS, (const char *)gPySendMessage_doc},
	{"getCurrentController", (PyCFunction) SCA_PythonController::sPyGetCurrentController, METH_NOARGS, SCA_PythonController::sPyGetCurrentController__doc__},
	{"getCurrentScene", (PyCFunction) gPyGetCurrentScene, METH_NOARGS, gPyGetCurrentScene_doc},
	{"getSceneList", (PyCFunction) gPyGetSceneList, METH_NOARGS, (const char *)gPyGetSceneList_doc},
	{"addScene", (PyCFunction)gPyAddScene, METH_VARARGS, (const char *)gPyAddScene_doc},
	{"getRandomFloat",(PyCFunction) gPyGetRandomFloat, METH_NOARGS, (const char *)gPyGetRandomFloat_doc},
	{"setGravity",(PyCFunction) gPySetGravity, METH_O, (const char *)"set Gravitation"},
	{"getSpectrum",(PyCFunction) gPyGetSpectrum, METH_NOARGS, (const char *)"get audio spectrum"},
	{"stopDSP",(PyCFunction) gPyStopDSP, METH_VARARGS, (const char *)"stop using the audio dsp (for performance reasons)"},
	{"getMaxLogicFrame", (PyCFunction) gPyGetMaxLogicFrame, METH_NOARGS, (const char *)"Gets the max number of logic frame per render frame"},
	{"setMaxLogicFrame", (PyCFunction) gPySetMaxLogicFrame, METH_VARARGS, (const char *)"Sets the max number of logic frame per render frame"},
	{"getMaxPhysicsFrame", (PyCFunction) gPyGetMaxPhysicsFrame, METH_NOARGS, (const char *)"Gets the max number of physics frame per render frame"},
	{"setMaxPhysicsFrame", (PyCFunction) gPySetMaxPhysicsFrame, METH_VARARGS, (const char *)"Sets the max number of physics farme per render frame"},
	{"getLogicTicRate", (PyCFunction) gPyGetLogicTicRate, METH_NOARGS, (const char *)"Gets the logic tic rate"},
	{"setLogicTicRate", (PyCFunction) gPySetLogicTicRate, METH_VARARGS, (const char *)"Sets the logic tic rate"},
	{"getPhysicsTicRate", (PyCFunction) gPyGetPhysicsTicRate, METH_NOARGS, (const char *)"Gets the physics tic rate"},
	{"setPhysicsTicRate", (PyCFunction) gPySetPhysicsTicRate, METH_VARARGS, (const char *)"Sets the physics tic rate"},
	{"getAverageFrameRate", (PyCFunction) gPyGetAverageFrameRate, METH_NOARGS, (const char *)"Gets the estimated average frame rate"},
	{"getBlendFileList", (PyCFunction)gPyGetBlendFileList, METH_VARARGS, (const char *)"Gets a list of blend files in the same directory as the current blend file"},
	{"PrintGLInfo", (PyCFunction)pyPrintExt, METH_NOARGS, (const char *)"Prints GL Extension Info"},
	{"PrintMemInfo", (PyCFunction)pyPrintStats, METH_NOARGS, (const char *)"Print engine stastics"},
	
	/* library functions */
	{"LibLoad", (PyCFunction)gLibLoad, METH_VARARGS, (const char *)""},
	{"LibNew", (PyCFunction)gLibNew, METH_VARARGS, (const char *)""},
	{"LibFree", (PyCFunction)gLibFree, METH_VARARGS, (const char *)""},
	{"LibList", (PyCFunction)gLibList, METH_VARARGS, (const char *)""},
	
	{NULL, (PyCFunction) NULL, 0, NULL }
};

static PyObject* gPyGetWindowHeight(PyObject*, PyObject* args)
{
	return PyLong_FromSsize_t((gp_Canvas ? gp_Canvas->GetHeight() : 0));
}



static PyObject* gPyGetWindowWidth(PyObject*, PyObject* args)
{
	return PyLong_FromSsize_t((gp_Canvas ? gp_Canvas->GetWidth() : 0));
}



// temporarility visibility thing, will be moved to rasterizer/renderer later
bool gUseVisibilityTemp = false;

static PyObject* gPyEnableVisibility(PyObject*, PyObject* args)
{
	int visible;
	if (!PyArg_ParseTuple(args,"i:enableVisibility",&visible))
		return NULL;
	
	gUseVisibilityTemp = (visible != 0);
	Py_RETURN_NONE;
}



static PyObject* gPyShowMouse(PyObject*, PyObject* args)
{
	int visible;
	if (!PyArg_ParseTuple(args,"i:showMouse",&visible))
		return NULL;
	
	if (visible)
	{
		if (gp_Canvas)
			gp_Canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
	} else
	{
		if (gp_Canvas)
			gp_Canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
	}
	
	Py_RETURN_NONE;
}



static PyObject* gPySetMousePosition(PyObject*, PyObject* args)
{
	int x,y;
	if (!PyArg_ParseTuple(args,"ii:setMousePosition",&x,&y))
		return NULL;
	
	if (gp_Canvas)
		gp_Canvas->SetMousePosition(x,y);
	
	Py_RETURN_NONE;
}

static PyObject* gPySetEyeSeparation(PyObject*, PyObject* args)
{
	float sep;
	if (!PyArg_ParseTuple(args, "f:setEyeSeparation", &sep))
		return NULL;

	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setEyeSeparation(float), Rasterizer not available");
		return NULL;
	}
	
	gp_Rasterizer->SetEyeSeparation(sep);
	
	Py_RETURN_NONE;
}

static PyObject* gPyGetEyeSeparation(PyObject*)
{
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.getEyeSeparation(), Rasterizer not available");
		return NULL;
	}
	
	return PyFloat_FromDouble(gp_Rasterizer->GetEyeSeparation());
}

static PyObject* gPySetFocalLength(PyObject*, PyObject* args)
{
	float focus;
	if (!PyArg_ParseTuple(args, "f:setFocalLength", &focus))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setFocalLength(float), Rasterizer not available");
		return NULL;
	}

	gp_Rasterizer->SetFocalLength(focus);
	
	Py_RETURN_NONE;
}

static PyObject* gPyGetFocalLength(PyObject*, PyObject*, PyObject*)
{
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.getFocalLength(), Rasterizer not available");
		return NULL;
	}
	
	return PyFloat_FromDouble(gp_Rasterizer->GetFocalLength());
	
	Py_RETURN_NONE;
}

static PyObject* gPySetBackgroundColor(PyObject*, PyObject* value)
{
	
	MT_Vector4 vec;
	if (!PyVecTo(value, vec))
		return NULL;
	
	if (gp_Canvas)
	{
		gp_Rasterizer->SetBackColor(vec[0], vec[1], vec[2], vec[3]);
	}
	Py_RETURN_NONE;
}



static PyObject* gPySetMistColor(PyObject*, PyObject* value)
{
	
	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setMistColor(color), Rasterizer not available");
		return NULL;
	}	
	gp_Rasterizer->SetFogColor(vec[0], vec[1], vec[2]);
	
	Py_RETURN_NONE;
}

static PyObject* gPyDisableMist(PyObject*)
{
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setMistColor(color), Rasterizer not available");
		return NULL;
	}	
	gp_Rasterizer->DisableFog();
	
	Py_RETURN_NONE;
}


static PyObject* gPySetMistStart(PyObject*, PyObject* args)
{

	float miststart;
	if (!PyArg_ParseTuple(args,"f:setMistStart",&miststart))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setMistStart(float), Rasterizer not available");
		return NULL;
	}
	
	gp_Rasterizer->SetFogStart(miststart);
	
	Py_RETURN_NONE;
}



static PyObject* gPySetMistEnd(PyObject*, PyObject* args)
{

	float mistend;
	if (!PyArg_ParseTuple(args,"f:setMistEnd",&mistend))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setMistEnd(float), Rasterizer not available");
		return NULL;
	}
	
	gp_Rasterizer->SetFogEnd(mistend);
	
	Py_RETURN_NONE;
}


static PyObject* gPySetAmbientColor(PyObject*, PyObject* value)
{
	
	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.setAmbientColor(color), Rasterizer not available");
		return NULL;
	}	
	gp_Rasterizer->SetAmbientColor(vec[0], vec[1], vec[2]);
	
	Py_RETURN_NONE;
}




static PyObject* gPyMakeScreenshot(PyObject*, PyObject* args)
{
	char* filename;
	if (!PyArg_ParseTuple(args,"s:makeScreenshot",&filename))
		return NULL;
	
	if (gp_Canvas)
	{
		gp_Canvas->MakeScreenShot(filename);
	}
	
	Py_RETURN_NONE;
}

static PyObject* gPyEnableMotionBlur(PyObject*, PyObject* args)
{
	float motionblurvalue;
	if (!PyArg_ParseTuple(args,"f:enableMotionBlur",&motionblurvalue))
		return NULL;
	
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.enableMotionBlur(float), Rasterizer not available");
		return NULL;
	}
	
	gp_Rasterizer->EnableMotionBlur(motionblurvalue);
	
	Py_RETURN_NONE;
}

static PyObject* gPyDisableMotionBlur(PyObject*)
{
	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.disableMotionBlur(), Rasterizer not available");
		return NULL;
	}
	
	gp_Rasterizer->DisableMotionBlur();
	
	Py_RETURN_NONE;
}

int getGLSLSettingFlag(char *setting)
{
	if(strcmp(setting, "lights") == 0)
		return GAME_GLSL_NO_LIGHTS;
	else if(strcmp(setting, "shaders") == 0)
		return GAME_GLSL_NO_SHADERS;
	else if(strcmp(setting, "shadows") == 0)
		return GAME_GLSL_NO_SHADOWS;
	else if(strcmp(setting, "ramps") == 0)
		return GAME_GLSL_NO_RAMPS;
	else if(strcmp(setting, "nodes") == 0)
		return GAME_GLSL_NO_NODES;
	else if(strcmp(setting, "extra_textures") == 0)
		return GAME_GLSL_NO_EXTRA_TEX;
	else
		return -1;
}

static PyObject* gPySetGLSLMaterialSetting(PyObject*,
											PyObject* args,
											PyObject*)
{
	GameData *gm= &(gp_KetsjiScene->GetBlenderScene()->gm);
	char *setting;
	int enable, flag, sceneflag;

	if (!PyArg_ParseTuple(args,"si:setGLSLMaterialSetting",&setting,&enable))
		return NULL;
	
	flag = getGLSLSettingFlag(setting);
	
	if  (flag==-1) {
		PyErr_SetString(PyExc_ValueError, "Rasterizer.setGLSLMaterialSetting(string): glsl setting is not known");
		return NULL;
	}

	sceneflag= gm->flag;
	
	if (enable)
		gm->flag &= ~flag;
	else
		gm->flag |= flag;

	/* display lists and GLSL materials need to be remade */
	if(sceneflag != gm->flag) {
		GPU_materials_free();
		if(gp_KetsjiEngine) {
			KX_SceneList *scenes = gp_KetsjiEngine->CurrentScenes();
			KX_SceneList::iterator it;

			for(it=scenes->begin(); it!=scenes->end(); it++)
				if((*it)->GetBucketManager()) {
					(*it)->GetBucketManager()->ReleaseDisplayLists();
					(*it)->GetBucketManager()->ReleaseMaterials();
				}
		}
	}

	Py_RETURN_NONE;
}

static PyObject* gPyGetGLSLMaterialSetting(PyObject*, 
									 PyObject* args, 
									 PyObject*)
{
	GameData *gm= &(gp_KetsjiScene->GetBlenderScene()->gm);
	char *setting;
	int enabled = 0, flag;

	if (!PyArg_ParseTuple(args,"s:getGLSLMaterialSetting",&setting))
		return NULL;
	
	flag = getGLSLSettingFlag(setting);
	
	if  (flag==-1) {
		PyErr_SetString(PyExc_ValueError, "Rasterizer.getGLSLMaterialSetting(string): glsl setting is not known");
		return NULL;
	}

	enabled = ((gm->flag & flag) != 0);
	return PyLong_FromSsize_t(enabled);
}

#define KX_TEXFACE_MATERIAL				0
#define KX_BLENDER_MULTITEX_MATERIAL	1
#define KX_BLENDER_GLSL_MATERIAL		2

static PyObject* gPySetMaterialType(PyObject*,
									PyObject* args,
									PyObject*)
{
	GameData *gm= &(gp_KetsjiScene->GetBlenderScene()->gm);
	int type;

	if (!PyArg_ParseTuple(args,"i:setMaterialType",&type))
		return NULL;

	if(type == KX_BLENDER_GLSL_MATERIAL)
		gm->matmode= GAME_MAT_GLSL;
	else if(type == KX_BLENDER_MULTITEX_MATERIAL)
		gm->matmode= GAME_MAT_MULTITEX;
	else if(type == KX_TEXFACE_MATERIAL)
		gm->matmode= GAME_MAT_TEXFACE;
	else {
		PyErr_SetString(PyExc_ValueError, "Rasterizer.setMaterialType(int): material type is not known");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject* gPyGetMaterialType(PyObject*)
{
	GameData *gm= &(gp_KetsjiScene->GetBlenderScene()->gm);
	int flag;

	if(gm->matmode == GAME_MAT_GLSL)
		flag = KX_BLENDER_GLSL_MATERIAL;
	else if(gm->matmode == GAME_MAT_MULTITEX)
		flag = KX_BLENDER_MULTITEX_MATERIAL;
	else
		flag = KX_TEXFACE_MATERIAL;
	
	return PyLong_FromSsize_t(flag);
}

static PyObject* gPyDrawLine(PyObject*, PyObject* args)
{
	PyObject* ob_from;
	PyObject* ob_to;
	PyObject* ob_color;

	if (!gp_Rasterizer) {
		PyErr_SetString(PyExc_RuntimeError, "Rasterizer.drawLine(obFrom, obTo, color): Rasterizer not available");
		return NULL;
	}

	if (!PyArg_ParseTuple(args,"OOO:drawLine",&ob_from,&ob_to,&ob_color))
		return NULL;

	MT_Vector3 from;
	MT_Vector3 to;
	MT_Vector3 color;
	if (!PyVecTo(ob_from, from))
		return NULL;
	if (!PyVecTo(ob_to, to))
		return NULL;
	if (!PyVecTo(ob_color, color))
		return NULL;

	gp_Rasterizer->DrawDebugLine(from,to,color);
	
	Py_RETURN_NONE;
}

static struct PyMethodDef rasterizer_methods[] = {
  {"getWindowWidth",(PyCFunction) gPyGetWindowWidth,
   METH_VARARGS, "getWindowWidth doc"},
   {"getWindowHeight",(PyCFunction) gPyGetWindowHeight,
   METH_VARARGS, "getWindowHeight doc"},
  {"makeScreenshot",(PyCFunction)gPyMakeScreenshot,
	METH_VARARGS, "make Screenshot doc"},
   {"enableVisibility",(PyCFunction) gPyEnableVisibility,
   METH_VARARGS, "enableVisibility doc"},
	{"showMouse",(PyCFunction) gPyShowMouse,
   METH_VARARGS, "showMouse(bool visible)"},
   {"setMousePosition",(PyCFunction) gPySetMousePosition,
   METH_VARARGS, "setMousePosition(int x,int y)"},
  {"setBackgroundColor",(PyCFunction)gPySetBackgroundColor,METH_O,"set Background Color (rgb)"},
	{"setAmbientColor",(PyCFunction)gPySetAmbientColor,METH_O,"set Ambient Color (rgb)"},
 {"disableMist",(PyCFunction)gPyDisableMist,METH_NOARGS,"turn off mist"},
 {"setMistColor",(PyCFunction)gPySetMistColor,METH_O,"set Mist Color (rgb)"},
  {"setMistStart",(PyCFunction)gPySetMistStart,METH_VARARGS,"set Mist Start(rgb)"},
  {"setMistEnd",(PyCFunction)gPySetMistEnd,METH_VARARGS,"set Mist End(rgb)"},
  {"enableMotionBlur",(PyCFunction)gPyEnableMotionBlur,METH_VARARGS,"enable motion blur"},
  {"disableMotionBlur",(PyCFunction)gPyDisableMotionBlur,METH_NOARGS,"disable motion blur"},

  
  {"setEyeSeparation", (PyCFunction) gPySetEyeSeparation, METH_VARARGS, "set the eye separation for stereo mode"},
  {"getEyeSeparation", (PyCFunction) gPyGetEyeSeparation, METH_NOARGS, "get the eye separation for stereo mode"},
  {"setFocalLength", (PyCFunction) gPySetFocalLength, METH_VARARGS, "set the focal length for stereo mode"},
  {"getFocalLength", (PyCFunction) gPyGetFocalLength, METH_VARARGS, "get the focal length for stereo mode"},
  {"setMaterialMode",(PyCFunction) gPySetMaterialType,
   METH_VARARGS, "set the material mode to use for OpenGL rendering"},
  {"getMaterialMode",(PyCFunction) gPyGetMaterialType,
   METH_NOARGS, "get the material mode being used for OpenGL rendering"},
  {"setGLSLMaterialSetting",(PyCFunction) gPySetGLSLMaterialSetting,
   METH_VARARGS, "set the state of a GLSL material setting"},
  {"getGLSLMaterialSetting",(PyCFunction) gPyGetGLSLMaterialSetting,
   METH_VARARGS, "get the state of a GLSL material setting"},
  {"drawLine", (PyCFunction) gPyDrawLine,
   METH_VARARGS, "draw a line on the screen"},
  { NULL, (PyCFunction) NULL, 0, NULL }
};

// Initialization function for the module (*must* be called initGameLogic)

static char GameLogic_module_documentation[] =
"This is the Python API for the game engine of GameLogic"
;

static char Rasterizer_module_documentation[] =
"This is the Python API for the game engine of Rasterizer"
;

static struct PyModuleDef GameLogic_module_def = {
	{}, /* m_base */
	"GameLogic",  /* m_name */
	GameLogic_module_documentation,  /* m_doc */
	0,  /* m_size */
	game_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject* initGameLogic(KX_KetsjiEngine *engine, KX_Scene* scene) // quick hack to get gravity hook
{
	PyObject* m;
	PyObject* d;
	PyObject* item; /* temp PyObject* storage */
	
	gp_KetsjiEngine = engine;
	gp_KetsjiScene = scene;

	gUseVisibilityTemp=false;
	
	PyObjectPlus::ClearDeprecationWarning(); /* Not that nice to call here but makes sure warnings are reset between loading scenes */
	
	/* Use existing module where possible
	 * be careful not to init any runtime vars after this */
	m = PyImport_ImportModule( "GameLogic" );
	if(m) {
		Py_DECREF(m);
		return m;
	}
	else {
		PyErr_Clear();
		// Create the module and add the functions	
		m = PyModule_Create(&GameLogic_module_def);
		PyDict_SetItemString(PySys_GetObject("modules"), GameLogic_module_def.m_name, m);
	}
	
	// Add some symbolic constants to the module
	d = PyModule_GetDict(m);
	
	// can be overwritten later for gameEngine instances that can load new blend files and re-initialize this module
	// for now its safe to make sure it exists for other areas such as the web plugin
	
	PyDict_SetItemString(d, "globalDict", item=PyDict_New()); Py_DECREF(item);

	// Add keyboard and mouse attributes to this module
	SCA_PythonKeyboard* pykeyb = new SCA_PythonKeyboard(gp_KetsjiEngine->GetKeyboardDevice());
	PyDict_SetItemString(d, "keyboard", pykeyb->NewProxy(true));

	SCA_PythonMouse* pymouse = new SCA_PythonMouse(gp_KetsjiEngine->GetMouseDevice(), gp_Canvas);
	PyDict_SetItemString(d, "mouse", pymouse->NewProxy(true));

	ErrorObject = PyUnicode_FromString("GameLogic.error");
	PyDict_SetItemString(d, "error", ErrorObject);
	Py_DECREF(ErrorObject);
	
	// XXXX Add constants here
	/* To use logic bricks, we need some sort of constants. Here, we associate */
	/* constants and sumbolic names. Add them to dictionary d.                 */

	/* 1. true and false: needed for everyone                                  */
	KX_MACRO_addTypesToDict(d, KX_TRUE,  SCA_ILogicBrick::KX_TRUE);
	KX_MACRO_addTypesToDict(d, KX_FALSE, SCA_ILogicBrick::KX_FALSE);

	/* 2. Property sensor                                                      */
	KX_MACRO_addTypesToDict(d, KX_PROPSENSOR_EQUAL,      SCA_PropertySensor::KX_PROPSENSOR_EQUAL);
	KX_MACRO_addTypesToDict(d, KX_PROPSENSOR_NOTEQUAL,   SCA_PropertySensor::KX_PROPSENSOR_NOTEQUAL);
	KX_MACRO_addTypesToDict(d, KX_PROPSENSOR_INTERVAL,   SCA_PropertySensor::KX_PROPSENSOR_INTERVAL);
	KX_MACRO_addTypesToDict(d, KX_PROPSENSOR_CHANGED,    SCA_PropertySensor::KX_PROPSENSOR_CHANGED);
	KX_MACRO_addTypesToDict(d, KX_PROPSENSOR_EXPRESSION, SCA_PropertySensor::KX_PROPSENSOR_EXPRESSION);

	/* 3. Constraint actuator                                                  */
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_LOCX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCX);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_LOCY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_LOCZ, KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCZ);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ROTX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTX);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ROTY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ROTZ, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ROTZ);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRPX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPX);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRPY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRPY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRPY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRNX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNX);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRNY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_DIRNY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DIRNY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ORIX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIX);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ORIY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIY);
	KX_MACRO_addTypesToDict(d, KX_CONSTRAINTACT_ORIZ, KX_ConstraintActuator::KX_ACT_CONSTRAINT_ORIZ);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHPX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPX);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHPY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPY);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHPZ, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHPZ);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHNX, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNX);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHNY, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNY);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_FHNZ, KX_ConstraintActuator::KX_ACT_CONSTRAINT_FHNZ);

	/* 4. Ipo actuator, simple part                                            */
	KX_MACRO_addTypesToDict(d, KX_IPOACT_PLAY,     KX_IpoActuator::KX_ACT_IPO_PLAY);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_PINGPONG, KX_IpoActuator::KX_ACT_IPO_PINGPONG);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_FLIPPER,  KX_IpoActuator::KX_ACT_IPO_FLIPPER);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_LOOPSTOP, KX_IpoActuator::KX_ACT_IPO_LOOPSTOP);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_LOOPEND,  KX_IpoActuator::KX_ACT_IPO_LOOPEND);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_FROM_PROP,KX_IpoActuator::KX_ACT_IPO_FROM_PROP);

	/* 5. Random distribution types                                            */
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_BOOL_CONST,      SCA_RandomActuator::KX_RANDOMACT_BOOL_CONST);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_BOOL_UNIFORM,    SCA_RandomActuator::KX_RANDOMACT_BOOL_UNIFORM);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_BOOL_BERNOUILLI, SCA_RandomActuator::KX_RANDOMACT_BOOL_BERNOUILLI);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_INT_CONST,       SCA_RandomActuator::KX_RANDOMACT_INT_CONST);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_INT_UNIFORM,     SCA_RandomActuator::KX_RANDOMACT_INT_UNIFORM);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_INT_POISSON,     SCA_RandomActuator::KX_RANDOMACT_INT_POISSON);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_FLOAT_CONST,     SCA_RandomActuator::KX_RANDOMACT_FLOAT_CONST);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_FLOAT_UNIFORM,   SCA_RandomActuator::KX_RANDOMACT_FLOAT_UNIFORM);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_FLOAT_NORMAL,    SCA_RandomActuator::KX_RANDOMACT_FLOAT_NORMAL);
	KX_MACRO_addTypesToDict(d, KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL, SCA_RandomActuator::KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL);

	/* 6. Sound actuator                                                      */
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_PLAYSTOP,              KX_SoundActuator::KX_SOUNDACT_PLAYSTOP);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_PLAYEND,               KX_SoundActuator::KX_SOUNDACT_PLAYEND);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPSTOP,              KX_SoundActuator::KX_SOUNDACT_LOOPSTOP);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPEND,               KX_SoundActuator::KX_SOUNDACT_LOOPEND);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPBIDIRECTIONAL,     KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP,     KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP);

	/* 7. Action actuator													   */
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_PLAY,        ACT_ACTION_PLAY);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_FLIPPER,     ACT_ACTION_FLIPPER);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_LOOPSTOP,    ACT_ACTION_LOOP_STOP);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_LOOPEND,     ACT_ACTION_LOOP_END);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_PROPERTY,    ACT_ACTION_FROM_PROP);
	
	/*8. GL_BlendFunc */
	KX_MACRO_addTypesToDict(d, BL_ZERO, GL_ZERO);
	KX_MACRO_addTypesToDict(d, BL_ONE, GL_ONE);
	KX_MACRO_addTypesToDict(d, BL_SRC_COLOR, GL_SRC_COLOR);
	KX_MACRO_addTypesToDict(d, BL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
	KX_MACRO_addTypesToDict(d, BL_DST_COLOR, GL_DST_COLOR);
	KX_MACRO_addTypesToDict(d, BL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_DST_COLOR);
	KX_MACRO_addTypesToDict(d, BL_SRC_ALPHA, GL_SRC_ALPHA);
	KX_MACRO_addTypesToDict(d, BL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	KX_MACRO_addTypesToDict(d, BL_DST_ALPHA, GL_DST_ALPHA);
	KX_MACRO_addTypesToDict(d, BL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
	KX_MACRO_addTypesToDict(d, BL_SRC_ALPHA_SATURATE, GL_SRC_ALPHA_SATURATE);


	/* 9. UniformTypes */
	KX_MACRO_addTypesToDict(d, SHD_TANGENT, BL_Shader::SHD_TANGENT);
	KX_MACRO_addTypesToDict(d, MODELVIEWMATRIX, BL_Shader::MODELVIEWMATRIX);
	KX_MACRO_addTypesToDict(d, MODELVIEWMATRIX_TRANSPOSE, BL_Shader::MODELVIEWMATRIX_TRANSPOSE);
	KX_MACRO_addTypesToDict(d, MODELVIEWMATRIX_INVERSE, BL_Shader::MODELVIEWMATRIX_INVERSE);
	KX_MACRO_addTypesToDict(d, MODELVIEWMATRIX_INVERSETRANSPOSE, BL_Shader::MODELVIEWMATRIX_INVERSETRANSPOSE);
	KX_MACRO_addTypesToDict(d, MODELMATRIX, BL_Shader::MODELMATRIX);
	KX_MACRO_addTypesToDict(d, MODELMATRIX_TRANSPOSE, BL_Shader::MODELMATRIX_TRANSPOSE);
	KX_MACRO_addTypesToDict(d, MODELMATRIX_INVERSE, BL_Shader::MODELMATRIX_INVERSE);
	KX_MACRO_addTypesToDict(d, MODELMATRIX_INVERSETRANSPOSE, BL_Shader::MODELMATRIX_INVERSETRANSPOSE);
	KX_MACRO_addTypesToDict(d, VIEWMATRIX, BL_Shader::VIEWMATRIX);
	KX_MACRO_addTypesToDict(d, VIEWMATRIX_TRANSPOSE, BL_Shader::VIEWMATRIX_TRANSPOSE);
	KX_MACRO_addTypesToDict(d, VIEWMATRIX_INVERSE, BL_Shader::VIEWMATRIX_INVERSE);
	KX_MACRO_addTypesToDict(d, VIEWMATRIX_INVERSETRANSPOSE, BL_Shader::VIEWMATRIX_INVERSETRANSPOSE);
	KX_MACRO_addTypesToDict(d, CAM_POS, BL_Shader::CAM_POS);
	KX_MACRO_addTypesToDict(d, CONSTANT_TIMER, BL_Shader::CONSTANT_TIMER);

	/* 10 state actuator */
	KX_MACRO_addTypesToDict(d, KX_STATE1, (1<<0));
	KX_MACRO_addTypesToDict(d, KX_STATE2, (1<<1));
	KX_MACRO_addTypesToDict(d, KX_STATE3, (1<<2));
	KX_MACRO_addTypesToDict(d, KX_STATE4, (1<<3));
	KX_MACRO_addTypesToDict(d, KX_STATE5, (1<<4));
	KX_MACRO_addTypesToDict(d, KX_STATE6, (1<<5));
	KX_MACRO_addTypesToDict(d, KX_STATE7, (1<<6));
	KX_MACRO_addTypesToDict(d, KX_STATE8, (1<<7));
	KX_MACRO_addTypesToDict(d, KX_STATE9, (1<<8));
	KX_MACRO_addTypesToDict(d, KX_STATE10, (1<<9));
	KX_MACRO_addTypesToDict(d, KX_STATE11, (1<<10));
	KX_MACRO_addTypesToDict(d, KX_STATE12, (1<<11));
	KX_MACRO_addTypesToDict(d, KX_STATE13, (1<<12));
	KX_MACRO_addTypesToDict(d, KX_STATE14, (1<<13));
	KX_MACRO_addTypesToDict(d, KX_STATE15, (1<<14));
	KX_MACRO_addTypesToDict(d, KX_STATE16, (1<<15));
	KX_MACRO_addTypesToDict(d, KX_STATE17, (1<<16));
	KX_MACRO_addTypesToDict(d, KX_STATE18, (1<<17));
	KX_MACRO_addTypesToDict(d, KX_STATE19, (1<<18));
	KX_MACRO_addTypesToDict(d, KX_STATE20, (1<<19));
	KX_MACRO_addTypesToDict(d, KX_STATE21, (1<<20));
	KX_MACRO_addTypesToDict(d, KX_STATE22, (1<<21));
	KX_MACRO_addTypesToDict(d, KX_STATE23, (1<<22));
	KX_MACRO_addTypesToDict(d, KX_STATE24, (1<<23));
	KX_MACRO_addTypesToDict(d, KX_STATE25, (1<<24));
	KX_MACRO_addTypesToDict(d, KX_STATE26, (1<<25));
	KX_MACRO_addTypesToDict(d, KX_STATE27, (1<<26));
	KX_MACRO_addTypesToDict(d, KX_STATE28, (1<<27));
	KX_MACRO_addTypesToDict(d, KX_STATE29, (1<<28));
	KX_MACRO_addTypesToDict(d, KX_STATE30, (1<<29));
	
	/* All Sensors */
	KX_MACRO_addTypesToDict(d, KX_SENSOR_JUST_ACTIVATED, SCA_ISensor::KX_SENSOR_JUST_ACTIVATED);
	KX_MACRO_addTypesToDict(d, KX_SENSOR_ACTIVE, SCA_ISensor::KX_SENSOR_ACTIVE);
	KX_MACRO_addTypesToDict(d, KX_SENSOR_JUST_DEACTIVATED, SCA_ISensor::KX_SENSOR_JUST_DEACTIVATED);
	KX_MACRO_addTypesToDict(d, KX_SENSOR_INACTIVE, SCA_ISensor::KX_SENSOR_INACTIVE);
	
	/* Radar Sensor */
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_POS_X, KX_RadarSensor::KX_RADAR_AXIS_POS_X);
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_POS_Y, KX_RadarSensor::KX_RADAR_AXIS_POS_Y);
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_POS_Z, KX_RadarSensor::KX_RADAR_AXIS_POS_Z);
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_NEG_X, KX_RadarSensor::KX_RADAR_AXIS_NEG_Y);
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_NEG_Y, KX_RadarSensor::KX_RADAR_AXIS_NEG_X);
	KX_MACRO_addTypesToDict(d, KX_RADAR_AXIS_NEG_Z, KX_RadarSensor::KX_RADAR_AXIS_NEG_Z);

	/* Ray Sensor */
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_POS_X, KX_RaySensor::KX_RAY_AXIS_POS_X);
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_POS_Y, KX_RaySensor::KX_RAY_AXIS_POS_Y);
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_POS_Z, KX_RaySensor::KX_RAY_AXIS_POS_Z);
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_NEG_X, KX_RaySensor::KX_RAY_AXIS_NEG_Y);
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_NEG_Y, KX_RaySensor::KX_RAY_AXIS_NEG_X);
	KX_MACRO_addTypesToDict(d, KX_RAY_AXIS_NEG_Z, KX_RaySensor::KX_RAY_AXIS_NEG_Z);

	/* Dynamic actuator */
	KX_MACRO_addTypesToDict(d, KX_DYN_RESTORE_DYNAMICS, KX_SCA_DynamicActuator::KX_DYN_RESTORE_DYNAMICS);
	KX_MACRO_addTypesToDict(d, KX_DYN_DISABLE_DYNAMICS, KX_SCA_DynamicActuator::KX_DYN_DISABLE_DYNAMICS);
	KX_MACRO_addTypesToDict(d, KX_DYN_ENABLE_RIGID_BODY, KX_SCA_DynamicActuator::KX_DYN_ENABLE_RIGID_BODY);
	KX_MACRO_addTypesToDict(d, KX_DYN_DISABLE_RIGID_BODY, KX_SCA_DynamicActuator::KX_DYN_DISABLE_RIGID_BODY);
	KX_MACRO_addTypesToDict(d, KX_DYN_SET_MASS, KX_SCA_DynamicActuator::KX_DYN_SET_MASS);

	/* Input & Mouse Sensor */
	KX_MACRO_addTypesToDict(d, KX_INPUT_NONE, SCA_InputEvent::KX_NO_INPUTSTATUS);
	KX_MACRO_addTypesToDict(d, KX_INPUT_JUST_ACTIVATED, SCA_InputEvent::KX_JUSTACTIVATED);
	KX_MACRO_addTypesToDict(d, KX_INPUT_ACTIVE, SCA_InputEvent::KX_ACTIVE);
	KX_MACRO_addTypesToDict(d, KX_INPUT_JUST_RELEASED, SCA_InputEvent::KX_JUSTRELEASED);
	
	KX_MACRO_addTypesToDict(d, KX_MOUSE_BUT_LEFT, SCA_IInputDevice::KX_LEFTMOUSE);
	KX_MACRO_addTypesToDict(d, KX_MOUSE_BUT_MIDDLE, SCA_IInputDevice::KX_MIDDLEMOUSE);
	KX_MACRO_addTypesToDict(d, KX_MOUSE_BUT_RIGHT, SCA_IInputDevice::KX_RIGHTMOUSE);

	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_ENABLED, RAS_2DFilterManager::RAS_2DFILTER_ENABLED);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_DISABLED, RAS_2DFilterManager::RAS_2DFILTER_DISABLED);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_NOFILTER, RAS_2DFilterManager::RAS_2DFILTER_NOFILTER);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_MOTIONBLUR, RAS_2DFilterManager::RAS_2DFILTER_MOTIONBLUR);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_BLUR, RAS_2DFilterManager::RAS_2DFILTER_BLUR);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_SHARPEN, RAS_2DFilterManager::RAS_2DFILTER_SHARPEN);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_DILATION, RAS_2DFilterManager::RAS_2DFILTER_DILATION);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_EROSION, RAS_2DFilterManager::RAS_2DFILTER_EROSION);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_LAPLACIAN, RAS_2DFilterManager::RAS_2DFILTER_LAPLACIAN);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_SOBEL, RAS_2DFilterManager::RAS_2DFILTER_SOBEL);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_PREWITT, RAS_2DFilterManager::RAS_2DFILTER_PREWITT);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_GRAYSCALE, RAS_2DFilterManager::RAS_2DFILTER_GRAYSCALE);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_SEPIA, RAS_2DFilterManager::RAS_2DFILTER_SEPIA);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_INVERT, RAS_2DFilterManager::RAS_2DFILTER_INVERT);
	KX_MACRO_addTypesToDict(d, RAS_2DFILTER_CUSTOMFILTER, RAS_2DFilterManager::RAS_2DFILTER_CUSTOMFILTER);
		
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_PLAYSTOP, KX_SoundActuator::KX_SOUNDACT_PLAYSTOP);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_PLAYEND, KX_SoundActuator::KX_SOUNDACT_PLAYEND);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPSTOP, KX_SoundActuator::KX_SOUNDACT_LOOPSTOP);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPEND, KX_SoundActuator:: KX_SOUNDACT_LOOPEND);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL);
	KX_MACRO_addTypesToDict(d, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP, KX_SoundActuator::KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP);

	KX_MACRO_addTypesToDict(d, KX_STATE_OP_CPY, KX_StateActuator::OP_CPY);
	KX_MACRO_addTypesToDict(d, KX_STATE_OP_SET, KX_StateActuator::OP_SET);
	KX_MACRO_addTypesToDict(d, KX_STATE_OP_CLR, KX_StateActuator::OP_CLR);
	KX_MACRO_addTypesToDict(d, KX_STATE_OP_NEG, KX_StateActuator::OP_NEG);

	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_NORMAL, KX_ConstraintActuator::KX_ACT_CONSTRAINT_NORMAL);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_MATERIAL, KX_ConstraintActuator::KX_ACT_CONSTRAINT_MATERIAL);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_PERMANENT, KX_ConstraintActuator::KX_ACT_CONSTRAINT_PERMANENT);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_DISTANCE, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DISTANCE);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_LOCAL, KX_ConstraintActuator::KX_ACT_CONSTRAINT_LOCAL);
	KX_MACRO_addTypesToDict(d, KX_ACT_CONSTRAINT_DOROTFH, KX_ConstraintActuator::KX_ACT_CONSTRAINT_DOROTFH);

	/* Game Actuator Modes */
	KX_MACRO_addTypesToDict(d, KX_GAME_LOAD, KX_GameActuator::KX_GAME_LOAD);
	KX_MACRO_addTypesToDict(d, KX_GAME_START, KX_GameActuator::KX_GAME_START);
	KX_MACRO_addTypesToDict(d, KX_GAME_RESTART, KX_GameActuator::KX_GAME_RESTART);
	KX_MACRO_addTypesToDict(d, KX_GAME_QUIT, KX_GameActuator::KX_GAME_QUIT);
	KX_MACRO_addTypesToDict(d, KX_GAME_SAVECFG, KX_GameActuator::KX_GAME_SAVECFG);
	KX_MACRO_addTypesToDict(d, KX_GAME_LOADCFG, KX_GameActuator::KX_GAME_LOADCFG);

	/* Scene Actuator Modes */
	KX_MACRO_addTypesToDict(d, KX_SCENE_RESTART, KX_SceneActuator::KX_SCENE_RESTART);
	KX_MACRO_addTypesToDict(d, KX_SCENE_SET_SCENE, KX_SceneActuator::KX_SCENE_SET_SCENE);
	KX_MACRO_addTypesToDict(d, KX_SCENE_SET_CAMERA, KX_SceneActuator::KX_SCENE_SET_CAMERA);
	KX_MACRO_addTypesToDict(d, KX_SCENE_ADD_FRONT_SCENE, KX_SceneActuator::KX_SCENE_ADD_FRONT_SCENE);
	KX_MACRO_addTypesToDict(d, KX_SCENE_ADD_BACK_SCENE, KX_SceneActuator::KX_SCENE_ADD_BACK_SCENE);
	KX_MACRO_addTypesToDict(d, KX_SCENE_REMOVE_SCENE, KX_SceneActuator::KX_SCENE_REMOVE_SCENE);
	KX_MACRO_addTypesToDict(d, KX_SCENE_SUSPEND, KX_SceneActuator::KX_SCENE_SUSPEND);
	KX_MACRO_addTypesToDict(d, KX_SCENE_RESUME, KX_SceneActuator::KX_SCENE_RESUME);

	/* Parent Actuator Modes */
	KX_MACRO_addTypesToDict(d, KX_PARENT_SET, KX_ParentActuator::KX_PARENT_SET);
	KX_MACRO_addTypesToDict(d, KX_PARENT_REMOVE, KX_ParentActuator::KX_PARENT_REMOVE);

	/* BL_ArmatureConstraint type */
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_TRACKTO, CONSTRAINT_TYPE_TRACKTO);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_KINEMATIC);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_ROTLIKE, CONSTRAINT_TYPE_ROTLIKE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_LOCLIKE, CONSTRAINT_TYPE_LOCLIKE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_MINMAX, CONSTRAINT_TYPE_MINMAX);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_SIZELIKE, CONSTRAINT_TYPE_SIZELIKE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_LOCKTRACK, CONSTRAINT_TYPE_LOCKTRACK);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_STRETCHTO, CONSTRAINT_TYPE_STRETCHTO);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_CLAMPTO, CONSTRAINT_TYPE_CLAMPTO);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_TRANSFORM, CONSTRAINT_TYPE_TRANSFORM);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_TYPE_DISTLIMIT, CONSTRAINT_TYPE_DISTLIMIT);
	/* BL_ArmatureConstraint ik_type */
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_COPYPOSE, CONSTRAINT_IK_COPYPOSE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_DISTANCE, CONSTRAINT_IK_DISTANCE);
	/* BL_ArmatureConstraint ik_mode */
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_MODE_INSIDE, LIMITDIST_INSIDE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_MODE_OUTSIDE, LIMITDIST_OUTSIDE);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_MODE_ONSURFACE, LIMITDIST_ONSURFACE);
	/* BL_ArmatureConstraint ik_flag */
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_FLAG_TIP, CONSTRAINT_IK_TIP);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_FLAG_ROT, CONSTRAINT_IK_ROT);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_FLAG_STRETCH, CONSTRAINT_IK_STRETCH);
	KX_MACRO_addTypesToDict(d, CONSTRAINT_IK_FLAG_POS, CONSTRAINT_IK_POS);
	/* KX_ArmatureSensor type */
	KX_MACRO_addTypesToDict(d, KX_ARMSENSOR_STATE_CHANGED, SENS_ARM_STATE_CHANGED);
	KX_MACRO_addTypesToDict(d, KX_ARMSENSOR_LIN_ERROR_BELOW, SENS_ARM_LIN_ERROR_BELOW);
	KX_MACRO_addTypesToDict(d, KX_ARMSENSOR_LIN_ERROR_ABOVE, SENS_ARM_LIN_ERROR_ABOVE);
	KX_MACRO_addTypesToDict(d, KX_ARMSENSOR_ROT_ERROR_BELOW, SENS_ARM_ROT_ERROR_BELOW);
	KX_MACRO_addTypesToDict(d, KX_ARMSENSOR_ROT_ERROR_ABOVE, SENS_ARM_ROT_ERROR_ABOVE);

	/* BL_ArmatureActuator type */
	KX_MACRO_addTypesToDict(d, KX_ACT_ARMATURE_RUN, ACT_ARM_RUN);
	KX_MACRO_addTypesToDict(d, KX_ACT_ARMATURE_ENABLE, ACT_ARM_ENABLE);
	KX_MACRO_addTypesToDict(d, KX_ACT_ARMATURE_DISABLE, ACT_ARM_DISABLE);
	KX_MACRO_addTypesToDict(d, KX_ACT_ARMATURE_SETTARGET, ACT_ARM_SETTARGET);
	KX_MACRO_addTypesToDict(d, KX_ACT_ARMATURE_SETWEIGHT, ACT_ARM_SETWEIGHT);

	/* BL_Armature Channel rotation_mode */
	KX_MACRO_addTypesToDict(d, ROT_MODE_QUAT, ROT_MODE_QUAT);
	KX_MACRO_addTypesToDict(d, ROT_MODE_XYZ, ROT_MODE_XYZ);
	KX_MACRO_addTypesToDict(d, ROT_MODE_XZY, ROT_MODE_XZY);
	KX_MACRO_addTypesToDict(d, ROT_MODE_YXZ, ROT_MODE_YXZ);
	KX_MACRO_addTypesToDict(d, ROT_MODE_YZX, ROT_MODE_YZX);
	KX_MACRO_addTypesToDict(d, ROT_MODE_ZXY, ROT_MODE_ZXY);
	KX_MACRO_addTypesToDict(d, ROT_MODE_ZYX, ROT_MODE_ZYX);

	// Check for errors
	if (PyErr_Occurred())
    {
		Py_FatalError("can't initialize module GameLogic");
    }

	return m;
}

// Python Sandbox code
// override builtin functions import() and open()


PyObject *KXpy_open(PyObject *self, PyObject *args) {
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: open() function disabled!\nGame Scripts should not use this function.");
	return NULL;
}

PyObject *KXpy_file(PyObject *self, PyObject *args) {
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: file() function disabled!\nGame Scripts should not use this function.");
	return NULL;
}

PyObject *KXpy_execfile(PyObject *self, PyObject *args) {
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: execfile() function disabled!\nGame Scripts should not use this function.");
	return NULL;
}

PyObject *KXpy_compile(PyObject *self, PyObject *args) {
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: compile() function disabled!\nGame Scripts should not use this function.");
	return NULL;
}

PyObject *KXpy_import(PyObject *self, PyObject *args)
{
	char *name;
	int found;
	PyObject *globals = NULL;
	PyObject *locals = NULL;
	PyObject *fromlist = NULL;
	PyObject *l, *m, *n;
	int level; /* not used yet */
	
	if (!PyArg_ParseTuple(args, "s|OOOi:m_import",
	        &name, &globals, &locals, &fromlist, &level))
	    return NULL;

	/* check for builtin modules */
	m = PyImport_AddModule("sys");
	l = PyObject_GetAttrString(m, "builtin_module_names");
	n = PyUnicode_FromString(name);
	
	if (PySequence_Contains(l, n)) {
		return PyImport_ImportModuleEx(name, globals, locals, fromlist);
	}

	/* quick hack for GamePython modules 
		TODO: register builtin modules properly by ExtendInittab */
	if (!strcmp(name, "GameLogic") || !strcmp(name, "GameKeys") || !strcmp(name, "PhysicsConstraints") ||
		!strcmp(name, "Rasterizer") || !strcmp(name, "mathutils") || !strcmp(name, "bgl") || !strcmp(name, "geometry")) {
		return PyImport_ImportModuleEx(name, globals, locals, fromlist);
	}
	
	/* Import blender texts as python modules */
	m= bpy_text_import_name(name, &found);
	if (m)
		return m;
	
	if(found==0) /* if its found but could not import then it has its own error */
		PyErr_Format(PyExc_ImportError, "Import of external Module %.20s not allowed.", name);
	
	return NULL;

}

PyObject *KXpy_reload(PyObject *self, PyObject *args) {
	
	/* Used to be sandboxed, bettet to allow importing of internal text only */ 
#if 0
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: reload() function disabled!\nGame Scripts should not use this function.");
	return NULL;
#endif
	int found;
	PyObject *module = NULL;
	PyObject *newmodule = NULL;

	/* check for a module arg */
	if( !PyArg_ParseTuple( args, "O:bpy_reload_meth", &module ) )
		return NULL;
	
	newmodule= bpy_text_reimport( module, &found );
	if (newmodule)
		return newmodule;
	
	if (found==0) /* if its found but could not import then it has its own error */
		PyErr_SetString(PyExc_ImportError, "reload(module): failed to reload from blenders internal text");
	
	return newmodule;
}

/* override python file type functions */
#if 0
static int
file_init(PyObject *self, PyObject *args, PyObject *kwds)
{
	KXpy_file(NULL, NULL);
	return -1;
}

static PyObject *
file_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	return KXpy_file(NULL, NULL);
}
#endif

static PyMethodDef meth_open[] = {{ "open", KXpy_open, METH_VARARGS, "(disabled)"}};
static PyMethodDef meth_reload[] = {{ "reload", KXpy_reload, METH_VARARGS, "(disabled)"}};
static PyMethodDef meth_file[] = {{ "file", KXpy_file, METH_VARARGS, "(disabled)"}};
static PyMethodDef meth_execfile[] = {{ "execfile", KXpy_execfile, METH_VARARGS, "(disabled)"}};
static PyMethodDef meth_compile[] = {{ "compile", KXpy_compile, METH_VARARGS, "(disabled)"}};

static PyMethodDef meth_import[] = {{ "import", KXpy_import, METH_VARARGS, "our own import"}};

//static PyObject *g_oldopen = 0;
//static PyObject *g_oldimport = 0;
//static int g_security = 0;

void setSandbox(TPythonSecurityLevel level)
{
    PyObject *m = PyImport_AddModule("__builtin__");
    PyObject *d = PyModule_GetDict(m);
	PyObject *item;
	switch (level) {
	case psl_Highest:
		//if (!g_security) {
			//g_oldopen = PyDict_GetItemString(d, "open");
	
			// functions we cant trust
			PyDict_SetItemString(d, "open", item=PyCFunction_New(meth_open, NULL));			Py_DECREF(item);
			PyDict_SetItemString(d, "reload", item=PyCFunction_New(meth_reload, NULL));		Py_DECREF(item);
			PyDict_SetItemString(d, "file", item=PyCFunction_New(meth_file, NULL));			Py_DECREF(item);
			PyDict_SetItemString(d, "execfile", item=PyCFunction_New(meth_execfile, NULL));	Py_DECREF(item);
			PyDict_SetItemString(d, "compile", item=PyCFunction_New(meth_compile, NULL));		Py_DECREF(item);
			
			// our own import
			PyDict_SetItemString(d, "__import__", PyCFunction_New(meth_import, NULL));
			//g_security = level;
			
			// Overiding file dosnt stop it being accessed if your sneaky
			//    f =  [ t for t in (1).__class__.__mro__[-1].__subclasses__() if t.__name__ == 'file'][0]('/some_file.txt', 'w')
			//    f.write('...')
			// so overwrite the file types functions. be very careful here still, since python uses python.
			// ps - python devs frown deeply upon this.
	
			/* this could mess up pythons internals, if we are serious about sandboxing
			 * issues like the one above need to be solved, possibly modify __subclasses__ is safer? */
#if 0
			PyFile_Type.tp_init = file_init;
			PyFile_Type.tp_new = file_new;
#endif
		//}
		break;
	/*
	case psl_Lowest:
		if (g_security) {
			PyDict_SetItemString(d, "open", g_oldopen);
			PyDict_SetItemString(d, "__import__", g_oldimport);
			g_security = level;
		}
	*/
	default:
			/* Allow importing internal text, from bpy_internal_import.py */
			PyDict_SetItemString(d, "reload", item=PyCFunction_New(bpy_reload_meth, NULL));		Py_DECREF(item);
			PyDict_SetItemString(d, "__import__", item=PyCFunction_New(bpy_import_meth, NULL));	Py_DECREF(item);
		break;
	}
}

/* Explanation of 
 * 
 * - backupPySysObjects()		: stores sys.path in gp_OrigPythonSysPath
 * - initPySysObjects(main)	: initializes the blendfile and library paths
 * - restorePySysObjects()		: restores sys.path from gp_OrigPythonSysPath
 * 
 * These exist so the current blend dir "//" can always be used to import modules from.
 * the reason we need a few functions for this is that python is not only used by the game engine
 * so we cant just add to sys.path all the time, it would leave pythons state in a mess.
 * It would also be incorrect since loading blend files for new levels etc would alwasy add to sys.path
 * 
 * To play nice with blenders python, the sys.path is backed up and the current blendfile along
 * with all its lib paths are added to the sys path.
 * When loading a new blendfile, the original sys.path is restored and the new paths are added over the top.
 */

/**
 * So we can have external modules mixed with our blend files.
 */
static void backupPySysObjects(void)
{
	PyObject *sys_path= PySys_GetObject("path"); /* should never fail */
	PyObject *sys_mods= PySys_GetObject("modules"); /* should never fail */
	
	/* paths */
	Py_XDECREF(gp_OrigPythonSysPath); /* just incase its set */
	gp_OrigPythonSysPath = PyList_GetSlice(sys_path, 0, INT_MAX); /* copy the list */
	
	/* modules */
	Py_XDECREF(gp_OrigPythonSysModules); /* just incase its set */
	gp_OrigPythonSysModules = PyDict_Copy(sys_mods); /* copy the list */
	
}

/* for initPySysObjects only,
 * takes a blend path and adds a scripts dir from it
 *
 * "/home/me/foo.blend" -> "/home/me/scripts"
 */
static void initPySysObjects__append(PyObject *sys_path, char *filename)
{
	PyObject *item;
	char expanded[FILE_MAXDIR + FILE_MAXFILE];
	
	BLI_split_dirfile(filename, expanded, NULL); /* get the dir part of filename only */
	BLI_path_abs(expanded, gp_GamePythonPath); /* filename from lib->filename is (always?) absolute, so this may not be needed but it wont hurt */
	BLI_cleanup_file(gp_GamePythonPath, expanded); /* Dont use BLI_cleanup_dir because it adds a slash - BREAKS WIN32 ONLY */
	item= PyUnicode_FromString(expanded);
	
//	printf("SysPath - '%s', '%s', '%s'\n", expanded, filename, gp_GamePythonPath);
	
	if(PySequence_Index(sys_path, item) == -1) {
		PyErr_Clear(); /* PySequence_Index sets a ValueError */
		PyList_Insert(sys_path, 0, item);
	}
	
	Py_DECREF(item);
}
static void initPySysObjects(Main *maggie)
{
	PyObject *sys_path= PySys_GetObject("path"); /* should never fail */
	
	if (gp_OrigPythonSysPath==NULL) {
		/* backup */
		backupPySysObjects();
	}
	else {
		/* get the original sys path when the BGE started */
		PyList_SetSlice(sys_path, 0, INT_MAX, gp_OrigPythonSysPath);
	}
	
	Library *lib= (Library *)maggie->library.first;
	
	while(lib) {
		/* lib->name wont work in some cases (on win32),
		 * even when expanding with gp_GamePythonPath, using lib->filename is less trouble */
		initPySysObjects__append(sys_path, lib->filename);
		lib= (Library *)lib->id.next;
	}
	
	initPySysObjects__append(sys_path, gp_GamePythonPath);
	
//	fprintf(stderr, "\nNew Path: %d ", PyList_Size(sys_path));
//	PyObject_Print(sys_path, stderr, 0);
}

static void restorePySysObjects(void)
{
	if (gp_OrigPythonSysPath==NULL)
		return;
	
	PyObject *sys_path= PySys_GetObject("path"); /* should never fail */
	PyObject *sys_mods= PySys_GetObject("modules"); /* should never fail */

	/* paths */
	PyList_SetSlice(sys_path, 0, INT_MAX, gp_OrigPythonSysPath);
	Py_DECREF(gp_OrigPythonSysPath);
	gp_OrigPythonSysPath= NULL;
	
	/* modules */
	PyDict_Clear(sys_mods);
	PyDict_Update(sys_mods, gp_OrigPythonSysModules);
	Py_DECREF(gp_OrigPythonSysModules);
	gp_OrigPythonSysModules= NULL;	
	
	
//	fprintf(stderr, "\nRestore Path: %d ", PyList_Size(sys_path));
//	PyObject_Print(sys_path, stderr, 0);
}

/**
 * Python is not initialised.
 */
PyObject* initGamePlayerPythonScripting(const STR_String& progname, TPythonSecurityLevel level, Main *maggie, int argc, char** argv)
{
	/* Yet another gotcha in the py api
	 * Cant run PySys_SetArgv more then once because this adds the
	 * binary dir to the sys.path each time.
	 * Id have thaught python being totally restarted would make this ok but
	 * somehow it remembers the sys.path - Campbell
	 */
	static bool first_time = true;
	
#if 0 // TODO - py3
	STR_String pname = progname;
	Py_SetProgramName(pname.Ptr());
#endif
	Py_NoSiteFlag=1;
	Py_FrozenFlag=1;
	Py_Initialize();
	
	if(argv && first_time) { /* browser plugins dont currently set this */
		// Until python support ascii again, we use our own.
		// PySys_SetArgv(argc, argv);
		int i;
		PyObject *py_argv= PyList_New(argc);

		for (i=0; i<argc; i++)
			PyList_SET_ITEM(py_argv, i, PyUnicode_FromString(argv[i]));

		PySys_SetObject("argv", py_argv);
		Py_DECREF(py_argv);
	}
	
	setSandbox(level);
	initPyTypes();
	
	bpy_import_main_set(maggie);
	
	initPySysObjects(maggie);
	
	first_time = false;
	
	PyObjectPlus::ClearDeprecationWarning();
	
	PyObject* moduleobj = PyImport_AddModule("__main__");
	return PyModule_GetDict(moduleobj);
}

void exitGamePlayerPythonScripting()
{	
	/* since python restarts we cant let the python backup of the sys.path hang around in a global pointer */
	restorePySysObjects(); /* get back the original sys.path and clear the backup */
	
	Py_Finalize();
	bpy_import_main_set(NULL);
	PyObjectPlus::ClearDeprecationWarning();
}



/**
 * Python is already initialized.
 */
PyObject* initGamePythonScripting(const STR_String& progname, TPythonSecurityLevel level, Main *maggie)
{
#if 0 // XXX TODO Py3
	STR_String pname = progname;
	Py_SetProgramName(pname.Ptr());
#endif
	Py_NoSiteFlag=1;
	Py_FrozenFlag=1;

	setSandbox(level);
	initPyTypes();
	
	bpy_import_main_set(maggie);
	
	initPySysObjects(maggie);

	PyObjectPlus::NullDeprecationWarning();
	
	PyObject* moduleobj = PyImport_AddModule("__main__");
	return PyModule_GetDict(moduleobj);
}

void exitGamePythonScripting()
{
	restorePySysObjects(); /* get back the original sys.path and clear the backup */
	bpy_import_main_set(NULL);
	PyObjectPlus::ClearDeprecationWarning();
}

/* similar to the above functions except it sets up the namespace
 * and other more general things */
void setupGamePython(KX_KetsjiEngine* ketsjiengine, KX_Scene* startscene, Main *blenderdata, PyObject * pyGlobalDict, PyObject **gameLogic, PyObject **gameLogic_keys, int argc, char** argv)
{
	PyObject* dictionaryobject;

	if(argv) /* player only */
		dictionaryobject= initGamePlayerPythonScripting("Ketsji", psl_Lowest, blenderdata, argc, argv);
	else
		dictionaryobject= initGamePythonScripting("Ketsji", psl_Lowest, blenderdata);

	ketsjiengine->SetPyNamespace(dictionaryobject);
	initRasterizer(ketsjiengine->GetRasterizer(), ketsjiengine->GetCanvas());
	*gameLogic = initGameLogic(ketsjiengine, startscene);

	/* is set in initGameLogic so only set here if we want it to persist between scenes */
	if(pyGlobalDict)
		PyDict_SetItemString(PyModule_GetDict(*gameLogic), "globalDict", pyGlobalDict); // Same as importing the module.

	*gameLogic_keys = PyDict_Keys(PyModule_GetDict(*gameLogic));
	PyDict_SetItemString(dictionaryobject, "GameLogic", *gameLogic); // Same as importing the module.

	initGameKeys();
	initPythonConstraintBinding();
	initMathutils();
	initGeometry();
	initBGL();
	initBLF();

#ifdef WITH_FFMPEG
	initVideoTexture();
#endif

	/* could be done a lot more nicely, but for now a quick way to get bge.* working */
	PyRun_SimpleString("__import__('sys').modules['bge']=[mod for mod in (type(__builtins__)('bge'), ) if mod.__dict__.update({'logic':__import__('GameLogic'), 'render':__import__('Rasterizer'), 'events':__import__('GameKeys'), 'constraints':__import__('PhysicsConstraints'), 'types':__import__('GameTypes')}) is None][0]");
}

static struct PyModuleDef Rasterizer_module_def = {
	{}, /* m_base */
	"Rasterizer",  /* m_name */
	Rasterizer_module_documentation,  /* m_doc */
	0,  /* m_size */
	rasterizer_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject* initRasterizer(RAS_IRasterizer* rasty,RAS_ICanvas* canvas)
{
	gp_Canvas = canvas;
	gp_Rasterizer = rasty;


  PyObject* m;
  PyObject* d;
  PyObject* item;

	/* Use existing module where possible
	 * be careful not to init any runtime vars after this */
	m = PyImport_ImportModule( "Rasterizer" );
	if(m) {
		Py_DECREF(m);
		return m;
	}
	else {
		PyErr_Clear();
	
		// Create the module and add the functions
		m = PyModule_Create(&Rasterizer_module_def);
		PyDict_SetItemString(PySys_GetObject("modules"), Rasterizer_module_def.m_name, m);
	}

  // Add some symbolic constants to the module
  d = PyModule_GetDict(m);
  ErrorObject = PyUnicode_FromString("Rasterizer.error");
  PyDict_SetItemString(d, "error", ErrorObject);
  Py_DECREF(ErrorObject);

  /* needed for get/setMaterialType */
  KX_MACRO_addTypesToDict(d, KX_TEXFACE_MATERIAL, KX_TEXFACE_MATERIAL);
  KX_MACRO_addTypesToDict(d, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL);
  KX_MACRO_addTypesToDict(d, KX_BLENDER_GLSL_MATERIAL, KX_BLENDER_GLSL_MATERIAL);

  // XXXX Add constants here

  // Check for errors
  if (PyErr_Occurred())
    {
      Py_FatalError("can't initialize module Rasterizer");
    }

  return d;
}



/* ------------------------------------------------------------------------- */
/* GameKeys: symbolic constants for key mapping                              */
/* ------------------------------------------------------------------------- */

static char GameKeys_module_documentation[] =
"This modules provides defines for key-codes"
;

static char gPyEventToString_doc[] =
"EventToString(event) - Take a valid event from the GameKeys module or Keyboard Sensor and return a name"
;

static PyObject* gPyEventToString(PyObject*, PyObject* value)
{
	PyObject* mod, *dict, *key, *val, *ret = NULL;
	Py_ssize_t pos = 0;
	
	mod = PyImport_ImportModule( "GameKeys" );
	if (!mod)
		return NULL;
	
	dict = PyModule_GetDict(mod);
	
	while (PyDict_Next(dict, &pos, &key, &val)) {
		if (PyObject_RichCompareBool(value, val, Py_EQ)) {
			ret = key;
			break;
		}
	}
	
	PyErr_Clear(); // incase there was an error clearing
	Py_DECREF(mod);
	if (!ret)	PyErr_SetString(PyExc_ValueError, "GameKeys.EventToString(int): expected a valid int keyboard event");
	else		Py_INCREF(ret);
	
	return ret;
}

static char gPyEventToCharacter_doc[] =
"EventToCharacter(event, is_shift) - Take a valid event from the GameKeys module or Keyboard Sensor and return a character"
;

static PyObject* gPyEventToCharacter(PyObject*, PyObject* args)
{
	int event, shift;
	if (!PyArg_ParseTuple(args,"ii:EventToCharacter", &event, &shift))
		return NULL;
	
	if(IsPrintable(event)) {
		char ch[2] = {'\0', '\0'};
		ch[0] = ToCharacter(event, (bool)shift);
		return PyUnicode_FromString(ch);
	}
	else {
		return PyUnicode_FromString("");
	}
}


static struct PyMethodDef gamekeys_methods[] = {
	{"EventToCharacter", (PyCFunction)gPyEventToCharacter, METH_VARARGS, (const char *)gPyEventToCharacter_doc},
	{"EventToString", (PyCFunction)gPyEventToString, METH_O, (const char *)gPyEventToString_doc},
	{ NULL, (PyCFunction) NULL, 0, NULL }
};

static struct PyModuleDef GameKeys_module_def = {
	{}, /* m_base */
	"GameKeys",  /* m_name */
	GameKeys_module_documentation,  /* m_doc */
	0,  /* m_size */
	gamekeys_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject* initGameKeys()
{
	PyObject* m;
	PyObject* d;
	PyObject* item;
	
	/* Use existing module where possible */
	m = PyImport_ImportModule( "GameKeys" );
	if(m) {
		Py_DECREF(m);
		return m;
	}
	else {
		PyErr_Clear();
	
		// Create the module and add the functions
		m = PyModule_Create(&GameKeys_module_def);
		PyDict_SetItemString(PySys_GetObject("modules"), GameKeys_module_def.m_name, m);
	}

	// Add some symbolic constants to the module
	d = PyModule_GetDict(m);

	// XXXX Add constants here

	KX_MACRO_addTypesToDict(d, AKEY, SCA_IInputDevice::KX_AKEY);
	KX_MACRO_addTypesToDict(d, BKEY, SCA_IInputDevice::KX_BKEY);
	KX_MACRO_addTypesToDict(d, CKEY, SCA_IInputDevice::KX_CKEY);
	KX_MACRO_addTypesToDict(d, DKEY, SCA_IInputDevice::KX_DKEY);
	KX_MACRO_addTypesToDict(d, EKEY, SCA_IInputDevice::KX_EKEY);
	KX_MACRO_addTypesToDict(d, FKEY, SCA_IInputDevice::KX_FKEY);
	KX_MACRO_addTypesToDict(d, GKEY, SCA_IInputDevice::KX_GKEY);
	KX_MACRO_addTypesToDict(d, HKEY, SCA_IInputDevice::KX_HKEY);
	KX_MACRO_addTypesToDict(d, IKEY, SCA_IInputDevice::KX_IKEY);
	KX_MACRO_addTypesToDict(d, JKEY, SCA_IInputDevice::KX_JKEY);
	KX_MACRO_addTypesToDict(d, KKEY, SCA_IInputDevice::KX_KKEY);
	KX_MACRO_addTypesToDict(d, LKEY, SCA_IInputDevice::KX_LKEY);
	KX_MACRO_addTypesToDict(d, MKEY, SCA_IInputDevice::KX_MKEY);
	KX_MACRO_addTypesToDict(d, NKEY, SCA_IInputDevice::KX_NKEY);
	KX_MACRO_addTypesToDict(d, OKEY, SCA_IInputDevice::KX_OKEY);
	KX_MACRO_addTypesToDict(d, PKEY, SCA_IInputDevice::KX_PKEY);
	KX_MACRO_addTypesToDict(d, QKEY, SCA_IInputDevice::KX_QKEY);
	KX_MACRO_addTypesToDict(d, RKEY, SCA_IInputDevice::KX_RKEY);
	KX_MACRO_addTypesToDict(d, SKEY, SCA_IInputDevice::KX_SKEY);
	KX_MACRO_addTypesToDict(d, TKEY, SCA_IInputDevice::KX_TKEY);
	KX_MACRO_addTypesToDict(d, UKEY, SCA_IInputDevice::KX_UKEY);
	KX_MACRO_addTypesToDict(d, VKEY, SCA_IInputDevice::KX_VKEY);
	KX_MACRO_addTypesToDict(d, WKEY, SCA_IInputDevice::KX_WKEY);
	KX_MACRO_addTypesToDict(d, XKEY, SCA_IInputDevice::KX_XKEY);
	KX_MACRO_addTypesToDict(d, YKEY, SCA_IInputDevice::KX_YKEY);
	KX_MACRO_addTypesToDict(d, ZKEY, SCA_IInputDevice::KX_ZKEY);
	
	KX_MACRO_addTypesToDict(d, ZEROKEY, SCA_IInputDevice::KX_ZEROKEY);		
	KX_MACRO_addTypesToDict(d, ONEKEY, SCA_IInputDevice::KX_ONEKEY);		
	KX_MACRO_addTypesToDict(d, TWOKEY, SCA_IInputDevice::KX_TWOKEY);		
	KX_MACRO_addTypesToDict(d, THREEKEY, SCA_IInputDevice::KX_THREEKEY);
	KX_MACRO_addTypesToDict(d, FOURKEY, SCA_IInputDevice::KX_FOURKEY);		
	KX_MACRO_addTypesToDict(d, FIVEKEY, SCA_IInputDevice::KX_FIVEKEY);		
	KX_MACRO_addTypesToDict(d, SIXKEY, SCA_IInputDevice::KX_SIXKEY);		
	KX_MACRO_addTypesToDict(d, SEVENKEY, SCA_IInputDevice::KX_SEVENKEY);
	KX_MACRO_addTypesToDict(d, EIGHTKEY, SCA_IInputDevice::KX_EIGHTKEY);
	KX_MACRO_addTypesToDict(d, NINEKEY, SCA_IInputDevice::KX_NINEKEY);		
		
	KX_MACRO_addTypesToDict(d, CAPSLOCKKEY, SCA_IInputDevice::KX_CAPSLOCKKEY);
		
	KX_MACRO_addTypesToDict(d, LEFTCTRLKEY, SCA_IInputDevice::KX_LEFTCTRLKEY);	
	KX_MACRO_addTypesToDict(d, LEFTALTKEY, SCA_IInputDevice::KX_LEFTALTKEY); 		
	KX_MACRO_addTypesToDict(d, RIGHTALTKEY, SCA_IInputDevice::KX_RIGHTALTKEY); 	
	KX_MACRO_addTypesToDict(d, RIGHTCTRLKEY, SCA_IInputDevice::KX_RIGHTCTRLKEY); 	
	KX_MACRO_addTypesToDict(d, RIGHTSHIFTKEY, SCA_IInputDevice::KX_RIGHTSHIFTKEY);	
	KX_MACRO_addTypesToDict(d, LEFTSHIFTKEY, SCA_IInputDevice::KX_LEFTSHIFTKEY);
		
	KX_MACRO_addTypesToDict(d, ESCKEY, SCA_IInputDevice::KX_ESCKEY);
	KX_MACRO_addTypesToDict(d, TABKEY, SCA_IInputDevice::KX_TABKEY);
	KX_MACRO_addTypesToDict(d, RETKEY, SCA_IInputDevice::KX_RETKEY);
	KX_MACRO_addTypesToDict(d, SPACEKEY, SCA_IInputDevice::KX_SPACEKEY);
	KX_MACRO_addTypesToDict(d, LINEFEEDKEY, SCA_IInputDevice::KX_LINEFEEDKEY);		
	KX_MACRO_addTypesToDict(d, BACKSPACEKEY, SCA_IInputDevice::KX_BACKSPACEKEY);
	KX_MACRO_addTypesToDict(d, DELKEY, SCA_IInputDevice::KX_DELKEY);
	KX_MACRO_addTypesToDict(d, SEMICOLONKEY, SCA_IInputDevice::KX_SEMICOLONKEY);
	KX_MACRO_addTypesToDict(d, PERIODKEY, SCA_IInputDevice::KX_PERIODKEY);		
	KX_MACRO_addTypesToDict(d, COMMAKEY, SCA_IInputDevice::KX_COMMAKEY);		
	KX_MACRO_addTypesToDict(d, QUOTEKEY, SCA_IInputDevice::KX_QUOTEKEY);		
	KX_MACRO_addTypesToDict(d, ACCENTGRAVEKEY, SCA_IInputDevice::KX_ACCENTGRAVEKEY);	
	KX_MACRO_addTypesToDict(d, MINUSKEY, SCA_IInputDevice::KX_MINUSKEY);		
	KX_MACRO_addTypesToDict(d, SLASHKEY, SCA_IInputDevice::KX_SLASHKEY);		
	KX_MACRO_addTypesToDict(d, BACKSLASHKEY, SCA_IInputDevice::KX_BACKSLASHKEY);
	KX_MACRO_addTypesToDict(d, EQUALKEY, SCA_IInputDevice::KX_EQUALKEY);		
	KX_MACRO_addTypesToDict(d, LEFTBRACKETKEY, SCA_IInputDevice::KX_LEFTBRACKETKEY);	
	KX_MACRO_addTypesToDict(d, RIGHTBRACKETKEY, SCA_IInputDevice::KX_RIGHTBRACKETKEY);	
		
	KX_MACRO_addTypesToDict(d, LEFTARROWKEY, SCA_IInputDevice::KX_LEFTARROWKEY);
	KX_MACRO_addTypesToDict(d, DOWNARROWKEY, SCA_IInputDevice::KX_DOWNARROWKEY);
	KX_MACRO_addTypesToDict(d, RIGHTARROWKEY, SCA_IInputDevice::KX_RIGHTARROWKEY);	
	KX_MACRO_addTypesToDict(d, UPARROWKEY, SCA_IInputDevice::KX_UPARROWKEY);		
	
	KX_MACRO_addTypesToDict(d, PAD2	, SCA_IInputDevice::KX_PAD2);
	KX_MACRO_addTypesToDict(d, PAD4	, SCA_IInputDevice::KX_PAD4);
	KX_MACRO_addTypesToDict(d, PAD6	, SCA_IInputDevice::KX_PAD6);
	KX_MACRO_addTypesToDict(d, PAD8	, SCA_IInputDevice::KX_PAD8);
		
	KX_MACRO_addTypesToDict(d, PAD1	, SCA_IInputDevice::KX_PAD1);
	KX_MACRO_addTypesToDict(d, PAD3	, SCA_IInputDevice::KX_PAD3);
	KX_MACRO_addTypesToDict(d, PAD5	, SCA_IInputDevice::KX_PAD5);
	KX_MACRO_addTypesToDict(d, PAD7	, SCA_IInputDevice::KX_PAD7);
	KX_MACRO_addTypesToDict(d, PAD9	, SCA_IInputDevice::KX_PAD9);
		
	KX_MACRO_addTypesToDict(d, PADPERIOD, SCA_IInputDevice::KX_PADPERIOD);
	KX_MACRO_addTypesToDict(d, PADSLASHKEY, SCA_IInputDevice::KX_PADSLASHKEY);
	KX_MACRO_addTypesToDict(d, PADASTERKEY, SCA_IInputDevice::KX_PADASTERKEY);
		
		
	KX_MACRO_addTypesToDict(d, PAD0, SCA_IInputDevice::KX_PAD0);
	KX_MACRO_addTypesToDict(d, PADMINUS, SCA_IInputDevice::KX_PADMINUS);
	KX_MACRO_addTypesToDict(d, PADENTER, SCA_IInputDevice::KX_PADENTER);
	KX_MACRO_addTypesToDict(d, PADPLUSKEY, SCA_IInputDevice::KX_PADPLUSKEY);
		
		
	KX_MACRO_addTypesToDict(d, F1KEY , SCA_IInputDevice::KX_F1KEY);
	KX_MACRO_addTypesToDict(d, F2KEY , SCA_IInputDevice::KX_F2KEY);
	KX_MACRO_addTypesToDict(d, F3KEY , SCA_IInputDevice::KX_F3KEY);
	KX_MACRO_addTypesToDict(d, F4KEY , SCA_IInputDevice::KX_F4KEY);
	KX_MACRO_addTypesToDict(d, F5KEY , SCA_IInputDevice::KX_F5KEY);
	KX_MACRO_addTypesToDict(d, F6KEY , SCA_IInputDevice::KX_F6KEY);
	KX_MACRO_addTypesToDict(d, F7KEY , SCA_IInputDevice::KX_F7KEY);
	KX_MACRO_addTypesToDict(d, F8KEY , SCA_IInputDevice::KX_F8KEY);
	KX_MACRO_addTypesToDict(d, F9KEY , SCA_IInputDevice::KX_F9KEY);
	KX_MACRO_addTypesToDict(d, F10KEY, SCA_IInputDevice::KX_F10KEY);
	KX_MACRO_addTypesToDict(d, F11KEY, SCA_IInputDevice::KX_F11KEY);
	KX_MACRO_addTypesToDict(d, F12KEY, SCA_IInputDevice::KX_F12KEY);
	KX_MACRO_addTypesToDict(d, F13KEY, SCA_IInputDevice::KX_F13KEY);
	KX_MACRO_addTypesToDict(d, F14KEY, SCA_IInputDevice::KX_F14KEY);
	KX_MACRO_addTypesToDict(d, F15KEY, SCA_IInputDevice::KX_F15KEY);
	KX_MACRO_addTypesToDict(d, F16KEY, SCA_IInputDevice::KX_F16KEY);
	KX_MACRO_addTypesToDict(d, F17KEY, SCA_IInputDevice::KX_F17KEY);
	KX_MACRO_addTypesToDict(d, F18KEY, SCA_IInputDevice::KX_F18KEY);
	KX_MACRO_addTypesToDict(d, F19KEY, SCA_IInputDevice::KX_F19KEY);
		
	KX_MACRO_addTypesToDict(d, PAUSEKEY, SCA_IInputDevice::KX_PAUSEKEY);
	KX_MACRO_addTypesToDict(d, INSERTKEY, SCA_IInputDevice::KX_INSERTKEY);
	KX_MACRO_addTypesToDict(d, HOMEKEY , SCA_IInputDevice::KX_HOMEKEY);
	KX_MACRO_addTypesToDict(d, PAGEUPKEY, SCA_IInputDevice::KX_PAGEUPKEY);
	KX_MACRO_addTypesToDict(d, PAGEDOWNKEY, SCA_IInputDevice::KX_PAGEDOWNKEY);
	KX_MACRO_addTypesToDict(d, ENDKEY, SCA_IInputDevice::KX_ENDKEY);

	// MOUSE
	KX_MACRO_addTypesToDict(d, LEFTMOUSE, SCA_IInputDevice::KX_LEFTMOUSE);
	KX_MACRO_addTypesToDict(d, MIDDLEMOUSE, SCA_IInputDevice::KX_MIDDLEMOUSE);
	KX_MACRO_addTypesToDict(d, RIGHTMOUSE, SCA_IInputDevice::KX_RIGHTMOUSE);
	KX_MACRO_addTypesToDict(d, WHEELUPMOUSE, SCA_IInputDevice::KX_WHEELUPMOUSE);
	KX_MACRO_addTypesToDict(d, WHEELDOWNMOUSE, SCA_IInputDevice::KX_WHEELDOWNMOUSE);
	KX_MACRO_addTypesToDict(d, MOUSEX, SCA_IInputDevice::KX_MOUSEX);
	KX_MACRO_addTypesToDict(d, MOUSEY, SCA_IInputDevice::KX_MOUSEY);

	// Check for errors
	if (PyErr_Occurred())
    {
		Py_FatalError("can't initialize module GameKeys");
    }

	return d;
}

PyObject* initMathutils()
{
	return Mathutils_Init();
}

PyObject* initGeometry()
{
	return Geometry_Init();
}

PyObject* initBGL()
{
	return BGL_Init();
}

PyObject* initBLF()
{
	return BLF_Init();
}

// utility function for loading and saving the globalDict
int saveGamePythonConfig( char **marshal_buffer)
{
	int marshal_length = 0;
	PyObject* gameLogic = PyImport_ImportModule("GameLogic");
	if (gameLogic) {
		PyObject* pyGlobalDict = PyDict_GetItemString(PyModule_GetDict(gameLogic), "globalDict"); // Same as importing the module
		if (pyGlobalDict) {
#ifdef Py_MARSHAL_VERSION	
			PyObject* pyGlobalDictMarshal = PyMarshal_WriteObjectToString(	pyGlobalDict, 2); // Py_MARSHAL_VERSION == 2 as of Py2.5
#else
			PyObject* pyGlobalDictMarshal = PyMarshal_WriteObjectToString(	pyGlobalDict ); 
#endif
			if (pyGlobalDictMarshal) {
				// for testing only
				// PyObject_Print(pyGlobalDictMarshal, stderr, 0);
				char *marshal_cstring;
				
				marshal_cstring = PyBytes_AsString(pyGlobalDictMarshal); // py3 uses byte arrays
				marshal_length= PyBytes_Size(pyGlobalDictMarshal);
				*marshal_buffer = new char[marshal_length + 1];
				memcpy(*marshal_buffer, marshal_cstring, marshal_length);
				Py_DECREF(pyGlobalDictMarshal);
			} else {
				printf("Error, GameLogic.globalDict could not be marshal'd\n");
			}
		} else {
			printf("Error, GameLogic.globalDict was removed\n");
		}
		Py_DECREF(gameLogic);
	} else {
		PyErr_Clear();
		printf("Error, GameLogic failed to import GameLogic.globalDict will be lost\n");
	}
	return marshal_length;
}

int loadGamePythonConfig(char *marshal_buffer, int marshal_length)
{
	/* Restore the dict */
	if (marshal_buffer) {
		PyObject* gameLogic = PyImport_ImportModule("GameLogic");

		if (gameLogic) {
			PyObject* pyGlobalDict = PyMarshal_ReadObjectFromString(marshal_buffer, marshal_length);
			if (pyGlobalDict) {
				PyObject* pyGlobalDict_orig = PyDict_GetItemString(PyModule_GetDict(gameLogic), "globalDict"); // Same as importing the module.
				if (pyGlobalDict_orig) {
					PyDict_Clear(pyGlobalDict_orig);
					PyDict_Update(pyGlobalDict_orig, pyGlobalDict);
				} else {
					/* this should not happen, but cant find the original globalDict, just assign it then */
					PyDict_SetItemString(PyModule_GetDict(gameLogic), "globalDict", pyGlobalDict); // Same as importing the module.
				}
				Py_DECREF(gameLogic);
				Py_DECREF(pyGlobalDict);
				return 1;
			} else {
				Py_DECREF(gameLogic);
				PyErr_Clear();
				printf("Error could not marshall string\n");
			}
		} else {
			PyErr_Clear();
			printf("Error, GameLogic failed to import GameLogic.globalDict will be lost\n");
		}	
	}
	return 0;
}

void pathGamePythonConfig( char *path )
{
	int len = strlen(gp_GamePythonPathOrig); // Always use the first loaded blend filename
	
	BLI_strncpy(path, gp_GamePythonPathOrig, sizeof(gp_GamePythonPathOrig));

	/* replace extension */
	if (BLI_testextensie(path, ".blend")) {
		strcpy(path+(len-6), ".bgeconf");
	} else {
		strcpy(path+len, ".bgeconf");
	}
}

void setGamePythonPath(char *path)
{
	BLI_strncpy(gp_GamePythonPath, path, sizeof(gp_GamePythonPath));
	BLI_cleanup_file(NULL, gp_GamePythonPath); /* not absolutely needed but makes resolving path problems less confusing later */
	
	if (gp_GamePythonPathOrig[0] == '\0')
		BLI_strncpy(gp_GamePythonPathOrig, path, sizeof(gp_GamePythonPathOrig));
}

// we need this so while blender is open (not blenderplayer)
// loading new blendfiles will reset this on starting the
// engine but loading blend files within the BGE wont overwrite gp_GamePythonPathOrig
void resetGamePythonPath()
{
	gp_GamePythonPathOrig[0] = '\0';
}

#endif // DISABLE_PYTHON
