/*  image.c        
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_packedFile_types.h"

#include "BLI_blenlib.h"

#include "BKE_bmfont.h"
#include "BKE_packedFile.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

/* bad level; call to free_realtime_image */
#include "BKE_bad_level_calls.h"	

void free_image_buffers(Image *ima)
{
	int a;

	if(ima->ibuf) {
		if (ima->ibuf->userdata) {
			MEM_freeN(ima->ibuf->userdata);
			ima->ibuf->userdata = 0;
		}
		IMB_freeImBuf(ima->ibuf);
		ima->ibuf= 0;
	}
	if(ima->anim) IMB_free_anim(ima->anim);
	ima->anim= 0;
	
	for(a=0; a<BLI_ARRAY_NELEMS(ima->mipmap); a++) {
		if(ima->mipmap[a]) IMB_freeImBuf(ima->mipmap[a]);
		ima->mipmap[a]= 0;
	}
	
	free_realtime_image(ima);
}


void free_image(Image *ima)
{

	free_image_buffers(ima);
	if (ima->packedfile) {
		freePackedFile(ima->packedfile);
		ima->packedfile = NULL;
	}
}


Image *add_image(char *name)
{
	Image *ima;
	int file, len;
	char *libname, str[256], strtest[256];
	
	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
	
	file= open(str, O_BINARY|O_RDONLY);
	if(file== -1) return 0;
	close(file);
	
	/* first search an identical image */
	ima= G.main->image.first;
	while(ima) {
		strcpy(strtest, ima->name);
		BLI_convertstringcode(strtest, G.sce, G.scene->r.cfra);
		if( strcmp(strtest, str)==0 ) {
			if(ima->anim==0 || ima->id.us==0) {
				strcpy(ima->name, name);	/* for stringcode */
				ima->id.us++;
				ima->ok= 1;
				return ima;
			}
		}
		ima= ima->id.next;
	}

	len= strlen(name);
	
	while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\') len--;
	libname= name+len;
	
	ima= alloc_libblock(&G.main->image, ID_IM, libname);
	strcpy(ima->name, name);
	ima->ok= 1;
	
	ima->xrep= ima->yrep= 1;
	
	return ima;
}

void free_unused_animimages()
{
	Image *ima, *nima;

	ima= G.main->image.first;
	while(ima) {
		nima= ima->id.next;
		if(ima->id.us==0) {
			if(ima->flag & IMA_FROMANIM) free_libblock(&G.main->image, ima);
		}
		ima= nima;
	}
}


/* *********** READ AND WRITE ************** */

void makepicstring(char *string, int frame)
{
	short i,len;
	char num[10], *extension;

	if (string==0) return;

	extension= "";

	strcpy(string, G.scene->r.pic);
	BLI_convertstringcode(string, G.sce, G.scene->r.cfra);

			len= strlen(string);
			
	/* can also: sprintf(num, "%04d", frame); */

	i=4-sprintf(num,"%d",frame);
	for(;i>0;i--){
		string[len]='0';
		len++;
	}
	string[len]=0;
	strcat(string,num);

	if(G.scene->r.imtype== R_IRIS) {
		extension= ".rgb";
	}
	else if(G.scene->r.imtype==R_IRIZ) {
		extension= ".rgb";
	}
	else if(G.scene->r.imtype==R_PNG) {
		extension= ".png";
	}
#ifdef WITH_OPENEXR
	else if(G.scene->r.imtype==R_OPENEXR) {
		extension= ".exr";
	}
#endif
	else if(G.scene->r.imtype==R_TARGA) {
		extension= ".tga";
	}
	else if(G.scene->r.imtype==R_RAWTGA) {
		extension= ".tga";
	}
	else if(G.scene->r.imtype==R_JPEG90) {
		extension= ".jpg";
	}
	else if(G.scene->r.imtype==R_BMP) {
		extension= ".bmp";
	}
	
	if(G.scene->r.scemode & R_EXTENSION) strcat(string, extension);
		
}

/* ******** IMAGE WRAPPING INIT ************* */

/* used by sequencer, texture */
void converttopremul(struct ImBuf *ibuf)
{
	int x, y, val;
	char *cp;
	
	if(ibuf==0) return;
	if(ibuf->depth==24) {	/* put alpha at 255 */

		cp= (char *)(ibuf->rect);
		for(y=0; y<ibuf->y; y++) {
			for(x=0; x<ibuf->x; x++, cp+=4) {
				cp[3]= 255;
			}
		}
		return;
	}
	
	cp= (char *)(ibuf->rect);
	for(y=0; y<ibuf->y; y++) {
		for(x=0; x<ibuf->x; x++, cp+=4) {
			if(cp[3]==0) {
				cp[0]= cp[1]= cp[2]= 0;
			}
			else if(cp[3]!=255) {
				val= cp[3];
				cp[0]= (cp[0]*val)>>8;
				cp[1]= (cp[1]*val)>>8;
				cp[2]= (cp[2]*val)>>8;
			}
		}
	}
}

/* used by sequencer, texture */
struct anim *openanim(char * name, int flags)
{
	struct anim * anim;
	struct ImBuf * ibuf;
	
	anim = IMB_open_anim(name, flags);
	if (anim == 0) return(0);

	
	ibuf = IMB_anim_absolute(anim, 0);
	if (ibuf == 0) {
		printf("anim_absolute 0 failed\n");
		IMB_free_anim(anim);
		return(0);
	}
	IMB_freeImBuf(ibuf);
	
	return(anim);
}


/*
load_image handles reading the image from disk or from the packedfile.
*/

void load_image(Image * ima, int flags, char *relabase, int framenum)
{
	char name[FILE_MAXDIR + FILE_MAXFILE];

	if (ima->ibuf == NULL) {

		// is there a PackedFile with this image ?;
		if (ima->packedfile) {
			ima->ibuf = IMB_ibImageFromMemory((int *) ima->packedfile->data, ima->packedfile->size, flags);
		} else {
			strcpy(name, ima->name);
			BLI_convertstringcode(name, relabase, framenum);

			ima->ibuf = IMB_loadiffname(name , flags);
		}
		// check if the image is a font image...
		// printf("Checking for font\n");

		if (ima->ibuf) {
			detectBitmapFont(ima->ibuf);
		}
	}
}


static void de_interlace_ng(struct ImBuf *ibuf)	/* neogeo fields */
{
	struct ImBuf * tbuf1, * tbuf2;
	
	if (ibuf == 0) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;
	
	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, (short)(ibuf->y >> 1), (unsigned char)32, (int)IB_rect, (unsigned char)0);
		tbuf2 = IMB_allocImBuf(ibuf->x, (short)(ibuf->y >> 1), (unsigned char)32, (int)IB_rect, (unsigned char)0);
		
		ibuf->x *= 2;
		/* These rectop calls are broken!!! I added a trailing 0 arg... */
		IMB_rectop(tbuf1, ibuf, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(tbuf2, ibuf, 0, 0, tbuf2->x, 0, 32767, 32767, IMB_rectcpy, 0);
		
		ibuf->x /= 2;
		IMB_rectop(ibuf, tbuf1, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(ibuf, tbuf2, 0, tbuf2->y, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		
		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

static void de_interlace_st(struct ImBuf *ibuf)	/* standard fields */
{
	struct ImBuf * tbuf1, * tbuf2;
	
	if (ibuf == 0) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;
	
	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, (short)(ibuf->y >> 1), (unsigned char)32, IB_rect, 0);
		tbuf2 = IMB_allocImBuf(ibuf->x, (short)(ibuf->y >> 1), (unsigned char)32, IB_rect, 0);
		
		ibuf->x *= 2;
		/* These are brolenm as well... */
		IMB_rectop(tbuf1, ibuf, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(tbuf2, ibuf, 0, 0, tbuf2->x, 0, 32767, 32767, IMB_rectcpy, 0);
		
		ibuf->x /= 2;
		IMB_rectop(ibuf, tbuf2, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(ibuf, tbuf1, 0, tbuf2->y, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		
		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}


void ima_ibuf_is_nul(Tex *tex)
{
	void (*de_interlacefunc)(struct ImBuf *ibuf);
	Image *ima;
	int a, fra, dur;
	char str[FILE_MAXDIR+FILE_MAXFILE], *cp;
	
	ima= tex->ima;
	if(ima==0) return;
	
	strcpy(str, ima->name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
	
	if(tex->imaflag & TEX_STD_FIELD) de_interlacefunc= de_interlace_st;
	else de_interlacefunc= de_interlace_ng;
	
	if(tex->imaflag & TEX_ANIM5) {
		
		if(ima->anim==0) ima->anim = openanim(str, IB_cmap | IB_rect);
		if (ima->anim) {
			dur = IMB_anim_get_duration(ima->anim);
			
			ima->lastquality= R.osa;
			fra= ima->lastframe-1;
			
			if(fra<0) fra = 0;
			if(fra>(dur-1)) fra= dur-1;
			ima->ibuf = IMB_anim_absolute(ima->anim, fra);
			
			/* patch for textbutton with name ima (B_NAMEIMA) */
			if(ima->ibuf) {
				strcpy(ima->ibuf->name, ima->name);
				if (tex->imaflag & TEX_FIELDS) de_interlacefunc(ima->ibuf);
			}
		}
		else printf("Not an anim");
		
	} else {
		// create a packedfile for this image when autopack is on
		// for performance (IMB_loadiffname uses mmap) we don't do this by default
		if ((ima->packedfile == NULL) && (G.fileflags & G_AUTOPACK)) {
			ima->packedfile = newPackedFile(str);
		}
		
		load_image(ima, IB_rect, G.sce, G.scene->r.cfra);
		
		if (tex->imaflag & TEX_FIELDS) de_interlacefunc(ima->ibuf);
		
		ima->lastquality= R.osa;
	}
	
	if(ima->ibuf) {
		
		/* stringcodes also in ibuf. ibuf->name is used as 'undo' (buttons.c) */
		strcpy(ima->ibuf->name, ima->name);
		
		if(ima->ibuf->cmap) {
			
			if(tex->imaflag & TEX_ANIM5) {
				
				if(tex->imaflag & TEX_MORKPATCH) {
					/**** PATCH TO SET COLOR 2 RIGHT (neogeo..) */
					if(ima->ibuf->maxcol > 4) {
						cp= (char *)(ima->ibuf->cmap+2);
						cp[0]= 0x80;
					}
				}
				
				IMB_applycmap(ima->ibuf);
				IMB_convert_rgba_to_abgr(ima->ibuf->x*ima->ibuf->y, ima->ibuf->rect);
				
			}
			
			converttopremul(ima->ibuf);
		}
		
		if(R.osa) {
			
			if(tex->imaflag & TEX_ANTISCALE) {
				IMB_clever_double(ima->ibuf);
				IMB_antialias(ima->ibuf);
			}
			else if(tex->imaflag & TEX_ANTIALI) IMB_antialias(ima->ibuf);
		}
	}
	
	if(ima->ibuf==0) ima->ok= 0;
	
	for(a=0; a<BLI_ARRAY_NELEMS(ima->mipmap); a++) {
		if(ima->mipmap[a]) IMB_freeImBuf(ima->mipmap[a]);
		ima->mipmap[a]= 0;
	}
	
}



