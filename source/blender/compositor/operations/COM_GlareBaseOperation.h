/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_GlareBaseOperation_h
#define _COM_GlareBaseOperation_h

#include "COM_SingleThreadedNodeOperation.h"
#include "DNA_node_types.h"


/* utility functions used by glare, tonemap and lens distortion */
/* soms macros for color handling */
typedef float fRGB[4];
/* clear color */
#define fRGB_clear(c) { c[0]=c[1]=c[2]=0.f; } (void)0
/* copy c2 to c1 */
#define fRGB_copy(c1, c2) { c1[0]=c2[0];  c1[1]=c2[1];  c1[2]=c2[2]; c1[3]=c2[3]; } (void)0
/* add c2 to c1 */
#define fRGB_add(c1, c2) { c1[0]+=c2[0];  c1[1]+=c2[1];  c1[2]+=c2[2]; } (void)0
/* subtract c2 from c1 */
#define fRGB_sub(c1, c2) { c1[0]-=c2[0];  c1[1]-=c2[1];  c1[2]-=c2[2]; } (void)0
/* multiply c by float value s */
#define fRGB_mult(c, s) { c[0]*=s;  c[1]*=s;  c[2]*=s; } (void)0
/* multiply c2 by s and add to c1 */
#define fRGB_madd(c1, c2, s) { c1[0]+=c2[0]*s;  c1[1]+=c2[1]*s;  c1[2]+=c2[2]*s; } (void)0
/* multiply c2 by color c1 */
#define fRGB_colormult(c, cs) { c[0]*=cs[0];  c[1]*=cs[1];  c[2]*=cs[2]; } (void)0
/* multiply c2 by color c3 and add to c1 */
#define fRGB_colormadd(c1, c2, c3) { c1[0]+=c2[0]*c3[0];  c1[1]+=c2[1]*c3[1];  c1[2]+=c2[2]*c3[2]; } (void)0
/* multiply c2 by color rgb, rgb as separate arguments */
#define fRGB_rgbmult(c, r, g, b) { c[0]*=(r);  c[1]*=(g);  c[2]*=(b); } (void)0
/* swap colors c1 & c2 */
#define fRGB_swap(c1, c2) { float _t=c1[0];  c1[0]=c2[0];  c2[0]=_t;\
                                  _t=c1[1];  c1[1]=c2[1];  c2[1]=_t;\
                                  _t=c1[2];  c1[2]=c2[2];  c2[2]=_t;\
                                  _t=c1[3];  c1[3]=c2[3];  c3[3]=_t;\
                          } (void)0


class GlareBaseOperation : public SingleThreadedNodeOperation {
private:
	/**
	  * @brief Cached reference to the inputProgram
	  */
	SocketReader * inputProgram;
	
	/**
	  * @brief settings of the glare node.
	  */
	NodeGlare * settings;
public:
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();

	void setGlareSettings(NodeGlare * settings) {this->settings = settings;}
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	
protected:
	GlareBaseOperation();
	
	virtual void generateGlare(float *data, MemoryBuffer *inputTile, NodeGlare *settings) = 0;
	
	MemoryBuffer *createMemoryBuffer(rcti *rect, MemoryBuffer **memoryBuffers);
	
};
#endif
