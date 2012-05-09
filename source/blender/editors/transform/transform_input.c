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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_input.c
 *  \ingroup edtransform
 */


#include <stdlib.h>
#include <math.h>

#include "DNA_screen_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "WM_types.h"

#include "transform.h"

#include "MEM_guardedalloc.h" 

/* ************************** INPUT FROM MOUSE *************************** */

static void InputVector(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	float vec[3], dvec[3];
	if (mi->precision) {
		/* calculate the main translation and the precise one separate */
		convertViewVec(t, dvec, (mval[0] - mi->precision_mval[0]), (mval[1] - mi->precision_mval[1]));
		mul_v3_fl(dvec, 0.1f);
		convertViewVec(t, vec, (mi->precision_mval[0] - t->imval[0]), (mi->precision_mval[1] - t->imval[1]));
		add_v3_v3v3(output, vec, dvec);
	}
	else {
		convertViewVec(t, output, (mval[0] - t->imval[0]), (mval[1] - t->imval[1]));
	}

}

static void InputSpring(TransInfo *UNUSED(t), MouseInput *mi, const int mval[2], float output[3])
{
	float ratio, precise_ratio, dx, dy;
	if (mi->precision) {
		/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
		dx = (float)(mi->center[0] - mi->precision_mval[0]);
		dy = (float)(mi->center[1] - mi->precision_mval[1]);
		ratio = (float)sqrt( dx*dx + dy*dy);

		dx= (float)(mi->center[0] - mval[0]);
		dy= (float)(mi->center[1] - mval[1]);
		precise_ratio = (float)sqrt( dx*dx + dy*dy);

		ratio = (ratio + (precise_ratio - ratio) / 10.0f) / mi->factor;
	}
	else {
		dx = (float)(mi->center[0] - mval[0]);
		dy = (float)(mi->center[1] - mval[1]);
		ratio = (float)sqrt( dx*dx + dy*dy) / mi->factor;
	}

	output[0] = ratio;
}

static void InputSpringFlip(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	InputSpring(t, mi, mval, output);

	/* flip scale */
	/* values can become really big when zoomed in so use longs [#26598] */
	if	((long long int)(mi->center[0] - mval[0]) * (long long int)(mi->center[0] - mi->imval[0]) +
		 (long long int)(mi->center[1] - mval[1]) * (long long int)(mi->center[1] - mi->imval[1]) < 0)
	 {
		output[0] *= -1.0f;
	 }
}

static void InputTrackBall(TransInfo *UNUSED(t), MouseInput *mi, const int mval[2], float output[3])
{

	if (mi->precision) {
		output[0] = ( mi->imval[1] - mi->precision_mval[1] ) + ( mi->precision_mval[1] - mval[1] ) * 0.1f;
		output[1] = ( mi->precision_mval[0] - mi->imval[0] ) + ( mval[0] - mi->precision_mval[0] ) * 0.1f;
	}
	else {
		output[0] = (float)( mi->imval[1] - mval[1] );
		output[1] = (float)( mval[0] - mi->imval[0] );
	}

	output[0] *= mi->factor;
	output[1] *= mi->factor;
}

static void InputHorizontalRatio(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	float x, pad;

	pad = t->ar->winx / 10;

	if (mi->precision) {
		/* deal with Shift key by adding motion / 10 to motion before shift press */
		x = mi->precision_mval[0] + (float)(mval[0] - mi->precision_mval[0]) / 10.0f;
	}
	else {
		x = mval[0];
	}

	output[0] = (x - pad) / (t->ar->winx - 2 * pad);
}

static void InputHorizontalAbsolute(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	float vec[3];

	InputVector(t, mi, mval, vec);
	project_v3_v3v3(vec, vec, t->viewinv[0]);

	output[0] = dot_v3v3(t->viewinv[0], vec) * 2.0f;
}

static void InputVerticalRatio(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	float y, pad;

	pad = t->ar->winy / 10;

	if (mi->precision) {
		/* deal with Shift key by adding motion / 10 to motion before shift press */
		y = mi->precision_mval[1] + (float)(mval[1] - mi->precision_mval[1]) / 10.0f;
	}
	else {
		y = mval[0];
	}

	output[0] = (y - pad) / (t->ar->winy - 2 * pad);
}

static void InputVerticalAbsolute(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	float vec[3];

	InputVector(t, mi, mval, vec);
	project_v3_v3v3(vec, vec, t->viewinv[1]);

	output[0] = dot_v3v3(t->viewinv[1], vec) * 2.0f;
}

void setCustomPoints(TransInfo *UNUSED(t), MouseInput *mi, int start[2], int end[2])
{
	int *data;

	if (mi->data == NULL) {
		mi->data = MEM_callocN(sizeof(int) * 4, "custom points");
	}
	
	data = mi->data;

	data[0] = start[0];
	data[1] = start[1];
	data[2] = end[0];
	data[3] = end[1];
}

static void InputCustomRatio(TransInfo *UNUSED(t), MouseInput *mi, const int mval[2], float output[3])
{
	double length;
	double distance;
	double dx, dy;
	int *data = mi->data;
	
	if (data) {
		dx = data[2] - data[0];
		dy = data[3] - data[1];
		
		length = sqrt(dx*dx + dy*dy);
		
		if (mi->precision) {
			/* deal with Shift key by adding motion / 10 to motion before shift press */
			int mdx, mdy;
			mdx = (mi->precision_mval[0] + (float)(mval[0] - mi->precision_mval[0]) / 10.0f) - data[2];
			mdy = (mi->precision_mval[1] + (float)(mval[1] - mi->precision_mval[1]) / 10.0f) - data[3];

			distance = (length != 0.0) ? (mdx * dx + mdy * dy) / length: 0.0;
		}
		else {
			int mdx, mdy;
			mdx = mval[0] - data[2];
			mdy = mval[1] - data[3];

			distance = (length != 0.0) ? (mdx * dx + mdy * dy) / length: 0.0;
		}

		output[0] = (length != 0.0) ? (double)(distance / length) : 0.0f;
	}
}

static void InputAngle(TransInfo *UNUSED(t), MouseInput *mi, const int mval[2], float output[3])
{
	double dx2 = mval[0] - mi->center[0];
	double dy2 = mval[1] - mi->center[1];
	double B = sqrt(dx2*dx2+dy2*dy2);

	double dx1 = mi->imval[0] - mi->center[0];
	double dy1 = mi->imval[1] - mi->center[1];
	double A = sqrt(dx1*dx1+dy1*dy1);

	double dx3 = mval[0] - mi->imval[0];
	double dy3 = mval[1] - mi->imval[1];

	double *angle = mi->data;

	/* use doubles here, to make sure a "1.0" (no rotation) doesnt become 9.999999e-01, which gives 0.02 for acos */
	double deler = (((dx1 * dx1 + dy1 * dy1) +
	                 (dx2 * dx2 + dy2 * dy2) -
	                 (dx3 * dx3 + dy3 * dy3)) / (2.0 * ((A * B) ? (A * B) : 1.0)));
	/* ((A*B)?(A*B):1.0) this takes care of potential divide by zero errors */

	float dphi;

	dphi = saacos((float)deler);
	if ( (dx1*dy2-dx2*dy1)>0.0 ) dphi= -dphi;

	/* If the angle is zero, because of lack of precision close to the 1.0 value in acos
	 * approximate the angle with the opposite side of the normalized triangle
	 * This is a good approximation here since the smallest acos value seems to be around
	 * 0.02 degree and lower values don't even have a 0.01% error compared to the approximation
	 */
	if (dphi == 0) {
		double dx, dy;

		dx2 /= A;
		dy2 /= A;

		dx1 /= B;
		dy1 /= B;

		dx = dx1 - dx2;
		dy = dy1 - dy2;

		dphi = sqrt(dx*dx + dy*dy);
		if ( (dx1*dy2-dx2*dy1)>0.0 ) dphi= -dphi;
	}

	if (mi->precision) {
		dphi = dphi/30.0f;
	}

	/* if no delta angle, don't update initial position */
	if (dphi != 0) {
		mi->imval[0] = mval[0];
		mi->imval[1] = mval[1];
	}

	*angle += (double)dphi;

	output[0] = *angle;
}

void initMouseInput(TransInfo *UNUSED(t), MouseInput *mi, int center[2], int mval[2])
{
	mi->factor = 0;
	mi->precision = 0;

	mi->center[0] = center[0];
	mi->center[1] = center[1];

	mi->imval[0] = mval[0];
	mi->imval[1] = mval[1];

	mi->post = NULL;
}

static void calcSpringFactor(MouseInput *mi)
{
	mi->factor = sqrtf(((float)(mi->center[1] - mi->imval[1])) * ((float)(mi->center[1] - mi->imval[1])) +
	                   ((float)(mi->center[0] - mi->imval[0])) * ((float)(mi->center[0] - mi->imval[0])));

	if (mi->factor == 0.0f) {
		mi->factor= 1.0f; /* prevent Inf */
	}
}

void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode)
{
	/* may have been allocated previously */
	/* TODO, holding R-key can cause mem leak, but this causes [#28903]
	 * disable for now. */
#if 0
	if (mi->data) {
		MEM_freeN(mi->data);
		mi->data= NULL;
	}
#endif

	switch (mode) {
	case INPUT_VECTOR:
		mi->apply = InputVector;
		t->helpline = HLP_NONE;
		break;
	case INPUT_SPRING:
		calcSpringFactor(mi);
		mi->apply = InputSpring;
		t->helpline = HLP_SPRING;
		break;
	case INPUT_SPRING_FLIP:
		calcSpringFactor(mi);
		mi->apply = InputSpringFlip;
		t->helpline = HLP_SPRING;
		break;
	case INPUT_ANGLE:
		mi->data = MEM_callocN(sizeof(double), "angle accumulator");
		mi->apply = InputAngle;
		t->helpline = HLP_ANGLE;
		break;
	case INPUT_TRACKBALL:
		/* factor has to become setting or so */
		mi->factor = 0.01f;
		mi->apply = InputTrackBall;
		t->helpline = HLP_TRACKBALL;
		break;
	case INPUT_HORIZONTAL_RATIO:
		mi->factor = (float)(mi->center[0] - mi->imval[0]);
		mi->apply = InputHorizontalRatio;
		t->helpline = HLP_HARROW;
		break;
	case INPUT_HORIZONTAL_ABSOLUTE:
		mi->apply = InputHorizontalAbsolute;
		t->helpline = HLP_HARROW;
		break;
	case INPUT_VERTICAL_RATIO:
		mi->apply = InputVerticalRatio;
		t->helpline = HLP_VARROW;
		break;
	case INPUT_VERTICAL_ABSOLUTE:
		mi->apply = InputVerticalAbsolute;
		t->helpline = HLP_VARROW;
		break;
	case INPUT_CUSTOM_RATIO:
		mi->apply = InputCustomRatio;
		t->helpline = HLP_NONE;
		break;
	case INPUT_NONE:
	default:
		mi->apply = NULL;
		break;
	}

	/* bootstrap mouse input with initial values */
	applyMouseInput(t, mi, mi->imval, t->values);
}

void setInputPostFct(MouseInput *mi, void	(*post)(struct TransInfo *, float [3]))
{
	mi->post = post;
}

void applyMouseInput(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	if (mi->apply != NULL) {
		mi->apply(t, mi, mval, output);
	}

	if (mi->post) {
		mi->post(t, output);
	}
}

int handleMouseInput(TransInfo *t, MouseInput *mi, wmEvent *event)
{
	int redraw = TREDRAW_NOTHING;

	switch (event->type) {
	case LEFTSHIFTKEY:
	case RIGHTSHIFTKEY:
		if (event->val == KM_PRESS) {
			t->modifiers |= MOD_PRECISION;
			/* shift is modifier for higher precision transform
			 * store the mouse position where the normal movement ended */
			copy_v2_v2_int(mi->precision_mval, event->mval);
			mi->precision = 1;
		}
		else {
			t->modifiers &= ~MOD_PRECISION;
			mi->precision = 0;
		}
		redraw = TREDRAW_HARD;
		break;
	}

	return redraw;
}
