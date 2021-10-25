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

#ifndef __FREESTYLE_CAST_H__
#define __FREESTYLE_CAST_H__

/** \file blender/freestyle/intern/system/Cast.h
 *  \ingroup freestyle
 *  \brief Cast function
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

namespace Freestyle {

namespace Cast
{
	template <class T, class U>
	U *cast(T *in)
	{
		if (!in)
			return NULL;
		return dynamic_cast<U*>(in);
	}
} // end of namespace Cast

} /* namespace Freestyle */

#endif // __FREESTYLE_CAST_H__
