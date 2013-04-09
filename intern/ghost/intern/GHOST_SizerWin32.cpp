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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SizerWin32.cpp
 *  \ingroup GHOST
 */

#ifndef __GHOST_SIZERWIN32_CPP__
#define __GHOST_SIZERWIN32_CPP__


#include "GHOST_SizerWin32.h"
#include <Windowsx.h>

#define T_NONE (0)
#define T_SIZE (1)
#define T_MOVE (2)
#define T_MOVESIZE (3)


static void RectCopyH(RECT * t, RECT * f)
{
	t->left = f->left;
	t->right = f->right;
}

static void RectCopyV(RECT * t, RECT * f)
{
	t->top = f->top;
	t->bottom = f->bottom;
}

static void RectCopy(RECT * t, RECT * f)
{
	RectCopyH(t,f);
	RectCopyV(t,f);
}


GHOST_SizerWin32::GHOST_SizerWin32(void)
{
	hortransf = vertransf = 0;
	minsize[0] = minsize[1] = 0;
	hwnd = 0;
}

GHOST_SizerWin32::~GHOST_SizerWin32(void)
{
	if(isWinChanges())
		cancel();
}

void GHOST_SizerWin32::setMinSize(int minx, int miny)
{
	minsize[0] = minx;
	minsize[1] = miny;


}

bool GHOST_SizerWin32::isWinChanges(void)
{
	return hortransf||vertransf;
}


void GHOST_SizerWin32::startSizing(unsigned short type)
{
	//SetCapture(hwnd);
	POINT coord;

	switch(type & 0xf)
	{

	case WMSZ_LEFT:			hortransf = T_MOVESIZE;
							vertransf = T_NONE; break;
	case WMSZ_RIGHT:		hortransf = T_SIZE;
							vertransf = T_NONE; break;		
	case WMSZ_TOP:			hortransf = T_NONE;
							vertransf = T_MOVESIZE; break;
	case WMSZ_TOPLEFT:		hortransf = T_MOVESIZE;
							vertransf = T_MOVESIZE; break;
	case WMSZ_TOPRIGHT:		hortransf = T_SIZE;
							vertransf = T_MOVESIZE; break;
	case WMSZ_BOTTOM:		hortransf = T_NONE;
							vertransf = T_SIZE; break;
	case WMSZ_BOTTOMLEFT:	hortransf = T_MOVESIZE;
							vertransf = T_SIZE; break;
	case WMSZ_BOTTOMRIGHT:	hortransf = T_SIZE;
							vertransf = T_SIZE; break;

	}



	POINT mp;
	GetCursorPos(&mp);
	startpos[0]=mp.x;
	startpos[1]=mp.y;

	GetWindowRect(hwnd, &initrect);
	initrect.bottom-=initrect.top;
	initrect.right-=initrect.left;
	RectCopy(&goodrect,&initrect);

}

void GHOST_SizerWin32::setHWND(HWND hWnd)
{
	this->hwnd = hWnd;
}


void GHOST_SizerWin32::updateWindowSize(void)
{
	if(!isWinChanges())
		return;
	if(hortransf||vertransf){
		POINT mp;
		GetCursorPos(&mp);
		int hordelta = mp.x-startpos[0];
		int verdelta = mp.y-startpos[1];

		RECT newrect;
		RectCopy(&newrect, &initrect);

		switch(hortransf)
		{
			case T_SIZE: 
				newrect.right+=hordelta;
				break;
				case T_MOVESIZE:
				newrect.right-=hordelta;
				case T_MOVE:
				newrect.left+=hordelta;
				break;
		}

		switch(vertransf)
		{
			case T_SIZE: 
				newrect.bottom+=verdelta;
				break;
				case T_MOVESIZE:
				newrect.bottom-=verdelta;
				case T_MOVE:
				newrect.top+=verdelta;
				break;
		}

				
		if(newrect.right<minsize[0])
			RectCopyH(&newrect,&goodrect);
		if(newrect.bottom<minsize[1])
			RectCopyV(&newrect,&goodrect);

	SetWindowPos(hwnd,0,newrect.left, newrect.top, 
		newrect.right, newrect.bottom,
		0);

	RectCopy(&goodrect, &newrect);


}
}

void GHOST_SizerWin32::cancel(void)
{
	accept();
	SetWindowPos(hwnd,0,initrect.left, initrect.top, 
		initrect.right, initrect.bottom, 0);
}

void GHOST_SizerWin32::accept(void)
{
	hortransf=vertransf=0;
}


#endif /* __GHOST_SIZERWIN32_CPP__*/