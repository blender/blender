/*******************************************************************************
 * Copyright 2009-2025 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/
#include "PyAnimateableProperty.h"

#include "Exception.h"

#include "sequence/AnimateableProperty.h"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <memory>

#include <numpy/ndarrayobject.h>

using namespace aud;

extern PyObject* AUDError;

static PyObject* AnimateableProperty_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	AnimateablePropertyP* self = (AnimateablePropertyP*) type->tp_alloc(type, 0);

	int count;
	float value;

	if(self != nullptr)
	{
		if(!PyArg_ParseTuple(args, "i|f:animateableProperty", &count, &value))
			return nullptr;

		try
		{
			if(PyTuple_Size(args) == 1)
			{
				self->animateableProperty = new std::shared_ptr<aud::AnimateableProperty>(new aud::AnimateableProperty(count));
			}
			else
			{
				self->animateableProperty = new std::shared_ptr<aud::AnimateableProperty>(new aud::AnimateableProperty(count, value));
			}
		}
		catch(aud::Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject*) self;
}

static void AnimateableProperty_dealloc(AnimateablePropertyP* self)
{
	if(self->animateableProperty)
		delete reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty);
	Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* AnimateableProperty_read(AnimateablePropertyP* self, PyObject* args)
{
	float position;

	if(!PyArg_ParseTuple(args, "f", &position))
		return nullptr;

	int count = (*reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty))->getCount();
	npy_intp dims[1] = {count};
	PyObject* np_array = PyArray_SimpleNew(1, dims, NPY_FLOAT32);
	if(!np_array)
		return nullptr;

	float* out = static_cast<float*>(PyArray_DATA(reinterpret_cast<PyArrayObject*>(np_array)));

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty))->read(position, out);
		return np_array;
	}
	catch(aud::Exception& e)
	{
		Py_DECREF(np_array);
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_AnimateableProperty_read_doc, ".. method:: read(position)\n\n"
                                                 "   Reads the properties value at the given position.\n\n"
                                                 "   :param position: The position in the animation in frames.\n"
                                                 "   :type position: float\n"
                                                 "   :return: A numpy array of values representing the properties value.\n"
                                                 "   :rtype: :class:`numpy.ndarray`\n");

static PyObject* AnimateableProperty_readSingle(AnimateablePropertyP* self, PyObject* args)
{
	float position;

	if(!PyArg_ParseTuple(args, "f", &position))
		return nullptr;

	try
	{
		float value = (*reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty))->readSingle(position);
		return Py_BuildValue("f", value);
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_AnimateableProperty_readSingle_doc, ".. method:: readSingle(position)\n\n"
                                                       "   Reads the properties value at the given position, assuming there is exactly one value.\n\n"
                                                       "   :param position: The position in the animation in frames.\n"
                                                       "   :type position: float\n"
                                                       "   :return: The value at that position.\n"
                                                       "   :rtype: float\n\n");

static PyObject* AnimateableProperty_write(AnimateablePropertyP* self, PyObject* args)
{
	PyObject* array_obj;
	int position = -1;

	if(!PyArg_ParseTuple(args, "O|i", &array_obj, &position))
		return nullptr;

	PyArrayObject* np_array = reinterpret_cast<PyArrayObject*>(PyArray_FROM_OTF(array_obj, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST));

	if(!np_array)
	{
		PyErr_SetString(PyExc_TypeError, "data must be a numpy array of dtype float32");
		return nullptr;
	}

	auto& prop = *reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty);
	int prop_count = prop->getCount();
	npy_intp size = PyArray_SIZE(np_array);

	int ndim = PyArray_NDIM(np_array);

	bool valid_shape = false;

	// For 1D arrays, the total number of elements must be a multiple of the property count
	if(ndim == 1)
	{
		valid_shape = (size % prop_count == 0);
	}
	// For 2D arrays, the number of elements in the second dimension must be the property count
	else if(ndim == 2)
	{
		npy_intp* shape = PyArray_DIMS(np_array);
		valid_shape = (shape[1] == prop_count);
	}

	if(!valid_shape)
	{
		PyErr_SetString(PyExc_ValueError, "array shape is invalid: must be 1D with length multiple of property count or 2D with the last dimension equal to property count");
		Py_DECREF(np_array);
		return nullptr;
	}

	int count = static_cast<int>(size / prop_count);

	if(count < 1)
	{
		PyErr_SetString(PyExc_ValueError, "input array must have at least 1 element");
		Py_DECREF(np_array);
		return nullptr;
	}

	float* data_ptr = reinterpret_cast<float*>(PyArray_DATA(np_array));
	try
	{
		if(position == -1)
		{
			if(count != 1)
			{
				PyErr_SetString(PyExc_ValueError, "input array must have exactly 1 element when position is not specified");
				Py_DECREF(np_array);
				return nullptr;
			}
			prop->write(data_ptr);
		}
		else
		{
			prop->write(data_ptr, position, count);
		}
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	Py_DECREF(np_array);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(M_aud_AnimateableProperty_write_doc, ".. method:: write(data[, position])\n\n"
                                                  "   Writes the properties value.\n\n"
                                                  "   If `position` is also given, the property is marked animated and\n"
                                                  "   the values are written starting at `position`.\n\n"
                                                  "   :param data: numpy array of float32 values.\n"
                                                  "   :type data: numpy.ndarray\n"
                                                  "   :param position: The starting position in frames.\n"
                                                  "   :type position: int\n\n");

static PyObject* AnimateableProperty_writeConstantRange(AnimateablePropertyP* self, PyObject* args)
{
	PyObject* array_obj;
	int position_start;
	int position_end;

	if(!PyArg_ParseTuple(args, "Oii", &array_obj, &position_start, &position_end))
		return nullptr;

	PyArrayObject* np_array = reinterpret_cast<PyArrayObject*>(PyArray_FROM_OTF(array_obj, NPY_FLOAT32, NPY_ARRAY_IN_ARRAY | NPY_ARRAY_FORCECAST));
	if(!np_array)
	{
		PyErr_SetString(PyExc_TypeError, "data must be a numpy array of dtype float32");
		return nullptr;
	}

	int ndim = PyArray_NDIM(np_array);
	if(ndim != 1)
	{
		PyErr_SetString(PyExc_ValueError, "data must be a 1D numpy array");
		Py_DECREF(np_array);
		return nullptr;
	}

	float* data_ptr = reinterpret_cast<float*>(PyArray_DATA(np_array));

	auto& prop = *reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty);
	int prop_count = prop->getCount();

	npy_intp size = PyArray_SIZE(np_array);

	if(size != prop_count)
	{
		PyErr_Format(PyExc_ValueError, "input array length (%lld) does not match property count (%d)", size, prop_count);
		Py_DECREF(np_array);
		return nullptr;
	}

	try
	{
		prop->writeConstantRange(data_ptr, position_start, position_end);
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}

	Py_DECREF(np_array);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(M_aud_AnimateableProperty_writeConstantRange_doc, ".. method:: writeConstantRange(data, position_start, position_end)\n\n"
                                                               "   Fills the properties frame range with a constant value and marks it animated.\n\n"
                                                               "   :param data: numpy array of float values representing the constant value.\n"
                                                               "   :type data: numpy.ndarray\n"
                                                               "   :param position_start: The start position in frames.\n"
                                                               "   :type position_start: int\n"
                                                               "   :param position_end: The end position in frames.\n"
                                                               "   :type position_end: int\n\n");

static PyMethodDef AnimateableProperty_methods[] = {

    {(char*) "read", (PyCFunction) AnimateableProperty_read, METH_VARARGS, M_aud_AnimateableProperty_read_doc},
    {(char*) "readSingle", (PyCFunction) AnimateableProperty_readSingle, METH_VARARGS, M_aud_AnimateableProperty_readSingle_doc},
    {(char*) "write", (PyCFunction) AnimateableProperty_write, METH_VARARGS, M_aud_AnimateableProperty_write_doc},
    {(char*) "writeConstantRange", (PyCFunction) AnimateableProperty_writeConstantRange, METH_VARARGS, M_aud_AnimateableProperty_writeConstantRange_doc},
    {nullptr} /* Sentinel */
};

static PyObject* AnimateableProperty_get_count(AnimateablePropertyP* self, void* nothing)
{
	try
	{
		int count = (*reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty))->getCount();
		return Py_BuildValue("i", count);
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_AnimateableProperty_count_doc, "The count of floats for a property.");

static PyObject* AnimateableProperty_get_animated(AnimateablePropertyP* self, void* nothing)
{
	try
	{
		bool animated = (*reinterpret_cast<std::shared_ptr<aud::AnimateableProperty>*>(self->animateableProperty))->isAnimated();
		return PyBool_FromLong(animated);
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_AnimateableProperty_animated_doc, "Whether the property is animated.");

static PyGetSetDef AnimateableProperty_properties[] = {
    {(char*) "count", (getter) AnimateableProperty_get_count, nullptr, M_aud_AnimateableProperty_count_doc, nullptr},
    {(char*) "animated", (getter) AnimateableProperty_get_animated, nullptr, M_aud_AnimateableProperty_animated_doc, nullptr},
    {nullptr} /* Sentinel */
};

PyDoc_STRVAR(M_aud_AnimateableProperty_doc, "An AnimateableProperty object stores an array of float values for animating sound properties (e.g. pan, volume, pitch-scale)");

// Note that AnimateablePropertyType name is already taken
PyTypeObject AnimateablePropertyPyType = {
    PyVarObject_HEAD_INIT(nullptr, 0) "aud.AnimateableProperty", /* tp_name */
    sizeof(AnimateablePropertyP),                                /* tp_basicsize */
    0,                                                           /* tp_itemsize */
    (destructor) AnimateableProperty_dealloc,                    /* tp_dealloc */
    0,                                                           /* tp_print */
    0,                                                           /* tp_getattr */
    0,                                                           /* tp_setattr */
    0,                                                           /* tp_reserved */
    0,                                                           /* tp_repr */
    0,                                                           /* tp_as_number */
    0,                                                           /* tp_as_sequence */
    0,                                                           /* tp_as_mapping */
    0,                                                           /* tp_hash  */
    0,                                                           /* tp_call */
    0,                                                           /* tp_str */
    0,                                                           /* tp_getattro */
    0,                                                           /* tp_setattro */
    0,                                                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                                          /* tp_flags */
    M_aud_AnimateableProperty_doc,                               /* tp_doc */
    0,                                                           /* tp_traverse */
    0,                                                           /* tp_clear */
    0,                                                           /* tp_richcompare */
    0,                                                           /* tp_weaklistoffset */
    0,                                                           /* tp_iter */
    0,                                                           /* tp_iternext */
    AnimateableProperty_methods,                                 /* tp_methods */
    0,                                                           /* tp_members */
    AnimateableProperty_properties,                              /* tp_getset */
    0,                                                           /* tp_base */
    0,                                                           /* tp_dict */
    0,                                                           /* tp_descr_get */
    0,                                                           /* tp_descr_set */
    0,                                                           /* tp_dictoffset */
    0,                                                           /* tp_init */
    0,                                                           /* tp_alloc */
    AnimateableProperty_new,                                     /* tp_new */
};

AUD_API PyObject* AnimateableProperty_empty()
{
	return AnimateablePropertyPyType.tp_alloc(&AnimateablePropertyPyType, 0);
}

AUD_API AnimateablePropertyP* checkAnimateableProperty(PyObject* animateableProperty)
{
	if(!PyObject_TypeCheck(animateableProperty, &AnimateablePropertyPyType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type AnimateableProperty!");
		return nullptr;
	}

	return (AnimateablePropertyP*) animateableProperty;
}

bool initializeAnimateableProperty()
{
	import_array1(false);
	return PyType_Ready(&AnimateablePropertyPyType) >= 0;
}

void addAnimateablePropertyToModule(PyObject* module)
{
	Py_INCREF(&AnimateablePropertyPyType);
	PyModule_AddObject(module, "AnimateableProperty", (PyObject*) &AnimateablePropertyPyType);
}
