/**
 * $Id: qtkit_import.m 19323 2009-03-17 21:44:58Z blendix $
 *
 * qtkit_import.m
 *
 * Code to use Quicktime to load images/movies as texture.
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

#include "IMB_anim.h"
#include "BLO_sys_types.h"
#include "BKE_global.h"
#include "BLI_dynstr.h"

#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>

#include "quicktime_import.h"
#include "quicktime_export.h"

// quicktime structure definition
// this structure is part of the anim struct

typedef struct _QuicktimeMovie {
	QTMovie *movie;
	QTMedia *media;
	
	long durationTime;
	long durationScale;
	long framecount;
	

	ImBuf *ibuf;
	
	long previousPosition;
	
} QuicktimeMovie;


#define QTIME_DEBUG 0


void quicktime_init(void)
{
		G.have_quicktime = TRUE;
}


void quicktime_exit(void)
{
	if(G.have_quicktime) {
		free_qtcomponentdata();
	}
}


int anim_is_quicktime (char *name)
{
	NSAutoreleasePool *pool;
	
	// dont let quicktime movie import handle these
	if( BLI_testextensie(name, ".swf") ||
		BLI_testextensie(name, ".txt") ||
		BLI_testextensie(name, ".mpg") ||
		BLI_testextensie(name, ".avi") ||	// wouldnt be appropriate ;)
		BLI_testextensie(name, ".tga") ||
		BLI_testextensie(name, ".png") ||
		BLI_testextensie(name, ".bmp") ||
		BLI_testextensie(name, ".jpg") ||
		BLI_testextensie(name, ".wav") ||
		BLI_testextensie(name, ".zip") ||
		BLI_testextensie(name, ".mp3")) return 0;

	if(QTIME_DEBUG) printf("qt: checking as movie: %s\n", name);

	pool = [[NSAutoreleasePool alloc] init];
	
	if([QTMovie canInitWithFile:[NSString stringWithUTF8String:name]])
	{
		[pool drain];
		return true;
	}
	else
	{
		[pool drain];
		return false;
	}
}


void free_anim_quicktime (struct anim *anim) {
	if (anim == NULL) return;
	if (anim->qtime == NULL) return;

	if(anim->qtime->ibuf)
		IMB_freeImBuf(anim->qtime->ibuf);

	[anim->qtime->media release];
	[anim->qtime->movie release];

	if(anim->qtime) MEM_freeN (anim->qtime);

	anim->qtime = NULL;

	anim->duration = 0;
}

static ImBuf * nsImageToiBuf(NSImage *sourceImage, int width, int height)
{
	ImBuf *ibuf = NULL;
	uchar *rasterRGB = NULL;
	uchar *rasterRGBA = NULL;
	uchar *toIBuf = NULL;
	int x, y, to_i, from_i;
	NSSize bitmapSize;
	NSBitmapImageRep *blBitmapFormatImageRGB,*blBitmapFormatImageRGBA,*bitmapImage;
	NSEnumerator *enumerator;
	NSImageRep *representation;
	
	ibuf = IMB_allocImBuf (width, height, 32, IB_rect, 0);
	if (!ibuf) {
		if(QTIME_DEBUG) printf("quicktime_import: could not allocate memory for the " \
				"image.\n");
		return NULL;
	}
	
	/*Get the bitmap of the image*/
	enumerator = [[sourceImage representations] objectEnumerator];
	while (representation = [enumerator nextObject]) {
        if ([representation isKindOfClass:[NSBitmapImageRep class]]) {
            bitmapImage = (NSBitmapImageRep *)representation;
			break;
        }
    }

	if (([bitmapImage bitmapFormat] & 0x5) == 0) {
		/* Try a fast copy if the image is a planar RGBA 32bit bitmap*/
		toIBuf = (uchar*)ibuf->rect;
		rasterRGB = (uchar*)[bitmapImage bitmapData];
		for (y = 0; y < height; y++) {
			to_i = (height-y-1)*width;
			from_i = y*width;
				memcpy(toIBuf+4*to_i, rasterRGB+4*from_i, 4*width);
		}
	}
	else {

		bitmapSize.width = width;
		bitmapSize.height = height;
		
		/* Tell cocoa image resolution is same as current system one */
		[bitmapImage setSize:bitmapSize];
		
		/* Convert the image in a RGBA 32bit format */
		/* As Core Graphics does not support contextes with non premutliplied alpha,
		 we need to get alpha key values in a separate batch */
		
		/* First get RGB values w/o Alpha to avoid pre-multiplication, 32bit but last byte is unused */
		blBitmapFormatImageRGB = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:width 
																		 pixelsHigh:height
																	  bitsPerSample:8 samplesPerPixel:3 hasAlpha:NO isPlanar:NO
																	 colorSpaceName:NSCalibratedRGBColorSpace 
																	   bitmapFormat:0
																		bytesPerRow:4*width
																	   bitsPerPixel:32/*RGB format padded to 32bits*/];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGB]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGB = (uchar*)[blBitmapFormatImageRGB bitmapData];
		if (rasterRGB == NULL) {
			[bitmapImage release];
			[blBitmapFormatImageRGB release];
			return NULL;
		}
		
		/* Then get Alpha values by getting the RGBA image (that is premultiplied btw) */
		blBitmapFormatImageRGBA = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		  pixelsWide:width
																		  pixelsHigh:height
																	   bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
																	  colorSpaceName:NSCalibratedRGBColorSpace
																		bitmapFormat:0
																		 bytesPerRow:4*width
																		bitsPerPixel:32/* RGBA */];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGBA]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGBA = (uchar*)[blBitmapFormatImageRGBA bitmapData];
		if (rasterRGBA == NULL) {
			[bitmapImage release];
			[blBitmapFormatImageRGB release];
			[blBitmapFormatImageRGBA release];
			return NULL;
		}

		/*Copy the image to ibuf, flipping it vertically*/
		toIBuf = (uchar*)ibuf->rect;
		for (x = 0; x < width; x++) {
			for (y = 0; y < height; y++) {
				to_i = (height-y-1)*width + x;
				from_i = y*width + x;
				
				toIBuf[4*to_i] = rasterRGB[4*from_i]; /* R */
				toIBuf[4*to_i+1] = rasterRGB[4*from_i+1]; /* G */
				toIBuf[4*to_i+2] = rasterRGB[4*from_i+2]; /* B */
				toIBuf[4*to_i+3] = rasterRGBA[4*from_i+3]; /* A */
			}
		}

		[blBitmapFormatImageRGB release];
		[blBitmapFormatImageRGBA release];
	}
	
	return ibuf;
}

ImBuf * qtime_fetchibuf (struct anim *anim, int position)
{
	NSImage *frameImage;
	QTTime time;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	ImBuf *ibuf;
	
	if (anim == NULL) {
		return (NULL);
	}

	if (position == anim->qtime->previousPosition+1) { //Optimize sequential read
		[anim->qtime->movie stepForward];
		frameImage = [anim->qtime->movie currentFrameImage];
		anim->qtime->previousPosition++;
	}
	else {
		time.timeScale = anim->qtime->durationScale;
		time.timeValue = (anim->qtime->durationTime * position) / anim->qtime->framecount;
	
		[anim->qtime->movie setCurrentTime:time];
		frameImage = [anim->qtime->movie currentFrameImage]; 
		
		anim->qtime->previousPosition = position;
	}
		
	if (frameImage == nil) {
		if(QTIME_DEBUG) printf ("Error reading frame from Quicktime");
		[pool drain];
		return NULL;
	}

	ibuf = nsImageToiBuf(frameImage,anim->x, anim->y);
	[pool drain];
	return ibuf;
}


int startquicktime (struct anim *anim)
{
	NSAutoreleasePool *pool;
	NSArray* videoTracks;
	NSSize frameSize;
	QTTime qtTimeDuration;
	NSDictionary *attributes;
	
	anim->qtime = MEM_callocN (sizeof(QuicktimeMovie),"animqt");

	if (anim->qtime == NULL) {
		if(QTIME_DEBUG) printf("Can't alloc qtime: %s\n", anim->name);
		return -1;
	}

	pool = [[NSAutoreleasePool alloc] init];
	
	attributes = [NSDictionary dictionaryWithObjectsAndKeys:
				 [NSString stringWithUTF8String:anim->name], QTMovieFileNameAttribute,
				 [NSNumber numberWithBool:NO], QTMovieEditableAttribute,
				 nil];
	
	anim->qtime->movie = [QTMovie movieWithAttributes:attributes error:NULL];
	
	if (!anim->qtime->movie) {
		if(QTIME_DEBUG) printf("qt: bad movie %s\n", anim->name);
		MEM_freeN(anim->qtime);
		if(QTIME_DEBUG) printf("qt: can't load %s\n", anim->name);
		[pool drain];
		return -1;
	}
	[anim->qtime->movie retain];

	// sets Media and Track!
	
	videoTracks = [anim->qtime->movie tracksOfMediaType:QTMediaTypeVideo];
	
	if([videoTracks count] == 0) {
		if(QTIME_DEBUG) printf("qt: no video tracks for movie %s\n", anim->name);
		[anim->qtime->movie release];
		MEM_freeN(anim->qtime);
		if(QTIME_DEBUG) printf("qt: can't load %s\n", anim->name);
		[pool drain];
		return -1;
	}
	
	anim->qtime->media = [[videoTracks objectAtIndex:0] media];
	[anim->qtime->media retain];
	
	
	frameSize = [[anim->qtime->movie attributeForKey:QTMovieCurrentSizeAttribute] sizeValue];
	anim->x = frameSize.width;
	anim->y = frameSize.height;

	if(anim->x == 0 && anim->y == 0) {
		if(QTIME_DEBUG) printf("qt: error, no dimensions\n");
		free_anim_quicktime(anim);
		[pool drain];
		return -1;
	}

	anim->qtime->ibuf = IMB_allocImBuf (anim->x, anim->y, 32, IB_rect, 0);
	
	qtTimeDuration = [[anim->qtime->media attributeForKey:QTMediaDurationAttribute] QTTimeValue];
	anim->qtime->durationTime = qtTimeDuration.timeValue;
	anim->qtime->durationScale = qtTimeDuration.timeScale;
	
	anim->qtime->framecount = [[anim->qtime->media attributeForKey:QTMediaSampleCountAttribute] longValue];
	anim->qtime->previousPosition = -2; //Force seeking for first read
	
	//fill blender's anim struct

	anim->duration = anim->qtime->framecount;
	anim->params = 0;

	anim->interlacing = 0;
	anim->orientation = 0;
	anim->framesize = anim->x * anim->y * 4;

	anim->curposition = 0;

	[pool drain];
												 
	return 0;
}

int imb_is_a_quicktime (char *name)
{
	NSImage *image;
	int result;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	// dont let quicktime image import handle these
	if( BLI_testextensie(name, ".swf") ||
		BLI_testextensie(name, ".txt") ||
		BLI_testextensie(name, ".mpg") ||
		BLI_testextensie(name, ".wav") ||
		BLI_testextensie(name, ".mov") ||	// not as image, doesn't work
		BLI_testextensie(name, ".avi") ||
		BLI_testextensie(name, ".mp3")) return 0;

	
	image = [NSImage alloc];
	if ([image initWithContentsOfFile:[NSString stringWithUTF8String:name]]) 
		result = true;
	else 
		result = false;

	[image release];
	[pool drain];
	return result;
}

ImBuf  *imb_quicktime_decode(unsigned char *mem, int size, int flags)
{
	struct ImBuf *ibuf = NULL;
	NSSize bitmapSize;
	uchar *rasterRGB = NULL;
	uchar *rasterRGBA = NULL;
	uchar *toIBuf = NULL;
	int x, y, to_i, from_i;
	NSData *data;
	NSBitmapImageRep *bitmapImage;
	NSBitmapImageRep *blBitmapFormatImageRGB,*blBitmapFormatImageRGBA;
	NSAutoreleasePool *pool;
	
	pool = [[NSAutoreleasePool alloc] init];
	
	data = [NSData dataWithBytes:mem length:size];
	bitmapImage = [[NSBitmapImageRep alloc] initWithData:data];
	
	if (!bitmapImage) {
		fprintf(stderr, "imb_cocoaLoadImage: error loading image\n");
		[pool drain];
		return NULL;
	}
	
	bitmapSize.width = [bitmapImage pixelsWide];
	bitmapSize.height = [bitmapImage pixelsHigh];
	
	/* Tell cocoa image resolution is same as current system one */
	[bitmapImage setSize:bitmapSize];
	
	/* allocate the image buffer */
	ibuf = IMB_allocImBuf(bitmapSize.width, bitmapSize.height, 32/*RGBA*/, 0, 0);
	if (!ibuf) {
		fprintf(stderr, 
				"imb_cocoaLoadImage: could not allocate memory for the " \
				"image.\n");
		[bitmapImage release];
		[pool drain];
		return NULL;
	}
	
	/* read in the image data */
	if (!(flags & IB_test)) {
		
		/* allocate memory for the ibuf->rect */
		imb_addrectImBuf(ibuf);
		
		/* Convert the image in a RGBA 32bit format */
		/* As Core Graphics does not support contextes with non premutliplied alpha,
		 we need to get alpha key values in a separate batch */
		
		/* First get RGB values w/o Alpha to avoid pre-multiplication, 32bit but last byte is unused */
		blBitmapFormatImageRGB = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:bitmapSize.width 
																		 pixelsHigh:bitmapSize.height
																	  bitsPerSample:8 samplesPerPixel:3 hasAlpha:NO isPlanar:NO
																	 colorSpaceName:NSCalibratedRGBColorSpace 
																	   bitmapFormat:0
																		bytesPerRow:4*bitmapSize.width
																	   bitsPerPixel:32/*RGB format padded to 32bits*/];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGB]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGB = (uchar*)[blBitmapFormatImageRGB bitmapData];
		if (rasterRGB == NULL) {
			[bitmapImage release];
			[blBitmapFormatImageRGB release];
			[pool drain];
			return NULL;
		}
		
		/* Then get Alpha values by getting the RGBA image (that is premultiplied btw) */
		blBitmapFormatImageRGBA = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		  pixelsWide:bitmapSize.width
																		  pixelsHigh:bitmapSize.height
																	   bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
																	  colorSpaceName:NSCalibratedRGBColorSpace
																		bitmapFormat:0
																		 bytesPerRow:4*bitmapSize.width
																		bitsPerPixel:32/* RGBA */];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGBA]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGBA = (uchar*)[blBitmapFormatImageRGBA bitmapData];
		if (rasterRGBA == NULL) {
			[bitmapImage release];
			[blBitmapFormatImageRGB release];
			[blBitmapFormatImageRGBA release];
			[pool drain];
			return NULL;
		}
		
		/*Copy the image to ibuf, flipping it vertically*/
		toIBuf = (uchar*)ibuf->rect;
		for (x = 0; x < bitmapSize.width; x++) {
			for (y = 0; y < bitmapSize.height; y++) {
				to_i = (bitmapSize.height-y-1)*bitmapSize.width + x;
				from_i = y*bitmapSize.width + x;
				
				toIBuf[4*to_i] = rasterRGB[4*from_i]; /* R */
				toIBuf[4*to_i+1] = rasterRGB[4*from_i+1]; /* G */
				toIBuf[4*to_i+2] = rasterRGB[4*from_i+2]; /* B */
				toIBuf[4*to_i+3] = rasterRGBA[4*from_i+3]; /* A */
			}
		}
		
		[blBitmapFormatImageRGB release];
		[blBitmapFormatImageRGBA release];
	}
	
	/* release the cocoa objects */
	[bitmapImage release];
	[pool drain];
	
	if (ENDIAN_ORDER == B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);
	
	/* return successfully */
	return (ibuf);	
}


#endif /* WITH_QUICKTIME */

