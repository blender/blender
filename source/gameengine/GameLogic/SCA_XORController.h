/**
 * SCA_XORController.h
 *
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef __KX_XORCONTROLLER
#define __KX_XORCONTROLLER

#include "SCA_IController.h"

class SCA_XORController : public SCA_IController
{
	Py_Header;
	//virtual void Trigger(class SCA_LogicManager* logicmgr);
public:
	SCA_XORController(SCA_IObject* gameobj,PyTypeObject* T=&Type);
	virtual ~SCA_XORController();
	virtual CValue* GetReplica();
	virtual void Trigger(SCA_LogicManager* logicmgr);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);

};

#endif //__KX_XORCONTROLLER

