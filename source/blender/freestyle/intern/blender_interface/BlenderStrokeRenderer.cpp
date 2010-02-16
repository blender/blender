# include "BlenderStrokeRenderer.h"
# include "../stroke/Canvas.h"
# include "../application/AppConfig.h"

# include "BlenderTextureManager.h"

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

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_library.h" /* free_libblock */
#include "BKE_material.h"
#include "BKE_main.h" /* struct Main */
#include "BKE_object.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"

#ifdef __cplusplus
}
#endif


BlenderStrokeRenderer::BlenderStrokeRenderer(Render* re)
:StrokeRenderer(){
	
	// TEMPORARY - need a  texture manager
	_textureManager = new BlenderTextureManager;
	_textureManager->load();

	// Scene.New("FreestyleStrokes")
	old_scene = re->scene;

	objects.first = objects.last = NULL;
	
	ListBase lb;
	freestyle_scene = add_scene("freestyle_strokes");
	lb = freestyle_scene->r.layers;
	freestyle_scene->r= old_scene->r;
	freestyle_scene->r.layers= lb;
	set_scene_bg( freestyle_scene );

	// image dimensions
	float ycor = ((float)re->r.yasp) / ((float)re->r.xasp);
	float width = freestyle_scene->r.xsch;
	float height = freestyle_scene->r.ysch * ycor;

	// Camera
	Object* object_camera = add_object(freestyle_scene, OB_CAMERA);
	
	Camera* camera = (Camera *) object_camera->data;
	camera->type = CAM_ORTHO;
	camera->ortho_scale = max(width,height);
    camera->clipsta = 0.1f;
    camera->clipend = 100.0f;

    _z_delta = 0.00001f;
    _z = camera->clipsta + _z_delta;

    // test
    //_z = 999.90f; _z_delta = 0.01f;
	
	object_camera->loc[0] = 0.5 * width;
	object_camera->loc[1] = 0.5 * height;
	object_camera->loc[2] = 1.0;
	
	freestyle_scene->camera = object_camera;

	store_object(object_camera);
	
	// Material
	material = add_material("stroke_material");
	material->mode |= MA_VERTEXCOLP;
	material->mode |= MA_TRANSP;
	material->mode |= MA_SHLESS;
	material->vcol_alpha = 1;
}

BlenderStrokeRenderer::~BlenderStrokeRenderer(){
	
	  if(0 != _textureManager)
	  {
	    delete _textureManager;
	    _textureManager = 0;
	  }

	// release scene
	free_libblock( &G.main->scene, freestyle_scene );

	// release objects and data blocks
	LinkData *link = (LinkData *)objects.first;
	while(link) {
		Object *ob = (Object *)link->data;
		void *data = ob->data;
		char name[24];
		strcpy(name, ob->id.name);
		//cout << "removing " << name[0] << name[1] << ":" << (name+2) << endl;
		switch (ob->type) {
		case OB_MESH:
			free_libblock( &G.main->object, ob );
			free_libblock( &G.main->mesh, data );
			break;
		case OB_CAMERA:
			free_libblock( &G.main->object, ob );
			free_libblock( &G.main->camera, data );
			break;
		default:
			cerr << "Warning: unexpected object in the scene: " << name[0] << name[1] << ":" << (name+2) << endl;
		}
		link = link->next;
	}
	BLI_freelistN( &objects );

	// release material
	free_libblock( &G.main->mat, material );
	
	set_scene_bg( old_scene );
}

void BlenderStrokeRenderer::store_object(Object *ob) const {

	LinkData *link = (LinkData *)MEM_callocN(sizeof(LinkData), "temporary object" );
	link->data = ob;
	BLI_addhead(const_cast<ListBase *>(&objects), link);
}

float BlenderStrokeRenderer::get_stroke_vertex_z(void) const {
    float z = _z;
    BlenderStrokeRenderer *self = const_cast<BlenderStrokeRenderer *>(this);
    if (!(_z < _z_delta * 100000.0f))
        self->_z_delta *= 10.0f;
    self->_z += _z_delta;
    return -z;
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
	  unsigned int vertex_index;
	  float ycor = ((float)freestyle_scene->r.yasp) / ((float)freestyle_scene->r.xasp);
	  float width = freestyle_scene->r.xsch;
	  float height = freestyle_scene->r.ysch * ycor;
	  Vec2r p;
	
	  for(vector<Strip*>::iterator s=strips.begin(), send=strips.end();
	  s!=send;
	  ++s){		
		
	    Strip::vertex_container& strip_vertices = (*s)->vertices();
		int strip_vertex_count = (*s)->sizeStrip();
		int m, n, visible_faces, visible_segments;
		bool visible;

		// iterate over all vertices and count visible faces and strip segments
		// (note: a strip segment is a series of visible faces, while two strip
		// segments are separated by one or more invisible faces)
		v[0] = strip_vertices.begin();
	    v[1] = v[0]; ++(v[1]);
	    v[2] = v[1]; ++(v[2]);
		visible_faces = visible_segments = 0;
		visible = false;
	    for (n = 2; n < strip_vertex_count; n++)
		{
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			m = 0;
			for (int j = 0; j < 3; j++) {
				p = svRep[j]->point2d();
				if (p[0] < 0.0 || p[0] > width || p[1] < 0.0 || p[1] > height)
					m++;
			}
			if (m == 3) {
				visible = false;
			} else {
				visible_faces++;
				if (!visible)
					visible_segments++;
				visible = true;
			}
			++v[0]; ++v[1]; ++v[2];
		}
		if (visible_faces == 0)
			continue;

		// me = Mesh.New()
		Object* object_mesh = add_object(freestyle_scene, OB_MESH);
		Mesh* mesh = (Mesh *) object_mesh->data;
		MEM_freeN(mesh->bb);
		mesh->bb= NULL;
		mesh->id.us = 0;

		store_object(object_mesh);
		
#if 1
		// me.materials = [mat]
		mesh->mat = ( Material ** ) MEM_mallocN( 1 * sizeof( Material * ), "MaterialList" );
		mesh->mat[0] = material;
		mesh->totcol = 1;
		test_object_materials( (ID*) mesh );
#else
		assign_material(object_mesh, material, object_mesh->totcol+1);
		object_mesh->actcol= object_mesh->totcol;
#endif
		
		// vertices allocation
		mesh->totvert = visible_faces + visible_segments * 2;
		mesh->mvert = (MVert*) CustomData_add_layer( &mesh->vdata, CD_MVERT, CD_CALLOC, NULL, mesh->totvert);
			
		// faces allocation
		mesh->totface = visible_faces;
		mesh->mface = (MFace*) CustomData_add_layer( &mesh->fdata, CD_MFACE, CD_CALLOC, NULL, mesh->totface);
		
		// colors allocation  - me.vertexColors = True
		mesh->mcol = (MCol *) CustomData_add_layer( &mesh->fdata, CD_MCOL, CD_CALLOC, NULL, mesh->totface );

		////////////////////
		//  Data copy
		////////////////////
		
		MVert* vertices = mesh->mvert;
		MFace* faces = mesh->mface;
		MCol* colors = mesh->mcol;
		
	    v[0] = strip_vertices.begin();
	    v[1] = v[0]; ++(v[1]);
	    v[2] = v[1]; ++(v[2]);

		vertex_index = 0;
		visible = false;
		
	    for (n = 2; n < strip_vertex_count; n++)
		{
			svRep[0] = *(v[0]);
			svRep[1] = *(v[1]);
			svRep[2] = *(v[2]);
			m = 0;
			for (int j = 0; j < 3; j++) {
				p = svRep[j]->point2d();
				if (p[0] < 0.0 || p[0] > width || p[1] < 0.0 || p[1] > height)
					m++;
			}
			if (m == 3) {
				visible = false;
			} else {
				if (!visible) {
					vertex_index += 2;

					// first vertex
					vertices->co[0] = svRep[0]->point2d()[0];
					vertices->co[1] = svRep[0]->point2d()[1];
					vertices->co[2] = get_stroke_vertex_z();
					++vertices;
					
					// second vertex
					vertices->co[0] = svRep[1]->point2d()[0];
					vertices->co[1] = svRep[1]->point2d()[1];
					vertices->co[2] = get_stroke_vertex_z();
					++vertices;
				}
				visible = true;

				// vertex
				vertices->co[0] = svRep[2]->point2d()[0];
				vertices->co[1] = svRep[2]->point2d()[1];
				vertices->co[2] = get_stroke_vertex_z();
				
				// faces
				faces->v1 = vertex_index - 2;
				faces->v2 = vertex_index - 1;
				faces->v3 = vertex_index;
				faces->v4 = 0;
				
				// colors
				// red and blue are swapped - cf DNA_meshdata_types.h : MCol	
				color[0] = svRep[0]->color();
				color[1] = svRep[1]->color();
				color[2] = svRep[2]->color();

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

				++faces; ++vertices; ++colors;
				++vertex_index;
			}
			++v[0]; ++v[1]; ++v[2];

		} // loop over strip vertices 
	
	} // loop over strips	

}

Render* BlenderStrokeRenderer::RenderScene( Render *re ) {
    Camera *camera = (Camera *)freestyle_scene->camera->data;
    if (camera->clipend < _z)
        camera->clipend = _z + _z_delta * 100.0f;
    //cout << "clipsta " << camera->clipsta << ", clipend " << camera->clipend << endl;

	freestyle_scene->r.mode &= ~( R_EDGE_FRS | R_SHADOW | R_SSS | R_PANORAMA | R_ENVMAP | R_MBLUR );
	freestyle_scene->r.scemode &= ~( R_SINGLE_LAYER );
	freestyle_scene->r.planes = R_PLANES32;
	freestyle_scene->r.imtype = R_PNG;
	
	Render *freestyle_render = RE_NewRender(freestyle_scene->id.name, RE_SLOT_DEFAULT);

	RE_BlenderFrame( freestyle_render, freestyle_scene, NULL, 1);
	return freestyle_render;
}
