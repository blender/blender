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

#ifndef EXPP_SCENERENDER_H
#define EXPP_SCENERENDER_H

#include <Python.h>
#include "mydevice.h"
#include "render_types.h"
#include "blendef.h"
#include "Scene.h"
#include "BIF_renderwin.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "BIF_drawscene.h"
#include "BLI_blenlib.h"
#include "BKE_image.h"
#include "BIF_space.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//local defines
#define R_PAL		1608
#define R_FULL		1609
#define R_PREVIEW	1610
#define R_PAL169	1612
#define R_DEFAULT	1618
#define R_PANO		1619
#define R_NTSC		1620
#define R_PC		1624
#define PY_NONE		0
#define PY_LOW		1
#define PY_MEDIUM	2
#define PY_HIGH		3
#define PY_HIGHER	4
#define PY_BEST		5
#define PY_SKYDOME	1
#define PY_GIFULL	2

/*****************************************************************************/
// Python API function prototypes for the Render module.												
/*****************************************************************************/
PyObject *M_Render_Render (PyObject *self);
PyObject *M_Render_RenderAnim (PyObject *self);
PyObject *M_Render_CloseRenderWindow (PyObject *self);
PyObject *M_Render_Play (PyObject *self);
PyObject *M_Render_SetRenderPath (PyObject *self, PyObject *args);
PyObject *M_Render_GetRenderPath (PyObject *self);
PyObject *M_Render_SetBackbufPath (PyObject *self, PyObject *args);
PyObject *M_Render_GetBackbufPath (PyObject *self);
PyObject *M_Render_EnableBackbuf (PyObject *self, PyObject *args);
PyObject *M_Render_SetFtypePath (PyObject *self, PyObject *args);
PyObject *M_Render_GetFtypePath (PyObject *self);
PyObject *M_Render_EnableExtensions (PyObject *self, PyObject *args);
PyObject *M_Render_EnableSequencer (PyObject *self, PyObject *args);
PyObject *M_Render_EnableRenderDaemon (PyObject *self, PyObject *args);
PyObject *M_Render_SetRenderWinPos (PyObject *self, PyObject *args);
PyObject *M_Render_EnableDispView (PyObject *self);
PyObject *M_Render_EnableDispWin (PyObject *self);
PyObject *M_Render_EnableToonShading (PyObject *self, PyObject *args);
PyObject *M_Render_EdgeIntensity (PyObject *self, PyObject *args);
PyObject *M_Render_EnableEdgeShift (PyObject *self, PyObject *args);
PyObject *M_Render_EnableEdgeAll (PyObject *self, PyObject *args);
PyObject *M_Render_SetEdgeColor (PyObject *self, PyObject *args);
PyObject *M_Render_GetEdgeColor(PyObject *self);
PyObject *M_Render_EdgeAntiShift (PyObject *self, PyObject *args);
PyObject *M_Render_EnableOversampling (PyObject *self, PyObject *args);
PyObject *M_Render_SetOversamplingLevel (PyObject *self, PyObject *args);
PyObject *M_Render_EnableMotionBlur (PyObject *self, PyObject *args);
PyObject *M_Render_MotionBlurLevel (PyObject *self, PyObject *args);
PyObject *M_Render_PartsX (PyObject *self, PyObject *args);
PyObject *M_Render_PartsY (PyObject *self, PyObject *args);
PyObject *M_Render_EnableSky (PyObject *self);
PyObject *M_Render_EnablePremultiply (PyObject *self);
PyObject *M_Render_EnableKey (PyObject *self);
PyObject *M_Render_EnableShadow (PyObject *self, PyObject *args);
PyObject *M_Render_EnablePanorama (PyObject *self, PyObject *args);
PyObject *M_Render_EnableEnvironmentMap (PyObject *self, PyObject *args);
PyObject *M_Render_EnableRayTracing (PyObject *self, PyObject *args);
PyObject *M_Render_EnableRadiosityRender (PyObject *self, PyObject *args);
PyObject *M_Render_SetRenderWinSize (PyObject *self, PyObject *args);
PyObject *M_Render_EnableFieldRendering (PyObject *self, PyObject *args);
PyObject *M_Render_EnableOddFieldFirst (PyObject *self, PyObject *args);
PyObject *M_Render_EnableFieldTimeDisable (PyObject *self, PyObject *args);
PyObject *M_Render_EnableGaussFilter (PyObject *self, PyObject *args);
PyObject *M_Render_EnableBorderRender (PyObject *self, PyObject *args);
PyObject *M_Render_EnableGammaCorrection (PyObject *self, PyObject *args);
PyObject *M_Render_GaussFilterSize (PyObject *self, PyObject *args);
PyObject *M_Render_StartFrame (PyObject *self, PyObject *args);
PyObject *M_Render_EndFrame (PyObject *self, PyObject *args);
PyObject *M_Render_ImageSizeX (PyObject *self, PyObject *args);
PyObject *M_Render_ImageSizeY (PyObject *self, PyObject *args);
PyObject *M_Render_AspectRatioX (PyObject *self, PyObject *args);
PyObject *M_Render_AspectRatioY (PyObject *self, PyObject *args);
PyObject *M_Render_SetRenderer (PyObject *self, PyObject *args);
PyObject *M_Render_EnableCropping (PyObject *self, PyObject *args);
PyObject *M_Render_SetImageType (PyObject *self, PyObject *args);
PyObject *M_Render_Quality (PyObject *self, PyObject *args);
PyObject *M_Render_FramesPerSec (PyObject *self, PyObject *args);
PyObject *M_Render_EnableGrayscale (PyObject *self);
PyObject *M_Render_EnableRGBColor (PyObject *self);
PyObject *M_Render_EnableRGBAColor (PyObject *self);
PyObject *M_Render_SizePreset(PyObject *self, PyObject *args);
PyObject *M_Render_EnableUnifiedRenderer (PyObject *self, PyObject *args);
PyObject *M_Render_SetYafrayGIQuality (PyObject *self, PyObject *args);
PyObject *M_Render_SetYafrayGIMethod (PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIPower(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIDepth(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGICDepth(PyObject *self, PyObject *args);
PyObject *M_Render_EnableYafrayGICache(PyObject *self, PyObject *args);
PyObject *M_Render_EnableYafrayGIPhotons(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIPhotonCount(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIPhotonRadius(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIPhotonMixCount(PyObject *self, PyObject *args);
PyObject *M_Render_EnableYafrayGITunePhotons(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIShadowQuality(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIPixelsPerSample(PyObject *self, PyObject *args);
PyObject *M_Render_EnableYafrayGIGradient(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGIRefinement(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayRayBias(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayRayDepth(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayGamma(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayExposure(PyObject *self, PyObject *args);
PyObject *M_Render_YafrayProcessorCount(PyObject *self, PyObject *args);
PyObject *M_Render_EnableGameFrameStretch(PyObject *self);
PyObject *M_Render_EnableGameFrameExpose(PyObject *self);
PyObject *M_Render_EnableGameFrameBars(PyObject *self);
PyObject *M_Render_SetGameFrameColor(PyObject *self, PyObject *args);
PyObject *M_Render_GetGameFrameColor(PyObject *self);
PyObject *M_Render_GammaLevel(PyObject *self, PyObject *args);
PyObject *M_Render_PostProcessAdd(PyObject *self, PyObject *args);
PyObject *M_Render_PostProcessMultiply(PyObject *self, PyObject *args);
PyObject *M_Render_PostProcessGamma(PyObject *self, PyObject *args);
PyObject *M_Render_SGIMaxsize(PyObject *self, PyObject *args);
PyObject *M_Render_EnableSGICosmo(PyObject *self, PyObject *args);
PyObject *M_Render_OldMapValue(PyObject *self, PyObject *args);
PyObject *M_Render_NewMapValue(PyObject *self, PyObject *args);

#endif /* EXPP_SCENERENDER_H */
