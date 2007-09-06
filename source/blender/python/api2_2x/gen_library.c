#include "gen_library.h"
#include "gen_utils.h" /*This must come first*/

/* use for GenericLib_getProperties */
#include "BKE_idprop.h"
#include "IDProp.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_curve.h"

/* GenericLib */
#include "World.h"
#include "Font.h"
#include "Metaball.h"
#include "Curve.h"
#include "Camera.h"
#include "NLA.h"
#include "Lattice.h"
#include "Armature.h"
#include "Lamp.h"
#include "Text.h"
#include "Text3d.h"
#include "Sound.h"
#include "Scene.h"
#include "Mesh.h"
#include "Group.h"
#include "Object.h"
#include "Texture.h"
#include "Ipo.h"
#include "DNA_object_types.h"
#include "DNA_ipo_types.h"


/* Generic get/set attrs */
PyObject *GenericLib_getName( void *self )
{	
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	return PyString_FromString( id->name + 2 );
}

int GenericLib_setName( void *self, PyObject *value )
{
	ID *id = ((BPy_GenericLib *)self)->id;
	char *name = NULL;
	if (!id) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "data has been removed" ) );
	
	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	rename_id( id, name );

	return 0;
}

PyObject *GenericLib_getFakeUser( void *self )
{
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	if (id->flag & LIB_FAKEUSER)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

int GenericLib_setFakeUser( void *self, PyObject *value )
{
	int param;
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "data has been removed" ) );
	
	param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if (param) {
		if (!(id->flag & LIB_FAKEUSER)) {
			id->flag |= LIB_FAKEUSER;
			id_us_plus(id);
		}
	} else {
		if (id->flag & LIB_FAKEUSER) {
			id->flag &= ~LIB_FAKEUSER;
			id->us--;
		}
	}
	return 0;
}

PyObject *GenericLib_getTag( void *self )
{	
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	if (id->flag & LIB_DOIT)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

int GenericLib_setTag( void *self, PyObject *value )
{
	int param;
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "data has been removed" ) );
	
	param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected int argument in range [0,1]" );
	
	if (param)
		id->flag |= LIB_DOIT;
	else
		id->flag &= ~LIB_DOIT;
	return 0;
}


/* read only */
PyObject *GenericLib_getLib( void *self )
{	
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	
	if (id->lib)
		return PyString_FromString(id->lib->name);
	else
		return EXPP_incr_ret( Py_None );
}

PyObject *GenericLib_getUsers( void *self )
{	
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	return PyInt_FromLong(id->us);
}

PyObject *GenericLib_getProperties( void *self )
{	
	ID *id = ((BPy_GenericLib *)self)->id;
	if (!id) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "data has been removed" ) );
	return BPy_Wrap_IDProperty( id, IDP_GetProperties(id, 1), NULL );
}

/* use for any.setName("name")*/
PyObject * GenericLib_setName_with_method( void *self, PyObject *args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)GenericLib_setName );
}


/* 
 * returns the Blender lib type code from a PyObject
 * -1 for no match, only give this function libdata
 * 
 * At the moment this is only used by GenericLib_assignData
 * so not all types are needed.
 */
short GenericLib_getType(PyObject * pydata)
{
	//~ if (BPy_Scene_Check(pydata))	return ID_SCE;
	if (BPy_Object_Check(pydata))	return ID_OB;
	if (BPy_Mesh_Check(pydata))		return ID_ME;
	//~ if (BPy_Curve_Check(pydata))	return ID_CU;
	//~ if (BPy_Metaball_Check(pydata))	return ID_MB;
	//~ if (BPy_Material_Check(pydata))	return ID_MA;
	if (BPy_Texture_Check(pydata))	return ID_TE;
	//~ if (BPy_Image_Check(pydata))	return ID_IM;
		//~ //if (BPy_Lattice_Check(pydata))	return ID_LT;
	//~ if (BPy_Lamp_Check(pydata))		return ID_LA;
	//~ if (BPy_Camera_Check(pydata))	return ID_CA;
	if (BPy_Ipo_Check(pydata))		return ID_IP;
	if (BPy_World_Check(pydata))	return ID_WO;
		//~ //if (BPy_Font_Check(pydata))		return ID_VF;
	//~ if (BPy_Text_Check(pydata))		return ID_TXT;
	//~ if (BPy_Sound_Check(pydata))	return ID_SO;
	if (BPy_Group_Check(pydata))	return ID_GR;
	//~ if (BPy_Armature_Check(pydata))	return ID_AR;
	if (BPy_Action_Check(pydata))	return ID_AC;
	
	return -1;
}


/*
 * This function is used to assign a PyObject representing
 * blender libdata to a pointer.
 * 
 * Python examples of this are...
 * 		ob.DupGroup = dupliGroup
 * 		mesh.texMesh = texme
 * 		ob.ipo = None 
 * 
 * This function deals with type checking, data usercounts,
 * and raising errors.
 * 
 * value - python value
 * data - Blender pointer to assign value to
 * ndata - Use this if there is a value data cannot be.
 * 		for instance, a curve's curve modifier cant point to its self.
 * refcount - non zero values will modify blenders user count.
 * type - ID type.
 * subtype - used only for objects and IPO's to stop the wrong types of obs/ipos
 * 		being assigned.
 * 
 */
int GenericLib_assignData(PyObject *value, void **data, void **ndata, short refcount, short type, short subtype)
{
	ID *id= NULL;
	
	if (*data) {
		id = ((ID*)*data);
		
		if (ndata && *data == *ndata) {
			return EXPP_ReturnIntError( PyExc_TypeError,
				"Cannot set this data to its self" );
		}
	}
	
	if (value == Py_None) {
		*data = NULL;
		if (refcount && id) id->us--;
	} else if (GenericLib_getType(value) == type) {
		
		/* object subtypes */
		if (subtype != 0) {
			if (type == ID_OB) {
				Object *ob= (Object *)(((BPy_GenericLib *)value)->id);
				if (ob->type != subtype)
					return EXPP_ReturnIntError( PyExc_TypeError,
						"Object type not supported" );
			}
			
			if (type == ID_IP) {
				Ipo *ipo = (Ipo *)(((BPy_GenericLib *)value)->id);
				if (ipo->blocktype != subtype)
					return EXPP_ReturnIntError( PyExc_TypeError,
						"Ipo type does is not compatible" );
			}
			
			
		}
		if (refcount && id) id->us--;
		id = ((BPy_GenericLib *)value)->id;
		id->us++;
		*data = id;
	} else {
		return EXPP_ReturnIntError( PyExc_TypeError,
				"Could not assign Python Type - None or Library Object" );
	}
	return 0;
} 


/*
 * returns the ID of the object with given name
 * from a given list.
 */
ID *GetIdFromList( ListBase * list, char *name )
{
	ID *id = list->first;

	while( id ) {
		if( strcmp( name, id->name + 2 ) == 0 )
			break;
		id = id->next;
	}

	return id;
}


PyObject *GetPyObjectFromID( ID * id )
{
	switch ( MAKE_ID2( id->name[0], id->name[1] ) ) {
	case ID_SCE:
		return Scene_CreatePyObject( ( Scene *) id );
	case ID_OB:
		return Object_CreatePyObject( (Object *) id );
	case ID_ME:
		return Mesh_CreatePyObject( (Mesh *)id, NULL );
	case ID_CU:
		switch (curve_type((Curve *)id)) {
		case OB_FONT:
			return Text3d_CreatePyObject( (Text3d *)id );
		default:
			return Curve_CreatePyObject( (Curve *)id );
		}
	case ID_MB:
		return Metaball_CreatePyObject((MetaBall *)id);
	case ID_MA:
		return Material_CreatePyObject((Material *)id);
	case ID_TE:
		return Texture_CreatePyObject((Tex *)id);
	case ID_IM:
		return Image_CreatePyObject((Image *)id);
	case ID_LT:
		return Lattice_CreatePyObject((Lattice *)id);
	case ID_LA:
		return Lamp_CreatePyObject((Lamp *)id);
	case ID_CA:
		return Camera_CreatePyObject((Camera *)id);
	case ID_IP:
		return Ipo_CreatePyObject((Ipo *)id);
	case ID_WO:
		return World_CreatePyObject((World *)id);
	case ID_VF:
		return Font_CreatePyObject((VFont *)id);
	case ID_TXT:
		return Text_CreatePyObject((Text *)id);
	case ID_SO:
		return Sound_CreatePyObject((bSound *)id);
	case ID_GR:
		return Group_CreatePyObject((Group *)id);
	case ID_AR:
		return Armature_CreatePyObject((bArmature *)id);
	case ID_AC:
		return Action_CreatePyObject((bAction *)id);
	}
	Py_RETURN_NONE;
}

long GenericLib_hash(BPy_GenericLib * pydata)
{
	return (long)pydata->id;
}

