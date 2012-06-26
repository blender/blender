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

#include "COM_DoubleEdgeMaskOperation.h"
#include "BLI_math.h"
#include "DNA_node_types.h"
#include "MEM_guardedalloc.h"

// this part has been copied from the double edge mask
// Contributor(s): Peter Larabell.
static void do_adjacentKeepBorders(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize)
{
	int x;
	unsigned int isz = 0; // inner edge size
	unsigned int osz = 0; // outer edge size
	unsigned int gsz = 0; // gradient fill area size
	/* Test the four corners */
	/* upper left corner */
	x = t - rw + 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel underneath, or to the right, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + 1] && lomask[x + 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* upper right corner */
	x = t;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel underneath, or to the left, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x - 1] && lomask[x - 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                         // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* lower left corner */
	x = 0;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel above, or to the right, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x + 1] && lomask[x + 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* lower right corner */
	x = rw - 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel above, or to the left, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x - 1] && lomask[x - 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	
	/* Test the TOP row of pixels in buffer, except corners */
	for (x = t - 1; x >= (t - rw) + 2; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel to the right, or to the left, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	/* Test the BOTTOM row of pixels in buffer, except corners */
	for (x = rw - 2; x; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel to the right, or to the left, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	/* Test the LEFT edge of pixels in buffer, except corners */
	for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel underneath, or above, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	/* Test the RIGHT edge of pixels in buffer, except corners */
	for (x = t - rw; x > rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel underneath, or above, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	rsize[0] = isz;  // fill in our return sizes for edges + fill
	rsize[1] = osz;
	rsize[2] = gsz;
}

static void do_adjacentBleedBorders(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize)
{
	int x;
	unsigned int isz = 0; // inner edge size
	unsigned int osz = 0; // outer edge size
	unsigned int gsz = 0; // gradient fill area size
	/* Test the four corners */
	/* upper left corner */
	x = t - rw + 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel underneath, or to the right, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + 1] && lomask[x + 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x - rw] || !lomask[x + 1]) { // test if outer mask is empty underneath or to the right
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* upper right corner */
	x = t;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel underneath, or to the left, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x - 1] && lomask[x - 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x - rw] || !lomask[x - 1]) { // test if outer mask is empty underneath or to the left
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* lower left corner */
	x = 0;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel above, or to the right, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x + 1] && lomask[x + 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x + rw] || !lomask[x + 1]) { // test if outer mask is empty above or to the right
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* lower right corner */
	x = rw - 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if pixel above, or to the left, are empty in the inner mask,
		// but filled in the outer mask
		if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x - 1] && lomask[x - 1])) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x + rw] || !lomask[x - 1]) { // test if outer mask is empty above or to the left
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* Test the TOP row of pixels in buffer, except corners */
	for (x = t - 1; x >= (t - rw) + 2; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel to the left, or to the right, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - 1] || !lomask[x + 1]) {   // test if outer mask is empty to the left or to the right
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	/* Test the BOTTOM row of pixels in buffer, except corners */
	for (x = rw - 2; x; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel to the left, or to the right, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                     // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - 1] || !lomask[x + 1]) { // test if outer mask is empty to the left or to the right
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	/* Test the LEFT edge of pixels in buffer, except corners */
	for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel underneath, or above, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - rw] || !lomask[x + rw]) { // test if outer mask is empty underneath or above
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	/* Test the RIGHT edge of pixels in buffer, except corners */
	for (x = t - rw; x > rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if pixel underneath, or above, are empty in the inner mask,
			// but filled in the outer mask
			if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - rw] || !lomask[x + rw]) { // test if outer mask is empty underneath or above
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	rsize[0] = isz;  // fill in our return sizes for edges + fill
	rsize[1] = osz;
	rsize[2] = gsz;
}

static void do_allKeepBorders(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize)
{
	int x;
	unsigned int isz = 0; // inner edge size
	unsigned int osz = 0; // outer edge size
	unsigned int gsz = 0; // gradient fill area size
	/* Test the four corners */
	/* upper left corner */
	x = t - rw + 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if the inner mask is empty underneath or to the right
		if (!limask[x - rw] || !limask[x + 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* upper right corner */
	x = t;
	// test if inner mask is filled
	if (limask[x]) {
		// test if the inner mask is empty underneath or to the left
		if (!limask[x - rw] || !limask[x - 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* lower left corner */
	x = 0;
	// test if inner mask is filled
	if (limask[x]) {
		// test if inner mask is empty above or to the right
		if (!limask[x + rw] || !limask[x + 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	/* lower right corner */
	x = rw - 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if inner mask is empty above or to the left
		if (!limask[x + rw] || !limask[x - 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		osz++;                                   // increment outer edge size
		lres[x] = 3;                             // flag pixel as outer edge
	}
	
	/* Test the TOP row of pixels in buffer, except corners */
	for (x = t - 1; x >= (t - rw) + 2; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty to the left or to the right
			if (!limask[x - 1] || !limask[x + 1]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	/* Test the BOTTOM row of pixels in buffer, except corners */
	for (x = rw - 2; x; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty to the left or to the right
			if (!limask[x - 1] || !limask[x + 1]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	/* Test the LEFT edge of pixels in buffer, except corners */
	for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty underneath or above
			if (!limask[x - rw] || !limask[x + rw]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	/* Test the RIGHT edge of pixels in buffer, except corners */
	for (x = t - rw; x > rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty underneath or above
			if (!limask[x - rw] || !limask[x + rw]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
	}
	
	rsize[0] = isz;  // fill in our return sizes for edges + fill
	rsize[1] = osz;
	rsize[2] = gsz;
}

static void do_allBleedBorders(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize)
{
	int x;
	unsigned int isz = 0; // inner edge size
	unsigned int osz = 0; // outer edge size
	unsigned int gsz = 0; // gradient fill area size
	/* Test the four corners */
	/* upper left corner */
	x = t - rw + 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if the inner mask is empty underneath or to the right
		if (!limask[x - rw] || !limask[x + 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x - rw] || !lomask[x + 1]) { // test if outer mask is empty underneath or to the right
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* upper right corner */
	x = t;
	// test if inner mask is filled
	if (limask[x]) {
		// test if the inner mask is empty underneath or to the left
		if (!limask[x - rw] || !limask[x - 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x - rw] || !lomask[x - 1]) { // test if outer mask is empty above or to the left
			osz++;                               // increment outer edge size
			lres[x] = 3;                           // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* lower left corner */
	x = 0;
	// test if inner mask is filled
	if (limask[x]) {
		// test if inner mask is empty above or to the right
		if (!limask[x + rw] || !limask[x + 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x + rw] || !lomask[x + 1]) { // test if outer mask is empty underneath or to the right
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* lower right corner */
	x = rw - 1;
	// test if inner mask is filled
	if (limask[x]) {
		// test if inner mask is empty above or to the left
		if (!limask[x + rw] || !limask[x - 1]) {
			isz++;                               // increment inner edge size
			lres[x] = 4;                         // flag pixel as inner edge
		}
		else {
			res[x] = 1.0f;                       // pixel is just part of inner mask, and it's not an edge
		}
	}
	else if (lomask[x]) {                        // inner mask was empty, test if outer mask is filled
		if (!lomask[x + rw] || !lomask[x - 1]) { // test if outer mask is empty underneath or to the left
			osz++;                               // increment outer edge size
			lres[x] = 3;                         // flag pixel as outer edge
		}
		else {
			gsz++;                               // increment the gradient pixel count
			lres[x] = 2;                         // flag pixel as gradient
		}
	}
	/* Test the TOP row of pixels in buffer, except corners */
	for (x = t - 1; x >= (t - rw) + 2; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty to the left or to the right
			if (!limask[x - 1] || !limask[x + 1]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - 1] || !lomask[x + 1]) { // test if outer mask is empty to the left or to the right
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	/* Test the BOTTOM row of pixels in buffer, except corners */
	for (x = rw - 2; x; x--) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty to the left or to the right
			if (!limask[x - 1] || !limask[x + 1]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - 1] || !lomask[x + 1]) {   // test if outer mask is empty to the left or to the right
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	/* Test the LEFT edge of pixels in buffer, except corners */
	for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty underneath or above
			if (!limask[x - rw] || !limask[x + rw]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                   // inner mask was empty, test if outer mask is filled
			if (!lomask[x - rw] || !lomask[x + rw]) { // test if outer mask is empty underneath or above
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	/* Test the RIGHT edge of pixels in buffer, except corners */
	for (x = t - rw; x > rw; x -= rw) {
		// test if inner mask is filled
		if (limask[x]) {
			// test if inner mask is empty underneath or above
			if (!limask[x - rw] || !limask[x + rw]) {
				isz++;                           // increment inner edge size
				lres[x] = 4;                     // flag pixel as inner edge
			}
			else {
				res[x] = 1.0f;                   // pixel is just part of inner mask, and it's not an edge
			}
		}
		else if (lomask[x]) {                    // inner mask was empty, test if outer mask is filled
			if (!lomask[x - rw] || !lomask[x + rw]) { // test if outer mask is empty underneath or above
				osz++;                           // increment outer edge size
				lres[x] = 3;                     // flag pixel as outer edge
			}
			else {
				gsz++;                           // increment the gradient pixel count
				lres[x] = 2;                     // flag pixel as gradient
			}
		}
	}
	
	rsize[0] = isz;  // fill in our return sizes for edges + fill
	rsize[1] = osz;
	rsize[2] = gsz;
}

static void do_allEdgeDetection(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize, unsigned int in_isz, unsigned int in_osz, unsigned int in_gsz)
{
	int x;                             // x = pixel loop counter
	int a;                             // a = pixel loop counter
	int dx;                            // dx = delta x
	int pix_prevRow;                   // pix_prevRow = pixel one row behind the one we are testing in a loop
	int pix_nextRow;                   // pix_nextRow = pixel one row in front of the one we are testing in a loop
	int pix_prevCol;                   // pix_prevCol = pixel one column behind the one we are testing in a loop
	int pix_nextCol;                   // pix_nextCol = pixel one column in front of the one we are testing in a loop
	/* Test all rows between the FIRST and LAST rows, excluding left and right edges */
	for (x = (t - rw) + 1, dx = x - (rw - 2); dx > rw; x -= rw, dx -= rw) {
		a = x - 2;
		pix_prevRow = a + rw;
		pix_nextRow = a - rw;
		pix_prevCol = a + 1;
		pix_nextCol = a - 1;
		while (a > dx - 2) {
			if (!limask[a]) {             // if the inner mask is empty
				if (lomask[a]) {          // if the outer mask is full
					/*
					   Next we test all 4 directions around the current pixel: next/prev/up/down
					   The test ensures that the outer mask is empty and that the inner mask
					   is also empty. If both conditions are true for any one of the 4 adjacent pixels
					   then the current pixel is counted as being a true outer edge pixel.
					 */
					if ((!lomask[pix_nextCol] && !limask[pix_nextCol]) ||
					    (!lomask[pix_prevCol] && !limask[pix_prevCol]) ||
					    (!lomask[pix_nextRow] && !limask[pix_nextRow]) ||
					    (!lomask[pix_prevRow] && !limask[pix_prevRow]))
					{
						in_osz++;                // increment the outer boundary pixel count
						lres[a] = 3;             // flag pixel as part of outer edge
					}
					else {                       // it's not a boundary pixel, but it is a gradient pixel
						in_gsz++;                // increment the gradient pixel count
						lres[a] = 2;             // flag pixel as gradient
					}
				}
				
			}
			else {
				if (!limask[pix_nextCol] || !limask[pix_prevCol] || !limask[pix_nextRow] || !limask[pix_prevRow]) {
					in_isz++;                    // increment the inner boundary pixel count
					lres[a] = 4;                 // flag pixel as part of inner edge
				}
				else {
					res[a] = 1.0f;               // pixel is part of inner mask, but not at an edge
				}
			}
			a--;
			pix_prevRow--;
			pix_nextRow--;
			pix_prevCol--;
			pix_nextCol--;
		}
	}
	
	rsize[0] = in_isz;  // fill in our return sizes for edges + fill
	rsize[1] = in_osz;
	rsize[2] = in_gsz;
}

static void do_adjacentEdgeDetection(unsigned int t, unsigned int rw, unsigned int *limask, unsigned int *lomask, unsigned int *lres, float *res, unsigned int *rsize, unsigned int in_isz, unsigned int in_osz, unsigned int in_gsz)
{
	int x;                             // x = pixel loop counter
	int a;                             // a = pixel loop counter
	int dx;                            // dx = delta x
	int pix_prevRow;                   // pix_prevRow = pixel one row behind the one we are testing in a loop
	int pix_nextRow;                   // pix_nextRow = pixel one row in front of the one we are testing in a loop
	int pix_prevCol;                   // pix_prevCol = pixel one column behind the one we are testing in a loop
	int pix_nextCol;                   // pix_nextCol = pixel one column in front of the one we are testing in a loop
	/* Test all rows between the FIRST and LAST rows, excluding left and right edges */
	for (x = (t - rw) + 1, dx = x - (rw - 2); dx > rw; x -= rw, dx -= rw) {
		a = x - 2;
		pix_prevRow = a + rw;
		pix_nextRow = a - rw;
		pix_prevCol = a + 1;
		pix_nextCol = a - 1;
		while (a > dx - 2) {
			if (!limask[a]) {                    // if the inner mask is empty
				if (lomask[a]) {                 // if the outer mask is full
					/*
					   Next we test all 4 directions around the current pixel: next/prev/up/down
					   The test ensures that the outer mask is empty and that the inner mask
					   is also empty. If both conditions are true for any one of the 4 adjacent pixels
					   then the current pixel is counted as being a true outer edge pixel.
					 */
					if ((!lomask[pix_nextCol] && !limask[pix_nextCol]) ||
					    (!lomask[pix_prevCol] && !limask[pix_prevCol]) ||
					    (!lomask[pix_nextRow] && !limask[pix_nextRow]) ||
					    (!lomask[pix_prevRow] && !limask[pix_prevRow]))
					{
						in_osz++;                // increment the outer boundary pixel count
						lres[a] = 3;             // flag pixel as part of outer edge
					}
					else {                       // it's not a boundary pixel, but it is a gradient pixel
						in_gsz++;                // increment the gradient pixel count
						lres[a] = 2;             // flag pixel as gradient
					}
				}
				
			}
			else {
				if ((!limask[pix_nextCol] && lomask[pix_nextCol]) ||
				    (!limask[pix_prevCol] && lomask[pix_prevCol]) ||
				    (!limask[pix_nextRow] && lomask[pix_nextRow]) ||
				    (!limask[pix_prevRow] && lomask[pix_prevRow]))
				{
					in_isz++;                    // increment the inner boundary pixel count
					lres[a] = 4;                 // flag pixel as part of inner edge
				}
				else {
					res[a] = 1.0f;               // pixel is part of inner mask, but not at an edge
				}
			}
			a--;
			pix_prevRow--;                       // advance all four "surrounding" pixel pointers
			pix_nextRow--;
			pix_prevCol--;
			pix_nextCol--;
		}
	}
	
	rsize[0] = in_isz;  // fill in our return sizes for edges + fill
	rsize[1] = in_osz;
	rsize[2] = in_gsz;
}

static void do_createEdgeLocationBuffer(unsigned int t, unsigned int rw, unsigned int *lres, float *res, unsigned short *gbuf, unsigned int *innerEdgeOffset, unsigned int *outerEdgeOffset, unsigned int isz, unsigned int gsz)
{
	int x;                             // x = pixel loop counter
	int a;                             // a = temporary pixel index buffer loop counter
	unsigned int ud;                   // ud = unscaled edge distance
	unsigned int dmin;                 // dmin = minimun edge distance
	
	unsigned int rsl;                  // long used for finding fast 1.0/sqrt
	unsigned int gradientFillOffset;
	unsigned int innerAccum = 0;       // for looping inner edge pixel indexes, represents current position from offset
	unsigned int outerAccum = 0;       // for looping outer edge pixel indexes, represents current position from offset
	unsigned int gradientAccum = 0;    // for looping gradient pixel indexes, represents current position from offset
	/*
	   Here we compute the size of buffer needed to hold (row,col) coordinates
	   for each pixel previously determined to be either gradient, inner edge,
	   or outer edge.

	   Allocation is done by requesting 4 bytes "sizeof(int)" per pixel, even
	   though gbuf[] is declared as unsigned short* (2 bytes) because we don't
	   store the pixel indexes, we only store x,y location of pixel in buffer.

	   This does make the assumption that x and y can fit in 16 unsigned bits
	   so if Blender starts doing renders greater than 65536 in either direction
	   this will need to allocate gbuf[] as unsigned int *and allocate 8 bytes
	   per flagged pixel.

	   In general, the buffer on-screen:

	   Example:  9 by 9 pixel block

	   . = pixel non-white in both outer and inner mask
	   o = pixel white in outer, but not inner mask, adjacent to "." pixel
	   g = pixel white in outer, but not inner mask, not adjacent to "." pixel
	   i = pixel white in inner mask, adjacent to "g" or "." pixel
	   F = pixel white in inner mask, only adjacent to other pixels white in the inner mask


	   .........   <----- pixel #80
	   ..oooo...
	   .oggggo..
	   .oggiggo.
	   .ogiFigo.
	   .oggiggo.
	   .oggggo..
	   ..oooo...
	   pixel #00 -----> .........

	   gsz = 18   (18 "g" pixels above)
	   isz = 4    (4 "i" pixels above)
	   osz = 18   (18 "o" pixels above)


	   The memory in gbuf[] after filling will look like this:

	   gradientFillOffset (0 pixels)                   innerEdgeOffset (18 pixels)    outerEdgeOffset (22 pixels)
	   /                                               /                              /
	   /                                               /                              /
	   |X   Y   X   Y   X   Y   X   Y   >     <X   Y   X   Y   >     <X   Y   X   Y   X   Y   >     <X   Y   X   Y   | <- (x,y)
	   +-------------------------------->     <---------------->     <------------------------>     <----------------+
	   |0   2   4   6   8   10  12  14  > ... <68  70  72  74  > ... <80  82  84  86  88  90  > ... <152 154 156 158 | <- bytes
	   +-------------------------------->     <---------------->     <------------------------>     <----------------+
	   |g0  g0  g1  g1  g2  g2  g3  g3  >     <g17 g17 i0  i0  >     <i2  i2  i3  i3  o0  o0  >     <o16 o16 o17 o17 | <- pixel
	         /                              /                              /
	        /                              /                              /
	          /                              /                              /
	   +---------- gradientAccum (18) ---------+      +--- innerAccum (22) ---+      +--- outerAccum (40) ---+


	   Ultimately we do need the pixel's memory buffer index to set the output
	   pixel color, but it's faster to reconstruct the memory buffer location
	   each iteration of the final gradient calculation than it is to deconstruct
	   a memory location into x,y pairs each round.
	 */


	gradientFillOffset = 0;                            // since there are likely "more" of these, put it first. :)
	*innerEdgeOffset = gradientFillOffset + gsz;       // set start of inner edge indexes
	*outerEdgeOffset = (*innerEdgeOffset) + isz;       // set start of outer edge indexes
	/* set the accumulators to correct positions */    // set up some accumulator variables for loops
	gradientAccum = gradientFillOffset;                // each accumulator variable starts at its respective
	innerAccum = *innerEdgeOffset;                     // section's offset so when we start filling, each
	outerAccum = *outerEdgeOffset;                     // section fills up it's allocated space in gbuf
	//uses dmin=row, rsl=col
	for (x = 0, dmin = 0; x < t; x += rw, dmin++) {
		for (rsl = 0; rsl < rw; rsl++) {
			a = x + rsl;
			if (lres[a] == 2) {                  // it is a gradient pixel flagged by 2
				ud = gradientAccum << 1;         // double the index to reach correct unsigned short location
				gbuf[ud] = dmin;                 // insert pixel's row into gradient pixel location buffer
				gbuf[ud + 1] = rsl;              // insert pixel's column into gradient pixel location buffer
				gradientAccum++;                 // increment gradient index buffer pointer
			}
			else if (lres[a] == 3) {             // it is an outer edge pixel flagged by 3
				ud = outerAccum << 1;            // double the index to reach correct unsigned short location
				gbuf[ud] = dmin;                 // insert pixel's row into outer edge pixel location buffer
				gbuf[ud + 1] = rsl;              // insert pixel's column into outer edge pixel location buffer
				outerAccum++;                    // increment outer edge index buffer pointer
				res[a] = 0.0f;                   // set output pixel intensity now since it won't change later
			}
			else if (lres[a] == 4) {             // it is an inner edge pixel flagged by 4
				ud = innerAccum << 1;            // double int index to reach correct unsigned short location
				gbuf[ud] = dmin;                 // insert pixel's row into inner edge pixel location buffer
				gbuf[ud + 1] = rsl;              // insert pixel's column into inner edge pixel location buffer
				innerAccum++;                    // increment inner edge index buffer pointer
				res[a] = 1.0f;                   // set output pixel intensity now since it won't change later
			}
		}
	}
	
}

static void do_fillGradientBuffer(unsigned int rw, float *res, unsigned short *gbuf, unsigned int isz, unsigned int osz, unsigned int gsz, unsigned int innerEdgeOffset, unsigned int outerEdgeOffset)
{
	int x;                             // x = pixel loop counter
	int a;                             // a = temporary pixel index buffer loop counter
	int fsz;                           // size of the frame
	unsigned int rsl;                  // long used for finding fast 1.0/sqrt
	float rsf;                         // float used for finding fast 1.0/sqrt
	const float rsopf = 1.5f;          // constant float used for finding fast 1.0/sqrt
	
	unsigned int gradientFillOffset;
	unsigned int t;
	unsigned int ud;                   // ud = unscaled edge distance
	unsigned int dmin;                 // dmin = minimun edge distance
	float odist;                       // odist = current outer edge distance
	float idist;                       // idist = current inner edge distance
	int dx;                            // dx = X-delta (used for distance proportion calculation)
	int dy;                            // dy = Y-delta (used for distance proportion calculation)
	
	/*
	 * The general algorithm used to color each gradient pixel is:
	 *
	 * 1.) Loop through all gradient pixels.
	 * A.) For each gradient pixel:
	 * a.) Loop though all outside edge pixels, looking for closest one
	 * to the gradient pixel we are in.
	 * b.) Loop through all inside edge pixels, looking for closest one
	 * to the gradient pixel we are in.
	 * c.) Find proportion of distance from gradient pixel to inside edge
	 * pixel compared to sum of distance to inside edge and distance to
	 * outside edge.
	 *
	 * In an image where:
	 * . = blank (black) pixels, not covered by inner mask or outer mask
	 * + = desired gradient pixels, covered only by outer mask
	 * * = white full mask pixels, covered by at least inner mask
	 *
	 * ...............................
	 * ...............+++++++++++.....
	 * ...+O++++++..++++++++++++++....
	 * ..+++\++++++++++++++++++++.....
	 * .+++++G+++++++++*******+++.....
	 * .+++++|+++++++*********+++.....
	 * .++***I****************+++.....
	 * .++*******************+++......
	 * .+++*****************+++.......
	 * ..+++***************+++........
	 * ....+++**********+++...........
	 * ......++++++++++++.............
	 * ...............................
	 *
	 * O = outside edge pixel
	 * \
	 *  G = gradient pixel
	 *  |
	 *  I = inside edge pixel
	 * 
	 *   __
	 *  *note that IO does not need to be a straight line, in fact
	 *  many cases can arise where straight lines do not work
	 *  correctly.
	 *
	 *     __       __     __
	 * d.) Pixel color is assigned as |GO| / ( |GI| + |GO| )
	 *
	 * The implementation does not compute distance, but the reciprocal of the
	 * distance. This is done to avoid having to compute a square root, as a
	 * reciprocal square root can be computed faster. Therefore, the code computes
	 * pixel color as |GI| / (|GI| + |GO|). Since these are reciprocals, GI serves the
	 * purpose of GO for the proportion calculation.
	 *
	 * For the purposes of the minimun distance comparisons, we only check
	 * the sums-of-squares against eachother, since they are in the same
	 * mathematical sort-order as if we did go ahead and take square roots
	 *
	 * Loop through all gradient pixels.
	 */

	for (x = gsz - 1; x >= 0; x--) {
		gradientFillOffset = x << 1;
		t = gbuf[gradientFillOffset];            // calculate column of pixel indexed by gbuf[x]
		fsz = gbuf[gradientFillOffset + 1];      // calculate row of pixel indexed by gbuf[x]
		dmin = 0xffffffff;                       // reset min distance to edge pixel
		for (a = outerEdgeOffset + osz - 1; a >= outerEdgeOffset; a--) {   // loop through all outer edge buffer pixels
			ud = a << 1;
			dy = t - gbuf[ud];                   // set dx to gradient pixel column - outer edge pixel row
			dx = fsz - gbuf[ud + 1];             // set dy to gradient pixel row - outer edge pixel column
			ud = dx * dx + dy * dy;              // compute sum of squares
			if (ud < dmin) {                     // if our new sum of squares is less than the current minimum
				dmin = ud;                       // set a new minimum equal to the new lower value
			}
		}
		odist = (float)(dmin);                   // cast outer min to a float
		rsf = odist * 0.5f;                      //
		rsl = *(unsigned int *)&odist;           // use some peculiar properties of the way bits are stored
		rsl = 0x5f3759df - (rsl >> 1);           // in floats vs. unsigned ints to compute an approximate
		odist = *(float *)&rsl;                  // reciprocal square root
		odist = odist * (rsopf - (rsf * odist * odist));   // -- ** this line can be iterated for more accuracy ** --
		dmin = 0xffffffff;                       // reset min distance to edge pixel
		for (a = innerEdgeOffset + isz - 1; a >= innerEdgeOffset; a--) {   // loop through all inside edge pixels
			ud = a << 1;
			dy = t - gbuf[ud];         // compute delta in Y from gradient pixel to inside edge pixel
			dx = fsz - gbuf[ud + 1];     // compute delta in X from gradient pixel to inside edge pixel
			ud = dx * dx + dy * dy;        // compute sum of squares
			if (ud < dmin) {          // if our new sum of squares is less than the current minimum we've found
				dmin = ud;           // set a new minimum equal to the new lower value
			}
		}
		idist = (float)(dmin);                   // cast inner min to a float
		rsf = idist * 0.5f;                      //
		rsl = *(unsigned int *)&idist;           //
		rsl = 0x5f3759df - (rsl >> 1);           // see notes above
		idist = *(float *)&rsl;                  //
		idist = idist * (rsopf - (rsf * idist * idist));   //
		/*
		 * Note once again that since we are using reciprocals of distance values our
		 * proportion is already the correct intensity, and does not need to be
		 * subracted from 1.0 like it would have if we used real distances.
		 */
		
		/*
		 * Here we reconstruct the pixel's memory location in the CompBuf by
		 * Pixel Index = Pixel Column + ( Pixel Row * Row Width )
		 */
		res[gbuf[gradientFillOffset + 1] + (gbuf[gradientFillOffset] * rw)] = (idist / (idist + odist));    //set intensity
	}
	
}

// end of copy

void DoubleEdgeMaskOperation::doDoubleEdgeMask(float *imask, float *omask, float *res)
{
	unsigned int *lres;                // lres = unsigned int pointer to output pixel buffer (for bit operations)
	unsigned int *limask;              // limask = unsigned int pointer to inner mask (for bit operations)
	unsigned int *lomask;              // lomask = unsigned int pointer to outer mask (for bit operations)
	
	int rw;                            // rw = pixel row width
	int t;                             // t = total number of pixels in buffer - 1 (used for loop starts)
	int fsz;                           // size of the frame
	
	unsigned int isz = 0;                // size (in pixels) of inside edge pixel index buffer
	unsigned int osz = 0;                // size (in pixels) of outside edge pixel index buffer
	unsigned int gsz = 0;                // size (in pixels) of gradient pixel index buffer
	unsigned int rsize[3];               // size storage to pass to helper functions
	unsigned int innerEdgeOffset = 0;    // offset into final buffer where inner edge pixel indexes start
	unsigned int outerEdgeOffset = 0;    // offset into final buffer where outer edge pixel indexes start
	
	unsigned short *gbuf;              // gradient/inner/outer pixel location index buffer
	
	if (true) {                    // if both input sockets have some data coming in...
		
		t = (this->getWidth() * this->getHeight()) - 1;                                // determine size of the frame
		
		lres = (unsigned int *)res;      // unsigned int pointer to output buffer (for bit level ops)
		limask = (unsigned int *)imask;   // unsigned int pointer to input mask (for bit level ops)
		lomask = (unsigned int *)omask;   // unsigned int pointer to output mask (for bit level ops)
		rw = this->getWidth();                   // width of a row of pixels
		
		
		/*
		 * The whole buffer is broken up into 4 parts. The four CORNERS, the FIRST and LAST rows, the
		 * LEFT and RIGHT edges (excluding the corner pixels), and all OTHER rows.
		 * This allows for quick computation of outer edge pixels where
		 * a screen edge pixel is marked to be gradient.
		 *
		 * The pixel type (gradient vs inner-edge vs outer-edge) tests change
		 * depending on the user selected "Inner Edge Mode" and the user selected
		 * "Buffer Edge Mode" on the node's GUI. There are 4 sets of basically the
		 * same algorithm:
		 *
		 * 1.) Inner Edge -> Adjacent Only
		 *   Buffer Edge -> Keep Inside
		 *
		 * 2.) Inner Edge -> Adjacent Only
		 *   Buffer Edge -> Bleed Out
		 *
		 * 3.) Inner Edge -> All
		 *   Buffer Edge -> Keep Inside
		 *
		 * 4.) Inner Edge -> All
		 *   Buffer Edge -> Bleed Out
		 *
		 * Each version has slightly different criteria for detecting an edge pixel.
		 */
		if (this->m_adjecentOnly) {              // if "adjacent only" inner edge mode is turned on
			if (this->m_keepInside) {            // if "keep inside" buffer edge mode is turned on
				do_adjacentKeepBorders(t, rw, limask, lomask, lres, res, rsize);
			}
			else {                               // "bleed out" buffer edge mode is turned on
				do_adjacentBleedBorders(t, rw, limask, lomask, lres, res, rsize);
			}
			isz = rsize[0];                      // set up inner edge, outer edge, and gradient buffer sizes after border pass
			osz = rsize[1];
			gsz = rsize[2];
			// detect edges in all non-border pixels in the buffer
			do_adjacentEdgeDetection(t, rw, limask, lomask, lres, res, rsize, isz, osz, gsz);
		}
		else {                                   // "all" inner edge mode is turned on
			if (this->m_keepInside) {              // if "keep inside" buffer edge mode is turned on
				do_allKeepBorders(t, rw, limask, lomask, lres, res, rsize);
			}
			else {                               // "bleed out" buffer edge mode is turned on
				do_allBleedBorders(t, rw, limask, lomask, lres, res, rsize);
			}
			isz = rsize[0];                      // set up inner edge, outer edge, and gradient buffer sizes after border pass
			osz = rsize[1];
			gsz = rsize[2];
			// detect edges in all non-border pixels in the buffer
			do_allEdgeDetection(t, rw, limask, lomask, lres, res, rsize, isz, osz, gsz);
		}
		
		isz = rsize[0];                          // set edge and gradient buffer sizes once again...
		osz = rsize[1];                          // the sizes in rsize[] may have been modified
		gsz = rsize[2];                          // by the do_*EdgeDetection() function.
		
		fsz = gsz + isz + osz;                                   // calculate size of pixel index buffer needed
		gbuf = (unsigned short *)MEM_callocN(sizeof(unsigned short) * fsz * 2, "DEM"); // allocate edge/gradient pixel index buffer
		
		do_createEdgeLocationBuffer(t, rw, lres, res, gbuf, &innerEdgeOffset, &outerEdgeOffset, isz, gsz);
		do_fillGradientBuffer(rw, res, gbuf, isz, osz, gsz, innerEdgeOffset, outerEdgeOffset);
		
		MEM_freeN(gbuf);                         // free the gradient index buffer
	}
}

DoubleEdgeMaskOperation::DoubleEdgeMaskOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_inputInnerMask = NULL;
	this->m_inputOuterMask = NULL;
	this->m_adjecentOnly = false;
	this->m_keepInside = false;
	this->setComplex(true);
}

bool DoubleEdgeMaskOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->m_cachedInstance == NULL) {
		rcti newInput;
		newInput.xmax = this->getWidth();
		newInput.xmin = 0;
		newInput.ymax = this->getHeight();
		newInput.ymin = 0;
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
	else {
		return false;
	}
}

void DoubleEdgeMaskOperation::initExecution()
{
	this->m_inputInnerMask = this->getInputSocketReader(0);
	this->m_inputOuterMask = this->getInputSocketReader(1);
	initMutex();
	this->m_cachedInstance = NULL;
}

void *DoubleEdgeMaskOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->m_cachedInstance)
		return this->m_cachedInstance;
	
	lockMutex();
	if (this->m_cachedInstance == NULL) {
		MemoryBuffer *innerMask = (MemoryBuffer *)this->m_inputInnerMask->initializeTileData(rect, memoryBuffers);
		MemoryBuffer *outerMask = (MemoryBuffer *)this->m_inputOuterMask->initializeTileData(rect, memoryBuffers);
		float *data = new float[this->getWidth() * this->getHeight()];
		float *imask = innerMask->convertToValueBuffer();
		float *omask = outerMask->convertToValueBuffer();
		doDoubleEdgeMask(imask, omask, data);
		delete imask;
		delete omask;
		this->m_cachedInstance = data;
	}
	unlockMutex();
	return this->m_cachedInstance;
}
void DoubleEdgeMaskOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float *buffer = (float *) data;
	int index = (y * this->getWidth() + x);
	color[0] = buffer[index];
	color[1] = buffer[index + 1];
	color[2] = buffer[index + 2];
	color[3] = buffer[index + 3];
}

void DoubleEdgeMaskOperation::deinitExecution()
{
	this->m_inputInnerMask = NULL;
	this->m_inputOuterMask = NULL;
	deinitMutex();
	if (this->m_cachedInstance) {
		delete this->m_cachedInstance;
		this->m_cachedInstance = NULL;
	}
}

