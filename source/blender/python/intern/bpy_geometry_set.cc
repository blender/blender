/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file contains the `bpy.types.GeometrySet` Python API which is a wrapper for the internal
 * `GeometrySet` type.
 *
 * It's not implemented as RNA type because a `GeometrySet` is standalone (i.e. is not necessarily
 * owned by anything else in Blender like an ID), is wrapping a DNA type and is itself a
 * non-trivial owner of other data (like sub-geometries).
 */

#include <sstream>

#include "BKE_duplilist.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_idtype.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_pointcloud.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "bpy_geometry_set.hh"
#include "bpy_rna.hh"

#include "../generic/py_capi_utils.hh"

using blender::bke::GeometrySet;

extern PyTypeObject bpy_geometry_set_Type;

struct BPy_GeometrySet {
  PyObject_HEAD
  GeometrySet geometry;
  PointCloud *instances_pointcloud;
};

static BPy_GeometrySet *python_object_from_geometry_set(GeometrySet geometry = {})
{
  BPy_GeometrySet *self = reinterpret_cast<BPy_GeometrySet *>(
      bpy_geometry_set_Type.tp_alloc(&bpy_geometry_set_Type, 0));
  if (self == nullptr) {
    return nullptr;
  }
  new (&self->geometry) GeometrySet(std::move(geometry));
  self->instances_pointcloud = nullptr;
  /* We can't safely give access to shared geometries via the Python API currently, because
   * constness can't be enforced. Therefore, ensure that this Python object has its own copy of
   * each data-block. Note that attributes may still be shared with other data in Blender. */
  self->geometry.ensure_no_shared_components();
  return self;
}

static PyObject *BPy_GeometrySet_new(PyTypeObject * /*type*/, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", const_cast<char **>(kwlist))) {
    return nullptr;
  }
  return reinterpret_cast<PyObject *>(python_object_from_geometry_set());
}

static void BPy_GeometrySet_dealloc(BPy_GeometrySet *self)
{
  std::destroy_at(&self->geometry);
  if (self->instances_pointcloud) {
    BKE_id_free(nullptr, self->instances_pointcloud);
  }
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject *>(self));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_from_evaluated_object_doc,
    ".. staticmethod:: from_evaluated_object(evaluated_object)\n"
    "\n"
    "   Create a geometry set from the evaluated geometry of an evaluated object.\n"
    "   Typically, it's more convenient to use :func:`bpy.types.Object.evaluated_geometry`.\n"
    "\n"
    "   :arg evaluated_object: The evaluated object to create a geometry set from.\n"
    "   :type evaluated_object: bpy.types.Object\n");
static BPy_GeometrySet *BPy_GeometrySet_static_from_evaluated_object(PyObject * /*self*/,
                                                                     PyObject *args,
                                                                     PyObject *kwds)
{
  using namespace blender;
  static const char *kwlist[] = {"evaluated_object", nullptr};
  PyObject *py_evaluated_object;
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O", const_cast<char **>(kwlist), &py_evaluated_object))
  {
    return nullptr;
  }
  ID *evaluated_object_id = nullptr;
  if (!pyrna_id_FromPyObject(py_evaluated_object, &evaluated_object_id)) {
    PyErr_Format(
        PyExc_TypeError, "Expected an Object, not %.200s", Py_TYPE(py_evaluated_object)->tp_name);
    return nullptr;
  }

  if (GS(evaluated_object_id->name) != ID_OB) {
    PyErr_Format(PyExc_TypeError,
                 "Expected an Object, not %.200s",
                 BKE_idtype_idcode_to_name(GS(evaluated_object_id->name)));
    return nullptr;
  }
  Object *evaluated_object = reinterpret_cast<Object *>(evaluated_object_id);
  if (!DEG_is_evaluated(evaluated_object)) {
    PyErr_SetString(PyExc_TypeError, "Expected an evaluated object");
    return nullptr;
  }
  const bool is_instance_collection = evaluated_object->type == OB_EMPTY &&
                                      evaluated_object->instance_collection;
  const bool valid_object_type = OB_TYPE_IS_GEOMETRY(evaluated_object->type) ||
                                 is_instance_collection;
  if (!valid_object_type) {
    const char *ob_type_name = "<unknown>";
    RNA_enum_name_from_value(rna_enum_object_type_items, evaluated_object->type, &ob_type_name);
    PyErr_Format(PyExc_TypeError, "Expected a geometry object, not %.200s", ob_type_name);
    return nullptr;
  }
  if (!DEG_object_geometry_is_evaluated(*evaluated_object)) {
    PyErr_SetString(PyExc_TypeError,
                    "Object geometry is not yet evaluated, is the depsgraph evaluated?");
    return nullptr;
  }
  Depsgraph *depsgraph = DEG_get_depsgraph_by_id(*evaluated_object_id);
  if (!depsgraph) {
    PyErr_SetString(PyExc_TypeError, "Object is not owned by a depsgraph");
    return nullptr;
  }
  Scene *scene = DEG_get_input_scene(depsgraph);

  GeometrySet geometry;
  if (is_instance_collection) {
    bke::Instances *instances = new bke::Instances();
    instances->add_new_reference(bke::InstanceReference{*evaluated_object->instance_collection});
    instances->add_instance(0, float4x4::identity());
    geometry.replace_instances(instances);
  }
  else {
    bke::Instances instances = object_duplilist_legacy_instances(
        *depsgraph, *scene, *evaluated_object);
    geometry = bke::object_get_evaluated_geometry_set(*evaluated_object, false);
    if (instances.instances_num() > 0) {
      geometry.replace_instances(new bke::Instances(std::move(instances)));
    }
  }
  BPy_GeometrySet *self = python_object_from_geometry_set(std::move(geometry));
  return self;
}

static PyObject *BPy_GeometrySet_repr(BPy_GeometrySet *self)
{
  std::stringstream ss;
  ss << self->geometry;
  std::string str = ss.str();
  return PyC_UnicodeFromStdStr(str);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_get_instances_pointcloud_doc,
    ".. method:: instances_pointcloud()\n"
    "\n"
    "   Get a pointcloud that encodes information about the instances of the geometry.\n"
    "   The returned pointcloud should not be modified.\n"
    "   There is a point per instance and per-instance data is stored in point attributes.\n"
    "   The local transforms are stored in the ``instance_transform`` attribute.\n"
    "   The data instanced by each point is referenced by the ``.reference_index`` attribute,\n"
    "   indexing into the list returned by :func:`bpy.types.GeometrySet.instance_references`.\n"
    "\n"
    "   :rtype: bpy.types.PointCloud\n");
static PyObject *BPy_GeometrySet_get_instances_pointcloud(BPy_GeometrySet *self)
{
  using namespace blender;
  const bke::Instances *instances = self->geometry.get_instances();
  if (!instances) {
    Py_RETURN_NONE;
  }
  if (self->instances_pointcloud == nullptr) {
    const int instances_num = instances->instances_num();
    PointCloud *pointcloud = BKE_pointcloud_new_nomain(instances_num);
    bke::gather_attributes(instances->attributes(),
                           bke::AttrDomain::Instance,
                           bke::AttrDomain::Point,
                           {},
                           IndexMask(instances_num),
                           pointcloud->attributes_for_write());
    self->instances_pointcloud = pointcloud;
  }
  return pyrna_id_CreatePyObject(&self->instances_pointcloud->id);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_get_instance_references_doc,
    ".. method:: instance_references()\n"
    "\n"
    "   This returns a list of geometries that is indexed by the ``.reference_index``\n"
    "   attribute of the pointcloud returned by \n"
    "   :func:`bpy.types.GeometrySet.instances_pointcloud`.\n"
    "   It may contain other geometry sets, objects, collections and None values.\n"
    "\n"
    "   :rtype: list[None | bpy.types.Object | bpy.types.Collection | bpy.types.GeometrySet]\n");
static PyObject *BPy_GeometrySet_get_instance_references(BPy_GeometrySet *self)
{
  using namespace blender;
  const bke::Instances *instances = self->geometry.get_instances();
  if (!instances) {
    return PyList_New(0);
  }
  const Span<bke::InstanceReference> references = instances->references();
  PyObject *py_references = PyList_New(references.size());
  for (const int i : references.index_range()) {
    const bke::InstanceReference &reference = references[i];
    switch (reference.type()) {
      case bke::InstanceReference::Type::None: {
        PyList_SET_ITEM(py_references, i, Py_NewRef(Py_None));
        break;
      }
      case bke::InstanceReference::Type::Object: {
        Object &object = reference.object();
        PyList_SET_ITEM(py_references, i, pyrna_id_CreatePyObject(&object.id));
        break;
      }
      case bke::InstanceReference::Type::Collection: {
        Collection &collection = reference.collection();
        PyList_SET_ITEM(py_references, i, pyrna_id_CreatePyObject(&collection.id));
        break;
      }
      case bke::InstanceReference::Type::GeometrySet: {
        const bke::GeometrySet &geometry_set = reference.geometry_set();
        PyList_SET_ITEM(py_references, i, python_object_from_geometry_set(geometry_set));
        break;
      }
    }
  }
  return py_references;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_name_doc,
    "The name of the geometry set. It can be used for debugging purposes and is not unique.\n"
    "\n"
    ":type: str\n");
static PyObject *BPy_GeometrySet_get_name(BPy_GeometrySet *self, void * /*closure*/)
{
  return PyC_UnicodeFromStdStr(self->geometry.name);
}

static int BPy_GeometrySet_set_name(BPy_GeometrySet *self, PyObject *value, void * /*closure*/)
{
  if (!PyUnicode_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "expected a string");
    return -1;
  }
  const char *name = PyUnicode_AsUTF8(value);
  self->geometry.name = name;
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_mesh_doc,
    "The mesh data-block in the geometry set.\n"
    "\n"
    ":type: :class:`bpy.types.Mesh`\n");
static PyObject *BPy_GeometrySet_get_mesh(BPy_GeometrySet *self, void * /*closure*/)
{
  Mesh *base_mesh = self->geometry.get_mesh_for_write();
  if (!base_mesh) {
    Py_RETURN_NONE;
  }
  Mesh *mesh = BKE_mesh_wrapper_ensure_subdivision(base_mesh);
  return pyrna_id_CreatePyObject(reinterpret_cast<ID *>(mesh));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_mesh_base_doc,
    "The mesh data-block in the geometry set without final subdivision.\n"
    "\n"
    ":type: :class:`bpy.types.Mesh`\n");
static PyObject *BPy_GeometrySet_get_mesh_base(BPy_GeometrySet *self, void * /*closure*/)
{
  Mesh *base_mesh = self->geometry.get_mesh_for_write();
  return pyrna_id_CreatePyObject(reinterpret_cast<ID *>(base_mesh));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_pointcloud_doc,
    "The point cloud data-block in the geometry set.\n"
    "\n"
    ":type: :class:`bpy.types.PointCloud`\n");
static PyObject *BPy_GeometrySet_get_pointcloud(BPy_GeometrySet *self, void * /*closure*/)
{
  return pyrna_id_CreatePyObject(
      reinterpret_cast<ID *>(self->geometry.get_pointcloud_for_write()));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_curves_doc,
    "The curves data-block in the geometry set.\n"
    "\n"
    ":type: :class:`bpy.types.Curves`\n");
static PyObject *BPy_GeometrySet_get_curves(BPy_GeometrySet *self, void * /*closure*/)
{
  return pyrna_id_CreatePyObject(reinterpret_cast<ID *>(self->geometry.get_curves_for_write()));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_volume_doc,
    "The volume data-block in the geometry set.\n"
    "\n"
    ":type: :class:`bpy.types.Volume`\n");
static PyObject *BPy_GeometrySet_get_volume(BPy_GeometrySet *self, void * /*closure*/)
{
  return pyrna_id_CreatePyObject(reinterpret_cast<ID *>(self->geometry.get_volume_for_write()));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_grease_pencil_doc,
    "The Grease Pencil data-block in the geometry set.\n"
    "\n"
    ":type: :class:`bpy.types.GreasePencil`\n");
static PyObject *BPy_GeometrySet_get_grease_pencil(BPy_GeometrySet *self, void * /*closure*/)
{
  return pyrna_id_CreatePyObject(
      reinterpret_cast<ID *>(self->geometry.get_grease_pencil_for_write()));
}

static PyGetSetDef BPy_GeometrySet_getseters[] = {
    {
        "name",
        reinterpret_cast<getter>(BPy_GeometrySet_get_name),
        reinterpret_cast<setter>(BPy_GeometrySet_set_name),
        bpy_geometry_set_name_doc,
        nullptr,
    },
    {
        "mesh",
        reinterpret_cast<getter>(BPy_GeometrySet_get_mesh),
        nullptr,
        bpy_geometry_set_mesh_doc,
        nullptr,
    },
    {
        "mesh_base",
        reinterpret_cast<getter>(BPy_GeometrySet_get_mesh_base),
        nullptr,
        bpy_geometry_set_mesh_base_doc,
        nullptr,
    },
    {
        "pointcloud",
        reinterpret_cast<getter>(BPy_GeometrySet_get_pointcloud),
        nullptr,
        bpy_geometry_set_pointcloud_doc,
        nullptr,
    },
    {
        "curves",
        reinterpret_cast<getter>(BPy_GeometrySet_get_curves),
        nullptr,
        bpy_geometry_set_curves_doc,
        nullptr,
    },
    {
        "volume",
        reinterpret_cast<getter>(BPy_GeometrySet_get_volume),
        nullptr,
        bpy_geometry_set_volume_doc,
        nullptr,
    },
    {
        "grease_pencil",
        reinterpret_cast<getter>(BPy_GeometrySet_get_grease_pencil),
        nullptr,
        bpy_geometry_set_grease_pencil_doc,
        nullptr,
    },
    {nullptr},

};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef BPy_GeometrySet_methods[] = {
    {"from_evaluated_object",
     reinterpret_cast<PyCFunction>(BPy_GeometrySet_static_from_evaluated_object),
     METH_VARARGS | METH_KEYWORDS | METH_STATIC,
     bpy_geometry_set_from_evaluated_object_doc},
    {"instances_pointcloud",
     reinterpret_cast<PyCFunction>(BPy_GeometrySet_get_instances_pointcloud),
     METH_NOARGS,
     bpy_geometry_set_get_instances_pointcloud_doc},
    {"instance_references",
     reinterpret_cast<PyCFunction>(BPy_GeometrySet_get_instance_references),
     METH_NOARGS,
     bpy_geometry_set_get_instance_references_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    bpy_geometry_set_doc,
    "Stores potentially multiple geometry components of different types.\n"
    "For example, it might contain a mesh, curves and nested instances.\n");
PyTypeObject bpy_geometry_set_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "GeometrySet",
    /*tp_basicsize*/ sizeof(BPy_GeometrySet),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(BPy_GeometrySet_dealloc),
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(BPy_GeometrySet_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ bpy_geometry_set_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_GeometrySet_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_GeometrySet_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ BPy_GeometrySet_new,
};

PyObject *BPyInit_geometry_set_type()
{
  if (PyType_Ready(&bpy_geometry_set_Type) < 0) {
    return nullptr;
  }
  return reinterpret_cast<PyObject *>(&bpy_geometry_set_Type);
}
