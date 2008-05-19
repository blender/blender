#include "Controller.h"
#include <iostream>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

	void FRS_execute() {
		cout << "Freestyle" << endl;
	
		Controller *c = new Controller;
	
		//c->Load3DSFile( "/Users/mx/Documents/work/GSoC_2008/bf-blender/branches/soc-2008-mxcurioni/source/blender/freestyle/data/models/teapot.3DS" );

	}
	
#ifdef __cplusplus
}
#endif
