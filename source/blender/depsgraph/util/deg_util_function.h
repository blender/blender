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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Lukas Toenne
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/util/deg_util_function.h
 *  \ingroup depsgraph
 */

#pragma once

#if (__cplusplus > 199711L)

#include <functional>

using std::function;
using namespace std::placeholders;
#define function_bind std::bind

#elif defined(HAVE_BOOST_FUNCTION_BINDINGS)

#include <boost/bind.hpp>
#include <boost/function.hpp>

using boost::function;
#define function_bind boost::bind

#else

#pragma message("No available function binding implementation. Using stub instead, disabling new depsgraph")

#ifndef WITH_LEGACY_DEPSGRAPH
#  error "Unable to build new depsgraph and legacy one is disabled."
#endif

#define DISABLE_NEW_DEPSGRAPH

#include "BLI_utildefines.h"
#include <cstdlib>

template<typename T>
class function {
public:
	function() {};
	function(void *) {}
	operator bool() const { return false; }
	bool operator== (void *) { return false; }

	template<typename T1>
	void operator() (T1) {
		BLI_assert(!"Should not be used");
	}
};

class Wrap {
public:
	Wrap() {}
	template <typename T>
	Wrap(T /*arg*/) {}
};

template <typename T>
void *function_bind(T func,
                    Wrap arg1 = Wrap(),
                    Wrap arg2 = Wrap(),
                    Wrap arg3 = Wrap(),
                    Wrap arg4 = Wrap(),
                    Wrap arg5 = Wrap(),
                    Wrap arg6 = Wrap(),
                    Wrap arg7 = Wrap())
{
	BLI_assert(!"Should not be used");
	(void)func;
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	(void)arg6;
	(void)arg7;
	return NULL;
}

#define _1 Wrap()
#define _2 Wrap()
#define _3 Wrap()
#define _4 Wrap()

#endif
