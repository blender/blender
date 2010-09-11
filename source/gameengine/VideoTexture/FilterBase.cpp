/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#include "FilterBase.h"

#include <PyObjectPlus.h>
#include <structmember.h>


// FilterBase class implementation

// constructor
FilterBase::FilterBase (void) : m_previous(NULL) {}


// destructor
FilterBase::~FilterBase (void)
{
	// release Python objects, if not released yet
	release();
}


// release python objects
void FilterBase::release (void)
{
	// release previous filter object
	setPrevious(NULL);
}


// set new previous filter
void FilterBase::setPrevious (PyFilter * filt, bool useRefCnt)
{
	// if reference counting has to be used
	if (useRefCnt)
	{
		// reference new filter
		if (filt != NULL) Py_INCREF(filt);
		// release old filter
		Py_XDECREF(m_previous);
	}
	// set new previous filter
	m_previous = filt;
}


// find first filter
FilterBase * FilterBase::findFirst (void)
{
	// find first filter in chain
	FilterBase * frst;
	for (frst = this; frst->m_previous != NULL; frst = frst->m_previous->m_filter) {};
	// set first filter
	return frst;
}



// list offilter types
PyTypeList pyFilterTypes;



// functions for python interface


// object allocation
PyObject * Filter_allocNew (PyTypeObject * type, PyObject * args, PyObject * kwds)
{
	// allocate object
	PyFilter * self = reinterpret_cast<PyFilter*>(type->tp_alloc(type, 0));
	// initialize object structure
	self->m_filter = NULL;
	// return allocated object
	return reinterpret_cast<PyObject*>(self);
}

// object deallocation
void Filter_dealloc (PyFilter * self)
{
	// release object attributes
	if (self->m_filter != NULL)
	{
		self->m_filter->release();
		delete self->m_filter;
		self->m_filter = NULL;
	}
}


// get previous pixel filter object
PyObject * Filter_getPrevious (PyFilter * self, void * closure)
{
	// if filter object is available
	if (self->m_filter != NULL)
	{
		// pixel filter object
		PyObject * filt = reinterpret_cast<PyObject*>(self->m_filter->getPrevious());
		// if filter is present
		if (filt != NULL)
		{
			// return it
			Py_INCREF(filt);
			return filt;
		}
	}
	// otherwise return none
	Py_RETURN_NONE;
}


// set previous pixel filter object
int Filter_setPrevious (PyFilter * self, PyObject * value, void * closure)
{
	// if filter object is available
	if (self->m_filter != NULL)
	{
		// check new value
		if (value == NULL || !pyFilterTypes.in(value->ob_type))
		{
			// report value error
			PyErr_SetString(PyExc_TypeError, "Invalid type of value");
			return -1;
		}
		// set new value
		self->m_filter->setPrevious(reinterpret_cast<PyFilter*>(value));
	}
	// return success
	return 0;
}
