/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Jacques Guignot, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_scene.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BSE_headerbuttons.h> /* for copy_scene */
#include <BIF_drawscene.h>		 /* for set_scene */
#include <BIF_space.h>				 /* for copy_view3d_lock() */
#include <MEM_guardedalloc.h>  /* for MEM_callocN */
#include <mydevice.h>					 /* for #define REDRAW */

#include "Object.h"
#include "bpy_types.h"

#include "Scene.h"

static Base *EXPP_Scene_getObjectBase (Scene *scene, Object *object);
PyObject *M_Object_Get (PyObject *self, PyObject *args); /* from Object.c */

/*****************************************************************************/
/* Python BPy_Scene defaults:												 */
/*****************************************************************************/
#define EXPP_SCENE_FRAME_MAX 18000
#define EXPP_SCENE_RENDER_WINRESOLUTION_MIN 4
#define EXPP_SCENE_RENDER_WINRESOLUTION_MAX 10000

/*****************************************************************************/
/* Python API function prototypes for the Scene module.						*/
/*****************************************************************************/
static PyObject *M_Scene_New (PyObject *self, PyObject *args,
															 PyObject *keywords);
static PyObject *M_Scene_Get (PyObject *self, PyObject *args);
static PyObject *M_Scene_GetCurrent (PyObject *self);
static PyObject *M_Scene_Unlink (PyObject *self, PyObject *arg);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.		*/
/* In Python these will be written to the console when doing a				*/
/* Blender.Scene.__doc__													*/
/*****************************************************************************/
static char M_Scene_doc[] =
"The Blender.Scene submodule";
static char M_Scene_New_doc[] =
"(name = 'Scene') - Create a new Scene called 'name' in Blender.";
static char M_Scene_Get_doc[] =
"(name = None) - Return the scene called 'name'.\n\
					 If 'name' is None, return a list with all Scenes.";
static char M_Scene_GetCurrent_doc[] =
"() - Return the currently active Scene in Blender.";
static char M_Scene_Unlink_doc[] =
"(scene) - Unlink (delete) scene 'Scene' from Blender.\n\
(scene) is of type Blender scene.";
// Python BPy_Scene rendering declarations:						
static char M_Scene_Render_doc[] =
"() - render the scene\n";
static char M_Scene_RenderAnim_doc[] =
"() - render a sequence\n";
static char M_Scene_CloseRenderWindow_doc[] =
"() - close the rendering window\n";
static char M_Scene_Play_doc[] =
"() - play animation of rendered images/avi (searches Pics: field)\n";
static char M_Scene_SetRenderPath_doc[] =
"() - get/set the path to output the rendered images to\n";
static char M_Scene_GetRenderPath_doc[] =
"() - get the path to directory where rendered images will go\n";
static char M_Scene_SetBackbufPath_doc[] =
"() - get/set the path to a background image and load it\n";
static char M_Scene_GetBackbufPath_doc[] =
"() - get the path to background image file\n";
static char M_Scene_EnableBackbuf_doc[] =
"() - enable/disable the backbuf image\n";
static char M_Scene_SetFtypePath_doc[] =
"() - get/set the path to output the Ftype file\n";
static char M_Scene_GetFtypePath_doc[] =
"() - get the path to Ftype file\n";
static char M_Scene_EnableExtensions_doc[] =
"() - enable/disable windows extensions for output files\n";
static char M_Scene_EnableSequencer_doc[] =
"() - enable/disable Do Sequence\n";
static char M_Scene_EnableRenderDaemon_doc[] =
"() - enable/disable Scene daemon\n";
static char M_Scene_SetRenderWinPos_doc[] =
"() - position the rendering window in around the edge of the screen\n";
static char M_Scene_EnableDispView_doc[] =
"() - enable Sceneing in view\n";
static char M_Scene_EnableDispWin_doc[] =
"() - enable Sceneing in new window\n";
static char M_Scene_EnableToonShading_doc[] =
"() - enable/disable Edge rendering\n";
static char M_Scene_EdgeIntensity_doc[] =
"() - get/set edge intensity for toon shading\n";
static char M_Scene_EnableEdgeShift_doc[] =
"() - with the unified renderer the outlines are shifted a bit.\n";
static char M_Scene_EnableEdgeAll_doc[] =
"() - also consider transparent faces for edge-rendering with the unified renderer\n";
static char M_Scene_SetEdgeColor_doc[] =
"() - set the edge color for toon shading - Red,Green,Blue expected.\n";
static char M_Scene_GetEdgeColor_doc[] =
"() - get the edge color for toon shading - Red,Green,Blue expected.\n";
static char M_Scene_EdgeAntiShift_doc[] =
"() - with the unified renderer to reduce intensity on boundaries.\n";
static char M_Scene_EnableOversampling_doc[] =
"() - enable/disable oversampling (anit-aliasing).\n";
static char M_Scene_SetOversamplingLevel_doc[] =
"() - get/set the level of oversampling (anit-aliasing).\n";
static char M_Scene_EnableMotionBlur_doc[] =
"() - enable/disable MBlur.\n";
static char M_Scene_MotionBlurLevel_doc[] =
"() - get/set the length of shutter time for motion blur.\n";
static char M_Scene_PartsX_doc[] =
"() - get/set the number of parts to divide the render in the X direction\n";
static char M_Scene_PartsY_doc[] =
"() - get/set the number of parts to divide the render in the Y direction\n";
static char M_Scene_EnableSky_doc[] =
"() - enable render background with sky\n";
static char M_Scene_EnablePremultiply_doc[] =
"() - enable premultiply alpha\n";
static char M_Scene_EnableKey_doc[] =
"() - enable alpha and colour values remain unchanged\n";
static char M_Scene_EnableShadow_doc[] =
"() - enable/disable shadow calculation\n";
static char M_Scene_EnableEnvironmentMap_doc[] =
"() - enable/disable environment map rendering\n";
static char M_Scene_EnableRayTracing_doc[] =
"() - enable/disable ray tracing\n";
static char M_Scene_EnableRadiosityRender_doc[] =
"() - enable/disable radiosity rendering\n";
static char M_Scene_EnablePanorama_doc[] =
"() - enable/disable panorama rendering (output width is multiplied by Xparts)\n";
static char M_Scene_SetRenderWinSize_doc[] =
"() - get/set the size of the render window\n";
static char M_Scene_EnableFieldRendering_doc[] =
"() - enable/disable field rendering\n";
static char M_Scene_EnableOddFieldFirst_doc[] =
"() - enable/disable Odd field first rendering (Default: Even field)\n";
static char M_Scene_EnableFieldTimeDisable_doc[] =
"() - enable/disable time difference in field calculations\n";
static char M_Scene_EnableGaussFilter_doc[] =
"() - enable/disable Gauss sampling filter for antialiasing\n";
static char M_Scene_EnableBorderRender_doc[] =
"() - enable/disable small cut-out rendering\n";
static char M_Scene_EnableGammaCorrection_doc[] =
"() - enable/disable gamma correction\n";
static char M_Scene_GaussFilterSize_doc[] =
"() - get/sets the Gauss filter size\n";
static char M_Scene_StartFrame_doc[] =
"() - get/set the starting frame for sequence rendering\n";
static char M_Scene_EndFrame_doc[] =
"() - get/set the ending frame for sequence rendering\n";
static char M_Scene_ImageSizeX_doc[] =
"() - get/set the image width in pixels\n";
static char M_Scene_ImageSizeY_doc[] =
"() - get/set the image height in pixels\n";
static char M_Scene_AspectRatioX_doc[] =
"() - get/set the horizontal aspect ratio\n";
static char M_Scene_AspectRatioY_doc[] =
"() - get/set the vertical aspect ratio\n";
static char M_Scene_SetRenderer_doc[] =
"() - get/set which renderer to render the output\n";
static char M_Scene_EnableCropping_doc[] =
"() - enable/disable exclusion of border rendering from total image\n";
static char M_Scene_SetImageType_doc[] =
"() - get/set the type of image to output from the render\n";
static char M_Scene_Quality_doc[] =
"() - get/set quality get/setting for JPEG images, AVI Jpeg and SGI movies\n";
static char M_Scene_FramesPerSec_doc[] =
"() - get/set frames per second\n";
static char M_Scene_EnableGrayscale_doc[] =
"() - images are saved with BW (grayscale) data\n";
static char M_Scene_EnableRGBColor_doc[] =
"() - images are saved with RGB (color) data\n";
static char M_Scene_EnableRGBAColor_doc[] =
"() - images are saved with RGB and Alpha data (if supported)\n";
static char M_Scene_SizePreset_doc[] =
"() - get/set the render to one of a few preget/sets\n";
static char M_Scene_EnableUnifiedRenderer_doc[] =
"() - use the unified renderer\n";
static char M_Scene_SetYafrayGIQuality_doc[] =
"() - get/set yafray global Illumination quality\n";
static char M_Scene_SetYafrayGIMethod_doc[] =
"() - get/set yafray global Illumination method\n";
static char M_Scene_YafrayGIPower_doc[] =
"() - get/set GI lighting intensity scale\n";
static char M_Scene_YafrayGIDepth_doc[] =
"() - get/set number of bounces of the indirect light\n";
static char M_Scene_YafrayGICDepth_doc[] =
"() - get/set number of bounces inside objects (for caustics)\n";
static char M_Scene_EnableYafrayGICache_doc[] =
"() - enable/disable cache irradiance samples (faster)\n";
static char M_Scene_EnableYafrayGIPhotons_doc[] =
"() - enable/disable use global photons to help in GI\n";
static char M_Scene_YafrayGIPhotonCount_doc[] =
"() - get/set number of photons to shoot\n";
static char M_Scene_YafrayGIPhotonRadius_doc[] =
"() - get/set radius to search for photons to mix (blur)\n";
static char M_Scene_YafrayGIPhotonMixCount_doc[] =
"() - get/set number of photons to shoot\n";
static char M_Scene_EnableYafrayGITunePhotons_doc[] =
"() - enable/disable show the photonmap directly in the render for tuning\n";
static char M_Scene_YafrayGIShadowQuality_doc[] =
"() - get/set the shadow quality, keep it under 0.95\n";
static char M_Scene_YafrayGIPixelsPerSample_doc[] =
"() - get/set maximum number of pixels without samples, the lower the better and slower\n";
static char M_Scene_EnableYafrayGIGradient_doc[] =
"() - enable/disable try to smooth lighting using a gradient\n";
static char M_Scene_YafrayGIRefinement_doc[] =
"() - get/setthreshold to refine shadows EXPERIMENTAL. 1 = no refinement\n";
static char M_Scene_YafrayRayBias_doc[] =
"() - get/set shadow ray bias to avoid self shadowing\n";
static char M_Scene_YafrayRayDepth_doc[] =
"() - get/set maximum render ray depth from the camera\n";
static char M_Scene_YafrayGamma_doc[] =
"() - get/set gamma correction, 1 is off\n";
static char M_Scene_YafrayExposure_doc[] =
"() - get/set exposure adjustment, 0 is off\n";
static char M_Scene_YafrayProcessorCount_doc[] =
"() - get/set number of processors to use\n";
static char M_Scene_EnableGameFrameStretch_doc[] =
"() - enble stretch or squeeze the viewport to fill the display window\n";
static char M_Scene_EnableGameFrameExpose_doc[] =
"() - enable show the entire viewport in the display window, viewing more horizontally or vertically\n";
static char M_Scene_EnableGameFrameBars_doc[] =
"() - enable show the entire viewport in the display window, using bar horizontally or vertically\n";
static char M_Scene_SetGameFrameColor_doc[] =
"() - set the red, green, blue component of the bars\n";
static char M_Scene_GetGameFrameColor_doc[] =
"() - get the red, green, blue component of the bars\n";
static char M_Scene_GammaLevel_doc[] =
"() - get/set the gamma value for blending oversampled images (1.0 = no correction\n";
static char M_Scene_PostProcessAdd_doc[] =
"() - get/set post processing add\n";
static char M_Scene_PostProcessMultiply_doc[] =
"() - get/set post processing multiply\n";
static char M_Scene_PostProcessGamma_doc[] =
"() - get/set post processing gamma\n";
static char M_Scene_SGIMaxsize_doc[] =
"() - get/set maximum size per frame to save in an SGI movie\n";
static char M_Scene_EnableSGICosmo_doc[] =
"() - enable/disable attempt to save SGI movies using Cosmo hardware\n";
static char M_Scene_OldMapValue_doc[] =
"() - get/set specify old map value in frames\n";
static char M_Scene_NewMapValue_doc[] =
"() - get/set specify new map value in frames\n";


/*****************************************************************************/
/* Python method structure definition for Blender.Scene module:							 */
/*****************************************************************************/
struct PyMethodDef M_Scene_methods[] = {
	{"New",(PyCFunction)M_Scene_New, METH_VARARGS|METH_KEYWORDS,
					M_Scene_New_doc},
	{"Get",					M_Scene_Get,				 METH_VARARGS, M_Scene_Get_doc},
	{"get",					M_Scene_Get,				 METH_VARARGS, M_Scene_Get_doc},
	{"GetCurrent",(PyCFunction)M_Scene_GetCurrent,
														 METH_NOARGS,  M_Scene_GetCurrent_doc},
	{"getCurrent",(PyCFunction)M_Scene_GetCurrent,
														 METH_NOARGS,  M_Scene_GetCurrent_doc},
	{"Unlink",			M_Scene_Unlink,			 METH_VARARGS, M_Scene_Unlink_doc},
	{"unlink",			M_Scene_Unlink,			 METH_VARARGS, M_Scene_Unlink_doc},
	{"CloseRenderWindow",(PyCFunction)M_Render_CloseRenderWindow, METH_NOARGS,
				M_Scene_CloseRenderWindow_doc},
	{"EnableDispView",(PyCFunction)M_Render_EnableDispView, METH_NOARGS,
				M_Scene_EnableDispView_doc},
	{"EnableDispWin",(PyCFunction)M_Render_EnableDispWin, METH_NOARGS,
				M_Scene_EnableDispWin_doc},
	{"SetRenderWinPos",(PyCFunction)M_Render_SetRenderWinPos, METH_VARARGS,
				M_Scene_SetRenderWinPos_doc}, 
	{"EnableEdgeShift",(PyCFunction)M_Render_EnableEdgeShift, METH_VARARGS,
				M_Scene_EnableEdgeShift_doc},
	{"EnableEdgeAll",(PyCFunction)M_Render_EnableEdgeAll, METH_VARARGS,
				M_Scene_EnableEdgeAll_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Scene methods declarations:																		 */
/*****************************************************************************/
static PyObject *Scene_getName(BPy_Scene *self);
static PyObject *Scene_setName(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_getWinSize(BPy_Scene *self);
static PyObject *Scene_setWinSize(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_copy(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_startFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_endFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_currentFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args);
static PyObject *Scene_makeCurrent(BPy_Scene *self);
static PyObject *Scene_update(BPy_Scene *self, PyObject *args);
static PyObject *Scene_link(BPy_Scene *self, PyObject *args);
static PyObject *Scene_unlink(BPy_Scene *self, PyObject *args);
static PyObject *Scene_getRenderdir(BPy_Scene *self);
static PyObject *Scene_getBackbufdir(BPy_Scene *self);
static PyObject *Scene_getChildren(BPy_Scene *self);
static PyObject *Scene_getCurrentCamera(BPy_Scene *self);
static PyObject *Scene_setCurrentCamera(BPy_Scene *self, PyObject *args);


/*****************************************************************************/
/* Python BPy_Scene methods table:											*/
/*****************************************************************************/
static PyMethodDef BPy_Scene_methods[] = {
 /* name, method, flags, doc */
	{"getName", (PyCFunction)Scene_getName, METH_NOARGS,
			"() - Return Scene name"},
	{"setName", (PyCFunction)Scene_setName, METH_VARARGS,
					"(str) - Change Scene name"},
	{"copy",		(PyCFunction)Scene_copy, METH_VARARGS,
					"(duplicate_objects = 1) - Return a copy of this scene\n"
	"The optional argument duplicate_objects defines how the scene\n"
	"children are duplicated:\n\t0: Link Objects\n\t1: Link Object Data"
	"\n\t2: Full copy\n"},
	{"currentFrame", (PyCFunction)Scene_currentFrame, METH_VARARGS,
					"(frame) - If frame is given, the current frame is set and"
									"\nreturned in any case"},
	{"makeCurrent", (PyCFunction)Scene_makeCurrent, METH_NOARGS,
					"() - Make self the current scene"},
	{"update", (PyCFunction)Scene_update, METH_VARARGS,
					"(full = 0) - Update scene self.\n"
					"full = 0: sort the base list of objects."
					"full = 1: full update -- also regroups, does ipos, ikas, keys"},
	{"link", (PyCFunction)Scene_link, METH_VARARGS,
					"(obj) - Link Object obj to this scene"},
	{"unlink", (PyCFunction)Scene_unlink, METH_VARARGS,
					"(obj) - Unlink Object obj from this scene"},
	{"getChildren", (PyCFunction)Scene_getChildren, METH_NOARGS,
					"() - Return list of all objects linked to scene self"},
	{"getCurrentCamera", (PyCFunction)Scene_getCurrentCamera, METH_NOARGS,
					"() - Return current active Camera"},
	{"setCurrentCamera", (PyCFunction)Scene_setCurrentCamera, METH_VARARGS,
					"() - Set the currently active Camera"},
	//RENDERING METHODS
	{"render",(PyCFunction)M_Render_Render, METH_NOARGS,
			M_Scene_Render_doc},
	{"renderAnim",(PyCFunction)M_Render_RenderAnim, METH_NOARGS,
				M_Scene_RenderAnim_doc},
	{"play",(PyCFunction)M_Render_Play, METH_NOARGS,
				M_Scene_Play_doc},
	{"setRenderPath",(PyCFunction)M_Render_SetRenderPath, METH_VARARGS,
				M_Scene_SetRenderPath_doc},
	{"getRenderPath",(PyCFunction)M_Render_GetRenderPath, METH_NOARGS,
				M_Scene_GetRenderPath_doc},
	{"setBackbufPath",(PyCFunction)M_Render_SetBackbufPath, METH_VARARGS,
				M_Scene_SetBackbufPath_doc},
	{"getBackbufPath",(PyCFunction)M_Render_GetBackbufPath, METH_NOARGS,
				M_Scene_GetBackbufPath_doc},
	{"enableBackbuf",(PyCFunction)M_Render_EnableBackbuf, METH_VARARGS,
				M_Scene_EnableBackbuf_doc},
	{"setFtypePath",(PyCFunction)M_Render_SetFtypePath, METH_VARARGS,
				M_Scene_SetFtypePath_doc},
	{"getFtypePath",(PyCFunction)M_Render_GetFtypePath, METH_NOARGS,
				M_Scene_GetFtypePath_doc},
	{"enableExtensions",(PyCFunction)M_Render_EnableExtensions, METH_VARARGS,
				M_Scene_EnableExtensions_doc},
	{"enableSequencer",(PyCFunction)M_Render_EnableSequencer, METH_VARARGS,
				M_Scene_EnableSequencer_doc},
	{"enableRenderDaemon",(PyCFunction)M_Render_EnableRenderDaemon, METH_VARARGS,
				M_Scene_EnableRenderDaemon_doc},
	{"enableToonShading",(PyCFunction)M_Render_EnableToonShading, METH_VARARGS,
				M_Scene_EnableToonShading_doc},
	{"edgeIntensity",(PyCFunction)M_Render_EdgeIntensity, METH_VARARGS,
				M_Scene_EdgeIntensity_doc},
	{"setEdgeColor",(PyCFunction)M_Render_SetEdgeColor, METH_VARARGS,
				M_Scene_SetEdgeColor_doc},
	{"getEdgeColor",(PyCFunction)M_Render_GetEdgeColor, METH_VARARGS,
				M_Scene_GetEdgeColor_doc},
	{"edgeAntiShift",(PyCFunction)M_Render_EdgeAntiShift, METH_VARARGS,
				M_Scene_EdgeAntiShift_doc},
	{"enableOversampling",(PyCFunction)M_Render_EnableOversampling, METH_VARARGS,
				M_Scene_EnableOversampling_doc},
	{"setOversamplingLevel",(PyCFunction)M_Render_SetOversamplingLevel, METH_VARARGS,
				M_Scene_SetOversamplingLevel_doc},
	{"enableMotionBlur",(PyCFunction)M_Render_EnableMotionBlur, METH_VARARGS,
				M_Scene_EnableMotionBlur_doc},
	{"motionBlurLevel",(PyCFunction)M_Render_MotionBlurLevel, METH_VARARGS,
				M_Scene_MotionBlurLevel_doc},
	{"partsX",(PyCFunction)M_Render_PartsX, METH_VARARGS,
				M_Scene_PartsX_doc},
	{"partsY",(PyCFunction)M_Render_PartsY, METH_VARARGS,
				M_Scene_PartsY_doc},
	{"enableSky",(PyCFunction)M_Render_EnableSky, METH_NOARGS,
				M_Scene_EnableSky_doc},
	{"enablePremultiply",(PyCFunction)M_Render_EnablePremultiply, METH_NOARGS,
				M_Scene_EnablePremultiply_doc},
	{"enableKey",(PyCFunction)M_Render_EnableKey, METH_NOARGS,
				M_Scene_EnableKey_doc}, 
	{"enableShadow",(PyCFunction)M_Render_EnableShadow, METH_VARARGS,
				M_Scene_EnableShadow_doc},
	{"enablePanorama",(PyCFunction)M_Render_EnablePanorama, METH_VARARGS,
				M_Scene_EnablePanorama_doc},
	{"enableEnvironmentMap",(PyCFunction)M_Render_EnableEnvironmentMap, METH_VARARGS,
				M_Scene_EnableEnvironmentMap_doc},
	{"enableRayTracing",(PyCFunction)M_Render_EnableRayTracing, METH_VARARGS,
				M_Scene_EnableRayTracing_doc},
	{"enableRadiosityRender",(PyCFunction)M_Render_EnableRadiosityRender, METH_VARARGS,
				M_Scene_EnableRadiosityRender_doc},
	{"setRenderWinSize",(PyCFunction)M_Render_SetRenderWinSize, METH_VARARGS,
				M_Scene_SetRenderWinSize_doc},
	{"enableFieldRendering",(PyCFunction)M_Render_EnableFieldRendering, METH_VARARGS,
				M_Scene_EnableFieldRendering_doc},
	{"enableOddFieldFirst",(PyCFunction)M_Render_EnableOddFieldFirst, METH_VARARGS,
				M_Scene_EnableOddFieldFirst_doc},
	{"enableFieldTimeDisable",(PyCFunction)M_Render_EnableFieldTimeDisable, METH_VARARGS,
				M_Scene_EnableFieldTimeDisable_doc},
	{"enableGaussFilter",(PyCFunction)M_Render_EnableGaussFilter, METH_VARARGS,
				M_Scene_EnableGaussFilter_doc},
	{"enableBorderRender",(PyCFunction)M_Render_EnableBorderRender, METH_VARARGS,
				M_Scene_EnableBorderRender_doc},
	{"enableGammaCorrection",(PyCFunction)M_Render_EnableGammaCorrection, METH_VARARGS,
				M_Scene_EnableGammaCorrection_doc},
	{"gaussFilterSize",(PyCFunction)M_Render_GaussFilterSize, METH_VARARGS,
				M_Scene_GaussFilterSize_doc},
	{"startFrame",(PyCFunction)M_Render_StartFrame, METH_VARARGS,
				M_Scene_StartFrame_doc},
	{"endFrame",(PyCFunction)M_Render_EndFrame, METH_VARARGS,
				M_Scene_EndFrame_doc},
	{"imageSizeX",(PyCFunction)M_Render_ImageSizeX, METH_VARARGS,
				M_Scene_ImageSizeX_doc},
	{"imageSizeY",(PyCFunction)M_Render_ImageSizeY, METH_VARARGS,
				M_Scene_ImageSizeY_doc},
	{"aspectRatioX",(PyCFunction)M_Render_AspectRatioX, METH_VARARGS,
				M_Scene_AspectRatioX_doc},
	{"aspectRatioY",(PyCFunction)M_Render_AspectRatioY, METH_VARARGS,
				M_Scene_AspectRatioY_doc},
	{"setRenderer",(PyCFunction)M_Render_SetRenderer, METH_VARARGS,
				M_Scene_SetRenderer_doc},
	{"enableCropping",(PyCFunction)M_Render_EnableCropping, METH_VARARGS,
				M_Scene_EnableCropping_doc},
	{"setImageType",(PyCFunction)M_Render_SetImageType, METH_VARARGS,
				M_Scene_SetImageType_doc},
	{"quality",(PyCFunction)M_Render_Quality, METH_VARARGS,
				M_Scene_Quality_doc},
	{"framesPerSec",(PyCFunction)M_Render_FramesPerSec, METH_VARARGS,
				M_Scene_FramesPerSec_doc},
	{"enableGrayscale",(PyCFunction)M_Render_EnableGrayscale, METH_NOARGS,
				M_Scene_EnableGrayscale_doc},
	{"enableRGBColor",(PyCFunction)M_Render_EnableRGBColor, METH_NOARGS,
				M_Scene_EnableRGBColor_doc},
	{"enableRGBAColor",(PyCFunction)M_Render_EnableRGBAColor, METH_NOARGS,
				M_Scene_EnableRGBAColor_doc},
	{"sizePreset",(PyCFunction)M_Render_SizePreset, METH_VARARGS,
				M_Scene_SizePreset_doc},
	{"enableUnifiedRenderer",(PyCFunction)M_Render_EnableUnifiedRenderer, METH_VARARGS,
				M_Scene_EnableUnifiedRenderer_doc},
	{"setYafrayGIQuality",(PyCFunction)M_Render_SetYafrayGIQuality, METH_VARARGS,
				M_Scene_SetYafrayGIQuality_doc},
	{"setYafrayGIMethod",(PyCFunction)M_Render_SetYafrayGIMethod, METH_VARARGS,
				M_Scene_SetYafrayGIMethod_doc},
	{"yafrayGIPower",(PyCFunction)M_Render_YafrayGIPower, METH_VARARGS,
				M_Scene_YafrayGIPower_doc},
	{"yafrayGIDepth",(PyCFunction)M_Render_YafrayGIDepth, METH_VARARGS,
				M_Scene_YafrayGIDepth_doc},
	{"yafrayGICDepth",(PyCFunction)M_Render_YafrayGICDepth, METH_VARARGS,
				M_Scene_YafrayGICDepth_doc},
	{"enableYafrayGICache",(PyCFunction)M_Render_EnableYafrayGICache, METH_VARARGS,
				M_Scene_EnableYafrayGICache_doc},
	{"enableYafrayGIPhotons",(PyCFunction)M_Render_EnableYafrayGIPhotons, METH_VARARGS,
				M_Scene_EnableYafrayGIPhotons_doc},
	{"yafrayGIPhotonCount",(PyCFunction)M_Render_YafrayGIPhotonCount, METH_VARARGS,
				M_Scene_YafrayGIPhotonCount_doc},
	{"yafrayGIPhotonRadius",(PyCFunction)M_Render_YafrayGIPhotonRadius, METH_VARARGS,
				M_Scene_YafrayGIPhotonRadius_doc},
	{"yafrayGIPhotonMixCount",(PyCFunction)M_Render_YafrayGIPhotonMixCount, METH_VARARGS,
				M_Scene_YafrayGIPhotonMixCount_doc},
	{"enableYafrayGITunePhotons",(PyCFunction)M_Render_EnableYafrayGITunePhotons, METH_VARARGS,
				M_Scene_EnableYafrayGITunePhotons_doc},
	{"yafrayGIShadowQuality",(PyCFunction)M_Render_YafrayGIShadowQuality, METH_VARARGS,
				M_Scene_YafrayGIShadowQuality_doc},
	{"yafrayGIPixelsPerSample",(PyCFunction)M_Render_YafrayGIPixelsPerSample, METH_VARARGS,
				M_Scene_YafrayGIPixelsPerSample_doc},
	{"enableYafrayGIGradient",(PyCFunction)M_Render_EnableYafrayGIGradient, METH_VARARGS,
				M_Scene_EnableYafrayGIGradient_doc},
	{"yafrayGIRefinement",(PyCFunction)M_Render_YafrayGIRefinement, METH_VARARGS,
				M_Scene_YafrayGIRefinement_doc},
	{"yafrayRayBias",(PyCFunction)M_Render_YafrayRayBias, METH_VARARGS,
				M_Scene_YafrayRayBias_doc},
	{"yafrayRayDepth",(PyCFunction)M_Render_YafrayRayDepth, METH_VARARGS,
				M_Scene_YafrayRayDepth_doc},
	{"yafrayGamma",(PyCFunction)M_Render_YafrayGamma, METH_VARARGS,
				M_Scene_YafrayGamma_doc},
	{"yafrayExposure",(PyCFunction)M_Render_YafrayExposure, METH_VARARGS,
				M_Scene_YafrayExposure_doc},
	{"yafrayProcessorCount",(PyCFunction)M_Render_YafrayProcessorCount, METH_VARARGS,
				M_Scene_YafrayProcessorCount_doc},
	{"enableGameFrameStretch",(PyCFunction)M_Render_EnableGameFrameStretch, METH_NOARGS,
				M_Scene_EnableGameFrameStretch_doc},
	{"enableGameFrameExpose",(PyCFunction)M_Render_EnableGameFrameExpose, METH_NOARGS,
				M_Scene_EnableGameFrameExpose_doc},
	{"enableGameFrameBars",(PyCFunction)M_Render_EnableGameFrameBars, METH_NOARGS,
				M_Scene_EnableGameFrameBars_doc},
	{"setGameFrameColor",(PyCFunction)M_Render_SetGameFrameColor, METH_VARARGS,
				M_Scene_SetGameFrameColor_doc},
	{"getGameFrameColor",(PyCFunction)M_Render_GetGameFrameColor, METH_VARARGS,
				M_Scene_GetGameFrameColor_doc},
	{"gammaLevel",(PyCFunction)M_Render_GammaLevel, METH_VARARGS,
				M_Scene_GammaLevel_doc},
	{"postProcessAdd",(PyCFunction)M_Render_PostProcessAdd, METH_VARARGS,
				M_Scene_PostProcessAdd_doc},
	{"postProcessMultiply",(PyCFunction)M_Render_PostProcessMultiply, METH_VARARGS,
				M_Scene_PostProcessMultiply_doc},
	{"postProcessGamma",(PyCFunction)M_Render_PostProcessGamma, METH_VARARGS,
				M_Scene_PostProcessGamma_doc},
 	{"SGIMaxsize",(PyCFunction)M_Render_SGIMaxsize, METH_VARARGS,
				M_Scene_SGIMaxsize_doc},
	{"enableSGICosmo",(PyCFunction)M_Render_EnableSGICosmo, METH_VARARGS,
				M_Scene_EnableSGICosmo_doc},
	{"oldMapValue",(PyCFunction)M_Render_OldMapValue, METH_VARARGS,
				M_Scene_OldMapValue_doc},
	{"newMapValue",(PyCFunction)M_Render_NewMapValue, METH_VARARGS,
				M_Scene_NewMapValue_doc},
	//DEPRECATED
	{"getWinSize", (PyCFunction)Scene_getWinSize, METH_NOARGS,
			"() - Return Render window [x,y] dimensions"},
	{"setWinSize", (PyCFunction)Scene_setWinSize, METH_VARARGS,
					"(str) - Change Render window [x,y] dimensions"},
	{"startFrame", (PyCFunction)Scene_startFrame, METH_VARARGS,
					"(frame) - If frame is given, the start frame is set and"
									"\nreturned in any case"},
	{"endFrame", (PyCFunction)Scene_endFrame, METH_VARARGS,
					"(frame) - If frame is given, the end frame is set and"
									"\nreturned in any case"},
	{"frameSettings", (PyCFunction)Scene_frameSettings, METH_VARARGS,
					"(start, end, current) - Sets or retrieves the Scene's frame"
					" settings.\nIf the frame arguments are specified, they are set. "
					"A tuple (start, end, current) is returned in any case."},
	{"getRenderdir", (PyCFunction)Scene_getRenderdir, METH_NOARGS,
					"() - Return directory where rendered images are saved to"},
	{"getBackbufdir", (PyCFunction)Scene_getBackbufdir, METH_NOARGS,
					"() - Return location of the backbuffer image"},
	{0}
};

/*****************************************************************************/
/* Python Scene_Type callback function prototypes:							*/
/*****************************************************************************/
static void Scene_dealloc (BPy_Scene *self);
static int Scene_setAttr (BPy_Scene *self, char *name, PyObject *v);
static int Scene_compare (BPy_Scene *a, BPy_Scene *b);
static PyObject *Scene_getAttr (BPy_Scene *self, char *name);
static PyObject *Scene_repr (BPy_Scene *self);

/*****************************************************************************/
/* Python Scene_Type structure definition:								     */
/*****************************************************************************/
PyTypeObject Scene_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,								/* ob_size */
	"Scene",						/* tp_name */
	sizeof (BPy_Scene),				/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	(destructor)Scene_dealloc,		/* tp_dealloc */
	0,								/* tp_print */
	(getattrfunc)Scene_getAttr,		/* tp_getattr */
	(setattrfunc)Scene_setAttr,		/* tp_setattr */
	(cmpfunc)Scene_compare,			/* tp_compare */
	(reprfunc)Scene_repr,			/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_as_hash */
	0,0,0,0,0,0,
	0,								/* tp_doc */ 
	0,0,0,0,0,0,
	BPy_Scene_methods,				/* tp_methods */
	0,								/* tp_members */
};

static PyObject *M_Scene_New(PyObject *self, PyObject *args, PyObject *kword)
{
	char		 *name = "Scene";
	char		 *kw[] = {"name", NULL};
	PyObject *pyscene; /* for the Scene object wrapper in Python */
	Scene		 *blscene; /* for the actual Scene we create in Blender */

	if (!PyArg_ParseTupleAndKeywords(args, kword, "|s", kw, &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string or an empty argument list"));

	blscene = add_scene(name); /* first create the Scene in Blender */

	if (blscene){ 
	  /* normally, for most objects, we set the user count to zero here.
	   * Scene is different than most objs since it is the container
	   * for all the others. Since add_scene() has already set 
	   * the user count to one, we leave it alone.
	   */ 

	  /* now create the wrapper obj in Python */
	  pyscene = Scene_CreatePyObject (blscene);
	}
	else
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
														"couldn't create Scene obj in Blender"));

	if (pyscene == NULL)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create Scene PyObject"));

	return pyscene;
}

static PyObject *M_Scene_Get(PyObject *self, PyObject *args)
{
	char	*name = NULL;
	Scene *scene_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument (or nothing)"));

	scene_iter = G.main->scene.first;

	if (name) { /* (name) - Search scene by name */

		PyObject *wanted_scene = NULL;

		while ((scene_iter) && (wanted_scene == NULL)) {

			if (strcmp (name, scene_iter->id.name+2) == 0)
				wanted_scene = Scene_CreatePyObject (scene_iter);

			scene_iter = scene_iter->id.next;
		}

		if (wanted_scene == NULL) { /* Requested scene doesn't exist */
			char error_msg[64];
			PyOS_snprintf(error_msg, sizeof(error_msg),
											"Scene \"%s\" not found", name);
			return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
		}

		return wanted_scene;
	}

	else { /* () - return a list with wrappers for all scenes in Blender */
		int index = 0;
		PyObject *sce_pylist, *pyobj;

		sce_pylist = PyList_New (BLI_countlist (&(G.main->scene)));

		if (sce_pylist == NULL)
			return (PythonReturnErrorObject (PyExc_MemoryError,
							"couldn't create PyList"));

		while (scene_iter) {
			pyobj = Scene_CreatePyObject (scene_iter);

			if (!pyobj)
				return (PythonReturnErrorObject (PyExc_MemoryError,
									"couldn't create PyString"));

			PyList_SET_ITEM (sce_pylist, index, pyobj);

			scene_iter = scene_iter->id.next;
			index++;
		}

		return sce_pylist;
	}
}

static PyObject *M_Scene_GetCurrent (PyObject *self)
{
	return Scene_CreatePyObject ((Scene *)G.scene);
}

static PyObject *M_Scene_Unlink (PyObject *self, PyObject *args)
{ 
	PyObject *pyobj;
	Scene		 *scene;

	if (!PyArg_ParseTuple (args, "O!", &Scene_Type, &pyobj))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
								"expected Scene PyType object");

	scene = ((BPy_Scene *)pyobj)->scene;

	if (scene == G.scene)
				return EXPP_ReturnPyObjError (PyExc_SystemError,
								"current Scene cannot be removed!");

	free_libblock(&G.main->scene, scene);

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *Scene_Init (void)
{
	PyObject	*submodule;
 	PyObject *dict;

	Scene_Type.ob_type = &PyType_Type;
 	submodule = Py_InitModule3("Blender.Scene",	M_Scene_methods, M_Scene_doc);
	dict = PyModule_GetDict(submodule);

	#define EXPP_ADDCONST(x) PyDict_SetItemString(dict, #x, PyInt_FromLong(R_##x))
	EXPP_ADDCONST(INTERN);
	EXPP_ADDCONST(YAFRAY);
	EXPP_ADDCONST(AVIRAW);
	EXPP_ADDCONST(AVIJPEG);
#ifdef _WIN32
	EXPP_ADDCONST(AVICODEC);
#endif
	EXPP_ADDCONST(QUICKTIME);
	EXPP_ADDCONST(TARGA);
	EXPP_ADDCONST(RAWTGA);
	EXPP_ADDCONST(PNG);
	EXPP_ADDCONST(BMP);
	EXPP_ADDCONST(JPEG90);
	EXPP_ADDCONST(HAMX);
	EXPP_ADDCONST(IRIS);
	EXPP_ADDCONST(IRIZ);
	EXPP_ADDCONST(FTYPE);
	EXPP_ADDCONST(PAL);
	EXPP_ADDCONST(NTSC);
	EXPP_ADDCONST(DEFAULT);
	EXPP_ADDCONST(PREVIEW);
	EXPP_ADDCONST(PC);
	EXPP_ADDCONST(PAL169);
	EXPP_ADDCONST(PANO);
	EXPP_ADDCONST(FULL);

	#undef EXPP_ADDCONST
	#define EXPP_ADDCONST(x) PyDict_SetItemString(dict, #x, PyInt_FromLong(PY_##x))
	EXPP_ADDCONST(NONE);
	EXPP_ADDCONST(LOW);
	EXPP_ADDCONST(MEDIUM);
	EXPP_ADDCONST(HIGH);
	EXPP_ADDCONST(HIGHER);
	EXPP_ADDCONST(BEST);
	EXPP_ADDCONST(SKYDOME);
	EXPP_ADDCONST(GIFULL);

	return submodule;
}

PyObject *Scene_CreatePyObject (Scene *scene)
{
	BPy_Scene *pyscene;

	pyscene = (BPy_Scene *)PyObject_NEW (BPy_Scene, &Scene_Type);

	if (!pyscene)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create BPy_Scene object");

	pyscene->scene = scene;

	return (PyObject *)pyscene;
}

int Scene_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &Scene_Type);
}

Scene *Scene_FromPyObject (PyObject *pyobj)
{
	return ((BPy_Scene *)pyobj)->scene;
}

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument	 */
/*							name. Note that the calling function has to remove the first */
/*							two characters of the object name. These two characters			 */
/*							specify the type of the object (OB, ME, WO, ...)						 */
/*							The function will return NULL when no object with the given  */
/*							name is found.																							 */
/*****************************************************************************/
Scene * GetSceneByName (char * name)
{
	Scene	* scene_iter;

	scene_iter = G.main->scene.first;
	while (scene_iter)
	{
		if (StringEqual (name, GetIdName (&(scene_iter->id))))
		{
			return (scene_iter);
		}
		scene_iter = scene_iter->id.next;
	}

	/* There is no object with the given name */
	return (NULL);
}

/*****************************************************************************/
/* Python BPy_Scene methods:																								 */
/*****************************************************************************/
static PyObject *Scene_getName(BPy_Scene *self)
{
	PyObject *attr = PyString_FromString(self->scene->id.name+2);

	if (attr) return attr;

	return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Scene.name attribute"));
}

static PyObject *Scene_setName(BPy_Scene *self, PyObject *args)
{
	char *name;
	char buf[21];

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
																		 "expected string argument"));

	PyOS_snprintf(buf, sizeof(buf), "%s", name);

	rename_id(&self->scene->id, buf);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Scene_copy (BPy_Scene *self, PyObject *args)
{
	short dup_objs = 1;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "|h", &dup_objs))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int in [0,2] or nothing as argument");

	return Scene_CreatePyObject (copy_scene (scene, dup_objs));
}

/* Blender seems to accept any positive value up to 18000 for start, end and
 * current frames, independently. */

static PyObject *Scene_currentFrame (BPy_Scene *self, PyObject *args)
{
	short frame = -1;
	RenderData *rd = &self->scene->r;

	if (!PyArg_ParseTuple (args, "|h", &frame))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument or nothing");

	if (frame > 0) rd->cfra = EXPP_ClampInt(frame, 1, EXPP_SCENE_FRAME_MAX);

	return PyInt_FromLong (rd->cfra);
}

static PyObject *Scene_makeCurrent (BPy_Scene *self)
{
	Scene *scene = self->scene;

	if (scene) set_scene (scene);

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *Scene_update (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	int full = 0;

	if (!scene)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "|i", &full))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected nothing or int (0 or 1) argument");

/* Under certain circunstances, sort_baselist *here* can crash Blender.
 * A "RuntimeError: max recursion limit" happens when a scriptlink
 * on frame change has scene.update(1).
 * Investigate better how to avoid this. */
	if (!full)
		sort_baselist (scene);

	else if (full == 1)
			set_scene_bg (scene);

	else
		return EXPP_ReturnPyObjError (PyExc_ValueError,
			"in method scene.update(full), full should be:\n"
			"0: to only sort scene elements (old behavior); or\n"
			"1: for a full update (regroups, does ipos, ikas, keys, etc.)");

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *Scene_link (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	BPy_Object *bpy_obj;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "O!", &Object_Type, &bpy_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected Object argument");

	else { /* Ok, all is fine, let's try to link it */
		Object *object = bpy_obj->object;
		Base *base;

		/* We need to link the object to a 'Base', then link this base
		 * to the scene.	See DNA_scene_types.h ... */

		/* First, check if the object isn't already in the scene */
		base = EXPP_Scene_getObjectBase (scene, object);
		/* if base is not NULL ... */
		if (base) /* ... the object is already in one of the Scene Bases */
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							"object already in scene!");

		/* not linked, go get mem for a new base object */

		base = MEM_callocN(sizeof(Base), "newbase");
 
		if (!base)
			return EXPP_ReturnPyObjError (PyExc_MemoryError,
							"couldn't allocate new Base for object");

		/* check if this object has obdata, case not, try to create it */
		if (!object->data && (object->type != OB_EMPTY))
			EXPP_add_obdata(object); /* returns -1 on error, defined in Object.c */

		base->object = object; /* link object to the new base */
		base->lay = object->lay;
		base->flag = object->flag;

		object->id.us += 1; /* incref the object user count in Blender */

		BLI_addhead(&scene->base, base); /* finally, link new base to scene */
	}

	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject *Scene_unlink (BPy_Scene *self, PyObject *args)
{ 
	BPy_Object *bpy_obj = NULL;
	Object *object;
	Scene *scene = self->scene;
	Base *base;
	short retval = 0;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender scene was deleted!");

	if (!PyArg_ParseTuple(args, "O!", &Object_Type, &bpy_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Object as argument");

	object = bpy_obj->object;

	/* is the object really in the scene? */
	base = EXPP_Scene_getObjectBase(scene, object);
	 
	if (base) { /* if it is, remove it: */
		BLI_remlink(&scene->base, base);
		object->id.us -= 1;
		MEM_freeN (base);
		scene->basact = 0; /* in case the object was selected */
		retval = 1;
	}

	return Py_BuildValue ("i", PyInt_FromLong (retval));
}


static PyObject *Scene_getChildren (BPy_Scene *self)
{	
	Scene *scene = self->scene;
	PyObject *pylist= PyList_New(0);
	PyObject *bpy_obj;
	Object *object;
	Base *base;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	base = scene->base.first;

	while (base) {
		object = base->object;

		bpy_obj = M_Object_Get(Py_None,
										Py_BuildValue ("(s)", object->id.name+2));

		if (!bpy_obj)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
								"couldn't create new object wrapper");

		PyList_Append (pylist, bpy_obj);
		Py_XDECREF (bpy_obj); /* PyList_Append incref'ed it */

		base = base->next;
	}

	return pylist;
}

static PyObject *Scene_getCurrentCamera (BPy_Scene *self)
{	
	Object *cam_obj;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	cam_obj = scene->camera;

	if (cam_obj) /* if found, return a wrapper for it */
		return M_Object_Get (Py_None, Py_BuildValue ("(s)", cam_obj->id.name+2));

	Py_INCREF(Py_None); /* none found */
	return Py_None;
}

static PyObject *Scene_setCurrentCamera (BPy_Scene *self, PyObject *args)
{
	Object *object;
	BPy_Object *cam_obj;
	Scene  *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	if (!PyArg_ParseTuple(args, "O!", &Object_Type, &cam_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Camera Object as argument");

	object = cam_obj->object;

	scene->camera = object; /* set the current Camera */

	/* if this is the current scene, update its window now */
	if (scene == G.scene) copy_view3d_lock(REDRAW);

/* XXX copy_view3d_lock(REDRAW) prints "bad call to addqueue: 0 (18, 1)".
 * The same happens in bpython. */

	Py_INCREF(Py_None);
	return Py_None;
}

static void Scene_dealloc (BPy_Scene *self)
{
	PyObject_DEL (self);
}

static PyObject *Scene_getAttr (BPy_Scene *self, char *name)
{
	PyObject *attr = Py_None;

	if (strcmp(name, "name") == 0)
		attr = PyString_FromString(self->scene->id.name+2);

	else if (strcmp(name, "__members__") == 0)
		attr = Py_BuildValue("[s]", "name");


	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
											"couldn't create PyObject"));

	if (attr != Py_None) return attr; /* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod(BPy_Scene_methods, (PyObject *)self, name);
}

static int Scene_setAttr (BPy_Scene *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Scene.member = val instead of Scene.setMember(val), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Scene structure when necessary. */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. Using "N" doesn't increment value's ref count */
	valtuple = Py_BuildValue("(O)", value);

	if (!valtuple) /* everything OK with our PyObject? */
		return EXPP_ReturnIntError(PyExc_MemoryError,
												 "SceneSetAttr: couldn't create PyTuple");

/* Now we just compare "name" with all possible BPy_Scene member variables */
	if (strcmp (name, "name") == 0)
		error = Scene_setName (self, valtuple);

	else { /* Error: no member with the given name was found */
		Py_DECREF(valtuple);
		return (EXPP_ReturnIntError (PyExc_AttributeError, name));
	}

/* valtuple won't be returned to the caller, so we need to DECREF it */
	Py_DECREF(valtuple);

	if (error != Py_None) return -1;

/* Py_None was incref'ed by the called Scene_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
	Py_DECREF(Py_None);
	return 0; /* normal exit */
}

static int Scene_compare (BPy_Scene *a, BPy_Scene *b)
{
	Scene *pa = a->scene, *pb = b->scene;
	return (pa == pb) ? 0:-1;
}

static PyObject *Scene_repr (BPy_Scene *self)
{
	return PyString_FromFormat("[Scene \"%s\"]", self->scene->id.name+2);
}

Base *EXPP_Scene_getObjectBase(Scene *scene, Object *object)
{
	Base *base = scene->base.first;

	while (base) {

		if (object == base->object) return base; /* found it? */

		base = base->next;
	}

	return NULL; /* object isn't linked to this scene */
}

/*****************************************************************************/
// DEPRECATED 	
/*****************************************************************************/
static PyObject *Scene_getRenderdir (BPy_Scene *self)
{
	if (self->scene)
		return M_Render_GetRenderPath((PyObject*)self);

	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");
}

static PyObject *Scene_getBackbufdir (BPy_Scene *self)
{
	if (self->scene)
		return M_Render_GetBackbufPath((PyObject*)self);
	else
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene already deleted");
}

static PyObject *Scene_startFrame (BPy_Scene *self, PyObject *args)
{
	short frame = -1;
								 
	if (!PyArg_ParseTuple (args, "|h", &frame))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument or nothing");

	return M_Render_StartFrame((PyObject*)self, args);
}

static PyObject *Scene_endFrame (BPy_Scene *self, PyObject *args)
{
	short frame = -1;

	if (!PyArg_ParseTuple (args, "|h", &frame))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int argument or nothing");

	return M_Render_EndFrame((PyObject*)self, args);
}

static PyObject *Scene_getWinSize(BPy_Scene *self)
{
	PyObject* list = PyList_New (0);

	PyList_Append (list, M_Render_ImageSizeX((PyObject*)self, NULL));
	PyList_Append (list, M_Render_ImageSizeY((PyObject*)self, NULL));

	return list;
}

static PyObject *Scene_setWinSize(BPy_Scene *self, PyObject *args)
{
	int xres = -1, yres = -1;

	if (!PyArg_ParseTuple(args, "(ii)", &xres, &yres))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected a [x, y] list as argument");

	if (xres > 0)
		self->scene->r.xsch = EXPP_ClampInt(xres,
										EXPP_SCENE_RENDER_WINRESOLUTION_MIN,
										EXPP_SCENE_RENDER_WINRESOLUTION_MAX);
	if (yres > 0)
		self->scene->r.ysch = EXPP_ClampInt(yres,
										EXPP_SCENE_RENDER_WINRESOLUTION_MIN,
										EXPP_SCENE_RENDER_WINRESOLUTION_MAX);

	Py_INCREF(Py_None);
	return Py_None;

}

static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args)
{	
	int start = -1;
	int end = -1;
	int current = -1;
	RenderData *rd = NULL;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	rd = &scene->r;

	if (!PyArg_ParseTuple (args, "|iii", &start, &end, &current))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected three ints or nothing as arguments");

	if (start > 0)	 rd->sfra = EXPP_ClampInt (start, 1, EXPP_SCENE_FRAME_MAX);
	if (end > 0)		 rd->efra = EXPP_ClampInt (end, 1, EXPP_SCENE_FRAME_MAX);
	if (current > 0) rd->cfra = EXPP_ClampInt (current, 1, EXPP_SCENE_FRAME_MAX);

	return Py_BuildValue("(iii)", rd->sfra, rd->efra, rd->cfra);
}
