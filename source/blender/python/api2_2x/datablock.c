/* 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <stdio.h>

#include <BKE_global.h>
#include <BKE_main.h>
#include <DNA_ID.h>

#include "datablock.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Function prototypes                                                       */
/*****************************************************************************/
void       DataBlock_dealloc (PyObject *self);
PyObject * DataBlock_getattr (PyObject *self, char *name);
int        DataBlock_setattr (PyObject *self, char *name, PyObject *ob);
PyObject * DataBlock_repr    (PyObject *self);

PyTypeObject DataBlock_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,                                /* ob_size      */
	"Block",                          /* tp_name      */
	sizeof (DataBlock),               /* tp_basicsize */
	0,                                /* tp_itemsize  */
	(destructor)   DataBlock_dealloc, /* tp_dealloc   */
	(printfunc)    NULL,              /* tp_print     */
	(getattrfunc)  DataBlock_getattr, /* tp_getattr   */
	(setattrfunc)  DataBlock_setattr, /* tp_setattr   */
	(cmpfunc)      NULL,              /* tp_compare   */
	(reprfunc)     DataBlock_repr     /* tp_repr      */
};

/*****************************************************************************/
/* Description: This function creates a Python datablock descriptor object   */
/*              from the specified data pointer. This pointer must point to  */
/*              a structure with a valid ID header.                          */
/*****************************************************************************/
PyObject * DataBlockFromID (ID * data)
{
	DataBlock   * new_block;
	int           obj_id;
	
	if (!data)
	{
		return ( PythonIncRef (Py_None) );
	}

	/* First get the object type. */
	obj_id = MAKE_ID2(data->name[0], data->name[1]);

	switch (obj_id)
	{
		case ID_OB:
			/* Create a new datablock of type: Object */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Object";
			new_block->type_list= &(G.main->object);
			new_block->properties= NULL; /* Object_Properties; */
			break;
		case ID_ME:
			/* Create a new datablock of type: Mesh */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Mesh";
			new_block->type_list= &(G.main->mesh);
			new_block->properties= NULL; /* Mesh_Properties; */
			break;
		case ID_LA:
			/* Create a new datablock of type: Lamp */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Lamp";
			new_block->type_list= &(G.main->lamp);
			new_block->properties= NULL; /* Lamp_Properties; */
			break;
		case ID_CA:
			/* Create a new datablock of type: Camera */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Camera";
			new_block->type_list= &(G.main->camera);
			new_block->properties= NULL; /* Camera_Properties; */
			break;
		case ID_MA:
			/* Create a new datablock of type: Material */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Material";
			new_block->type_list= &(G.main->mat);
			new_block->properties= NULL; /* Material_Properties; */
			break;
		case ID_WO:
			/* Create a new datablock of type: World */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "World";
			new_block->type_list= &(G.main->world);
			new_block->properties= NULL; /* World_Properties; */
			break;
		case ID_IP:
			/* Create a new datablock of type: Ipo */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Ipo";
			new_block->type_list= &(G.main->ipo);
			new_block->properties= NULL; /* Ipo_Properties; */
			break;
		case ID_IM:
			/* Create a new datablock of type: Image */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Image";
			new_block->type_list= &(G.main->image);
			new_block->properties= NULL; /* Image_Properties; */
			break;
		case ID_TXT:
			/* Create a new datablock of type: Text */
			new_block= PyObject_NEW(DataBlock, &DataBlock_Type);
			new_block->type= "Text";
			new_block->type_list= &(G.main->text);
			new_block->properties= NULL; /* Text_Properties; */
			break;
		default:
			return ( PythonReturnErrorObject (PyExc_SystemError,
						"Unable to create block for data") );
	}
	new_block->data = (void*)data;

	return ( (PyObject *) new_block );
}


/*****************************************************************************/
/* Private functions                                                         */
/*****************************************************************************/

/*****************************************************************************/
/* Description: Deallocates a Python Datablock object.                       */
/*****************************************************************************/
void DataBlock_dealloc (PyObject *self)
{
	PyMem_DEL (self);
}

/*****************************************************************************/
/* Description:                                                              */
/*****************************************************************************/
PyObject * DataBlock_getattr (PyObject *self, char *name)
{
	int          obj_id;
	PyObject   * ret = NULL;
	DataBlock  * block = (DataBlock*) self;

	if (!block)
	{
		return (PythonReturnErrorObject (PyExc_RuntimeError,
					"Block was deleted!"));
	}

	/* Check for common attributes. */
	if (StringEqual (name, "name") )
	{
		return (PyString_FromString ((((ID*)block->data)->name)+2));
	}
	if (StringEqual (name, "block_type") )
	{
		return(PyString_FromString (block->type));
	}
	if (StringEqual (name, "users") )
	{
		return (PyInt_FromLong (((ID*)block->data)->us));
	}

	/* The following datablock types have methods: */
	obj_id = MAKE_ID2 (((ID*)block->data)->name[0],
	                   ((ID*)block->data)->name[1]);
	switch (obj_id)
	{
		case ID_OB:
			ret = Py_FindMethod (Object_methods, self, name);
			break;
		case ID_IP:
			ret = Py_None;
			/* ret = Py_FindMethod (Ipo_methods, self, name); */
			break;
		case ID_CA:
			ret = Py_None;
			/* ret = Py_FindMethod (Camera_methods, self, name); */
			break;
		case ID_MA:
			ret = Py_None;
			/* ret = Py_FindMethod (Material_methods, self, name); */
			break;
		case ID_LA:
			ret = Py_None;
			/* ret = Py_FindMethod (Lamp_methods, self, name); */
			break;
		case ID_TXT:
			ret = Py_None;
			/* ret = Py_FindMethod (Text_methods, self, name); */
			break;
		default:
			break;
	}

	if (!ret)
	{
		/* No method found, clear the error message. */
		PyErr_Clear ();

		/* Try to find the common datablock methods. */
		/* ret = Py_FindMethod (commonDataBlock_methods, self, name); */

		if (!ret)
		{
			/* No method found, clear the error message. */
			PyErr_Clear ();

			/* Try to find attributes from property list */
			/* ret = datablock_getattr (block->properties,
					block->type, name, block->data); */
			ret = Py_None;
		}
	}

	return (ret);
}

/*****************************************************************************/
/* Description:                                                              */
/*****************************************************************************/
int DataBlock_setattr (PyObject *self, char *name, PyObject *ob)
{
	DataBlock * block = (DataBlock*) self;
	
	if (!block)
	{
		PythonReturnErrorObject (PyExc_RuntimeError, "Block was deleted!");
		return (0);
	}

	if (StringEqual (name, "name"))
	{
		if (!PyArg_Parse (ob, "s", &name))
		{
			/* TODO: Do we need to display some sort of error message here? */
			return (-1);
		}

		new_id (block->type_list, (ID*)block->data, name);

		return (0);
	}

	/* return (datablock_setattr (block->properties, block->type, name,
				block->data, ob)); */
	return (0);
}

/*****************************************************************************/
/* Description: This function prints a sensible string when doing a          */
/*              'print abc' from Python. Where abc is one of the Blender     */
/*              objects.                                                     */
/*****************************************************************************/
PyObject * DataBlock_repr (PyObject *self)
{
	DataBlock   * data_block;
	ID          * id;
	static char   s[256];
	
	data_block = (DataBlock *)self;
	if (data_block->data)
	{
		/* The object is still available, print the type and name. */
		id = (ID*)data_block->data;
		sprintf (s, "[%.32s %.32s]", data_block->type, id->name+2);
	}
	else
	{
		/* The object has been deleted, print the type and <deleted>. */
		sprintf (s, "[%.32s <deleted>]", data_block->type);
	}
	
	return Py_BuildValue("s", s);
}

