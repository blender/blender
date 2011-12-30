/**
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

/**

* Copyright (C) 2001 NaN Technologies B.V.
*/
#if defined(WIN32) || defined(__APPLE__)
#	ifdef WIN32
#		include <windows.h>
#		include <GL/gl.h>
#		include <GL/glu.h>
#	else // WIN32
#		include <AGL/gl.h>
#	endif // WIN32
#else // defined(WIN32) || defined(__APPLE__)
#	include <GL/gl.h>
#	include <GL/glu.h>
#endif // defined(WIN32) || defined(__APPLE__)


#include "BSP_GhostTest3D.h"
#include "BSP_MeshDrawer.h"

#include "GHOST_ISystem.h"
#include "GHOST_IWindow.h"

#include "MT_Quaternion.h"
#include "MT_Transform.h"
#include "CSG_BooleanOps.h"

#include <iostream>

	int 
EmptyInterpFunc(
	void *d1, 
	void * d2, 
	void *dnew, 
	float epsilon
){
	return 0;
}



using namespace std;


BSP_GhostTestApp3D::
BSP_GhostTestApp3D(
) :
	m_window(NULL),
	m_system(NULL),
	m_finish_me_off(false),
	m_current_object(0)
{
	//nothing to do;
}

	void
BSP_GhostTestApp3D::
SetMesh(
	MEM_SmartPtr<BSP_TMesh> mesh
){
	m_meshes.push_back(mesh);

	BSP_RotationSetting rotation_setting;
	BSP_TranslationSetting translation_setting;

	rotation_setting.m_angle_x = MT_Scalar(0);
	rotation_setting.m_angle_y = MT_Scalar(0);
	rotation_setting.m_moving = false;
	rotation_setting.x_old = 0;
	rotation_setting.y_old = 0;

	translation_setting.m_t_x = MT_Scalar(0);
	translation_setting.m_t_y = MT_Scalar(0);
	translation_setting.m_t_z = MT_Scalar(0);
	translation_setting.m_moving = false;
	translation_setting.x_old = 0;
	translation_setting.y_old = 0;

	m_rotation_settings.push_back(rotation_setting);
	m_translation_settings.push_back(translation_setting);
	m_render_modes.push_back(e_wireframe_shaded);
	m_scale_settings.push_back(MT_Scalar(1));

}

	void
BSP_GhostTestApp3D::
Swap(
	int i
){

	if (!m_rotation_settings[i].m_moving && !m_translation_settings[i].m_moving) {
		swap(m_meshes[i],m_meshes.back());
		swap(m_rotation_settings[i],m_rotation_settings.back());
		swap(m_translation_settings[i],m_translation_settings.back());
		swap(m_scale_settings[i],m_scale_settings.back());
		swap(m_render_modes[i],m_render_modes.back());
	}
}


	MT_Transform
BSP_GhostTestApp3D::
GetTransform(
	int i
){

	MT_Quaternion q_ax(MT_Vector3(0,1,0),m_rotation_settings[i].m_angle_x);
	MT_Quaternion q_ay(MT_Vector3(1,0,0),m_rotation_settings[i].m_angle_y);

	MT_Point3 tr(
		m_translation_settings[i].m_t_x,
		m_translation_settings[i].m_t_y,
		m_translation_settings[i].m_t_z
	);
	

	MT_Matrix3x3 rotx(q_ax);
	MT_Matrix3x3 roty(q_ay);

	MT_Matrix3x3 rot = rotx * roty;

	MT_Transform trans(tr,rot);

	MT_Transform scalet;
	scalet.setIdentity();
	scalet.scale(m_scale_settings[i],m_scale_settings[i],m_scale_settings[i]);

	return trans * scalet;
}

	void
BSP_GhostTestApp3D::
Operate(
	int type
){

	CSG_VertexIteratorDescriptor * vA = VertexIt_Construct(m_meshes[0],GetTransform(0));
	CSG_FaceIteratorDescriptor * fA = FaceIt_Construct(m_meshes[0]);
						
	CSG_VertexIteratorDescriptor * vB = VertexIt_Construct(m_meshes[1],GetTransform(1));
	CSG_FaceIteratorDescriptor * fB = FaceIt_Construct(m_meshes[1]);

	// describe properties.

	CSG_MeshPropertyDescriptor props;
	props.user_face_vertex_data_size = 0;
	props.user_data_size = 0;

	CSG_BooleanOperation * op =  CSG_NewBooleanFunction();
	props = CSG_DescibeOperands(op,props,props);

	CSG_PerformBooleanOperation(
		op,CSG_OperationType(type),
		*fA,*vA,*fB,*vB,EmptyInterpFunc
	);

	CSG_FaceIteratorDescriptor out_f;
	CSG_OutputFaceDescriptor(op,&out_f);

	CSG_VertexIteratorDescriptor out_v;
	CSG_OutputVertexDescriptor(op,&out_v);

	MEM_SmartPtr<BSP_TMesh> new_mesh (BuildMesh(props,out_f,out_v));

	// free stuff

	CSG_FreeVertexDescriptor(&out_v);
	CSG_FreeFaceDescriptor(&out_f);
	CSG_FreeBooleanOperation(op);

	op = NULL;
	SetMesh(new_mesh);
}


	void
BSP_GhostTestApp3D::
UpdateFrame(
){
if (m_window) {

	GHOST_Rect v_rect;
	m_window->getClientBounds(v_rect);

	glViewport(0,0,v_rect.getWidth(),v_rect.getHeight());

}
}


MT_Vector3
BSP_GhostTestApp3D::
UnProject(
	const MT_Vector3 & vec
) {

	GLint viewport[4];
	GLdouble mvmatrix[16],projmatrix[16];

	glGetIntegerv(GL_VIEWPORT,viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX,mvmatrix);
	glGetDoublev(GL_PROJECTION_MATRIX,projmatrix);
	
	GLdouble realy = viewport[3] - vec.y() - 1;
	GLdouble outx,outy,outz;

	gluUnProject(vec.x(),realy,vec.z(),mvmatrix,projmatrix,viewport,&outx,&outy,&outz);

	return MT_Vector3(outx,outy,outz);
}


	bool
BSP_GhostTestApp3D::
InitApp(
){

	// create a system and window with opengl
	// rendering context.

	GHOST_TSuccess success = GHOST_ISystem::createSystem();
	if (success == GHOST_kFailure) return false;

	m_system = GHOST_ISystem::getSystem();
	if (m_system == NULL) return false;

	m_system->addEventConsumer(this);
	
	m_window = m_system->createWindow(
		"GHOST crud3D!",
		100,100,512,512,GHOST_kWindowStateNormal,
		GHOST_kDrawingContextTypeOpenGL,false
	);

	if (
		m_window == NULL
	) {
		m_system = NULL;
		GHOST_ISystem::disposeSystem();
		return false;
	}

	// make an opengl frustum for this wind

	MT_Vector3 min,max;

	min = m_meshes[0]->m_min;
	max = m_meshes[0]->m_max;
	InitOpenGl(min,max);

	return true;
}

	void
BSP_GhostTestApp3D::
Run(
){
	if (m_system == NULL) {
		return;
	}

	while (!m_finish_me_off) {
		m_system->processEvents(true);
		m_system->dispatchEvents();
	};
}

	bool 
BSP_GhostTestApp3D::
processEvent(
	GHOST_IEvent* event
){

	bool handled = false;

	switch(event->getType()) {
		case GHOST_kEventWindowSize:
		case GHOST_kEventWindowActivate:
			UpdateFrame();
		case GHOST_kEventWindowUpdate:
			DrawPolies();
			handled = true;
			break;
		case GHOST_kEventButtonDown:
		{
			int x,y;
			m_system->getCursorPosition(x,y);


			int wx,wy;
			m_window->screenToClient(x,y,wx,wy);

			GHOST_TButtonMask button = 
				static_cast<GHOST_TEventButtonData *>(event->getData())->button;
			
			if (button == GHOST_kButtonMaskLeft) {
				m_rotation_settings[m_current_object].m_moving = true;
				m_rotation_settings[m_current_object].x_old = x;
				m_rotation_settings[m_current_object].y_old = y;	
			} else 
			if (button == GHOST_kButtonMaskRight) {
				m_translation_settings[m_current_object].m_moving = true;
				m_translation_settings[m_current_object].x_old = x;
				m_translation_settings[m_current_object].y_old = y;	
			} else 
		
			m_window->invalidate();
			handled = true;
			break;

		}

		case GHOST_kEventButtonUp:
		{

			GHOST_TButtonMask button = 
				static_cast<GHOST_TEventButtonData *>(event->getData())->button;
			
			if (button == GHOST_kButtonMaskLeft) {
				m_rotation_settings[m_current_object].m_moving = false;
				m_rotation_settings[m_current_object].x_old = 0;
				m_rotation_settings[m_current_object].y_old = 0;

		} else 
			if (button == GHOST_kButtonMaskRight) {
				m_translation_settings[m_current_object].m_moving = false;
				m_translation_settings[m_current_object].x_old;
				m_translation_settings[m_current_object].y_old;

			}
			m_window->invalidate();
			handled = true;
			break;

		}

		case GHOST_kEventCursorMove:
		{		
			int x,y;
			m_system->getCursorPosition(x,y);	
			int wx,wy;
			m_window->screenToClient(x,y,wx,wy);

			if (m_rotation_settings[m_current_object].m_moving) {
				m_rotation_settings[m_current_object].m_angle_x = MT_Scalar(wx)/20;
				m_rotation_settings[m_current_object].x_old = wx;
				m_rotation_settings[m_current_object].m_angle_y = MT_Scalar(wy)/20;
				m_rotation_settings[m_current_object].y_old = wy;

				m_window->invalidate();
			}
			if (m_translation_settings[m_current_object].m_moving) {

				// project current objects bounding box center into screen space.
				// unproject mouse point into object space using z-value from 
				// projected bounding box center.

				GHOST_Rect bounds;
				m_window->getClientBounds(bounds);

				int w_h = bounds.getHeight();

				y = w_h - wy;
				x = wx;

				double mvmatrix[16];
				double projmatrix[16];
				GLint viewport[4];

				double px, py, pz,sz;

				/* Get the matrices needed for gluUnProject */
				glGetIntegerv(GL_VIEWPORT, viewport);
				glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
				glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

				// work out the position of the end effector in screen space

				GLdouble ex,ey,ez;

				ex = m_translation_settings[m_current_object].m_t_x;
				ey = m_translation_settings[m_current_object].m_t_y;
				ez = m_translation_settings[m_current_object].m_t_z;

				gluProject(ex, ey, ez, mvmatrix, projmatrix, viewport, &px, &py, &sz);
				gluUnProject((GLdouble) x, (GLdouble) y, sz, mvmatrix, projmatrix, viewport, &px, &py, &pz);

				m_translation_settings[m_current_object].m_t_x = px;
				m_translation_settings[m_current_object].m_t_y = py;
				m_translation_settings[m_current_object].m_t_z = pz;
				m_window->invalidate();

			}		
	
			handled = true;
			break;
		}

		case GHOST_kEventKeyDown :
		{
			GHOST_TEventKeyData *kd = 
				static_cast<GHOST_TEventKeyData *>(event->getData());


			switch(kd->key) {
				case GHOST_kKeyI:
				{		
					// now intersect meshes.
					Operate(e_csg_intersection);
					handled = true;
					m_window->invalidate();
					break;					
				}
				case GHOST_kKeyU:	
				{		
					Operate(e_csg_union);
					handled = true;
					m_window->invalidate();
					break;					
				}
				case GHOST_kKeyD:	
				{		
					Operate(e_csg_difference);
					handled = true;
					m_window->invalidate();
					break;					
				}

				case GHOST_kKeyA:
				{

					m_scale_settings[m_current_object] *= 1.1;
					handled = true;
					m_window->invalidate();
					break;					
				}
				case GHOST_kKeyZ:
				{
					m_scale_settings[m_current_object] *= 0.8;

					handled = true;
					m_window->invalidate();
					break;					
				}

				case GHOST_kKeyR:
					m_render_modes[m_current_object]++;
					if (m_render_modes[m_current_object] > e_last_render_mode) {
						m_render_modes[m_current_object] = e_first_render_mode;
					}
					handled = true;
					m_window->invalidate();
					break;					

				case GHOST_kKeyB:
					handled = true;
					m_window->invalidate();
					break;					

				case GHOST_kKeyQ:
					m_finish_me_off = true;
					handled = true;
					break;

				case GHOST_kKeyS:
					Swap(m_current_object);
					m_window->invalidate();
					handled = true;
					break;

				case GHOST_kKeySpace:

					// increment the current object only if the object is not being
					// manipulated.
					if (! (m_rotation_settings[m_current_object].m_moving || m_translation_settings[m_current_object].m_moving)) {
						m_current_object ++;
						if (m_current_object >= m_meshes.size()) {
							m_current_object = 0;

						}
					}
					m_window->invalidate();
					handled = true;
					break;
				default :
					break;
			}
		}			

		default :
			break;
	}
	return handled;
};

BSP_GhostTestApp3D::
~BSP_GhostTestApp3D(
){

	if (m_window) {
		m_system->disposeWindow(m_window);
		m_window = NULL;
		GHOST_ISystem::disposeSystem();
		m_system = NULL;
	}
};

	

	void
BSP_GhostTestApp3D::
DrawPolies(
){

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	for (int i = 0; i < m_meshes.size(); ++i) {
		MT_Transform trans = GetTransform(i);

		float opengl_mat[16];
		trans.getValue(opengl_mat);

		glPushMatrix();
		glMultMatrixf(opengl_mat);
		MT_Vector3 color(1.0,1.0,1.0);
		if (i == m_current_object) {
			color = MT_Vector3(1.0,0,0);
		}
		BSP_MeshDrawer::DrawMesh(m_meshes[i].Ref(),m_render_modes[i]);

		glPopMatrix();	
	}

	m_window->swapBuffers();

}

	void
BSP_GhostTestApp3D::
InitOpenGl(
	const MT_Vector3 &min,
	const MT_Vector3 &max
){

	GLfloat light_diffuse0[] = {1.0, 0.0, 0.0, 0.5};  /* Red diffuse light. */
	GLfloat light_position0[] = {1.0, 1.0, 1.0, 0.0};  /* Infinite light location. */

	GLfloat light_diffuse1[] = {1.0, 1.0, 1.0, 0.5};  /* Red diffuse light. */
	GLfloat light_position1[] = {1.0, 0, 0, 0.0};  /* Infinite light location. */

	/* Enable a single OpenGL light. */

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position0);

	glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
	glLightfv(GL_LIGHT1, GL_POSITION, light_position1);


	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_LIGHTING);

	// make sure there is no back face culling.
	//	glDisable(GL_CULL_FACE);

	// use two sided lighting model
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);

	/* Use depth buffering for hidden surface elimination. */

	glEnable(GL_DEPTH_TEST);

	/* Setup the view of the cube. */

	glMatrixMode(GL_PROJECTION);

	// center of the box + 3* depth of box

	MT_Vector3 center = (min + max) * 0.5;
	MT_Vector3 diag = max - min;

	float depth = diag.length();
	float distance = 5;

	gluPerspective( 
	/* field of view in degree */ 40.0,
	/* aspect ratio */ 1.0,
	/* Z near */ 1.0, 
	/* Z far */ distance * depth * 2
	);
	glMatrixMode(GL_MODELVIEW);	

	gluLookAt(
		center.x(), center.y(), center.z() + distance*depth, //eye  
		center.x(), center.y(), center.z(), //center      
		0.0, 1.0, 0.
	);      /* up is in positive Y direction */

}	

			




	
	

	







	



