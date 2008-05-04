/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Blender's Ketsji startpoint
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
// don't show stl-warnings
#pragma warning (disable:4786)
#endif

#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#endif 

#include "KX_BlenderGL.h"
#include "KX_BlenderCanvas.h"
#include "KX_BlenderKeyboardDevice.h"
#include "KX_BlenderMouseDevice.h"
#include "KX_BlenderRenderTools.h"
#include "KX_BlenderSystem.h"
#include "BL_Material.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"

#include "RAS_OpenGLRasterizer.h"
#include "RAS_VAOpenGLRasterizer.h"
#include "RAS_ListRasterizer.h"
#include "RAS_GLExtensionManager.h"

#include "NG_LoopBackNetworkDeviceInterface.h"
#include "SND_DeviceManager.h"

#include "SYS_System.h"

	/***/

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "BKE_global.h"
#include "BIF_screen.h"
#include "BIF_scrarea.h"

#include "BKE_main.h"	
#include "BLI_blenlib.h"
#include "BLO_readfile.h"
#include "DNA_scene_types.h"
	/***/

#ifdef __cplusplus
extern "C" {
#endif
#include "BSE_headerbuttons.h"
void update_for_newframe();
#ifdef __cplusplus
}
#endif

static BlendFileData *load_game_data(char *filename) {
	BlendReadError error;
	//this doesn't work anymore for relative paths, so use BLO_read_from_memory instead
	//BlendFileData *bfd= BLO_read_from_file(filename, &error);
	FILE* file = fopen(filename,"rb");
	BlendFileData *bfd  = 0;
	if (file)
	{
		fseek(file, 0L, SEEK_END);
		int len= ftell(file);
		fseek(file, 0L, SEEK_SET);	
		char* filebuffer= new char[len];//MEM_mallocN(len, "text_buffer");
		int sizeread = fread(filebuffer,len,1,file);
		if (sizeread==1){
			bfd = BLO_read_from_memory(filebuffer, len, &error);
		} else {
			error = BRE_UNABLE_TO_READ;
		}
		fclose(file);
		// the memory is not released in BLO_read_from_memory, must do it here
		delete filebuffer;
	} else {
		error = BRE_UNABLE_TO_OPEN;
	}

	if (!bfd) {
		printf("Loading %s failed: %s\n", filename, BLO_bre_as_string(error));
	}
	
	return bfd;
}

extern "C" void StartKetsjiShell(struct ScrArea *area,
								 char* scenename,
								 struct Main* maggie1,
								 struct SpaceIpo *sipo,
								 int always_use_expand_framing)
{
	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	
	Main* blenderdata = maggie1;

	char* startscenename = scenename;
	char pathname[160];
	strcpy (pathname, blenderdata->name);
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();

	bgl::InitExtensions(1);
	
	do
	{
		View3D *v3d= (View3D*) area->spacedata.first;
		
		// get some preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool usefixed = (SYS_GetCommandLineInt(syshandle, "fixedtime", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool game2ipo = (SYS_GetCommandLineInt(syshandle, "game2ipo", 0) != 0);
		bool displaylists = (SYS_GetCommandLineInt(syshandle, "displaylists", 0) != 0);
		bool usemat = false;
		
		#if defined(GL_ARB_multitexture) && defined(WITH_GLEXT)
		if (!getenv("WITHOUT_GLEXT")) {
			if(bgl::RAS_EXT_support._ARB_multitexture && bgl::QueryVersion(1, 1)) {
				usemat = (SYS_GetCommandLineInt(syshandle, "blender_material", 0) != 0);
				int unitmax=0;
				glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&unitmax);
				bgl::max_texture_units = MAXTEX>unitmax?unitmax:MAXTEX;
				//std::cout << "using(" << bgl::max_texture_units << ") of(" << unitmax << ") texture units." << std::endl;
			} else {
				bgl::max_texture_units = 0;
			}
		}
		#endif


		// create the canvas, rasterizer and rendertools
		RAS_ICanvas* canvas = new KX_BlenderCanvas(area);
		canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
		RAS_IRenderTools* rendertools = new KX_BlenderRenderTools();
		RAS_IRasterizer* rasterizer = NULL;
		
		// let's see if we want to use vertexarrays or not
		int usevta = SYS_GetCommandLineInt(syshandle,"vertexarrays",1);
		bool useVertexArrays = (usevta > 0);
		
		bool lock_arrays = (displaylists && useVertexArrays);

		if(displaylists){
			if (useVertexArrays) {
				rasterizer = new RAS_ListRasterizer(canvas, true, lock_arrays);
			} else {
				rasterizer = new RAS_ListRasterizer(canvas);
			}
		} else if (useVertexArrays && bgl::QueryVersion(1, 1))
			rasterizer = new RAS_VAOpenGLRasterizer(canvas, lock_arrays);
		else
			rasterizer = new RAS_OpenGLRasterizer(canvas);
		
		// create the inputdevices
		KX_BlenderKeyboardDevice* keyboarddevice = new KX_BlenderKeyboardDevice();
		KX_BlenderMouseDevice* mousedevice = new KX_BlenderMouseDevice();
		
		// create a networkdevice
		NG_NetworkDeviceInterface* networkdevice = new
			NG_LoopBackNetworkDeviceInterface();
		
		// get an audiodevice
		SND_DeviceManager::Subscribe();
		SND_IAudioDevice* audiodevice = SND_DeviceManager::Instance();
		audiodevice->UseCD();
		
		// create a ketsji/blendersystem (only needed for timing and stuff)
		KX_BlenderSystem* kxsystem = new KX_BlenderSystem();
		
		// create the ketsjiengine
		KX_KetsjiEngine* ketsjiengine = new KX_KetsjiEngine(kxsystem);
		
		// set the devices
		ketsjiengine->SetKeyboardDevice(keyboarddevice);
		ketsjiengine->SetMouseDevice(mousedevice);
		ketsjiengine->SetNetworkDevice(networkdevice);
		ketsjiengine->SetCanvas(canvas);
		ketsjiengine->SetRenderTools(rendertools);
		ketsjiengine->SetRasterizer(rasterizer);
		ketsjiengine->SetNetworkDevice(networkdevice);
		ketsjiengine->SetAudioDevice(audiodevice);
		ketsjiengine->SetUseFixedTime(usefixed);
		ketsjiengine->SetTimingDisplay(frameRate, profile, properties);

		
	
		// some blender stuff
		MT_CmMatrix4x4 projmat;
		MT_CmMatrix4x4 viewmat;
		int i;
		
		for (i = 0; i < 16; i++)
		{
			float *viewmat_linear= (float*) v3d->viewmat;
			viewmat.setElem(i, viewmat_linear[i]);
		}
		for (i = 0; i < 16; i++)
		{
			float *projmat_linear = (float*) area->winmat;
			projmat.setElem(i, projmat_linear[i]);
		}
		
		float camzoom = (1.41421 + (v3d->camzoom / 50.0));
		camzoom *= camzoom;
		camzoom = 4.0 / camzoom;
		
		ketsjiengine->SetDrawType(v3d->drawtype);
		ketsjiengine->SetCameraZoom(camzoom);
		
		// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
		if (exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME || exitrequested == KX_EXIT_REQUEST_RESTART_GAME)
		{
			exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
			if (bfd) BLO_blendfiledata_free(bfd);
			
			char basedpath[240];
			// base the actuator filename with respect
			// to the original file working directory
			if (exitstring != "")
				strcpy(basedpath, exitstring.Ptr());

			BLI_convertstringcode(basedpath, pathname);
			bfd = load_game_data(basedpath);
			
			// if it wasn't loaded, try it forced relative
			if (!bfd)
			{
				// just add "//" in front of it
				char temppath[242];
				strcpy(temppath, "//");
				strcat(temppath, basedpath);
				
				BLI_convertstringcode(temppath, pathname);
				bfd = load_game_data(temppath);
			}
			
			// if we got a loaded blendfile, proceed
			if (bfd)
			{
				blenderdata = bfd->main;
				startscenename = bfd->curscene->id.name + 2;
			}
			// else forget it, we can't find it
			else
			{
				exitrequested = KX_EXIT_REQUEST_QUIT_GAME;
			}
		}
		
		Scene *blscene = NULL;
		if (!bfd)
		{
			blscene = (Scene*) blenderdata->scene.first;
			for (Scene *sce= (Scene*) blenderdata->scene.first; sce; sce= (Scene*) sce->id.next)
			{
				if (startscenename == (sce->id.name+2))
				{
					blscene = sce;
					break;
				}
			}
		} else {
			blscene = bfd->curscene;
		}

		if (blscene)
		{
			int startFrame = blscene->r.cfra;
			ketsjiengine->SetGame2IpoMode(game2ipo,startFrame);
		}


		// Quad buffered needs a special window.
		if (blscene->r.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
			rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) blscene->r.stereomode);
		
		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			if (v3d->persp != V3D_CAMOB)
			{
				ketsjiengine->EnableCameraOverride(startscenename);
				ketsjiengine->SetCameraOverrideUseOrtho((v3d->persp == V3D_ORTHO));
				ketsjiengine->SetCameraOverrideProjectionMatrix(projmat);
				ketsjiengine->SetCameraOverrideViewMatrix(viewmat);
			}
			
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(blenderdata,sipo, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			sceneconverter->addInitFromFrame=false;
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);
			
			if(usemat)
				sceneconverter->SetMaterials(true);
					
			KX_Scene* startscene = new KX_Scene(keyboarddevice,
				mousedevice,
				networkdevice,
				audiodevice,
				startscenename);
			
			// some python things
			PyObject* dictionaryobject = initGamePythonScripting("Ketsji", psl_Lowest);
			ketsjiengine->SetPythonDictionary(dictionaryobject);
			initRasterizer(rasterizer, canvas);
			PyObject *gameLogic = initGameLogic(startscene);
			initGameKeys();
			initPythonConstraintBinding();


			if (sceneconverter)
			{
				// convert and add scene
				sceneconverter->ConvertScene(
					startscenename,
					startscene,
					dictionaryobject,
					keyboarddevice,
					rendertools,
					canvas);
				ketsjiengine->AddScene(startscene);
				
				// init the rasterizer
				rasterizer->Init();
				
				// start the engine
				ketsjiengine->StartEngine(true);
				
				// the mainloop
				while (!exitrequested)
				{
					// first check if we want to exit
					exitrequested = ketsjiengine->GetExitCode();
					
					// kick the engine
					bool render = ketsjiengine->NextFrame();
					
					if (render)
					{
						// render the frame
						ketsjiengine->Render();
					}
					
					// test for the ESC key
					while (qtest())
					{
						short val; 
						unsigned short event = extern_qread(&val);
						
						if (keyboarddevice->ConvertBlenderEvent(event,val))
							exitrequested = KX_EXIT_REQUEST_BLENDER_ESC;
						
							/* Coordinate conversion... where
							* should this really be?
						*/
						if (event==MOUSEX) {
							val = val - scrarea_get_win_x(area);
						} else if (event==MOUSEY) {
							val = scrarea_get_win_height(area) - (val - scrarea_get_win_y(area)) - 1;
						}
						
						mousedevice->ConvertBlenderEvent(event,val);
					}
				}
				exitstring = ketsjiengine->GetExitString();
				
				// when exiting the mainloop
				dictionaryClearByHand(gameLogic);
				ketsjiengine->StopEngine();
				exitGamePythonScripting();
				networkdevice->Disconnect();
			}
			if (sceneconverter)
			{
				delete sceneconverter;
				sceneconverter = NULL;
			}
		}
		// set the cursor back to normal
		canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		
		// clean up some stuff
		audiodevice->StopCD();
		
		if (ketsjiengine)
		{
			delete ketsjiengine;
			ketsjiengine = NULL;
		}
		if (kxsystem)
		{
			delete kxsystem;
			kxsystem = NULL;
		}
		if (networkdevice)
		{
			delete networkdevice;
			networkdevice = NULL;
		}
		if (keyboarddevice)
		{
			delete keyboarddevice;
			keyboarddevice = NULL;
		}
		if (mousedevice)
		{
			delete mousedevice;
			mousedevice = NULL;
		}
		if (rasterizer)
		{
			delete rasterizer;
			rasterizer = NULL;
		}
		if (rendertools)
		{
			delete rendertools;
			rendertools = NULL;
		}
		if (canvas)
		{
			delete canvas;
			canvas = NULL;
		}
		SND_DeviceManager::Unsubscribe();
	
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);

	if (bfd) BLO_blendfiledata_free(bfd);

	// Release Python's GIL
	PyGILState_Release(gilstate);
}

extern "C" void StartKetsjiShellSimulation(struct ScrArea *area,
								 char* scenename,
								 struct Main* maggie,
								 struct SpaceIpo *sipo,
								 int always_use_expand_framing)
{
    int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;

	Main* blenderdata = maggie;

	char* startscenename = scenename;
	char pathname[160];
	strcpy (pathname, maggie->name);
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();

	bgl::InitExtensions(1);

	do
	{

		// get some preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool usefixed = (SYS_GetCommandLineInt(syshandle, "fixedtime", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool game2ipo = true;//(SYS_GetCommandLineInt(syshandle, "game2ipo", 0) != 0);
		bool displaylists = (SYS_GetCommandLineInt(syshandle, "displaylists", 0) != 0);
		bool usemat = false;

		// create the canvas, rasterizer and rendertools
		RAS_ICanvas* canvas = new KX_BlenderCanvas(area);
		//canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
		RAS_IRenderTools* rendertools = new KX_BlenderRenderTools();
		RAS_IRasterizer* rasterizer = NULL;

		// let's see if we want to use vertexarrays or not
		int usevta = SYS_GetCommandLineInt(syshandle,"vertexarrays",1);
		bool useVertexArrays = (usevta > 0);

		bool lock_arrays = (displaylists && useVertexArrays);

		if(displaylists && !useVertexArrays)
			rasterizer = new RAS_ListRasterizer(canvas);
		else if (useVertexArrays && bgl::QueryVersion(1, 1))
			rasterizer = new RAS_VAOpenGLRasterizer(canvas, lock_arrays);
		else
			rasterizer = new RAS_OpenGLRasterizer(canvas);

		// create the inputdevices
		KX_BlenderKeyboardDevice* keyboarddevice = new KX_BlenderKeyboardDevice();
		KX_BlenderMouseDevice* mousedevice = new KX_BlenderMouseDevice();

		// create a networkdevice
		NG_NetworkDeviceInterface* networkdevice = new
			NG_LoopBackNetworkDeviceInterface();

		// get an audiodevice
		SND_DeviceManager::Subscribe();
		SND_IAudioDevice* audiodevice = SND_DeviceManager::Instance();
		audiodevice->UseCD();

		// create a ketsji/blendersystem (only needed for timing and stuff)
		KX_BlenderSystem* kxsystem = new KX_BlenderSystem();

		// create the ketsjiengine
		KX_KetsjiEngine* ketsjiengine = new KX_KetsjiEngine(kxsystem);

		int i;

		Scene *blscene = NULL;
		if (!bfd)
		{
			blscene = (Scene*) maggie->scene.first;
			for (Scene *sce= (Scene*) maggie->scene.first; sce; sce= (Scene*) sce->id.next)
			{
				if (startscenename == (sce->id.name+2))
				{
					blscene = sce;
					break;
				}
			}
		} else {
			blscene = bfd->curscene;
		}
        int cframe,startFrame;
		if (blscene)
		{
			cframe=blscene->r.cfra;
			startFrame = blscene->r.sfra;
			blscene->r.cfra=startFrame;
			update_for_newframe();
			ketsjiengine->SetGame2IpoMode(game2ipo,startFrame);
		}

		// Quad buffered needs a special window.
		if (blscene->r.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
			rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) blscene->r.stereomode);

		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(maggie,sipo, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			sceneconverter->addInitFromFrame=true;
			
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);

			if(usemat)
				sceneconverter->SetMaterials(true);

			KX_Scene* startscene = new KX_Scene(keyboarddevice,
				mousedevice,
				networkdevice,
				audiodevice,
				startscenename);
			// some python things
			PyObject* dictionaryobject = initGamePythonScripting("Ketsji", psl_Lowest);
			ketsjiengine->SetPythonDictionary(dictionaryobject);
			initRasterizer(rasterizer, canvas);
			PyObject *gameLogic = initGameLogic(startscene);
			initGameKeys();
			initPythonConstraintBinding();

			if (sceneconverter)
			{
				// convert and add scene
				sceneconverter->ConvertScene(
					startscenename,
					startscene,
					dictionaryobject,
					keyboarddevice,
					rendertools,
					canvas);
				ketsjiengine->AddScene(startscene);

				// start the engine
				ketsjiengine->StartEngine(false);
				
				ketsjiengine->SetUseFixedTime(true);
				
				ketsjiengine->SetTicRate(
					(double) blscene->r.frs_sec /
					(double) blscene->r.frs_sec_base);

				// the mainloop
				while ((blscene->r.cfra<=blscene->r.efra)&&(!exitrequested))
				{
                    printf("frame %i\n",blscene->r.cfra);
                    // first check if we want to exit
					exitrequested = ketsjiengine->GetExitCode();
	
					// kick the engine
					ketsjiengine->NextFrame();
				    blscene->r.cfra=blscene->r.cfra+1;
				    update_for_newframe();
					
				}
				exitstring = ketsjiengine->GetExitString();
			}
			if (sceneconverter)
			{
				delete sceneconverter;
				sceneconverter = NULL;
			}
		}
		blscene->r.cfra=cframe;
		// set the cursor back to normal
		canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);

		// clean up some stuff
		audiodevice->StopCD();
		if (ketsjiengine)
		{
			delete ketsjiengine;
			ketsjiengine = NULL;
		}
		if (kxsystem)
		{
			delete kxsystem;
			kxsystem = NULL;
		}
		if (networkdevice)
		{
			delete networkdevice;
			networkdevice = NULL;
		}
		if (keyboarddevice)
		{
			delete keyboarddevice;
			keyboarddevice = NULL;
		}
		if (mousedevice)
		{
			delete mousedevice;
			mousedevice = NULL;
		}
		SND_DeviceManager::Unsubscribe();

	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);
	if (bfd) BLO_blendfiledata_free(bfd);

	// Release Python's GIL
	PyGILState_Release(gilstate);
}
