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

/** \file SCA_NANDController.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_NANDCONTROLLER_H__
#define __SCA_NANDCONTROLLER_H__

#include "SCA_IController.h"

class SCA_NANDController : public SCA_IController
{
	Py_Header
	//virtual void Trigger(class SCA_LogicManager* logicmgr);
public:
	SCA_NANDController(SCA_IObject* gameobj);
	virtual ~SCA_NANDController();
	virtual CValue* GetReplica();
	virtual void Trigger(SCA_LogicManager* logicmgr);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
};

#endif  /* __SCA_NANDCONTROLLER_H__ */
