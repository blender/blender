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

#include "KX_BlenderRenderTools.h"

#include "GL/glew.h"

#include "RAS_IRenderTools.h"
#include "RAS_IRasterizer.h"
#include "RAS_LightObject.h"
#include "RAS_ICanvas.h"
#include "RAS_GLExtensionManager.h"

// next two includes/dependencies come from the shadow feature
// it needs the gameobject and the sumo physics scene for a raycast
#include "KX_GameObject.h"

#include "KX_BlenderPolyMaterial.h"
#include "KX_PolygonMaterial.h"
#include "KX_BlenderMaterial.h"

#include "Value.h"

#include "KX_BlenderGL.h" // for text printing
#include "STR_String.h"
#include "RAS_BucketManager.h" // for polymaterial (needed for textprinting)

#include "KX_RayCast.h"
#include "KX_IPhysicsController.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_Scene.h"

KX_BlenderRenderTools::KX_BlenderRenderTools()
{
	glGetIntegerv(GL_MAX_LIGHTS, (GLint*) &m_numgllights);
	if (m_numgllights < 8)
		m_numgllights = 8;
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
			if (layer == RAS_LIGHT_OBJECT_LAYER)
			{
				layer = static_cast<KX_GameObject*>(m_clientobject)->GetLayer();
			}
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

void KX_BlenderRenderTools::SetClientObject(void* obj)
{
	if (m_clientobject != obj)
	{
		if (obj == NULL || !((KX_GameObject*)obj)->IsNegativeScaling())
		{
			glFrontFace(GL_CCW);
		} else 
		{
			glFrontFace(GL_CW);
		}
		m_clientobject = obj;
		m_modified = true;
	}
}

bool KX_BlenderRenderTools::RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data)
{
	double* const oglmatrix = (double* const) data;
	MT_Point3 resultpoint(hit_point);
	MT_Vector3 resultnormal(hit_normal);
	MT_Vector3 left(oglmatrix[0],oglmatrix[1],oglmatrix[2]);
	MT_Vector3 dir = -(left.cross(resultnormal)).safe_normalized();
	left = (dir.cross(resultnormal)).safe_normalized();
	// for the up vector, we take the 'resultnormal' returned by the physics
	
	double maat[16]={
			left[0],        left[1],        left[2], 0,
				dir[0],         dir[1],         dir[2], 0,
		resultnormal[0],resultnormal[1],resultnormal[2], 0,
				0,              0,              0, 1};
	glTranslated(resultpoint[0],resultpoint[1],resultpoint[2]);
	//glMultMatrixd(oglmatrix);
	glMultMatrixd(maat);
	return true;
}

void KX_BlenderRenderTools::applyTransform(RAS_IRasterizer* rasty,double* oglmatrix,int objectdrawmode )
{
	/* FIXME:
	blender: intern/moto/include/MT_Vector3.inl:42: MT_Vector3 operator/(const
	MT_Vector3&, double): Assertion `!MT_fuzzyZero(s)' failed. 
	
	Program received signal SIGABRT, Aborted. 
	[Switching to Thread 16384 (LWP 1519)] 
	0x40477571 in kill () from /lib/libc.so.6 
	(gdb) bt 
	#7  0x08334368 in MT_Vector3::normalized() const () 
	#8  0x0833e6ec in KX_BlenderRenderTools::applyTransform(RAS_IRasterizer*, double*, int) () 
	*/

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
			MT_Point3 frompoint(oglmatrix[12],oglmatrix[13],oglmatrix[14]);
			KX_GameObject *gameobj = (KX_GameObject*) this->m_clientobject;
			MT_Vector3 direction = MT_Vector3(0,0,-1);

			direction.normalize();
			direction *= 100000;

			MT_Point3 topoint = frompoint + direction;

			KX_Scene* kxscene = (KX_Scene*) m_auxilaryClientInfo;
			PHY_IPhysicsEnvironment* physics_environment = kxscene->GetPhysicsEnvironment();
			KX_IPhysicsController* physics_controller = gameobj->GetPhysicsController();
			
			KX_GameObject *parent = gameobj->GetParent();
			if (!physics_controller && parent)
				physics_controller = parent->GetPhysicsController();
			if (parent)
				parent->Release();
				
			MT_Point3 resultpoint;
			MT_Vector3 resultnormal;
			if (!KX_RayCast::RayTest(physics_controller, physics_environment, frompoint, topoint, resultpoint, resultnormal, KX_RayCast::Callback<KX_BlenderRenderTools>(this, oglmatrix)))
			{
				// couldn't find something to cast the shadow on...
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
	
	const unsigned int flag = polymat->GetFlag();
	struct MTFace* tface = 0;
	unsigned int *col = 0;

	if(flag & RAS_BLENDERMAT) {
		KX_BlenderMaterial *bl_mat = static_cast<KX_BlenderMaterial*>(polymat);
		tface = bl_mat->GetMTFace();
		col = bl_mat->GetMCol();
	} else {
		KX_PolygonMaterial* blenderpoly = static_cast<KX_PolygonMaterial*>(polymat);
		tface = blenderpoly->GetMTFace();
		col = blenderpoly->GetMCol();
	}
	
	BL_RenderText( mode,mytext,mytext.Length(),tface,col,v1,v2,v3,v4);
	
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
}

	
void KX_BlenderRenderTools::EnableOpenGLLights()
{
	glEnable(GL_LIGHTING);
	
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, true);
	if (GLEW_EXT_separate_specular_color || GLEW_VERSION_1_2)
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

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
	
	unsigned int count;
	float vec[4];
		
	vec[3]= 1.0;
	
	for(count=0; count<m_numgllights; count++)
		glDisable((GLenum)(GL_LIGHT0+count));
	
	//std::vector<struct	RAS_LightObject*> m_lights;
	std::vector<struct	RAS_LightObject*>::iterator lit = m_lights.begin();
	
	glPushMatrix();
	glLoadMatrixf(m_viewmat);
	for (lit = m_lights.begin(), count = 0; !(lit==m_lights.end()) && count < m_numgllights; ++lit)
	{
		RAS_LightObject* lightdata = (*lit);
		if (lightdata->m_layer & objectlayer)
		{
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
				glLightf((GLenum)(GL_LIGHT0+count), GL_QUADRATIC_ATTENUATION, lightdata->m_att2/(lightdata->m_distance*lightdata->m_distance)); 
				
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
			
			if (lightdata->m_nodiffuse)
			{
				vec[0] = vec[1] = vec[2] = vec[3] = 0.0;
			} else {
				vec[0]= lightdata->m_energy*lightdata->m_red;
				vec[1]= lightdata->m_energy*lightdata->m_green;
				vec[2]= lightdata->m_energy*lightdata->m_blue;
				vec[3]= 1.0;
			}
			glLightfv((GLenum)(GL_LIGHT0+count), GL_DIFFUSE, vec);
			if (lightdata->m_nospecular)
			{
				vec[0] = vec[1] = vec[2] = vec[3] = 0.0;
			} else if (lightdata->m_nodiffuse) {
				vec[0]= lightdata->m_energy*lightdata->m_red;
				vec[1]= lightdata->m_energy*lightdata->m_green;
				vec[2]= lightdata->m_energy*lightdata->m_blue;
				vec[3]= 1.0;
			}
			glLightfv((GLenum)(GL_LIGHT0+count), GL_SPECULAR, vec);
			glEnable((GLenum)(GL_LIGHT0+count));

			count++;
			
		}
	}
	glPopMatrix();

	return count;

}



RAS_IPolyMaterial* KX_BlenderRenderTools::CreateBlenderPolyMaterial(
		const STR_String &texname,
		bool ba,const STR_String& matname,int tile,int tilexrep,int tileyrep,int mode,bool transparant,bool zsort, int lightlayer
		,bool bIsTriangle,void* clientobject,void* tface)
{
	assert(!"Deprecated");
/*	return new KX_BlenderPolyMaterial(

		texname,
		ba,matname,tile,tilexrep,tileyrep,mode,transparant,zsort, lightlayer
		,bIsTriangle,clientobject,(struct MTFace*)tface);*/
	return NULL;
}

void KX_BlenderRenderTools::MotionBlur(RAS_IRasterizer* rasterizer)
{
	int state = rasterizer->GetMotionBlurState();
	float motionblurvalue;
	if(state)
	{
		motionblurvalue = rasterizer->GetMotionBlurValue();
		if(state==1)
		{
			//bugfix:load color buffer into accum buffer for the first time(state=1)
			glAccum(GL_LOAD, 1.0);
			rasterizer->SetMotionBlurState(2);
		}
		else if(motionblurvalue>=0.0 && motionblurvalue<=1.0)
		{
			glAccum(GL_MULT, motionblurvalue);
			glAccum(GL_ACCUM, 1-motionblurvalue);
			glAccum(GL_RETURN, 1.0);
			glFlush();
		}
	}
}

void KX_BlenderRenderTools::Update2DFilter(RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text)
{
	m_filtermanager.EnableFilter(filtermode, pass, text);
}

void KX_BlenderRenderTools::Render2DFilters(RAS_ICanvas* canvas)
{
	m_filtermanager.RenderFilters(canvas);
}

unsigned int KX_BlenderRenderTools::m_numgllights;
