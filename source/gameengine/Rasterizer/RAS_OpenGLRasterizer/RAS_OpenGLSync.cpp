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

#include "GPU_glew.h"

#include <stdio.h>

#include "RAS_OpenGLSync.h"

RAS_OpenGLSync::RAS_OpenGLSync()
    :m_sync(NULL)
{
}

RAS_OpenGLSync::~RAS_OpenGLSync()
{
	Destroy();
}

bool RAS_OpenGLSync::Create(RAS_SYNC_TYPE type)
{
	if (m_sync) {
		printf("RAS_OpenGLSync::Create(): sync already exists, destroy first\n");
		return false;
	}
	if (type != RAS_SYNC_TYPE_FENCE) {
		printf("RAS_OpenGLSync::Create(): only RAS_SYNC_TYPE_FENCE are currently supported\n");
		return false;
	}
	if (!GLEW_ARB_sync) {
		printf("RAS_OpenGLSync::Create(): ARB_sync extension is needed to create sync object\n");
		return false;
	}
	m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	if (!m_sync) {
		printf("RAS_OpenGLSync::Create(): glFenceSync() failed");
		return false;
	}
	return true;
}

void RAS_OpenGLSync::Destroy()
{
	if (m_sync) {
		glDeleteSync(m_sync);
		m_sync = NULL;
	}
}

void RAS_OpenGLSync::Wait()
{
	if (m_sync) {
		// this is needed to ensure that the sync is in the GPU
		glFlush();
		// block until the operation have completed
		glWaitSync(m_sync, 0, GL_TIMEOUT_IGNORED);
	}
}
