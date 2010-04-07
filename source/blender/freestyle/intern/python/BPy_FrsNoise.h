#ifndef FREESTYLE_PYTHON_FRSNOISE_H
#define FREESTYLE_PYTHON_FRSNOISE_H

#include <Python.h>

#include "../geometry/Noise.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject FrsNoise_Type;

#define BPy_FrsNoise_Check(v)	(  PyObject_IsInstance( (PyObject *) v, (PyObject *) &FrsNoise_Type)  )

/*---------------------------Python BPy_FrsNoise structure definition----------*/
typedef struct {
	PyObject_HEAD
	Noise *n;
} BPy_FrsNoise;

/*---------------------------Python BPy_FrsNoise visible prototypes-----------*/

int FrsNoise_Init( PyObject *module );


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* FREESTYLE_PYTHON_FRSNOISE_H */
