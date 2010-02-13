/**
 * @file IMB_imbuf.h
 * @brief IMage Buffer module.
 *
 * This module offers import/export of several graphical file formats.
 * \ref IMB
 * @ingroup imbuf
 * @ingroup undoc
 *
 * @page IMB - Imbuf module external interface
 *
 *
 * @section about About the IMB module
 *
 * External interface of the IMage Buffer module. This module offers
 * import/export of several graphical file formats. It offers the
 * ImBuf type as a common structure to refer to different graphical
 * file formats, and to enable a uniform way of handling them.
 *
 * @section issues Known issues with IMB
 *
 * - imbuf is written in C.
 * - Endianness issues are dealt with internally.
 * - File I/O must be done externally. The module uses FILE*'s to
 *   direct input/output.
 * - Platform dependency is limited. Some minor patches for
 *   amiga and Irix are present. A 'posix-compliancy-patch'
 *   provides the interface to windows.
 *
 * @section dependencies Dependencies
 *
 * IMB needs:
 * - SDNA module
 *     The listbase types are used for handling the memory
 *     management.
 * - blenlib module
 *     blenlib handles guarded memory management in blender-style.
 *     BLI_winstuff.h makes a few windows specific behaviours
 *     posix-compliant.
 * - avi
 *     avi defines import/export to the avi format. Only anim.c
 *     needs this. It uses the following functions:
 *       - avi_close
 *       - avi_is_avi
 *       - avi_print_error
 *       - avi_open_movie
 *       - avi_read_frame
 *       - avi_get_stream
 *     Additionally, it needs the types from the avi module.
 * - external jpeg library
 *     The jpeg lib defines import/export to the jpeg format.
 *     only jpeg.c needs these. Used functions are:
 *       - jpeg_destroy
 *       - jpeg_resync_to_restart
 *       - jpeg_set_marker_processor
 *       - jpeg_read_header
 *       - jpeg_start_decompress
 *       - jpeg_abort_decompress
 *       - jpeg_read_scanlines
 *       - jpeg_finish_decompress
 *       - jpeg_std_error
 *       - jpeg_create_decompress
 *       - jpeg_stdio_src
 *       - jpeg_start_compress
 *       - jpeg_write_marker
 *       - jpeg_write_scanlines
 *       - jpeg_finish_compress
 *       - jpeg_create_compress
 *       - jpeg_stdio_dest
 *       - jpeg_set_defaults
 *       - jpeg_set_quality
 *       - jpeg_destroy_compress
 *     Additionally, it needs the types from the jpeg lib.
 */
/*
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
#ifndef IMB_IMBUF_H
#define IMB_IMBUF_H

/**
 *
 * @attention defined in ???
 */
struct ImBuf;

/**
 *
 * @attention defined in ???
 */
struct anim;

/**
 *
 * @attention Defined in cmap.c
 */
void IMB_freeImBufdata(void);

/**
 *
 * @attention Defined in cmap.c
 */
void IMB_applycmap(struct ImBuf *ibuf);

/**
 *
 * @attention Defined in cmap.c
 */
short IMB_converttocmap(struct ImBuf *ibuf);

/**
 *
 * @attention Defined in cmap.c
 */
int IMB_alpha_to_col0(int value);

/**
 *
 * @attention Defined in readimage.c
 */
struct ImBuf *IMB_ibImageFromMemory(int *mem, int size, int flags);

/**
 *
 * @attention Defined in readimage.c
 */
struct ImBuf *IMB_testiffname(char *naam,int flags);

/**
 *
 * @attention Defined in readimage.c
 */
struct ImBuf *IMB_loadiffname(const char *naam, int flags);

/**
 *
 * @attention Defined in allocimbuf.c
 */
void IMB_freeImBuf(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_allocImBuf(short x, short y,
						 unsigned char d, unsigned int flags,
						 unsigned char bitmap);

/**
 *
 * Increase reference count to imbuf
 * (to delete an imbuf you have to call freeImBuf as many times as it
 * is referenced)
 *
 * @attention Defined in allocimbuf.c
 */

void IMB_refImBuf(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in allocimbuf.c
 */
void IMB_cache_limiter_insert(struct ImBuf * i);
void IMB_cache_limiter_unmanage(struct ImBuf * i);
void IMB_cache_limiter_touch(struct ImBuf * i);
void IMB_cache_limiter_ref(struct ImBuf * i);
void IMB_cache_limiter_unref(struct ImBuf * i);
int IMB_cache_limiter_get_refcount(struct ImBuf * i);

/**
 *
 * @attention Defined in allocimbuf.c
 */
struct ImBuf *IMB_dupImBuf(struct ImBuf *ibuf1);

/**
 *
 * @attention Defined in allocimbuf.c
 */
short addzbufImBuf(struct ImBuf * ibuf);
short addzbuffloatImBuf(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in allocimbuf.c
 */
void IMB_freecmapImBuf(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in rectop.c
 */

typedef enum IMB_BlendMode {
	IMB_BLEND_MIX = 0,
	IMB_BLEND_ADD = 1,
	IMB_BLEND_SUB = 2,
	IMB_BLEND_MUL = 3,
	IMB_BLEND_LIGHTEN = 4,
	IMB_BLEND_DARKEN = 5,
	IMB_BLEND_ERASE_ALPHA = 6,
	IMB_BLEND_ADD_ALPHA = 7,

	IMB_BLEND_COPY = 1000,
	IMB_BLEND_COPY_RGB = 1001,
	IMB_BLEND_COPY_ALPHA = 1002
} IMB_BlendMode;

unsigned int IMB_blend_color(unsigned int src1, unsigned int src2, int fac,
	IMB_BlendMode mode);
void IMB_blend_color_float(float *dst, float *src1, float *src2, float fac,
	IMB_BlendMode mode);

void IMB_rectclip(struct ImBuf *dbuf, struct ImBuf *sbuf, int *destx, 
	int *desty, int *srcx, int *srcy, int *width, int *height);
void IMB_rectcpy(struct ImBuf *drect, struct ImBuf *srect, int destx,
	int desty, int srcx, int srcy, int width, int height);
void IMB_rectblend(struct ImBuf *dbuf, struct ImBuf *sbuf, int destx, 
	int desty, int srcx, int srcy, int width, int height, IMB_BlendMode mode);
void IMB_rectblend_torus(struct ImBuf *dbuf, struct ImBuf *sbuf, int destx, 
	int desty, int srcx, int srcy, int width, int height, IMB_BlendMode mode);

/**
 * Return the length (in frames) of the given @a anim.
 */
int IMB_anim_get_duration(struct anim *anim);

/**
 *
 * @attention Defined in anim.c
 */
struct anim * IMB_open_anim(const char * name, int ib_flags);
void IMB_close_anim(struct anim * anim);

/**
 *
 * @attention Defined in anim.c
 */

int ismovie(char *name);
void IMB_anim_set_preseek(struct anim * anim, int preseek);
int IMB_anim_get_preseek(struct anim * anim);

/**
 *
 * @attention Defined in anim.c
 */

struct ImBuf * IMB_anim_absolute(struct anim * anim, int position);

/**
 *
 * @attention Defined in anim.c
 * fetches a define previewframe, usually half way into the movie
 */
struct ImBuf * IMB_anim_previewframe(struct anim * anim);

/**
 *
 * @attention Defined in anim.c
 */
void IMB_free_anim_ibuf(struct anim * anim);

/**
 *
 * @attention Defined in anim.c
 */
void IMB_free_anim(struct anim * anim);

/**
 *
 * @attention Defined in anim.c
 */
struct ImBuf * IMB_anim_nextpic(struct anim * anim);     


/**
 *
 * @attention Defined in antialias.c
 */
void IMB_clever_double (struct ImBuf * ibuf);

/**
 *
 * @attention Defined in antialias.c
 */
void IMB_antialias(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in filter.c
 */
void IMB_filter(struct ImBuf *ibuf);
void IMB_filterN(struct ImBuf *out, struct ImBuf *in);
void IMB_filter_extend(struct ImBuf *ibuf, char *mask);
void IMB_makemipmap(struct ImBuf *ibuf, int use_filter, int SAT);

/**
 *
 * @attention Defined in filter.c
 */
void IMB_filtery(struct ImBuf *ibuf);

/**
 *
 * @attention Defined in scaling.c
 */
struct ImBuf *IMB_onehalf(struct ImBuf *ibuf1);

/**
 *
 * @attention Defined in scaling.c
 */
struct ImBuf *IMB_scaleImBuf(struct ImBuf * ibuf, short newx, short newy);

/**
 *
 * @attention Defined in scaling.c
 */
struct ImBuf *IMB_scalefieldImBuf(struct ImBuf *ibuf, short newx, short newy);

/**
 *
 * @attention Defined in scaling.c
 */
struct ImBuf *IMB_scalefastImBuf(struct ImBuf *ibuf, short newx, short newy);

/**
 *
 * @attention Defined in writeimage.c
 */
short IMB_saveiff(struct ImBuf *ibuf,char *naam,int flags);

/**
 * Encodes a png image from an ImBuf
 *
 * @attention Defined in png_encode.c
 */
short IMB_png_encode(struct ImBuf *ibuf, int file, int flags);

/**
 *
 * @attention Defined in util.c
 */
int IMB_ispic(char *name);

/**
 *
 * @attention Defined in util.c
 */
int IMB_isanim(char * name);

/**
 *
 * @attention Defined in util.c
 */
int imb_get_anim_type(char * name);

/**
 *
 * @attention Defined in divers.c
 */
void IMB_de_interlace(struct ImBuf *ibuf);
void IMB_interlace(struct ImBuf *ibuf);
void IMB_gamwarp(struct ImBuf *ibuf, double gamma);
void IMB_rect_from_float(struct ImBuf *ibuf);
void IMB_float_from_rect(struct ImBuf *ibuf);

/**
 * Change the ordering of the color bytes pointed to by rect from
 * rgba to abgr. size * 4 color bytes are reordered.
 *
 * @attention Defined in imageprocess.c
 */
void IMB_convert_rgba_to_abgr(struct ImBuf *ibuf);
/**
 *
 * @attention defined in imageprocess.c
 */
void bicubic_interpolation(struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void neareast_interpolation(struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);
void bilinear_interpolation(struct ImBuf *in, struct ImBuf *out, float u, float v, int xout, int yout);

void bicubic_interpolation_color(struct ImBuf *in, unsigned char *col, float *col_float, float u, float v);
void neareast_interpolation_color(struct ImBuf *in, unsigned char *col, float *col_float, float u, float v);
void bilinear_interpolation_color(struct ImBuf *in, unsigned char *col, float *col_float, float u, float v);
void bilinear_interpolation_color_wrap(struct ImBuf *in, unsigned char *col, float *col_float, float u, float v);

/**
 * Change the ordering of the color bytes pointed to by rect from
 * rgba to abgr. size * 4 color bytes are reordered.
 *
 * @attention Defined in imageprocess.c
 */
void IMB_convert_bgra_to_rgba(int size, unsigned int *rect);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_scalefastfieldImBuf(struct ImBuf *ibuf,
									  short newx,
									  short newy);

/**
 *
 * @attention defined in readimage.c
 * @deprecated Only here for backwards compatibility of the
 * @deprecated plugin system.
 */
struct ImBuf *IMB_loadiffmem(int *mem, int flags);

/**
 *
 * @attention defined in readimage.c
 * @deprecated Only here for backwards compatibility of the
 * @deprecated plugin system.
 */  
struct ImBuf *IMB_loadifffile(int file, int flags);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_half_x(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_x(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_double_x(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_half_y(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_double_fast_y(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_double_y(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_onethird(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in scaling.c
 */
struct ImBuf *IMB_halflace(struct ImBuf *ibuf1);

/**
 *
 * @attention defined in dither.c
 */
void IMB_dit2(struct ImBuf * ibuf, short ofs, short bits);

/**
 *
 * @attention defined in dither.c
 */
void IMB_dit0(struct ImBuf * ibuf, short ofs, short bits);

/** Externally used vars: fortunately they do not use funny types */

/**
 * boolean toggle that tells whether or not to
 * scale the color map in the y-direction.
 *
 * @attention declared in hamx.c
 */
extern int scalecmapY;

/**
 * This 'matrix' defines the transformation from rgb to bw color
 * maps. You need to do a sort of dot-product for that. It is a matrix
 * with fixed coefficients, extracted from some book.
 *
 * @attention Defined in matrix.h, only included in hamx.c
 */
extern float rgb_to_bw[4][4]; 

/**
 *
 * @attention Defined in rotate.c
 */
void IMB_flipx(struct ImBuf *ibuf);
void IMB_flipy(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in cspace.c
 */
void IMB_cspace(struct ImBuf *ibuf, float mat[][4]);

/**
 *
 * @attention Defined in allocimbuf.c
 */
void IMB_freezbufImBuf(struct ImBuf * ibuf);
void IMB_freezbuffloatImBuf(struct ImBuf * ibuf);

/**
 *
 * @attention Defined in rectop.c
 */
void IMB_rectfill(struct ImBuf *drect, float col[4]);
void IMB_rectfill_area(struct ImBuf *ibuf, float *col, int x1, int y1, int x2, int y2);

/* this should not be here, really, we needed it for operating on render data, IMB_rectfill_area calls it */
void buf_rectfill_area(unsigned char *rect, float *rectf, int width, int height, float *col, int x1, int y1, int x2, int y2);

/* defined in imginfo.c */
int IMB_imginfo_change_field(struct ImBuf *img, const char *key, const char *field);

/* exported for image tools in blender, to quickly allocate 32 bits rect */
short imb_addrectImBuf(struct ImBuf * ibuf);
void imb_freerectImBuf(struct ImBuf * ibuf);

short imb_addrectfloatImBuf(struct ImBuf * ibuf);
void imb_freerectfloatImBuf(struct ImBuf * ibuf);
void imb_freemipmapImBuf(struct ImBuf * ibuf);

#ifdef WITH_QUICKTIME
/**
 *
 * @attention Defined in quicktime_import.c
 */
void quicktime_init(void);

/**
 *
 * @attention Defined in quicktime_import.c
 */
void quicktime_exit(void);

#endif //WITH_QUICKTIME

/* intern/dynlibtiff.c */
void libtiff_init(void);
void libtiff_exit(void);

#endif
