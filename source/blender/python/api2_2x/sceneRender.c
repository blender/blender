/* 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can Redistribute it and/or
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
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BIF_renderwin.h>
#include <BKE_utildefines.h>
#include <BKE_global.h>
#include <DNA_image_types.h>
#include <BIF_drawscene.h>
#include <BLI_blenlib.h>
#include <BKE_image.h>
#include <BIF_space.h>
#include <DNA_scene_types.h>
#include <DNA_space_types.h>
#include <mydevice.h>
#include <butspace.h>
#include <BKE_bad_level_calls.h>
#include "sceneRender.h"
#include "render_types.h"
#include "blendef.h"
#include "Scene.h"
#include "gen_utils.h"
#include "modules.h"

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

//local defines
#define PY_NONE		0
#define PY_LOW		1
#define PY_MEDIUM	2
#define PY_HIGH		3
#define PY_HIGHER	4
#define PY_BEST		5
#define PY_SKYDOME	1
#define PY_FULL	 2

RE_Render R;

//----------------------------------------------Render prototypes---------------------------------------------------------------
static PyObject *M_Render_CloseRenderWindow (PyObject *self);
static PyObject *M_Render_EnableDispView (PyObject *self);
static PyObject *M_Render_EnableDispWin (PyObject *self);
static PyObject *M_Render_SetRenderWinPos (PyObject *self, PyObject *args);
static PyObject *M_Render_EnableEdgeShift (PyObject *self, PyObject *args);
static PyObject *M_Render_EnableEdgeAll (PyObject *self, PyObject *args);
//----------------------------------------------Render doc strings--------------------------------------------------------------
static char M_Render_doc[] = "The Blender Render module";
//----------------------------------------------Render method def--------------------------------------------------------------
struct PyMethodDef M_Render_methods[] = {
	{"CloseRenderWindow",(PyCFunction)M_Render_CloseRenderWindow, METH_NOARGS,
				"() - close the rendering window\n"},
	{"EnableDispView",(PyCFunction)M_Render_EnableDispView, METH_NOARGS,
				"(bool) - enable Sceneing in view\n"},
	{"EnableDispWin",(PyCFunction)M_Render_EnableDispWin, METH_NOARGS,
				"(bool) - enable Sceneing in new window\n"},
	{"SetRenderWinPos",(PyCFunction)M_Render_SetRenderWinPos, METH_VARARGS,
				"([string list]) - position the rendering window in around the edge of the screen\n"}, 
	{"EnableEdgeShift",(PyCFunction)M_Render_EnableEdgeShift, METH_VARARGS,
				"(bool) - with the unified renderer the outlines are shifted a bit.\n"},
	{"EnableEdgeAll",(PyCFunction)M_Render_EnableEdgeAll, METH_VARARGS,
				"(bool) - also consider transparent faces for edge-rendering with the unified renderer\n"},
  {NULL, NULL, 0, NULL}
};
//------------------------------------BPy_RenderData methods/callbacks--------------------------------------------------
static PyObject *RenderData_Render (BPy_RenderData *self);
static PyObject *RenderData_RenderAnim (BPy_RenderData *self);
static PyObject *RenderData_Play (BPy_RenderData *self);
static PyObject *RenderData_SetRenderPath (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_GetRenderPath (BPy_RenderData *self);
static PyObject *RenderData_SetBackbufPath (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_GetBackbufPath (BPy_RenderData *self);
static PyObject *RenderData_EnableBackbuf (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetFtypePath (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_GetFtypePath (BPy_RenderData *self);
static PyObject *RenderData_EnableExtensions (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableSequencer (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableRenderDaemon (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableToonShading (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EdgeIntensity (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetEdgeColor (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_GetEdgeColor(BPy_RenderData *self);
static PyObject *RenderData_EdgeAntiShift (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableOversampling (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_SetOversamplingLevel (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableMotionBlur (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_MotionBlurLevel (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_PartsX (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_PartsY (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableSky (BPy_RenderData *self);
static PyObject *RenderData_EnablePremultiply (BPy_RenderData *self);
static PyObject *RenderData_EnableKey (BPy_RenderData *self);
static PyObject *RenderData_EnableShadow (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnablePanorama (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableEnvironmentMap (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableRayTracing (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableRadiosityRender (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_SetRenderWinSize (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableFieldRendering (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableOddFieldFirst (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableFieldTimeDisable (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableGaussFilter (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableBorderRender (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EnableGammaCorrection (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_GaussFilterSize (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_StartFrame (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_CurrentFrame (BPy_RenderData*self, PyObject *args);
static PyObject *RenderData_EndFrame (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_ImageSizeX (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_ImageSizeY (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_AspectRatioX (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_AspectRatioY (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetRenderer (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableCropping (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetImageType (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_Quality (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_FramesPerSec (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableGrayscale (BPy_RenderData *self);
static PyObject *RenderData_EnableRGBColor (BPy_RenderData *self);
static PyObject *RenderData_EnableRGBAColor (BPy_RenderData *self);
static PyObject *RenderData_SizePreset(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableUnifiedRenderer (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetYafrayGIQuality (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SetYafrayGIMethod (BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIPower(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIDepth(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGICDepth(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableYafrayGICache(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableYafrayGIPhotons(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIPhotonCount(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIPhotonRadius(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIPhotonMixCount(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableYafrayGITunePhotons(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIShadowQuality(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIPixelsPerSample(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableYafrayGIGradient(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGIRefinement(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayRayBias(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayRayDepth(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayGamma(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayExposure(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_YafrayProcessorCount(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableGameFrameStretch(BPy_RenderData *self);
static PyObject *RenderData_EnableGameFrameExpose(BPy_RenderData *self);
static PyObject *RenderData_EnableGameFrameBars(BPy_RenderData *self);
static PyObject *RenderData_SetGameFrameColor(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_GetGameFrameColor(BPy_RenderData *self);
static PyObject *RenderData_GammaLevel(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_PostProcessAdd(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_PostProcessMultiply(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_PostProcessGamma(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_SGIMaxsize(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_EnableSGICosmo(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_OldMapValue(BPy_RenderData *self, PyObject *args);
static PyObject *RenderData_NewMapValue(BPy_RenderData *self, PyObject *args);

static void RenderData_dealloc (BPy_RenderData * self);
static PyObject *RenderData_getAttr (BPy_RenderData * self, char *name);
static PyObject *RenderData_repr (BPy_RenderData * self);
//------------------------------------BPy_RenderData  method def-----------------------------------------------------------
static PyMethodDef BPy_RenderData_methods[] = {
	{"render",(PyCFunction)RenderData_Render, METH_NOARGS,
				"() - render the scene\n"},
	{"renderAnim",(PyCFunction)RenderData_RenderAnim, METH_NOARGS,
				"() - render a sequence from start frame to end frame\n"},
	{"play",(PyCFunction)RenderData_Play, METH_NOARGS,
				"() - play animation of rendered images/avi (searches Pics: field)\n"},
	{"setRenderPath",(PyCFunction)RenderData_SetRenderPath, METH_VARARGS,
				"(string) - get/set the path to output the rendered images to\n"},
	{"getRenderPath",(PyCFunction)RenderData_GetRenderPath, METH_NOARGS,
				"() - get the path to directory where rendered images will go\n"},
	{"setBackbufPath",(PyCFunction)RenderData_SetBackbufPath, METH_VARARGS,
				"(string) - get/set the path to a background image and load it\n"},
	{"getBackbufPath",(PyCFunction)RenderData_GetBackbufPath, METH_NOARGS,
				"() - get the path to background image file\n"},
	{"enableBackbuf",(PyCFunction)RenderData_EnableBackbuf, METH_VARARGS,
				"(bool) - enable/disable the backbuf image\n"},
	{"setFtypePath",(PyCFunction)RenderData_SetFtypePath, METH_VARARGS,
				"(string) - get/set the path to output the Ftype file\n"},
	{"getFtypePath",(PyCFunction)RenderData_GetFtypePath, METH_NOARGS,
				"() - get the path to Ftype file\n"},
	{"enableExtensions",(PyCFunction)RenderData_EnableExtensions, METH_VARARGS,
				"(bool) - enable/disable windows extensions for output files\n"},
	{"enableSequencer",(PyCFunction)RenderData_EnableSequencer, METH_VARARGS,
				"(bool) - enable/disable Do Sequence\n"},
	{"enableRenderDaemon",(PyCFunction)RenderData_EnableRenderDaemon, METH_VARARGS,
				"(bool) - enable/disable Scene daemon\n"},
	{"enableToonShading",(PyCFunction)RenderData_EnableToonShading, METH_VARARGS,
				"(bool) - enable/disable Edge rendering\n"},
	{"edgeIntensity",(PyCFunction)RenderData_EdgeIntensity, METH_VARARGS,
				"(int) - get/set edge intensity for toon shading\n"},
	{"setEdgeColor",(PyCFunction)RenderData_SetEdgeColor, METH_VARARGS,
				"(f,f,f) - set the edge color for toon shading - Red,Green,Blue expected.\n"},
	{"getEdgeColor",(PyCFunction)RenderData_GetEdgeColor, METH_VARARGS,
				"() - get the edge color for toon shading - Red,Green,Blue expected.\n"},
	{"edgeAntiShift",(PyCFunction)RenderData_EdgeAntiShift, METH_VARARGS,
				"(int) - with the unified renderer to reduce intensity on boundaries.\n"},
	{"enableOversampling",(PyCFunction)RenderData_EnableOversampling, METH_VARARGS,
				"(bool) - enable/disable oversampling (anit-aliasing).\n"},
	{"setOversamplingLevel",(PyCFunction)RenderData_SetOversamplingLevel, METH_VARARGS,
				"(enum) - get/set the level of oversampling (anit-aliasing).\n"},
	{"enableMotionBlur",(PyCFunction)RenderData_EnableMotionBlur, METH_VARARGS,
				"(bool) - enable/disable MBlur.\n"},
	{"motionBlurLevel",(PyCFunction)RenderData_MotionBlurLevel, METH_VARARGS,
				"(float) - get/set the length of shutter time for motion blur.\n"},
	{"partsX",(PyCFunction)RenderData_PartsX, METH_VARARGS,
				"(int) - get/set the number of parts to divide the render in the X direction\n"},
	{"partsY",(PyCFunction)RenderData_PartsY, METH_VARARGS,
				"(int) - get/set the number of parts to divide the render in the Y direction\n"},
	{"enableSky",(PyCFunction)RenderData_EnableSky, METH_NOARGS,
				"() - enable render background with sky\n"},
	{"enablePremultiply",(PyCFunction)RenderData_EnablePremultiply, METH_NOARGS,
				"() - enable premultiply alpha\n"},
	{"enableKey",(PyCFunction)RenderData_EnableKey, METH_NOARGS,
				"() - enable alpha and colour values remain unchanged\n"}, 
	{"enableShadow",(PyCFunction)RenderData_EnableShadow, METH_VARARGS,
				"(bool) - enable/disable shadow calculation\n"},
	{"enablePanorama",(PyCFunction)RenderData_EnablePanorama, METH_VARARGS,
				"(bool) - enable/disable panorama rendering (output width is multiplied by Xparts)\n"},
	{"enableEnvironmentMap",(PyCFunction)RenderData_EnableEnvironmentMap, METH_VARARGS,
				"(bool) - enable/disable environment map rendering\n"},
	{"enableRayTracing",(PyCFunction)RenderData_EnableRayTracing, METH_VARARGS,
				"(bool) - enable/disable ray tracing\n"},
	{"enableRadiosityRender",(PyCFunction)RenderData_EnableRadiosityRender, METH_VARARGS,
				"(bool) - enable/disable radiosity rendering\n"},
	{"setRenderWinSize",(PyCFunction)RenderData_SetRenderWinSize, METH_VARARGS,
				"(enum) - get/set the size of the render window\n"},
	{"enableFieldRendering",(PyCFunction)RenderData_EnableFieldRendering, METH_VARARGS,
				"(bool) - enable/disable field rendering\n"},
	{"enableOddFieldFirst",(PyCFunction)RenderData_EnableOddFieldFirst, METH_VARARGS,
				"(bool) - enable/disable Odd field first rendering (Default: Even field)\n"},
	{"enableFieldTimeDisable",(PyCFunction)RenderData_EnableFieldTimeDisable, METH_VARARGS,
				"(bool) - enable/disable time difference in field calculations\n"},
	{"enableGaussFilter",(PyCFunction)RenderData_EnableGaussFilter, METH_VARARGS,
				"(bool) - enable/disable Gauss sampling filter for antialiasing\n"},
	{"enableBorderRender",(PyCFunction)RenderData_EnableBorderRender, METH_VARARGS,
				"(bool) - enable/disable small cut-out rendering\n"},
	{"enableGammaCorrection",(PyCFunction)RenderData_EnableGammaCorrection, METH_VARARGS,
				"(bool) - enable/disable gamma correction\n"},
	{"gaussFilterSize",(PyCFunction)RenderData_GaussFilterSize, METH_VARARGS,
				"(float) - get/sets the Gauss filter size\n"},
	{"startFrame",(PyCFunction)RenderData_StartFrame, METH_VARARGS,
				"(int) - get/set the starting frame for rendering\n"},
	{"currentFrame",(PyCFunction)RenderData_CurrentFrame, METH_VARARGS,
				"(int) - get/set the current frame for rendering\n"},
	{"endFrame",(PyCFunction)RenderData_EndFrame, METH_VARARGS,
				"(int) - get/set the ending frame for rendering\n"},
	{"imageSizeX",(PyCFunction)RenderData_ImageSizeX, METH_VARARGS,
				"(int) - get/set the image width in pixels\n"},
	{"imageSizeY",(PyCFunction)RenderData_ImageSizeY, METH_VARARGS,
				"(int) - get/set the image height in pixels\n"},
	{"aspectRatioX",(PyCFunction)RenderData_AspectRatioX, METH_VARARGS,
				"(int) - get/set the horizontal aspect ratio\n"},
	{"aspectRatioY",(PyCFunction)RenderData_AspectRatioY, METH_VARARGS,
				"(int) - get/set the vertical aspect ratio\n"},
	{"setRenderer",(PyCFunction)RenderData_SetRenderer, METH_VARARGS,
				"(enum) - get/set which renderer to render the output\n"},
	{"enableCropping",(PyCFunction)RenderData_EnableCropping, METH_VARARGS,
				"(bool) - enable/disable exclusion of border rendering from total image\n"},
	{"setImageType",(PyCFunction)RenderData_SetImageType, METH_VARARGS,
				"(enum) - get/set the type of image to output from the render\n"},
	{"quality",(PyCFunction)RenderData_Quality, METH_VARARGS,
				"(int) - get/set quality get/setting for JPEG images, AVI Jpeg and SGI movies\n"},
	{"framesPerSec",(PyCFunction)RenderData_FramesPerSec, METH_VARARGS,
				"(int) - get/set frames per second\n"},
	{"enableGrayscale",(PyCFunction)RenderData_EnableGrayscale, METH_NOARGS,
				"() - images are saved with BW (grayscale) data\n"},
	{"enableRGBColor",(PyCFunction)RenderData_EnableRGBColor, METH_NOARGS,
				"() - images are saved with RGB (color) data\n"},
	{"enableRGBAColor",(PyCFunction)RenderData_EnableRGBAColor, METH_NOARGS,
				"() - images are saved with RGB and Alpha data (if supported)\n"},
	{"sizePreset",(PyCFunction)RenderData_SizePreset, METH_VARARGS,
				"(enum) - get/set the render to one of a few preget/sets\n"},
	{"enableUnifiedRenderer",(PyCFunction)RenderData_EnableUnifiedRenderer, METH_VARARGS,
				"(bool) - use the unified renderer\n"},
	{"setYafrayGIQuality",(PyCFunction)RenderData_SetYafrayGIQuality, METH_VARARGS,
				"(enum) - get/set yafray global Illumination quality\n"},
	{"setYafrayGIMethod",(PyCFunction)RenderData_SetYafrayGIMethod, METH_VARARGS,
				"(enum) - get/set yafray global Illumination method\n"},
	{"yafrayGIPower",(PyCFunction)RenderData_YafrayGIPower, METH_VARARGS,
				"(float) - get/set GI lighting intensity scale\n"},
	{"yafrayGIDepth",(PyCFunction)RenderData_YafrayGIDepth, METH_VARARGS,
			    "(int) - get/set number of bounces of the indirect light\n"},
	{"yafrayGICDepth",(PyCFunction)RenderData_YafrayGICDepth, METH_VARARGS,
				"(int) - get/set number of bounces inside objects (for caustics)\n"},
	{"enableYafrayGICache",(PyCFunction)RenderData_EnableYafrayGICache, METH_VARARGS,
				"(bool) - enable/disable cache irradiance samples (faster)\n"},
	{"enableYafrayGIPhotons",(PyCFunction)RenderData_EnableYafrayGIPhotons, METH_VARARGS,
				"(bool) - enable/disable use global photons to help in GI\n"},
	{"yafrayGIPhotonCount",(PyCFunction)RenderData_YafrayGIPhotonCount, METH_VARARGS,
				"(int) - get/set number of photons to shoot\n"},
	{"yafrayGIPhotonRadius",(PyCFunction)RenderData_YafrayGIPhotonRadius, METH_VARARGS,
				"(float) - get/set radius to search for photons to mix (blur)\n"},
	{"yafrayGIPhotonMixCount",(PyCFunction)RenderData_YafrayGIPhotonMixCount, METH_VARARGS,
				"(int) - get/set number of photons to shoot\n"},
	{"enableYafrayGITunePhotons",(PyCFunction)RenderData_EnableYafrayGITunePhotons, METH_VARARGS,
				"(bool) - enable/disable show the photonmap directly in the render for tuning\n"},
	{"yafrayGIShadowQuality",(PyCFunction)RenderData_YafrayGIShadowQuality, METH_VARARGS,
				"(float) - get/set the shadow quality, keep it under 0.95\n"},
	{"yafrayGIPixelsPerSample",(PyCFunction)RenderData_YafrayGIPixelsPerSample, METH_VARARGS,
				"(int) - get/set maximum number of pixels without samples, the lower the better and slower\n"},
	{"enableYafrayGIGradient",(PyCFunction)RenderData_EnableYafrayGIGradient, METH_VARARGS,
				"(bool) - enable/disable try to smooth lighting using a gradient\n"},
	{"yafrayGIRefinement",(PyCFunction)RenderData_YafrayGIRefinement, METH_VARARGS,
				"(float) - get/setthreshold to refine shadows EXPERIMENTAL. 1 = no refinement\n"},
	{"yafrayRayBias",(PyCFunction)RenderData_YafrayRayBias, METH_VARARGS,
				"(float) - get/set shadow ray bias to avoid self shadowing\n"},
	{"yafrayRayDepth",(PyCFunction)RenderData_YafrayRayDepth, METH_VARARGS,
				"(int) - get/set maximum render ray depth from the camera\n"},
	{"yafrayGamma",(PyCFunction)RenderData_YafrayGamma, METH_VARARGS,
				"(float) - get/set gamma correction, 1 is off\n"},
	{"yafrayExposure",(PyCFunction)RenderData_YafrayExposure, METH_VARARGS,
				"(float) - get/set exposure adjustment, 0 is off\n"},
	{"yafrayProcessorCount",(PyCFunction)RenderData_YafrayProcessorCount, METH_VARARGS,
				"(int) - get/set number of processors to use\n"},
	{"enableGameFrameStretch",(PyCFunction)RenderData_EnableGameFrameStretch, METH_NOARGS,
				"(l) - enble stretch or squeeze the viewport to fill the display window\n"},
	{"enableGameFrameExpose",(PyCFunction)RenderData_EnableGameFrameExpose, METH_NOARGS,
				"(l) - enable show the entire viewport in the display window, viewing more horizontally or vertically\n"},
	{"enableGameFrameBars",(PyCFunction)RenderData_EnableGameFrameBars, METH_NOARGS,
				"() - enable show the entire viewport in the display window, using bar horizontally or vertically\n"},
	{"setGameFrameColor",(PyCFunction)RenderData_SetGameFrameColor, METH_VARARGS,
				"(f,f,f) - set the red, green, blue component of the bars\n"},
	{"getGameFrameColor",(PyCFunction)RenderData_GetGameFrameColor, METH_VARARGS,
				"() - get the red, green, blue component of the bars\n"},
	{"gammaLevel",(PyCFunction)RenderData_GammaLevel, METH_VARARGS,
				"(float) - get/set the gamma value for blending oversampled images (1.0 = no correction\n"},
	{"postProcessAdd",(PyCFunction)RenderData_PostProcessAdd, METH_VARARGS,
				"(float) - get/set post processing add\n"},
	{"postProcessMultiply",(PyCFunction)RenderData_PostProcessMultiply, METH_VARARGS,
				"(float) - get/set post processing multiply\n"},
	{"postProcessGamma",(PyCFunction)RenderData_PostProcessGamma, METH_VARARGS,
				"(float) - get/set post processing gamma\n"},
 	{"SGIMaxsize",(PyCFunction)RenderData_SGIMaxsize, METH_VARARGS,
				"(int) - get/set maximum size per frame to save in an SGI movie\n"},
	{"enableSGICosmo",(PyCFunction)RenderData_EnableSGICosmo, METH_VARARGS,
				"(bool) - enable/disable attempt to save SGI movies using Cosmo hardware\n"},
	{"oldMapValue",(PyCFunction)RenderData_OldMapValue, METH_VARARGS,
				"(int) - get/set specify old map value in frames\n"},
	{"newMapValue",(PyCFunction)RenderData_NewMapValue, METH_VARARGS,
				"(int) - get/set specify new map value in frames\n"},
  {NULL, NULL, 0, NULL}
};
//------------------------------------BPy_RenderData Type defintion--------------------------------------------------------
PyTypeObject RenderData_Type = {
  PyObject_HEAD_INIT (NULL) 0,	  /* ob_size */
  "Blender RenderData",		                          /* tp_name */
  sizeof (BPy_RenderData),		                  /* tp_basicsize */
  0,				                                          /* tp_itemsize */
  /* methods */
  (destructor) RenderData_dealloc,	          /* tp_dealloc */
  0,				                                         /* tp_print */
  (getattrfunc) RenderData_getAttr,	         /* tp_getattr */
  0,	         /* tp_setattr */
  0,	                                                     /* tp_compare */
  (reprfunc) RenderData_repr,		             /* tp_repr */
  0,				                                         /* tp_as_number */
  0,				                                         /* tp_as_sequence */
  0,				                                         /* tp_as_mapping */
  0,				                                         /* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				                                         /* tp_doc */
  0, 0, 0, 0, 0, 0,
  BPy_RenderData_methods,		             /* tp_methods */
  0,				                                         /* tp_members */
};
//---------------------------------------------------Render Module Init--------------------------------------------------
PyObject *
Render_Init (void)
{
  PyObject *submodule;

  RenderData_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3 ("Blender.Scene.Render",
			      M_Render_methods, M_Render_doc);

  PyModule_AddIntConstant(submodule, "INTERNAL",  R_INTERN);
  PyModule_AddIntConstant(submodule, "YAFRAY",  R_YAFRAY);
  PyModule_AddIntConstant(submodule, "AVIRAW",  R_AVIRAW);
  PyModule_AddIntConstant(submodule, "AVIJPEG",  R_AVIJPEG);
  PyModule_AddIntConstant(submodule, "AVICODEC",  R_AVICODEC);
  PyModule_AddIntConstant(submodule, "QUICKTIME",  R_QUICKTIME);
  PyModule_AddIntConstant(submodule, "TARGA",  R_TARGA);
  PyModule_AddIntConstant(submodule, "RAWTGA",  R_RAWTGA);
  PyModule_AddIntConstant(submodule, "PNG",  R_PNG);
  PyModule_AddIntConstant(submodule, "BMP",  R_BMP);
  PyModule_AddIntConstant(submodule, "JPEG",  R_JPEG90);
  PyModule_AddIntConstant(submodule, "HAMX",  R_HAMX);
  PyModule_AddIntConstant(submodule, "IRIS",  R_IRIS);
  PyModule_AddIntConstant(submodule, "IRISZ",  R_IRIZ);
  PyModule_AddIntConstant(submodule, "FTYPE",  R_FTYPE);
  PyModule_AddIntConstant(submodule, "PAL",  B_PR_PAL);
  PyModule_AddIntConstant(submodule, "NTSC",  B_PR_NTSC);
  PyModule_AddIntConstant(submodule, "DEFAULT",  B_PR_PRESET);
  PyModule_AddIntConstant(submodule, "PREVIEW",  B_PR_PRV);
  PyModule_AddIntConstant(submodule, "PC",  B_PR_PC);
  PyModule_AddIntConstant(submodule, "PAL169",  B_PR_PAL169);
  PyModule_AddIntConstant(submodule, "PANO",  B_PR_PANO);
  PyModule_AddIntConstant(submodule, "FULL",  B_PR_FULL);
   PyModule_AddIntConstant(submodule, "NONE",  PY_NONE);
   PyModule_AddIntConstant(submodule, "LOW",  PY_LOW);
   PyModule_AddIntConstant(submodule, "MEDIUM",  PY_MEDIUM);
   PyModule_AddIntConstant(submodule, "HIGH",  PY_HIGH);
   PyModule_AddIntConstant(submodule, "HIGHER",  PY_HIGHER);
   PyModule_AddIntConstant(submodule, "BEST",  PY_BEST);
   PyModule_AddIntConstant(submodule, "SKYDOME",  PY_SKYDOME);
   PyModule_AddIntConstant(submodule, "GIFULL",  PY_FULL);

   return (submodule);
}
//-----------------------------------BPy_RenderData Internal Protocols---------------------------------------------------
//-------------------------------------------------dealloc-----------------------------------------------------------------
static void
RenderData_dealloc (BPy_RenderData * self)
{
    PyObject_DEL (self);
}
//-------------------------------------------------getAttr-------------------------------------------------------------------
static PyObject *
RenderData_getAttr (BPy_RenderData * self, char *name)
{
  return Py_FindMethod (BPy_RenderData_methods, (PyObject *) self, name);
}
//-------------------------------------------------repr---------------------------------------------------------------------
static PyObject *
RenderData_repr (BPy_RenderData * self)
{
  if (self->renderContext)
    return PyString_FromFormat ("[RenderData \"%s\"]", self->scene->id.name + 2);
  else
    return PyString_FromString ("NULL");
}
//------------------------------BPy_RenderData Callbacks--------------------------------------------------------------
//--------------------------------------CreatePyObject-----------------------------------------------------------------------
PyObject *
RenderData_CreatePyObject (struct Scene * scene)
{
  BPy_RenderData *py_renderdata;

  py_renderdata = (BPy_RenderData *) PyObject_NEW (BPy_RenderData, &RenderData_Type);

  if (py_renderdata == NULL) {
      return (NULL);
  }
  py_renderdata->renderContext = &scene->r;
  py_renderdata->scene = scene;

  return ((PyObject *) py_renderdata);
}
//------------------------------CheckPyObject--------------------------------------------------------------------------------
int
RenderData_CheckPyObject (PyObject * py_obj)
{
  return (py_obj->ob_type == &RenderData_Type);
}
//------------------------------------BitToggleInt---------------------------------------------------------------------------------
static PyObject *M_Render_BitToggleInt(PyObject *args, int setting, int *structure)
{
	int flag;

	if (!PyArg_ParseTuple(args, "i", &flag))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError, "expected TRUE or FALSE (1 or 0)"));

	if(flag < 0 || flag > 1)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError, "expected TRUE or FALSE (1 or 0)"));

	if(flag)
		*structure |= setting;
	else
		*structure &= ~setting;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);

}
//------------------------------------BitToggleShort---------------------------------------------------------------------------------
static PyObject *M_Render_BitToggleShort(PyObject *args, short setting, short *structure)
{
	int flag;

	if (!PyArg_ParseTuple(args, "i", &flag))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError, "expected TRUE or FALSE (1 or 0)"));

	if(flag < 0 || flag > 1)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError, "expected TRUE or FALSE (1 or 0)"));

	if(flag)
		*structure |= setting;
	else
		*structure &= ~setting;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);

}
//------------------------------------GetSetAttributeFloat------------------------------------------------------------------------
static PyObject *M_Render_GetSetAttributeFloat(PyObject *args, float *structure, float min, float max)
{
	float property = -10.0f;
	char error[48];

	if (!PyArg_ParseTuple(args, "|f", &property))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError, 	"expected float"));

	if(property != -10.0f){
		if(property < min || property > max){
			sprintf(error, "out of range - expected %f to %f", min, max);
 			return (EXPP_ReturnPyObjError (PyExc_AttributeError, error));
		}

		*structure = property;
		allqueue(REDRAWBUTSSCENE, 0);
		return EXPP_incr_ret(Py_None);
	}else
		return Py_BuildValue("f", *structure);	
}
//------------------------------------GetSetAttributeShort------------------------------------------------------------------------
static PyObject *M_Render_GetSetAttributeShort(PyObject *args, short *structure, int min, int max)
{
	short property = -10;
	char error[48];

	if (!PyArg_ParseTuple(args, "|h", &property))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(property != -10){
		if(property < min || property > max){
			sprintf(error, "out of range - expected %d to %d", min, max);
 			return (EXPP_ReturnPyObjError (PyExc_AttributeError, error));
		}

		*structure = property;
		allqueue(REDRAWBUTSSCENE, 0);
		return EXPP_incr_ret(Py_None);
	}else
		return Py_BuildValue("h", *structure);	
}
//------------------------------------GetSetAttributeInt------------------------------------------------------------------------
static PyObject *M_Render_GetSetAttributeInt(PyObject *args, int *structure, int min, int max)
{
	int property = -10;
	char error[48];

	if (!PyArg_ParseTuple(args, "|i", &property))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(property != -10){
		if(property < min || property > max){
			sprintf(error, "out of range - expected %d to %d", min, max);
 			return (EXPP_ReturnPyObjError (PyExc_AttributeError, error));
		}

		*structure = property;
		allqueue(REDRAWBUTSSCENE, 0);
		return EXPP_incr_ret(Py_None);
	}else
		return Py_BuildValue("i", *structure);	
}
//------------------------------------DoSizePrese ---------------------------------------------------------------------------------
static void M_Render_DoSizePreset(BPy_RenderData *self, short xsch, short ysch, short xasp,
									   short yasp, short size, short xparts, short yparts,
									   short frames, float a, float b, float c, float d)
{
		self->renderContext->xsch= xsch;
		self->renderContext->ysch= ysch;
		self->renderContext->xasp= xasp;
		self->renderContext->yasp= yasp;
		self->renderContext->size= size;
		self->renderContext->frs_sec= frames;
		self->renderContext->xparts= xparts;
		self->renderContext->yparts= yparts;

		BLI_init_rctf(&self->renderContext->safety, a, b, c, d);
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
}
//------------------------------------Render Module Function Definitions-----------------------------------------------
//------------------------------------Render.CloseRenderWindow() -------------------------------------------------------
PyObject *M_Render_CloseRenderWindow(PyObject *self)
{
	BIF_close_render_display();
	return EXPP_incr_ret(Py_None);
}
//------------------------------------Render.SetRenderWinPos() -------------------------------------------------------
PyObject *M_Render_SetRenderWinPos(PyObject *self, PyObject *args)
{
	PyObject *list = NULL;
	char *loc = NULL;
	int x;

	if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a list"));

	R.winpos = 0;
	for (x = 0; x < PyList_Size(list); x++) {
		if (!PyArg_Parse(PyList_GetItem(list, x), "s", &loc)){
			return EXPP_ReturnPyObjError (PyExc_TypeError, 
				"python list not parseable\n");
		}	
		if(strcmp(loc,"SW") == 0 || strcmp(loc,"sw") == 0)
			R.winpos |= 1;
		else if (strcmp(loc,"S") == 0 || strcmp(loc,"s") == 0)
			R.winpos |= 2;
 		else if (strcmp(loc,"SE") == 0 || strcmp(loc,"se") == 0)
			R.winpos |= 4;
		else if (strcmp(loc,"W") == 0 || strcmp(loc,"w") == 0)
			R.winpos |= 8;
		else if (strcmp(loc,"C") == 0 || strcmp(loc,"c") == 0)
			R.winpos |= 16;
		else if (strcmp(loc,"E") == 0 || strcmp(loc,"e") == 0)
			R.winpos |= 32;
		else if (strcmp(loc,"NW") == 0 || strcmp(loc,"nw") == 0)
			R.winpos |= 64;
		else if (strcmp(loc,"N") == 0 || strcmp(loc,"n") == 0)
			R.winpos |= 128;
		else if (strcmp(loc,"NE") == 0 || strcmp(loc,"ne") == 0)
			R.winpos |= 256;
		else
 			return EXPP_ReturnPyObjError (PyExc_AttributeError, 
				"list contains unknown string\n");
	}
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);
}
//------------------------------------Render.EnableDispView() -------------------------------------------------------------------------
PyObject *M_Render_EnableDispView(PyObject *self)
{
	R.displaymode = R_DISPLAYVIEW;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);
}
//------------------------------------Render.EnableDispWin() -------------------------------------------------------------------------
PyObject *M_Render_EnableDispWin(PyObject *self)
{
	R.displaymode = R_DISPLAYWIN;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);
}
//------------------------------------Render.EnableEdgeShift() -------------------------------------------------------------------------
PyObject *M_Render_EnableEdgeShift(PyObject *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, 1, &G.compat);
}
//------------------------------------Render.EnableEdgeAll() -------------------------------------------------------------------------
PyObject *M_Render_EnableEdgeAll(PyObject *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, 1, &G.notonlysolid);
}
//------------------------------------BPy_RenderData Function Definitions-----------------------------------------------
//------------------------------------RenderData.Render() -------------------------------------------------------------------------
PyObject *RenderData_Render(BPy_RenderData *self)
{
	Scene* oldsce;

	oldsce= G.scene;
	set_scene(self->scene);
	BIF_do_render(0);
	set_scene(oldsce);
 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.RenderAnim() ------------------------------------------------------------------
PyObject *RenderData_RenderAnim(BPy_RenderData *self)
{
	Scene* oldsce;

	oldsce= G.scene;
	set_scene(self->scene);
	BIF_do_render(1);
	set_scene(oldsce);
 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.Play() -------------------------------------------------------------------------
PyObject *RenderData_Play(BPy_RenderData *self)
{
	char file[FILE_MAXDIR+FILE_MAXFILE];
	extern char bprogname[];
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int pos[2], size[2];
	char txt[64];

#ifdef WITH_QUICKTIME
	if(self->renderContext->imtype == R_QUICKTIME){

		strcpy(file, self->renderContext->pic);
		BLI_convertstringcode(file, self->scene, self->renderContext->cfra);
 		RE_make_existing_file(file);
  		if (strcasecmp(file + strlen(file) - 4, ".mov")) {
			sprintf(txt, "%04d_%04d.mov", (self->renderContext->sfra) , 
				(self->renderContext->efra));
			strcat(file, txt);
		}
	}else
#endif
	{

		strcpy(file, self->renderContext->pic);
		BLI_convertstringcode(file, G.sce, self->renderContext->cfra);
		RE_make_existing_file(file);
  		if (strcasecmp(file + strlen(file) - 4, ".avi")) {
			sprintf(txt, "%04d_%04d.avi", (self->renderContext->sfra) , 
				(self->renderContext->efra) );
			strcat(file, txt);
		}
	}
	if(BLI_exist(file)) {
		calc_renderwin_rectangle(R.winpos, pos, size);
		sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
		system(str);
	}
	else {
		makepicstring(file, self->renderContext->sfra);
		if(BLI_exist(file)) {
			calc_renderwin_rectangle(R.winpos, pos, size);
			sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
			system(str);
		}
		else sprintf("Can't find image: %s", file);
	}

	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.StRenderPath() ---------------------------------------------------------
PyObject *RenderData_SetRenderPath(BPy_RenderData *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetRenderPath)"));

	strcpy(self->renderContext->pic, name);
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.GetRenderPath() -------------------------------------------------------
PyObject *RenderData_GetRenderPath(BPy_RenderData *self)
{
	return Py_BuildValue("s", self->renderContext->pic);
}
//------------------------------------RenderData.SetBackbufPath() -------------------------------------------------------
PyObject *RenderData_SetBackbufPath(BPy_RenderData *self, PyObject *args)
{
	char *name;
	Image *ima;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetBackbufPath)"));

	strcpy(self->renderContext->backbuf, name);
	allqueue(REDRAWBUTSSCENE, 0);

	ima= add_image(name);
	if(ima) {
		free_image_buffers(ima);	
		ima->ok= 1;
	}

 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.GetBackbufPath() --------------------------------------------------------------
PyObject *RenderData_GetBackbufPath(BPy_RenderData *self)
{
	return Py_BuildValue("s", self->renderContext->backbuf);
}
//------------------------------------RenderData.EnableBackbuf() ----------------------------------------------------------------
PyObject *RenderData_EnableBackbuf(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleShort(args, 1, &self->renderContext->bufflag);
}
//------------------------------------RenderData.SetFtypePath() ------------------------------------------------------------------
PyObject *RenderData_SetFtypePath(BPy_RenderData *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetFtypePath)"));

	strcpy(self->renderContext->ftype, name);
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.GetFtypePath() --------------------------------------------------------
PyObject *RenderData_GetFtypePath(BPy_RenderData *self)
{
	return Py_BuildValue("s", self->renderContext->ftype);
}
//------------------------------------RenderData.EnableExtensions() --------------------------------------------------
PyObject *RenderData_EnableExtensions(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleShort(args, R_EXTENSION, &self->renderContext->scemode);
}
//------------------------------------RenderData.EnableSequencer() --------------------------------------------------
PyObject *RenderData_EnableSequencer(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleShort(args, R_DOSEQ, &self->renderContext->scemode);
}
//------------------------------------RenderData.EnableRenderDaemon() ------------------------------------------
PyObject *RenderData_EnableRenderDaemon(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleShort(args, R_BG_RENDER, &self->renderContext->scemode);
}
//------------------------------------RenderData.EnableToonShading() ------------------------------------------
PyObject *RenderData_EnableToonShading(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_EDGE, &self->renderContext->mode);
}
//------------------------------------RenderData.EdgeIntensity() --------------------------------------------------------
PyObject *RenderData_EdgeIntensity(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->edgeint, 0, 255);
}
//------------------------------------RenderData.SetEdgeColor() ------------------------------------------------------
PyObject *RenderData_SetEdgeColor(BPy_RenderData *self, PyObject *args)
{
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;

	if (!PyArg_ParseTuple(args, "fff", &red, &green, &blue))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected three floats"));

	if(red < 0 || red > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (red)"));
 	if(green < 0 || green > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (green)"));
	if(blue < 0 || blue > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (blue)"));

	self->renderContext->edgeR = red;
	self->renderContext->edgeG = green;
	self->renderContext->edgeB = blue;

	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.GetEdgeColor() -----------------------------------------------------
PyObject *RenderData_GetEdgeColor(BPy_RenderData *self)
{
	char rgb[24];

	sprintf(rgb, "[%.3f,%.3f,%.3f]\n", self->renderContext->edgeR,
		self->renderContext->edgeG, self->renderContext->edgeB);
	return PyString_FromString (rgb);
}
//------------------------------------RenderData.EdgeAntiShift() -------------------------------------------------------
PyObject *RenderData_EdgeAntiShift(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->same_mat_redux, 0, 255);
}
//------------------------------------RenderData.EnableOversampling() ------------------------------------------
PyObject *RenderData_EnableOversampling(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_OSA, &self->renderContext->mode);
}
//------------------------------------RenderData.SetOversamplingLevel() ------------------------------------------
PyObject *RenderData_SetOversamplingLevel(BPy_RenderData *self, PyObject *args)
{
	int level;

	if (!PyArg_ParseTuple(args, "i", &level))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(level != 5 && level != 8 && level != 11 && level != 16)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 5,8,11, or 16"));

	self->renderContext->osa = level;
	allqueue(REDRAWBUTSSCENE, 0);
  
	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableMotionBlur() -------------------------------------------------
PyObject *RenderData_EnableMotionBlur(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_MBLUR, &self->renderContext->mode);
}
//------------------------------------RenderData.MotionBlurLevel() ----------------------------------------------------
PyObject *RenderData_MotionBlurLevel(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &self->renderContext->blurfac, 0.01f, 5.0f);
}
//------------------------------------RenderData.PartsX() ------------------------------------------------------------
PyObject *RenderData_PartsX(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->xparts, 1, 64);
}
//------------------------------------RenderData.PartsY() -------------------------------------------------------------
PyObject *RenderData_PartsY(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->yparts, 1, 64);
}
//------------------------------------RenderData.EnableSky() ------------------------------------------------------
PyObject *RenderData_EnableSky(BPy_RenderData *self)
{
	self->renderContext->alphamode = R_ADDSKY;
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnablePremultiply() ------------------------------------------
PyObject *RenderData_EnablePremultiply(BPy_RenderData *self)
{
	self->renderContext->alphamode = R_ALPHAPREMUL;
	allqueue(REDRAWBUTSSCENE, 0);

  	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableKey() ----------------------------------------------------
PyObject *RenderData_EnableKey(BPy_RenderData *self)
{
	self->renderContext->alphamode = R_ALPHAKEY;
	allqueue(REDRAWBUTSSCENE, 0);

   	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableShadow() ------------------------------------------
PyObject *RenderData_EnableShadow(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_SHADOW, &self->renderContext->mode);
}
//------------------------------------RenderData.EnvironmentMap() ------------------------------------------
PyObject *RenderData_EnableEnvironmentMap(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_ENVMAP, &self->renderContext->mode);
}
//------------------------------------RenderData.EnablePanorama() ------------------------------------------
PyObject *RenderData_EnablePanorama(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_PANORAMA, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableRayTracing() ------------------------------------------
PyObject *RenderData_EnableRayTracing(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_RAYTRACE, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableRadiosityRender() ------------------------------------------
PyObject *RenderData_EnableRadiosityRender(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_RADIO, &self->renderContext->mode);
}
//------------------------------------RenderData.SetRenderWinSize() ------------------------------------------
PyObject *RenderData_SetRenderWinSize(BPy_RenderData *self, PyObject *args)
{
	int size;

	if (!PyArg_ParseTuple(args, "i", &size))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(size != 25 && size != 50 && size != 75 && size != 100)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 25, 50, 75, or 100"));

	self->renderContext->size = size;
	allqueue(REDRAWBUTSSCENE, 0);
  
	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableFieldRendering() ------------------------------------------
PyObject *RenderData_EnableFieldRendering(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_FIELDS, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableOddFieldFirst() ------------------------------------------
PyObject *RenderData_EnableOddFieldFirst(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_ODDFIELD, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableFieldTimeDisable() ------------------------------------------
PyObject *RenderData_EnableFieldTimeDisable(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_FIELDSTILL, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableGaussFilter() ------------------------------------------
PyObject *RenderData_EnableGaussFilter(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_GAUSS, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableBorderRender() ------------------------------------------
PyObject *RenderData_EnableBorderRender(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_BORDER, &self->renderContext->mode);
}
//------------------------------------RenderData.EnableGammaCorrection() ------------------------------------------
PyObject *RenderData_EnableGammaCorrection(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_GAMMA, &self->renderContext->mode);
}
//------------------------------------RenderData.GaussFilterSize() ------------------------------------------------------
PyObject *RenderData_GaussFilterSize(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &self->renderContext->gauss, 0.5f, 1.5f);
}
//------------------------------------RenderData.StartFrame() --------------------------------------------------------
PyObject *RenderData_StartFrame(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->sfra, 1, 18000);
}
//------------------------------------RenderData.CurrentFrame() --------------------------------------------------------
PyObject *RenderData_CurrentFrame(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->cfra, 1, 18000);
}
//------------------------------------RenderData.EndFrame() ---------------------------------------------------------
PyObject *RenderData_EndFrame(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->efra, 1, 18000);
}
 //------------------------------------RenderData.ImageSizeX() ---------------------------------------------------------
PyObject *RenderData_ImageSizeX(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->xsch, 4, 10000);
}
//------------------------------------RenderData.ImageSizeY() -------------------------------------------------------
PyObject *RenderData_ImageSizeY(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->ysch, 4, 10000);
}
//------------------------------------RenderData.AspectRatioX() ----------------------------------------------------
PyObject *RenderData_AspectRatioX(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->xasp, 1, 200);
}
//------------------------------------RenderData.AspectRatioY() ----------------------------------------------------
PyObject *RenderData_AspectRatioY(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->yasp, 1, 200);
}
//------------------------------------RenderData.SetRenderer() ----------------------------------------------------
PyObject *RenderData_SetRenderer(BPy_RenderData *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant INTERN or YAFRAY"));

	if(type == R_INTERN)
		self->renderContext->renderer = R_INTERN;
	else if (type == R_YAFRAY)
		self->renderContext->renderer = R_YAFRAY;
	else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected INTERN or YAFRAY"));

	allqueue(REDRAWBUTSSCENE, 0);
 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableCropping() ----------------------------------------------------
PyObject *RenderData_EnableCropping(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_MOVIECROP, &self->renderContext->mode);
}
//------------------------------------RenderData.SetImageType() ----------------------------------------------------
PyObject *RenderData_SetImageType(BPy_RenderData *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if(type == R_AVIRAW)
		self->renderContext->imtype = R_AVIRAW;
	else if (type == R_AVIJPEG)
		self->renderContext->imtype = R_AVIJPEG;
#ifdef _WIN32
	else if (type == R_AVICODEC)
		self->renderContext->imtype = R_AVICODEC;
#endif
	else if (type == R_QUICKTIME && G.have_quicktime)
		self->renderContext->imtype = R_QUICKTIME;
	else if (type == R_TARGA)
		self->renderContext->imtype = R_TARGA;
	else if (type == R_RAWTGA)
		self->renderContext->imtype = R_RAWTGA;
	else if (type == R_PNG)
		self->renderContext->imtype = R_PNG;
	else if (type == R_BMP)
		self->renderContext->imtype = R_BMP;
	else if (type == R_JPEG90)
		self->renderContext->imtype = R_JPEG90;
	else if (type == R_HAMX)
		self->renderContext->imtype = R_HAMX;
	else if (type == R_IRIS)
		self->renderContext->imtype = R_IRIS;
	else if (type == R_IRIZ)
		self->renderContext->imtype = R_IRIZ;
	else if (type == R_FTYPE)
		self->renderContext->imtype = R_FTYPE;
	else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.Quality() ----------------------------------------------------
PyObject *RenderData_Quality(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->quality, 10, 100);
}
//------------------------------------RenderData.FramesPerSec() ----------------------------------------------------
PyObject *RenderData_FramesPerSec(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->frs_sec, 1, 120);
}
//------------------------------------RenderData.EnableGrayscale() ----------------------------------------------------
PyObject *RenderData_EnableGrayscale(BPy_RenderData *self)
{
	self->renderContext->planes = R_PLANESBW;
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableRGBColor() ----------------------------------------------------
PyObject *RenderData_EnableRGBColor(BPy_RenderData *self)
{
	self->renderContext->planes = R_PLANES24;
	allqueue(REDRAWBUTSSCENE, 0);

  	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableRGBAColor() ----------------------------------------------------
PyObject *RenderData_EnableRGBAColor(BPy_RenderData *self)
{
	self->renderContext->planes = R_PLANES32;
	allqueue(REDRAWBUTSSCENE, 0);

   	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.SizePreset() ----------------------------------------------------------------
PyObject *RenderData_SizePreset(BPy_RenderData *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if(type == B_PR_PAL){
		M_Render_DoSizePreset(self,720,576,54,51,100, self->renderContext->xparts,
					  self->renderContext->yparts, 25, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode &= ~R_PANORAMA;
	}else if (type == B_PR_NTSC){
		M_Render_DoSizePreset(self,720,480,10,11,100, 1, 1, 
			30, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode &= ~R_PANORAMA;
	}else if (type == B_PR_PRESET){
		M_Render_DoSizePreset(self,720,576,54,51,100, 1, 1, 
			self->renderContext->frs_sec, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode= R_OSA+R_SHADOW+R_FIELDS;
		self->renderContext->imtype= R_TARGA;
	}else if (type == B_PR_PRV){
		M_Render_DoSizePreset(self,640,512,1,1,50, 1, 1, 
			self->renderContext->frs_sec, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode &= ~R_PANORAMA;
	}else if (type == B_PR_PC){
		M_Render_DoSizePreset(self,640,480,100,100,100, 1, 1, 
			self->renderContext->frs_sec, 0.0, 1.0, 0.0, 1.0);
		self->renderContext->mode &= ~R_PANORAMA;
	}else if (type == B_PR_PAL169){
		M_Render_DoSizePreset(self,720,576,64,45,100, 1, 1, 
			25, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode &= ~R_PANORAMA;
	}else if (type == B_PR_PANO){
		M_Render_DoSizePreset(self,36,176,115,100,100, 16, 1, 
			self->renderContext->frs_sec, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode |= R_PANORAMA;
	}else if (type == B_PR_FULL){
		M_Render_DoSizePreset(self,1280,1024,1,1,100, 1, 1, 
			self->renderContext->frs_sec, 0.1, 0.9, 0.1, 0.9);
		self->renderContext->mode &= ~R_PANORAMA;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableUnifiedRenderer() ----------------------------------------------------
PyObject *RenderData_EnableUnifiedRenderer(BPy_RenderData *self, PyObject *args)
{
	return M_Render_BitToggleInt(args, R_UNIFIED, &self->renderContext->mode);
}
//------------------------------------RenderData.SetYafrayGIQuality() ----------------------------------------------------
PyObject *RenderData_SetYafrayGIQuality(BPy_RenderData *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if( type == PY_NONE   || type == PY_LOW  ||
		type == PY_MEDIUM || type == PY_HIGH ||
		type == PY_HIGHER || type == PY_BEST){
		self->renderContext->GIquality = type;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.SetYafrayGIMethod() ----------------------------------------------------
PyObject *RenderData_SetYafrayGIMethod(BPy_RenderData *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if( type == PY_NONE   || type == PY_SKYDOME  || type == PY_FULL){
		self->renderContext->GImethod = type;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.YafrayGIPower() ----------------------------------------------------
PyObject *RenderData_YafrayGIPower(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod>0) {
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->GIpower, 0.01f, 100.00f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'SKYDOME' or 'FULL'"));
}
//------------------------------------RenderData.YafrayGIDepth() ----------------------------------------------------
PyObject *RenderData_YafrayGIDepth(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2) {
		return M_Render_GetSetAttributeInt(args, &self->renderContext->GIdepth, 1, 8);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}
//------------------------------------RenderData.afrayGICDepth() ----------------------------------------------------
PyObject *RenderData_YafrayGICDepth(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2) {
		return M_Render_GetSetAttributeInt(args, &self->renderContext->GIcausdepth, 1, 8);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}
//------------------------------------RenderData.EnableYafrayGICache() ----------------------------------------------------
PyObject *RenderData_EnableYafrayGICache(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2) {
		return  M_Render_BitToggleShort(args, 1, &self->renderContext->GIcache);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}
//------------------------------------RenderData.EnableYafrayGIPhotons() ----------------------------------------------------
PyObject *RenderData_EnableYafrayGIPhotons(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2) {
		return  M_Render_BitToggleShort(args, 1, &self->renderContext->GIphotons);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}
//------------------------------------RenderData.YafrayGIPhotonCount() ----------------------------------------------------
PyObject *RenderData_YafrayGIPhotonCount(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIphotons==1) {
		return M_Render_GetSetAttributeInt(args, &self->renderContext->GIphotoncount, 0, 10000000);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}
//------------------------------------RenderData.YafrayGIPhotonRadius() ----------------------------------------------------
PyObject *RenderData_YafrayGIPhotonRadius(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIphotons==1) {
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->GIphotonradius, 0.00001f, 100.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}
//------------------------------------RenderData.YafrayGIPhotonMixCount() ----------------------------------------------------
PyObject *RenderData_YafrayGIPhotonMixCount(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIphotons==1) {
		return M_Render_GetSetAttributeInt(args, &self->renderContext->GImixphotons, 0, 1000);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}
//------------------------------------RenderData.EnableYafrayGITunePhotons() ----------------------------------------------------
PyObject *RenderData_EnableYafrayGITunePhotons(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIphotons==1) {
		return  M_Render_BitToggleShort(args, 1, &self->renderContext->GIdirect);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled"));
}
//------------------------------------RenderData.YafrayGIShadowQuality() ----------------------------------------------------
PyObject *RenderData_YafrayGIShadowQuality(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIcache==1) {
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->GIshadowquality, 0.01f, 1.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}
//------------------------------------RenderData.YafrayGIPixelsPerSample() ----------------------------------------------------
PyObject *RenderData_YafrayGIPixelsPerSample(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIcache==1) {
		return M_Render_GetSetAttributeInt(args, &self->renderContext->GIpixelspersample, 1, 50);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}
//------------------------------------RenderData.EnableYafrayGIGradient() ----------------------------------------------------
PyObject *RenderData_EnableYafrayGIGradient(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIcache==1) {
		return  M_Render_BitToggleShort(args, 1, &self->renderContext->GIgradient);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled"));
}
//------------------------------------RenderData.YafrayGIRefinement() ----------------------------------------------------
PyObject *RenderData_YafrayGIRefinement(BPy_RenderData *self, PyObject *args)
{
	if (self->renderContext->GImethod==2 && self->renderContext->GIcache==1) {
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->GIrefinement, 0.001f, 1.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}
//------------------------------------RenderData.YafrayRayBias() -------------------------------------------------------------
PyObject *RenderData_YafrayRayBias(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &self->renderContext->YF_raybias, 0.0f, 10.0f);
}
//------------------------------------RenderData.YafrayRayDepth() -----------------------------------------------------------
PyObject *RenderData_YafrayRayDepth(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeInt(args, &self->renderContext->YF_raydepth, 1, 80);
}
//------------------------------------RenderData.YafrayGamma() -----------------------------------------------------------
PyObject *RenderData_YafrayGamma(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &self->renderContext->YF_gamma, 0.001f, 5.0f);
}
//------------------------------------RenderData.YafrayExposure() -----------------------------------------------------------
PyObject *RenderData_YafrayExposure(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &self->renderContext->YF_exposure, 0.0f, 10.0f);
}
//------------------------------------RenderData.YafrayProcessorCount() -----------------------------------------------------------
PyObject *RenderData_YafrayProcessorCount(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeInt(args, &self->renderContext->YF_numprocs, 1, 8);
}
//------------------------------------RenderData.EnableGameFrameStretch() -----------------------------------------------------------
PyObject *RenderData_EnableGameFrameStretch(BPy_RenderData *self)
{
	self->scene->framing.type = SCE_GAMEFRAMING_SCALE;
 	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableGameFrameExpose() -----------------------------------------------------------
PyObject *RenderData_EnableGameFrameExpose(BPy_RenderData *self)
{
	self->scene->framing.type = SCE_GAMEFRAMING_EXTEND;
  	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.EnableGameFrameBars() -----------------------------------------------------------
PyObject *RenderData_EnableGameFrameBars(BPy_RenderData *self)
{
	self->scene->framing.type = SCE_GAMEFRAMING_BARS;
   	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.SetGameFrameColor() -----------------------------------------------------------
PyObject *RenderData_SetGameFrameColor(BPy_RenderData *self, PyObject *args)
{
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;

	if (!PyArg_ParseTuple(args, "fff", &red, &green, &blue))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected three floats"));

	if(red < 0 || red > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (red)"));
 	if(green < 0 || green > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (green)"));
	if(blue < 0 || blue > 1)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"value out of range 0.000 - 1.000 (blue)"));

	self->scene->framing.col[0] = red;
	self->scene->framing.col[1] = green;
	self->scene->framing.col[2] = blue;

	return EXPP_incr_ret(Py_None);
}
//------------------------------------RenderData.GetGameFrameColor() -----------------------------------------------------------
PyObject *RenderData_GetGameFrameColor(BPy_RenderData *self)
{
	char rgb[24];

	sprintf(rgb, "[%.3f,%.3f,%.3f]\n", self->scene->framing.col[0],
		self->scene->framing.col[1], self->scene->framing.col[2]);
	return PyString_FromString (rgb);
}
//------------------------------------RenderData.GammaLevel() -----------------------------------------------------------
PyObject *RenderData_GammaLevel(BPy_RenderData *self, PyObject *args)
{
	if(self->renderContext->mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->gamma, 0.2f, 5.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}
//------------------------------------RenderData.PostProcessAdd() -----------------------------------------------------------
PyObject *RenderData_PostProcessAdd(BPy_RenderData *self, PyObject *args)
{
	if(self->renderContext->mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->postadd, -1.0f, 1.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}
//------------------------------------RenderData.PostProcessMultiply() -----------------------------------------------------------
PyObject *RenderData_PostProcessMultiply(BPy_RenderData *self, PyObject *args)
{
	if(self->renderContext->mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->postmul, 0.01f, 4.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}
//------------------------------------RenderData.PostProcessGamma() -----------------------------------------------------------
PyObject *RenderData_PostProcessGamma(BPy_RenderData *self, PyObject *args)
{
	if(self->renderContext->mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &self->renderContext->postgamma, 0.2f, 2.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}
//------------------------------------RenderData.SGIMaxsize() -----------------------------------------------------------
PyObject *RenderData_SGIMaxsize(BPy_RenderData *self, PyObject *args)
{
#ifdef __sgi
	return M_Render_GetSetAttributeShort(args, &self->renderContext->maximsize, 0, 500);
#else
  	return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"SGI is not defined on this machine"));
#endif
}
//------------------------------------RenderData.EnableSGICosmo() -----------------------------------------------------------
PyObject *RenderData_EnableSGICosmo(BPy_RenderData *self, PyObject *args)
{
#ifdef __sgi
	return  M_Render_BitToggleInt(args, R_COSMO, &self->renderContext->mode);
#else
  	return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"SGI is not defined on this machine"));
#endif
}
//------------------------------------RenderData.OldMapValue() -----------------------------------------------------------
PyObject *RenderData_OldMapValue(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->framapto, 1, 900);
}
//------------------------------------RenderData.NewMapValue() -----------------------------------------------------------
PyObject *RenderData_NewMapValue(BPy_RenderData *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &self->renderContext->images, 1, 900);
}



