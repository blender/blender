#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////////////////////////////////////////

static char M_Convert_doc[] = "The Blender.Freestyle.Convert utility submodule";
/*----------------------Freestyle module method def----------------------------*/
struct PyMethodDef M_Convert_methods[] = {
//	{"testOutput", ( PyCFunction ) Freestyle_testOutput, METH_NOARGS, "() - Return Curve Data name"},
	{NULL, NULL, 0, NULL}
};


//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Convert_Init( void )
{
	return Py_InitModule3( "Blender.Freestyle.Convert", M_Convert_methods, M_Convert_doc );
}

//-------------------------------------------------------------------------

PyObject *PyBool_from_bool( bool b ){
	// SWIG_From_bool
	return PyBool_FromLong( b ? 1 : 0);
}


PyObject *Vector_from_Vec2f( Vec2f vec ) {
	float vec_data[2]; // because vec->_coord is protected
	vec_data[0] = vec.x();		vec_data[1] = vec.y();
	return newVectorObject( vec_data, 2, Py_NEW);
}

PyObject *Vector_from_Vec3f( Vec3f vec ) {
	float vec_data[3]; // because vec->_coord is protected
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject *Vector_from_Vec3r( Vec3r vec ) {
	float vec_data[3]; // because vec->_coord is protected
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject *BPy_Id_from_Id( Id id ) {
	BPy_Id *py_id;
	
	py_id = (BPy_Id *) Id_Type.tp_new( &Id_Type, 0, 0 );
	py_id->id->setFirst( id.getFirst() );
	py_id->id->setSecond( id.getSecond() );
	
	return (PyObject *)py_id;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif