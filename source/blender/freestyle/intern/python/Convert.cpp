#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////////////////////////////////////////


PyObject *PyBool_from_bool( bool b ){
	// SWIG_From_bool
	return PyBool_FromLong( b ? 1 : 0);
}


PyObject *Vector_from_Vec2f( Vec2f vec ) {
	float vec_data[2]; // because vec->_coord is protected

	if( &vec != 0 ){
		vec_data[0] = vec.x();		vec_data[1] = vec.y();
		return newVectorObject( vec_data, 2, Py_NEW);
	} 

	Py_RETURN_NONE;
}

PyObject *Vector_from_Vec3f( Vec3f vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	if( &vec != 0 ){
		vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
		return newVectorObject( vec_data, 3, Py_NEW);
	} 

	Py_RETURN_NONE;
}

PyObject *Vector_from_Vec3r( Vec3r vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	if( &vec != 0 ){
		vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
		return newVectorObject( vec_data, 3, Py_NEW);
	} 

	Py_RETURN_NONE;
}

PyObject *BPy_Id_from_Id( Id id ) {
	BPy_Id *py_id;
	
	if( &id != 0 ) {
		py_id = (BPy_Id *) Id_Type.tp_new( &Id_Type, 0, 0 );
		py_id->id = new Id( id.getFirst(), id.getSecond() );

		return (PyObject *)py_id;
	}
	
	Py_RETURN_NONE;
}

PyObject *BPy_SVertex_from_SVertex( SVertex sv ) {
	BPy_SVertex *py_sv;

	if( &sv != 0 ) {
		py_sv = (BPy_SVertex *) SVertex_Type.tp_new( &SVertex_Type, 0, 0 );
		py_sv->sv = new SVertex( sv );
		py_sv->py_if0D.if0D = py_sv->sv;

		return (PyObject *)py_sv;
	}

	Py_RETURN_NONE;
}
	
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif