/**
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_PYKEYBOARD
#define __KX_PYKEYBOARD

#include "PyObjectPlus.h"

class SCA_PythonKeyboard : public PyObjectPlus
{
	Py_Header;
	class SCA_IInputDevice *m_keyboard;
public:
	SCA_PythonKeyboard(class SCA_IInputDevice* keyboard);
	virtual ~SCA_PythonKeyboard();

#ifndef DISABLE_PYTHON
	static PyObject*	pyattr_get_events(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);
#endif
};

#endif //__KX_PYKEYBOARD

