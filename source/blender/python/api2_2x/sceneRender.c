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

#include "sceneRender.h"

RE_Render R;


/*****************************************************************************/
// Python funciton definitions for Render module		
/*****************************************************************************/

PyObject *M_Render_Render(PyObject *self)
{
	Scene* oldsce;

	oldsce= G.scene;
	set_scene(((BPy_Scene*)self)->scene);
	BIF_do_render(0);
	set_scene(oldsce);
 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_RenderAnim(PyObject *self)
{
	Scene* oldsce;

	oldsce= G.scene;
	set_scene(((BPy_Scene*)self)->scene);
	BIF_do_render(1);
	set_scene(oldsce);
 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_CloseRenderWindow(PyObject *self)
{
	BIF_close_render_display();
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_Play(PyObject *self)
{
	char file[FILE_MAXDIR+FILE_MAXFILE];
	extern char bprogname[];
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int pos[2], size[2];
	char txt[64];

#ifdef WITH_QUICKTIME
	if(((BPy_Scene*)self)->scene->r.imtype == R_QUICKTIME){

		strcpy(file, ((BPy_Scene*)self)->scene->r.pic);
		BLI_convertstringcode(file, ((BPy_Scene*)self)->scene, ((BPy_Scene*)self)->scene->r.cfra);
 		RE_make_existing_file(file);
  		if (strcasecmp(file + strlen(file) - 4, ".mov")) {
			sprintf(txt, "%04d_%04d.mov", (((BPy_Scene*)self)->scene->r.sfra) , 
				(((BPy_Scene*)self)->scene->r.efra));
			strcat(file, txt);
		}
	}else
#endif
	{

		strcpy(file, ((BPy_Scene*)self)->scene->r.pic);
		BLI_convertstringcode(file, G.sce, ((BPy_Scene*)self)->scene->r.cfra);
		RE_make_existing_file(file);
  		if (strcasecmp(file + strlen(file) - 4, ".avi")) {
			sprintf(txt, "%04d_%04d.avi", (((BPy_Scene*)self)->scene->r.sfra) , 
				(((BPy_Scene*)self)->scene->r.efra) );
			strcat(file, txt);
		}
	}
	if(BLI_exist(file)) {
		calc_renderwin_rectangle(R.winpos, pos, size);
		sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
		system(str);
	}
	else {
		makepicstring(file, ((BPy_Scene*)self)->scene->r.sfra);
		if(BLI_exist(file)) {
			calc_renderwin_rectangle(R.winpos, pos, size);
			sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
			system(str);
		}
		else sprintf("Can't find image: %s", file);
	}

	return EXPP_incr_ret(Py_None);
}

static PyObject *M_Render_BitToggleInt(PyObject *args, int setting, int *structure)
{
	int flag;

	if (!PyArg_ParseTuple(args, "i", &flag))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(flag < 0 || flag > 1)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 0 or 1"));

	if(flag)
		*structure |= setting;
	else
		*structure &= ~setting;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);

}

static PyObject *M_Render_BitToggleShort(PyObject *args, short setting, short *structure)
{
	int flag;

	if (!PyArg_ParseTuple(args, "i", &flag))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(flag < 0 || flag > 1)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 0 or 1"));

	if(flag)
		*structure |= setting;
	else
		*structure &= ~setting;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);

}

static PyObject *M_Render_GetSetAttributeFloat(PyObject *args, float *structure, float min, float max)
{
	float property = -10.0f;
	char error[48];

	if (!PyArg_ParseTuple(args, "|f", &property))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected float"));

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

PyObject *M_Render_SetRenderPath(PyObject *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetRenderPath)"));

	strcpy(((BPy_Scene*)self)->scene->r.pic, name);
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GetRenderPath(PyObject *self)
{
	return Py_BuildValue("s", ((BPy_Scene*)self)->scene->r.pic);
}

PyObject *M_Render_SetBackbufPath(PyObject *self, PyObject *args)
{
	char *name;
	Image *ima;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetBackbufPath)"));

	strcpy(((BPy_Scene*)self)->scene->r.backbuf, name);
	allqueue(REDRAWBUTSSCENE, 0);

	ima= add_image(name);
	if(ima) {
		free_image_buffers(ima);	
		ima->ok= 1;
	}

 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GetBackbufPath(PyObject *self)
{
	return Py_BuildValue("s", ((BPy_Scene*)self)->scene->r.backbuf);
}

PyObject *M_Render_EnableBackbuf(PyObject *self, PyObject *args)
{
	M_Render_BitToggleShort(args, 1, &((BPy_Scene*)self)->scene->r.bufflag);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetFtypePath(PyObject *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string"));

	if(strlen(name) > 160)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"path is too long (SetFtypePath)"));

	strcpy(((BPy_Scene*)self)->scene->r.ftype, name);
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GetFtypePath(PyObject *self)
{
	return Py_BuildValue("s", ((BPy_Scene*)self)->scene->r.ftype);
}


PyObject *M_Render_EnableExtensions(PyObject *self, PyObject *args)
{
	M_Render_BitToggleShort(args, R_EXTENSION, &((BPy_Scene*)self)->scene->r.scemode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableSequencer(PyObject *self, PyObject *args)
{
	M_Render_BitToggleShort(args, R_DOSEQ, &((BPy_Scene*)self)->scene->r.scemode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableRenderDaemon(PyObject *self, PyObject *args)
{
	M_Render_BitToggleShort(args, R_BG_RENDER, &((BPy_Scene*)self)->scene->r.scemode);
	return EXPP_incr_ret(Py_None);
}

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

PyObject *M_Render_EnableDispView(PyObject *self)
{
	R.displaymode = R_DISPLAYVIEW;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableDispWin(PyObject *self)
{
	R.displaymode = R_DISPLAYWIN;
	allqueue(REDRAWBUTSSCENE, 0);

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableToonShading(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_EDGE, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EdgeIntensity(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.edgeint, 0, 255);
}

PyObject *M_Render_EnableEdgeShift(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, 1, &G.compat);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableEdgeAll(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, 1, &G.notonlysolid);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetEdgeColor(PyObject *self, PyObject *args)
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

	((BPy_Scene*)self)->scene->r.edgeR = red;
	((BPy_Scene*)self)->scene->r.edgeG = green;
	((BPy_Scene*)self)->scene->r.edgeB = blue;

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GetEdgeColor(PyObject *self)
{
	char rgb[24];

	sprintf(rgb, "[%.3f,%.3f,%.3f]\n", ((BPy_Scene*)self)->scene->r.edgeR,
		((BPy_Scene*)self)->scene->r.edgeG, ((BPy_Scene*)self)->scene->r.edgeB);
	return PyString_FromString (rgb);
}

PyObject *M_Render_EdgeAntiShift(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.same_mat_redux, 0, 255);
}

PyObject *M_Render_EnableOversampling(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_OSA, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetOversamplingLevel(PyObject *self, PyObject *args)
{
	int level;

	if (!PyArg_ParseTuple(args, "i", &level))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(level != 5 && level != 8 && level != 11 && level != 16)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 5,8,11, or 16"));

	((BPy_Scene*)self)->scene->r.osa = level;
	allqueue(REDRAWBUTSSCENE, 0);
  
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableMotionBlur(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_MBLUR, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_MotionBlurLevel(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.blurfac, 0.01f, 5.0f);
}

PyObject *M_Render_PartsX(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.xparts, 1, 64);
}

PyObject *M_Render_PartsY(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.yparts, 1, 64);
}

PyObject *M_Render_EnableSky(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.alphamode = R_ADDSKY;
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnablePremultiply(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.alphamode = R_ALPHAPREMUL;
	allqueue(REDRAWBUTSSCENE, 0);

  	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableKey(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.alphamode = R_ALPHAKEY;
	allqueue(REDRAWBUTSSCENE, 0);

   	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableShadow(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_SHADOW, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableEnvironmentMap(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_ENVMAP, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnablePanorama(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_PANORAMA, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableRayTracing(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_RAYTRACE, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableRadiosityRender(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_RADIO, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetRenderWinSize(PyObject *self, PyObject *args)
{
	int size;

	if (!PyArg_ParseTuple(args, "i", &size))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected int"));

	if(size != 25 && size != 50 && size != 75 && size != 100)
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected 25, 50, 75, or 100"));

	((BPy_Scene*)self)->scene->r.size = size;
	allqueue(REDRAWBUTSSCENE, 0);
  
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableFieldRendering(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_FIELDS, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableOddFieldFirst(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_ODDFIELD, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableFieldTimeDisable(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_FIELDSTILL, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableGaussFilter(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_GAUSS, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableBorderRender(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_BORDER, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableGammaCorrection(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_GAMMA, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GaussFilterSize(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.gauss, 0.5f, 1.5f);
}

PyObject *M_Render_StartFrame(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.sfra, 1, 18000);
}

PyObject *M_Render_EndFrame(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.efra, 1, 18000);
}
 
PyObject *M_Render_ImageSizeX(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.xsch, 4, 10000);
}

PyObject *M_Render_ImageSizeY(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.ysch, 4, 10000);
}

PyObject *M_Render_AspectRatioX(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.xasp, 1, 200);
}

PyObject *M_Render_AspectRatioY(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.yasp, 1, 200);
}

PyObject *M_Render_SetRenderer(PyObject *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant INTERN or YAFRAY"));

	if(type == R_INTERN)
		((BPy_Scene*)self)->scene->r.renderer = R_INTERN;
	else if (type == R_YAFRAY)
		((BPy_Scene*)self)->scene->r.renderer = R_YAFRAY;
	else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected INTERN or YAFRAY"));

	allqueue(REDRAWBUTSSCENE, 0);
 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableCropping(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_MOVIECROP, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetImageType(PyObject *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if(type == R_AVIRAW)
		((BPy_Scene*)self)->scene->r.imtype = R_AVIRAW;
	else if (type == R_AVIJPEG)
		((BPy_Scene*)self)->scene->r.imtype = R_AVIJPEG;
#ifdef _WIN32
	else if (type == R_AVICODEC)
		((BPy_Scene*)self)->scene->r.imtype = R_AVICODEC;
#endif
	else if (type == R_QUICKTIME && G.have_quicktime)
		((BPy_Scene*)self)->scene->r.imtype = R_QUICKTIME;
	else if (type == R_TARGA)
		((BPy_Scene*)self)->scene->r.imtype = R_TARGA;
	else if (type == R_RAWTGA)
		((BPy_Scene*)self)->scene->r.imtype = R_RAWTGA;
	else if (type == R_PNG)
		((BPy_Scene*)self)->scene->r.imtype = R_PNG;
	else if (type == R_BMP)
		((BPy_Scene*)self)->scene->r.imtype = R_BMP;
	else if (type == R_JPEG90)
		((BPy_Scene*)self)->scene->r.imtype = R_JPEG90;
	else if (type == R_HAMX)
		((BPy_Scene*)self)->scene->r.imtype = R_HAMX;
	else if (type == R_IRIS)
		((BPy_Scene*)self)->scene->r.imtype = R_IRIS;
	else if (type == R_IRIZ)
		((BPy_Scene*)self)->scene->r.imtype = R_IRIZ;
	else if (type == R_FTYPE)
		((BPy_Scene*)self)->scene->r.imtype = R_FTYPE;
	else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_Quality(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.quality, 10, 100);
}

PyObject *M_Render_FramesPerSec(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.frs_sec, 1, 120);
}

PyObject *M_Render_EnableGrayscale(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.planes = R_PLANESBW;
	allqueue(REDRAWBUTSSCENE, 0);

 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableRGBColor(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.planes = R_PLANES24;
	allqueue(REDRAWBUTSSCENE, 0);

  	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableRGBAColor(PyObject *self)
{
	((BPy_Scene*)self)->scene->r.planes = R_PLANES32;
	allqueue(REDRAWBUTSSCENE, 0);

   	return EXPP_incr_ret(Py_None);
}

static void M_Render_DoSizePreset(PyObject *self, short xsch, short ysch, short xasp,
									   short yasp, short size, short xparts, short yparts,
									   short frames, float a, float b, float c, float d)
{
		((BPy_Scene*)self)->scene->r.xsch= xsch;
		((BPy_Scene*)self)->scene->r.ysch= ysch;
		((BPy_Scene*)self)->scene->r.xasp= xasp;
		((BPy_Scene*)self)->scene->r.yasp= yasp;
		((BPy_Scene*)self)->scene->r.size= size;
		((BPy_Scene*)self)->scene->r.frs_sec= frames;
		((BPy_Scene*)self)->scene->r.xparts= xparts;
		((BPy_Scene*)self)->scene->r.yparts= yparts;

		BLI_init_rctf(&((BPy_Scene*)self)->scene->r.safety, a, b, c, d);
		allqueue(REDRAWBUTSSCENE, 0);
		allqueue(REDRAWVIEWCAM, 0);
}

PyObject *M_Render_SizePreset(PyObject *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if(type == R_PAL){
		M_Render_DoSizePreset(self,720,576,54,51,100, ((BPy_Scene*)self)->scene->r.xparts,
					  ((BPy_Scene*)self)->scene->r.yparts, 25, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else if (type == R_NTSC){
		M_Render_DoSizePreset(self,720,480,10,11,100, 1, 1, 
			30, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else if (type == R_DEFAULT){
		M_Render_DoSizePreset(self,720,576,54,51,100, 1, 1, 
			((BPy_Scene*)self)->scene->r.frs_sec, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode= R_OSA+R_SHADOW+R_FIELDS;
		((BPy_Scene*)self)->scene->r.imtype= R_TARGA;
	}else if (type == R_PREVIEW){
		M_Render_DoSizePreset(self,640,512,1,1,50, 1, 1, 
			((BPy_Scene*)self)->scene->r.frs_sec, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else if (type == R_PC){
		M_Render_DoSizePreset(self,640,480,100,100,100, 1, 1, 
			((BPy_Scene*)self)->scene->r.frs_sec, 0.0, 1.0, 0.0, 1.0);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else if (type == R_PAL169){
		M_Render_DoSizePreset(self,720,576,64,45,100, 1, 1, 
			25, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else if (type == R_PANO){
		M_Render_DoSizePreset(self,36,176,115,100,100, 16, 1, 
			((BPy_Scene*)self)->scene->r.frs_sec, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode |= R_PANORAMA;
	}else if (type == R_FULL){
		M_Render_DoSizePreset(self,1280,1024,1,1,100, 1, 1, 
			((BPy_Scene*)self)->scene->r.frs_sec, 0.1, 0.9, 0.1, 0.9);
		((BPy_Scene*)self)->scene->r.mode &= ~R_PANORAMA;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableUnifiedRenderer(PyObject *self, PyObject *args)
{
	M_Render_BitToggleInt(args, R_UNIFIED, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetYafrayGIQuality(PyObject *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if( type == PY_NONE   || type == PY_LOW  ||
		type == PY_MEDIUM || type == PY_HIGH ||
		type == PY_HIGHER || type == PY_BEST){
		((BPy_Scene*)self)->scene->r.GIquality = type;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetYafrayGIMethod(PyObject *self, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected constant"));

	if( type == PY_NONE   || type == PY_SKYDOME  || type == PY_GIFULL){
		((BPy_Scene*)self)->scene->r.GImethod = type;
	}else
 		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"unknown constant - see modules dict for help"));

	allqueue(REDRAWBUTSSCENE, 0);
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_YafrayGIPower(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod>0) {
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.GIpower, 0.01f, 100.00f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'SKYDOME' or 'FULL'"));
}

PyObject *M_Render_YafrayGIDepth(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2) {
		return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.GIdepth, 1, 8);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}

PyObject *M_Render_YafrayGICDepth(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2) {
		return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.GIcausdepth, 1, 8);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
}

PyObject *M_Render_EnableYafrayGICache(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2) {
		M_Render_BitToggleShort(args, 1, &((BPy_Scene*)self)->scene->r.GIcache);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));
	
	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableYafrayGIPhotons(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2) {
		M_Render_BitToggleShort(args, 1, &((BPy_Scene*)self)->scene->r.GIphotons);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL'"));

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_YafrayGIPhotonCount(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIphotons==1) {
		return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.GIphotoncount, 0, 10000000);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}

PyObject *M_Render_YafrayGIPhotonRadius(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIphotons==1) {
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.GIphotonradius, 0.00001f, 100.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}

PyObject *M_Render_YafrayGIPhotonMixCount(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIphotons==1) {
		return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.GImixphotons, 0, 1000);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled\n"));
}

PyObject *M_Render_EnableYafrayGITunePhotons(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIphotons==1) {
		M_Render_BitToggleShort(args, 1, &((BPy_Scene*)self)->scene->r.GIdirect);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GIPhotons must be enabled"));

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_YafrayGIShadowQuality(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIcache==1) {
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.GIshadowquality, 0.01f, 1.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}

PyObject *M_Render_YafrayGIPixelsPerSample(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIcache==1) {
		return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.GIpixelspersample, 1, 50);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}

PyObject *M_Render_EnableYafrayGIGradient(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIcache==1) {
		M_Render_BitToggleShort(args, 1, &((BPy_Scene*)self)->scene->r.GIgradient);;
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled"));

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_YafrayGIRefinement(PyObject *self, PyObject *args)
{
	if (((BPy_Scene*)self)->scene->r.GImethod==2 && ((BPy_Scene*)self)->scene->r.GIcache==1) {
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.GIrefinement, 0.001f, 1.0f);
	}else
		return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"YafrayGIMethod must be set to 'FULL' and GICache must be enabled\n"));
}

PyObject *M_Render_YafrayRayBias(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.YF_raybias, 0.0f, 10.0f);
}

PyObject *M_Render_YafrayRayDepth(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.YF_raydepth, 1, 80);
}

PyObject *M_Render_YafrayGamma(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.YF_gamma, 0.001f, 5.0f);
}

PyObject *M_Render_YafrayExposure(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.YF_exposure, 0.0f, 10.0f);
}

PyObject *M_Render_YafrayProcessorCount(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeInt(args, &((BPy_Scene*)self)->scene->r.YF_numprocs, 1, 8);
}

PyObject *M_Render_EnableGameFrameStretch(PyObject *self)
{
	((BPy_Scene*)self)->scene->framing.type = SCE_GAMEFRAMING_SCALE;
 	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableGameFrameExpose(PyObject *self)
{
	((BPy_Scene*)self)->scene->framing.type = SCE_GAMEFRAMING_EXTEND;
  	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_EnableGameFrameBars(PyObject *self)
{
	((BPy_Scene*)self)->scene->framing.type = SCE_GAMEFRAMING_BARS;
   	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_SetGameFrameColor(PyObject *self, PyObject *args)
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

	((BPy_Scene*)self)->scene->framing.col[0] = red;
	((BPy_Scene*)self)->scene->framing.col[1] = green;
	((BPy_Scene*)self)->scene->framing.col[2] = blue;

	return EXPP_incr_ret(Py_None);
}

PyObject *M_Render_GetGameFrameColor(PyObject *self)
{
	char rgb[24];

	sprintf(rgb, "[%.3f,%.3f,%.3f]\n", ((BPy_Scene*)self)->scene->framing.col[0],
		((BPy_Scene*)self)->scene->framing.col[1], ((BPy_Scene*)self)->scene->framing.col[2]);
	return PyString_FromString (rgb);
}


PyObject *M_Render_GammaLevel(PyObject *self, PyObject *args)
{
	if(((BPy_Scene*)self)->scene->r.mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.gamma, 0.2f, 5.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}

PyObject *M_Render_PostProcessAdd(PyObject *self, PyObject *args)
{
	if(((BPy_Scene*)self)->scene->r.mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.postadd, -1.0f, 1.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}

PyObject *M_Render_PostProcessMultiply(PyObject *self, PyObject *args)
{
	if(((BPy_Scene*)self)->scene->r.mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.postmul, 0.01f, 4.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}

PyObject *M_Render_PostProcessGamma(PyObject *self, PyObject *args)
{
	if(((BPy_Scene*)self)->scene->r.mode & R_UNIFIED){
		return M_Render_GetSetAttributeFloat(args, &((BPy_Scene*)self)->scene->r.postgamma, 0.2f, 2.0f);
	}else
  		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"Unified Render must be enabled"));
}

PyObject *M_Render_SGIMaxsize(PyObject *self, PyObject *args)
{
#ifdef __sgi
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.maximsize, 0, 500);
#else
  	return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"SGI is not defined on this machine"));
#endif
}

PyObject *M_Render_EnableSGICosmo(PyObject *self, PyObject *args)
{
#ifdef __sgi
	M_Render_BitToggleInt(args, R_COSMO, &((BPy_Scene*)self)->scene->r.mode);
	return EXPP_incr_ret(Py_None);
#else
  	return (EXPP_ReturnPyObjError (PyExc_StandardError,
						"SGI is not defined on this machine"));
#endif
}

PyObject *M_Render_OldMapValue(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.framapto, 1, 900);
}

PyObject *M_Render_NewMapValue(PyObject *self, PyObject *args)
{
	return M_Render_GetSetAttributeShort(args, &((BPy_Scene*)self)->scene->r.images, 1, 900);
}



