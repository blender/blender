/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_PythonInit.h
 *  \ingroup ketsji
 */

#ifndef __KX_PYTHONINIT_H__
#define __KX_PYTHONINIT_H__

#include "KX_Python.h"
#include "STR_String.h"
#include "MT_Vector3.h"

typedef enum {
	psl_Lowest = 0,
	psl_Highest,
} TPythonSecurityLevel;

extern bool gUseVisibilityTemp;

#ifdef WITH_PYTHON
PyObject *initGameLogic(class KX_KetsjiEngine *engine, class KX_Scene *ketsjiscene);
PyObject *initGameKeys();
PyObject *initRasterizer(class RAS_IRasterizer *rasty,class RAS_ICanvas *canvas);
PyObject *initGamePlayerPythonScripting(const STR_String &progname, TPythonSecurityLevel level,
                                        struct Main *maggie, int argc, char **argv);
PyObject *initVideoTexture(void); 
PyObject *initGamePythonScripting(const STR_String &progname, TPythonSecurityLevel level, struct Main *maggie);

void exitGamePlayerPythonScripting();
void exitGamePythonScripting();
void setupGamePython(KX_KetsjiEngine *ketsjiengine, KX_Scene *startscene, Main *blenderdata,
                     PyObject *pyGlobalDict, PyObject **gameLogic, PyObject **gameLogic_keys, int argc, char **argv);
void setGamePythonPath(const char *path);
void resetGamePythonPath();
void pathGamePythonConfig(char *path);
int saveGamePythonConfig(char **marshal_buffer);
int loadGamePythonConfig(char *marshal_buffer, int marshal_length);
#endif

void addImportMain(struct Main *maggie);
void removeImportMain(struct Main *maggie);

class KX_KetsjiEngine;
class KX_Scene;

void KX_SetActiveScene(class KX_Scene *scene);
class KX_Scene *KX_GetActiveScene();
class KX_KetsjiEngine *KX_GetActiveEngine();

typedef int (*PyNextFrameFunc)(void *);

struct PyNextFrameState {
	/** can be either a GPG_NextFrameState or a BL_KetsjiNextFrameState */
	void *state;
	/** can be either GPG_PyNextFrame or BL_KetsjiPyNextFrame */
	PyNextFrameFunc func;
};
extern struct PyNextFrameState pynextframestate;

void KX_RasterizerDrawDebugLine(const MT_Vector3 &from,const MT_Vector3 &to,const MT_Vector3 &color);
void KX_RasterizerDrawDebugCircle(const MT_Vector3 &center, const MT_Scalar radius, const MT_Vector3 &color,
                                  const MT_Vector3 &normal, int nsector);


#endif  /* __KX_PYTHONINIT_H__ */
