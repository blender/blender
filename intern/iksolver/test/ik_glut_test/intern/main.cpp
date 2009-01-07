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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_SmartPtr.h"

#ifdef USE_QUATERNIONS
#include "IK_Qsolver.h"
#else 
#include "IK_solver.h"
#endif

#include <GL/glut.h>
#include "MT_Vector3.h"
#include "MT_Quaternion.h"
#include "MT_Matrix3x3.h"
#include "MyGlutMouseHandler.h"	
#include "MyGlutKeyHandler.h"
#include "ChainDrawer.h"
 
void
init(MT_Vector3 min,MT_Vector3 max)
{
 
	GLfloat light_diffuse0[] = {1.0, 0.0, 0.0, 1.0};  /* Red diffuse light. */
	GLfloat light_position0[] = {1.0, 1.0, 1.0, 0.0};  /* Infinite light location. */

	GLfloat light_diffuse1[] = {1.0, 1.0, 1.0, 1.0};  /* Red diffuse light. */
	GLfloat light_position1[] = {1.0, 0, 0, 0.0};  /* Infinite light location. */

  /* Enable a single OpenGL light. */
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position0);

  glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
  glLightfv(GL_LIGHT1, GL_POSITION, light_position1);

  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_LIGHTING);

  /* Use depth buffering for hidden surface elimination. */
  glEnable(GL_DEPTH_TEST);

  /* Setup the view of the cube. */
  glMatrixMode(GL_PROJECTION);

	// center of the box + 3* depth of box

  MT_Vector3 center = (min + max) * 0.5;
  MT_Vector3 diag = max - min;

	float depth = diag.length();
	float distance = 2;

  gluPerspective( 
	/* field of view in degree */ 40.0,
    /* aspect ratio */ 1.0,
    /* Z near */ 1.0, 
	/* Z far */ distance * depth * 2
  );
  glMatrixMode(GL_MODELVIEW);	


  gluLookAt(
	center.x(), center.y(), center.z() + distance*depth,  /* eye is at (0,0,5) */
    center.x(), center.y(), center.z(),      /* center is at (0,0,0) */
    0.0, 1.0, 0.);      /* up is in positive Y direction */

  glPushMatrix();	

 
}
int
main(int argc, char **argv)
{


	const int seg_num = 5;
	const MT_Scalar seg_length = 15;

	const float seg_startA[3] = {0,0,0};
	const float seg_startB[3] = {0,-20,0};

	// create some segments to solve with

	// First chain
	//////////////


	IK_Segment_ExternPtr const segmentsA = new IK_Segment_Extern[seg_num]; 	
	IK_Segment_ExternPtr const segmentsB = new IK_Segment_Extern[seg_num]; 	

	IK_Segment_ExternPtr seg_it = segmentsA; 	
	IK_Segment_ExternPtr seg_itB = segmentsB; 	

	
	{

//		MT_Quaternion qmat(MT_Vector3(0,0,1),-3.141/2);
		MT_Quaternion qmat(MT_Vector3(0,0,1),0);
		MT_Matrix3x3 mat(qmat);

		seg_it->seg_start[0] = seg_startA[0];
		seg_it->seg_start[1] = seg_startA[1];
		seg_it->seg_start[2] = seg_startA[2];

		float temp[12];
		mat.getValue(temp);
 
		seg_it->basis[0] = temp[0];
		seg_it->basis[1] = temp[1];
		seg_it->basis[2] = temp[2];

		seg_it->basis[3] = temp[4];
		seg_it->basis[4] = temp[5];
		seg_it->basis[5] = temp[6];

		seg_it->basis[6] = temp[8];
		seg_it->basis[7] = temp[9];
		seg_it->basis[8] = temp[10];
 
		seg_it->length = seg_length;

		MT_Quaternion q;
		q.setEuler(0,0,0);

			
		MT_Matrix3x3 qrot(q);

		seg_it->basis_change[0] = 1;
		seg_it->basis_change[1] = 0;
		seg_it->basis_change[2] = 0;
		seg_it->basis_change[3] = 0;
		seg_it->basis_change[4] = 1;
		seg_it->basis_change[5] = 0;
		seg_it->basis_change[6] = 0;
		seg_it->basis_change[7] = 0;
		seg_it->basis_change[8] = 1;


		seg_it ++;			

		seg_itB->seg_start[0] = seg_startA[0];
		seg_itB->seg_start[1] = seg_startA[1];
		seg_itB->seg_start[2] = seg_startA[2];
 
		seg_itB->basis[0] = temp[0];
		seg_itB->basis[1] = temp[1];
		seg_itB->basis[2] = temp[2];

		seg_itB->basis[3] = temp[4];
		seg_itB->basis[4] = temp[5];
		seg_itB->basis[5] = temp[6];

		seg_itB->basis[6] = temp[8];
		seg_itB->basis[7] = temp[9];
		seg_itB->basis[8] = temp[10];
 
		seg_itB->length = seg_length;

		seg_itB->basis_change[0] = 1;
		seg_itB->basis_change[1] = 0;
		seg_itB->basis_change[2] = 0;
		seg_itB->basis_change[3] = 0;
		seg_itB->basis_change[4] = 1;
		seg_itB->basis_change[5] = 0;
		seg_itB->basis_change[6] = 0;
		seg_itB->basis_change[7] = 0;
		seg_itB->basis_change[8] = 1;


		seg_itB ++;			


	}


	int i;
	for (i=1; i < seg_num; ++i, ++seg_it,++seg_itB) {

		MT_Quaternion qmat(MT_Vector3(0,0,1),0.3);
		MT_Matrix3x3 mat(qmat);

		seg_it->seg_start[0] = 0;
		seg_it->seg_start[1] = 0;
		seg_it->seg_start[2] = 0;

		float temp[12];
		mat.getValue(temp);
 
		seg_it->basis[0] = temp[0];
		seg_it->basis[1] = temp[1];
		seg_it->basis[2] = temp[2];

		seg_it->basis[3] = temp[4];
		seg_it->basis[4] = temp[5];
		seg_it->basis[5] = temp[6];

		seg_it->basis[6] = temp[8];
		seg_it->basis[7] = temp[9];
		seg_it->basis[8] = temp[10];
 
		seg_it->length = seg_length;

		MT_Quaternion q;
		q.setEuler(0,0,0);

			
		MT_Matrix3x3 qrot(q);

		seg_it->basis_change[0] = 1;
		seg_it->basis_change[1] = 0;
		seg_it->basis_change[2] = 0;
		seg_it->basis_change[3] = 0;
		seg_it->basis_change[4] = 1;
		seg_it->basis_change[5] = 0;
		seg_it->basis_change[6] = 0;
		seg_it->basis_change[7] = 0;
		seg_it->basis_change[8] = 1;


		///////////////////////////////

		seg_itB->seg_start[0] = 0;
		seg_itB->seg_start[1] = 0;
		seg_itB->seg_start[2] = 0;
 
		seg_itB->basis[0] = temp[0];
		seg_itB->basis[1] = temp[1];
		seg_itB->basis[2] = temp[2];

		seg_itB->basis[3] = temp[4];
		seg_itB->basis[4] = temp[5];
		seg_itB->basis[5] = temp[6];

		seg_itB->basis[6] = temp[8];
		seg_itB->basis[7] = temp[9];
		seg_itB->basis[8] = temp[10];
 
		seg_itB->length = seg_length;

		seg_itB->basis_change[0] = 1;
		seg_itB->basis_change[1] = 0;
		seg_itB->basis_change[2] = 0;
		seg_itB->basis_change[3] = 0;
		seg_itB->basis_change[4] = 1;
		seg_itB->basis_change[5] = 0;
		seg_itB->basis_change[6] = 0;
		seg_itB->basis_change[7] = 0;
		seg_itB->basis_change[8] = 1;



	}

	// create the chains

	const int num_chains = 2;

	IK_Chain_ExternPtr chains[num_chains];

	chains[0] = IK_CreateChain();
	chains[1] = IK_CreateChain();

	// load segments into chain

	IK_LoadChain(chains[0],segmentsA,seg_num);
	IK_LoadChain(chains[1],segmentsB,seg_num);

	// make and install a mouse handler

	MEM_SmartPtr<MyGlutMouseHandler> mouse_handler (MyGlutMouseHandler::New());
	GlutMouseManager::Instance()->InstallHandler(mouse_handler);

	mouse_handler->SetChain(chains,num_chains);

	// make and install a keyhandler
	MEM_SmartPtr<MyGlutKeyHandler> key_handler (MyGlutKeyHandler::New());
	GlutKeyboardManager::Instance()->InstallHandler(key_handler);

	// instantiate the drawing class	

	MEM_SmartPtr<ChainDrawer> drawer (ChainDrawer::New());
	GlutDrawManager::Instance()->InstallDrawer(drawer);

	drawer->SetMouseHandler(mouse_handler);
	drawer->SetChain(chains,num_chains);
	drawer->SetKeyHandler(key_handler);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("ik");
	glutDisplayFunc(GlutDrawManager::Draw);
	glutMouseFunc(GlutMouseManager::Mouse);
	glutMotionFunc(GlutMouseManager::Motion);
	glutKeyboardFunc(GlutKeyboardManager::HandleKeyboard);

	init(MT_Vector3(-50,-50,-50),MT_Vector3(50,50,50));
	glutMainLoop();
	return 0;             /* ANSI C requires main to return int. */
}
