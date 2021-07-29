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

#ifndef __FREESTYLE_MODULE_H__
#define __FREESTYLE_MODULE_H__

/** \file blender/freestyle/intern/stroke/Module.h
 *  \ingroup freestyle
 *  \brief Set the type of the module
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include "Canvas.h"
#include "StyleModule.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Module
{
public:
	static void setAlwaysRefresh(bool b = true)
	{
		getCurrentStyleModule()->setAlwaysRefresh(b);
	}

	static void setCausal(bool b = true)
	{
		getCurrentStyleModule()->setCausal(b);
	}

	static void setDrawable(bool b = true)
	{
		getCurrentStyleModule()->setDrawable(b);
	}

	static bool getAlwaysRefresh()
	{
		return getCurrentStyleModule()->getAlwaysRefresh();
	}

	static bool getCausal()
	{
		return getCurrentStyleModule()->getCausal();
	}

	static bool getDrawable()
	{
		return getCurrentStyleModule()->getDrawable();
	}

private:
	static StyleModule *getCurrentStyleModule()
	{
		Canvas *canvas = Canvas::getInstance();
		return canvas->getCurrentStyleModule();
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Module")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_MODULE_H__
