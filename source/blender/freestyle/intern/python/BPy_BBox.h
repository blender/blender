#ifndef FREESTYLE_PYTHON_BBOX_H
#define FREESTYLE_PYTHON_BBOX_H

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject BBox_Type;

#define BPy_BBox_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &BBox_Type)  )

/*---------------------------Python BPy_BBox structure definition----------*/
typedef struct {
	PyObject_HEAD
	BBox<Vec3r> *bb;
} BPy_BBox;

/*---------------------------Python BPy_BBox visible prototypes-----------*/

PyMODINIT_FUNC BBox_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_BBOX_H */
