
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

	
	void FRS_initialize(){
		
		if( pathconfig == NULL )
			pathconfig = new Config::Path;
		
		if( controller == NULL )
			controller = new Controller;
		
		if( view == NULL )
			view = new AppGLWidget;
		
		controller->SetView(view);
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
			view->workingBuffer = GL_COLOR_ATTACHMENT0_EXT;
		} else {
			view->workingBuffer = GL_BACK;
		}
		
		// add style module
		string style_module = pathconfig->getProjectDir() + 
								Config::DIR_SEP + "style_modules" + 
								Config::DIR_SEP + "contour.py";
		controller->InsertStyleModule( 0, const_cast<char *>(style_module.c_str()) 	 );
		controller->toggleLayer(0, true);
		
		// compute view map
		controller->ComputeViewMap();
		
		// build strokes
		controller->DrawStrokes(); 
		
		// render final result
		view->draw(); 
	}

	void FRS_execute(Render* re, int render_in_layer) {
		
		GLuint framebuffer, renderbuffers[2];
		GLenum status;
		RenderLayer *rl;
		
		if(render_in_layer) {
			cout << "Freestyle as a render layer - SETUP" << endl;
		
			// set up frame buffer
			glGenFramebuffersEXT(1, &framebuffer);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);

			// set up render buffer: one color buffer, one depth buffer
			glGenRenderbuffersEXT(2, renderbuffers);
			
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,	renderbuffers[0]);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGB, re->winx, re->winy);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, renderbuffers[0]);
			
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffers[1]);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, re->winx, re->winy);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, renderbuffers[1]);

			// status verification 
			status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if (status != GL_FRAMEBUFFER_COMPLETE_EXT){
				cout << "Framebuffer setup error" << endl;
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
				glDeleteRenderbuffersEXT(2, renderbuffers);
				glDeleteFramebuffersEXT(1, &framebuffer);
				return;
			}
			
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		}
		
		FRS_render(re, render_in_layer);
		
		if(render_in_layer) {
			for(rl = (RenderLayer *)re->result->layers.first; rl; rl= rl->next) {
				if(rl->layflag & SCE_LAY_FRS) {
					cout << "Freestyle as a render layer - RESULT" << endl;
					
					// transfer render to layer
					glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
					glReadPixels(0, 0, re->winx, re->winy, GL_RGBA, GL_FLOAT, rl->rectf );

					// bind window
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
					glDeleteRenderbuffersEXT(2, renderbuffers);
					glDeleteFramebuffersEXT(1, &framebuffer);
				}
			}
		} else {
			// copy result into render window
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
