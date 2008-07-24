#include "BPy_Convert.h"

#include "BPy_BBox.h"
#include "BPy_Id.h"
#include "BPy_IntegrationType.h"
#include "BPy_Interface0D.h"
#include "Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_SShape.h"
#include "BPy_Nature.h"
#include "BPy_MediumType.h"
#include "BPy_StrokeAttribute.h"

#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////////////////////////////////////////


PyObject * PyBool_from_bool( bool b ){
	return PyBool_FromLong( b ? 1 : 0);
}

bool bool_from_PyBool( PyObject *b ) {
	return b == Py_True;
}

PyObject * Vector_from_Vec2f( Vec2f& vec ) {
	float vec_data[2]; // because vec->_coord is protected

	vec_data[0] = vec.x();		vec_data[1] = vec.y();
	return newVectorObject( vec_data, 2, Py_NEW);
}

PyObject * Vector_from_Vec3f( Vec3f& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject * Vector_from_Vec3r( Vec3r& vec ) {
	float vec_data[3]; // because vec->_coord is protected
	
	vec_data[0] = vec.x();		vec_data[1] = vec.y(); 		vec_data[2] = vec.z(); 
	return newVectorObject( vec_data, 3, Py_NEW);
}

PyObject * BPy_Id_from_Id( Id& id ) {
	PyObject *py_id = Id_Type.tp_new( &Id_Type, 0, 0 );
	((BPy_Id *) py_id)->id = new Id( id.getFirst(), id.getSecond() );

	return py_id;
}

PyObject * BPy_Interface0D_from_Interface0D( Interface0D& if0D ) {
	PyObject *py_if0D =  Interface0D_Type.tp_new( &Interface0D_Type, 0, 0 );
	((BPy_Interface0D *) py_if0D)->if0D = &if0D;

	return py_if0D;
}

PyObject * BPy_SVertex_from_SVertex( SVertex& sv ) {
	PyObject *py_sv = SVertex_Type.tp_new( &SVertex_Type, 0, 0 );
	((BPy_SVertex *) py_sv)->sv = new SVertex( sv );
	((BPy_SVertex *) py_sv)->py_if0D.if0D = ((BPy_SVertex *) py_sv)->sv;

	return py_sv;
}

PyObject * BPy_FEdge_from_FEdge( FEdge& fe ) {
	PyObject *py_fe = FEdge_Type.tp_new( &FEdge_Type, 0, 0 );
	((BPy_FEdge *) py_fe)->fe = new FEdge( fe );
	((BPy_FEdge *) py_fe)->py_if1D.if1D = ((BPy_FEdge *) py_fe)->fe;

	return py_fe;
}

PyObject * BPy_Nature_from_Nature( unsigned short n ) {
	PyObject *py_n =  Nature_Type.tp_new( &Nature_Type, 0, 0 );

	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem( args, 0, PyInt_FromLong(n) );
	Nature_Type.tp_init( py_n, args, 0 );
	Py_DECREF(args);

	return py_n;
}

PyObject * BPy_StrokeAttribute_from_StrokeAttribute( StrokeAttribute& sa ) {
	PyObject *py_sa = StrokeAttribute_Type.tp_new( &StrokeAttribute_Type, 0, 0 );
	((BPy_StrokeAttribute *) py_sa)->sa = new StrokeAttribute( sa );

	return py_sa;	
}

PyObject * BPy_MediumType_from_MediumType( int n ) {
	PyObject *py_mt =  MediumType_Type.tp_new( &MediumType_Type, 0, 0 );

	PyObject *args = PyTuple_New(1);
	PyTuple_SetItem( args, 0, PyInt_FromLong(n) );
	MediumType_Type.tp_init( py_mt, args, 0 );
	Py_DECREF(args);

	return py_mt;
}

PyObject * BPy_StrokeVertex_from_StrokeVertex( StrokeVertex& sv ) {
	PyObject *py_sv = StrokeVertex_Type.tp_new( &StrokeVertex_Type, 0, 0 );
	((BPy_StrokeVertex *) py_sv)->sv = new StrokeVertex( sv );
	((BPy_StrokeVertex *) py_sv)->py_cp.cp = ((BPy_StrokeVertex *) py_sv)->sv;
	((BPy_StrokeVertex *) py_sv)->py_cp.py_if0D.if0D = ((BPy_StrokeVertex *) py_sv)->sv;

	return py_sv;
}

PyObject * BPy_ViewVertex_from_ViewVertex_ptr( ViewVertex *vv ) {
	PyObject *py_vv = ViewVertex_Type.tp_new( &ViewVertex_Type, 0, 0 );
	((BPy_ViewVertex *) py_vv)->vv = vv;
	((BPy_ViewVertex *) py_vv)->py_if0D.if0D = ((BPy_ViewVertex *) py_vv)->vv;

	return py_vv;
}

PyObject * BPy_BBox_from_BBox( BBox< Vec3r > &bb ) {
	PyObject *py_bb = BBox_Type.tp_new( &BBox_Type, 0, 0 );
	((BPy_BBox *) py_bb)->bb = new BBox< Vec3r >( bb );

	return py_bb;
}

PyObject * BPy_ViewEdge_from_ViewEdge( ViewEdge& ve ) {
	PyObject *py_ve = ViewEdge_Type.tp_new( &ViewEdge_Type, 0, 0 );
	((BPy_ViewEdge *) py_ve)->ve = new ViewEdge( ve );
	((BPy_ViewEdge *) py_ve)->py_if1D.if1D = ((BPy_ViewEdge *) py_ve)->ve;

	return py_ve;
}

PyObject * BPy_SShape_from_SShape( SShape& ss ) {
	PyObject *py_ss = SShape_Type.tp_new( &SShape_Type, 0, 0 );
	((BPy_SShape *) py_ss)->ss = new SShape( ss );

	return py_ss;	
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif