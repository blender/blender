#ifndef FREESTYLE_PYTHON_SSHAPE_H
#define FREESTYLE_PYTHON_SSHAPE_H

#include "../view_map/Silhouette.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject SShape_Type;

#define BPy_SShape_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &SShape_Type)  )

/*---------------------------Python BPy_SShape structure definition----------*/
typedef struct {
	PyObject_HEAD
	SShape *ss;
	int borrowed; /* non-zero if *ss is a borrowed object */
} BPy_SShape;

/*---------------------------Python BPy_SShape visible prototypes-----------*/

int SShape_Init( PyObject *module );

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* FREESTYLE_PYTHON_SSHAPE_H */
