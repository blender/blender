/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Blender's Ketsji startpoint
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// don't show stl-warnings
#pragma warning (disable:4786)
#endif

#include "KX_BlenderGL.h"
#include "KX_BlenderCanvas.h"
#include "KX_BlenderKeyboardDevice.h"
#include "KX_BlenderMouseDevice.h"
#include "KX_BlenderRenderTools.h"
#include "KX_BlenderSystem.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"

#include "RAS_OpenGLRasterizer.h"
#include "RAS_VAOpenGLRasterizer.h"
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

static BlendFileData *load_game_data(char *filename) {
	BlendReadError error;
	BlendFileData *bfd= BLO_read_from_file(filename, &error);
	
	if (!bfd) {
		printf("Loading %s failed: %s\n", filename, BLO_bre_as_string(error));
	}
	
	return bfd;
}

extern "C" void StartKetsjiShell(struct ScrArea *area,
								 char* scenename,
								 struct Main* maggie,
								 int always_use_expand_framing)
{
	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	Main* blenderdata = maggie;
	char* startscenename = scenename;
	char pathname[160];
	strcpy (pathname, maggie->name);
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;
	
	RAS_GLExtensionManager *extman = new RAS_GLExtensionManager(SYS_GetCommandLineInt(SYS_GetSystem(), "show_extensions", 1));
	extman->LinkExtensions();

	do
	{
		View3D *v3d= (View3D*) area->spacedata.first;
		
		// get some preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool usefixed = (SYS_GetCommandLineInt(syshandle, "fixedtime", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		
		// create the canvas, rasterizer and rendertools
		RAS_ICanvas* canvas = new KX_BlenderCanvas(area);
		canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
		RAS_IRenderTools* rendertools = new KX_BlenderRenderTools();
		RAS_IRasterizer* rasterizer = NULL;
		
		// let's see if we want to use vertexarrays or not
		int usevta = SYS_GetCommandLineInt(syshandle,"vertexarrays",1);
		bool useVertexArrays = (usevta > 0);
		
		if (useVertexArrays && extman->QueryVersion(1, 1))
			rasterizer = new RAS_VAOpenGLRasterizer(canvas);
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
			
			char basedpath[160];
			// base the actuator filename with respect
			// to the original file working directory
			if (exitstring != "")
				strcpy(basedpath, exitstring.Ptr());

			BLI_convertstringcode(basedpath, pathname, 0);
			bfd = load_game_data(basedpath);
			
			// if it wasn't loaded, try it forced relative
			if (!bfd)
			{
				// just add "//" in front of it
				char temppath[162];
				strcpy(temppath, "//");
				strcat(temppath, basedpath);
				
				BLI_convertstringcode(temppath, pathname, 0);
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
		
		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			if (v3d->persp != 2)
			{
				ketsjiengine->EnableCameraOverride(startscenename);
				ketsjiengine->SetCameraOverrideUseOrtho((v3d->persp == 0));
				ketsjiengine->SetCameraOverrideProjectionMatrix(projmat);
				ketsjiengine->SetCameraOverrideViewMatrix(viewmat);
			}
			
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(blenderdata, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);
			
			
			KX_Scene* startscene = new KX_Scene(keyboarddevice,
				mousedevice,
				networkdevice,
				audiodevice,
				startscenename);
			
			// some python things
			PyObject* dictionaryobject = initGamePythonScripting("Ketsji", psl_Lowest);
			ketsjiengine->SetPythonDictionary(dictionaryobject);
			initRasterizer(rasterizer, canvas);
			initGameLogic(startscene);
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
				ketsjiengine->StartEngine();
				
				// the mainloop
				while (!exitrequested)
				{
					// first check if we want to exit
					exitrequested = ketsjiengine->GetExitCode();
					
					// kick the engine
					ketsjiengine->NextFrame();
					
					// render the frame
					ketsjiengine->Render();
					
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
				exitGamePythonScripting();
				ketsjiengine->StopEngine();
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
		SND_DeviceManager::Unsubscribe();
		
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
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);

	if (extman)
	{
		delete extman;
		extman = NULL;
	}
	if (bfd) BLO_blendfiledata_free(bfd);
}
