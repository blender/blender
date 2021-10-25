/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_ID_H__
#define __FREESTYLE_ID_H__

/** \file blender/freestyle/intern/system/Id.h
 *  \ingroup freestyle
 *  \brief Identification system
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/*! Class used to tag any object by an id.
 *  It is made of two unsigned integers.
 */
class Id
{
public:
	typedef unsigned id_type;

	/*! Default constructor */
	Id()
	{
		_first = 0;
		_second = 0;
	}

	/*! Builds an Id from an integer.
	 *  The second number is set to 0.
	 */
	Id(id_type id)
	{
		_first = id;
		_second = 0;
	}

	/*! Builds the Id from the two numbers */
	Id(id_type ifirst, id_type isecond)
	{
		_first = ifirst;
		_second = isecond;
	}

	/*! Copy constructor */
	Id(const Id& iBrother)
	{
		_first = iBrother._first;
		_second = iBrother._second;
	}

	/*! Operator= */
	Id& operator=(const Id& iBrother)
	{
		_first = iBrother._first;
		_second = iBrother._second;
		return *this;
	} 

	/*! Returns the first Id number */
	id_type getFirst() const
	{
		return _first;
	}

	/*! Returns the second Id number */
	id_type getSecond() const
	{
		return _second;
	}

	/*! Sets the first number constituting the Id */
	void setFirst(id_type first)
	{
		_first = first;
	}

	/*! Sets the second number constituting the Id */
	void setSecond(id_type second)
	{
		_second = second;
	}

	/*! Operator== */
	bool operator==(const Id& id) const
	{
		return ((_first == id._first) && (_second == id._second));
	}

	/*! Operator!= */
	bool operator!=(const Id& id) const
	{
		return !((*this) == id);
	}

	/*! Operator< */
	bool operator<(const Id& id) const
	{
		if (_first < id._first)
			return true;
		if (_first == id._first && _second < id._second)
			return true;
		return false;
	}

private:
	id_type _first;
	id_type _second;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Id")
#endif
};

// stream operator
inline std::ostream& operator<<(std::ostream& s, const Id& id)
{
	s << "[" << id.getFirst() << ", " << id.getSecond() << "]";
	return s;
}

} /* namespace Freestyle */

# endif // __FREESTYLE_ID_H__
