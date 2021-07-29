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

#ifndef __FREESTYLE_AUTOPTR_HELPER_H__
#define __FREESTYLE_AUTOPTR_HELPER_H__

/** \file blender/freestyle/intern/view_map/AutoPtrHelper.h
 *  \ingroup freestyle
 *  \brief Utility header for auto_ptr/unique_ptr selection
 *  \author Sergey Sharybin
 *  \date 2015-02-09
 */

#include <memory>

namespace Freestyle {

#if __cplusplus > 199711L
template<typename T>
class AutoPtr : public std::unique_ptr<T> {
public:
	AutoPtr() : std::unique_ptr<T>() {}
	AutoPtr(T *ptr) : std::unique_ptr<T>(ptr) {}

	/* TODO(sergey): Is there more clear way to do this? */
	template<typename X>
	AutoPtr(AutoPtr<X>& other) : std::unique_ptr<T>(other.get()) {
		other.release();
	}
};
#else
template<typename T>
class AutoPtr : public std::auto_ptr<T> {
public:
	AutoPtr() : std::auto_ptr<T>() {}
	AutoPtr(T *ptr) : std::auto_ptr<T>(ptr) {}
	AutoPtr(std::auto_ptr_ref<T> ref) : std::auto_ptr<T>(ref) {}
};
#endif

}  /* namespace Freestyle */

#endif  // __FREESTYLE_AUTOPTR_HELPER_H__
