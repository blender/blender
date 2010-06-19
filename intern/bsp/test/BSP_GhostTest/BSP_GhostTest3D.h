/**
 * $Id$
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

#ifndef BSP_GhostTest_h
#define BSP_GhostTest_h

#include "GHOST_IEventConsumer.h"
#include "MT_Vector3.h"
#include "BSP_TMesh.h"
#include "BSP_MeshDrawer.h"

#include <vector>

class GHOST_IWindow;
class GHOST_ISystem;


class BSP_GhostTestApp3D :
public GHOST_IEventConsumer
{
public :
	// Construct an instance of the application;

	BSP_GhostTestApp3D(
	);

	// initialize the applicaton

		bool
	InitApp(
	);

	// Run the application untill internal return.
		void
	Run(
	);
	
	~BSP_GhostTestApp3D(
	);

		void
	SetMesh(
		MEM_SmartPtr<BSP_TMesh> mesh
	);

private :
	
	struct BSP_RotationSetting {
		MT_Scalar m_angle_x;
		MT_Scalar m_angle_y;
		int x_old;
		int y_old;
		bool m_moving;
	};

	struct BSP_TranslationSetting {
		MT_Scalar m_t_x;
		MT_Scalar m_t_y;
		MT_Scalar m_t_z;
		int x_old;
		int y_old;
		bool m_moving;
	};

	// Return the transform of object i

		MT_Transform
	GetTransform(
		int active_object
	);

	// Perform an operation between the first two objects in the
	// list
	
		void
	Operate(
		int type
	);
	
	// Swap mesh i and settings with the last mesh in list.

		void
	Swap(
		int i
	);

		void
	DrawPolies(
	);

		void
	UpdateFrame(
	);
	
		MT_Vector3
	UnProject(
		const MT_Vector3 & vec
	);

	// Create a frustum and projection matrix to
	// look at the bounding box 

		void
	InitOpenGl(
		const MT_Vector3 &min,
		const MT_Vector3 &max
	);


	// inherited from GHOST_IEventConsumer
		bool 
	processEvent(
		GHOST_IEvent* event
	);

	GHOST_IWindow *m_window;
	GHOST_ISystem *m_system;

	bool m_finish_me_off;

	// List of current meshes.
	std::vector< MEM_SmartPtr<BSP_TMesh> > m_meshes;

	std::vector< BSP_RotationSetting> m_rotation_settings;
	std::vector< BSP_TranslationSetting> m_translation_settings;
	std::vector< MT_Scalar> m_scale_settings;
	std::vector< int> m_render_modes;

	int m_current_object;


};

#endif

