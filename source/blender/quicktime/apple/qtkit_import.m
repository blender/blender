/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "MEM_guardedalloc.h"

#include "IMB_anim.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "BKE_global.h"

#include "BLI_dynstr.h"
#include "BLI_path_util.h"

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
	G.have_quicktime = true;
}

void quicktime_exit(void)
{
	if (G.have_quicktime) {
		free_qtcomponentdata();
	}
}


int anim_is_quicktime(const char *name)
{
	NSAutoreleasePool *pool;
	
	// don't let quicktime movie import handle these

	if (BLI_testextensie_n(
	        name,
	        ".swf",
	        ".txt",
	        ".mpg",
	        ".avi",  /* wouldn't be appropriate ;) */
	        ".mov",  /* disabled, suboptimal decoding speed */
	        ".mp4",  /* disabled, suboptimal decoding speed */
	        ".m4v",  /* disabled, suboptimal decoding speed */
	        ".tga",
	        ".png",
	        ".bmp",
	        ".jpg",
	        ".tif",
	        ".exr",
	        ".wav",
	        ".zip",
	        ".mp3",
	        NULL))
	{
		return 0;
	}

	if (QTIME_DEBUG) printf("qt: checking as movie: %s\n", name);

	pool = [[NSAutoreleasePool alloc] init];
	
	if ([QTMovie canInitWithFile:[NSString stringWithCString:name
	                              encoding:[NSString defaultCStringEncoding]]])
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


void free_anim_quicktime(struct anim *anim)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	if (anim == NULL) return;
	if (anim->qtime == NULL) return;

	if (anim->qtime->ibuf)
		IMB_freeImBuf(anim->qtime->ibuf);

	[anim->qtime->media release];
	[anim->qtime->movie release];

	[QTMovie exitQTKitOnThread];

	if (anim->qtime) MEM_freeN (anim->qtime);

	anim->qtime = NULL;

	anim->duration = 0;

	[pool drain];
}

static ImBuf *nsImageToiBuf(NSImage *sourceImage, int width, int height)
{
	ImBuf *ibuf = NULL;
	uchar *rasterRGB = NULL;
	uchar *rasterRGBA = NULL;
	uchar *toIBuf = NULL;
	int x, y, to_i, from_i;
	NSSize bitmapSize;
	NSBitmapImageRep *blBitmapFormatImageRGB,*blBitmapFormatImageRGBA, *bitmapImage = nil;
	NSEnumerator *enumerator;
	NSImageRep *representation;
	
	ibuf = IMB_allocImBuf(width, height, 32, IB_rect);
	if (!ibuf) {
		if (QTIME_DEBUG) {
			printf("quicktime_import: could not allocate memory for the image.\n");
		}
		return NULL;
	}
	
	/*Get the bitmap of the image*/
	enumerator = [[sourceImage representations] objectEnumerator];
	while ((representation = [enumerator nextObject])) {
		if ([representation isKindOfClass:[NSBitmapImageRep class]]) {
			bitmapImage = (NSBitmapImageRep *)representation;
			break;
		}
	}
	if (bitmapImage == nil) return NULL;

	if (([bitmapImage bitsPerPixel] == 32) && (([bitmapImage bitmapFormat] & 0x5) == 0)
		&& ![bitmapImage isPlanar]) {
		/* Try a fast copy if the image is a meshed RGBA 32bit bitmap*/
		toIBuf = (uchar *)ibuf->rect;
		rasterRGB = (uchar *)[bitmapImage bitmapData];
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
																	 colorSpaceName:NSDeviceRGBColorSpace 
																	   bitmapFormat:0
																		bytesPerRow:4*width
																	   bitsPerPixel:32/*RGB format padded to 32bits*/];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGB]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGB = (uchar *)[blBitmapFormatImageRGB bitmapData];
		if (rasterRGB == NULL) {
			[blBitmapFormatImageRGB release];
			return NULL;
		}
		
		/* Then get Alpha values by getting the RGBA image (that is premultiplied btw) */
		blBitmapFormatImageRGBA = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		  pixelsWide:width
																		  pixelsHigh:height
																	   bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
																	  colorSpaceName:NSDeviceRGBColorSpace
																		bitmapFormat:0
																		 bytesPerRow:4*width
																		bitsPerPixel:32/* RGBA */];
		
		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:blBitmapFormatImageRGBA]];
		[bitmapImage draw];
		[NSGraphicsContext restoreGraphicsState];
		
		rasterRGBA = (uchar *)[blBitmapFormatImageRGBA bitmapData];
		if (rasterRGBA == NULL) {
			[blBitmapFormatImageRGB release];
			[blBitmapFormatImageRGBA release];
			return NULL;
		}

		/*Copy the image to ibuf, flipping it vertically*/
		toIBuf = (uchar *)ibuf->rect;
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
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

ImBuf *qtime_fetchibuf (struct anim *anim, int position)
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
		if (QTIME_DEBUG) printf ("Error reading frame from Quicktime");
		[pool drain];
		return NULL;
	}

	ibuf = nsImageToiBuf(frameImage,anim->x, anim->y);
	[pool drain];
	
	return ibuf;
}


int startquicktime(struct anim *anim)
{
	NSAutoreleasePool *pool;
	NSArray* videoTracks;
	NSSize frameSize;
	QTTime qtTimeDuration;
	NSDictionary *attributes;
	
	anim->qtime = MEM_callocN(sizeof(QuicktimeMovie),"animqt");

	if (anim->qtime == NULL) {
		if (QTIME_DEBUG) printf("Can't alloc qtime: %s\n", anim->name);
		return -1;
	}

	pool = [[NSAutoreleasePool alloc] init];
	
	[QTMovie enterQTKitOnThread];

	attributes = [NSDictionary dictionaryWithObjectsAndKeys:
	        [NSString stringWithCString:anim->name
	        encoding:[NSString defaultCStringEncoding]], QTMovieFileNameAttribute,
	        [NSNumber numberWithBool:NO], QTMovieEditableAttribute,
	    nil];

	anim->qtime->movie = [QTMovie movieWithAttributes:attributes error:NULL];
	
	if (!anim->qtime->movie) {
		if (QTIME_DEBUG) printf("qt: bad movie %s\n", anim->name);
		MEM_freeN(anim->qtime);
		if (QTIME_DEBUG) printf("qt: can't load %s\n", anim->name);
		[QTMovie exitQTKitOnThread];
		[pool drain];
		return -1;
	}
	[anim->qtime->movie retain];

	// sets Media and Track!
	
	videoTracks = [anim->qtime->movie tracksOfMediaType:QTMediaTypeVideo];
	
	if ([videoTracks count] == 0) {
		if (QTIME_DEBUG) printf("qt: no video tracks for movie %s\n", anim->name);
		[anim->qtime->movie release];
		MEM_freeN(anim->qtime);
		if (QTIME_DEBUG) printf("qt: can't load %s\n", anim->name);
		[QTMovie exitQTKitOnThread];
		[pool drain];
		return -1;
	}
	
	anim->qtime->media = [[videoTracks objectAtIndex:0] media];
	[anim->qtime->media retain];
	
	
	frameSize = [[anim->qtime->movie attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
	anim->x = frameSize.width;
	anim->y = frameSize.height;

	if (anim->x == 0 && anim->y == 0) {
		if (QTIME_DEBUG) printf("qt: error, no dimensions\n");
		free_anim_quicktime(anim);
		[pool drain];
		return -1;
	}

	anim->qtime->ibuf = IMB_allocImBuf(anim->x, anim->y, 32, IB_rect);
	
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

#endif /* WITH_QUICKTIME */

