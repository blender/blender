/*  image.c        
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2006, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
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

#include <time.h>

#ifdef _WIN32
#define open _open
#define close _close
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef WITH_OPENEXR
#include "intern/openexr/openexr_multi.h"
#endif

#include "DNA_image_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_sequence_types.h"
#include "DNA_texture_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"

#include "BKE_bmfont.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

//XXX #include "BIF_editseq.h"

#include "BLF_api.h"

#include "PIL_time.h"

#include "RE_pipeline.h"

#include "GPU_extensions.h"
#include "GPU_draw.h"

#include "BLO_sys_types.h" // for intptr_t support

/* max int, to indicate we don't store sequences in ibuf */
#define IMA_NO_INDEX	0x7FEFEFEF

/* quick lookup: supports 1 million frames, thousand passes */
#define IMA_MAKE_INDEX(frame, index)	((frame)<<10)+index
#define IMA_INDEX_FRAME(index)			(index>>10)
#define IMA_INDEX_PASS(index)			(index & ~1023)

/* ******** IMAGE PROCESSING ************* */

/* used by sequencer and image premul option - IMA_DO_PREMUL */
void converttopremul(struct ImBuf *ibuf)
{
	int x, y;
	
	if(ibuf==0) return;
	if (ibuf->rect) {
		int val;
		char *cp;
		if(ibuf->depth==24) {	/* put alpha at 255 */
			cp= (char *)(ibuf->rect);
			for(y=0; y<ibuf->y; y++) {
				for(x=0; x<ibuf->x; x++, cp+=4) {
					cp[3]= 255;
				}
			}
		} else {
			cp= (char *)(ibuf->rect);
			for(y=0; y<ibuf->y; y++) {
				for(x=0; x<ibuf->x; x++, cp+=4) {
					val= cp[3];
					cp[0]= (cp[0]*val)>>8;
					cp[1]= (cp[1]*val)>>8;
					cp[2]= (cp[2]*val)>>8;
				}
			}
		}
	}
	if (ibuf->rect_float) {
		float val;
		float *cp;
		if(ibuf->depth==24) {	/* put alpha at 1.0 */
			cp= ibuf->rect_float;;
			for(y=0; y<ibuf->y; y++) {
				for(x=0; x<ibuf->x; x++, cp+=4) {
					cp[3]= 1.0;
				}
			}
		} else {
			cp= ibuf->rect_float;
			for(y=0; y<ibuf->y; y++) {
				for(x=0; x<ibuf->x; x++, cp+=4) {
					val= cp[3];
					cp[0]= cp[0]*val;
					cp[1]= cp[1]*val;
					cp[2]= cp[2]*val;
				}
			}
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
		
		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);
		
		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, 0, tbuf2->y, 0, 0, tbuf2->x, tbuf2->y);
		
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
		
		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);
		
		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf2, 0, 0, 0, 0, tbuf2->x, tbuf2->y);
		IMB_rectcpy(ibuf, tbuf1, 0, tbuf2->y, 0, 0, tbuf1->x, tbuf1->y);
		
		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

void image_de_interlace(Image *ima, int odd)
{
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	if(ibuf) {
		if(odd)
			de_interlace_st(ibuf);
		else
			de_interlace_ng(ibuf);
	}
}

/* ***************** ALLOC & FREE, DATA MANAGING *************** */

static void image_free_buffers(Image *ima)
{
	ImBuf *ibuf;
	
	while((ibuf = ima->ibufs.first)) {
		BLI_remlink(&ima->ibufs, ibuf);
		
		if (ibuf->userdata) {
			MEM_freeN(ibuf->userdata);
			ibuf->userdata = NULL;
		}
		IMB_freeImBuf(ibuf);
	}
	
	if(ima->anim) IMB_free_anim(ima->anim);
	ima->anim= NULL;
	
	if(ima->rr) {
		RE_FreeRenderResult(ima->rr);
		ima->rr= NULL;
	}	
	
	GPU_free_image(ima);
	
	ima->ok= IMA_OK;
}

/* called by library too, do not free ima itself */
void free_image(Image *ima)
{
	image_free_buffers(ima);
	if (ima->packedfile) {
		freePackedFile(ima->packedfile);
		ima->packedfile = NULL;
	}
	BKE_icon_delete(&ima->id);
	ima->id.icon_id = 0;
	if (ima->preview) {
		BKE_previewimg_free(&ima->preview);
	}
	if (ima->render_text) {
		MEM_freeN(ima->render_text);
		ima->render_text= NULL;
	}
}

/* only image block itself */
static Image *image_alloc(const char *name, short source, short type)
{
	Image *ima;
	
	ima= alloc_libblock(&G.main->image, ID_IM, name);
	if(ima) {
		ima->ok= IMA_OK;
		
		ima->xrep= ima->yrep= 1;
		ima->aspx= ima->aspy= 1.0;
		ima->gen_x= 1024; ima->gen_y= 1024;
		ima->gen_type= 1;	/* no defines yet? */
		
		ima->source= source;
		ima->type= type;
	}
	return ima;
}

/* get the ibuf from an image cache, local use here only */
static ImBuf *image_get_ibuf(Image *ima, int index, int frame)
{
	/* this function is intended to be thread safe. with IMA_NO_INDEX this
	 * should be OK, but when iterating over the list this is more tricky
	 * */
	if(index==IMA_NO_INDEX)
		return ima->ibufs.first;
	else {
		ImBuf *ibuf;

		index= IMA_MAKE_INDEX(frame, index);
		for(ibuf= ima->ibufs.first; ibuf; ibuf= ibuf->next)
			if(ibuf->index==index)
				return ibuf;

		return NULL;
	}
}

/* no ima->ibuf anymore, but listbase */
static void image_remove_ibuf(Image *ima, ImBuf *ibuf)
{
	if(ibuf) {
		BLI_remlink(&ima->ibufs, ibuf);
		IMB_freeImBuf(ibuf);
	}
}


/* no ima->ibuf anymore, but listbase */
static void image_assign_ibuf(Image *ima, ImBuf *ibuf, int index, int frame)
{
	if(ibuf) {
		ImBuf *link;
		
		if(index!=IMA_NO_INDEX)
			index= IMA_MAKE_INDEX(frame, index);
		
		/* insert based on index */
		for(link= ima->ibufs.first; link; link= link->next)
			if(link->index>=index)
				break;

		ibuf->index= index;

		/* this function accepts link==NULL */
		BLI_insertlinkbefore(&ima->ibufs, link, ibuf);

		/* now we don't want copies? */
		if(link && ibuf->index==link->index)
			image_remove_ibuf(ima, link);
	}
}

/* empty image block, of similar type and filename */
Image *BKE_image_copy(Image *ima)
{
	Image *new= image_alloc(ima->id.name+2, ima->source, ima->type);

	BLI_strncpy(new->name, ima->name, sizeof(ima->name));
	
	new->gen_x= ima->gen_x;
	new->gen_y= ima->gen_y;
	new->gen_type= ima->gen_type;
	
	return new;
}

void BKE_image_merge(Image *dest, Image *source)
{
	ImBuf *ibuf;
	
	/* sanity check */
	if(dest && source && dest!=source) {
	
		while((ibuf= source->ibufs.first)) {
			BLI_remlink(&source->ibufs, ibuf);
			image_assign_ibuf(dest, ibuf, IMA_INDEX_PASS(ibuf->index), IMA_INDEX_FRAME(ibuf->index));
		}
		
		free_libblock(&G.main->image, source);
	}
}


/* checks if image was already loaded, then returns same image */
/* otherwise creates new. */
/* does not load ibuf itself */
/* pass on optional frame for #name images */
Image *BKE_add_image_file(const char *name, int frame)
{
	Image *ima;
	int file, len;
	const char *libname;
	char str[FILE_MAX], strtest[FILE_MAX];
	
	/* escape when name is directory */
	len= strlen(name);
	if(len) {
		if(name[len-1]=='/' || name[len-1]=='\\')
			return NULL;
	}
	
	BLI_strncpy(str, name, sizeof(str));
	BLI_convertstringcode(str, G.sce);
	
	/* exists? */
	file= open(str, O_BINARY|O_RDONLY);
	if(file== -1) return NULL;
	close(file);
	
	/* first search an identical image */
	for(ima= G.main->image.first; ima; ima= ima->id.next) {
		if(ima->source!=IMA_SRC_VIEWER && ima->source!=IMA_SRC_GENERATED) {
			BLI_strncpy(strtest, ima->name, sizeof(ima->name));
			BLI_convertstringcode(strtest, G.sce);
			
			if( strcmp(strtest, str)==0 ) {
				if(ima->anim==NULL || ima->id.us==0) {
					BLI_strncpy(ima->name, name, sizeof(ima->name));	/* for stringcode */
					ima->id.us++;										/* officially should not, it doesn't link here! */
					if(ima->ok==0)
						ima->ok= IMA_OK;
			/* RETURN! */
					return ima;
				}
			}
		}
	}
	/* add new image */
	
	/* create a short library name */
	len= strlen(name);
	
	while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\') len--;
	libname= name+len;
	
	ima= image_alloc(libname, IMA_SRC_FILE, IMA_TYPE_IMAGE);
	BLI_strncpy(ima->name, name, sizeof(ima->name));
	
	/* do a wild guess! */
	if(BLI_testextensie(name, ".avi") || BLI_testextensie(name, ".mov")
			|| BLI_testextensie(name, ".mpg")  || BLI_testextensie(name, ".mp4"))
		ima->source= IMA_SRC_MOVIE;
	
	return ima;
}

static ImBuf *add_ibuf_size(int width, int height, char *name, int floatbuf, short uvtestgrid, float color[4])
{
	ImBuf *ibuf;
	float h=0.0, hoffs=0.0, hue=0.0, s=0.9, v=0.9, r, g, b;
	unsigned char *rect= NULL;
	float *rect_float= NULL;
	int x, y;
	int checkerwidth=32, dark=1;
	
	if (floatbuf) {
		ibuf= IMB_allocImBuf(width, height, 24, IB_rectfloat, 0);
		rect_float= (float*)ibuf->rect_float;
	}
	else {
		ibuf= IMB_allocImBuf(width, height, 24, IB_rect, 0);
		rect= (unsigned char*)ibuf->rect;
	}
	
	strcpy(ibuf->name, "//Untitled");
	ibuf->userflags |= IB_BITMAPDIRTY;
	
	if (uvtestgrid) {
		/* these two passes could be combined into one, but it's more readable and 
		* easy to tweak like this, speed isn't really that much of an issue in this situation... */
		
		/* checkers */
		for(y=0; y<height; y++) {
			dark = powf(-1.0f, floorf(y / checkerwidth));
			
			for(x=0; x<width; x++) {
				if (x % checkerwidth == 0) dark *= -1;
				
				if (floatbuf) {
					if (dark > 0) {
						rect_float[0] = rect_float[1] = rect_float[2] = 0.25f;
						rect_float[3] = 1.0f;
					} else {
						rect_float[0] = rect_float[1] = rect_float[2] = 0.58f;
						rect_float[3] = 1.0f;
					}
					rect_float+=4;
				}
				else {
					if (dark > 0) {
						rect[0] = rect[1] = rect[2] = 64;
						rect[3] = 255;
					} else {
						rect[0] = rect[1] = rect[2] = 150;
						rect[3] = 255;
					}
					rect += 4;
				}
			}
		}
		
		/* 2nd pass, colored + */
		if (floatbuf) rect_float= (float*)ibuf->rect_float;
		else rect= (unsigned char*)ibuf->rect;
		
		for(y=0; y<height; y++) {
			hoffs = 0.125f * floorf(y / checkerwidth);
			
			for(x=0; x<width; x++) {
				h = 0.125f * floorf(x / checkerwidth);
				
				if ((fabs((x % checkerwidth) - (checkerwidth / 2)) < 4) &&
					(fabs((y % checkerwidth) - (checkerwidth / 2)) < 4)) {
					
					if ((fabs((x % checkerwidth) - (checkerwidth / 2)) < 1) ||
						(fabs((y % checkerwidth) - (checkerwidth / 2)) < 1)) {
						
						hue = fmodf(fabs(h-hoffs), 1.0f);
						hsv_to_rgb(hue, s, v, &r, &g, &b);
						
						if (floatbuf) {
							rect_float[0]= r;
							rect_float[1]= g;
							rect_float[2]= b;
							rect_float[3]= 1.0f;
						}
						else {
							rect[0]= (char)(r * 255.0f);
							rect[1]= (char)(g * 255.0f);
							rect[2]= (char)(b * 255.0f);
							rect[3]= 255;
						}
					}
				}

				if (floatbuf)
					rect_float+=4;
				else
					rect+=4;
			}
		}
	} else {	/* blank image */
		char ccol[4];

		ccol[0]= (char)(color[0]*255.0f);
		ccol[1]= (char)(color[1]*255.0f);
		ccol[2]= (char)(color[2]*255.0f);
		ccol[3]= (char)(color[3]*255.0f);

		for(y=0; y<height; y++) {
			for(x=0; x<width; x++) {
				if (floatbuf) {
					rect_float[0]= color[0];
					rect_float[1]= color[1];
					rect_float[2]= color[2];
					rect_float[3]= color[3];
					rect_float+=4;
				}
				else {
					rect[0]= ccol[0];
					rect[1]= ccol[1];
					rect[2]= ccol[2];
					rect[3]= ccol[3];
					rect+=4;
				}
			}
		}
	}
	return ibuf;
}

/* adds new image block, creates ImBuf and initializes color */
Image *BKE_add_image_size(int width, int height, char *name, int floatbuf, short uvtestgrid, float color[4])
{
	Image *ima;
	
	/* on save, type is changed to FILE in editsima.c */
	ima= image_alloc(name, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);
	
	if (ima) {
		ImBuf *ibuf;
		
		BLI_strncpy(ima->name, name, FILE_MAX);
		ima->gen_x= width;
		ima->gen_y= height;
		ima->gen_type= uvtestgrid;
		
		ibuf= add_ibuf_size(width, height, name, floatbuf, uvtestgrid, color);
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
		
		ima->ok= IMA_OK_LOADED;
	}

	return ima;
}

/* packs rect from memory as PNG */
void BKE_image_memorypack(Image *ima)
{
	ImBuf *ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);
	
	if(ibuf==NULL)
		return;
	if (ima->packedfile) {
		freePackedFile(ima->packedfile);
		ima->packedfile = NULL;
	}
	
	ibuf->ftype= PNG;
	ibuf->depth= 32;
	
	IMB_saveiff(ibuf, ibuf->name, IB_rect | IB_mem);
	if(ibuf->encodedbuffer==NULL) {
		printf("memory save for pack error\n");
	}
	else {
		PackedFile *pf = MEM_callocN(sizeof(*pf), "PackedFile");
		
		pf->data = ibuf->encodedbuffer;
		pf->size = ibuf->encodedsize;
		ima->packedfile= pf;
		ibuf->encodedbuffer= NULL;
		ibuf->encodedsize= 0;
		ibuf->userflags &= ~IB_BITMAPDIRTY;
		
		if(ima->source==IMA_SRC_GENERATED) {
			ima->source= IMA_SRC_FILE;
			ima->type= IMA_TYPE_IMAGE;
		}
	}
}

void tag_image_time(Image *ima)
{
	if (ima)
		ima->lastused = (int)PIL_check_seconds_timer();
}

#if 0
static void tag_all_images_time() 
{
	Image *ima;
	int ctime = (int)PIL_check_seconds_timer();

	ima= G.main->image.first;
	while(ima) {
		if(ima->bindcode || ima->repbind || ima->ibufs.first) {
			ima->lastused = ctime;
		}
	}
}
#endif

void free_old_images()
{
	Image *ima;
	static int lasttime = 0;
	int ctime = (int)PIL_check_seconds_timer();
	
	/* 
	   Run garbage collector once for every collecting period of time 
	   if textimeout is 0, that's the option to NOT run the collector
	*/
	if (U.textimeout == 0 || ctime % U.texcollectrate || ctime == lasttime)
		return;

	lasttime = ctime;

	ima= G.main->image.first;
	while(ima) {
		if((ima->flag & IMA_NOCOLLECT)==0 && ctime - ima->lastused > U.textimeout) {
			/*
			   If it's in GL memory, deallocate and set time tag to current time
			   This gives textures a "second chance" to be used before dying.
			*/
			if(ima->bindcode || ima->repbind) {
				GPU_free_image(ima);
				ima->lastused = ctime;
			}
			/* Otherwise, just kill the buffers */
			else if (ima->ibufs.first) {
				image_free_buffers(ima);
			}
		}
		ima = ima->id.next;
	}
}

static uintptr_t image_mem_size(Image *ima)
{
	ImBuf *ibuf, *ibufm;
	int level;
	uintptr_t size = 0;

	size= 0;
	
	/* viewers have memory depending on other rules, has no valid rect pointer */
	if(ima->source==IMA_SRC_VIEWER)
		return 0;
	
	for(ibuf= ima->ibufs.first; ibuf; ibuf= ibuf->next) {
		if(ibuf->rect) size += MEM_allocN_len(ibuf->rect);
		else if(ibuf->rect_float) size += MEM_allocN_len(ibuf->rect_float);

		for(level=0; level<IB_MIPMAP_LEVELS; level++) {
			ibufm= ibuf->mipmap[level];
			if(ibufm) {
				if(ibufm->rect) size += MEM_allocN_len(ibufm->rect);
				else if(ibufm->rect_float) size += MEM_allocN_len(ibufm->rect_float);
			}
		}
	}

	return size;
}

void BKE_image_print_memlist(void)
{
	Image *ima;
	uintptr_t size, totsize= 0;

	for(ima= G.main->image.first; ima; ima= ima->id.next)
		totsize += image_mem_size(ima);

	printf("\ntotal image memory len: %.3lf MB\n", (double)totsize/(double)(1024*1024));

	for(ima= G.main->image.first; ima; ima= ima->id.next) {
		size= image_mem_size(ima);

		if(size)
			printf("%s len: %.3f MB\n", ima->id.name+2, (double)size/(double)(1024*1024));
	}
}

void BKE_image_free_all_textures(void)
{
	Tex *tex;
	Image *ima;
	unsigned int totsize= 0;
	
	for(ima= G.main->image.first; ima; ima= ima->id.next)
		ima->id.flag &= ~LIB_DOIT;
	
	for(tex= G.main->tex.first; tex; tex= tex->id.next)
		if(tex->ima)
			tex->ima->id.flag |= LIB_DOIT;
	
	for(ima= G.main->image.first; ima; ima= ima->id.next) {
		if(ima->ibufs.first && (ima->id.flag & LIB_DOIT)) {
			ImBuf *ibuf;
			for(ibuf= ima->ibufs.first; ibuf; ibuf= ibuf->next) {
				if(ibuf->mipmap[0]) 
					totsize+= 1.33*ibuf->x*ibuf->y*4;
				else
					totsize+= ibuf->x*ibuf->y*4;
			}
			image_free_buffers(ima);
		}
	}
	/* printf("freed total %d MB\n", totsize/(1024*1024)); */
}

/* except_frame is weak, only works for seqs without offset... */
void BKE_image_free_anim_ibufs(Image *ima, int except_frame)
{
	ImBuf *ibuf, *nbuf;

	for(ibuf= ima->ibufs.first; ibuf; ibuf= nbuf) {
		nbuf= ibuf->next;
		if(ibuf->userflags & IB_BITMAPDIRTY)
			continue;
		if(ibuf->index==IMA_NO_INDEX)
			continue;
		if(except_frame!=IMA_INDEX_FRAME(ibuf->index)) {
			BLI_remlink(&ima->ibufs, ibuf);
			
			if (ibuf->userdata) {
				MEM_freeN(ibuf->userdata);
				ibuf->userdata = NULL;
			}
			IMB_freeImBuf(ibuf);
		}					
	}
}

void BKE_image_all_free_anim_ibufs(int cfra)
{
	Image *ima;
	
	for(ima= G.main->image.first; ima; ima= ima->id.next)
		if(ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE))
			BKE_image_free_anim_ibufs(ima, cfra);
}


/* *********** READ AND WRITE ************** */

int BKE_imtype_to_ftype(int imtype)
{
	if(imtype==0)
		return TGA;
	else if(imtype== R_IRIS) 
		return IMAGIC;
	else if (imtype==R_RADHDR)
		return RADHDR;
	else if (imtype==R_PNG)
		return PNG;
#ifdef WITH_DDS
	else if (imtype==R_DDS)
		return DDS;
#endif
	else if (imtype==R_BMP)
		return BMP;
	else if (imtype==R_TIFF)
		return TIF;
	else if (imtype==R_OPENEXR || imtype==R_MULTILAYER)
		return OPENEXR;
	else if (imtype==R_CINEON)
		return CINEON;
	else if (imtype==R_DPX)
		return DPX;
	else if (imtype==R_TARGA)
		return TGA;
	else if(imtype==R_RAWTGA)
		return RAWTGA;
	else if(imtype==R_HAMX)
		return AN_hamx;
#ifdef WITH_OPENJPEG
	else if(imtype==R_JP2)
		return JP2;
#endif
	else
		return JPG|90;
}

int BKE_ftype_to_imtype(int ftype)
{
	if(ftype==0)
		return TGA;
	else if(ftype == IMAGIC) 
		return R_IRIS;
	else if (ftype & RADHDR)
		return R_RADHDR;
	else if (ftype & PNG)
		return R_PNG;
#ifdef WITH_DDS
	else if (ftype & DDS)
		return R_DDS;
#endif
	else if (ftype & BMP)
		return R_BMP;
	else if (ftype & TIF)
		return R_TIFF;
	else if (ftype & OPENEXR)
		return R_OPENEXR;
	else if (ftype & CINEON)
		return R_CINEON;
	else if (ftype & DPX)
		return R_DPX;
	else if (ftype & TGA)
		return R_TARGA;
	else if(ftype & RAWTGA)
		return R_RAWTGA;
	else if(ftype == AN_hamx)
		return R_HAMX;
#ifdef WITH_OPENJPEG
	else if(ftype & JP2)
		return R_JP2;
#endif
	else
		return R_JPEG90;
}


int BKE_imtype_is_movie(int imtype)
{
	switch(imtype) {
	case R_MOVIE:
	case R_AVIRAW:
	case R_AVIJPEG:
	case R_AVICODEC:
	case R_QUICKTIME:
	case R_FFMPEG:
	case R_H264:
	case R_THEORA:
	case R_XVID:
	case R_FRAMESERVER:
			return 1;
	}
	return 0;
}

void BKE_add_image_extension(char *string, int imtype)
{
	char *extension="";
	
	if(imtype== R_IRIS) {
		if(!BLI_testextensie(string, ".rgb"))
			extension= ".rgb";
	}
	else if(imtype==R_IRIZ) {
		if(!BLI_testextensie(string, ".rgb"))
			extension= ".rgb";
	}
	else if(imtype==R_RADHDR) {
		if(!BLI_testextensie(string, ".hdr"))
			extension= ".hdr";
	}
	else if (ELEM5(imtype, R_PNG, R_FFMPEG, R_H264, R_THEORA, R_XVID)) {
		if(!BLI_testextensie(string, ".png"))
			extension= ".png";
	}
#ifdef WITH_DDS
	else if(imtype==R_DDS) {
		if(!BLI_testextensie(string, ".dds"))
			extension= ".dds";
	}
#endif
	else if(imtype==R_RAWTGA) {
		if(!BLI_testextensie(string, ".tga"))
			extension= ".tga";
	}
	else if(imtype==R_BMP) {
		if(!BLI_testextensie(string, ".bmp"))
			extension= ".bmp";
	}
	else if(G.have_libtiff && (imtype==R_TIFF)) {
		if(!BLI_testextensie(string, ".tif") && 
			!BLI_testextensie(string, ".tiff")) extension= ".tif";
	}
#ifdef WITH_OPENEXR
	else if( ELEM(imtype, R_OPENEXR, R_MULTILAYER)) {
		if(!BLI_testextensie(string, ".exr"))
			extension= ".exr";
	}
#endif
	else if(imtype==R_CINEON){
		if (!BLI_testextensie(string, ".cin"))
			extension= ".cin";
	}
	else if(imtype==R_DPX){
		if (!BLI_testextensie(string, ".dpx"))
			extension= ".dpx";
	}
	else if(imtype==R_TARGA) {
		if(!BLI_testextensie(string, ".tga"))
			extension= ".tga";
	}
#ifdef WITH_OPENJPEG
	else if(imtype==R_JP2) {
		if(!BLI_testextensie(string, ".jp2"))
			extension= ".jp2";
	}
#endif
	else { //   R_MOVIE, R_AVICODEC, R_AVIRAW, R_AVIJPEG, R_JPEG90, R_QUICKTIME etc
		if(!( BLI_testextensie(string, ".jpg") || BLI_testextensie(string, ".jpeg")))
			extension= ".jpg";
	}

	strcat(string, extension);
}

/* could allow access externally - 512 is for long names, 64 is for id names */
typedef struct StampData {
	char 	file[512];
	char 	note[512];
	char 	date[512];
	char 	marker[512];
	char 	time[512];
	char 	frame[512];
	char 	camera[64];
	char 	scene[64];
	char 	strip[64];
	char 	rendertime[64];
} StampData;

static void stampdata(Scene *scene, StampData *stamp_data, int do_prefix)
{
	char text[256];
	
#ifndef WIN32
	struct tm *tl;
	time_t t;
#else
	char sdate[9];
#endif /* WIN32 */
	
	if (scene->r.stamp & R_STAMP_FILENAME) {
		if (G.relbase_valid) {
			if (do_prefix)		sprintf(stamp_data->file, "File %s", G.sce);
			else				sprintf(stamp_data->file, "%s", G.sce);
		} else {
			if (do_prefix)		strcpy(stamp_data->file, "File <untitled>");
			else				strcpy(stamp_data->file, "<untitled>");
		}
	} else {
		stamp_data->file[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_NOTE) {
		/* Never do prefix for Note */
		sprintf(stamp_data->note, "%s", scene->r.stamp_udata);
	} else {
		stamp_data->note[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_DATE) {
#ifdef WIN32
		_strdate (sdate);
		sprintf (text, "%s", sdate);
#else
		t = time (NULL);
		tl = localtime (&t);
		sprintf (text, "%04d-%02d-%02d", tl->tm_year+1900, tl->tm_mon+1, tl->tm_mday);
#endif /* WIN32 */
		if (do_prefix)		sprintf(stamp_data->date, "Date %s", text);
		else				sprintf(stamp_data->date, "%s", text);
	} else {
		stamp_data->date[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_MARKER) {
		char *name = scene_find_last_marker_name(scene, CFRA);
	
		if (name)	strcpy(text, name);
		else 		strcpy(text, "<none>");
		
		if (do_prefix)		sprintf(stamp_data->marker, "Marker %s", text);
		else				sprintf(stamp_data->marker, "%s", text);
	} else {
		stamp_data->marker[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_TIME) {
		int h, m, s, f;
		h= m= s= f= 0;
		f = (int)(scene->r.cfra % scene->r.frs_sec);
		s = (int)(scene->r.cfra / scene->r.frs_sec);

		if (s) {
			m = (int)(s / 60);
			s %= 60;

			if (m) {
				h = (int)(m / 60);
				m %= 60;
			}
		}

		if (scene->r.frs_sec < 100)
			sprintf (text, "%02d:%02d:%02d.%02d", h, m, s, f);
		else
			sprintf (text, "%02d:%02d:%02d.%03d", h, m, s, f);
		
		if (do_prefix)		sprintf(stamp_data->time, "Time %s", text);
		else				sprintf(stamp_data->time, "%s", text);
	} else {
		stamp_data->time[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_FRAME) {
		char format[32];
		if (do_prefix)		sprintf(format, "Frame %%0%di", 1 + (int) log10(scene->r.efra));
		else				sprintf(format, "%%0%di", 1 + (int) log10(scene->r.efra));
		sprintf (stamp_data->frame, format, scene->r.cfra);
	} else {
		stamp_data->frame[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_CAMERA) {
		if (scene->camera) strcpy(text, ((Camera *) scene->camera)->id.name+2);
		else 		strcpy(text, "<none>");
		
		if (do_prefix)		sprintf(stamp_data->camera, "Camera %s", text);
		else				sprintf(stamp_data->camera, "%s", text);
	} else {
		stamp_data->camera[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_SCENE) {
		if (do_prefix)		sprintf(stamp_data->scene, "Scene %s", scene->id.name+2);
		else				sprintf(stamp_data->scene, "%s", scene->id.name+2);
	} else {
		stamp_data->scene[0] = '\0';
	}
	
	if (scene->r.stamp & R_STAMP_SEQSTRIP) {
		Sequence *seq= NULL; //XXX = get_foreground_frame_seq(scene->r.cfra);
	
		if (seq) strcpy(text, seq->name+2);
		else 		strcpy(text, "<none>");
		
		if (do_prefix)		sprintf(stamp_data->strip, "Strip %s", text);
		else				sprintf(stamp_data->strip, "%s", text);
	} else {
		stamp_data->strip[0] = '\0';
	}

	{
		Render *re= RE_GetRender(scene->id.name);
		RenderStats *stats= re ? RE_GetStats(re):NULL;

		if (stats && (scene->r.stamp & R_STAMP_RENDERTIME)) {
			BLI_timestr(stats->lastframetime, text);

			if (do_prefix)		sprintf(stamp_data->rendertime, "RenderTime %s", text);
			else				sprintf(stamp_data->rendertime, "%s", text);
		} else {
			stamp_data->rendertime[0] = '\0';
		}
	}
}

// XXX - Bad level call.
extern int datatoc_bmonofont_ttf_size;
extern char datatoc_bmonofont_ttf[];

// XXX - copied from text_font_begin
static void stamp_font_begin(int size)
{
	static int mono= -1;

	if (mono == -1)
		mono= BLF_load_mem("monospace", (unsigned char *)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	BLF_set(mono);
	BLF_aspect(1.0);
	BLF_size(size, 72);
}

void BKE_stamp_buf(Scene *scene, unsigned char *rect, float *rectf, int width, int height, int channels)
{
	struct StampData stamp_data;
	float w, h, pad;
	int x, y;
	
	if (!rect && !rectf)
		return;
	
	stampdata(scene, &stamp_data, 1);

	/* TODO, do_versions */
	if(scene->r.stamp_font_id < 8)
		scene->r.stamp_font_id= 12;

	stamp_font_begin(scene->r.stamp_font_id);

	BLF_buffer(rectf, rect, width, height, channels);
	BLF_buffer_col(scene->r.fg_stamp[0], scene->r.fg_stamp[1], scene->r.fg_stamp[2], 1.0);
	pad= BLF_width("--");

	x= 0;
	y= height;

	if (stamp_data.file[0]) {
		/* Top left corner */
		BLF_width_and_height(stamp_data.file, &w, &h);
		y -= h;

		/* also a little of space to the background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y-3, w+3, y+h+3);

		/* and draw the text. */
		BLF_position(x, y, 0.0);
		BLF_draw_buffer(stamp_data.file);

		/* the extra pixel for background. */
		y -= 4;
	}

	/* Top left corner, below File */
	if (stamp_data.note[0]) {
		BLF_width_and_height(stamp_data.note, &w, &h);
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, 0, y-2, w+3, y+h+2);

		BLF_position(x, y+1, 0.0);
		BLF_draw_buffer(stamp_data.note);

		/* the extra pixel for background. */
		y -= 4;
	}
	
	/* Top left corner, below File (or Note) */
	if (stamp_data.date[0]) {
		BLF_width_and_height(stamp_data.date, &w, &h);
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, 0, y-3, w+3, y+h+3);

		BLF_position(x, y, 0.0);
		BLF_draw_buffer(stamp_data.date);

		/* the extra pixel for background. */
		y -= 4;
	}

	/* Top left corner, below File, Date or Note */
	if (stamp_data.rendertime[0]) {
		BLF_width_and_height(stamp_data.rendertime, &w, &h);
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, 0, y-3, w+3, y+h+3);

		BLF_position(x, y, 0.0);
		BLF_draw_buffer(stamp_data.rendertime);
	}

	x= 0;
	y= 0;

	/* Bottom left corner, leaving space for timing */
	if (stamp_data.marker[0]) {
		BLF_width_and_height(stamp_data.marker, &w, &h);

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y, w+2, y+h+3);

		/* and pad the text. */
		BLF_position(x, y+3, 0.0);
		BLF_draw_buffer(stamp_data.marker);

		/* space width. */
		x += w + pad;
	}
	
	/* Left bottom corner */
	if (stamp_data.time[0]) {
		BLF_width_and_height(stamp_data.time, &w, &h);

		/* extra space for background */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y, x+w+2, y+h+3);

		/* and pad the text. */
		BLF_position(x, y+3, 0.0);
		BLF_draw_buffer(stamp_data.time);

		/* space width. */
		x += w + pad;
	}
	
	if (stamp_data.frame[0]) {
		BLF_width_and_height(stamp_data.frame, &w, &h);

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y, x+w+2, y+h+3);

		/* and pad the text. */
		BLF_position(x, y+3, 0.0);

		BLF_draw_buffer(stamp_data.frame);

		/* space width. */
		x += w + pad;
	}

	if (stamp_data.camera[0]) {
		BLF_width_and_height(stamp_data.camera, &w, &h);

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y, x+w+2, y+h+3);
		BLF_position(x, y+3, 0.0);
		BLF_draw_buffer(stamp_data.camera);
	}
	
	if (stamp_data.scene[0]) {
		BLF_width_and_height(stamp_data.scene, &w, &h);

		/* Bottom right corner, with an extra space because blenfont is too strict! */
		x= width - w - 2;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y, x+w+3, y+h+3);

		/* and pad the text. */
		BLF_position(x, y+3, 0.0);
		BLF_draw_buffer(stamp_data.scene);
	}
	
	if (stamp_data.strip[0]) {
		BLF_width_and_height(stamp_data.scene, &w, &h);

		/* Top right corner, with an extra space because blenfont is too strict! */
		x= width - w - pad;
		y= height - h;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, x, y-3, x+w+pad, y+h+3);

		BLF_position(x, y, 0.0);
		BLF_draw_buffer(stamp_data.strip);
	}

	/* cleanup the buffer. */
	BLF_buffer(NULL, NULL, 0, 0, 0);
}

void BKE_stamp_info(Scene *scene, struct ImBuf *ibuf)
{
	struct StampData stamp_data;

	if (!ibuf)	return;
	
	/* fill all the data values, no prefix */
	stampdata(scene, &stamp_data, 0);
	
	if (stamp_data.file[0])		IMB_imginfo_change_field (ibuf, "File",		stamp_data.file);
	if (stamp_data.note[0])		IMB_imginfo_change_field (ibuf, "Note",		stamp_data.note);
	if (stamp_data.date[0])		IMB_imginfo_change_field (ibuf, "Date",		stamp_data.date);
	if (stamp_data.marker[0])	IMB_imginfo_change_field (ibuf, "Marker",	stamp_data.marker);
	if (stamp_data.time[0])		IMB_imginfo_change_field (ibuf, "Time",		stamp_data.time);
	if (stamp_data.frame[0])	IMB_imginfo_change_field (ibuf, "Frame",	stamp_data.frame);
	if (stamp_data.camera[0])	IMB_imginfo_change_field (ibuf, "Camera",	stamp_data.camera);
	if (stamp_data.scene[0])	IMB_imginfo_change_field (ibuf, "Scene",	stamp_data.scene);
	if (stamp_data.strip[0])	IMB_imginfo_change_field (ibuf, "Strip",	stamp_data.strip);
}

int BKE_write_ibuf(Scene *scene, ImBuf *ibuf, char *name, int imtype, int subimtype, int quality)
{
	int ok;
	
	if(imtype==0);
	else if(imtype== R_IRIS) 
		ibuf->ftype= IMAGIC;
	else if ((imtype==R_RADHDR)) {
		ibuf->ftype= RADHDR;
	}
	else if (ELEM5(imtype, R_PNG, R_FFMPEG, R_H264, R_THEORA, R_XVID)) {
		ibuf->ftype= PNG;
	}
#ifdef WITH_DDS
	else if ((imtype==R_DDS)) {
		ibuf->ftype= DDS;
	}
#endif
	else if ((imtype==R_BMP)) {
		ibuf->ftype= BMP;
	}
	else if ((G.have_libtiff) && (imtype==R_TIFF)) {
		ibuf->ftype= TIF;

		if(subimtype & R_TIFF_16BIT)
			ibuf->ftype |= TIF_16BIT;
	}
#ifdef WITH_OPENEXR
	else if (imtype==R_OPENEXR || imtype==R_MULTILAYER) {
		ibuf->ftype= OPENEXR;
		if(subimtype & R_OPENEXR_HALF)
			ibuf->ftype |= OPENEXR_HALF;
		ibuf->ftype |= (quality & OPENEXR_COMPRESS);
		
		if(!(subimtype & R_OPENEXR_ZBUF))
			ibuf->zbuf_float = NULL;	/* signal for exr saving */
		
	}
#endif
	else if (imtype==R_CINEON) {
		ibuf->ftype = CINEON;
	}
	else if (imtype==R_DPX) {
		ibuf->ftype = DPX;
	}
	else if (imtype==R_TARGA) {
		ibuf->ftype= TGA;
	}
	else if(imtype==R_RAWTGA) {
		ibuf->ftype= RAWTGA;
	}
	else if(imtype==R_HAMX) {
		ibuf->ftype= AN_hamx;
	}
#ifdef WITH_OPENJPEG
	else if(imtype==R_JP2) {
		if(quality < 10) quality= 90;
		ibuf->ftype= JP2|quality;
		
		if (subimtype & R_JPEG2K_16BIT) {
			ibuf->ftype |= JP2_16BIT;
		} else if (subimtype & R_JPEG2K_12BIT) {
			ibuf->ftype |= JP2_12BIT;
		}
		
		if (subimtype & R_JPEG2K_YCC) {
			ibuf->ftype |= JP2_YCC;
		}
		
		if (subimtype & R_JPEG2K_CINE_PRESET) {
			ibuf->ftype |= JP2_CINE;
			if (subimtype & R_JPEG2K_CINE_48FPS)
				ibuf->ftype |= JP2_CINE_48FPS;
		}
	}
#endif
	else {
		/* R_JPEG90, R_MOVIE, etc. default we save jpegs */
		if(quality < 10) quality= 90;
		ibuf->ftype= JPG|quality;
		if(ibuf->depth==32) ibuf->depth= 24;	/* unsupported feature only confuses other s/w */
	}
	
	BLI_make_existing_file(name);

	if(scene && scene->r.stamp & R_STAMP_ALL)
		BKE_stamp_info(scene, ibuf);
	
	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}
	
	return(ok);
}


void BKE_makepicstring(char *string, char *base, int frame, int imtype, int use_ext)
{
	if (string==NULL) return;
	BLI_strncpy(string, base, FILE_MAX - 10);	/* weak assumption */
	BLI_convertstringcode(string, G.sce);
	BLI_convertstringframe(string, frame, 4);

	if(use_ext)
		BKE_add_image_extension(string, imtype);
		
}

/* used by sequencer too */
struct anim *openanim(char *name, int flags)
{
	struct anim *anim;
	struct ImBuf *ibuf;
	
	anim = IMB_open_anim(name, flags);
	if (anim == NULL) return(0);

	ibuf = IMB_anim_absolute(anim, 0);
	if (ibuf == NULL) {
		if(BLI_exists(name))
			printf("not an anim: %s\n", name);
		else
			printf("anim file doesn't exist: %s\n", name);
		IMB_free_anim(anim);
		return(0);
	}
	IMB_freeImBuf(ibuf);
	
	return(anim);
}

/* ************************* New Image API *************** */


/* Notes about Image storage 
- packedfile
  -> written in .blend
- filename
  -> written in .blend
- movie
  -> comes from packedfile or filename
- renderresult
  -> comes from packedfile or filename
- listbase
  -> ibufs from exrhandle
- flipbook array
  -> ibufs come from movie, temporary renderresult or sequence
- ibuf
  -> comes from packedfile or filename or generated

*/


/* forces existence of 1 Image for renderout or nodes, returns Image */
/* name is only for default, when making new one */
Image *BKE_image_verify_viewer(int type, const char *name)
{
	Image *ima;
	
	for(ima=G.main->image.first; ima; ima= ima->id.next)
		if(ima->source==IMA_SRC_VIEWER)
			if(ima->type==type)
				break;
	
	if(ima==NULL)
		ima= image_alloc(name, IMA_SRC_VIEWER, type);
	
	/* happens on reload, imagewindow cannot be image user when hidden*/
	if(ima->id.us==0)
		id_us_plus(&ima->id);

	return ima;
}

void BKE_image_assign_ibuf(Image *ima, ImBuf *ibuf)
{
	image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
}

void BKE_image_signal(Image *ima, ImageUser *iuser, int signal)
{
	if(ima==NULL)
		return;
	
	switch(signal) {
	case IMA_SIGNAL_FREE:
		image_free_buffers(ima);
		if(iuser)
			iuser->ok= 1;
		break;
	case IMA_SIGNAL_SRC_CHANGE:
		if(ima->type == IMA_TYPE_UV_TEST)
			if(ima->source != IMA_SRC_GENERATED)
				ima->type= IMA_TYPE_IMAGE;

		if(ima->source==IMA_SRC_GENERATED) {
			if(ima->gen_x==0 || ima->gen_y==0) {
				ImBuf *ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);
				if(ibuf) {
					ima->gen_x= ibuf->x;
					ima->gen_y= ibuf->y;
				}
			}
		}

		image_free_buffers(ima);

		ima->ok= 1;
		if(iuser)
			iuser->ok= 1;
		break;
			
	case IMA_SIGNAL_RELOAD:
		/* try to repack file */
		if(ima->packedfile) {
			PackedFile *pf;
			pf = newPackedFile(NULL, ima->name);
			if (pf) {
				freePackedFile(ima->packedfile);
				ima->packedfile = pf;
				image_free_buffers(ima);
			} else {
				printf("ERROR: Image not available. Keeping packed image\n");
			}
		}
		else
			image_free_buffers(ima);
		
		if(iuser)
			iuser->ok= 1;
		
		break;
	case IMA_SIGNAL_USER_NEW_IMAGE:
		if(iuser) {
			iuser->ok= 1;
			if(ima->source==IMA_SRC_FILE || ima->source==IMA_SRC_SEQUENCE) {
				if(ima->type==IMA_TYPE_MULTILAYER) {
					iuser->multi_index= 0;
					iuser->layer= iuser->pass= 0;
				}
			}
		}
		break;
	}
}

/* if layer or pass changes, we need an index for the imbufs list */
/* note it is called for rendered results, but it doesnt use the index! */
/* and because rendered results use fake layer/passes, don't correct for wrong indices here */
RenderPass *BKE_image_multilayer_index(RenderResult *rr, ImageUser *iuser)
{
	RenderLayer *rl;
	RenderPass *rpass= NULL;
	
	if(rr==NULL) 
		return NULL;
	
	if(iuser) {
		short index= 0, rl_index= 0, rp_index;
		
		for(rl= rr->layers.first; rl; rl= rl->next, rl_index++) {
			rp_index= 0;
			for(rpass= rl->passes.first; rpass; rpass= rpass->next, index++, rp_index++)
				if(iuser->layer==rl_index && iuser->pass==rp_index)
					break;
			if(rpass)
				break;
		}
		
		if(rpass)
			iuser->multi_index= index;
		else 
			iuser->multi_index= 0;
	}
	if(rpass==NULL) {
		rl= rr->layers.first;
		if(rl)
			rpass= rl->passes.first;
	}
	
	return rpass;
}

RenderResult *BKE_image_acquire_renderresult(struct Scene *scene, Image *ima)
{
	if(ima->rr)
		return ima->rr;
	else if(ima->type==IMA_TYPE_R_RESULT)
		return RE_AcquireResultRead(RE_GetRender(scene->id.name));
	return NULL;
}

void BKE_image_release_renderresult(struct Scene *scene, Image *ima)
{
	if(ima->rr);
	else if(ima->type==IMA_TYPE_R_RESULT)
		RE_ReleaseResult(RE_GetRender(scene->id.name));
}

/* after imbuf load, openexr type can return with a exrhandle open */
/* in that case we have to build a render-result */
static void image_create_multilayer(Image *ima, ImBuf *ibuf, int framenr)
{
	
	ima->rr= RE_MultilayerConvert(ibuf->userdata, ibuf->x, ibuf->y);

#ifdef WITH_OPENEXR
	IMB_exr_close(ibuf->userdata);
#endif

	ibuf->userdata= NULL;
	if(ima->rr)
		ima->rr->framenr= framenr;
}

/* common stuff to do with images after loading */
static void image_initialize_after_load(Image *ima, ImBuf *ibuf)
{
	
	
	/* preview is NULL when it has never been used as an icon before */
	if(G.background==0 && ima->preview==NULL)
		BKE_icon_changed(BKE_icon_getid(&ima->id));
	
	/* stringcodes also in ibuf, ibuf->name is used to retrieve original (buttons) */
	BLI_strncpy(ibuf->name, ima->name, FILE_MAX);
	
	/* fields */
	if (ima->flag & IMA_FIELDS) {
		if(ima->flag & IMA_STD_FIELD) de_interlace_st(ibuf);
		else de_interlace_ng(ibuf);
	}
	/* timer */
	ima->lastused = clock() / CLOCKS_PER_SEC;
	
	ima->ok= IMA_OK_LOADED;
	
}

static ImBuf *image_load_sequence_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf;
	unsigned short numlen;
	char name[FILE_MAX], head[FILE_MAX], tail[FILE_MAX];
	
	ima->lastframe= frame;

	BLI_stringdec(ima->name, head, tail, &numlen);
	BLI_stringenc(ima->name, head, tail, numlen, frame);
	BLI_strncpy(name, ima->name, sizeof(name));
	
	if(ima->id.lib)
		BLI_convertstringcode(name, ima->id.lib->filename);
	else
		BLI_convertstringcode(name, G.sce);
	
	/* read ibuf */
	ibuf = IMB_loadiffname(name, IB_rect|IB_multilayer);
	if(G.f & G_DEBUG) printf("loaded %s\n", name);
	
	if (ibuf) {
#ifdef WITH_OPENEXR
		/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_get_ibuf */
		if (ibuf->ftype==OPENEXR && ibuf->userdata) {
			image_create_multilayer(ima, ibuf, frame);	
			ima->type= IMA_TYPE_MULTILAYER;
			IMB_freeImBuf(ibuf);
			ibuf= NULL;
		}
		else {
			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, 0, frame);
		}
#else
		image_initialize_after_load(ima, ibuf);
		image_assign_ibuf(ima, ibuf, 0, frame);
#endif
		
		if(ima->flag & IMA_DO_PREMUL)
			converttopremul(ibuf);
		
	}
	else
		ima->ok= 0;
	
	if(iuser)
		iuser->ok= ima->ok;
	
	return ibuf;
}

static ImBuf *image_load_sequence_multilayer(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf= NULL;
	
	/* either we load from RenderResult, or we have to load a new one */
	
	/* check for new RenderResult */
	if(ima->rr==NULL || frame!=ima->rr->framenr) {
		/* copy to survive not found multilayer image */
		RenderResult *oldrr= ima->rr;
	
		ima->rr= NULL;
		ibuf = image_load_sequence_file(ima, iuser, frame);
		
		if(ibuf) { /* actually an error */
			ima->type= IMA_TYPE_IMAGE;
			printf("error, multi is normal image\n");
		}
		// printf("loaded new result %p\n", ima->rr);
		/* free result if new one found */
		if(ima->rr) {
			// if(oldrr) printf("freed previous result %p\n", oldrr);
			if(oldrr) RE_FreeRenderResult(oldrr);
		}
		else {
			ima->rr= oldrr;
		}

	}
	if(ima->rr) {
		RenderPass *rpass= BKE_image_multilayer_index(ima->rr, iuser);
		
		if(rpass) {
			// printf("load from pass %s\n", rpass->name);
			/* since we free  render results, we copy the rect */
			ibuf= IMB_allocImBuf(ima->rr->rectx, ima->rr->recty, 32, 0, 0);
			ibuf->rect_float= MEM_dupallocN(rpass->rect);
			ibuf->flags |= IB_rectfloat;
			ibuf->mall= IB_rectfloat;
			ibuf->channels= rpass->channels;
			
			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, iuser?iuser->multi_index:0, frame);
			
		}
		// else printf("pass not found\n");
	}
	else
		ima->ok= 0;
	
	if(iuser)
		iuser->ok= ima->ok;
	
	return ibuf;
}


static ImBuf *image_load_movie_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf= NULL;
	
	ima->lastframe= frame;
	
	if(ima->anim==NULL) {
		char str[FILE_MAX];
		
		BLI_strncpy(str, ima->name, FILE_MAX);
		if(ima->id.lib)
			BLI_convertstringcode(str, ima->id.lib->filename);
		else
			BLI_convertstringcode(str, G.sce);
		
		ima->anim = openanim(str, IB_cmap | IB_rect);
		
		/* let's initialize this user */
		if(ima->anim && iuser && iuser->frames==0)
			iuser->frames= IMB_anim_get_duration(ima->anim);
	}
	
	if(ima->anim) {
		int dur = IMB_anim_get_duration(ima->anim);
		int fra= frame-1;
		
		if(fra<0) fra = 0;
		if(fra>(dur-1)) fra= dur-1;
		ibuf = IMB_anim_absolute(ima->anim, fra);
		
		if(ibuf) {
			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, 0, frame);
		}
		else
			ima->ok= 0;
	}
	else
		ima->ok= 0;
	
	if(iuser)
		iuser->ok= ima->ok;
	
	return ibuf;
}

/* cfra used for # code, Image can only have this # for all its users */
static ImBuf *image_load_image_file(Image *ima, ImageUser *iuser, int cfra)
{
	struct ImBuf *ibuf;
	char str[FILE_MAX];
	int assign = 0;
	
	/* always ensure clean ima */
	image_free_buffers(ima);
	
	/* is there a PackedFile with this image ? */
	if (ima->packedfile) {
		ibuf = IMB_ibImageFromMemory((int *) ima->packedfile->data, ima->packedfile->size, IB_rect|IB_multilayer);
	} 
	else {
			
		/* get the right string */
		BLI_strncpy(str, ima->name, sizeof(str));
		if(ima->id.lib)
			BLI_convertstringcode(str, ima->id.lib->filename);
		else
			BLI_convertstringcode(str, G.sce);
		
		BLI_convertstringframe(str, cfra, 0);
		
		/* read ibuf */
		ibuf = IMB_loadiffname(str, IB_rect|IB_multilayer|IB_imginfo);
	}
	
	if (ibuf) {
		/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_get_ibuf */
		if (ibuf->ftype==OPENEXR && ibuf->userdata) {
			image_create_multilayer(ima, ibuf, cfra);	
			ima->type= IMA_TYPE_MULTILAYER;
			IMB_freeImBuf(ibuf);
			ibuf= NULL;
		}
		else {
			image_initialize_after_load(ima, ibuf);
			assign= 1;

			/* check if the image is a font image... */
			detectBitmapFont(ibuf);
			
			/* make packed file for autopack */
			if ((ima->packedfile == NULL) && (G.fileflags & G_AUTOPACK))
				ima->packedfile = newPackedFile(NULL, str);
		}
		
		if(ima->flag & IMA_DO_PREMUL)
			converttopremul(ibuf);
	}
	else
		ima->ok= 0;
	
	if(assign)
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);

	if(iuser)
		iuser->ok= ima->ok;
	
	return ibuf;
}

static ImBuf *image_get_ibuf_multilayer(Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf= NULL;
	
	if(ima->rr==NULL) {
		ibuf = image_load_image_file(ima, iuser, 0);
		if(ibuf) { /* actually an error */
			ima->type= IMA_TYPE_IMAGE;
			return ibuf;
		}
	}
	if(ima->rr) {
		RenderPass *rpass= BKE_image_multilayer_index(ima->rr, iuser);

		if(rpass) {
			ibuf= IMB_allocImBuf(ima->rr->rectx, ima->rr->recty, 32, 0, 0);
			
			image_initialize_after_load(ima, ibuf);
			
			ibuf->rect_float= rpass->rect;
			ibuf->flags |= IB_rectfloat;
			ibuf->channels= rpass->channels;
			ibuf->profile = IB_PROFILE_LINEAR_RGB;

			image_assign_ibuf(ima, ibuf, iuser?iuser->multi_index:IMA_NO_INDEX, 0);
		}
	}
	
	if(ibuf==NULL) 
		ima->ok= 0;
	if(iuser)
		iuser->ok= ima->ok;
	
	return ibuf;
}


/* showing RGBA result itself (from compo/sequence) or
   like exr, using layers etc */
/* always returns a single ibuf, also during render progress */
static ImBuf *image_get_render_result(Image *ima, ImageUser *iuser, void **lock_r)
{
	Render *re= NULL;
	RenderResult *rr= NULL;
	
	/* if we the caller is not going to release the lock, don't give the image */
	if(!lock_r)
		return NULL;

	if(iuser && iuser->scene) {
		re= RE_GetRender(iuser->scene->id.name);
		rr= RE_AcquireResultRead(re);

		/* release is done in BKE_image_release_ibuf using lock_r */
		*lock_r= re;
	}

	if(rr==NULL)
		return NULL;
	
	if(RE_RenderInProgress(re)) {
		ImBuf *ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);
		
		/* make ibuf if needed, and initialize it */
		/* this only gets called when mutex locked */
		if(ibuf==NULL) {
			ibuf= IMB_allocImBuf(rr->rectx, rr->recty, 32, IB_rect, 0);
			image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
		}

		return ibuf;
	}
	else {
		RenderResult rres;
		float *rectf, *rectz;
		unsigned int *rect;
		float dither;
		int channels, layer, pass;

		channels= 4;
		layer= (iuser)? iuser->layer: 0;
		pass= (iuser)? iuser->pass: 0;
		
		/* this gives active layer, composite or seqence result */
		RE_AcquireResultImage(RE_GetRender(iuser->scene->id.name), &rres);
		rect= (unsigned int *)rres.rect32;
		rectf= rres.rectf;
		rectz= rres.rectz;
		dither= iuser->scene->r.dither_intensity;

		/* get compo/seq result by default */
		if(rr->rectf && layer==0);
		else if(rr->layers.first) {
			RenderLayer *rl= BLI_findlink(&rr->layers, layer-(rr->rectf?1:0));
			if(rl) {
				RenderPass *rpass;

				/* there's no combined pass, is in renderlayer itself */
				if(pass==0) {
					rectf= rl->rectf;
				}
				else {
					rpass= BLI_findlink(&rl->passes, pass-1);
					if(rpass) {
						channels= rpass->channels;
						rectf= rpass->rect;
						dither= 0.0f; /* don't dither passes */
					}
				}

				for(rpass= rl->passes.first; rpass; rpass= rpass->next)
					if(rpass->passtype == SCE_PASS_Z)
						rectz= rpass->rect;
			}
		}
		
		if(rectf || rect) {
			ImBuf *ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);

			/* make ibuf if needed, and initialize it */
			if(ibuf==NULL) {
				ibuf= IMB_allocImBuf(rr->rectx, rr->recty, 32, 0, 0);
				image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
			}
			ibuf->x= rr->rectx;
			ibuf->y= rr->recty;
			
			if(ibuf->rect_float!=rectf || rect) /* ensure correct redraw */
				imb_freerectImBuf(ibuf);
			if(rect)
				ibuf->rect= rect;
			
			ibuf->rect_float= rectf;
			ibuf->flags |= IB_rectfloat;
			ibuf->channels= channels;
			ibuf->zbuf_float= rectz;
			ibuf->flags |= IB_zbuffloat;
			ibuf->dither= dither;

			RE_ReleaseResultImage(re);

			ima->ok= IMA_OK_LOADED;
			return ibuf;
		}

		RE_ReleaseResultImage(re);
	}
	
	return NULL;
}

static ImBuf *image_get_ibuf_threadsafe(Image *ima, ImageUser *iuser, int *frame_r, int *index_r)
{
	ImBuf *ibuf = NULL;
	int frame = 0, index = 0;

	/* see if we already have an appropriate ibuf, with image source and type */
	if(ima->source==IMA_SRC_MOVIE) {
		frame= iuser?iuser->framenr:ima->lastframe;
		ibuf= image_get_ibuf(ima, 0, frame);
	}
	else if(ima->source==IMA_SRC_SEQUENCE) {
		if(ima->type==IMA_TYPE_IMAGE) {
			frame= iuser?iuser->framenr:ima->lastframe;
			ibuf= image_get_ibuf(ima, 0, frame);
		}
		else if(ima->type==IMA_TYPE_MULTILAYER) {
			frame= iuser?iuser->framenr:ima->lastframe;
			index= iuser?iuser->multi_index:IMA_NO_INDEX;
			ibuf= image_get_ibuf(ima, index, frame);
		}
	}
	else if(ima->source==IMA_SRC_FILE) {
		if(ima->type==IMA_TYPE_IMAGE)
			ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);
		else if(ima->type==IMA_TYPE_MULTILAYER)
			ibuf= image_get_ibuf(ima, iuser?iuser->multi_index:IMA_NO_INDEX, 0);
	}
	else if(ima->source == IMA_SRC_GENERATED) {
		ibuf= image_get_ibuf(ima, IMA_NO_INDEX, 0);
	}
	else if(ima->source == IMA_SRC_VIEWER) {
		if(ima->type==IMA_TYPE_R_RESULT) {
			/* always verify entirely, not that this shouldn't happen
			 * during render anyway */
		}
		else if(ima->type==IMA_TYPE_COMPOSITE) {
			frame= iuser?iuser->framenr:0;
			ibuf= image_get_ibuf(ima, 0, frame);
		}
	}

	*frame_r = frame;
	*index_r = index;

	return ibuf;
}

/* Checks optional ImageUser and verifies/creates ImBuf. */
/* use this one if you want to get a render result in progress,
 * if not, use BKE_image_get_ibuf which doesn't require a release */
ImBuf *BKE_image_acquire_ibuf(Image *ima, ImageUser *iuser, void **lock_r)
{
	ImBuf *ibuf= NULL;
	float color[] = {0, 0, 0, 1};
	int frame= 0, index= 0;

	/* This function is intended to be thread-safe. It postpones the mutex lock
	 * until it needs to load the image, if the image is already there it
	 * should just get the pointer and return. The reason is that a lot of mutex
	 * locks appears to be very slow on certain multicore macs, causing a render
	 * with image textures to actually slow down as more threads are used.
	 *
	 * Note that all the image loading functions should also make sure they do
	 * things in a threadsafe way for image_get_ibuf_threadsafe to work correct.
	 * That means, the last two steps must be, 1) add the ibuf to the list and
	 * 2) set ima/iuser->ok to 0 to IMA_OK_LOADED */
	
	if(lock_r)
		*lock_r= NULL;

	/* quick reject tests */
	if(ima==NULL) 
		return NULL;
	if(iuser) {
		if(iuser->ok==0)
			return NULL;
	}
	else if(ima->ok==0)
		return NULL;
	
	/* try to get the ibuf without locking */
	ibuf= image_get_ibuf_threadsafe(ima, iuser, &frame, &index);

	if(ibuf == NULL) {
		/* couldn't get ibuf and image is not ok, so let's lock and try to
		 * load the image */
		BLI_lock_thread(LOCK_IMAGE);

		/* need to check ok flag and loading ibuf again, because the situation
		 * might have changed in the meantime */
		if(iuser) {
			if(iuser->ok==0) {
				BLI_unlock_thread(LOCK_IMAGE);
				return NULL;
			}
		}
		else if(ima->ok==0) {
			BLI_unlock_thread(LOCK_IMAGE);
			return NULL;
		}

		ibuf= image_get_ibuf_threadsafe(ima, iuser, &frame, &index);

		if(ibuf == NULL) {
			/* we are sure we have to load the ibuf, using source and type */
			if(ima->source==IMA_SRC_MOVIE) {
				/* source is from single file, use flipbook to store ibuf */
				ibuf= image_load_movie_file(ima, iuser, frame);
			}
			else if(ima->source==IMA_SRC_SEQUENCE) {
				if(ima->type==IMA_TYPE_IMAGE) {
					/* regular files, ibufs in flipbook, allows saving */
					ibuf= image_load_sequence_file(ima, iuser, frame);
				}
				/* no else; on load the ima type can change */
				if(ima->type==IMA_TYPE_MULTILAYER) {
					/* only 1 layer/pass stored in imbufs, no exrhandle anim storage, no saving */
					ibuf= image_load_sequence_multilayer(ima, iuser, frame);
				}

				if(ibuf)
					BLI_strncpy(ima->name, ibuf->name, sizeof(ima->name));
			}
			else if(ima->source==IMA_SRC_FILE) {
				
				if(ima->type==IMA_TYPE_IMAGE)
					ibuf= image_load_image_file(ima, iuser, frame);	/* cfra only for '#', this global is OK */
				/* no else; on load the ima type can change */
				if(ima->type==IMA_TYPE_MULTILAYER)
					/* keeps render result, stores ibufs in listbase, allows saving */
					ibuf= image_get_ibuf_multilayer(ima, iuser);
					
			}
			else if(ima->source == IMA_SRC_GENERATED) {
				/* generated is: ibuf is allocated dynamically */
				/* UV testgrid or black or solid etc */
				if(ima->gen_x==0) ima->gen_x= 1024;
				if(ima->gen_y==0) ima->gen_y= 1024;
				ibuf= add_ibuf_size(ima->gen_x, ima->gen_y, ima->name, 0, ima->gen_type, color);
				image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
				ima->ok= IMA_OK_LOADED;
			}
			else if(ima->source == IMA_SRC_VIEWER) {
				if(ima->type==IMA_TYPE_R_RESULT) {
					/* always verify entirely, and potentially
					   returns pointer to release later */
					ibuf= image_get_render_result(ima, iuser, lock_r);
				}
				else if(ima->type==IMA_TYPE_COMPOSITE) {
					/* Composite Viewer, all handled in compositor */
					/* fake ibuf, will be filled in compositor */
					ibuf= IMB_allocImBuf(256, 256, 32, IB_rect, 0);
					image_assign_ibuf(ima, ibuf, 0, frame);
				}
			}
		}

		BLI_unlock_thread(LOCK_IMAGE);
	}

	/* we assuming that if it is not rendering, it's also not multithreaded
	 * (a somewhat weak assumption) */
	if(G.rendering==0)
		tag_image_time(ima);

	return ibuf;
}

void BKE_image_release_ibuf(Image *ima, void *lock)
{
	/* for getting image during threaded render, need to release */
	if(lock)
		RE_ReleaseResult(lock);
}

ImBuf *BKE_image_get_ibuf(Image *ima, ImageUser *iuser)
{
	return BKE_image_acquire_ibuf(ima, iuser, NULL);
}

void BKE_image_user_calc_frame(ImageUser *iuser, int cfra, int fieldnr)
{
	int imanr, len;
	
	/* here (+fie_ima/2-1) makes sure that division happens correctly */
	len= (iuser->fie_ima*iuser->frames)/2;
	
	if(len==0) {
		iuser->framenr= 0;
	}
	else {
		cfra= cfra - iuser->sfra+1;
		
		/* cyclic */
		if(iuser->cycl) {
			cfra= ( (cfra) % len );
			if(cfra < 0) cfra+= len;
			if(cfra==0) cfra= len;
		}
		
		if(cfra<1) cfra= 1;
		else if(cfra>len) cfra= len;
		
		/* convert current frame to current field */
		cfra= 2*(cfra);
		if(fieldnr) cfra++;
		
		/* transform to images space */
		imanr= (cfra+iuser->fie_ima-2)/iuser->fie_ima;
		if(imanr>iuser->frames) imanr= iuser->frames;
		imanr+= iuser->offset;
		
		if(iuser->cycl) {
			imanr= ( (imanr) % len );
			while(imanr < 0) imanr+= len;
			if(imanr==0) imanr= len;
		}
	
		/* allows image users to handle redraws */
		if(iuser->flag & IMA_ANIM_ALWAYS)
			if(imanr!=iuser->framenr)
				iuser->flag |= IMA_ANIM_REFRESHED;
		
		iuser->framenr= imanr;
		if(iuser->ok==0) iuser->ok= 1;
	}
}

/*
  Produce image export path.

  Fails returning 0 if image filename is empty or if destination path
  matches image path (i.e. both are the same file).

  Trailing slash in dest_dir is optional.

  Logic:

  - if an image is "below" current .blend file directory, rebuild the
    same dir structure in dest_dir

  For example //textures/foo/bar.png becomes
  [dest_dir]/textures/foo/bar.png.

  - if an image is not "below" current .blend file directory,
  disregard it's path and copy it in the same directory where 3D file
  goes.

  For example //../foo/bar.png becomes [dest_dir]/bar.png.

  This logic will help ensure that all image paths are relative and
  that a user gets his images in one place. It'll also provide
  consistent behaviour across exporters.
 */
int BKE_get_image_export_path(struct Image *im, const char *dest_dir, char *abs, int abs_size, char *rel, int rel_size)
{
	char path[FILE_MAX];
	char dir[FILE_MAX];
	char base[FILE_MAX];
	char blend_dir[FILE_MAX];	/* directory, where current .blend file resides */
	char dest_path[FILE_MAX];
	char rel_dir[FILE_MAX];
	int len;

	if (abs)
		abs[0]= 0;

	if (rel)
		rel[0]= 0;

	BLI_split_dirfile_basic(G.sce, blend_dir, NULL);

	if (!strlen(im->name)) {
		if (G.f & G_DEBUG) printf("Invalid image type.\n");
		return 0;
	}

	BLI_strncpy(path, im->name, sizeof(path));

	/* expand "//" in filename and get absolute path */
	BLI_convertstringcode(path, G.sce);

	/* get the directory part */
	BLI_split_dirfile_basic(path, dir, base);

	len= strlen(blend_dir);

	rel_dir[0] = 0;

	/* if image is "below" current .blend file directory */
	if (!strncmp(path, blend_dir, len)) {

		/* if image is _in_ current .blend file directory */
		if (!strcmp(dir, blend_dir)) {
			BLI_join_dirfile(dest_path, dest_dir, base);
		}
		/* "below" */
		else {
			/* rel = image_path_dir - blend_dir */
			BLI_strncpy(rel_dir, dir + len, sizeof(rel_dir));

			BLI_join_dirfile(dest_path, dest_dir, rel_dir);
			BLI_join_dirfile(dest_path, dest_path, base);
		}
			
	}
	/* image is out of current directory */
	else {
		BLI_join_dirfile(dest_path, dest_dir, base);
	}

	if (abs)
		BLI_strncpy(abs, dest_path, abs_size);

	if (rel) {
		strncat(rel, rel_dir, rel_size);
		strncat(rel, base, rel_size);
	}

	/* return 2 if src=dest */
	if (!strcmp(path, dest_path)) {
		if (G.f & G_DEBUG) printf("%s and %s are the same file\n", path, dest_path);
		return 2;
	}

	return 1;
}

