#include "Director.h"

#include "BPy_Convert.h"

bool director_BPy_UnaryPredicate1D___call__( PyObject *py_up1D, Interface1D& if1D) {
	cout << "Polymorphism works" << endl;

	PyObject *method = PyObject_GetAttrString( py_up1D, "__call__");
	PyObject *result = PyObject_CallFunction(method, "O", BPy_Interface1D_from_Interface1D(if1D) );

	return bool_from_PyBool(result);
}
