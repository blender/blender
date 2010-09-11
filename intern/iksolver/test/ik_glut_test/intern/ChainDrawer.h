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

#ifndef NAN_INCLUDED_ChainDrawer_h
#define NAN_INCLUDED_ChainDrawer_h

#include "../common/GlutDrawer.h"
#include "MyGlutMouseHandler.h"
#include "MyGlutKeyHandler.h"
#include "MT_Transform.h"
#	include "IK_Qsolver.h"
#	include "../intern/IK_QChain.h"
#	include "../intern/IK_QSolver_Class.h"
#include <GL/glut.h>

class ChainDrawer : public GlutDrawer
{
public :
	static
		ChainDrawer *
	New(
	) {
		return new ChainDrawer();
	}
	
		void
	SetMouseHandler(
		MyGlutMouseHandler *mouse_handler
	) {
		m_mouse_handler = mouse_handler;
	}

		void
	SetKeyHandler (
		MyGlutKeyHandler *key_handler
	) {
		m_key_handler = key_handler;
	}

		void
	SetChain(
		IK_Chain_ExternPtr *chains,int chain_num
	) {
		m_chain_num = chain_num;
		m_chains = chains;
	}


	// inherited from GlutDrawer
		void
	Draw(
	) {
	  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		  glPopMatrix();
		  glPushMatrix();
		  glRotatef(m_mouse_handler->AngleX(), 0.0, 1.0, 0.0);
		  glRotatef(m_mouse_handler->AngleY(), 1.0, 0.0, 0.0);
			
	  DrawScene();
	  glutSwapBuffers();

	}

	~ChainDrawer(
	){
		// nothing to do
	};		
	
private :

		void
	DrawScene(
	){

		// draw a little cross at the position of the key handler
		// coordinates

		MT_Vector3 line_x(4,0,0);
		MT_Vector3 line_y(0.0,4,0);
		MT_Vector3 line_z(0.0,0.0,4);

		MT_Vector3 cross_origin = m_mouse_handler->Position();
		MT_Vector3 temp;
		
		glDisable(GL_LIGHTING);


		glBegin(GL_LINES);

		glColor3f (1.0f,1.0f,1.0f);

		temp = cross_origin - line_x;
		glVertex3f(temp[0],temp[1],temp[2]);
		temp = cross_origin + line_x;
		glVertex3f(temp[0],temp[1],temp[2]);

		temp = cross_origin - line_y;
		glVertex3f(temp[0],temp[1],temp[2]);
		temp = cross_origin + line_y;
		glVertex3f(temp[0],temp[1],temp[2]);

		temp = cross_origin - line_z;
		glVertex3f(temp[0],temp[1],temp[2]);
		temp = cross_origin + line_z;
		glVertex3f(temp[0],temp[1],temp[2]);

		glEnd();
		glEnable(GL_LIGHTING);


		IK_Chain_ExternPtr chain;

		int chain_num;
		for (chain_num = 0; chain_num < m_chain_num; chain_num++) {
			chain = m_chains[chain_num];


			IK_Segment_ExternPtr segs = chain->segments;
			IK_Segment_ExternPtr seg_start = segs;
			const IK_Segment_ExternPtr seg_end = segs + chain->num_segments;
			float ogl_matrix[16];

			glColor3f (0.0f,1.0f,0.0f);

			MT_Vector3 previous_origin(0,0,0);

			MT_Transform global_transform;
			global_transform.setIdentity();
			
			for (; seg_start != seg_end; ++seg_start) {
				
				glPushMatrix();

				// fill ogl_matrix with zeros

				std::fill(ogl_matrix,ogl_matrix + 16,float(0));

				// we have to do a bit of work here to compute the chain's
				// bone values
					
				// first compute all the matrices we need
				
				MT_Transform translation;
				translation.setIdentity();
				translation.translate(MT_Vector3(0,seg_start->length,0));

				MT_Matrix3x3 seg_rot(
					seg_start->basis_change[0],seg_start->basis_change[1],seg_start->basis_change[2],
					seg_start->basis_change[3],seg_start->basis_change[4],seg_start->basis_change[5],
					seg_start->basis_change[6],seg_start->basis_change[7],seg_start->basis_change[8]
				);

				seg_rot.transpose();

				MT_Matrix3x3 seg_pre_rot(
					seg_start->basis[0],seg_start->basis[1],seg_start->basis[2],
					seg_start->basis[3],seg_start->basis[4],seg_start->basis[5],
					seg_start->basis[6],seg_start->basis[7],seg_start->basis[8]
				);

		
				MT_Transform seg_t_pre_rot(
					MT_Point3(
						seg_start->seg_start[0],
						seg_start->seg_start[1],
						seg_start->seg_start[2]
					),
					seg_pre_rot
				);
				// start of the bone is just the current global transform
				// multiplied by the seg_start vector

				

				MT_Transform seg_t_rot(MT_Point3(0,0,0),seg_rot);
				MT_Transform seg_local = seg_t_pre_rot * seg_t_rot * translation;

				MT_Vector3 bone_start = global_transform * 	
					MT_Point3(
						seg_start->seg_start[0],
						seg_start->seg_start[1],
						seg_start->seg_start[2]
					);


				global_transform = global_transform * seg_local;

				global_transform.getValue(ogl_matrix);
				MT_Vector3 bone_end = global_transform.getOrigin();

				glMultMatrixf(ogl_matrix);
//				glutSolidSphere(0.5,5,5);

				glPopMatrix();
		
				glDisable(GL_LIGHTING);

				glBegin(GL_LINES);

				// draw lines of the principle axis of the local transform

				MT_Vector3 x_axis(1,0,0);
				MT_Vector3 y_axis(0,1,0);
				MT_Vector3 z_axis(0,0,1);

				x_axis = global_transform.getBasis() * x_axis * 5;
				y_axis = global_transform.getBasis() * y_axis * 5;
				z_axis = global_transform.getBasis() * z_axis * 5;


				x_axis = x_axis + bone_start;
				y_axis = y_axis + bone_start;
				z_axis = z_axis + bone_start;

				glColor3f(1,0,0);

				glVertex3f(x_axis.x(),x_axis.y(),x_axis.z());
				glVertex3f(
						bone_start.x(),
						bone_start.y(),
						bone_start.z()
				);

				glColor3f(0,1,0);

				glVertex3f(y_axis.x(),y_axis.y(),y_axis.z());
				glVertex3f(
						bone_start.x(),
						bone_start.y(),
						bone_start.z()
				);

				glColor3f(0,1,1);

				glVertex3f(z_axis.x(),z_axis.y(),z_axis.z());
				glVertex3f(
						bone_start.x(),
						bone_start.y(),
						bone_start.z()
				);

				glColor3f(0,0,1);

				glVertex3f(
						bone_start.x(),
						bone_start.y(),
						bone_start.z()
				);
				glVertex3f(bone_end[0],bone_end[1],bone_end[2]);

				glEnd();
				glEnable(GL_LIGHTING);
			}
#if 0
			// draw jacobian column vectors

			// hack access to internals

			IK_Solver_Class * internals = static_cast<IK_Solver_Class *>(chain->intern);

			glDisable(GL_LIGHTING);

			glBegin(GL_LINES);

			const TNT::Matrix<MT_Scalar> & jac = internals->Chain().TransposedJacobian();

			int i = 0;
			for (i=0; i < jac.num_rows(); i++) {
				glColor3f(1,1,1);

				previous_origin = internals->Chain().Segments()[i/3].GlobalSegmentStart();

				glVertex3f(previous_origin[0],previous_origin[1],previous_origin[2]);
				glVertex3f(jac[i][0] + previous_origin[0],jac[i][1] + previous_origin[1],jac[i][2] + previous_origin[2]);
			
				
			}
			glEnd();
			glEnable(GL_LIGHTING);
#endif
			
		}

		glColor3f(1.0,1.0,1.0);

		glDisable(GL_LIGHTING);
		glBegin(GL_LINES);

		MT_Scalar cube_size = 50;
		glVertex3f(cube_size,cube_size,cube_size);
		glVertex3f(-cube_size,cube_size,cube_size);

		glVertex3f(cube_size,-cube_size,cube_size);
		glVertex3f(-cube_size,-cube_size,cube_size);
		
		glVertex3f(cube_size,cube_size,-cube_size);
		glVertex3f(-cube_size,cube_size,-cube_size);

		glVertex3f(cube_size,-cube_size,-cube_size);
		glVertex3f(-cube_size,-cube_size,-cube_size);


		glVertex3f(-cube_size,cube_size,cube_size);
		glVertex3f(-cube_size,-cube_size,cube_size);

		glVertex3f(cube_size,cube_size,-cube_size);
		glVertex3f(cube_size,-cube_size,-cube_size);

		glVertex3f(cube_size,cube_size,cube_size);
		glVertex3f(cube_size,-cube_size,cube_size);

		glVertex3f(-cube_size,cube_size,-cube_size);
		glVertex3f(-cube_size,-cube_size,-cube_size);


		glVertex3f(cube_size,cube_size,cube_size);
		glVertex3f(cube_size,cube_size,-cube_size);

		glVertex3f(cube_size,-cube_size,cube_size);
		glVertex3f(cube_size,-cube_size,-cube_size);

		glVertex3f(-cube_size,cube_size,cube_size);
		glVertex3f(-cube_size,cube_size,-cube_size);

		glVertex3f(-cube_size,-cube_size,cube_size);
		glVertex3f(-cube_size,-cube_size,-cube_size);
		glEnd();
		glEnable(GL_LIGHTING);

	};



private :

	MyGlutMouseHandler * m_mouse_handler;
	MyGlutKeyHandler *m_key_handler;
	IK_Chain_ExternPtr *m_chains;

	int m_chain_num;
	ChainDrawer (
	) : m_chains (NULL),
		m_mouse_handler (NULL),
		m_chain_num (0)
	{
	};
	
};

#endif

