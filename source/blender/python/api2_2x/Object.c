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
/* Python API function prototypes for the Blender module.                    */
/*****************************************************************************/
static PyObject *M_Object_New(PyObject *self, PyObject *args);
PyObject *M_Object_Get(PyObject *self, PyObject *args);
static PyObject *M_Object_GetSelected (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Object.__doc__                                                    */
/*****************************************************************************/
char M_Object_doc[] =
"The Blender Object module\n\n\
This module provides access to **Object Data** in Blender.\n";

char M_Object_New_doc[] =
"(type) - Add a new object of type 'type' in the current scene";

char M_Object_Get_doc[] =
"(name) - return the object with the name 'name', returns None if not\
    found.\n\
    If 'name' is not specified, it returns a list of all objects in the\n\
    current scene.";

char M_Object_GetSelected_doc[] =
"() - Returns a list of selected Objects in the active layer(s)\n\
The active object is the first in the list, if visible";

/*****************************************************************************/
/* Python method structure definition for Blender.Object module:             */
/*****************************************************************************/
struct PyMethodDef M_Object_methods[] = {
    {"New",         (PyCFunction)M_Object_New,         METH_VARARGS,
                    M_Object_New_doc},
    {"Get",         (PyCFunction)M_Object_Get,         METH_VARARGS,
                    M_Object_Get_doc},
    {"get",         (PyCFunction)M_Object_Get,         METH_VARARGS,
                    M_Object_Get_doc},
    {"getSelected", (PyCFunction)M_Object_GetSelected, METH_VARARGS,
                    M_Object_GetSelected_doc},
    {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_Object methods declarations:                                     */
/*****************************************************************************/
static PyObject *Object_clrParent (C_Object *self, PyObject *args);
static PyObject *Object_getData (C_Object *self);
static PyObject *Object_getDeformData (C_Object *self);
static PyObject *Object_getDeltaLocation (C_Object *self);
static PyObject *Object_getDrawMode (C_Object *self);
static PyObject *Object_getDrawType (C_Object *self);
static PyObject *Object_getEuler (C_Object *self);
static PyObject *Object_getInverseMatrix (C_Object *self);
static PyObject *Object_getLocation (C_Object *self, PyObject *args);
static PyObject *Object_getMaterials (C_Object *self);
static PyObject *Object_getMatrix (C_Object *self);
static PyObject *Object_getParent (C_Object *self);
static PyObject *Object_getTracked (C_Object *self);
static PyObject *Object_getType (C_Object *self);
static PyObject *Object_link (C_Object *self, PyObject *args);
static PyObject *Object_makeParent (C_Object *self, PyObject *args);
static PyObject *Object_materialUsage (C_Object *self, PyObject *args);
static PyObject *Object_setDeltaLocation (C_Object *self, PyObject *args);
static PyObject *Object_setDrawMode (C_Object *self, PyObject *args);
static PyObject *Object_setDrawType (C_Object *self, PyObject *args);
static PyObject *Object_setEuler (C_Object *self, PyObject *args);
static PyObject *Object_setLocation (C_Object *self, PyObject *args);
static PyObject *Object_setMaterials (C_Object *self, PyObject *args);
static PyObject *Object_shareFrom (C_Object *self, PyObject *args);

/*****************************************************************************/
/* Python C_Object methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_Object_methods[] = {
    /* name, method, flags, doc */
    {"clrParent",        (PyCFunction)Object_clrParent,        METH_VARARGS,
        "Clears parent object. Optionally specify:\n\
mode\n\t2: Keep object transform\nfast\n\t>0: Don't update scene \
hierarchy (faster)"},
    {"getData",          (PyCFunction)Object_getData,          METH_NOARGS,
        "Returns the datablock object containing the object's data, \
e.g. Mesh"},
    {"getDeformData",    (PyCFunction)Object_getDeformData,    METH_NOARGS,
        "Returns the datablock object containing the object's deformed \
data.\nCurrently, this is only supported for a Mesh"},
    {"getDeltaLocation", (PyCFunction)Object_getDeltaLocation, METH_NOARGS,
        "Returns the object's delta location (x, y, z)"},
    {"getDrawMode",      (PyCFunction)Object_getDrawMode,      METH_NOARGS,
        "Returns the object draw modes"},
    {"getDrawType",      (PyCFunction)Object_getDrawType,      METH_NOARGS,
        "Returns the object draw type"},
    {"getEuler",         (PyCFunction)Object_getEuler,         METH_NOARGS,
        "Returns the object's rotation as Euler rotation vector\n\
(rotX, rotY, rotZ)"},
    {"getInverseMatrix", (PyCFunction)Object_getInverseMatrix, METH_NOARGS,
        "Returns the object's inverse matrix"},
    {"getLocation",      (PyCFunction)Object_getLocation,      METH_VARARGS,
        "Returns the object's location (x, y, z)"},
    {"getMaterials",     (PyCFunction)Object_getMaterials,     METH_NOARGS,
        "Returns list of materials assigned to the object"},
    {"getMatrix",        (PyCFunction)Object_getMatrix,        METH_NOARGS,
        "Returns the object matrix"},
    {"getParent",        (PyCFunction)Object_getParent,        METH_NOARGS,
        "Returns the object's parent object"},
    {"getTracked",       (PyCFunction)Object_getTracked,       METH_NOARGS,
        "Returns the object's tracked object"},
    {"getType",          (PyCFunction)Object_getType,          METH_NOARGS,
        "Returns type of string of Object"},
    {"link",             (PyCFunction)Object_link,             METH_VARARGS,
        "Links Object with data provided in the argument. The data must \n\
match the Object's type, so you cannot link a Lamp to a Mesh type object."},
    {"makeParent",       (PyCFunction)Object_makeParent,       METH_VARARGS,
        "Makes the object the parent of the objects provided in the \n\
argument which must be a list of valid Objects. Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fase:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitely update the Scene hierarchy."},
    {"materialUsage",    (PyCFunction)Object_materialUsage,    METH_VARARGS,
        "Determines the way the material is used and returs status.\n\
Possible arguments (provide as strings):\n\
\tData:   Materials assigned to the object's data are shown. (default)\n\
\tObject: Materials assigned to the object are shown."},
    {"setDeltaLocation", (PyCFunction)Object_setDeltaLocation, METH_VARARGS,
        "Sets the object's delta location which must be a vector triple."},
    {"setDrawMode",      (PyCFunction)Object_setDrawMode,      METH_VARARGS,
        "Sets the object's drawing mode. The argument can be a sum of:\n\
2:  axis\n4:  texspace\n8:  drawname\n16: drawimage\n32: drawwire"},
    {"setDrawType",      (PyCFunction)Object_setDrawType,      METH_VARARGS,
        "Sets the object's drawing type. The argument must be one of:\n\
1: Bounding box\n2: Wire\n3: Solid\n4: Shaded\n5: Textured"},
    {"setEuler",         (PyCFunction)Object_setEuler,         METH_VARARGS,
        "Set the object's rotation according to the specified Euler\n\
angles. The argument must be a vector triple"},
    {"setLocation",      (PyCFunction)Object_setLocation,      METH_VARARGS,
        "Set the object's location. The first argument must be a vector\n\
triple."},
    {"setMaterials",     (PyCFunction)Object_setMaterials,     METH_VARARGS,
        "Sets materials. The argument must be a list of valid material\n\
objects."},
    {"shareFrom",        (PyCFunction)Object_shareFrom,        METH_VARARGS,
        "Link data of self with object specified in the argument. This\n\
works only if self and the object specified are of the same type."},
    {0}
};

/*****************************************************************************/
/* PythonTypeObject callback function prototypes                             */
/*****************************************************************************/
static void      ObjectDeAlloc (C_Object *obj);
static int       ObjectPrint   (C_Object *obj, FILE *fp, int flags);
static PyObject* ObjectGetAttr (C_Object *obj, char *name);
static int       ObjectSetAttr (C_Object *obj, char *name, PyObject *v);
static PyObject* ObjectRepr    (C_Object *obj);

/*****************************************************************************/
/* Python TypeObject structure definition.                                   */
/*****************************************************************************/
PyTypeObject Object_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                                /* ob_size */
    "Object",                         /* tp_name */
    sizeof (C_Object),                /* tp_basicsize */
    0,                                /* tp_itemsize */
    /* methods */
    (destructor)ObjectDeAlloc,        /* tp_dealloc */
    (printfunc)ObjectPrint,           /* tp_print */
    (getattrfunc)ObjectGetAttr,       /* tp_getattr */
    (setattrfunc)ObjectSetAttr,       /* tp_setattr */
    0,                                /* tp_compare */
    (reprfunc)ObjectRepr,             /* tp_repr */
    0,                                /* tp_as_number */
    0,                                /* tp_as_sequence */
    0,                                /* tp_as_mapping */
    0,                                /* tp_as_hash */
    0,0,0,0,0,0,
    0,                                /* tp_doc */ 
    0,0,0,0,0,0,
    C_Object_methods,                 /* tp_methods */
    0,                                /* tp_members */
};

/*****************************************************************************/
/* Function:              M_Object_New                                       */
/* Python equivalent:     Blender.Object.New                                 */
/*****************************************************************************/
PyObject *M_Object_New(PyObject *self, PyObject *args)
{
    struct Object   * object;
    C_Object        * blen_object;
    int               type;
    char            * str_type;
    char            * name = NULL;

    printf ("In Object_New()\n");

    if (!PyArg_ParseTuple(args, "s|s", &str_type, &name))
    {
        PythonReturnErrorObject (PyExc_TypeError,
                    "string expected as argument");
        return (NULL);
    }

    if (strcmp (str_type, "Armature") == 0)     type = OB_ARMATURE;
    else if (strcmp (str_type, "Camera") == 0)  type = OB_CAMERA;
    else if (strcmp (str_type, "Curve") == 0)   type = OB_CURVE;
/*    else if (strcmp (str_type, "Text") == 0)    type = OB_FONT; */
/*    else if (strcmp (str_type, "Ika") == 0)     type = OB_IKA; */
    else if (strcmp (str_type, "Lamp") == 0)    type = OB_LAMP;
/*    else if (strcmp (str_type, "Lattice") == 0) type = OB_LATTICE; */
/*    else if (strcmp (str_type, "Mball") == 0)   type = OB_MBALL; */
    else if (strcmp (str_type, "Mesh") == 0)    type = OB_MESH;
/*    else if (strcmp (str_type, "Surf") == 0)    type = OB_SURF; */
/*    else if (strcmp (str_type, "Wave") == 0)    type = OB_WAVE; */
    else if (strcmp (str_type, "Empty") == 0)   type = OB_EMPTY;
    else
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
            "Unknown type specified"));
    }

    /* Create a new object. */
    if (name == NULL)
    {
        /* No name is specified, set the name to the type of the object. */
        name = str_type;
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
        case OB_ARMATURE:
        /* TODO: Do we need to add something to G? (see the OB_LAMP case) */
            object->data = add_armature();
            break;
        case OB_CAMERA:
        /* TODO: Do we need to add something to G? (see the OB_LAMP case) */
            object->data = add_camera();
            break;
        case OB_CURVE:
            object->data = add_curve(OB_CURVE);
            G.totcurve++;
            break;
        case OB_LAMP:
            object->data = add_lamp();
            G.totlamp++;
            break;
        case OB_MESH:
            object->data = add_mesh();
            G.totmesh++;
            break;

    /* TODO the following types will be supported later
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
    */
    }

    G.totobj++;

    /* Create a Python object from it. */
    blen_object = (C_Object*)PyObject_NEW (C_Object, &Object_Type); 
    blen_object->object = object;
    blen_object->data = NULL;
    blen_object->parent = NULL;

    return ((PyObject*)blen_object);
}

/*****************************************************************************/
/* Function:              M_Object_Get                                       */
/* Python equivalent:     Blender.Object.Get                                 */
/*****************************************************************************/
PyObject *M_Object_Get(PyObject *self, PyObject *args)
{
    struct Object   * object;
    char            * name = NULL;

    printf ("In Object_Get()\n");

    PyArg_ParseTuple(args, "|s", &name);

    if (name != NULL)
    {
        C_Object    * blen_object;

        object = GetObjectByName (name);

        if (object == NULL)
        {
            /* No object exists with the name specified in the argument name. */
            return (PythonReturnErrorObject (PyExc_AttributeError,
                        "Unknown object specified."));
        }
        blen_object = (C_Object*)PyObject_NEW (C_Object, &Object_Type); 
        blen_object->object = object;
        blen_object->parent = NULL;
        blen_object->data = NULL;

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
static PyObject *M_Object_GetSelected (PyObject *self, PyObject *args)
{
    C_Object        * blen_object;
    PyObject        * list;
    Base            * base_iter;

    printf ("In Object_GetSelected()\n");

    list = PyList_New (0);
    if ((G.scene->basact) &&
        ((G.scene->basact->flag & SELECT) &&
         (G.scene->basact->lay & G.vd->lay)))
    {
        /* Active object is first in the list. */
        blen_object = (C_Object*)PyObject_NEW (C_Object, &Object_Type); 
        if (blen_object == NULL)
        {
            Py_DECREF (list);
            Py_INCREF (Py_None);
            return (Py_None);
        }
        blen_object->object = G.scene->basact->object;
        blen_object->data = NULL;
        PyList_Append (list, (PyObject*)blen_object);
    }

    base_iter = G.scene->base.first;
    while (base_iter)
    {
        if (((base_iter->flag & SELECT) &&
             (base_iter->lay & G.vd->lay)) &&
            (base_iter != G.scene->basact))
        {
            blen_object = (C_Object*)PyObject_NEW (C_Object, &Object_Type); 
            if (blen_object == NULL)
            {
                Py_DECREF (list);
                Py_INCREF (Py_None);
                return (Py_None);
            }
            blen_object->object = base_iter->object;
            blen_object->data = NULL;
            PyList_Append (list, (PyObject*)blen_object);
        }
        base_iter = base_iter->next;
    }
    return (list);
}

/*****************************************************************************/
/* Function:              initObject                                         */
/*****************************************************************************/
PyObject *M_Object_Init (void)
{
    PyObject    * module;

    printf ("In initObject()\n");

    Object_Type.ob_type = &PyType_Type;

    module = Py_InitModule3("Object", M_Object_methods, M_Object_doc);

    return (module);
}

/*****************************************************************************/
/* Python C_Object methods:                                                  */
/*****************************************************************************/
static PyObject *Object_clrParent (C_Object *self, PyObject *args)
{
    int       mode=0;
    int       fast=0;

    if (!PyArg_ParseTuple (args, "|ii", &mode, &fast))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected one or two integers as arguments"));
    }

    /* Remove the link only, the object is still in the scene. */
    self->object->parent = NULL;
    self->parent = NULL;

    if (mode == 2)
    {
        /* Keep transform */
        apply_obmat (self->object);
    }

    if (!fast)
    {
        sort_baselist (G.scene);
    }

    return (Py_None);
}

static PyObject *Object_getData (C_Object *self)
{
    PyObject  * data_object;
//#    int         obj_id;
//#    ID        * id;

    /* If there's a valid PyObject already, then just return that one. */
    if (self->data != NULL)
    {
        Py_INCREF (self->data);
        return (self->data);
    }

    /* If there's no data associated to the Object, then there's nothing to */
    /* return. */
    if (self->object->data == NULL)
    {
        Py_INCREF (Py_None);
        return (Py_None);
    }

    data_object = NULL;

    //#id = (ID*)self->object;
    //#obj_id = MAKE_ID2 (id->name[0], id->name[1]);
    switch (self->object->type)//#obj_id)
    {
        case OB_ARMATURE://#ID_AR:
            data_object = M_ArmatureCreatePyObject (self->object->data);
            break;
        case OB_CAMERA://#ID_CA:
            data_object = Camera_CreatePyObject (self->object->data);
            break;
        case OB_CURVE://#ID_CU:
            data_object = CurveCreatePyObject (self->object->data);
            break;
        case ID_IM:
            data_object = Image_CreatePyObject (self->object->data);
            break;
        case ID_IP:
            break;
        case OB_LAMP://#ID_LA:
            data_object = Lamp_CreatePyObject (self->object->data);
            break;
        case ID_MA:
            break;
        case OB_MESH://#ID_ME:
            data_object = NMesh_CreatePyObject (self->object->data);
            break;
        case ID_OB:
            data_object = M_ObjectCreatePyObject (self->object->data);
            break;
        case ID_SCE:
            break;
        case ID_TXT:
            break;
        case ID_WO:
            break;
        default:
            break;
    }
    if (data_object == NULL)
    {
        Py_INCREF (Py_None);
        return (Py_None);
    }
    else
    {
        self->data = data_object;
        Py_INCREF (data_object);
        return (data_object);
    }
}

static PyObject *Object_getDeformData (C_Object *self)
{
    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "getDeformData: not yet implemented"));
}

static PyObject *Object_getDeltaLocation (C_Object *self)
{
    PyObject *attr = Py_BuildValue ("fff",
                                    self->object->dloc[0],
                                    self->object->dloc[1],
                                    self->object->dloc[2]);

    if (attr) return (attr);

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.dloc attributes"));
}

static PyObject *Object_getDrawMode (C_Object *self)
{
    PyObject *attr = Py_BuildValue ("b", self->object->dtx);

    if (attr) return (attr);

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.drawMode attribute"));
}

static PyObject *Object_getDrawType (C_Object *self)
{
    PyObject *attr = Py_BuildValue ("b", self->object->dt);

    if (attr) return (attr);

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.drawType attribute"));
}

static PyObject *Object_getEuler (C_Object *self)
{
    PyObject *attr = Py_BuildValue ("fff",
                                    self->object->drot[0],
                                    self->object->drot[1],
                                    self->object->drot[2]);

    if (attr) return (attr);

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.drot attributes"));
}

static PyObject *Object_getInverseMatrix (C_Object *self)
{
    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "getInverseMatrix: not yet implemented"));
}

static PyObject *Object_getLocation (C_Object *self, PyObject *args)
{
    PyObject *attr = Py_BuildValue ("fff",
                                    self->object->loc[0],
                                    self->object->loc[1],
                                    self->object->loc[2]);

    if (attr) return (attr);

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.loc attributes"));
}

static PyObject *Object_getMaterials (C_Object *self)
{
    /* TODO: Implement when the Material module is implemented. */
    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "getMaterials: not yet implemented"));
}

static PyObject *Object_getMatrix (C_Object *self)
{
    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "getMatrix: not yet implemented"));
}

static PyObject *Object_getParent (C_Object *self)
{
    PyObject *attr;

    if (self->parent)
    {
        Py_INCREF ((PyObject*)self->parent);
        return ((PyObject*)self->parent);
    }

    if (self->object->parent == NULL)
    {
        return (EXPP_incr_ret (Py_None));
    }
    attr = M_ObjectCreatePyObject (self->object->parent);

    if (attr)
    {
        self->parent = (struct C_Object*)attr;
        return (attr);
    }

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.parent attribute"));
}

static PyObject *Object_getTracked (C_Object *self)
{
    PyObject    *attr;

    if (self->track)
    {
        Py_INCREF ((PyObject*)self->track);
        return ((PyObject*)self->track);
    }

    /* TODO: what if self->object->track==NULL? Should we return Py_None? */
    attr = M_ObjectCreatePyObject (self->object->track);

    if (attr)
    {
        self->track = (struct C_Object*)attr;
        return (attr);
    }

    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't get Object.track attribute"));
}

static PyObject *Object_getType (C_Object *self)
{
    switch (self->object->type)
    {
        case OB_ARMATURE:   return (Py_BuildValue ("s", "Armature"));
        case OB_CAMERA:     return (Py_BuildValue ("s", "Camera"));
        case OB_CURVE:      return (Py_BuildValue ("s", "Curve"));
        case OB_EMPTY:      return (Py_BuildValue ("s", "Empty"));
        case OB_FONT:       return (Py_BuildValue ("s", "Text"));
        case OB_IKA:        return (Py_BuildValue ("s", "Ika"));
        case OB_LAMP:       return (Py_BuildValue ("s", "Lamp"));
        case OB_LATTICE:    return (Py_BuildValue ("s", "Lattice"));
        case OB_MBALL:      return (Py_BuildValue ("s", "MBall"));
        case OB_MESH:       return (Py_BuildValue ("s", "Mesh"));
        case OB_SURF:       return (Py_BuildValue ("s", "Surf"));
        case OB_WAVE:       return (Py_BuildValue ("s", "Wave"));
        default:            return (Py_BuildValue ("s", "unknown"));
    }
}

static PyObject *Object_link (C_Object *self, PyObject *args)
{
    PyObject    * py_data;
    ID          * id;
    ID          * oldid;
    int           obj_id;
    void        * data = NULL;

    if (!PyArg_ParseTuple (args, "O", &py_data))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected an object as argument"));
    }
    if (Camera_CheckPyObject (py_data))
        data = (void *)Camera_FromPyObject (py_data);
    if (Lamp_CheckPyObject (py_data))
        data = (void *)Lamp_FromPyObject (py_data);
    /* TODO: add the (N)Mesh check and from functions here when finished. */

    oldid = (ID*) self->object->data;
    id = (ID*) data;
    obj_id = MAKE_ID2 (id->name[0], id->name[1]);

    switch (obj_id)
    {
        case ID_CA:
            if (self->object->type != OB_CAMERA)
            {
                return (PythonReturnErrorObject (PyExc_AttributeError,
                    "The 'link' object is incompatible with the base object"));
            }
            break;
        case ID_LA:
            if (self->object->type != OB_LAMP)
            {
                return (PythonReturnErrorObject (PyExc_AttributeError,
                    "The 'link' object is incompatible with the base object"));
            }
            break;
        case ID_ME:
            if (self->object->type != OB_MESH)
            {
                return (PythonReturnErrorObject (PyExc_AttributeError,
                    "The 'link' object is incompatible with the base object"));
            }
            break;
        default:
            return (PythonReturnErrorObject (PyExc_AttributeError,
                "Linking this object type is not supported"));
    }
    self->object->data = data;
    self->data = py_data;
    id_us_plus (id);
    if (oldid)
    {
        if (oldid->us > 0)
        {
            oldid->us--;
        }
        else
        {
            return (PythonReturnErrorObject (PyExc_RuntimeError,
                "old object reference count below 0"));
        }
    }
    return (Py_None);
}

static PyObject *Object_makeParent (C_Object *self, PyObject *args)
{
    PyObject    * list;
    PyObject    * py_child;
    C_Object    * py_obj_child;
    Object      * child;
    Object      * parent;
    int           noninverse;
    int           fast;
    int           i;

    /* Check if the arguments passed to makeParent are valid. */
    if (!PyArg_ParseTuple (args, "O|ii", &list, &noninverse, &fast))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a list of objects and one or two integers as arguments"));
    }
    if (!PySequence_Check (list))
    {
        return (PythonReturnErrorObject (PyExc_TypeError,
            "expected a list of objects"));
    }

    /* Check if the PyObject passed in list is a Blender object. */
    for (i=0 ; i<PySequence_Length (list) ; i++)
    {
        child = NULL;
        py_child = PySequence_GetItem (list, i);
        if (M_ObjectCheckPyObject (py_child))
            child = (Object*) M_ObjectFromPyObject (py_child);

        if (child == NULL)
        {
            return (PythonReturnErrorObject (PyExc_TypeError,
                "Object Type expected"));
        }

        parent = (Object*)self->object;
        if (test_parent_loop (parent, child))
        {
            return (PythonReturnErrorObject (PyExc_RuntimeError,
                "parenting loop detected - parenting failed"));
        }
        child->partype = PAROBJECT;
        child->parent = parent;
        py_obj_child = (C_Object *) py_child;
        py_obj_child->parent = (struct C_Object *)self;
        if (noninverse == 1)
        {
            /* Parent inverse = unity */
            child->loc[0] = 0.0;
            child->loc[1] = 0.0;
            child->loc[2] = 0.0;
        }
        else
        {
            what_does_parent (child);
            Mat4Invert (child->parentinv, parent->obmat);
        }

        if (!fast)
        {
            sort_baselist (G.scene);
        }

        /* We don't need the child object anymore. */
        Py_DECREF ((PyObject *) child);
    }
    return (Py_None);
}

static PyObject *Object_materialUsage (C_Object *self, PyObject *args)
{
    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "materialUsage: not yet implemented"));
}

static PyObject *Object_setDeltaLocation (C_Object *self, PyObject *args)
{
    float   dloc1;
    float   dloc2;
    float   dloc3;

    if (!PyArg_Parse (args, "fff", &dloc1, &dloc2, &dloc3))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected three float arguments"));
    }

    self->object->dloc[1] = dloc1;
    self->object->dloc[2] = dloc2;
    self->object->dloc[3] = dloc3;

    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *Object_setDrawMode (C_Object *self, PyObject *args)
{
    char    dt;

    if (!PyArg_Parse (args, "b", &dt))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected an integer as argument"));
    }
    self->object->dt = dt;

    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *Object_setDrawType (C_Object *self, PyObject *args)
{ 
    char    dtx;

    if (!PyArg_Parse (args, "b", &dtx))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected an integer as argument"));
    }
    self->object->dtx = dtx;

    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *Object_setEuler (C_Object *self, PyObject *args)
{
    float   drot1;
    float   drot2;
    float   drot3;

    if (!PyArg_Parse (args, "fff", &drot1, &drot2, &drot3))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected three float arguments"));
    }

    self->object->drot[1] = drot1;
    self->object->drot[2] = drot2;
    self->object->drot[3] = drot3;

    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *Object_setLocation (C_Object *self, PyObject *args)
{
    float   loc1;
    float   loc2;
    float   loc3;

    if (!PyArg_Parse (args, "fff", &loc1, &loc2, &loc3))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected three float arguments"));
    }

    self->object->loc[1] = loc1;
    self->object->loc[2] = loc2;
    self->object->loc[3] = loc3;

    Py_INCREF (Py_None);
    return (Py_None);
}

static PyObject *Object_setMaterials (C_Object *self, PyObject *args)
{
#if 0
    PyObject     * list;
    int            len;
    int            i;
    Material    ** matlist;

    if (!PyArg_Parse (args, "O", &list))
    {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                "expected a list of materials as argument"));
    }

    len = PySequence_Length (list);
    if (len > 0)
    {
        matlist = EXPP_newMaterialList_fromPyList (list);
        if (!matlist)
        {
            return (PythonReturnErrorObject (PyExc_AttributeError,
                "material list must be a list of valid materials!"));
        }
        if ((len < 0) || (len > MAXMAT))
        {
            return (PythonReturnErrorObject (PyExc_RuntimeError,
                "illegal material index!"));
        }

        if (self->object->mat)
        {
            /* TODO: create replacement function */
            releaseMaterialList (self->object->mat, len);
        }
        /* Increase the user count on all materials */
        for (i=0 ; i<len ; i++)
        {
            id_us_plus ((ID *) matlist[i]);
        }
        self->object->mat = matlist;
        self->object->totcol = len;
        self->object->actcol = -1;

        switch (self->object->type)
        {
            case OB_CURVE:  /* fall through */
            case OB_FONT:   /* fall through */
            case OB_MESH:   /* fall through */
            case OB_MBALL:  /* fall through */
            case OB_SURF
                /* TODO: create replacement function */:
                synchronizeMaterialLists (self->object, self->object->data);
                break;
            default:
                break;
        }
    }
    return (Py_None);
#endif

    return (PythonReturnErrorObject (PyExc_NotImplementedError,
            "setMaterials: not yet implemented"));
}

static PyObject *Object_shareFrom (C_Object *self, PyObject *args)
{
    C_Object        * object;
    ID              * id;
    ID              * oldid;

    if (!PyArg_Parse (args, "O", &object))
    {
        PythonReturnErrorObject (PyExc_AttributeError,
                "expected an object argument");
        return (NULL);
    }
    if (!M_ObjectCheckPyObject ((PyObject*)object))
    {
        PythonReturnErrorObject (PyExc_TypeError,
                "argument 1 is not of type 'Object'");
        return (NULL);
    }

    if (self->object->type != object->object->type)
    {
        PythonReturnErrorObject (PyExc_TypeError,
                "objects are not of same data type");
        return (NULL);
    }
    switch (self->object->type)
    {
        case OB_MESH:
            oldid = (ID*) self->object->data;
            id = (ID*) object->data;
            self->object->data = object->data;
            if (self->data != NULL)
            {
                Py_DECREF (self->data);
                self->data = NULL;
            }
            id_us_plus (id);
            if (oldid)
            {
                if (oldid->us > 0)
                {
                    oldid->us--;
                }
                else
                {
                    return (PythonReturnErrorObject (PyExc_RuntimeError,
                            "old object reference count below 0"));
                }
            }
            Py_INCREF (Py_None);
            return (Py_None);
        default:
            PythonReturnErrorObject (PyExc_TypeError,
                    "type not supported");
            return (NULL);
    }
    return (Py_None);
}

/*****************************************************************************/
/* Function:    M_ObjectCreatePyObject                                       */
/* Description: This function will create a new BlenObject from an existing  */
/*              Object structure.                                            */
/*****************************************************************************/
PyObject* M_ObjectCreatePyObject (struct Object *obj)
{
    C_Object    * blen_object;

    printf ("In M_ObjectCreatePyObject\n");

    blen_object = (C_Object*)PyObject_NEW (C_Object, &Object_Type);

    if (blen_object == NULL)
    {
        return (NULL);
    }
    blen_object->object = obj;
    return ((PyObject*)blen_object);
}

/*****************************************************************************/
/* Function:    M_ObjectCheckPyObject                                        */
/* Description: This function returns true when the given PyObject is of the */
/*              type Object. Otherwise it will return false.                 */
/*****************************************************************************/
int M_ObjectCheckPyObject (PyObject *py_obj)
{
    return (py_obj->ob_type == &Object_Type);
}

/*****************************************************************************/
/* Function:    M_ObjectFromPyObject                                         */
/* Description: This function returns the Blender object from the given      */
/*              PyObject.                                                    */
/*****************************************************************************/
struct Object* M_ObjectFromPyObject (PyObject *py_obj)
{
    C_Object    * blen_obj;

    blen_obj = (C_Object*)py_obj;
    return (blen_obj->object);
}

/*****************************************************************************/
/* Function:    ObjectDeAlloc                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
static void ObjectDeAlloc (C_Object *obj)
{
    PyObject_DEL (obj);
}

/*****************************************************************************/
/* Function:    ObjectGetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Blender and       */
/*              passes it to Python.                                         */
/*****************************************************************************/
static PyObject* ObjectGetAttr (C_Object *obj, char *name)
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
        return (M_ObjectCreatePyObject (object->parent));
    if (StringEqual (name, "track"))
        return (M_ObjectCreatePyObject (object->track));
    if (StringEqual (name, "data"))
        return (Object_getData (obj));
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

    /* not an attribute, search the methods table */
    return Py_FindMethod(C_Object_methods, (PyObject *)obj, name);
}

/*****************************************************************************/
/* Function:    ObjectSetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Python and sets   */
/*              it accordingly in Blender.                                   */
/*****************************************************************************/
static int ObjectSetAttr (C_Object *obj, char *name, PyObject *value)
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
    {
        if (Object_setLocation (obj, value) != Py_None)
            return (-1);
        else
            return (0);
    }
    if (StringEqual (name, "dLocX"))
        return (!PyArg_Parse (value, "f", &(object->dloc[0])));
    if (StringEqual (name, "dLocY"))
        return (!PyArg_Parse (value, "f", &(object->dloc[1])));
    if (StringEqual (name, "dLocZ"))
        return (!PyArg_Parse (value, "f", &(object->dloc[2])));
    if (StringEqual (name, "dloc"))
    {
        if (Object_setDeltaLocation (obj, value) != Py_None)
            return (-1);
        else
            return (0);
    }
    if (StringEqual (name, "RotX"))
        return (!PyArg_Parse (value, "f", &(object->rot[0])));
    if (StringEqual (name, "RotY"))
        return (!PyArg_Parse (value, "f", &(object->rot[1])));
    if (StringEqual (name, "RotZ"))
        return (!PyArg_Parse (value, "f", &(object->rot[2])));
    if (StringEqual (name, "rot"))
    {
        if (Object_setEuler (obj, value) != Py_None)
            return (-1);
        else
            return (0);
    }
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
    {
        if (Object_setDrawType (obj, value) != Py_None)
            return (-1);
        else
            return (0);
    }
    if (StringEqual (name, "drawMode"))
    {
        if (Object_setDrawMode (obj, value) != Py_None)
            return (-1);
        else
            return (0);
    }

    printf ("Unknown variable.\n");
    return (0);
}

/*****************************************************************************/
/* Function:    ObjectPrint                                                  */
/* Description: This is a callback function for the C_Object type. It        */
/*              builds a meaninful string to 'print' object objects.         */
/*****************************************************************************/
static int ObjectPrint(C_Object *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Object \"%s\"]", self->object->id.name+2);
  return 0;
}

/*****************************************************************************/
/* Function:    ObjectRepr                                                   */
/* Description: This is a callback function for the C_Object type. It        */
/*              builds a meaninful string to represent object objects.       */
/*****************************************************************************/
static PyObject *ObjectRepr (C_Object *self)
{
  return PyString_FromString(self->object->id.name+2);
}
