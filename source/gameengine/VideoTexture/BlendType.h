/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2006 The Zdeno Ash Miklas

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

#if !defined BLENDTYPE_H
#define BLENDTYPE_H


/// class allows check type of blender python object and access its contained object
/// MUST ONLY BE USED FOR KX classes that are descendent of PyObjectPlus
template <class PyObj> class BlendType
{
public:
	/// constructor
	BlendType (const char * name) : m_name(name) {}

	/// check blender type and return pointer to contained object or NULL (if type is not valid)
	PyObj * checkType (PyObject * obj)
	{
		// if pointer to type isn't set 
		if (m_objType == NULL)
		{
			// compare names of type
			if (strcmp(obj->ob_type->tp_name, m_name) == 0)
				// if name of type match, save pointer to type
				m_objType = obj->ob_type;
			else
				// if names of type don't match, return NULL
				return NULL;
		}
		// if pointer to type is set and don't match to type of provided object, return NULL
		else if (obj->ob_type != m_objType) 
			return NULL;
		// return pointer to object, this class can only be used for KX object =>
		// the Py object is actually a proxy
		return (PyObj*)BGE_PROXY_REF(obj);
	}

	/// parse arguments to get object
	PyObj * parseArg (PyObject * args)
	{
		// parse arguments
		PyObject * obj;
		if (PyArg_ParseTuple(args, "O", &obj))
			// if successfully parsed, return pointer to object
			return checkType(obj);
		// otherwise return NULL
		return NULL;
	}

protected:
	/// name of Python type
	const char * m_name;
	/// pointer to Python type
	PyTypeObject * m_objType;
};


#endif
