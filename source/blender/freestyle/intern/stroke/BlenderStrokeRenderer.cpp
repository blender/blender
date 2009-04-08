# include "BlenderStrokeRenderer.h"
# include "Canvas.h"
# include "../app_blender/AppConfig.h"

# include "../rendering/GLStrokeRenderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BIF_drawscene.h"
#include "BIF_renderwin.h"
#include "BIF_writeimage.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_library.h" /* free_libblock */
#include "BKE_material.h"
#include "BKE_main.h" /* struct Main */
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BSE_sequence.h"	/* to clear_scene_in_allseqs */
#include "BSE_node.h"	/* to clear_scene_in_nodes */
#include "BSE_edit.h"	/* countall */

#include "RE_pipeline.h"

#ifdef __cplusplus
}
#endif




BlenderStrokeRenderer::BlenderStrokeRenderer()
:StrokeRenderer(){
	
	// TEMPORARY - need a  texture manager
	_textureManager = new GLTextureManager;
	_textureManager->load();

	// Scene.New("FreestyleStrokes")
	old_scene = G.scene;
	
	ListBase lb;
	scene = add_scene("freestyle_strokes_scene  ");
	lb = scene->r.layers;
	scene->r= old_scene->r;
	scene->r.layers= lb;
	set_scene_bg( scene );

	// image dimensions
	float width = scene->r.xsch;
	float height = scene->r.ysch;

	// Camera
	object_camera = add_object(OB_CAMERA);
	
	Camera* camera = (Camera *) object_camera->data;
	camera->type = CAM_ORTHO;
	camera->ortho_scale = max(width,height);
	
	object_camera->loc[0] = 0.5 * width;
	object_camera->loc[1] = 0.5 * height;
	object_camera->loc[2] = 1.0;
	
	scene->camera = object_camera;
	
	// Material
	material = add_material("stroke_material");
	material->mode |= MA_VERTEXCOLP;
	material->mode |= MA_SHLESS;
}

BlenderStrokeRenderer::~BlenderStrokeRenderer(){
	
	  if(0 != _textureManager)
	  {
	    delete _textureManager;
	    _textureManager = 0;
	  }
	
	free_object( object_camera );
	free_material( material );
	free_libblock( &G.main->scene, scene );
	
	set_scene_bg( old_scene );
}

void BlenderStrokeRenderer::RenderStrokeRep(StrokeRep *iStrokeRep) const{
  RenderStrokeRepBasic(iStrokeRep);
}

void BlenderStrokeRenderer::RenderStrokeRepBasic(StrokeRep *iStrokeRep) const{
	
	////////////////////
	//  Build up scene
	////////////////////
	
	  vector<Strip*>& strips = iStrokeRep->getStrips();
	  Strip::vertex_container::iterator v[3];
	  StrokeVertexRep *svRep[3];
	  Vec3r color[3];
	  unsigned int face_index;
	
	  for(vector<Strip*>::iterator s=strips.begin(), send=strips.end();
	  s!=send;
	  ++s){		
		
		// me = Mesh.New()
		Object* object_mesh = add_object(OB_MESH);
		Mesh* mesh = (Mesh *) object_mesh->data;
		MEM_freeN(mesh->bb);
		mesh->bb= NULL;
		mesh->id.us = 0;
		
		// me.materials = [mat]
		mesh->mat = ( Material ** ) MEM_mallocN( 1 * sizeof( Material * ), "MaterialList" );
		mesh->mat[0] = material;
		mesh->totcol = 1;
		test_object_materials( (ID*) mesh );
		
		int strip_vertex_count = (*s)->sizeStrip();
	
		// vertices allocation
		mesh->totvert = strip_vertex_count;
		mesh->mvert = (MVert*) CustomData_add_layer( &mesh->vdata, CD_MVERT, CD_CALLOC, NULL, mesh->totvert);
			
		// faces allocation
		mesh->totface = strip_vertex_count - 2;
		mesh->mface = (MFace*) CustomData_add_layer( &mesh->fdata, CD_MFACE, CD_CALLOC, NULL, mesh->totface);
		
		// colors allocation  - me.vertexColors = True
		mesh->mcol = (MCol *) CustomData_add_layer( &mesh->fdata, CD_MCOL, CD_CALLOC, NULL, mesh->totface );
		
		////////////////////
		//  Data copy
		////////////////////
		
		MVert* vertices = mesh->mvert;
		MFace* faces = mesh->mface;
		MCol* colors = mesh->mcol;
		
	    Strip::vertex_container& strip_vertices = (*s)->vertices();
	    v[0] = strip_vertices.begin();
	    v[1] = v[0]; ++(v[1]);
	    v[2] = v[1]; ++(v[2]);

		// first vertex
		svRep[0] = *(v[0]);
		vertices->co[0] = svRep[0]->point2d()[0];
		vertices->co[1] = svRep[0]->point2d()[1];
		vertices->co[2] = 0.0;
		++vertices;
		
		// second vertex
		svRep[1] = *(v[1]);
		vertices->co[0] = svRep[1]->point2d()[0];
		vertices->co[1] = svRep[1]->point2d()[1];
		vertices->co[2] = 0.0;
		++vertices;
		
		// iterating over subsequent vertices: each vertex adds a new face
		face_index = 0;
		
	    while( v[2] != strip_vertices.end() ) 
		{
			// INPUT
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			
			color[0] = svRep[0]->color();
			color[1] = svRep[1]->color();
			color[2] = svRep[2]->color();
			
			// vertex
			vertices->co[0] = svRep[2]->point2d()[0];
			vertices->co[1] = svRep[2]->point2d()[1];
			vertices->co[2] = 0.0;
			
			// faces
			faces->v1 = face_index;
			faces->v2 = face_index + 1;
			faces->v3 = face_index + 2;
			faces->v4 = 0;
			
			// colors
			// red and blue are swapped - cf DNA_meshdata_types.h : MCol	
			colors->r = (short)(255.0f*(color[0])[2]);
			colors->g = (short)(255.0f*(color[0])[1]);
			colors->b = (short)(255.0f*(color[0])[0]);
			colors->a = (short)(255.0f*svRep[0]->alpha());
			++colors;
			
			colors->r = (short)(255.0f*(color[1])[2]);
			colors->g = (short)(255.0f*(color[1])[1]);
			colors->b = (short)(255.0f*(color[1])[0]);
			colors->a = (short)(255.0f*svRep[1]->alpha());
			++colors;
			
			colors->r = (short)(255.0f*(color[2])[2]);
			colors->g = (short)(255.0f*(color[2])[1]);
			colors->b = (short)(255.0f*(color[2])[0]);
			colors->a = (short)(255.0f*svRep[2]->alpha());
			++colors;
			
			// ITERATION
			++v[0]; ++v[1]; ++v[2];
			++faces; ++vertices; ++colors;
			++face_index;
		
		} // loop over strip vertices 
	
	} // loop over strips	

}

Render* BlenderStrokeRenderer::RenderScene( Render *re ) {
	scene->r.mode &= ~( R_EDGE_FRS | R_SHADOW | R_SSS | R_PANORAMA | R_ENVMAP | R_MBLUR );
	scene->r.scemode &= ~( R_SINGLE_LAYER );
	scene->r.planes = R_PLANES32;
	scene->r.imtype = R_PNG;
	
	Render* freestyle_render = RE_NewRender(scene->id.name);
	
	RE_BlenderFrame( freestyle_render, scene, 1);
	return freestyle_render;
}
