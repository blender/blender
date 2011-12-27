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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/quicktime/quicktime_export.h
 *  \ingroup quicktime
 */


#ifndef __QUICKTIME_EXPORT_H__
#define __QUICKTIME_EXPORT_H__

#if defined (_WIN32) || (__APPLE__)

#define __AIFF__


#define QTAUDIO_FLAG_RESAMPLE_NOHQ 1
#define QTAUDIO_FLAG_CODEC_ISCBR 2


/*Codec list*/
typedef struct QuicktimeCodecTypeDesc {
	int codecType;
	int rnatmpvalue;
	char * codecName;
} QuicktimeCodecTypeDesc ;

// quicktime movie output functions
struct RenderData;
struct Scene;
struct wmOperatorType;
struct ReportList;

int start_qt(struct Scene *scene, struct RenderData *rd, int rectx, int recty, struct ReportList *reports);	//for movie handle (BKE writeavi.c now)
int append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty, struct ReportList *reports);
void end_qt(void);
void filepath_qt(char *string, struct RenderData *rd);

/*RNA helper functions */
void quicktime_verify_image_type(struct RenderData *rd, struct ImageFormatData *imf); //used by RNA for defaults values init, if needed
/*Video codec type*/
int quicktime_get_num_videocodecs(void);
QuicktimeCodecTypeDesc* quicktime_get_videocodecType_desc(int indexValue);
int quicktime_rnatmpvalue_from_videocodectype(int codecType);
int quicktime_videocodecType_from_rnatmpvalue(int rnatmpvalue);

#ifdef USE_QTKIT
/*Audio codec type*/
int quicktime_get_num_audiocodecs(void);
QuicktimeCodecTypeDesc* quicktime_get_audiocodecType_desc(int indexValue);
int quicktime_rnatmpvalue_from_audiocodectype(int codecType);
int quicktime_audiocodecType_from_rnatmpvalue(int rnatmpvalue);
#endif

#ifndef USE_QTKIT
void SCENE_OT_render_data_set_quicktime_codec(struct wmOperatorType *ot); //Operator to raise quicktime standard dialog to request codec settings
#endif


void free_qtcomponentdata(void);
void makeqtstring(struct RenderData *rd, char *string);		//for playanim.c



#if (defined(USE_QTKIT) && defined(MAC_OS_X_VERSION_10_6) && __LP64__)
//Include the quicktime codec types constants that are missing in QTKitDefines.h in 10.6 / 64bit
enum {
	kRawCodecType						= 'raw ',
	kCinepakCodecType 					= 'cvid',
	kGraphicsCodecType					= 'smc ',
	kAnimationCodecType 				= 'rle ',
	kVideoCodecType 					= 'rpza',
	kComponentVideoCodecType			= 'yuv2',
	kJPEGCodecType						= 'jpeg',
	kMotionJPEGACodecType 				= 'mjpa',
	kMotionJPEGBCodecType 				= 'mjpb',
	kSGICodecType 						= '.SGI',
	kPlanarRGBCodecType 				= '8BPS',
	kMacPaintCodecType					= 'PNTG',
	kGIFCodecType 						= 'gif ',
	kPhotoCDCodecType 					= 'kpcd',
	kQuickDrawGXCodecType 				= 'qdgx',
	kAVRJPEGCodecType 					= 'avr ',
	kOpenDMLJPEGCodecType 				= 'dmb1',
	kBMPCodecType 						= 'WRLE',
	kWindowsRawCodecType				= 'WRAW',
	kVectorCodecType					= 'path',
	kQuickDrawCodecType 				= 'qdrw',
	kWaterRippleCodecType 				= 'ripl',
	kFireCodecType						= 'fire',
	kCloudCodecType 					= 'clou',
	kH261CodecType						= 'h261',
	kH263CodecType						= 'h263',
	kDVCNTSCCodecType					= 'dvc ',	/* DV - NTSC and DVCPRO NTSC (available in QuickTime 6.0 or later)*/
	/* NOTE: kDVCProNTSCCodecType is deprecated.	*/
	/* Use kDVCNTSCCodecType instead -- as far as the codecs are concerned, */
	/* the two data formats are identical.*/
	kDVCPALCodecType					= 'dvcp',
	kDVCProPALCodecType 				= 'dvpp',	/* available in QuickTime 6.0 or later*/
	kDVCPro50NTSCCodecType				= 'dv5n',
	kDVCPro50PALCodecType 				= 'dv5p',
	kDVCPro100NTSCCodecType 			= 'dv1n',
	kDVCPro100PALCodecType				= 'dv1p',
	kDVCPROHD720pCodecType				= 'dvhp',
	kDVCPROHD1080i60CodecType			= 'dvh6',
	kDVCPROHD1080i50CodecType			= 'dvh5',
	kBaseCodecType						= 'base',
	kFLCCodecType 						= 'flic',
	kTargaCodecType 					= 'tga ',
	kPNGCodecType 						= 'png ',
	kTIFFCodecType						= 'tiff',	/* NOTE: despite what might seem obvious from the two constants*/
	/* below and their names, they really are correct. 'yuvu' really */
	/* does mean signed, and 'yuvs' really does mean unsigned. Really. */
	kComponentVideoSigned 				= 'yuvu',
	kComponentVideoUnsigned 			= 'yuvs',
	kCMYKCodecType						= 'cmyk',
	kMicrosoftVideo1CodecType			= 'msvc',
	kSorensonCodecType					= 'SVQ1',
	kSorenson3CodecType 				= 'SVQ3',	/* available in QuickTime 5 and later*/
	kIndeo4CodecType					= 'IV41',
	kMPEG4VisualCodecType 				= 'mp4v',
	k64ARGBCodecType					= 'b64a',
	k48RGBCodecType 					= 'b48r',
	k32AlphaGrayCodecType 				= 'b32a',
	k16GrayCodecType					= 'b16g',
	kMpegYUV420CodecType				= 'myuv',
	kYUV420CodecType					= 'y420',
	kSorensonYUV9CodecType				= 'syv9',
	k422YpCbCr8CodecType				= '2vuy',	/* Component Y'CbCr 8-bit 4:2:2	*/
	k444YpCbCr8CodecType				= 'v308',	/* Component Y'CbCr 8-bit 4:4:4	*/
	k4444YpCbCrA8CodecType				= 'v408',	/* Component Y'CbCrA 8-bit 4:4:4:4 */
	k422YpCbCr16CodecType 				= 'v216',	/* Component Y'CbCr 10,12,14,16-bit 4:2:2*/
	k422YpCbCr10CodecType 				= 'v210',	/* Component Y'CbCr 10-bit 4:2:2 */
	k444YpCbCr10CodecType 				= 'v410',	/* Component Y'CbCr 10-bit 4:4:4 */
	k4444YpCbCrA8RCodecType 			= 'r408',	/* Component Y'CbCrA 8-bit 4:4:4:4, rendering format. full range alpha, zero biased yuv*/
	kJPEG2000CodecType					= 'mjp2',
	kPixletCodecType					= 'pxlt',
	kH264CodecType						= 'avc1'
};
#endif

#endif //(_WIN32) || (__APPLE__)

#endif  // __QUICKTIME_IMP_H__
