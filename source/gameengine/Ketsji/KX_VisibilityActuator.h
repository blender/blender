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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_VisibilityActuator.h
 *  \ingroup ketsji
 *  \brief Actuator to toggle visibility/invisibility of objects
 */

#ifndef __KX_VISIBILITYACTUATOR_H__
#define __KX_VISIBILITYACTUATOR_H__

#include "SCA_IActuator.h"

class KX_VisibilityActuator : public SCA_IActuator
{
	Py_Header

	/** Make visible? */
	bool m_visible;
	bool m_occlusion;
	bool m_recursive;

 public:
	
	KX_VisibilityActuator(
		SCA_IObject* gameobj,
		bool visible,
		bool occlusion,
		bool recursive);

	virtual
		~KX_VisibilityActuator(
			void
			);

	virtual CValue*
		GetReplica(
			void
			);

	virtual bool
		Update();

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

};

#endif

