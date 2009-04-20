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

#ifndef NAN_INCLUDED_MyGlutMouseHandler_h
#define NAN_INCLUDED_MyGlutMouseHandler_h

#include "../common/GlutMouseManager.h"
#include <GL/glut.h>
#include "IK_solver.h"

class MyGlutMouseHandler : public GlutMouseHandler
{
 
public :
 
	static 
		MyGlutMouseHandler *
	New(
	) {
		MEM_SmartPtr<MyGlutMouseHandler> output = new MyGlutMouseHandler();
		if (output == NULL
		) {
			return NULL;
		}
		return output.Release();
		
	}

		void
	SetChain(
		IK_Chain_ExternPtr *chains, int num_chains
	){
		m_chains = chains;
		m_num_chains = num_chains;
	}

		void
	Mouse(
		int button,
		int state,
		int x,
		int y
	){
		if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
			m_moving = true;
			m_begin_x = x;
			m_begin_y = y;	
		}
		if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
			m_moving = false;
		}

		if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
			m_tracking = true;
		}
		if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
			m_tracking = false;
		}

		if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
			m_cg_on = true;
		}
		if (button == GLUT_MIDDLE_BUTTON && state == GLUT_UP) {
			m_cg_on = false;
		}

	}


		void
	Motion(
		int x,
		int y
	){
		if (m_moving) {
			m_angle_x = m_angle_x + (x - m_begin_x);
			m_begin_x = x;

			m_angle_y = m_angle_y + (y - m_begin_y);
			m_begin_y = y;

			glutPostRedisplay();
		}
		if (m_tracking) {

			int w_h = glutGet((GLenum)GLUT_WINDOW_HEIGHT);

			y = w_h - y;

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
			ex = m_pos.x();
			ey = m_pos.y();
			ez = m_pos.z();

			gluProject(ex, ey, ez, mvmatrix, projmatrix, viewport, &px, &py, &sz);
			gluUnProject((GLdouble) x, (GLdouble) y, sz, mvmatrix, projmatrix, viewport, &px, &py, &pz);

			m_pos = MT_Vector3(px,py,pz);

		}
		if (m_tracking || m_cg_on) {
			float temp[3];
			m_pos.getValue(temp);

			IK_SolveChain(m_chains[0],temp,0.01,200,0.1,m_chains[1]->segments);
			IK_LoadChain(m_chains[0],m_chains[0]->segments,m_chains[0]->num_segments);
	
			glutPostRedisplay();
		}			


	}

	const 
		float
	AngleX(
	) const {
		return m_angle_x;
	}

	const 
		float
	AngleY(
	) const {
		return m_angle_y;
	}

	const
		MT_Vector3	
	Position(
	) const {
		return m_pos;
	}


private :

	MyGlutMouseHandler (
	) :  
		m_angle_x(0),
		m_angle_y(0),
		m_begin_x(0),
		m_begin_y(0),
		m_moving (false),
		m_tracking (false),
		m_pos(0,0,0),
		m_cg_on (false),
		m_chains(NULL),
		m_num_chains(0)
	{
	};
		
	float m_angle_x;
	float m_angle_y;
	float m_begin_x;
	float m_begin_y;

	bool m_moving;
	bool m_tracking;
	bool m_cg_on;
	MT_Vector3 m_pos;
	
	IK_Chain_ExternPtr *m_chains;
	int m_num_chains;

};

#endif

