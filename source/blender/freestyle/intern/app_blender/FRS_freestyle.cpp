#include "AppView.h"
#include "Controller.h"
#include "AppConfig.h"
#include "AppCanvas.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../FRS_freestyle.h"

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

#include "render_types.h"
#include "renderpipeline.h"
#include "pixelblending.h"

#include "BLI_blenlib.h"
#include "BIF_renderwin.h"
#include "BPY_extern.h"

#ifdef __cplusplus
}
#endif

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

	static Config::Path *pathconfig = NULL;
	static Controller *controller = NULL;
	static AppView *view = NULL;

	char style_module[255] = "";
	int freestyle_flags;
	float freestyle_sphere_radius = 1.0;
	float freestyle_dkr_epsilon = 0.001;
	
	float freestyle_viewpoint[3];
	float freestyle_mv[4][4];
	float freestyle_proj[4][4];
	int freestyle_viewport[4];

	void FRS_initialize(){
		
		if( pathconfig == NULL )
			pathconfig = new Config::Path;
		
		if( controller == NULL )
			controller = new Controller;
		
		if( view == NULL ) {
			view = new AppView;
			controller->setView(view);
		}
		
		if( strlen(style_module) == 0 ){
			string path( pathconfig->getProjectDir() +  Config::DIR_SEP + "style_modules" + Config::DIR_SEP + "contour.py" );
			strcpy( style_module, path.c_str() );
		}
		
	}


	void FRS_init_view(Render* re){
		int width = re->scene->r.xsch;
		int height = re->scene->r.ysch;
		
		freestyle_viewport[0] = freestyle_viewport[1] = 0;
		freestyle_viewport[2] = width;
		freestyle_viewport[3] = height;
		
		view->setWidth( width );
		view->setHeight( height );
	}

	void FRS_init_camera(Render* re){
		Object* maincam_obj = re->scene->camera;
		Camera *cam = (Camera*) maincam_obj->data;

		view->setHorizontalFov( cam->angle );
		
		freestyle_viewpoint[0] = maincam_obj->obmat[3][0];
		freestyle_viewpoint[1] = maincam_obj->obmat[3][1];
		freestyle_viewpoint[2] = maincam_obj->obmat[3][2];
		
		freestyle_mv[0][0] = maincam_obj->obmat[0][0];
		freestyle_mv[0][1] = maincam_obj->obmat[1][0];
		freestyle_mv[0][2] = maincam_obj->obmat[2][0];
		freestyle_mv[0][3] = 0.0;

		freestyle_mv[1][0] = maincam_obj->obmat[0][1];
		freestyle_mv[1][1] = maincam_obj->obmat[1][1];
		freestyle_mv[1][2] = maincam_obj->obmat[2][1];
		freestyle_mv[1][3] = 0.0;

		freestyle_mv[2][0] = re->viewmat[2][0];
		freestyle_mv[2][1] = re->viewmat[2][1];
		freestyle_mv[2][2] = re->viewmat[2][2];
		freestyle_mv[2][3] = 0.0;

		freestyle_mv[3][0] = re->viewmat[3][0];
		freestyle_mv[3][1] = re->viewmat[3][1];
		freestyle_mv[3][2] = re->viewmat[3][2];
		freestyle_mv[3][3] = 1.0;

		for( int i = 0; i < 4; i++ )
		   for( int j = 0; j < 4; j++ )
			freestyle_proj[i][j] = re->winmat[i][j];
	}

	
	void FRS_prepare(Render* re) {
		
		// init
		FRS_initialize();
		FRS_init_view(re);
		FRS_init_camera(re);
		controller->Clear();

		// load mesh
		if( controller->LoadMesh(re) ) // returns if scene cannot be loaded or if empty
			return;
		
		// add style module
			cout << "\n===  Rendering options  ===" << endl;
		cout << "Module: " << style_module << endl;
		controller->InsertStyleModule( 0, style_module );
		controller->toggleLayer(0, true);
		
		// set parameters
		controller->setSphereRadius(freestyle_sphere_radius);
		controller->setComputeRidgesAndValleysFlag((freestyle_flags & FREESTYLE_RIDGES_AND_VALLEYS_FLAG) ? true : false);
		controller->setComputeSuggestiveContoursFlag((freestyle_flags & FREESTYLE_SUGGESTIVE_CONTOURS_FLAG) ? true : false);
		controller->setSuggestiveContourKrDerivativeEpsilon(freestyle_dkr_epsilon);

		cout << "Sphere radius : " << controller->getSphereRadius() << endl;
		cout << "Redges and valleys : " << (controller->getComputeRidgesAndValleysFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contours : " << (controller->getComputeSuggestiveContoursFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contour dkr epsilon : " << controller->getSuggestiveContourKrDerivativeEpsilon() << endl;

		// compute view map
		controller->ComputeViewMap();
	}
	
	void FRS_render_Blender(Render* re) {
		
		if( controller->_ViewMap ) {
			cout << "\n===  Rendering Freestyle with Blender's internal renderer  ===" << endl;
			
			// build strokes
			controller->DrawStrokes();

			controller->RenderBlender(re);
			controller->CloseFile();
		} else {
			cout << "Freestyle cannot be used because the view map is not available" << endl;
		}
		cout << "\n###################################################################" << endl;
	}
	
	void FRS_composite_result(Render* re, SceneRenderLayer* srl)
	{

		RenderLayer *rl;
	    float *src, *dest, *pixSrc, *pixDest;
		int x, y, rectx, recty;
		
		if( re->freestyle_render == NULL || re->freestyle_render->result == NULL )
			return;

		rl = render_get_active_layer( re->freestyle_render, re->freestyle_render->result );
	    if( !rl || rl->rectf == NULL) { cout << "Cannot find Freestyle result image" << endl; return; }
		src  = rl->rectf;
		
		rl = RE_GetRenderLayer(re->result, srl->name);
	    if( !rl || rl->rectf == NULL) { cout << "No layer to composite to" << endl; return; }
		dest  = rl->rectf;
		
		rectx = re->rectx;
		recty = re->recty;

	    for( y = 0; y < recty; y++) {
	        for( x = 0; x < rectx; x++) {

	            pixSrc = src + 4 * (rectx * y + x);
	            if( pixSrc[3] > 0.0) {
					pixDest = dest + 4 * (rectx * y + x);
					addAlphaOverFloat(pixDest, pixSrc);
	             }
	         }
	    }
		
	}
	
	void FRS_add_Freestyle(Render* re) {
		
		SceneRenderLayer *srl, *freestyle_srl = NULL;
		for(srl= (SceneRenderLayer *)re->scene->r.layers.first; srl && (freestyle_srl == NULL); srl= srl->next) {
			if(srl->layflag & SCE_LAY_FRS) {
				if (!freestyle_srl) freestyle_srl = srl;
			}
		}
		
		if( freestyle_srl ) {
			FRS_prepare(re);
			FRS_render_Blender(re);
			FRS_composite_result(re, freestyle_srl);
		}
	}
	
#ifdef __cplusplus
}
#endif
