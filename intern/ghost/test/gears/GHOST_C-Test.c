/**
 * $Id$
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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * Simple test file for the GHOST library.
 * The OpenGL gear code is taken from the Qt sample code which,
 * in turn, is probably taken from somewhere as well.
 * @author	Maarten Gribnau
 * @date	May 31, 2001
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define FALSE 0

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "GHOST_C-api.h"

#if defined(WIN32) || defined(__APPLE__)
	#ifdef WIN32
		#include <windows.h>
		#include <GL/gl.h>
	#else /* WIN32 */
		/* __APPLE__ is defined */
		#include <AGL/gl.h>
	#endif /* WIN32 */
#else /* defined(WIN32) || defined(__APPLE__) */
	#include <GL/gl.h>
#endif /* defined(WIN32) || defined(__APPLE__) */


static void gearsTimerProc(GHOST_TimerTaskHandle task, GHOST_TUns64 time);
int processEvent(GHOST_EventHandle hEvent, GHOST_TUserDataPtr userData);

static GLfloat view_rotx=20.0, view_roty=30.0, view_rotz=0.0;
static GLfloat fAngle = 0.0;
static int sExitRequested = 0;
static GHOST_SystemHandle shSystem = NULL;
static GHOST_WindowHandle sMainWindow = NULL;
static GHOST_WindowHandle sSecondaryWindow = NULL;
static GHOST_TStandardCursor sCursor = GHOST_kStandardCursorFirstCursor;
static GHOST_WindowHandle sFullScreenWindow = NULL;
static GHOST_TimerTaskHandle sTestTimer;
static GHOST_TimerTaskHandle sGearsTimer;

static void testTimerProc(GHOST_TimerTaskHandle task, GHOST_TUns64 time)
{
	printf("timer1, time=%d\n", (int)time);
}


static void gearGL(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth)
{
	GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;
	const double pi = 3.14159264;
	
    r0 = inner_radius;
    r1 = (float)(outer_radius - tooth_depth/2.0);
    r2 = (float)(outer_radius + tooth_depth/2.0);
	
    da = (float)(2.0*pi / teeth / 4.0);
	
    glShadeModel(GL_FLAT);
    glNormal3f(0.0, 0.0, 1.0);
	
    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(width*0.5));
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(width*0.5));
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(width*0.5));
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(width*0.5));
    }
    glEnd();
	
    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = (float)(2.0*pi / teeth / 4.0);
    for (i=0;i<teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(width*0.5));
		glVertex3f((float)(r2*cos(angle+da)), (float)(r2*sin(angle+da)), (float)(width*0.5));
		glVertex3f((float)(r2*cos(angle+2*da)), (float)(r2*sin(angle+2*da)), (float)(width*0.5));
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(width*0.5));
    }
    glEnd();
	
    glNormal3f(0.0, 0.0, -1.0);
	
	/* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(-width*0.5));
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(-width*0.5));
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(-width*0.5));
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(-width*0.5));
    }
    glEnd();
	
    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = (float)(2.0*pi / teeth / 4.0);
    for (i=0;i<teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(-width*0.5));
		glVertex3f((float)(r2*cos(angle+2*da)), (float)(r2*sin(angle+2*da)), (float)(-width*0.5));
		glVertex3f((float)(r2*cos(angle+da)), (float)(r2*sin(angle+da)), (float)(-width*0.5));
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(-width*0.5));
    }
    glEnd();
	
    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i=0;i<teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(width*0.5));
		glVertex3f((float)(r1*cos(angle)), (float)(r1*sin(angle)), (float)(-width*0.5));
		u = (float)(r2*cos(angle+da) - r1*cos(angle));
		v = (float)(r2*sin(angle+da) - r1*sin(angle));
		len = (float)(sqrt(u*u + v*v));
		u /= len;
		v /= len;
		glNormal3f(v, -u, 0.0);
		glVertex3f((float)(r2*cos(angle+da)), (float)(r2*sin(angle+da)), (float)(width*0.5));
		glVertex3f((float)(r2*cos(angle+da)), (float)(r2*sin(angle+da)), (float)(-width*0.5));
		glNormal3f((float)(cos(angle)), (float)(sin(angle)), 0.0);
		glVertex3f((float)(r2*cos(angle+2*da)), (float)(r2*sin(angle+2*da)), (float)(width*0.5));
		glVertex3f((float)(r2*cos(angle+2*da)), (float)(r2*sin(angle+2*da)), (float)(-width*0.5));
		u = (float)(r1*cos(angle+3*da) - r2*cos(angle+2*da));
		v = (float)(r1*sin(angle+3*da) - r2*sin(angle+2*da));
		glNormal3f(v, -u, 0.0);
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(width*0.5));
		glVertex3f((float)(r1*cos(angle+3*da)), (float)(r1*sin(angle+3*da)), (float)(-width*0.5));
		glNormal3f((float)(cos(angle)), (float)(sin(angle)), 0.0);
    }
    glVertex3f((float)(r1*cos(0.0)), (float)(r1*sin(0.0)), (float)(width*0.5));
    glVertex3f((float)(r1*cos(0.0)), (float)(r1*sin(0.0)), (float)(-width*0.5));
    glEnd();
	
    glShadeModel(GL_SMOOTH);
	
    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i=0;i<=teeth;i++) {
		angle = (float)(i * 2.0*pi / teeth);
		glNormal3f((float)(-cos(angle)), (float)(-sin(angle)), 0.0);
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(-width*0.5));
		glVertex3f((float)(r0*cos(angle)), (float)(r0*sin(angle)), (float)(width*0.5));
    }
    glEnd();
}



static void drawGearGL(int id)
{
    static GLfloat pos[4] = { 5.0f, 5.0f, 10.0f, 1.0f };
    static GLfloat ared[4] = { 0.8f, 0.1f, 0.0f, 1.0f };
    static GLfloat agreen[4] = { 0.0f, 0.8f, 0.2f, 1.0f };
    static GLfloat ablue[4] = { 0.2f, 0.2f, 1.0f, 1.0f };
	
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
	
	switch (id)
	{
	case 1:
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ared);
		gearGL(1.0f, 4.0f, 1.0f, 20, 0.7f);
		break;
	case 2:
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, agreen);
		gearGL(0.5f, 2.0f, 2.0f, 10, 0.7f);
		break;
	case 3:
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, ablue);
		gearGL(1.3f, 2.0f, 0.5f, 10, 0.7f);
		break;
	default:
		break;
	}
    glEnable(GL_NORMALIZE);
}


static void drawGL(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
    glPushMatrix();
	
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);
    glRotatef(view_rotz, 0.0, 0.0, 1.0);
	
    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(fAngle, 0.0, 0.0, 1.0);
    drawGearGL(1);
    glPopMatrix();
	
    glPushMatrix();
    glTranslatef(3.1f, -2.0f, 0.0f);
    glRotatef((float)(-2.0*fAngle-9.0), 0.0, 0.0, 1.0);
    drawGearGL(2);
    glPopMatrix();
	
    glPushMatrix();
    glTranslatef(-3.1f, 2.2f, -1.8f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef((float)(2.0*fAngle-2.0), 0.0, 0.0, 1.0);
    drawGearGL(3);
    glPopMatrix();
	
    glPopMatrix();
}


static void setViewPortGL(GHOST_WindowHandle hWindow)
{
	GHOST_RectangleHandle hRect = NULL;
	GLfloat w, h;
	
    GHOST_ActivateWindowDrawingContext(hWindow);
    hRect = GHOST_GetClientBounds(hWindow);
	
    w = (float)GHOST_GetWidthRectangle(hRect) / (float)GHOST_GetHeightRectangle(hRect);
    h = 1.0;
	
	glViewport(0, 0, GHOST_GetWidthRectangle(hRect), GHOST_GetHeightRectangle(hRect));

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    glFrustum(-w, w, -h, h, 5.0, 60.0);
	/* glOrtho(0, bnds.getWidth(), 0, bnds.getHeight(), -10, 10); */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
    glTranslatef(0.0, 0.0, -40.0);
	
	glClearColor(.2f,0.0f,0.0f,0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	GHOST_DisposeRectangle(hRect);
}



int processEvent(GHOST_EventHandle hEvent, GHOST_TUserDataPtr userData)
{
	int handled = 1;
	int cursor;
	int visibility;
	GHOST_TEventKeyData* keyData = NULL;
	GHOST_TEventWheelData* wheelData = NULL;
	GHOST_DisplaySetting setting;
	GHOST_WindowHandle window = GHOST_GetEventWindow(hEvent);
	
	switch (GHOST_GetEventType(hEvent))
	{
	/*
	case GHOST_kEventUnknown:
		break;
	case GHOST_kEventCursorButton:
		break;
	case GHOST_kEventCursorMove:
		break;
	*/
	case GHOST_kEventWheel:
		{
		wheelData = (GHOST_TEventWheelData*)GHOST_GetEventData(hEvent);
		if (wheelData->z > 0)
		{
			view_rotz += 5.f;
		}
		else
		{
			view_rotz -= 5.f;
		}
		}
		break;

	case GHOST_kEventKeyUp:
		break;
		
	case GHOST_kEventKeyDown:
		{
			keyData = (GHOST_TEventKeyData*)GHOST_GetEventData(hEvent);
			switch (keyData->key)
			{
			case GHOST_kKeyC:
				{
					cursor = sCursor;
					cursor++;
					if (cursor >= GHOST_kStandardCursorNumCursors)
					{
						cursor = GHOST_kStandardCursorFirstCursor;
					}
					sCursor = (GHOST_TStandardCursor)cursor;
					GHOST_SetCursorShape(window, sCursor);
				}
				break;
			case GHOST_kKeyF:
				if (!GHOST_GetFullScreen(shSystem))
				{
					/* Begin fullscreen mode */
					setting.bpp = 24;
					setting.frequency = 85;
					setting.xPixels = 640;
					setting.yPixels = 480;
					
					/*
					setting.bpp = 16;
					setting.frequency = 75;
					setting.xPixels = 640;
					setting.yPixels = 480;
					*/

					sFullScreenWindow = GHOST_BeginFullScreen(shSystem, &setting,

						FALSE /* stereo flag */);
				}
				else
				{
					GHOST_EndFullScreen(shSystem);
					sFullScreenWindow = 0;
				}
				break;
			case GHOST_kKeyH:
				{
					visibility = GHOST_GetCursorVisibility(window);
					GHOST_SetCursorVisibility(window, !visibility);
				}
				break;
			case GHOST_kKeyQ:
				if (GHOST_GetFullScreen(shSystem))
				{
					GHOST_EndFullScreen(shSystem);
					sFullScreenWindow = 0;
				}
				sExitRequested = 1;
			case GHOST_kKeyT:
				if (!sTestTimer)
				{
					sTestTimer = GHOST_InstallTimer(shSystem, 0, 1000, testTimerProc, NULL);
				}
				else
				{
					GHOST_RemoveTimer(shSystem, sTestTimer);
					sTestTimer = 0;
				}
				break;
			case GHOST_kKeyW:
				{
					if (sMainWindow)
					{
						char *title = GHOST_GetTitle(sMainWindow);
						char *ntitle = malloc(strlen(title)+2);

						sprintf(ntitle, "%s-", title);
						GHOST_SetTitle(sMainWindow, ntitle);
						
						free(ntitle);
						free(title);
					}
				}
				break;
			default:
				break;
			}
		}
		break;
		
	case GHOST_kEventWindowClose:
		{
			GHOST_WindowHandle window2 = GHOST_GetEventWindow(hEvent);
			if (window2 == sMainWindow)
			{
				sExitRequested = 1;
			}
			else
			{
				if (sGearsTimer)
				{
					GHOST_RemoveTimer(shSystem, sGearsTimer);
					sGearsTimer = 0;
				}
				GHOST_DisposeWindow(shSystem, window2);
			}
		}
		break;
		
	case GHOST_kEventWindowActivate:
		handled = 0;
		break;
	case GHOST_kEventWindowDeactivate:
		handled = 0;
		break;
	case GHOST_kEventWindowUpdate:
		{
			GHOST_WindowHandle window2 = GHOST_GetEventWindow(hEvent);
			if (!GHOST_ValidWindow(shSystem, window2))
				break;
			setViewPortGL(window2);
			drawGL();
			GHOST_SwapWindowBuffers(window2);
		}
		break;
		
	default:
		handled = 0;
		break;
	}
	return handled;
}


int main(int argc, char** argv)
{
	char* title1 = "gears - main window";
	char* title2 = "gears - secondary window";
	GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(processEvent, NULL);

	/* Create the system */
	shSystem = GHOST_CreateSystem();
	GHOST_AddEventConsumer(shSystem, consumer);
	
	if (shSystem)
	{
		/* Create the main window */
		sMainWindow = GHOST_CreateWindow(shSystem,
			title1,
			10,
			64,
			320,
			200,
			GHOST_kWindowStateNormal,
			GHOST_kDrawingContextTypeOpenGL,
			FALSE);
		if (!sMainWindow)
		{
			printf("could not create main window\n");
			exit(-1);
		}
		
		/* Create a secondary window */
		sSecondaryWindow = GHOST_CreateWindow(shSystem,
			title2,
			340,
			64,
			320,
			200,
			GHOST_kWindowStateNormal,
			GHOST_kDrawingContextTypeOpenGL,
			FALSE);
		if (!sSecondaryWindow)
		{
			printf("could not create secondary window\n");
			exit(-1);
		}
		
		/* Install a timer to have the gears running */
		 sGearsTimer = GHOST_InstallTimer(shSystem,
			0,
			10,
			gearsTimerProc,
			sMainWindow);

		/* Enter main loop */
		while (!sExitRequested)
		{
			if (!GHOST_ProcessEvents(shSystem, 0)) 
			{
#ifdef WIN32
				/* If there were no events, be nice to other applications */
				Sleep(10);
#endif
			}
			GHOST_DispatchEvents(shSystem);
		}
	}

	/* Dispose windows */
	if (GHOST_ValidWindow(shSystem, sMainWindow))
	{
		GHOST_DisposeWindow(shSystem, sMainWindow);
	}
	if (GHOST_ValidWindow(shSystem, sSecondaryWindow))
	{
		GHOST_DisposeWindow(shSystem, sSecondaryWindow);
	}

	/* Dispose the system */
	GHOST_DisposeSystem(shSystem);
	GHOST_DisposeEventConsumer(consumer);
	
	return 0;
}


static void gearsTimerProc(GHOST_TimerTaskHandle hTask, GHOST_TUns64 time)
{
	GHOST_WindowHandle hWindow = NULL;
    fAngle += 2.0;
    view_roty += 1.0;
	hWindow = (GHOST_WindowHandle)GHOST_GetTimerTaskUserData(hTask);
	if (GHOST_GetFullScreen(shSystem))
	{
		/* Running full screen */
		GHOST_InvalidateWindow(sFullScreenWindow);
	}
	else
	{
		if (GHOST_ValidWindow(shSystem, hWindow))
		{
			GHOST_InvalidateWindow(hWindow);
		}
	}
}
