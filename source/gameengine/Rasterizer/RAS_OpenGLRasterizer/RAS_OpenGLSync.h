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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RAS_OPENGLSYNC__
#define __RAS_OPENGLSYNC__


#include "RAS_ISync.h"

struct __GLsync;

class RAS_OpenGLSync : public RAS_ISync
{
private:
	struct __GLsync *m_sync;

public:
	RAS_OpenGLSync();
	~RAS_OpenGLSync();

	virtual bool Create(RAS_SYNC_TYPE type);
	virtual void Destroy();
	virtual void Wait();
};

#endif  /* __RAS_OPENGLSYNC__ */
