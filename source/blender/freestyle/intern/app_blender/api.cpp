#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_camera_types.h"

#include "render_types.h"
#include "renderpipeline.h"

#include "BLI_blenlib.h"
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

	void FRS_initialize(){
		
		if( pathconfig == NULL )
			pathconfig = new Config::Path;
		
		if( controller == NULL )
			controller = new Controller;
		
		if( view == NULL )
			view = new AppGLWidget;
		
		controller->setView(view);
		controller->Clear();
		
		if( strlen(style_module) == 0 ){
			string path( pathconfig->getProjectDir() +  Config::DIR_SEP + "style_modules_blender" + Config::DIR_SEP + "contour.py" );
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
	
	void FRS_scene_3ds_export(Render* re) {
		// export scene to 3ds format
		string script_3ds_export = 	pathconfig->getProjectDir() + 
									Config::DIR_SEP + "python" + 
									Config::DIR_SEP + "3ds_export.py";
		BPY_run_python_script( const_cast<char *>(script_3ds_export.c_str()) );
		
		// load 3ds scene
		char btempdir[255];
		BLI_where_is_temp(btempdir,1);
		string exported_3ds_file =  btempdir;
		exported_3ds_file += Config::DIR_SEP + "tmp_scene_freestyle.3ds";
		if( BLI_exists( const_cast<char *>(exported_3ds_file.c_str()) ) ) {
			controller->Load3DSFile( exported_3ds_file.c_str() );
		}
		else {
			cout << "Cannot find" << exported_3ds_file << endl;
			return;
		}
	}
	
	void FRS_prepare(Render* re) {
		FRS_initialize();
		
		FRS_init_view(re);
		FRS_init_camera(re);
		
		FRS_scene_3ds_export(re);
	}

	void FRS_render(Render* re, int render_in_layer) {
		
		if(render_in_layer) {
			view->workingBuffer = GL_COLOR_ATTACHMENT1_EXT;
		} else {
			view->workingBuffer = GL_BACK;
		}
		
		// add style module
		cout << "Module: " << style_module << endl;
		controller->InsertStyleModule( 0, style_module );
		controller->toggleLayer(0, true);
		
		// compute view map
		controller->ComputeViewMap();
		
		// build strokes
		controller->DrawStrokes(); 
		
		// render final result
		view->draw(); 
	}

	void FRS_execute(Render* re, int render_in_layer) {
		
		if(render_in_layer) {

		// 	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		// 	switch(status){
		// 		case GL_FRAMEBUFFER_COMPLETE_EXT:
		// 		cout << "CORRECT: GL_FRAMEBUFFER_COMPLETE" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT" << endl;
		// 		break;
		// 		case GL_FRAMEBUFFER_UNSUPPORTED_EXT: 
		// 		cout << "ERROR: GL_FRAMEBUFFER_UNSUPPORTED" << endl;
		// 		break;
		// }
		
			RenderLayer *rl;
		
			for(rl = (RenderLayer *)re->result->layers.first; rl; rl= rl->next)
				if(rl->layflag & SCE_LAY_FRS)
					break;
			
			int p;
			for(int j = 0; j < re->winy; j++) {
				for(int i = 0; i < re->winx; i++){
					p = 4*(j*re->winx + i);
					rl->rectf[p]      *= 0.6;
					rl->rectf[p + 1]  = 0.6 * rl->rectf[p + 1];
					rl->rectf[p + 2] *= 0.6;
					rl->rectf[p + 3]  = 1.0;
				}
			}
			
		} else {
			FRS_render(re, render_in_layer);
			
			RenderResult rres;
			RE_GetResultImage(re, &rres);
			view->readPixels(0, 0, re->winx, re->winy, AppGLWidget::RGBA, rres.rectf );		
			re->result->renlay = render_get_active_layer(re, re->result);
			re->display_draw(re->result, NULL);
		}

		
		controller->CloseFile();
	}
	
#ifdef __cplusplus
}
#endif
