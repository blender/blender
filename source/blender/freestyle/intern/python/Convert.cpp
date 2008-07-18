#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////////////////////////////////////////


PyObject *PyBool_from_bool( bool b ){
	return PyBool_FromLong( b ? 1 : 0);
}


PyObject *Vector_from_Vec2f( Vec2f& vec ) {
	float vec_data[2]; // because vec->_coord is protected

	vec_data[0] = vec.x();		vec_data[1] = vec.y();
	return newVectorObject( vec_data, 2, Py_NEW);
}

PyObject *Vector_from_Vec3f( Vec3f& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject *Vector_from_Vec3r( Vec3r& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject *BPy_Id_from_Id( Id& id ) {
	PyObject *py_id = Id_Type.tp_new( &Id_Type, 0, 0 );
	((BPy_Id *) py_id)->id = new Id( id.getFirst(), id.getSecond() );

	return py_id;
}

PyObject *BPy_Interface0D_from_Interface0D( Interface0D& if0D ) {
	PyObject *py_if0D =  Interface0D_Type.tp_new( &Interface0D_Type, 0, 0 );
	((BPy_Interface0D *) py_if0D)->if0D = &if0D;

	return py_if0D;
}

PyObject *BPy_SVertex_from_SVertex( SVertex& sv ) {
	PyObject *py_sv = SVertex_Type.tp_new( &SVertex_Type, 0, 0 );
	((BPy_SVertex *) py_sv)->sv = new SVertex( sv );
	((BPy_SVertex *) py_sv)->py_if0D.if0D = ((BPy_SVertex *) py_sv)->sv;

	return py_sv;
}

PyObject *BPy_FEdge_from_FEdge( FEdge& fe ) {
	PyObject *py_fe = FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	((BPy_FEdge *) py_fe)->fe = new FEdge( fe );
	((BPy_FEdge *) py_fe)->py_if1D.if1D = ((BPy_FEdge *) py_fe)->fe;

	return py_fe;
}

	


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif