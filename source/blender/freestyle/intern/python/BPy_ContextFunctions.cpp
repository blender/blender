#include "BPy_ContextFunctions.h"
#include "BPy_Convert.h"

#include "../stroke/ContextFunctions.h"

#ifdef __cplusplus
extern "C" {
#endif
  
///////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------Python API function prototypes for the ContextFunctions module--*/

static PyObject * ContextFunctions_GetTimeStampCF( PyObject* self );
static PyObject * ContextFunctions_GetCanvasWidthCF( PyObject* self );
static PyObject * ContextFunctions_GetCanvasHeightCF( PyObject* self );
static PyObject * ContextFunctions_LoadMapCF( PyObject *self, PyObject *args );
static PyObject * ContextFunctions_ReadMapPixelCF( PyObject *self, PyObject *args );
static PyObject * ContextFunctions_ReadCompleteViewMapPixelCF( PyObject *self, PyObject *args );
static PyObject * ContextFunctions_ReadDirectionalViewMapPixelCF( PyObject *self, PyObject *args );
static PyObject * ContextFunctions_GetSelectedFEdgeCF( PyObject *self );

/*-----------------------ContextFunctions module docstring-------------------------------*/

static char module_docstring[] = "The Blender.Freestyle.ContextFunctions submodule";

/*-----------------------ContextFunctions module functions definitions-------------------*/

static PyMethodDef module_functions[] = {
  {"GetTimeStampCF", (PyCFunction)ContextFunctions_GetTimeStampCF, METH_NOARGS, ""},
  {"GetCanvasWidthCF", (PyCFunction)ContextFunctions_GetCanvasWidthCF, METH_NOARGS, ""},
  {"GetCanvasHeightCF", (PyCFunction)ContextFunctions_GetCanvasHeightCF, METH_NOARGS, ""},
  {"LoadMapCF", (PyCFunction)ContextFunctions_LoadMapCF, METH_VARARGS, ""},
  {"ReadMapPixelCF", (PyCFunction)ContextFunctions_ReadMapPixelCF, METH_VARARGS, ""},
  {"ReadCompleteViewMapPixelCF", (PyCFunction)ContextFunctions_ReadCompleteViewMapPixelCF, METH_VARARGS, ""},
  {"ReadDirectionalViewMapPixelCF", (PyCFunction)ContextFunctions_ReadDirectionalViewMapPixelCF, METH_VARARGS, ""},
  {"GetSelectedFEdgeCF", (PyCFunction)ContextFunctions_GetSelectedFEdgeCF, METH_NOARGS, ""},
  {NULL, NULL, 0, NULL}
};

//------------------- MODULE INITIALIZATION --------------------------------

void ContextFunctions_Init( PyObject *module )
{	
  PyObject *m, *d, *f;

  if( module == NULL )
    return;

  m = Py_InitModule3("Blender.Freestyle.ContextFunctions", module_functions, module_docstring);
  if (m == NULL)
    return;
  Py_INCREF(m);
  PyModule_AddObject(module, "ContextFunctions", m);

  // from ContextFunctions import *
  d = PyModule_GetDict(m);
  for (PyMethodDef *p = module_functions; p->ml_name; p++) {
	f = PyDict_GetItemString(d, p->ml_name);
	Py_INCREF(f);
	PyModule_AddObject(module, p->ml_name, f);
  }
}

//------------------------ MODULE FUNCTIONS ----------------------------------

static PyObject *
ContextFunctions_GetTimeStampCF( PyObject* self )
{
  return PyInt_FromLong( ContextFunctions::GetTimeStampCF() );
}

static PyObject *
ContextFunctions_GetCanvasWidthCF( PyObject* self )
{
  return PyInt_FromLong( ContextFunctions::GetCanvasWidthCF() );
}

static PyObject *
ContextFunctions_GetCanvasHeightCF( PyObject* self )
{
  return PyInt_FromLong( ContextFunctions::GetCanvasHeightCF() );
}

static PyObject *
ContextFunctions_LoadMapCF( PyObject *self, PyObject *args )
{
  char *fileName, *mapName;
  unsigned nbLevels;
  float sigma;

  if( !PyArg_ParseTuple(args, "ssIf", &fileName, &mapName, &nbLevels, &sigma) )
    return NULL;

  ContextFunctions::LoadMapCF(fileName, mapName, nbLevels, sigma);

  Py_RETURN_NONE;
}

static PyObject *
ContextFunctions_ReadMapPixelCF( PyObject *self, PyObject *args )
{
  char *mapName;
  int level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "siII", &mapName, &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadMapPixelCF(mapName, level, x, y);

  return PyFloat_FromDouble( f );
}

static PyObject *
ContextFunctions_ReadCompleteViewMapPixelCF( PyObject *self, PyObject *args )
{
  int level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "iII", &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadCompleteViewMapPixelCF(level, x, y);

  return PyFloat_FromDouble( f );
}

static PyObject *
ContextFunctions_ReadDirectionalViewMapPixelCF( PyObject *self, PyObject *args )
{
  int orientation, level;
  unsigned x, y;

  if( !PyArg_ParseTuple(args, "iiII", &orientation, &level, &x, &y) )
    return NULL;

  float f = ContextFunctions::ReadDirectionalViewMapPixelCF(orientation, level, x, y);

  return PyFloat_FromDouble( f );
}

static PyObject *
ContextFunctions_GetSelectedFEdgeCF( PyObject *self )
{
  FEdge *fe = ContextFunctions::GetSelectedFEdgeCF();
  if( fe )
    return Any_BPy_FEdge_from_FEdge( *fe );

  Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
