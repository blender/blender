
#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

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

	static Controller *controller = NULL;
	static AppGLWidget *view = NULL;
	
	void FRS_initialize(){
		
		if( controller == NULL )
			controller = new Controller;
		
		if( view == NULL )
			view = new AppGLWidget;
	}

	void FRS_execute(Render* re) {
		
		Config::Path pathconfig;
		FRS_initialize();
		
		controller->SetView(view);
		unsigned int width = re->winx;
		unsigned int height = re->winy;
		view->setWidth(width);
		view->setHeight(height);
		view->_camera->setScreenWidthAndHeight(width, height);
		//view->setCameraState(const float* position, const float* orientation) 
		
		string script_3ds_export = 	pathconfig.getProjectDir() + 
									Config::DIR_SEP + "python" + 
									Config::DIR_SEP + "3ds_export.py";
		BPY_run_python_script( const_cast<char *>(script_3ds_export.c_str()) );
		
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
		
		string style_module = pathconfig.getProjectDir() + 
								Config::DIR_SEP + "style_modules" + 
								Config::DIR_SEP + "contour.py";
		controller->InsertStyleModule( 0, const_cast<char *>(style_module.c_str()) 	 );
		controller->toggleLayer(0, true);
		controller->ComputeViewMap();
		
		controller->DrawStrokes(); // build strokes
		view->draw(); // render final result
		
		RenderResult rres;
		RE_GetResultImage(re, &rres);
		view->readPixels(0,0,width,height,AppGLWidget::RGBA, rres.rectf );		
		re->result->renlay = render_get_active_layer(re, re->result);
		re->display_draw(re->result, NULL);

	}
	
#ifdef __cplusplus
}
#endif
