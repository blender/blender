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
 * Initialize Python thingies.
 */

#include "GL/glew.h"

#include <stdlib.h>

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "KX_PythonInit.h"
//python physics binding
#include "KX_PyConstraintBinding.h"

#include "KX_KetsjiEngine.h"

#include "SCA_IInputDevice.h"
#include "SCA_PropertySensor.h"
#include "SCA_RandomActuator.h"
#include "KX_ConstraintActuator.h"
#include "KX_IpoActuator.h"
#include "KX_SoundActuator.h"
#include "BL_ActionActuator.h"
#include "RAS_IRasterizer.h"
#include "RAS_ICanvas.h"
#include "MT_Vector3.h"
#include "MT_Point3.h"
#include "ListValue.h"
#include "KX_Scene.h"
#include "SND_DeviceManager.h"

#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"
#include "BL_Shader.h"

#include "KX_PyMath.h"

#include "PHY_IPhysicsEnvironment.h"
// FIXME: Enable for access to blender python modules.  This is disabled because
// python has dependencies on a lot of other modules and is a pain to link.
//#define USE_BLENDER_PYTHON
#ifdef USE_BLENDER_PYTHON
//#include "BPY_extern.h"
#endif 

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BLI_blenlib.h"

static void setSandbox(TPythonSecurityLevel level);


// 'local' copy of canvas ptr, for window height/width python scripts
static RAS_ICanvas* gp_Canvas = NULL;
static KX_Scene*	gp_KetsjiScene = NULL;
static RAS_IRasterizer* gp_Rasterizer = NULL;

void	KX_RasterizerDrawDebugLine(const MT_Vector3& from,const MT_Vector3& to,const MT_Vector3& color)
{
	if (gp_Rasterizer)
		gp_Rasterizer->DrawDebugLine(from,to,color);
}

/* Macro for building the keyboard translation */
//#define KX_MACRO_addToDict(dict, name) PyDict_SetItemString(dict, #name, PyInt_FromLong(SCA_IInputDevice::KX_##name))
#define KX_MACRO_addToDict(dict, name) PyDict_SetItemString(dict, #name, PyInt_FromLong(name))
/* For the defines for types from logic bricks, we do stuff explicitly... */
#define KX_MACRO_addTypesToDict(dict, name, name2) PyDict_SetItemString(dict, #name, PyInt_FromLong(name2))


// temporarily python stuff, will be put in another place later !
#include "KX_Python.h"
#include "SCA_PythonController.h"
// List of methods defined in the module

static PyObject* ErrorObject;
STR_String gPyGetRandomFloat_doc="getRandomFloat returns a random floating point value in the range [0..1)";

static PyObject* gPyGetRandomFloat(PyObject*)
{
	return PyFloat_FromDouble(MT_random());
}

static PyObject* gPySetGravity(PyObject*,
										 PyObject* args, 
										 PyObject*)
{
	MT_Vector3 vec = MT_Vector3(0., 0., 0.);
	if (PyVecArgTo(args, vec))
	{
		if (gp_KetsjiScene)
			gp_KetsjiScene->SetGravity(vec);
		
		Py_Return;
	}
	
	return NULL;
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

static PyObject* gPyExpandPath(PyObject*,
								PyObject* args, 
								PyObject*)
{
	char expanded[FILE_MAXDIR + FILE_MAXFILE];
	char* filename;
	
	if (PyArg_ParseTuple(args,"s",&filename))
	{
		BLI_strncpy(expanded, filename, FILE_MAXDIR + FILE_MAXFILE);
		BLI_convertstringcode(expanded, G.sce);
		return PyString_FromString(expanded);
	}
	return NULL;
}


static bool usedsp = false;

// this gets a pointer to an array filled with floats
static PyObject* gPyGetSpectrum(PyObject*)
{
	SND_IAudioDevice* audiodevice = SND_DeviceManager::Instance();

	PyObject* resultlist = PyList_New(512);

	if (audiodevice)
	{
		if (!usedsp)
		{
			audiodevice->StartUsingDSP();
			usedsp = true;
		}
			
		float* spectrum = audiodevice->GetSpectrum();

		for (int index = 0; index < 512; index++)
		{
			PyList_SetItem(resultlist, index, PyFloat_FromDouble(spectrum[index]));
		}
	}

	return resultlist;
}



static PyObject* gPyStartDSP(PyObject*,
						PyObject* args, 
						PyObject*)
{
	SND_IAudioDevice* audiodevice = SND_DeviceManager::Instance();

	if (audiodevice)
	{
		if (!usedsp)
		{
			audiodevice->StartUsingDSP();
			usedsp = true;
			Py_Return;
		}
	}
	return NULL;
}



static PyObject* gPyStopDSP(PyObject*,
					   PyObject* args, 
					   PyObject*)
{
	SND_IAudioDevice* audiodevice = SND_DeviceManager::Instance();

	if (audiodevice)
	{
		if (usedsp)
		{
			audiodevice->StopUsingDSP();
			usedsp = false;
			Py_Return;
		}
	}
	return NULL;
}

static PyObject* gPySetLogicTicRate(PyObject*,
					PyObject* args,
					PyObject*)
{
	float ticrate;
	if (PyArg_ParseTuple(args, "f", &ticrate))
	{
		KX_KetsjiEngine::SetTicRate(ticrate);
		Py_Return;
	}
	
	return NULL;
}

static PyObject* gPyGetLogicTicRate(PyObject*)
{
	return PyFloat_FromDouble(KX_KetsjiEngine::GetTicRate());
}

static PyObject* gPySetPhysicsTicRate(PyObject*,
					PyObject* args,
					PyObject*)
{
	float ticrate;
	if (PyArg_ParseTuple(args, "f", &ticrate))
	{

		PHY_GetActiveEnvironment()->setFixedTimeStep(true,ticrate);
		Py_Return;
	}
	
	return NULL;
}

static PyObject* gPySetPhysicsDebug(PyObject*,
					PyObject* args,
					PyObject*)
{
	int debugMode;
	if (PyArg_ParseTuple(args, "i", &debugMode))
	{
		PHY_GetActiveEnvironment()->setDebugMode(debugMode);
		Py_Return;
	}
	
	return NULL;
}



static PyObject* gPyGetPhysicsTicRate(PyObject*)
{
	return PyFloat_FromDouble(PHY_GetActiveEnvironment()->getFixedTimeStep());
}

static STR_String gPyGetCurrentScene_doc =  
"getCurrentScene()\n"
"Gets a reference to the current scene.\n";
static PyObject* gPyGetCurrentScene(PyObject* self)
{
	Py_INCREF(gp_KetsjiScene);
	return (PyObject*) gp_KetsjiScene;
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

	Py_INCREF(Py_None);
	return Py_None;
}


static struct PyMethodDef game_methods[] = {
	{"expandPath", (PyCFunction)gPyExpandPath, METH_VARARGS, gPyExpandPath_doc},
	{"getCurrentController",
	(PyCFunction) SCA_PythonController::sPyGetCurrentController,
	METH_NOARGS, SCA_PythonController::sPyGetCurrentController__doc__},
	{"getCurrentScene", (PyCFunction) gPyGetCurrentScene,
	METH_NOARGS, gPyGetCurrentScene_doc.Ptr()},
	{"addActiveActuator",(PyCFunction) SCA_PythonController::sPyAddActiveActuator,
	METH_VARARGS, SCA_PythonController::sPyAddActiveActuator__doc__},
	{"getRandomFloat",(PyCFunction) gPyGetRandomFloat,
	METH_NOARGS,gPyGetRandomFloat_doc.Ptr()},
	{"setGravity",(PyCFunction) gPySetGravity, METH_VARARGS,"set Gravitation"},
	{"getSpectrum",(PyCFunction) gPyGetSpectrum, METH_NOARGS,"get audio spectrum"},
	{"stopDSP",(PyCFunction) gPyStopDSP, METH_VARARGS,"stop using the audio dsp (for performance reasons)"},
	{"getLogicTicRate", (PyCFunction) gPyGetLogicTicRate, METH_NOARGS, "Gets the logic tic rate"},
	{"setLogicTicRate", (PyCFunction) gPySetLogicTicRate, METH_VARARGS, "Sets the logic tic rate"},
	{"getPhysicsTicRate", (PyCFunction) gPyGetPhysicsTicRate, METH_NOARGS, "Gets the physics tic rate"},
	{"setPhysicsTicRate", (PyCFunction) gPySetPhysicsTicRate, METH_VARARGS, "Sets the physics tic rate"},
	{"PrintGLInfo", (PyCFunction)pyPrintExt, METH_NOARGS, "Prints GL Extension Info"},
	{NULL, (PyCFunction) NULL, 0, NULL }
};


static PyObject* gPyGetWindowHeight(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	int height = (gp_Canvas ? gp_Canvas->GetHeight() : 0);

		PyObject* heightval = PyInt_FromLong(height);
		return heightval;
}



static PyObject* gPyGetWindowWidth(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
		

	int width = (gp_Canvas ? gp_Canvas->GetWidth() : 0);
	
		PyObject* widthval = PyInt_FromLong(width);
		return widthval;
}



// temporarility visibility thing, will be moved to rasterizer/renderer later
bool gUseVisibilityTemp = false;

static PyObject* gPyEnableVisibility(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	int visible;
	if (PyArg_ParseTuple(args,"i",&visible))
	{
	    gUseVisibilityTemp = (visible != 0);
	}
	else
	{
		return NULL;
	}
   Py_Return;
}



static PyObject* gPyShowMouse(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	int visible;
	if (PyArg_ParseTuple(args,"i",&visible))
	{
	    if (visible)
		{
			if (gp_Canvas)
				gp_Canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		} else
		{
			if (gp_Canvas)
				gp_Canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
		}
	}
	else {
		return NULL;
	}
	
   Py_Return;
}



static PyObject* gPySetMousePosition(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	int x,y;
	if (PyArg_ParseTuple(args,"ii",&x,&y))
	{
	    if (gp_Canvas)
			gp_Canvas->SetMousePosition(x,y);
	}
	else {
		return NULL;
	}
	
   Py_Return;
}

static PyObject* gPySetEyeSeparation(PyObject*,
						PyObject* args,
						PyObject*)
{
	float sep;
	if (PyArg_ParseTuple(args, "f", &sep))
	{
		if (gp_Rasterizer)
			gp_Rasterizer->SetEyeSeparation(sep);
			
		Py_Return;
	}
	
	return NULL;
}

static PyObject* gPyGetEyeSeparation(PyObject*, PyObject*, PyObject*)
{
	if (gp_Rasterizer)
		return PyFloat_FromDouble(gp_Rasterizer->GetEyeSeparation());
	
	return NULL;
}

static PyObject* gPySetFocalLength(PyObject*,
					PyObject* args,
					PyObject*)
{
	float focus;
	if (PyArg_ParseTuple(args, "f", &focus))
	{
		if (gp_Rasterizer)
			gp_Rasterizer->SetFocalLength(focus);
		Py_Return;
	}
	
	return NULL;
}

static PyObject* gPyGetFocalLength(PyObject*, PyObject*, PyObject*)
{
	if (gp_Rasterizer)
		return PyFloat_FromDouble(gp_Rasterizer->GetFocalLength());
	return NULL;
}

static PyObject* gPySetBackgroundColor(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	
	MT_Vector4 vec = MT_Vector4(0., 0., 0.3, 0.);
	if (PyVecArgTo(args, vec))
	{
		if (gp_Canvas)
		{
			gp_Rasterizer->SetBackColor(vec[0], vec[1], vec[2], vec[3]);
		}
		Py_Return;
	}
	
	return NULL;
}



static PyObject* gPySetMistColor(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	
	MT_Vector3 vec = MT_Vector3(0., 0., 0.);
	if (PyVecArgTo(args, vec))
	{
		if (gp_Rasterizer)
		{
			gp_Rasterizer->SetFogColor(vec[0], vec[1], vec[2]);
		}
		Py_Return;
	}
	
	return NULL;
}



static PyObject* gPySetMistStart(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{

	float miststart;
	if (PyArg_ParseTuple(args,"f",&miststart))
	{
		if (gp_Rasterizer)
		{
			gp_Rasterizer->SetFogStart(miststart);
		}
	}
	else {
		return NULL;
	}
   Py_Return;
}



static PyObject* gPySetMistEnd(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{

	float mistend;
	if (PyArg_ParseTuple(args,"f",&mistend))
	{
		if (gp_Rasterizer)
		{
			gp_Rasterizer->SetFogEnd(mistend);
		}
	}
	else {
		return NULL;
	}
   Py_Return;
}


static PyObject* gPySetAmbientColor(PyObject*, 
										 PyObject* args, 
										 PyObject*)
{
	
	MT_Vector3 vec = MT_Vector3(0., 0., 0.);
	if (PyVecArgTo(args, vec))
	{
		if (gp_Rasterizer)
		{
			gp_Rasterizer->SetAmbientColor(vec[0], vec[1], vec[2]);
		}
		Py_Return;
	}
	
	return NULL;
}




static PyObject* gPyMakeScreenshot(PyObject*,
									PyObject* args,
									PyObject*)
{
	char* filename;
	if (PyArg_ParseTuple(args,"s",&filename))
	{
		if (gp_Canvas)
		{
			gp_Canvas->MakeScreenShot(filename);
		}
	}
	else {
		return NULL;
	}
	Py_Return;
}

static PyObject* gPyEnableMotionBlur(PyObject*,
									PyObject* args,
									PyObject*)
{
	float motionblurvalue;
	if (PyArg_ParseTuple(args,"f",&motionblurvalue))
	{
		if(gp_Rasterizer)
		{
			gp_Rasterizer->EnableMotionBlur(motionblurvalue);
		}
	}
	else {
		return NULL;
	}
	Py_Return;
}

static PyObject* gPyDisableMotionBlur(PyObject*,
									PyObject* args,
									PyObject*)
{
	if(gp_Rasterizer)
	{
		gp_Rasterizer->DisableMotionBlur();
	}
	Py_Return;
}

STR_String	gPyGetWindowHeight__doc__="getWindowHeight doc";
STR_String	gPyGetWindowWidth__doc__="getWindowWidth doc";
STR_String	gPyEnableVisibility__doc__="enableVisibility doc";
STR_String	gPyMakeScreenshot__doc__="make Screenshot doc";
STR_String	gPyShowMouse__doc__="showMouse(bool visible)";
STR_String	gPySetMousePosition__doc__="setMousePosition(int x,int y)";

static struct PyMethodDef rasterizer_methods[] = {
  {"getWindowWidth",(PyCFunction) gPyGetWindowWidth,
   METH_VARARGS, gPyGetWindowWidth__doc__.Ptr()},
   {"getWindowHeight",(PyCFunction) gPyGetWindowHeight,
   METH_VARARGS, gPyGetWindowHeight__doc__.Ptr()},
  {"makeScreenshot",(PyCFunction)gPyMakeScreenshot,
	METH_VARARGS, gPyMakeScreenshot__doc__.Ptr()},
   {"enableVisibility",(PyCFunction) gPyEnableVisibility,
   METH_VARARGS, gPyEnableVisibility__doc__.Ptr()},
	{"showMouse",(PyCFunction) gPyShowMouse,
   METH_VARARGS, gPyShowMouse__doc__.Ptr()},
   {"setMousePosition",(PyCFunction) gPySetMousePosition,
   METH_VARARGS, gPySetMousePosition__doc__.Ptr()},
  {"setBackgroundColor",(PyCFunction)gPySetBackgroundColor,METH_VARARGS,"set Background Color (rgb)"},
	{"setAmbientColor",(PyCFunction)gPySetAmbientColor,METH_VARARGS,"set Ambient Color (rgb)"},
 {"setMistColor",(PyCFunction)gPySetMistColor,METH_VARARGS,"set Mist Color (rgb)"},
  {"setMistStart",(PyCFunction)gPySetMistStart,METH_VARARGS,"set Mist Start(rgb)"},
  {"setMistEnd",(PyCFunction)gPySetMistEnd,METH_VARARGS,"set Mist End(rgb)"},
  {"enableMotionBlur",(PyCFunction)gPyEnableMotionBlur,METH_VARARGS,"enable motion blur"},
  {"disableMotionBlur",(PyCFunction)gPyDisableMotionBlur,METH_VARARGS,"disable motion blur"},

  
  {"setEyeSeparation", (PyCFunction) gPySetEyeSeparation, METH_VARARGS, "set the eye separation for stereo mode"},
  {"getEyeSeparation", (PyCFunction) gPyGetEyeSeparation, METH_VARARGS, "get the eye separation for stereo mode"},
  {"setFocalLength", (PyCFunction) gPySetFocalLength, METH_VARARGS, "set the focal length for stereo mode"},
  {"getFocalLength", (PyCFunction) gPyGetFocalLength, METH_VARARGS, "get the focal length for stereo mode"},
  { NULL, (PyCFunction) NULL, 0, NULL }
};



// Initialization function for the module (*must* be called initGameLogic)

static char GameLogic_module_documentation[] =
"This is the Python API for the game engine of GameLogic"
;

static char Rasterizer_module_documentation[] =
"This is the Python API for the game engine of Rasterizer"
;



PyObject* initGameLogic(KX_Scene* scene) // quick hack to get gravity hook
{
	PyObject* m;
	PyObject* d;

	gp_KetsjiScene = scene;

	gUseVisibilityTemp=false;

	// Create the module and add the functions
	m = Py_InitModule4("GameLogic", game_methods,
					   GameLogic_module_documentation,
					   (PyObject*)NULL,PYTHON_API_VERSION);

	// Add some symbolic constants to the module
	d = PyModule_GetDict(m);

	ErrorObject = PyString_FromString("GameLogic.error");
	PyDict_SetItemString(d, "error", ErrorObject);

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

	/* 4. Ipo actuator, simple part                                            */
	KX_MACRO_addTypesToDict(d, KX_IPOACT_PLAY,     KX_IpoActuator::KX_ACT_IPO_PLAY);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_PINGPONG, KX_IpoActuator::KX_ACT_IPO_PINGPONG);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_FLIPPER,  KX_IpoActuator::KX_ACT_IPO_FLIPPER);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_LOOPSTOP, KX_IpoActuator::KX_ACT_IPO_LOOPSTOP);
	KX_MACRO_addTypesToDict(d, KX_IPOACT_LOOPEND,  KX_IpoActuator::KX_ACT_IPO_LOOPEND);

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
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_PLAY,     BL_ActionActuator::KX_ACT_ACTION_PLAY);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_FLIPPER,     BL_ActionActuator::KX_ACT_ACTION_FLIPPER);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_LOOPSTOP,     BL_ActionActuator::KX_ACT_ACTION_LOOPSTOP);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_LOOPEND,     BL_ActionActuator::KX_ACT_ACTION_LOOPEND);
	KX_MACRO_addTypesToDict(d, KX_ACTIONACT_PROPERTY,     BL_ActionActuator::KX_ACT_ACTION_PROPERTY);
	
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

	// Check for errors
	if (PyErr_Occurred())
    {
		Py_FatalError("can't initialize module GameLogic");
    }

	return d;
}

void dictionaryClearByHand(PyObject *dict)
{
	// Clears the dictionary by hand:
	// This prevents, extra references to global variables
	// inside the GameLogic dictionary when the python interpreter is finalized.
	// which allows the scene to safely delete them :)
	// see: (space.c)->start_game
  	if(dict) PyDict_Clear(dict);
}


// Python Sandbox code
// override builtin functions import() and open()


PyObject *KXpy_open(PyObject *self, PyObject *args)
{
	PyErr_SetString(PyExc_RuntimeError, "Sandbox: open() function disabled!\nGame Scripts should not use this function.");
	return NULL;
}



PyObject *KXpy_import(PyObject *self, PyObject *args)
{
	char *name;
	PyObject *globals = NULL;
	PyObject *locals = NULL;
	PyObject *fromlist = NULL;
	PyObject *l, *m, *n;

	if (!PyArg_ParseTuple(args, "s|OOO:m_import",
	        &name, &globals, &locals, &fromlist))
	    return NULL;

	/* check for builtin modules */
	m = PyImport_AddModule("sys");
	l = PyObject_GetAttrString(m, "builtin_module_names");
	n = PyString_FromString(name);
	
	if (PySequence_Contains(l, n)) {
		return PyImport_ImportModuleEx(name, globals, locals, fromlist);
	}

	/* quick hack for GamePython modules 
		TODO: register builtin modules properly by ExtendInittab */
	if (!strcmp(name, "GameLogic") || !strcmp(name, "GameKeys") || !strcmp(name, "PhysicsConstraints") ||
		!strcmp(name, "Rasterizer")) {
		return PyImport_ImportModuleEx(name, globals, locals, fromlist);
	}
		
	PyErr_Format(PyExc_ImportError,
		 "Import of external Module %.20s not allowed.", name);
	return NULL;

}



static PyMethodDef meth_open[] = {
	{ "open", KXpy_open, METH_VARARGS,
		"(disabled)"}
};


static PyMethodDef meth_import[] = {
	{ "import", KXpy_import, METH_VARARGS,
		"our own import"}
};



//static PyObject *g_oldopen = 0;
//static PyObject *g_oldimport = 0;
//static int g_security = 0;


void setSandbox(TPythonSecurityLevel level)
{
    PyObject *m = PyImport_AddModule("__builtin__");
    PyObject *d = PyModule_GetDict(m);
	PyObject *meth = PyCFunction_New(meth_open, NULL);

	switch (level) {
	case psl_Highest:
		//if (!g_security) {
			//g_oldopen = PyDict_GetItemString(d, "open");
			PyDict_SetItemString(d, "open", meth);
			meth = PyCFunction_New(meth_import, NULL);
			PyDict_SetItemString(d, "__import__", meth);
			//g_security = level;
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
		break;
	}
}

/**
 * Python is not initialised.
 */
PyObject* initGamePlayerPythonScripting(const STR_String& progname, TPythonSecurityLevel level)
{
	STR_String pname = progname;
	Py_SetProgramName(pname.Ptr());
	Py_NoSiteFlag=1;
	Py_FrozenFlag=1;
	Py_Initialize();

	//importBlenderModules()
	
	setSandbox(level);

	PyObject* moduleobj = PyImport_AddModule("__main__");
	return PyModule_GetDict(moduleobj);
}

void exitGamePlayerPythonScripting()
{
	Py_Finalize();
}

/**
 * Python is already initialized.
 */
PyObject* initGamePythonScripting(const STR_String& progname, TPythonSecurityLevel level)
{
	STR_String pname = progname;
	Py_SetProgramName(pname.Ptr());
	Py_NoSiteFlag=1;
	Py_FrozenFlag=1;

	setSandbox(level);

	PyObject* moduleobj = PyImport_AddModule("__main__");
	return PyModule_GetDict(moduleobj);
}



void exitGamePythonScripting()
{
}



PyObject* initRasterizer(RAS_IRasterizer* rasty,RAS_ICanvas* canvas)
{
	gp_Canvas = canvas;
	gp_Rasterizer = rasty;


  PyObject* m;
  PyObject* d;

  // Create the module and add the functions
  m = Py_InitModule4("Rasterizer", rasterizer_methods,
		     Rasterizer_module_documentation,
		     (PyObject*)NULL,PYTHON_API_VERSION);

  // Add some symbolic constants to the module
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("Rasterizer.error");
  PyDict_SetItemString(d, "error", ErrorObject);

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



static struct PyMethodDef gamekeys_methods[] = {
	{ NULL, (PyCFunction) NULL, 0, NULL }
};



PyObject* initGameKeys()
{
	PyObject* m;
	PyObject* d;

	// Create the module and add the functions
	m = Py_InitModule4("GameKeys", gamekeys_methods,
					   GameKeys_module_documentation,
					   (PyObject*)NULL,PYTHON_API_VERSION);

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
		
	KX_MACRO_addTypesToDict(d, PAUSEKEY, SCA_IInputDevice::KX_PAUSEKEY);
	KX_MACRO_addTypesToDict(d, INSERTKEY, SCA_IInputDevice::KX_INSERTKEY);
	KX_MACRO_addTypesToDict(d, HOMEKEY , SCA_IInputDevice::KX_HOMEKEY);
	KX_MACRO_addTypesToDict(d, PAGEUPKEY, SCA_IInputDevice::KX_PAGEUPKEY);
	KX_MACRO_addTypesToDict(d, PAGEDOWNKEY, SCA_IInputDevice::KX_PAGEDOWNKEY);
	KX_MACRO_addTypesToDict(d, ENDKEY, SCA_IInputDevice::KX_ENDKEY);


	// Check for errors
	if (PyErr_Occurred())
    {
		Py_FatalError("can't initialize module GameKeys");
    }

	return d;
}

void PHY_SetActiveScene(class KX_Scene* scene)
{
	gp_KetsjiScene = scene;
}

class KX_Scene* PHY_GetActiveScene()
{
	return gp_KetsjiScene;
}
