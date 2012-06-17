/*
-----------------------------------------------------------------------------

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.

Contributor(s): Dalai Felinto

This source uses some of the ideas and code from Paul Bourke.
Developed as part of a Research and Development project for SAT - La Société des arts technologiques.
-----------------------------------------------------------------------------
*/

/** \file KX_Dome.h
 *  \ingroup ketsji
 */

#ifndef __KX_DOME_H__
#define __KX_DOME_H__

#include "KX_Scene.h"
#include "KX_Camera.h"
#include "DNA_screen_types.h"
#include "RAS_ICanvas.h"
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"
#include "KX_KetsjiEngine.h"

#include "GL/glew.h"
#include <vector>

#include "MEM_guardedalloc.h"
#include "BKE_text.h"

//Dome modes: limit hardcoded in buttons_scene.c
#define DOME_FISHEYE			1
#define DOME_TRUNCATED_FRONT	2
#define DOME_TRUNCATED_REAR		3
#define DOME_ENVMAP				4
#define DOME_PANORAM_SPH		5
#define DOME_NUM_MODES			6


/// class for render 3d scene
class KX_Dome
{
public:
	/// constructor
	KX_Dome (RAS_ICanvas* m_canvas,
	         /// rasterizer
	         RAS_IRasterizer* m_rasterizer,
	         /// render tools
	         RAS_IRenderTools* m_rendertools,
	         /// engine
	         KX_KetsjiEngine* m_engine,

	         short res,
	         short mode,
	         short angle,
	         float resbuf,
	         short tilt,
	         struct Text* warptext
	         );

	/// destructor
	virtual ~KX_Dome (void);

	//openGL checks:
	bool	dlistSupported;
	bool	fboSupported;

	//openGL names:
	GLuint domefacesId[7];      /* ID of the images -- room for 7 images, using only 4 for 180deg x 360deg dome,
	                             * 6 for panoramic and +1 for warp mesh */
	GLuint dlistId;             /* ID of the Display Lists of the images (used as an offset) */
	
	typedef struct {
		double u[3], v[3];
		MT_Vector3 verts[3]; //three verts
	} DomeFace;

	//mesh warp functions
	typedef struct {
		double x, y, u, v, i;
	} WarpMeshNode;

	struct {
		bool usemesh;
		int mode;
		int n_width, n_height; //nodes width and height
		int imagesize;
		int bufferwidth, bufferheight;
		GLuint fboId;
		vector <vector <WarpMeshNode> > nodes;
	} warp;

	bool ParseWarpMesh(STR_String text);

	vector <DomeFace> cubetop, cubebottom, cuberight, cubeleft, cubefront, cubeback; //for fisheye
	vector <DomeFace> cubeleftback, cuberightback; //for panorama
	
	int nfacestop, nfacesbottom, nfacesleft, nfacesright, nfacesfront, nfacesback;
	int nfacesleftback, nfacesrightback;

	int GetNumberRenders() { return m_numfaces; }

	void RenderDome(void);
	void RenderDomeFrame(KX_Scene* scene, KX_Camera* cam, int i);
	void BindImages(int i);

	void SetViewPort(GLuint viewport[4]);
	void CalculateFrustum(KX_Camera* cam);
	void RotateCamera(KX_Camera* cam, int i);

	//Mesh creation Functions
	void CreateMeshDome180(void);
	void CreateMeshDome250(void);
	void CreateMeshPanorama(void);

	void SplitFace(vector <DomeFace>& face, int *nfaces);

	void FlattenDome(MT_Vector3 verts[3]);
	void FlattenPanorama(MT_Vector3 verts[3]);

	//Draw functions
	void GLDrawTriangles(vector <DomeFace>& face, int nfaces);
	void GLDrawWarpQuads(void);
	void Draw(void);
	void DrawDomeFisheye(void);
	void DrawEnvMap(void);
	void DrawPanorama(void);
	void DrawDomeWarped(void);

	//setting up openGL
	void CreateGLImages(void);
	void ClearGLImages(void);//called on resize
	bool CreateDL(void); //create Display Lists
	void ClearDL(void);  //remove Display Lists 
	bool CreateFBO(void);//create FBO (for warp mesh)
	void ClearFBO(void); //remove FBO

	void CalculateCameraOrientation();
	void CalculateImageSize(); //set m_imagesize

	int canvaswidth;
	int canvasheight;

protected:
	int m_drawingmode;

	int m_imagesize;
	int m_buffersize;	// canvas small dimension
	int m_numfaces;		// 4 to 6 depending on the kind of dome image
	int m_numimages;	//numfaces +1 if we have warp mesh

	short m_resolution;	//resolution to tessellate the mesh
	short m_mode;		// the mode (truncated, warped, panoramic,...)
	short m_angle;		//the angle of the fisheye
	float m_radangle;	//the angle of the fisheye in radians
	float m_resbuffer;	//the resolution of the buffer
	short m_tilt;		//the dome tilt (camera rotation on horizontal axis)
	
	RAS_Rect m_viewport;

	MT_Matrix4x4 m_projmat;

	MT_Matrix3x3 m_locRot[6]; // the rotation matrix

	/// rendered scene
	KX_Scene * m_scene;

	/// canvas
	RAS_ICanvas* m_canvas;
	/// rasterizer
	RAS_IRasterizer* m_rasterizer;
	/// render tools
	RAS_IRenderTools* m_rendertools;
	/// engine
	KX_KetsjiEngine* m_engine;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_Dome"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif /* __KX_DOME_H__ */
