/**
 * $Id$
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
 */

#include "IMG_CanvasRGBA32.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

IMG_CanvasRGBA32::IMG_CanvasRGBA32(TUns32 width, TUns32 height)
	: IMG_PixmapRGBA32(width, height)
{
}

IMG_CanvasRGBA32::IMG_CanvasRGBA32(void* image, TUns32 width, TUns32 height, TUns32 rowBytes)
	: IMG_PixmapRGBA32(image, width, height, rowBytes)
{
}


void IMG_CanvasRGBA32::blendPixmap(
	TUns32 xStart, TUns32 yStart, TUns32 xEnd, TUns32 yEnd,
	const IMG_PixmapRGBA32& pixmap,char mode)
{
	// Determine visibility of the line
	IMG_Line l (xStart, yStart, xEnd, yEnd);	// Line used for blending
	IMG_Rect bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	TVisibility v = bnds.getVisibility(l);
	if (mode == 'c'){
	if (v == kNotVisible) return;
	if (v == kPartiallyVisible) {
		bnds.clip(l);
	}
	}

	float numSteps = (((float)l.getLength()) / ((float)pixmap.getWidth() / 4));
	//numSteps *= 4;
	numSteps = numSteps ? numSteps : 1;
	float step = 0.f, stepSize = 1.f / ((float)numSteps);
	TInt32 x, y;
    for (TUns32 s = 0; s < numSteps; s++) {
		l.getPoint(step, x, y);
		if (mode == 'c') {
		IMG_PixmapRGBA32::blendPixmap((TUns32)x, (TUns32)y, pixmap);
		}
		else {
			if (mode == 't') IMG_PixmapRGBA32::blendPixmapTorus((TUns32)x, (TUns32)y, pixmap);
		}
		step += stepSize;
	}
}


void IMG_CanvasRGBA32::blendPixmap(
	float uStart, float vStart, float uEnd, float vEnd,
	const IMG_PixmapRGBA32& pixmap, char mode)
{
	TUns32 xStart, yStart, xEnd, yEnd;
	getPixelAddress(uStart, vStart, xStart, yStart);
	getPixelAddress(uEnd, vEnd, xEnd, yEnd);
	blendPixmap(xStart, yStart, xEnd, yEnd, pixmap,mode);
}


void IMG_CanvasRGBA32::SoftenAt(float u, float v, TUns32 size, float alpha, float aspect,char mode)
{
	IMG_BrushRGBA32* brush = 0;
	int flag=0;
	try {
		IMG_ColorRGB c (1.0, 1.0, 1.0);
		brush = new IMG_BrushRGBA32 (size, size, c, alpha);
	}
	catch (...) {
		/* no brush , no fun ! */
		return;
	}

	TUns32 ro,ri;
	ro = size/2;
	ri = (int)(aspect/2.0f * size);
	if (ri > 2 ) ri =2;
	if (ri > ro ) ri =ro;
	brush->setRadii(ri,ro);

	
	TUns32 x, y;
	TUns32 xx, yy;
	getPixelAddress(u, v, x, y);
	xx = x - size/2;
	yy = y - size/2;
    if(mode == 't') flag = 1;

	/* now modify brush */
	for (int i= 0 ; i<(int)size;i++){
		for (int j= 0 ; j<(int)size;j++){
			
			float sR,sG,sB,sA;
			float cR,cG,cB=0.0;
			
if(mode == 't')
			IMG_PixmapRGBA32::getRGBAatTorus(xx+i,yy+j ,&cR,&cG,&cB,0);

else
  		IMG_PixmapRGBA32::getRGBAat(xx+i,yy+j ,&cR,&cG,&cB,0);
			int ccount = 1;
			/*
			cR += 7.0*cR;
			cG += 7.0*cG;
			cB += 7.0*cB;
			ccount += 7.0;
			*/

add_if_in(xx+i+1,yy+j+1,cR,cG,cB,ccount,flag);
add_if_in(xx+i+1,yy+j  ,cR,cG,cB,ccount,flag);
add_if_in(xx+i+1,yy+j-1,cR,cG,cB,ccount,flag);

add_if_in(xx+i,yy+j+1,cR,cG,cB,ccount,flag);
add_if_in(xx+i,yy+j-1,cR,cG,cB,ccount,flag);

add_if_in(xx+i-1,yy+j+1,cR,cG,cB,ccount,flag);
add_if_in(xx+i-1,yy+j  ,cR,cG,cB,ccount,flag);
add_if_in(xx+i-1,yy+j-1,cR,cG,cB,ccount,flag);


			sR =cR*255.0f/ccount;
			sG =cG*255.0f/ccount;
			sB =cB*255.0f/ccount;

			sA =255.0;
			brush->setRGBAat(i,j,&sR,&sG,&sB,NULL);
		}
	}

	/* apply */
if(mode == 't')
	IMG_PixmapRGBA32::blendPixmapTorus(x, y, *brush);
else
	IMG_PixmapRGBA32::blendPixmap(x, y, *brush);
	/* done  clean up */
   	if (brush) {
		delete ((IMG_BrushRGBA32*)brush);
		brush = 0;
	}
}


IMG_BrushRGBA32* IMG_CanvasRGBA32::LiftBrush(TUns32 x, TUns32 y, TUns32 size, float alpha, float aspect, short flags )
{
	IMG_BrushRGBA32* brush = 0;
	float mR,mG,mB=0.0;
	try {
		IMG_ColorRGB c (1.0, 1.0, 1.0);
		brush = new IMG_BrushRGBA32 (size, size, c, alpha);
	}
	catch (...) {
		/* no brush , no fun ! */
		return(NULL);
	}

	TUns32 ro,ri;
	ro = size/2;
	ri = (int)(aspect/2.0f * size);
	if (ri > 2 ) ri =2;
	if (ri > ro ) ri =ro;
	brush->setRadii(ri,ro);


	TUns32 xx, yy;
	xx = x - size/2;
	yy = y - size/2;
	IMG_PixmapRGBA32::getRGBAat(xx,yy,&mR,&mG,&mB,0);
	for (int i= 0 ; i<(int)size;i++){
		for (int j= 0 ; j<(int)size;j++){			
			float cR,cG,cB,cA=0.0;
			cR = mR; cG = mG; cB = mB;
			int res =IMG_PixmapRGBA32::getRGBAat(xx+i,yy+j ,&cR,&cG,&cB,0);
			cR *= 255.0f;
			cG *= 255.0f;
			cB *= 255.0f;
			cA *= 0.0;
			if (res)
			brush->setRGBAat(i,j,&cR,&cG,&cB,NULL);
			else 
			brush->setRGBAat(i,j,&cR,&cG,&cB,&cA);

		}
	}
	return(brush);
}

IMG_BrushRGBA32* IMG_CanvasRGBA32::LiftBrush(float u, float v, TUns32 size, float alpha, float aspect, short flags )
{
	IMG_BrushRGBA32* brush = 0;
	float mR,mG,mB=0.0;
	try {
		IMG_ColorRGB c (1.0, 1.0, 1.0);
		brush = new IMG_BrushRGBA32 (size, size, c, alpha);
	}
	catch (...) {
		/* no brush , no fun ! */
		return(NULL);
	}

	TUns32 ro,ri;
	ro = size/2;
	ri = (int)(aspect/2.0f * size);
	if (ri > 2 ) ri =2;
	if (ri > ro ) ri =ro;
	brush->setRadii(ri,ro);


	TUns32 x, y;
	TUns32 xx, yy;
	getPixelAddress(u, v, x, y);
	xx = x - size/2;
	yy = y - size/2;
	IMG_PixmapRGBA32::getRGBAat(xx,yy,&mR,&mG,&mB,0);
	for (int i= 0 ; i<(int)size;i++){
		for (int j= 0 ; j<(int)size;j++){			
			float cR,cG,cB=0.0;
			if (flags & 0x1)
				IMG_PixmapRGBA32::getRGBAatTorus(xx+i,yy+j ,&cR,&cG,&cB,0);
			else { 
				cR = mR; cG = mG; cB = mB;
				IMG_PixmapRGBA32::getRGBAat(xx+i,yy+j ,&cR,&cG,&cB,0);
			}
			cR *= 255.0f;
			cG *= 255.0f;
			cB *= 255.0f;
			brush->setRGBAat(i,j,&cR,&cG,&cB,NULL);
		}
	}
	return(brush);
}

void IMG_CanvasRGBA32::Smear(float uStart, float vStart, float uEnd, float vEnd, TUns32 size, float alpha, float aspect,char mode)
{
	IMG_BrushRGBA32* brush = NULL;
	float du,dv;
	du = uEnd - uStart;
	dv = vEnd - vStart;
	try {
		brush = LiftBrush(uStart-du,vStart-dv,size,alpha,aspect,1);
	}
	catch (...) {
		/* no brush , no fun ! */
		return;
	}
	if (brush){
	blendPixmap(uStart,vStart,uEnd,vEnd,*brush,mode);
	delete(brush);
	}

}

void IMG_CanvasRGBA32::CloneAt(IMG_CanvasRGBA32* other,float u,float v,float cu,float cv,TUns32 size,float alpha,float aspect)
{
	TUns32 x, y;
	TUns32 cx, cy;
	TUns32 xx, yy;
	getPixelAddress(u, v, x, y);
	getPixelAddress(cu, cv, cx, cy);

	xx = (x-cx);// - size/2;
	yy = (y-cy);// - size/2;

	if (other  == NULL) return;
	IMG_BrushRGBA32* brush = NULL;
	try {
		brush = other->LiftBrush(xx,yy,size,alpha,aspect,1);
	}
	catch (...) {
		/* no brush , no fun ! */
		return;
	}
	if (brush){
	IMG_PixmapRGBA32::blendPixmap(x, y, *brush);
	delete(brush);
	}

}



int IMG_CanvasRGBA32::add_if_in(int x, int y,float &R,float &G,float &B, int &count, short flags)

{
    float r,g,b= 0.0f;
	if ((flags & 0x1) == 0)
	{
		if (IMG_PixmapRGBA32::getRGBAat(x,y,&r,&g,&b,0))
		{
		R += r;
		G += g;
		B += b;
		count++;
		}
}
	else {

	IMG_PixmapRGBA32::getRGBAatTorus(x,y,&r,&g,&b,0);
		R += r;
		G += g;
		B += b;
		count++;
	}

	return 1;
}
