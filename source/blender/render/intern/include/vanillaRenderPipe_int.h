/*
 * vanillaRenderPipe_int.h
 *
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef VANILLARENDERPIPE_INT_H
#define VANILLARENDERPIPE_INT_H

#include "vanillaRenderPipe_types.h"
#include "zbufferdatastruct_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * Z buffer initializer, for new pipeline.
 * <LI>
 * <IT> AColourBuffer : colour buffer for one line
 * <IT> APixbufExt    : pixel data buffer for one line, depth RE_ZBUFLEN 
 * </LI>
 */
void initRenderBuffers(int width);
/*  void initRenderBuffers(void); */

/**
 * Z buffer destructor, frees stuff from initZBuffers().
 */
void freeRenderBuffers(void);

/** 
 * Fill the accumulation buffer APixbufExt with face and halo indices. 
 * Note: Uses globals.
 * @param y the line number to set
 */
void calcZBufLine(int y);

/** 
 * Shade and render the pixels in this line, into AColourBuffer
 * Note: Uses globals.
 * @param y the line number to set
 */
void renderZBufLine(int y);

/**
 * Count and sort the list behind ap into buf. Sorts on min. distance.
 * Low index <=> high z
 */
int countAndSortPixelFaces(int buf[RE_MAX_FACES_PER_PIXEL][5], 
                           RE_APixstrExt *ap);

/** 
 * Compose the conflict and colour stacks 
 * Note: Uses globals.
 */
int composeStack(int zrow[RE_MAX_FACES_PER_PIXEL][RE_PIXELFIELDSIZE],
				 struct RE_faceField* stack, int ptr,
				 int totvlak, float x, float y, int osaNr);
/**
 * Integrate conflicting layers.
 * Note: Uses globals.
 */
int resolveConflict(struct RE_faceField* stack, int ptr, float x, float y);

/**
 * Integrate the colour stack, defer conflicts.
 * Note: Uses globals.
 */
void integrateStack(struct RE_faceField* stack, int ptr,
					float x, float y, int osaNr);
					
/**
 * Calculate the view depth to this object on this location, with 
 * the current view parameters in R.
 */
int calcDepth(float x, float y, void *data, int type);



/**
 * Fills in distances of all faces in a z buffer, for given jitter settings.
 */
int fillZBufDistances(void);

/**
 * Fills in distances of faces in the z buffer.
 *
 * Halo z buffering ---------------------------------------------- 
 *
 * A halo is treated here as a billboard: no z-extension, always   
 * oriented perpendicular to the viewer. The rest of the z-buffer  
 * stores face-numbers first, then calculates colours as the       
 * final image is rendered. We'll use the same approach here,      
 * which differs from the original method (which was add halos per 
 * scan line). This means that the z-buffer now also needs to      
 * store info about what sort of 'thing' the index refers to.      
 *                                                                 
 * Halo extension:                                                 
 * h.maxy  ---------                                               
 *         |          h.xs + h.rad                                 
 *             |      h.xs                                         
 *                 |  h.xs - h.rad                                 
 * h.miny  ---------                                               
 *                                                                 
 * These coordinates must be clipped to picture size.              
 * I'm not quite certain about halo numbering.                     
 *                                                                 
 * Halos and jittering -------------------------------------------  
 *                                                                 
 * Halos were not jittered previously. Now they are. I wonder      
 * whether this may have some adverse effects here.                
 
 * @return 1 for succes, 0 if the operation was interrupted.
 */
int zBufferAllFaces(void);

/**
 * Fills in distances of halos in the z buffer.
 * @return 1 for succes, 0 if the operation was interrupted.
 */
int zBufferAllHalos(void);

/**
 * New fill function for z buffer, for edge-only rendering.
 */
void zBufferFillEdge(float *vec1, float *vec2);

/**
 * New fill function for z buffer.
 */
void zBufferFillFace(float *v1, float *v2, float *v3);

/**
 * One more filler: fill in halo data in z buffer.
 * Empty so far, but may receive content of halo loop.
 */
void zBufferFillHalo(void);

/**
 * Copy the colour buffer output to R.rectot, to line y.
 */
void transferColourBufferToOutput(int y);

/**
 * Set the colour buffer fields to zero.
 */
void eraseColBuf(RE_COLBUFTYPE *buf);

/**
 * Blend source over dest, and leave result in dest. 1 pixel.
 */
void blendOverFloat(int type, float* dest, float* source, void* data);

/**
 * Blend source over dest, and leave result in dest. 1 pixel into 
 * multiple bins.
 */
void blendOverFloatRow(int type, float* dest, float* source, 
                       void* data, int mask, int osaNr) ;

/**
 * Do a post-process step on a finalized render image.
 */
void std_transFloatColV2CharColV( RE_COLBUFTYPE *buf, char *target);

#endif /* VANILLARENDERPIPE_INT_H */

