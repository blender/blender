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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_defocus.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

/* ************ qdn: Defocus node ****************** */
static bNodeSocketTemplate cmp_node_defocus_in[]= {
	{	SOCK_RGBA, 1, "Image",			1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Z",			1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_defocus_out[]= {
	{	SOCK_RGBA, 0, "Image"},
	{	-1, 0, ""	}
};


// line coefs for point sampling & scancon. data.
typedef struct BokehCoeffs {
	float x0, y0, dx, dy;
	float ls_x, ls_y;
	float min_x, min_y, max_x, max_y;
} BokehCoeffs;

// returns array of BokehCoeffs
// returns length of array in 'len_bkh',
// radius squared of inscribed disk in 'inradsq', needed in getWeight() test,
// BKH[8] is the data returned for the bokeh shape & bkh_b[4] is it's 2d bound
static void makeBokeh(char bktype, char ro, int* len_bkh, float* inradsq, BokehCoeffs BKH[8], float bkh_b[4])
{
	float x0, x1, y0, y1, dx, dy, iDxy;
	/* ro now is in radians. */
	float w = MAX2(1e-6f, ro);  // never reported stangely enough, but a zero offset causes missing center line...
	float wi = DEG2RADF(360.f/bktype);
	int i, ov, nv;
	
	// bktype must be at least 3 & <= 8
	bktype = (bktype<3) ? 3 : ((bktype>8) ? 8 : bktype);
	*len_bkh = bktype;
	*inradsq = -1.f;

	for (i=0; i<(*len_bkh); i++) {
		x0 = cos(w);
		y0 = sin(w);
		w += wi;
		x1 = cos(w);
		y1 = sin(w);
		if ((*inradsq)<0.f) {
			// radius squared of inscribed disk
			float idx=(x0+x1)*0.5f, idy=(y0+y1)*0.5f;
			*inradsq = idx*idx + idy*idy;
		}
		BKH[i].x0 = x0;
		BKH[i].y0 = y0;
		dx = x1-x0, dy = y1-y0;
		iDxy = 1.f / sqrtf(dx*dx + dy*dy);
		dx *= iDxy;
		dy *= iDxy;
		BKH[i].dx = dx;
		BKH[i].dy = dy;
	}

	// precalc scanconversion data
	// bokeh bound, not transformed, for scanconvert
	bkh_b[0] = bkh_b[2] = 1e10f;	// xmin/ymin
	bkh_b[1] = bkh_b[3] = -1e10f;	// xmax/ymax
	ov = (*len_bkh) - 1;
	for (nv=0; nv<(*len_bkh); nv++) {
		bkh_b[0] = MIN2(bkh_b[0], BKH[nv].x0);	// xmin
		bkh_b[1] = MAX2(bkh_b[1], BKH[nv].x0);	// xmax
		bkh_b[2] = MIN2(bkh_b[2], BKH[nv].y0);	// ymin
		bkh_b[3] = MAX2(bkh_b[3], BKH[nv].y0);	// ymax
		BKH[nv].min_x = MIN2(BKH[ov].x0, BKH[nv].x0);
		BKH[nv].max_x = MAX2(BKH[ov].x0, BKH[nv].x0);
		BKH[nv].min_y = MIN2(BKH[ov].y0, BKH[nv].y0);
		BKH[nv].max_y = MAX2(BKH[ov].y0, BKH[nv].y0);
		dy = BKH[nv].y0 - BKH[ov].y0;
		BKH[nv].ls_x = (BKH[nv].x0 - BKH[ov].x0) / ((dy==0.f) ? 1.f : dy);
		BKH[nv].ls_y = (BKH[nv].ls_x==0.f) ? 1.f : (1.f/BKH[nv].ls_x);
		ov = nv;
	}
}

// test if u/v inside shape & returns weight value
static float getWeight(BokehCoeffs* BKH, int len_bkh, float u, float v, float rad, float inradsq)
{
	BokehCoeffs* bc = BKH;
	float cdist, irad = (rad==0.f) ? 1.f : (1.f/rad);
	u *= irad;
	v *= irad;
 
	// early out test1: if point outside outer unit disk, it cannot be inside shape
	cdist = u*u + v*v;
	if (cdist>1.f) return 0.f;
	
	// early out test2: if point inside or on inner disk, point must be inside shape
	if (cdist<=inradsq) return 1.f;
	
	while (len_bkh--) {
		if ((bc->dy*(u - bc->x0) - bc->dx*(v - bc->y0)) > 0.f) return 0.f;
		bc++;
	}
	return 1.f;
}

// QMC.seq. for sampling, A.Keller, EMS
static float RI_vdC(unsigned int bits, unsigned int r)
{
	bits = ( bits << 16) | ( bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	bits ^= r;
	return (float)((double)bits / 4294967296.0);
}

// single channel IIR gaussian filtering
// much faster than anything else, constant time independent of width
// should extend to multichannel and make this a node, could be useful
// note: this is an almost exact copy of 'IIR_gauss'
static void IIR_gauss_single(CompBuf* buf, float sigma)
{
	double q, q2, sc, cf[4], tsM[9], tsu[3], tsv[3];
	float *X, *Y, *W;
	int i, x, y, sz;

	// single channel only for now
	if (buf->type != CB_VAL) return;

	// <0.5 not valid, though can have a possibly useful sort of sharpening effect
	if (sigma < 0.5f) return;
	
	// see "Recursive Gabor Filtering" by Young/VanVliet
	// all factors here in double.prec. Required, because for single.prec it seems to blow up if sigma > ~200
	if (sigma >= 3.556f)
		q = 0.9804f*(sigma - 3.556f) + 2.5091f;
	else // sigma >= 0.5
		q = (0.0561f*sigma + 0.5784f)*sigma - 0.2568f;
	q2 = q*q;
	sc = (1.1668 + q)*(3.203729649  + (2.21566 + q)*q);
	// no gabor filtering here, so no complex multiplies, just the regular coefs.
	// all negated here, so as not to have to recalc Triggs/Sdika matrix
	cf[1] = q*(5.788961737 + (6.76492 + 3.0*q)*q)/ sc;
	cf[2] = -q2*(3.38246 + 3.0*q)/sc;
	// 0 & 3 unchanged
	cf[3] = q2*q/sc;
	cf[0] = 1.0 - cf[1] - cf[2] - cf[3];

	// Triggs/Sdika border corrections,
	// it seems to work, not entirely sure if it is actually totally correct,
	// Besides J.M.Geusebroek's anigauss.c (see http://www.science.uva.nl/~mark),
	// found one other implementation by Cristoph Lampert,
	// but neither seem to be quite the same, result seems to be ok sofar anyway.
	// Extra scale factor here to not have to do it in filter,
	// though maybe this had something to with the precision errors
	sc = cf[0]/((1.0 + cf[1] - cf[2] + cf[3])*(1.0 - cf[1] - cf[2] - cf[3])*(1.0 + cf[2] + (cf[1] - cf[3])*cf[3]));
	tsM[0] = sc*(-cf[3]*cf[1] + 1.0 - cf[3]*cf[3] - cf[2]);
	tsM[1] = sc*((cf[3] + cf[1])*(cf[2] + cf[3]*cf[1]));
	tsM[2] = sc*(cf[3]*(cf[1] + cf[3]*cf[2]));
	tsM[3] = sc*(cf[1] + cf[3]*cf[2]);
	tsM[4] = sc*(-(cf[2] - 1.0)*(cf[2] + cf[3]*cf[1]));
	tsM[5] = sc*(-(cf[3]*cf[1] + cf[3]*cf[3] + cf[2] - 1.0)*cf[3]);
	tsM[6] = sc*(cf[3]*cf[1] + cf[2] + cf[1]*cf[1] - cf[2]*cf[2]);
	tsM[7] = sc*(cf[1]*cf[2] + cf[3]*cf[2]*cf[2] - cf[1]*cf[3]*cf[3] - cf[3]*cf[3]*cf[3] - cf[3]*cf[2] + cf[3]);
	tsM[8] = sc*(cf[3]*(cf[1] + cf[3]*cf[2]));

#define YVV(L)\
{\
	W[0] = cf[0]*X[0] + cf[1]*X[0] + cf[2]*X[0] + cf[3]*X[0];\
	W[1] = cf[0]*X[1] + cf[1]*W[0] + cf[2]*X[0] + cf[3]*X[0];\
	W[2] = cf[0]*X[2] + cf[1]*W[1] + cf[2]*W[0] + cf[3]*X[0];\
	for (i=3; i<L; i++)\
		W[i] = cf[0]*X[i] + cf[1]*W[i-1] + cf[2]*W[i-2] + cf[3]*W[i-3];\
	tsu[0] = W[L-1] - X[L-1];\
	tsu[1] = W[L-2] - X[L-1];\
	tsu[2] = W[L-3] - X[L-1];\
	tsv[0] = tsM[0]*tsu[0] + tsM[1]*tsu[1] + tsM[2]*tsu[2] + X[L-1];\
	tsv[1] = tsM[3]*tsu[0] + tsM[4]*tsu[1] + tsM[5]*tsu[2] + X[L-1];\
	tsv[2] = tsM[6]*tsu[0] + tsM[7]*tsu[1] + tsM[8]*tsu[2] + X[L-1];\
	Y[L-1] = cf[0]*W[L-1] + cf[1]*tsv[0] + cf[2]*tsv[1] + cf[3]*tsv[2];\
	Y[L-2] = cf[0]*W[L-2] + cf[1]*Y[L-1] + cf[2]*tsv[0] + cf[3]*tsv[1];\
	Y[L-3] = cf[0]*W[L-3] + cf[1]*Y[L-2] + cf[2]*Y[L-1] + cf[3]*tsv[0];\
	for (i=L-4; i>=0; i--)\
		Y[i] = cf[0]*W[i] + cf[1]*Y[i+1] + cf[2]*Y[i+2] + cf[3]*Y[i+3];\
}

	// intermediate buffers
	sz = MAX2(buf->x, buf->y);
	Y = MEM_callocN(sz*sizeof(float), "IIR_gauss Y buf");
	W = MEM_callocN(sz*sizeof(float), "IIR_gauss W buf");
	// H
	for (y=0; y<buf->y; y++) {
		X = &buf->rect[y*buf->x];
		YVV(buf->x);
		memcpy(X, Y, sizeof(float)*buf->x);
	}
	// V
	X = MEM_callocN(buf->y*sizeof(float), "IIR_gauss X buf");
	for (x=0; x<buf->x; x++) {
		for (y=0; y<buf->y; y++)
			X[y] = buf->rect[x + y*buf->x];
		YVV(buf->y);
		for (y=0; y<buf->y; y++)
			buf->rect[x + y*buf->x] = Y[y];
	}
	MEM_freeN(X);

	MEM_freeN(W);
	MEM_freeN(Y);
#undef YVV
}

static void defocus_blur(bNode *node, CompBuf *new, CompBuf *img, CompBuf *zbuf, float inpval, int no_zbuf)
{
	NodeDefocus *nqd = node->storage;
	CompBuf *wts;		// weights buffer
	CompBuf *crad;		// CoC radius buffer
	BokehCoeffs BKH[8];	// bokeh shape data, here never > 8 pts.
	float bkh_b[4] = {0};	// shape 2D bound
	float cam_fdist=1, cam_invfdist=1, cam_lens=35;
	float dof_sp, maxfgc, bk_hn_theta=0, inradsq=0;
	int y, len_bkh=0, ydone=0;
	float aspect, aperture;
	int minsz;
	//float bcrad, nmaxc, scf;
	
	// get some required params from the current scene camera
	// (ton) this is wrong, needs fixed
	Scene *scene= (Scene*)node->id;
	Object* camob = (scene)? scene->camera: NULL;
	if (camob && camob->type==OB_CAMERA) {
		Camera* cam = (Camera*)camob->data;
		cam_lens = cam->lens;
		cam_fdist = object_camera_dof_distance(camob);
		if (cam_fdist==0.0f) cam_fdist = 1e10f; /* if the dof is 0.0 then set it be be far away */
		cam_invfdist = 1.f/cam_fdist;
	}

	// guess work here.. best match with raytraced result
	minsz = MIN2(img->x, img->y);
	dof_sp = (float)minsz / (16.f / cam_lens);	// <- == aspect * MIN2(img->x, img->y) / tan(0.5f * fov);
	
	// aperture
	aspect = (img->x > img->y) ? (img->y / (float)img->x) : (img->x / (float)img->y);
	aperture = 0.5f*(cam_lens / (aspect*32.f)) / nqd->fstop;
	
	// if not disk, make bokeh coefficients and other needed data
	if (nqd->bktype!=0) {
		makeBokeh(nqd->bktype, nqd->rotation, &len_bkh, &inradsq, BKH, bkh_b);
		bk_hn_theta = 0.5 * nqd->bktype * sin(2.0 * M_PI / nqd->bktype);	// weight factor
	}
	
	// accumulated weights
	wts = alloc_compbuf(img->x, img->y, CB_VAL, 1);
	// CoC radius buffer
	crad = alloc_compbuf(img->x, img->y, CB_VAL, 1);

	// if 'no_zbuf' flag set (which is always set if input is not an image),
	// values are instead interpreted directly as blur radius values
	if (no_zbuf) {
		// to prevent *reaaallly* big radius values and impossible calculation times,
		// limit the maximum to half the image width or height, whichever is smaller
		float maxr = 0.5f*(float)MIN2(img->x, img->y);
		unsigned int p;

		for (p=0; p<(unsigned int)(img->x*img->y); p++) {
			crad->rect[p] = zbuf ? (zbuf->rect[p]*nqd->scale) : inpval;
			// bug #5921, limit minimum
			crad->rect[p] = MAX2(1e-5f, crad->rect[p]);
			crad->rect[p] = MIN2(crad->rect[p], maxr);
			// if maxblur!=0, limit maximum
			if (nqd->maxblur != 0.f) crad->rect[p] = MIN2(crad->rect[p], nqd->maxblur);
		}
	}
	else {
		float wt;

		// actual zbuffer.
		// separate foreground from background CoC's
		// then blur background and blend in again with foreground,
		// improves the 'blurred foreground overlapping in-focus midground' sharp boundary problem.
		// wts buffer here used for blendmask
		maxfgc = 0.f; // maximum foreground CoC radius
		for (y=0; y<img->y; y++) {
			unsigned int p = y * img->x;
			int x;
			for (x=0; x<img->x; x++) {
				unsigned int px = p + x;
				float iZ = (zbuf->rect[px]==0.f) ? 0.f : (1.f/zbuf->rect[px]);
				crad->rect[px] = 0.5f*(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
				if (crad->rect[px] <= 0.f) {
					wts->rect[px] = 1.f;
					crad->rect[px] = -crad->rect[px];
					if (crad->rect[px] > maxfgc) maxfgc = crad->rect[px];
				}
				else crad->rect[px] = wts->rect[px] = 0;
			}
		}
		
		// fast blur...
		// bug #6656 part 1, probably when previous node_composite.c was split into separate files, it was not properly updated
		// to include recent cvs commits (well, at least not defocus node), so this part was missing...
		wt = aperture*128.f;
		IIR_gauss_single(crad, wt);
		IIR_gauss_single(wts, wt);
		
		// bug #6656 part 2a, although foreground blur is not based anymore on closest object,
		// the rescaling op below was still based on that anyway, and unlike the comment in below code,
		// the difference is therefore not always that small at all...
		// so for now commented out, not sure if this is going to cause other future problems, lets just wait and see...
		/*
		// find new maximum to scale it back to original
		// (could skip this, not strictly necessary, in general, difference is quite small, but just in case...)
		nmaxc = 0;
		for (p=0; p<(img->x*img->y); p++)
			if (crad->rect[p] > nmaxc) nmaxc = crad->rect[p];
		// rescale factor
		scf = (nmaxc==0.f) ? 1.f: (maxfgc / nmaxc);
		*/

		// and blend...
		for (y=0; y<img->y; y++) {
			unsigned int p = y*img->x;
			int x;

			for (x=0; x<img->x; x++) {
				unsigned px = p + x;
				if (zbuf->rect[px]!=0.f) {
					float iZ = (zbuf->rect[px]==0.f) ? 0.f : (1.f/zbuf->rect[px]);
					
					// bug #6656 part 2b, do not rescale
					/*
					bcrad = 0.5f*fabs(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
					// scale crad back to original maximum and blend
					crad->rect[px] = bcrad + wts->rect[px]*(scf*crad->rect[px] - bcrad);
					*/
					crad->rect[px] = 0.5f*fabsf(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
					
					// 'bug' #6615, limit minimum radius to 1 pixel, not really a solution, but somewhat mitigates the problem
					crad->rect[px] = MAX2(crad->rect[px], 0.5f);
					// if maxblur!=0, limit maximum
					if (nqd->maxblur != 0.f) crad->rect[px] = MIN2(crad->rect[px], nqd->maxblur);
				}
				else crad->rect[px] = 0.f;
				// clear weights for next part
				wts->rect[px] = 0.f;
			}
			// esc set by main calling process
			if(node->exec & NODE_BREAK)
				break;
		}
	}

	//------------------------------------------------------------------
	// main loop
#ifndef __APPLE__ /* can crash on Mac, see bug #22856, disabled for now */
#ifdef __INTEL_COMPILER /* icc doesn't like the compound statement -- internal error: 0_1506 */
	#pragma omp parallel for private(y) if(!nqd->preview) schedule(guided)
#else
	#pragma omp parallel for private(y) if(!nqd->preview && img->y*img->x > 16384) schedule(guided)
#endif
#endif
	for (y=0; y<img->y; y++) {
		unsigned int p, p4, zp, cp, cp4;
		float *ctcol, u, v, ct_crad, cR2=0;
		int x, sx, sy;

		// some sort of visual feedback would be nice, or at least this text in the renderwin header
		// but for now just print some info in the console every 8 scanlines.
		#pragma omp critical
		{
			if (((ydone & 7)==0) || (ydone==(img->y-1))) {
				if(G.background==0) {
					printf("\rdefocus: Processing Line %d of %d ... ", ydone+1, img->y);
					fflush(stdout);
				}
			}

			ydone++;
		}

		// esc set by main calling process. don't break because openmp doesn't
		// allow it, just continue and do nothing 
		if(node->exec & NODE_BREAK)
			continue;

		zp = y * img->x;
		for (x=0; x<img->x; x++) {
			cp = zp + x;
			cp4 = cp * img->type;

			// Circle of Confusion radius for current pixel
			cR2 = ct_crad = crad->rect[cp];
			// skip if zero (border render)
			if (ct_crad==0.f) {
				// related to bug #5921, forgot output image when skipping 0 radius values
				new->rect[cp4] = img->rect[cp4];
				if (new->type != CB_VAL) {
					new->rect[cp4+1] = img->rect[cp4+1];
					new->rect[cp4+2] = img->rect[cp4+2];
					new->rect[cp4+3] = img->rect[cp4+3];
				}
				continue;
			}
			cR2 *= cR2;
			
			// pixel color
			ctcol = &img->rect[cp4];
			
			if (!nqd->preview) {
				int xs, xe, ys, ye;
				float lwt, wtcol[4] = {0}, aacol[4] = {0};
				float wt;

				// shape weight
				if (nqd->bktype==0)	// disk
					wt = 1.f/((float)M_PI*cR2);
				else
					wt = 1.f/(cR2*bk_hn_theta);

				// weighted color
				wtcol[0] = wt*ctcol[0];
				if (new->type != CB_VAL) {
					wtcol[1] = wt*ctcol[1];
					wtcol[2] = wt*ctcol[2];
					wtcol[3] = wt*ctcol[3];
				}

				// macro for background blur overlap test
				// unfortunately, since this is done per pixel,
				// it has a very significant negative impact on processing time...
				// (eg. aa disk blur without test: 112 sec, vs with test: 176 sec...)
				// iff center blur radius > threshold
				// and if overlap pixel in focus, do nothing, else add color/weigbt
				// (threshold constant is dependant on amount of blur)
				#define TESTBG1(c, w) {\
					if (ct_crad > nqd->bthresh) {\
						if (crad->rect[p] > nqd->bthresh) {\
							new->rect[p] += c[0];\
							wts->rect[p] += w;\
						}\
					}\
					else {\
						new->rect[p] += c[0];\
						wts->rect[p] += w;\
					}\
				}
				#define TESTBG4(c, w) {\
					if (ct_crad > nqd->bthresh) {\
						if (crad->rect[p] > nqd->bthresh) {\
							new->rect[p4] += c[0];\
							new->rect[p4+1] += c[1];\
							new->rect[p4+2] += c[2];\
							new->rect[p4+3] += c[3];\
							wts->rect[p] += w;\
						}\
					}\
					else {\
						new->rect[p4] += c[0];\
						new->rect[p4+1] += c[1];\
						new->rect[p4+2] += c[2];\
						new->rect[p4+3] += c[3];\
						wts->rect[p] += w;\
					}\
				}
				if (nqd->bktype == 0) {
					// Disk
					int _x, i, j, di;
					float Dj, T;
					// AA pixel
					#define AAPIX(a, b) {\
						int _ny = b;\
						if ((_ny >= 0) && (_ny < new->y)) {\
							int _nx = a;\
							if ((_nx >=0) && (_nx < new->x)) {\
								p = _ny*new->x + _nx;\
								if (new->type==CB_VAL) {\
									TESTBG1(aacol, lwt);\
								}\
								else {\
									p4 = p * new->type;\
									TESTBG4(aacol, lwt);\
								}\
							}\
						}\
					}
					// circle scanline
					#define CSCAN(a, b) {\
						int _ny = y + b;\
						if ((_ny >= 0) && (_ny < new->y)) {\
							xs = x - a + 1;\
							if (xs < 0) xs = 0;\
							xe = x + a;\
							if (xe > new->x) xe = new->x;\
							p = _ny*new->x + xs;\
							if (new->type==CB_VAL) {\
								for (_x=xs; _x<xe; _x++, p++) TESTBG1(wtcol, wt);\
							}\
							else {\
								p4 = p * new->type;\
								for (_x=xs; _x<xe; _x++, p++, p4+=new->type) TESTBG4(wtcol, wt);\
							}\
						}\
					}

					i = ceil(ct_crad);
					j = 0;
					T = 0;
					while (i > j) {
						Dj = sqrt(cR2 - j*j);
						Dj -= floorf(Dj);
						di = 0;
						if (Dj > T) { i--;  di = 1; }
						T = Dj;
						aacol[0] = wtcol[0]*Dj;
						if (new->type != CB_VAL) {
							aacol[1] = wtcol[1]*Dj;
							aacol[2] = wtcol[2]*Dj;
							aacol[3] = wtcol[3]*Dj;
						}
						lwt = wt*Dj;
						if (i!=j) {
							// outer pixels
							AAPIX(x+j, y+i)
							AAPIX(x+j, y-i)
							if (j) {
								AAPIX(x-j, y+i) // BL
								AAPIX(x-j, y-i) // TL
							}
							if (di) { // only when i changed, interior of outer section
								CSCAN(j, i) // bottom
								CSCAN(j, -i) // top
							}
						}
						// lower mid section
						AAPIX(x+i, y+j)
						if (i) AAPIX(x-i, y+j)
						CSCAN(i, j)
						// upper mid section
						if (j) {
							AAPIX(x+i, y-j)
							if (i) AAPIX(x-i, y-j)
							CSCAN(i, -j)
						}
						j++;
					}
					#undef CSCAN
					#undef AAPIX
				}
				else {
					// n-agonal
					int ov, nv;
					float mind, maxd, lwt;
					ys = MAX2((int)floor(bkh_b[2]*ct_crad + y), 0);
					ye = MIN2((int)ceil(bkh_b[3]*ct_crad + y), new->y - 1);
					for (sy=ys; sy<=ye; sy++) {
						float fxs = 1e10f, fxe = -1e10f;
						float yf = (sy - y)/ct_crad;
						int found = 0;
						ov = len_bkh - 1;
						mind = maxd = 0;
						for (nv=0; nv<len_bkh; nv++) {
							if ((BKH[nv].max_y >= yf) && (BKH[nv].min_y <= yf)) {
								float tx = BKH[ov].x0 + BKH[nv].ls_x*(yf - BKH[ov].y0);
								if (tx < fxs) { fxs = tx;  mind = BKH[nv].ls_x; }
								if (tx > fxe) { fxe = tx;  maxd = BKH[nv].ls_x; }
								if (++found == 2) break;
							}
							ov = nv;
						}
						if (found) {
							fxs = fxs*ct_crad + x;
							fxe = fxe*ct_crad + x;
							xs = (int)floor(fxs), xe = (int)ceil(fxe);
							// AA hack for first and last x pixel, near vertical edges only
							if (fabsf(mind) <= 1.f) {
								if ((xs >= 0) && (xs < new->x)) {
									lwt = 1.f-(fxs - xs);
									aacol[0] = wtcol[0]*lwt;
									p = xs + sy*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							if (fabsf(maxd) <= 1.f) {
								if ((xe >= 0) && (xe < new->x)) {
									lwt = 1.f-(xe - fxe);
									aacol[0] = wtcol[0]*lwt;
									p = xe + sy*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							xs = MAX2(xs+1, 0);
							xe = MIN2(xe, new->x);
							// remaining interior scanline
							p = sy*new->x + xs;
							if (new->type==CB_VAL) {
								for (sx=xs; sx<xe; sx++, p++) TESTBG1(wtcol, wt);
							}
							else {
								p4 = p * new->type;
								for (sx=xs; sx<xe; sx++, p++, p4+=new->type) TESTBG4(wtcol, wt);
							}
						}
					}

					// now traverse in opposite direction, y scanlines,
					// but this time only draw the near horizontal edges,
					// applying same AA hack as above
					xs = MAX2((int)floor(bkh_b[0]*ct_crad + x), 0);
					xe = MIN2((int)ceil(bkh_b[1]*ct_crad + x), img->x - 1);
					for (sx=xs; sx<=xe; sx++) {
						float xf = (sx - x)/ct_crad;
						float fys = 1e10f, fye = -1e10f;
						int found = 0;
						ov = len_bkh - 1;
						mind = maxd = 0;
						for (nv=0; nv<len_bkh; nv++) {
							if ((BKH[nv].max_x >= xf) && (BKH[nv].min_x <= xf)) {
								float ty = BKH[ov].y0 + BKH[nv].ls_y*(xf - BKH[ov].x0);
								if (ty < fys) { fys = ty;  mind = BKH[nv].ls_y; }
								if (ty > fye) { fye = ty;  maxd = BKH[nv].ls_y; }
								if (++found == 2) break;
							}
							ov = nv;
						}
						if (found) {
							fys = fys*ct_crad + y;
							fye = fye*ct_crad + y;
							// near horizontal edges only, line slope <= 1
							if (fabsf(mind) <= 1.f) {
								int iys = (int)floor(fys);
								if ((iys >= 0) && (iys < new->y)) {
									lwt = 1.f - (fys - iys);
									aacol[0] = wtcol[0]*lwt;
									p = sx + iys*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
							if (fabsf(maxd) <= 1.f) {
								int iye = ceil(fye);
								if ((iye >= 0) && (iye < new->y)) {
									lwt = 1.f - (iye - fye);
									aacol[0] = wtcol[0]*lwt;
									p = sx + iye*new->x;
									if (new->type==CB_VAL) {
										lwt *= wt;
										TESTBG1(aacol, lwt);
									}
									else {
										p4 = p * new->type;
										aacol[1] = wtcol[1]*lwt;
										aacol[2] = wtcol[2]*lwt;
										aacol[3] = wtcol[3]*lwt;
										lwt *= wt;
										TESTBG4(aacol, lwt);
									}
								}
							}
						}
					}

				}
				#undef TESTBG4
				#undef TESTBG1

			}
			else {
				// sampled, simple rejection sampling here, good enough
				unsigned int maxsam, s, ui = BLI_rand()*BLI_rand();
				float wcor, cpr = BLI_frand(), lwt;
				if (no_zbuf)
					maxsam = nqd->samples;	// no zbuffer input, use sample value directly
				else {
					// depth adaptive sampling hack, the more out of focus, the more samples taken, 16 minimum.
					maxsam = (int)(0.5f + nqd->samples*(1.f-(float)exp(-fabs(zbuf->rect[cp] - cam_fdist))));
					if (maxsam < 16) maxsam = 16;
				}
				wcor = 1.f/(float)maxsam;
				for (s=0; s<maxsam; ++s) {
					u = ct_crad*(2.f*RI_vdC(s, ui) - 1.f);
					v = ct_crad*(2.f*(s + cpr)/(float)maxsam - 1.f);
					sx = (int)(x + u + 0.5f), sy = (int)(y + v + 0.5f);
					if ((sx<0) || (sx >= new->x) || (sy<0) || (sy >= new->y)) continue;
					p = sx + sy*new->x;
					p4 = p * new->type;
					if (nqd->bktype==0)	// Disk
						lwt = ((u*u + v*v)<=cR2) ? wcor : 0.f;
					else	// AA not needed here
						lwt = wcor * getWeight(BKH, len_bkh, u, v, ct_crad, inradsq);
					// prevent background bleeding onto in-focus pixels, user-option
					if (ct_crad > nqd->bthresh) {  // if center blur > threshold
						if (crad->rect[p] > nqd->bthresh) { // if overlap pixel in focus, do nothing, else add color/weigbt
							new->rect[p4] += ctcol[0] * lwt;
							if (new->type != CB_VAL) {
								new->rect[p4+1] += ctcol[1] * lwt;
								new->rect[p4+2] += ctcol[2] * lwt;
								new->rect[p4+3] += ctcol[3] * lwt;
							}
							wts->rect[p] += lwt;
						}
					}
					else {
						new->rect[p4] += ctcol[0] * lwt;
						if (new->type != CB_VAL) {
							new->rect[p4+1] += ctcol[1] * lwt;
							new->rect[p4+2] += ctcol[2] * lwt;
							new->rect[p4+3] += ctcol[3] * lwt;
						}
						wts->rect[p] += lwt;
					}
				}
			}

		}
	}
	
	// finally, normalize
	for (y=0; y<new->y; y++) {
		unsigned int p = y * new->x;
		unsigned int p4 = p * new->type;
		int x;

		for (x=0; x<new->x; x++) {
			float dv = (wts->rect[p]==0.f) ? 1.f : (1.f/wts->rect[p]);
			new->rect[p4] *= dv;
			if (new->type!=CB_VAL) {
				new->rect[p4+1] *= dv;
				new->rect[p4+2] *= dv;
				new->rect[p4+3] *= dv;
			}
			p++;
			p4 += new->type;
		}
	}

	free_compbuf(crad);
	free_compbuf(wts);
	
	printf("Done\n");
}


static void node_composit_exec_defocus(void *UNUSED(data), bNode *node, bNodeStack **in, bNodeStack **out)
{
	CompBuf *new, *old, *zbuf_use = NULL, *img = in[0]->data, *zbuf = in[1]->data;
	NodeDefocus *nqd = node->storage;
	int no_zbuf = nqd->no_zbuf;
	
	if ((img==NULL) || (out[0]->hasoutput==0)) return;
	
	// if image not valid type or fstop==infinite (128), nothing to do, pass in to out
	if (((img->type!=CB_RGBA) && (img->type!=CB_VAL)) || ((no_zbuf==0) && (nqd->fstop==128.f))) {
		out[0]->data = pass_on_compbuf(img);
		return;
	}
	
	if (zbuf!=NULL) {
		// Zbuf input, check to make sure, single channel, same size
		// doesn't have to be actual zbuffer, but must be value type
		if ((zbuf->x != img->x) || (zbuf->y != img->y)) {
			// could do a scale here instead...
			printf("Z input must be same size as image !\n");
			return;
		}
		zbuf_use = typecheck_compbuf(zbuf, CB_VAL);
	}
	else no_zbuf = 1;	// no zbuffer input
		
	// ok, process
	old = img;
	if (nqd->gamco) {
		// gamma correct, blender func is simplified, fixed value & RGBA only,
		// should make user param. also depremul and premul afterwards, gamma
		// correction can't work with premul alpha
		old = dupalloc_compbuf(img);
		premul_compbuf(old, 1);
		gamma_correct_compbuf(old, 0);
		premul_compbuf(old, 0);
	}
	
	new = alloc_compbuf(old->x, old->y, old->type, 1);
	defocus_blur(node, new, old, zbuf_use, in[1]->vec[0]*nqd->scale, no_zbuf);
	
	if (nqd->gamco) {
		premul_compbuf(new, 1);
		gamma_correct_compbuf(new, 1);
		premul_compbuf(new, 0);
		free_compbuf(old);
	}
	if(node->exec & NODE_BREAK) {
		free_compbuf(new);
		new= NULL;
	}	
	out[0]->data = new;
	if (zbuf_use && (zbuf_use != zbuf)) free_compbuf(zbuf_use);
}

static void node_composit_init_defocus(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	/* qdn: defocus node */
	NodeDefocus *nbd = MEM_callocN(sizeof(NodeDefocus), "node defocus data");
	nbd->bktype = 0;
	nbd->rotation = 0.0f;
	nbd->preview = 1;
	nbd->gamco = 0;
	nbd->samples = 16;
	nbd->fstop = 128.f;
	nbd->maxblur = 0;
	nbd->bthresh = 1.f;
	nbd->scale = 1.f;
	nbd->no_zbuf = 1;
	node->storage = nbd;
}

void register_node_type_cmp_defocus(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_DEFOCUS, "Defocus", NODE_CLASS_OP_FILTER, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_defocus_in, cmp_node_defocus_out);
	node_type_size(&ntype, 150, 120, 200);
	node_type_init(&ntype, node_composit_init_defocus);
	node_type_storage(&ntype, "NodeDefocus", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, node_composit_exec_defocus);

	nodeRegisterType(ttype, &ntype);
}
