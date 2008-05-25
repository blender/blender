
#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"

#include <iostream>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

	void FRS_execute() {
		cout << "Freestyle start" << endl;
	
		Config::Path pathconfig;
		Controller *c = new Controller;
		AppGLWidget *view = new AppGLWidget;
		
		c->SetView(view);
	
		c->Load3DSFile( "/Users/mx/Documents/work/GSoC_2008/bf-blender/branches/soc-2008-mxcurioni/source/blender/freestyle/data/models/teapot.3DS" );
		
		c->InsertStyleModule( 0,  "/Users/mx/Documents/work/GSoC_2008/bf-blender/branches/soc-2008-mxcurioni/source/blender/freestyle/style_modules/contour.py" );
		c->toggleLayer(0, true);
		c->ComputeViewMap();
		 
		//c->DrawStrokes();
		
		cout << "Freestyle end" << endl;

	}
	
#ifdef __cplusplus
}
#endif
