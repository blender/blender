/**
 * $Id:
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef __APPLE__

#include "BKE_context.h"
#include "BKE_global.h"
#include "WM_api.h"

#include <OpenGL/OpenGL.h>
#define __CARBONSOUND__
/* XXX BIG WARNING: carbon.h can not be included in blender code, it conflicts with struct ID */
#define ID ID_
#include <Carbon/Carbon.h>


/* To avoid killing small end comps, we want to allow
blender to start maximised if all the followings are true :
- Renderer is OpenGL capable
- Hardware acceleration
- VRAM > 16 Mo

	We will bail out if VRAM is less than 8Mo
	*/
/* bad global, used in wm_window.c to open windows */
int macPrefState = 0;

static int checkAppleVideoCard(void) 
{
	CGLRendererInfoObj rend;
	long theErr;
	unsigned long display_mask;
	long nrend;
	int j;
	long value;
	long maxvram = 0;   /* we get always more than 1 renderer, check one, at least, has 8 Mo */
	
	display_mask = CGDisplayIDToOpenGLDisplayMask (CGMainDisplayID() );	
	
	theErr = CGLQueryRendererInfo( display_mask, &rend, &nrend);
	if (theErr == 0) {
		theErr = CGLDescribeRenderer (rend, 0, kCGLRPRendererCount, &nrend);
		if (theErr == 0) {
			for (j = 0; j < nrend; j++) {
				theErr = CGLDescribeRenderer (rend, j, kCGLRPVideoMemory, &value); 
				if (value > maxvram)
					maxvram = value;
				if ((theErr == 0) && (value >= 20000000)) {
					theErr = CGLDescribeRenderer (rend, j, kCGLRPAccelerated, &value); 
					if ((theErr == 0) && (value != 0)) {
						theErr = CGLDescribeRenderer (rend, j, kCGLRPCompliant, &value); 
						if ((theErr == 0) && (value != 0)) {
							/*fprintf(stderr,"make it big\n");*/
							CGLDestroyRendererInfo (rend);
							macPrefState = 8;
							return 1;
						}
					}
				}
			}
		}
	}
	if (maxvram < 7500000 ) {       /* put a standard alert and quit*/ 
		SInt16 junkHit;
		char  inError[] = "* Not enough VRAM    ";
		char  inText[] = "* blender needs at least 8Mb    ";
		inError[0] = 16;
		inText[0] = 28;
		
		fprintf(stderr, " vram is %li . not enough, aborting\n", maxvram);
		StandardAlert (   kAlertStopAlert, (ConstStr255Param) &inError, (ConstStr255Param)&inText,NULL,&junkHit);
		abort();
	}
CGLDestroyRendererInfo (rend);
return 0;
}

static void getMacAvailableBounds(short *top, short *left, short *bottom, short *right) 
{
	Rect outAvailableRect;
	
	GetAvailableWindowPositioningBounds ( GetMainDevice(), &outAvailableRect);
	
	*top = outAvailableRect.top;  
    *left = outAvailableRect.left;
    *bottom = outAvailableRect.bottom; 
    *right = outAvailableRect.right;
}


void wm_set_apple_prefsize(int scr_x, int scr_y)
{
	
	/* first let us check if we are hardware accelerated and with VRAM > 16 Mo */
	
	if (checkAppleVideoCard()) {
		short top, left, bottom, right;
		
		getMacAvailableBounds(&top, &left, &bottom, &right);
		WM_setprefsize(left +10,scr_y - bottom +10,right-left -20,bottom - 64);
		G.windowstate= 0;
		
	} else {
		
		/* 40 + 684 + (headers) 22 + 22 = 768, the powerbook screen height */
		WM_setprefsize(120, 40, 850, 684);
		G.windowstate= 0;
	}
}


#endif /* __APPLE__ */


