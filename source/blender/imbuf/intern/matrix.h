/**
 * matrix.c
 *
 * $Id$
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

/* rgbyuv is identiek aan rgbbeta */

float rgbyuv[4][4]={				/* afgeleid uit videoframer = Y Cr Cb in kopieen van Francois*/
									/* is identriek aan matrix van jpeg */
		{ .50000,	.11400,		-.08131,	0.0,},	/* b */
		{-.33126,	.58700,		-.41869,	0.0,},	/* g */
		{-.16874,	.29900,		 .50000,	0.0,},	/* r */
		{  128.0,	0.0,		128.0,		1.0}};
		  
		/* b-y (u)	y	r-y (v)  */


float rgbbeta[4][4]={				/* afgeleid uit videoframer = Y Cr Cb in kopieen van Francois*/
									/* is identriek aan matrix van jpeg */
		{.50000,	.11400,		-.08131,	0.0,},	/* b-y	-> b */
		{-.33126,	.58700,		-.41869,	0.0,},	/* y	-> g */
		{-.16874,	.29900,		 .50000,	0.0,},	/* r-y	-> r */
		{  128.0,	0.0,		128.0,		1.0}};
		  
		/* b-y		y	r-y	*/



float yuvrgb[4][4]={
	{1.77200,	-0.34414,		0.0,		0.0, },
	{1.0,		 1.0,			1.0,		0.0, },
	{0.0,		-0.71414,		1.40200,	0.0, },
	{-226.816,	135.460,		-179.456,	1.0}};

float rgb_to_bw[4][4]={
		 {.299,	.299,	.299,	0.0,},
		 {.587,	.587,	.587,	0.0,},
		 {.114,	.114,	.114,	0.0,},
		 { 0.5,	 0.5,	 0.5,	1.0}};

float dyuvrgb_oud[4][4]={
		{1.0 ,	1.0 ,	1.0,	0.0,},
		{1.733,	-0.337,	0.0,	0.0,},
		{0.0,	-.698,	1.371,	0.0,},
		{-221.8,	132.47, -175.5,	1.0}};

float dyuvrgb[4][4]={
		{1.164 ,	1.164 ,	1.164,	0.0,},
		{2.018,	-0.391,	0.0,	0.0,},
		{0.0,	-0.813,	1.596,	0.0,},
		{-276.7,	135.6, -222.7,	1.0}};

float rgbdyuv[4][4]={
		{0.439,	0.098,	-0.071,	0.0,},
		{-0.291,	0.504,	-0.368,	0.0,},
		{-0.148,	0.257,	0.439,	0.0,},
		{128.0,	16.0,	128.0,	1.0}};

