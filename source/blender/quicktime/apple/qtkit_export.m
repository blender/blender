/**
 * $Id$
 *
 * qtkit_export.m
 *
 * Code to create QuickTime Movies with Blender
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is written by Rob Haarsma (phase)
 *
 * Contributor(s): Stefan Gartner (sgefant)
 *				   Damien Plisson 11/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)

#include <stdio.h>
#include <string.h>

#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_scene.h"

#include "BLI_blenlib.h"

#include "BLO_sys_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

#include "quicktime_import.h"
#include "quicktime_export.h"


#ifdef __APPLE__
/* evil */
#ifndef __AIFF__
#define __AIFF__
#endif
#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>
#endif /* __APPLE__ */

typedef struct QuicktimeExport {
	QTMovie *movie;
	
	NSString *filename;

	QTTime frameDuration;
	NSDictionary *frameAttributes;
} QuicktimeExport;

static struct QuicktimeExport *qtexport;


static NSString *stringWithCodecType(int codecType) {
	switch (codecType) {
		case QT_CODECTYPE_RAW:
			return @"raw ";
		case QT_CODECTYPE_MJPEGA:
			return @"mjpa";
		case QT_CODECTYPE_MJPEGB:
			return @"mjpb";
		case QT_CODECTYPE_DVCPAL:
			return @"dvcp";
		case QT_CODECTYPE_DVCNTSC:
			return @"dvc ";
		case QT_CODECTYPE_MPEG4:
			return @"mp4v";
		case QT_CODECTYPE_H263:
			return @"h263";
		case QT_CODECTYPE_H264:
			return @"avc1";
		case QT_CODECTYPE_DVCPROHD720p:
			return @"dvhp";
		case QT_CODECTYPE_DVCPROHD1080i50:
			return @"dvh5";
		case QT_CODECTYPE_DVCPROHD1080i60:
			return @"dvh6";
			
		case QT_CODECTYPE_JPEG:
		default:
			return @"jpeg";	
	}
}

void makeqtstring (RenderData *rd, char *string) {
	char txt[64];

	strcpy(string, rd->pic);
	BLI_convertstringcode(string, G.sce);

	BLI_make_existing_file(string);

	if (BLI_strcasecmp(string + strlen(string) - 4, ".mov")) {
		sprintf(txt, "%04d_%04d.mov", (rd->sfra) , (rd->efra) );
		strcat(string, txt);
	}
}


void start_qt(struct Scene *scene, struct RenderData *rd, int rectx, int recty)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSError *error;
	char name[2048];

	
	if(qtexport == NULL) qtexport = MEM_callocN(sizeof(QuicktimeExport), "QuicktimeExport");

	if (G.afbreek != 1) {
		/* Check first if the QuickTime 7.2.1 initToWritableFile: method is available */
		if ([[[[QTMovie alloc] init] autorelease] respondsToSelector:@selector(initToWritableFile:error:)] != YES) {
			G.afbreek = 1;
			fprintf(stderr, "\nUnable to create quicktime movie, need Quicktime rev 7.2.1 or later");
		}
		else {
			makeqtstring(rd, name);
			qtexport->filename = [NSString stringWithCString:name
									  encoding:[NSString defaultCStringEncoding]];
			qtexport->movie = [[QTMovie alloc] initToWritableFile:qtexport->filename error:&error];
				
			if(qtexport->movie == nil) {
				G.afbreek = 1;
				NSLog(@"Unable to create quicktime movie : %@",[error localizedDescription]);
			} else {
				[qtexport->movie retain];
				[qtexport->filename retain];
				[qtexport->movie setAttribute:[NSNumber numberWithBool:YES] forKey:QTMovieEditableAttribute];
				[qtexport->movie setAttribute:@"Made with Blender" forKey:QTMovieCopyrightAttribute];
				
				qtexport->frameDuration = QTMakeTime(rd->frs_sec_base*1000, rd->frs_sec*1000);
				
				/* specifying the codec attributes : try to retrieve them from render data first*/
				if (rd->qtcodecsettings.codecType) {
					qtexport->frameAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
												 stringWithCodecType(rd->qtcodecsettings.codecType),
												 QTAddImageCodecType,
												 [NSNumber numberWithLong:((rd->qtcodecsettings.codecSpatialQuality)*codecLosslessQuality)/100],
												 QTAddImageCodecQuality,
												 nil];
				}
				else {
					qtexport->frameAttributes = [NSDictionary dictionaryWithObjectsAndKeys:@"jpeg",
												 QTAddImageCodecType,
												 [NSNumber numberWithLong:codecHighQuality],
												 QTAddImageCodecQuality,
												 nil];
				}
				[qtexport->frameAttributes retain];
			}
		}
	}
	
	[pool drain];
}


void append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSBitmapImageRep *blBitmapFormatImage;
	NSImage *frameImage;
	unsigned char *from_Ptr,*to_Ptr;
	int y,from_i,to_i;
	
	
	/* Create bitmap image rep in blender format (32bit RGBA) */
	blBitmapFormatImage = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																  pixelsWide:rectx 
																  pixelsHigh:recty
															   bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
															  colorSpaceName:NSCalibratedRGBColorSpace 
																bitmapFormat:NSAlphaNonpremultipliedBitmapFormat
																 bytesPerRow:rectx*4
																bitsPerPixel:32];
	if (!blBitmapFormatImage) {
		[pool drain];
		return;
	}
	
	from_Ptr = (unsigned char*)pixels;
	to_Ptr = (unsigned char*)[blBitmapFormatImage bitmapData];
	for (y = 0; y < recty; y++) {
		to_i = (recty-y-1)*rectx;
		from_i = y*rectx;
		memcpy(to_Ptr+4*to_i, from_Ptr+4*from_i, 4*rectx);
	}
	
	frameImage = [[NSImage alloc] initWithSize:NSMakeSize(rectx, recty)];
	[frameImage addRepresentation:blBitmapFormatImage];
	
	/* Add the image to the movie clip */
	[qtexport->movie addImage:frameImage
				  forDuration:qtexport->frameDuration
			   withAttributes:qtexport->frameAttributes];

	[blBitmapFormatImage release];
	[frameImage release];
	[pool drain];	
}


void end_qt(void)
{
	if (qtexport->movie) {
		/* Flush update of the movie file */
		[qtexport->movie updateMovieFile];
		
		[qtexport->movie invalidate];
		
		/* Clean up movie structure */
		[qtexport->filename release];
		[qtexport->frameAttributes release];
		[qtexport->movie release];
		}

	if(qtexport) {
		MEM_freeN(qtexport);
		qtexport = NULL;
	}
}


void free_qtcomponentdata(void) {
}

void quicktime_verify_image_type(RenderData *rd)
{
	if (rd->imtype == R_QUICKTIME) {
		if ((rd->qtcodecsettings.codecType<= 0) ||
			(rd->qtcodecsettings.codecSpatialQuality <0) ||
			(rd->qtcodecsettings.codecSpatialQuality > 100)) {
			
			rd->qtcodecsettings.codecType = QT_CODECTYPE_JPEG;
			rd->qtcodecsettings.codecSpatialQuality = (codecHighQuality*100)/codecLosslessQuality;
		}
	}
}

#endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

