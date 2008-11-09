#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "../../FRS_freestyle.h"

#include "DNA_camera_types.h"

#include "render_types.h"
#include "renderpipeline.h"

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
	static AppGLWidget *view = NULL;

	char style_module[255] = "";
	int freestyle_flags;
	float freestyle_sphere_radius = 1.0;
	float freestyle_dkr_epsilon = 0.001;

	void FRS_initialize(){
		
		if( pathconfig == NULL )
			pathconfig = new Config::Path;
		
		if( controller == NULL )
			controller = new Controller;
		
		if( view == NULL ) {
			view = new AppGLWidget;
			controller->setView(view);
		}
		
		if( strlen(style_module) == 0 ){
			string path( pathconfig->getProjectDir() +  Config::DIR_SEP + "style_modules" + Config::DIR_SEP + "contour.py" );
			strcpy( style_module, path.c_str() );
		}
		
	}


	void FRS_init_view(Render* re){
		view->setWidth( re->winx );
		view->setHeight( re->winy );
		view->_camera->setScreenWidthAndHeight( re->winx, re->winy);
	}

	void FRS_init_camera(Render* re){
		Object* maincam_obj = re->scene->camera;
		Camera *cam = (Camera*) maincam_obj->data;

		if(cam->type == CAM_PERSP){
			view->_camera->setType(AppGLWidget_Camera::PERSPECTIVE);
			view->_camera->setHorizontalFieldOfView( M_PI / 180.0f * cam->angle );
		}
		// else if (cam->type == CAM_ORTHO){
		// 	view->_camera->setType(AppGLWidget_Camera::ORTHOGRAPHIC);
		// 	// view->_camera->setFocusDistance does not seem to work
		// 	// integrate cam->ortho_scale parameter
		// }
		
		Vec camPosition(maincam_obj->obmat[3][0], maincam_obj->obmat[3][1], maincam_obj->obmat[3][2]);
		Vec camUp( re->viewmat[0][1], re->viewmat[1][1], re->viewmat[2][1]);
		Vec camDirection( -re->viewmat[0][2], -re->viewmat[1][2], -re->viewmat[2][2]);
		view->_camera->setPosition(camPosition);
		view->_camera->setUpVector(camUp);	
		view->_camera->setViewDirection(camDirection);
	}

	
	void FRS_prepare(Render* re) {
		
		// init
		FRS_initialize();
		FRS_init_view(re);
		FRS_init_camera(re);
		controller->Clear();

		// load mesh
		controller->LoadMesh(re);
		
		// add style module
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
		
		// build strokes
		controller->DrawStrokes(); 
	}

	void FRS_render_GL(Render* re) {
		
		cout << "Rendering Freestyle with OpenGL" << endl;
		
		// render strokes
		view->workingBuffer = GL_BACK;
		view->draw();
		
		// display result
		RenderResult rres;
		RE_GetResultImage(re, &rres);
		view->readPixels(0, 0, re->winx, re->winy, AppGLWidget::RGBA, rres.rectf );		
		re->result->renlay = render_get_active_layer(re, re->result);
		re->display_draw(re->result, NULL);
	
		controller->CloseFile();
	}
	
#ifdef __cplusplus
}
#endif
