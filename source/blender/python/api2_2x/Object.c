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

#include "Object.h"

/*****************************************************************************/
/* Function:              M_Object_New                                       */
/* Python equivalent:     Blender.Object.New                                 */
/*****************************************************************************/
PyObject *M_Object_New(PyObject *self, PyObject *args)
{
    struct Object   * object;
    C_BlenObject    * blen_object;
    int               type;
    char              name[32];

    printf ("In Object_New()\n");

    if (!PyArg_ParseTuple(args, "i", &type))
    {
        PythonReturnErrorObject (PyExc_TypeError,
                    "type expected");
        return (NULL);
    }

    /* Create a new object. */
    switch (type)
    {
        case OB_MESH:       strcpy (name, "Mesh");      break;
        case OB_CURVE:      strcpy (name, "Curve");     break;
        case OB_SURF:       strcpy (name, "Surf");      break;
        case OB_FONT:       strcpy (name, "Text");      break;
        case OB_MBALL:      strcpy (name, "Mball");     break;
        case OB_CAMERA:     strcpy (name, "Camera");    break;
        case OB_LAMP:       strcpy (name, "Lamp");      break;
        case OB_IKA:        strcpy (name, "Ika");       break;
        case OB_LATTICE:    strcpy (name, "Lattice");   break;
        case OB_WAVE:       strcpy (name, "Wave");      break;
        case OB_ARMATURE:   strcpy (name, "Armature");  break;
        default:            strcpy (name, "Empty");
    }

    object = alloc_libblock (&(G.main->object), ID_OB, name);

    object->flag = 0;
    object->type = type;

    /* transforms */
    QuatOne(object->quat);
    QuatOne(object->dquat);
    
    object->col[3]= 1.0;    // alpha 

    object->size[0] = object->size[1] = object->size[2] = 1.0;
    object->loc[0] = object->loc[1] = object->loc[2] = 0.0;
    Mat4One(object->parentinv);
    Mat4One(object->obmat);
    object->dt = OB_SHADED; // drawtype

    if (U.flag & MAT_ON_OB)
    {
        object->colbits = -1;
    }
    switch (object->type)
    {
        case OB_CAMERA: /* fall through. */
        case OB_LAMP:
            object->trackflag = OB_NEGZ;
            object->upflag = OB_POSY;
            break;
        default:
            object->trackflag = OB_POSY;
            object->upflag = OB_POSZ;
    }
    object->ipoflag = OB_OFFS_OB + OB_OFFS_PARENT;

    /* duplivert settings */
    object->dupon = 1;
    object->dupoff = 0;
    object->dupsta = 1;
    object->dupend = 100;

    /* Gameengine defaults*/
    object->mass = 1.0;
    object->inertia = 1.0;
    object->formfactor = 0.4;
    object->damping = 0.04;
    object->rdamping = 0.1;
    object->anisotropicFriction[0] = 1.0;
    object->anisotropicFriction[1] = 1.0;
    object->anisotropicFriction[2] = 1.0;
    object->gameflag = OB_PROP;
    
    object->lay = 1; // Layer, by default visible

    switch(type)
    {
        case OB_MESH:
            object->data = add_mesh();
            G.totmesh++;
            break;
        case OB_CAMERA:
            object->data = add_camera();
            break;
        case OB_LAMP:
            object->data = add_lamp();
            G.totlamp++;
            break;

    /* TODO the following types will be supported later
        case OB_CURVE:
            object->data = add_curve(OB_CURVE);
            G.totcurve++;
            break;
        case OB_SURF:
            object->data = add_curve(OB_SURF);
            G.totcurve++;
            break;
        case OB_FONT:
            object->data = add_curve(OB_FONT);
            break;
        case OB_MBALL:
            object->data = add_mball();
            break;
        case OB_IKA:
            object->data = add_ika();
            object->dt = OB_WIRE;
            break;
        case OB_LATTICE:
            object->data = (void *)add_lattice();
            object->dt = OB_WIRE;
            break;
        case OB_WAVE:
            object->data = add_wave();
            break;
        case OB_ARMATURE:
            object->data = add_armature();
            break;
    */
    }

    G.totobj++;

    /* Create a Python object from it. */
    blen_object = (C_BlenObject*)PyObject_NEW (C_BlenObject, &object_type); 
    blen_object->object = object;

    return ((PyObject*)blen_object);
}

/*****************************************************************************/
/* Function:              M_Object_Get                                       */
/* Python equivalent:     Blender.Object.Get                                 */
/*****************************************************************************/
PyObject *M_Object_Get(PyObject *self, PyObject *args)
{
    struct Object   * object;
    char            * name;

    printf ("In Object_Get()\n");

    PyArg_ParseTuple(args, "|s", &name);

    if (name != NULL)
    {
        C_BlenObject    * blen_object;

        object = GetObjectByName (name);

        if (object == NULL)
        {
            /* No object exists with the name specified in the argument name. */
            return (PythonReturnErrorObject (PyExc_AttributeError,
                        "Unknown object specified."));
        }
        blen_object = (C_BlenObject*)PyObject_NEW (C_BlenObject, &object_type); 
        blen_object->object = object;

        return ((PyObject*)blen_object);
    }
    else
    {
        /* No argument has been given. Return a list of all objects by name. */
        PyObject    * obj_list;
        ID          * id_iter;
        int           index = 0;

        obj_list = PyList_New (BLI_countlist (&(G.main->object)));

        if (obj_list == NULL)
        {
            return (PythonReturnErrorObject (PyExc_SystemError,
                        "List creation failed."));
        }

        object = G.main->object.first;
        id_iter = &(object->id);
        while (id_iter)
        {
            PyObject    * object;

            object = PyString_FromString (GetIdName (id_iter));
            if (object == NULL)
            {
                return (PythonReturnErrorObject (PyExc_SystemError,
                        "Python string creation failed."));
            }
            PyList_SetItem (obj_list, index, object);
            id_iter = id_iter->next;
            index++;
        }
        return (obj_list);
    }
}

/*****************************************************************************/
/* Function:              M_Object_GetSelected                               */
/* Python equivalent:     Blender.Object.getSelected                         */
/*****************************************************************************/
PyObject *M_Object_GetSelected (PyObject *self, PyObject *args)
{
    C_BlenObject    * blen_object;
    PyObject        * list;
    Base            * base_iter;

    printf ("In Object_GetSelected()\n");

    list = PyList_New (0);
    if ((G.scene->basact) &&
        ((G.scene->basact->flag & SELECT) &&
         (G.scene->basact->lay & G.vd->lay)))
    {
        /* Active object is first in the list. */
        blen_object = (C_BlenObject*)PyObject_NEW (C_BlenObject, &object_type); 
        if (blen_object == NULL)
        {
            Py_DECREF (list);
            Py_INCREF (Py_None);
            return (Py_None);
        }
        blen_object->object = G.scene->basact->object;
        PyList_Append (list, (PyObject*)blen_object);
    }

    base_iter = G.scene->base.first;
    while (base_iter)
    {
        if (((G.scene->basact->flag & SELECT) &&
             (G.scene->basact->lay & G.vd->lay)) &&
            (base_iter != G.scene->basact))
        {
            blen_object = (C_BlenObject*)PyObject_NEW (C_BlenObject,
                                                       &object_type); 
            if (blen_object == NULL)
            {
                Py_DECREF (list);
                Py_INCREF (Py_None);
                return (Py_None);
            }
            blen_object->object = base_iter->object;
            PyList_Append (list, (PyObject*)blen_object);
        }
        base_iter = base_iter->next;
    }
    return (list);
}

/*****************************************************************************/
/* Function:              initObject                                         */
/*****************************************************************************/
PyObject *initObject (void)
{
    PyObject    * module;

    printf ("In initObject()\n");

    module = Py_InitModule3("Object", M_Object_methods, M_Object_doc);

    return (module);
}

/*****************************************************************************/
/* Function:    ObjectCreatePyObject                                         */
/* Description: This function will create a new BlenObject from an existing  */
/*              Object structure.                                            */
/*****************************************************************************/
PyObject* ObjectCreatePyObject (struct Object *obj)
{
    C_BlenObject    * blen_object;

    printf ("In ObjectCreatePyObject\n");

    blen_object = (C_BlenObject*)PyObject_NEW (C_BlenObject, &object_type);

    if (blen_object == NULL)
    {
        return (NULL);
    }
    blen_object->object = obj;
    return ((PyObject*)blen_object);
}

/*****************************************************************************/
/* Function:    ObjectDeAlloc                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
void ObjectDeAlloc (C_BlenObject *obj)
{
    PyObject_DEL (obj);
}

/*****************************************************************************/
/* Function:    ObjectGetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Blender and       */
/*              passes it to Python.                                         */
/*****************************************************************************/
PyObject* ObjectGetAttr (C_BlenObject *obj, char *name)
{
    struct Object   * object;
    struct Ika      * ika;

    object = obj->object;
    if (StringEqual (name, "LocX"))
        return (PyFloat_FromDouble(object->loc[0]));
    if (StringEqual (name, "LocY"))
        return (PyFloat_FromDouble(object->loc[1]));
    if (StringEqual (name, "LocZ"))
        return (PyFloat_FromDouble(object->loc[2]));
    if (StringEqual (name, "loc"))
        return (Py_BuildValue ("fff", object->loc[0], object->loc[1],
                               object->loc[2]));
    if (StringEqual (name, "dLocX"))
        return (PyFloat_FromDouble(object->dloc[0]));
    if (StringEqual (name, "dLocY"))
        return (PyFloat_FromDouble(object->dloc[1]));
    if (StringEqual (name, "dLocZ"))
        return (PyFloat_FromDouble(object->dloc[2]));
    if (StringEqual (name, "dloc"))
        return (Py_BuildValue ("fff", object->dloc[0], object->dloc[1],
                               object->dloc[2]));
    if (StringEqual (name, "RotX"))
        return (PyFloat_FromDouble(object->rot[0]));
    if (StringEqual (name, "RotY"))
        return (PyFloat_FromDouble(object->rot[1]));
    if (StringEqual (name, "RotZ"))
        return (PyFloat_FromDouble(object->rot[2]));
    if (StringEqual (name, "rot"))
        return (Py_BuildValue ("fff", object->rot[0], object->rot[1],
                               object->rot[2]));
    if (StringEqual (name, "dRotX"))
        return (PyFloat_FromDouble(object->drot[0]));
    if (StringEqual (name, "dRotY"))
        return (PyFloat_FromDouble(object->drot[1]));
    if (StringEqual (name, "dRotZ"))
        return (PyFloat_FromDouble(object->drot[2]));
    if (StringEqual (name, "drot"))
        return (Py_BuildValue ("fff", object->drot[0], object->drot[1],
                               object->drot[2]));
    if (StringEqual (name, "SizeX"))
        return (PyFloat_FromDouble(object->size[0]));
    if (StringEqual (name, "SizeY"))
        return (PyFloat_FromDouble(object->size[1]));
    if (StringEqual (name, "SizeZ"))
        return (PyFloat_FromDouble(object->size[2]));
    if (StringEqual (name, "size"))
        return (Py_BuildValue ("fff", object->size[0], object->size[1],
                               object->size[2]));
    if (StringEqual (name, "dSizeX"))
        return (PyFloat_FromDouble(object->dsize[0]));
    if (StringEqual (name, "dSizeY"))
        return (PyFloat_FromDouble(object->dsize[1]));
    if (StringEqual (name, "dSizeZ"))
        return (PyFloat_FromDouble(object->dsize[2]));
    if (StringEqual (name, "dsize"))
        return (Py_BuildValue ("fff", object->dsize[0], object->dsize[1],
                               object->dsize[2]));
    if (strncmp (name,"Eff", 3) == 0)
    {
        if ( (object->type == OB_IKA) && (object->data != NULL) )
        {
            ika = object->data;
            switch (name[3])
            {
                case 'X':
                    return (PyFloat_FromDouble (ika->effg[0]));
                case 'Y':
                    return (PyFloat_FromDouble (ika->effg[1]));
                case 'Z':
                    return (PyFloat_FromDouble (ika->effg[2]));
                default:
                    /* Do we need to display a sensible error message here? */
                    return (NULL);
            }
        }
        return (NULL);
    }
    if (StringEqual (name, "Layer"))
        return (PyInt_FromLong(object->lay));
    if (StringEqual (name, "parent"))
        return (ObjectCreatePyObject (object->parent));
    if (StringEqual (name, "track"))
        return (ObjectCreatePyObject (object->track));
    if (StringEqual (name, "data"))
    {
        printf ("This is not implemented yet.\n");
        return (Py_None);
    }
    if (StringEqual (name, "ipo"))
    {
        printf ("This is not implemented yet.\n");
        return (Py_None);
    }
    if (StringEqual (name, "mat"))
    {
        printf ("This is not implemented yet. (matrix)\n");
        return (Py_None);
    }
    if (StringEqual (name, "matrix"))
    {
        printf ("This is not implemented yet. (matrix)\n");
        return (Py_None);
    }
    if (StringEqual (name, "colbits"))
        return (Py_BuildValue ("h", object->colbits));
    if (StringEqual (name, "drawType"))
        return (Py_BuildValue ("b", object->dt));
    if (StringEqual (name, "drawMode"))
        return (Py_BuildValue ("b", object->dtx));

    printf ("Unknown variable.\n");
    return (Py_None);
}

/*****************************************************************************/
/* Function:    ObjectSetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Python and sets   */
/*              it accordingly in Blender.                                   */
/*****************************************************************************/
int ObjectSetAttr (C_BlenObject *obj, char *name, PyObject *value)
{
    struct Object    * object;
    struct Ika      * ika;

    object = obj->object;
    if (StringEqual (name, "LocX"))
        return (!PyArg_Parse (value, "f", &(object->loc[0])));
    if (StringEqual (name, "LocY"))
        return (!PyArg_Parse (value, "f", &(object->loc[1])));
    if (StringEqual (name, "LocZ"))
        return (!PyArg_Parse (value, "f", &(object->loc[2])));
    if (StringEqual (name, "loc"))
        return (!PyArg_Parse (value, "fff", &(object->loc[0]),
                              &(object->loc[1]), &(object->loc[2])));
    if (StringEqual (name, "dLocX"))
        return (!PyArg_Parse (value, "f", &(object->dloc[0])));
    if (StringEqual (name, "dLocY"))
        return (!PyArg_Parse (value, "f", &(object->dloc[1])));
    if (StringEqual (name, "dLocZ"))
        return (!PyArg_Parse (value, "f", &(object->dloc[2])));
    if (StringEqual (name, "dloc"))
        return (!PyArg_Parse (value, "fff", &(object->dloc[0]),
                              &(object->dloc[1]), &(object->dloc[2])));
    if (StringEqual (name, "RotX"))
        return (!PyArg_Parse (value, "f", &(object->rot[0])));
    if (StringEqual (name, "RotY"))
        return (!PyArg_Parse (value, "f", &(object->rot[1])));
    if (StringEqual (name, "RotZ"))
        return (!PyArg_Parse (value, "f", &(object->rot[2])));
    if (StringEqual (name, "rot"))
        return (!PyArg_Parse (value, "fff", &(object->rot[0]),
                              &(object->rot[1]), &(object->rot[2])));
    if (StringEqual (name, "dRotX"))
        return (!PyArg_Parse (value, "f", &(object->drot[0])));
    if (StringEqual (name, "dRotY"))
        return (!PyArg_Parse (value, "f", &(object->drot[1])));
    if (StringEqual (name, "dRotZ"))
        return (!PyArg_Parse (value, "f", &(object->drot[2])));
    if (StringEqual (name, "drot"))
        return (!PyArg_Parse (value, "fff", &(object->drot[0]),
                              &(object->drot[1]), &(object->drot[2])));
    if (StringEqual (name, "SizeX"))
        return (!PyArg_Parse (value, "f", &(object->size[0])));
    if (StringEqual (name, "SizeY"))
        return (!PyArg_Parse (value, "f", &(object->size[1])));
    if (StringEqual (name, "SizeZ"))
        return (!PyArg_Parse (value, "f", &(object->size[2])));
    if (StringEqual (name, "size"))
        return (!PyArg_Parse (value, "fff", &(object->size[0]),
                              &(object->size[1]), &(object->size[2])));
    if (StringEqual (name, "dSizeX"))
        return (!PyArg_Parse (value, "f", &(object->dsize[0])));
    if (StringEqual (name, "dSizeY"))
        return (!PyArg_Parse (value, "f", &(object->dsize[1])));
    if (StringEqual (name, "dSizeZ"))
        return (!PyArg_Parse (value, "f", &(object->dsize[2])));
    if (StringEqual (name, "dsize"))
        return (!PyArg_Parse (value, "fff", &(object->dsize[0]),
                              &(object->dsize[1]), &(object->dsize[2])));
    if (strncmp (name,"Eff", 3) == 0)
    {
        if ( (object->type == OB_IKA) && (object->data != NULL) )
        {
            ika = object->data;
            switch (name[3])
            {
                case 'X':
                    return (!PyArg_Parse (value, "f", &(ika->effg[0])));
                case 'Y':
                    return (!PyArg_Parse (value, "f", &(ika->effg[1])));
                case 'Z':
                    return (!PyArg_Parse (value, "f", &(ika->effg[2])));
                default:
                    /* Do we need to display a sensible error message here? */
                    return (0);
            }
        }
        return (0);
    }
    if (StringEqual (name, "Layer"))
        return (!PyArg_Parse (value, "i", &(object->lay)));
    if (StringEqual (name, "parent"))
    {
        /* This is not allowed. */
        PythonReturnErrorObject (PyExc_AttributeError,
                    "Setting the parent is not allowed.");
        return (0);
    }
    if (StringEqual (name, "track"))
    {
        /* This is not allowed. */
        PythonReturnErrorObject (PyExc_AttributeError,
                    "Setting the track is not allowed.");
        return (0);
    }
    if (StringEqual (name, "data"))
    {
        /* This is not allowed. */
        PythonReturnErrorObject (PyExc_AttributeError,
                    "Setting the data is not allowed.");
        return (0);
    }
    if (StringEqual (name, "ipo"))
    {
        /* This is not allowed. */
        PythonReturnErrorObject (PyExc_AttributeError,
                    "Setting the ipo is not allowed.");
        return (0);
    }
    if (StringEqual (name, "mat"))
    {
        printf ("This is not implemented yet. (matrix)\n");
        return (1);
    }
    if (StringEqual (name, "matrix"))
    {
        printf ("This is not implemented yet. (matrix)\n");
        return (1);
    }
    if (StringEqual (name, "colbits"))
        return (!PyArg_Parse (value, "h", &(object->colbits)));
    if (StringEqual (name, "drawType"))
        return (!PyArg_Parse (value, "b", &(object->dt)));
    if (StringEqual (name, "drawMode"))
        return (!PyArg_Parse (value, "b", &(object->dtx)));

    printf ("Unknown variable.\n");
    return (0);
}

