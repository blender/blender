/*
 * radiance_hdr.c
 *
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
 * The Original Code is Copyright
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
*/


/*
  -------------------------------------------------------------------------------------------------
  Radiance High Dynamic Range image file IO
  For description and code for reading/writing of radiance hdr files by Greg Ward, refer to:
  http://radsite.lbl.gov/radiance/refer/Notes/picture_format.html
  -------------------------------------------------------------------------------------------------
*/

#ifdef WIN32
#include <io.h>
#endif
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_cmap.h"
#include "IMB_radiance_hdr.h"

/* needed constants */
#define MINELEN 8
#define MAXELEN 0x7fff
#define MINRUN	4	/* minimum run length */
#define RED 0
#define GRN 1
#define BLU 2
#define EXP 3
#define COLXS 128
typedef unsigned char RGBE[4];
typedef float fCOLOR[3];
/* copy source -> dest */
#define copy_rgbe(c1, c2) (c2[RED]=c1[RED], c2[GRN]=c1[GRN], c2[BLU]=c1[BLU], c2[EXP]=c1[EXP])
#define copy_fcol(f1, f2) (f2[RED]=f1[RED], f2[GRN]=f1[GRN], f2[BLU]=f1[BLU])

/*-------------------------------------------------------------------------------------------------*/
/* read routines */

static unsigned char* oldreadcolrs(RGBE *scan, unsigned char *mem, int xmax)
{
	int i, rshift = 0, len = xmax;
	while (len > 0) {
		scan[0][RED] = *mem++;
		scan[0][GRN] = *mem++;
		scan[0][BLU] = *mem++;
		scan[0][EXP] = *mem++;
		if (scan[0][RED] == 1 && scan[0][GRN] == 1 && scan[0][BLU] == 1) {
			for (i=scan[0][EXP]<<rshift;i>0;i--) {
				copy_rgbe(scan[-1], scan[0]);
				scan++;
				len--;
			}
			rshift += 8;
		}
		else {
			scan++;
			len--;
			rshift = 0;
		}
	}
	return mem;
}

static unsigned char* freadcolrs(RGBE *scan, unsigned char* mem, int xmax)
{
	int i, j, code, val;

	if ((xmax < MINELEN) | (xmax > MAXELEN)) return oldreadcolrs(scan, mem, xmax);

	i = *mem++;
	if (i != 2) return oldreadcolrs(scan, mem-1, xmax);

	scan[0][GRN] = *mem++;
	scan[0][BLU] = *mem++;

	i = *mem++;
	if (((scan[0][BLU] << 8) | i) != xmax) return NULL;

	for (i=0;i<4;i++)
		for (j=0;j<xmax;) {
			code = *mem++;
			if (code > 128) {
				code &= 127;
				val = *mem++;
				while (code--)
					scan[j++][i] = (unsigned char)val;
			}
			else
				while (code--)
					scan[j++][i] = *mem++;
		}
	return mem;
}

/*-------------------------------------------------------------------------------------------------*/
/* helper functions */

/* rgbe -> float color */
static void RGBE2FLOAT(RGBE rgbe, fCOLOR fcol)
{
	if (rgbe[EXP]==0) {
		fcol[RED] = fcol[GRN] = fcol[BLU] = 0;
	}
	else {
		float f = ldexp(1.0, rgbe[EXP]-(COLXS+8));
		fcol[RED] = f*(rgbe[RED] + 0.5f);
		fcol[GRN] = f*(rgbe[GRN] + 0.5f);
		fcol[BLU] = f*(rgbe[BLU] + 0.5f);
	}
}

/* float color -> rgbe */
static void FLOAT2RGBE(fCOLOR fcol, RGBE rgbe)
{
	int e;
	float d = (fcol[RED]>fcol[GRN]) ? fcol[RED] : fcol[GRN];
	if (fcol[BLU]>d) d = fcol[BLU];
	if (d <= 1e-32f)
		rgbe[RED] = rgbe[GRN] = rgbe[BLU] = rgbe[EXP] = 0;
	else {
		d = frexp(d, &e) * 256.f / d;
		rgbe[RED] = (unsigned char)(fcol[RED] * d);
		rgbe[GRN] = (unsigned char)(fcol[GRN] * d);
		rgbe[BLU] = (unsigned char)(fcol[BLU] * d);
		rgbe[EXP] = (unsigned char)(e + COLXS);
	}
}

/*-------------------------------------------------------------------------------------------------*/
/* ImBuf read */

int imb_is_a_hdr(void *buf)
{
	/* For recognition, Blender only loades first 32 bytes, so use #?RADIANCE id instead */
	if (strstr((char*)buf, "#?RADIANCE")) return 1;
	// if (strstr((char*)buf, "32-bit_rle_rgbe")) return 1;
	return 0;
}

struct ImBuf *imb_loadhdr(unsigned char *mem, int size, int flags)
{
	int found=0;
	char oriY[80], oriX[80];
	int width=0, height=0;
	RGBE* sline;
	int x, y;
	char* ptr;
	fCOLOR fcol;
	int ir, ig, ib;
	unsigned char* rect;
	struct ImBuf* ibuf;

	if (imb_is_a_hdr((void*)mem))
	{
		/* find empty line, next line is resolution info */
		for (x=1;x<size;x++) {
			if ((mem[x-1]=='\n') && (mem[x]=='\n')) {
				found = 1;
				break;
			}
		}
		if (found) {
			sscanf((char*)&mem[x+1], "%s %d %s %d", (char*)&oriY, &height, (char*)&oriX, &width);

			/* find end of this line, data right behind it */
			ptr = strchr((char*)&mem[x+1], '\n');
			ptr++;

			if (flags & IB_test) ibuf = IMB_allocImBuf(width, height, 24, 0, 0);
			else ibuf = IMB_allocImBuf(width, height, 24, 1, 0);

			if (ibuf==NULL) return NULL;
			ibuf->ftype = RADHDR;
			ibuf->xorig = ibuf->yorig = 0;

			if (flags & IB_test) return ibuf;

			/* read in and decode the actual data */
			sline = (RGBE*)MEM_mallocN(sizeof(RGBE)*width, "radhdr_read_tmpscan");
			rect = (unsigned char*)ibuf->rect;
			for (y=0;y<height;y++) {
				ptr = freadcolrs(sline, ptr, width);
				if (ptr==NULL) {
					printf("HDR decode error\n");
					MEM_freeN(sline);
					return ibuf;
				}
				for (x=0;x<width;x++) {
					/* convert to ldr */
					RGBE2FLOAT(sline[x], fcol);
					/*--------------------------------------------------------------------------------------*/
					/* THIS PART NEEDS TO BE ADAPTED TO WRITE TO IMBUF FLOATBUFFER ONCE THAT IS IMPLEMENTED
					 * temporarily now loads as regular image using simple tone mapping
					 * (of course this should NOT be done when loading to floatbuffer!) */
					fcol[RED] = 1.f-exp(fcol[RED]*-1.414213562f);
					fcol[GRN] = 1.f-exp(fcol[GRN]*-1.414213562f);
					fcol[BLU] = 1.f-exp(fcol[BLU]*-1.414213562f);
					ir = (int)(255.f*pow(fcol[RED], 0.45454545f));
					ig = (int)(255.f*pow(fcol[GRN], 0.45454545f));
					ib = (int)(255.f*pow(fcol[BLU], 0.45454545f));
					*rect++ = (unsigned char)((ir<0) ? 0 : ((ir>255) ? 255 : ir));
					*rect++ = (unsigned char)((ig<0) ? 0 : ((ig>255) ? 255 : ig));
					*rect++ = (unsigned char)((ib<0) ? 0 : ((ib>255) ? 255 : ib));
					*rect++ = 255;
					/*--------------------------------------------------------------------------------------*/
				}
			}
			MEM_freeN(sline);
			if (oriY[0]=='-') IMB_flipy(ibuf);
			return ibuf;
		}
		//else printf("Data not found!\n");
	}
	//else printf("Not a valid radiance HDR file!\n");

	return NULL;
}

/*-------------------------------------------------------------------------------------------------*/
/* ImBuf write */


static int fwritecolrs(FILE* file, int width, RGBE* rgbe_scan, unsigned char* ibufscan, float* fpscan)
{
	int i, j, beg, c2, cnt=0;
	fCOLOR fcol;
	RGBE rgbe;

	if ((ibufscan==NULL) && (fpscan==NULL)) return 0;

	/* convert scanline */
	for (i=0, j=0;i<width;i++, j+=4) {
			if (ibufscan) {
				fcol[RED] = (float)ibufscan[j] / 255.f;
				fcol[GRN] = (float)ibufscan[j+1] / 255.f;
				fcol[BLU] = (float)ibufscan[j+2] /255.f;
			}
			else {
				fcol[RED] = fpscan[j];
				fcol[GRN] = fpscan[j+1];
				fcol[BLU] = fpscan[j+2];
			}
			FLOAT2RGBE(fcol, rgbe);
			copy_rgbe(rgbe, rgbe_scan[i]);
	}

	if ((width < MINELEN) | (width > MAXELEN))	/* OOBs, write out flat */
		return (fwrite((char *)rgbe_scan, sizeof(RGBE), width, file) - width);
	/* put magic header */
	putc(2, file);
	putc(2, file);
	putc((unsigned char)(width >> 8), file);
	putc((unsigned char)(width & 255), file);
	/* put components seperately */
	for (i=0;i<4;i++) {
		for (j=0;j<width;j+=cnt) {	/* find next run */
			for (beg=j;beg<width;beg+=cnt) {
				for (cnt=1;(cnt<127) && ((beg+cnt)<width) && (rgbe_scan[beg+cnt][i] == rgbe_scan[beg][i]); cnt++);
				if (cnt>=MINRUN) break;   /* long enough */
			}
			if (((beg-j)>1) && ((beg-j) < MINRUN)) {
				c2 = j+1;
				while (rgbe_scan[c2++][i] == rgbe_scan[j][i])
					if (c2 == beg) {        /* short run */
						putc((unsigned char)(128+beg-j), file);
						putc((unsigned char)(rgbe_scan[j][i]), file);
						j = beg;
						break;
					}
			}
			while (j < beg) {     /* write out non-run */
				if ((c2 = beg-j) > 128) c2 = 128;
				putc((unsigned char)(c2), file);
				while (c2--) putc(rgbe_scan[j++][i], file);
			}
			if (cnt >= MINRUN) {      /* write out run */
				putc((unsigned char)(128+cnt), file);
				putc(rgbe_scan[beg][i], file);
			}
			else cnt = 0;
		}
	}
	return(ferror(file) ? -1 : 0);
}

static void writeHeader(FILE *file, int width, int height)
{
	fprintf(file, "#?RADIANCE");
	fputc(10, file);
	fprintf(file, "# %s", "Created with Blender");
	fputc(10, file);
	fprintf(file, "FORMAT=32-bit_rle_rgbe");
	fputc(10, file);
	fprintf(file, "EXPOSURE=%25.13f", 1.0);
	fputc(10, file);
	fputc(10, file);
	fprintf(file, "-Y %d +X %d", height, width);
	fputc(10, file);
}

short imb_savehdr(struct ImBuf *ibuf, char *name, int flags)
{
	int y, width=ibuf->x, height=ibuf->y;
	RGBE* sline;

	FILE* file = fopen(name, "wb");
	if (file==NULL) return 0;

	writeHeader(file, width, height);

	sline = (RGBE*)MEM_mallocN(sizeof(RGBE)*width, "radhdr_write_tmpscan");

	for (y=height-1;y>=0;y--) {
		if (fwritecolrs(file, width, sline, (unsigned char*)&ibuf->rect[y*width], NULL) < 0)
		{ // error
			fclose(file);
			MEM_freeN(sline);
			printf("HDR write error\n");
			return 0;
		}
	}

	fclose(file);
	MEM_freeN(sline);
	return 1;
}

/* Temporary routine to save directly from render floatbuffer.
   Called by schrijfplaatje() in toets.c */
short imb_savehdr_fromfloat(float *fbuf, char *name, int width, int height)
{
	int y;
	RGBE* sline;

	FILE* file = fopen(name, "wb");
	if (file==NULL) return 0;

	writeHeader(file, width, height);

	sline = (RGBE*)MEM_mallocN(sizeof(RGBE)*width, "radhdr_write_tmpscan");

	for (y=height-1;y>=0;y--) {
		if (fwritecolrs(file, width, sline, NULL, &fbuf[y*width*4]) < 0)
		{ // error
			fclose(file);
			MEM_freeN(sline);
			printf("HDR write error\n");
			return 0;
		}
	}

	fclose(file);
	MEM_freeN(sline);
	return 1;
}

/*-------------------------------------------------------------------------------------------------*/
