/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "KX_BlenderRenderTools.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// OpenGL gl.h needs 'windows.h' on windows platforms 
#include <windows.h>
#endif //WIN32
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "RAS_IRenderTools.h"
#include "RAS_IRasterizer.h"
#include "RAS_LightObject.h"
#include "RAS_ICanvas.h"


// next two includes/dependencies come from the shadow feature
// it needs the gameobject and the sumo physics scene for a raycast
#include "KX_GameObject.h"

#include "KX_BlenderPolyMaterial.h"
#include "Value.h"

#include "KX_BlenderGL.h" // for text printing
#include "STR_String.h"
#include "RAS_BucketManager.h" // for polymaterial (needed for textprinting)

KX_BlenderRenderTools::KX_BlenderRenderTools()
{
}

/**
ProcessLighting performs lighting on objects. the layer is a bitfield that contains layer information.
There are 20 'official' layers in blender.
A light is applied on an object only when they are in the same layer.
OpenGL has a maximum of 8 lights (simultaneous), so 20 * 8 lights are possible in a scene.
*/

int	KX_BlenderRenderTools::ProcessLighting(int layer) 
{
	
	int result = false;

	if (layer < 0)
	{
		DisableOpenGLLights();
		result = false;
	} else
	{
		if (m_clientobject)
		{

			
			if (applyLights(layer))
			{
				EnableOpenGLLights();
				result = true;
			} else
			{
				DisableOpenGLLights();
				result = false;
			}

			
		}
	}
	return result;
	
	
}


void KX_BlenderRenderTools::BeginFrame(RAS_IRasterizer* rasty)
{
	m_clientobject = NULL;
	m_lastblenderobject = NULL;
	m_lastblenderlights = false;
	m_lastlayer = -1;
	m_lastlighting = false;
	m_modified = true;
	DisableOpenGLLights();


}
	

void KX_BlenderRenderTools::applyTransform(RAS_IRasterizer* rasty,double* oglmatrix,int objectdrawmode )
{
	if (objectdrawmode & RAS_IPolyMaterial::BILLBOARD_SCREENALIGNED ||
		objectdrawmode & RAS_IPolyMaterial::BILLBOARD_AXISALIGNED)
	{
		// rotate the billboard/halo
		//page 360/361 3D Game Engine Design, David Eberly for a discussion
		// on screen aligned and axis aligned billboards
		// assumed is that the preprocessor transformed all billboard polygons
		// so that their normal points into the positive x direction (1.0 , 0.0 , 0.0)
		// when new parenting for objects is done, this rotation
		// will be moved into the object
		
		MT_Point3 objpos (oglmatrix[12],oglmatrix[13],oglmatrix[14]);
		MT_Point3 campos = rasty->GetCameraPosition();
		MT_Vector3 dir = (campos - objpos).safe_normalized();
		MT_Vector3 up(0,0,1.0);

		KX_GameObject* gameobj = (KX_GameObject*) this->m_clientobject;
		// get scaling of halo object
		MT_Vector3  size = gameobj->GetSGNode()->GetLocalScale();
		
		bool screenaligned = (objectdrawmode & RAS_IPolyMaterial::BILLBOARD_SCREENALIGNED)!=0;//false; //either screen or axisaligned
		if (screenaligned)
		{
			up = (up - up.dot(dir) * dir).safe_normalized();
		} else
		{
			dir = (dir - up.dot(dir)*up).safe_normalized();
		}

		MT_Vector3 left = dir.normalized();
		dir = (left.cross(up)).normalized();

		// we have calculated the row vectors, now we keep
		// local scaling into account:

		left *= size[0];
		dir  *= size[1];
		up   *= size[2];
		double maat[16]={
			left[0], left[1],left[2], 0,
				dir[0], dir[1],dir[2],0,
				up[0],up[1],up[2],0,
				0,0,0,1};
			glTranslated(objpos[0],objpos[1],objpos[2]);
			glMultMatrixd(maat);
			
	} else
	{
		if (objectdrawmode & RAS_IPolyMaterial::SHADOW)
		{
			// shadow must be cast to the ground, physics system needed here!
			// KX_GameObject* gameobj = (KX_GameObject*) this->m_clientobject;
			MT_Point3 frompoint(oglmatrix[12],oglmatrix[13],oglmatrix[14]);
			MT_Vector3 direction = MT_Vector3(0,0,-1);

			
			direction.normalize();
			direction *= 100000;

			// MT_Point3 topoint = frompoint + direction;
			MT_Point3 resultpoint;
			MT_Vector3 resultnormal;

			//todo:
			//use physics abstraction

			
			//SM_Scene* scene = (SM_Scene*) m_auxilaryClientInfo;

			//SM_Object* hitObj = scene->rayTest(gameobj->GetSumoObject(),frompoint,topoint,
			//									 resultpoint, resultnormal);

			
			if (0) //hitObj)
			{
				MT_Vector3 left(oglmatrix[0],oglmatrix[1],oglmatrix[2]);
				MT_Vector3 dir = -(left.cross(resultnormal)).normalized();
				left = (dir.cross(resultnormal)).normalized();
				// for the up vector, we take the 'resultnormal' returned by the physics
				
				double maat[16]={
					left[0], left[1],left[2], 0,
					dir[0], dir[1],dir[2],0,
					resultnormal[0],resultnormal[1],resultnormal[2],0,
					0,0,0,1};
				glTranslated(resultpoint[0],resultpoint[1],resultpoint[2]);
				glMultMatrixd(maat);
					//	glMultMatrixd(oglmatrix);
			} else
			{
				glMultMatrixd(oglmatrix);
			}

			
		} else
		{

			// 'normal' object
			glMultMatrixd(oglmatrix);
		}
	}
}


/**
Render Text renders text into a (series of) polygon, using a texture font,
Each character consists of one polygon (one quad or two triangles)
*/
void	KX_BlenderRenderTools::RenderText(int mode,RAS_IPolyMaterial* polymat,float v1[3],float v2[3],float v3[3],float v4[3])
{
		
	STR_String mytext = ((CValue*)m_clientobject)->GetPropertyText("Text");
	
	KX_BlenderPolyMaterial* blenderpoly = (KX_BlenderPolyMaterial*)polymat;
	struct TFace* tface = blenderpoly->GetTFace();
	
	BL_RenderText( mode,mytext,mytext.Length(),tface,v1,v2,v3,v4);
	
}



KX_BlenderRenderTools::~KX_BlenderRenderTools()
{
};
	
	
void	KX_BlenderRenderTools::EndFrame(RAS_IRasterizer* rasty)
{
}
	

	
void KX_BlenderRenderTools::DisableOpenGLLights()
{
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
#ifndef SLOWPAINT
	glDisableClientState(GL_NORMAL_ARRAY);
#endif //SLOWPAINT
}

	
void KX_BlenderRenderTools::EnableOpenGLLights()
{
	glEnable(GL_LIGHTING);
	
	glColorMaterial(GL_FRONT_AND_BACK,GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
#ifndef SLOWPAINT
	glEnableClientState(GL_NORMAL_ARRAY);
#endif //SLOWPAINT
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, false);
}
	

/**
 * Rendering text using 2D bitmap functionality.  
 */
void KX_BlenderRenderTools::RenderText2D(RAS_TEXT_RENDER_MODE mode,
										 const char* text,
										 int xco,
										 int yco,									 
										 int width,
										 int height)
{
	switch (mode) {
	case RAS_IRenderTools::RAS_TEXT_PADDED: {
		STR_String tmpstr(text);
		BL_print_gamedebug_line_padded(tmpstr.Ptr(),xco,yco,width,height);
		break;
	}
	default: {
		STR_String tmpstr(text);
		BL_print_gamedebug_line(tmpstr.Ptr(),xco,yco,width,height);
	}
	}
}

	

void KX_BlenderRenderTools::PushMatrix()
{
	glPushMatrix();
}

void KX_BlenderRenderTools::PopMatrix()
{
	glPopMatrix();
}



int	KX_BlenderRenderTools::applyLights(int objectlayer)
{
// taken from blender source, incompatibility between Blender Object / GameObject	
	
	int count;
	float vec[4];
		
	vec[3]= 1.0;
	
	for(count=0; count<8; count++)
		glDisable((GLenum)(GL_LIGHT0+count));
	
	count= 0;

	//std::vector<struct	RAS_LightObject*> m_lights;
	std::vector<struct	RAS_LightObject*>::iterator lit = m_lights.begin();

	
	for (lit = m_lights.begin(); !(lit==m_lights.end()); ++lit)
	{
		RAS_LightObject* lightdata = (*lit);
		if (lightdata->m_layer & objectlayer)
		{

			glPushMatrix();
			glLoadMatrixf(m_viewmat);
			
			
			vec[0] = (*(lightdata->m_worldmatrix))(0,3);
			vec[1] = (*(lightdata->m_worldmatrix))(1,3);
			vec[2] = (*(lightdata->m_worldmatrix))(2,3);
			vec[3] = 1;


			if(lightdata->m_type==RAS_LightObject::LIGHT_SUN) {
				
				vec[0] = (*(lightdata->m_worldmatrix))(0,2);
				vec[1] = (*(lightdata->m_worldmatrix))(1,2);
				vec[2] = (*(lightdata->m_worldmatrix))(2,2);
				//vec[0]= base->object->obmat[2][0];
				//vec[1]= base->object->obmat[2][1];
				//vec[2]= base->object->obmat[2][2];
				vec[3]= 0.0;
				glLightfv((GLenum)(GL_LIGHT0+count), GL_POSITION, vec); 
			}
			else {
				//vec[3]= 1.0;
				glLightfv((GLenum)(GL_LIGHT0+count), GL_POSITION, vec); 
				glLightf((GLenum)(GL_LIGHT0+count), GL_CONSTANT_ATTENUATION, 1.0);
				glLightf((GLenum)(GL_LIGHT0+count), GL_LINEAR_ATTENUATION, lightdata->m_att1/lightdata->m_distance);
						// without this next line it looks backward compatible.
						//attennuation still is acceptable 
						// glLightf((GLenum)(GL_LIGHT0+count), GL_QUADRATIC_ATTENUATION, la->att2/(la->dist*la->dist)); 
				
				if(lightdata->m_type==RAS_LightObject::LIGHT_SPOT) {
					vec[0] = -(*(lightdata->m_worldmatrix))(0,2);
					vec[1] = -(*(lightdata->m_worldmatrix))(1,2);
					vec[2] = -(*(lightdata->m_worldmatrix))(2,2);
					//vec[0]= -base->object->obmat[2][0];
					//vec[1]= -base->object->obmat[2][1];
					//vec[2]= -base->object->obmat[2][2];
					glLightfv((GLenum)(GL_LIGHT0+count), GL_SPOT_DIRECTION, vec);
					glLightf((GLenum)(GL_LIGHT0+count), GL_SPOT_CUTOFF, lightdata->m_spotsize/2.0);
					glLightf((GLenum)(GL_LIGHT0+count), GL_SPOT_EXPONENT, 128.0*lightdata->m_spotblend);
				}
				else glLightf((GLenum)(GL_LIGHT0+count), GL_SPOT_CUTOFF, 180.0);
			}
			
			vec[0]= lightdata->m_energy*lightdata->m_red;
			vec[1]= lightdata->m_energy*lightdata->m_green;
			vec[2]= lightdata->m_energy*lightdata->m_blue;
			vec[3]= 1.0;
			glLightfv((GLenum)(GL_LIGHT0+count), GL_DIFFUSE, vec); 
			glLightfv((GLenum)(GL_LIGHT0+count), GL_SPECULAR, vec);
			glEnable((GLenum)(GL_LIGHT0+count));

			glPopMatrix();

			count++;
			if(count>7) 
				break;
		}
	}

	return count;

}



RAS_IPolyMaterial* KX_BlenderRenderTools::CreateBlenderPolyMaterial(
		const STR_String &texname,
		bool ba,const STR_String& matname,int tile,int tilexrep,int tileyrep,int mode,int transparant,int lightlayer
		,bool bIsTriangle,void* clientobject,void* tface)
{
	return new KX_BlenderPolyMaterial(

		texname,
		ba,matname,tile,tilexrep,tileyrep,mode,transparant,lightlayer
		,bIsTriangle,clientobject,(struct TFace*)tface);
}
