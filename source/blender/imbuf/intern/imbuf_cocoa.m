/*
 * imbuf_coca.m
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Damien Plisson 10/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**
 * Provides image file loading and saving for Blender, via Cocoa.
 *
 */

#include <stdint.h>
#include <string.h>
#import <Cocoa/Cocoa.h>

#include "imbuf.h"

#include "IMB_cocoa.h"

#include "BKE_global.h"
#include "BKE_colortools.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"



#pragma mark load/save functions

/**
 * Loads an image from the supplied buffer
 *
 * Loads any Core Graphics supported type
 * Currently is : TIFF, BMP, JPEG, GIF, PNG, DIB, ICO, and various RAW formats
 *
 * @param mem:   Memory containing the bitmap image
 * @param size:  Size of the mem buffer.
 * @param flags: If flags has IB_test set then the file is not actually loaded,
 *                but all other operations take place.
 *
 * @return: A newly allocated ImBuf structure if successful, otherwise NULL.
 */
struct ImBuf *imb_cocoaLoadImage(unsigned char *mem, int size, int flags)
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

	ibuf->ftype = TIF;
	ibuf->profile = IB_PROFILE_SRGB;

	/* return successfully */
	return (ibuf);
}

/**
 * Saves an image to a file.
 *
 * ImBuf structures with 1, 3 or 4 bytes per pixel (GRAY, RGB, RGBA 
 * respectively) are accepted, and interpreted correctly. 
 *
 * Accepted formats: TIFF, GIF, BMP, PNG, JPEG, JPEG2000
 *
 * @param ibuf:  Image buffer.
 * @param name:  Name of the image file to create.
 * @param flags: Currently largely ignored.
 *
 * @return: 1 if the function is successful, 0 on failure.
 */

#define FTOUSHORT(val) ((val >= 1.0f-0.5f/65535)? 65535: (val <= 0.0f)? 0: (unsigned short)(val*65535.0f + 0.5f))

short imb_cocoaSaveImage(struct ImBuf *ibuf, char *name, int flags)
{
	uint16_t samplesperpixel, bitspersample;
	unsigned char *from = NULL, *to = NULL;
	unsigned short *to16 = NULL;
	float *fromf = NULL;
	int x, y, from_i, to_i, i;
	int success;
	BOOL hasAlpha;
	NSString* colorSpace;
	NSBitmapImageRep *blBitmapFormatImage;
	NSData *dataToWrite;
	NSDictionary *imageProperties;
	
	NSAutoreleasePool *pool;
	
	if (!ibuf) return FALSE;
	if (!name) return FALSE;
	
	/* check for a valid number of bytes per pixel.  Like the PNG writer,
	 * the TIFF writer supports 1, 3 or 4 bytes per pixel, corresponding
	 * to gray, RGB, RGBA respectively. */
	samplesperpixel = (uint16_t)((ibuf->depth + 7) >> 3);
	switch (samplesperpixel) {
		case 4: /*RGBA type*/
			hasAlpha = YES;
			colorSpace = NSCalibratedRGBColorSpace;
			break;
		case 3: /*RGB type*/
			hasAlpha = NO;
			colorSpace = NSCalibratedRGBColorSpace;
			break;
		case 1:
			hasAlpha = NO;
			colorSpace = NSCalibratedWhiteColorSpace;
			break;
		default:
			fprintf(stderr,
					"imb_cocoaSaveImage: unsupported number of bytes per " 
					"pixel: %d\n", samplesperpixel);
			return (0);
	}

	if((ibuf->ftype & TIF_16BIT) && ibuf->rect_float)
		bitspersample = 16;
	else
		bitspersample = 8;

	pool = [[NSAutoreleasePool alloc] init];
	
	/* Create bitmap image rep in blender format */
	blBitmapFormatImage = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																  pixelsWide:ibuf->x 
																  pixelsHigh:ibuf->y
															   bitsPerSample:bitspersample samplesPerPixel:samplesperpixel hasAlpha:hasAlpha isPlanar:NO
															  colorSpaceName:colorSpace 
																bitmapFormat:NSAlphaNonpremultipliedBitmapFormat
																 bytesPerRow:(ibuf->x*bitspersample*samplesperpixel/8)
																bitsPerPixel:(bitspersample*samplesperpixel)];
	if (!blBitmapFormatImage) {
		[pool drain];
		return FALSE;
	}
	
	/* setup pointers */
	if(bitspersample == 16) {
		fromf = ibuf->rect_float;
		to16   = (unsigned short*)[blBitmapFormatImage bitmapData];
	}
	else {
		from = (unsigned char*)ibuf->rect;
		to   = (unsigned char*)[blBitmapFormatImage bitmapData];
	}

	/* copy pixel data.  While copying, we flip the image vertically. */
	for (x = 0; x < ibuf->x; x++) {
		for (y = 0; y < ibuf->y; y++) {
			from_i = 4*(y*ibuf->x+x);
			to_i   = samplesperpixel*((ibuf->y-y-1)*ibuf->x+x);
			
			if(bitspersample == 16) {
				if (ibuf->profile == IB_PROFILE_SRGB) {
					switch (samplesperpixel) {
						case 4 /*RGBA*/:
							to16[to_i] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i]));
							to16[to_i+1] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i+1]));
							to16[to_i+2] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i+2]));
							to16[to_i+3] = FTOUSHORT(fromf[from_i+3]);
							break;
						case 3 /*RGB*/:
							to16[to_i] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i]));
							to16[to_i+1] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i+1]));
							to16[to_i+2] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i+2]));
							break;
						case 1 /*BW*/:
							to16[to_i] = FTOUSHORT(linearrgb_to_srgb(fromf[from_i]));
							break;
					}
				} 
				else {
					for (i = 0; i < samplesperpixel; i++, to_i++, from_i++)
							to16[to_i] = FTOUSHORT(fromf[from_i]);
				}
			}
			else {
				/* 8bits per sample*/
				for (i = 0; i < samplesperpixel; i++, to_i++, from_i++)
					to[to_i] = from[from_i];
			}
		}
	}
	
	/* generate file data */
	if (IS_tiff(ibuf)) {
		dataToWrite = [blBitmapFormatImage TIFFRepresentationUsingCompression:NSTIFFCompressionLZW factor:1.0];
	}
	else if (IS_png(ibuf)) {
		imageProperties = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:false], NSImageInterlaced,
						   nil];
		dataToWrite = [blBitmapFormatImage representationUsingType:NSPNGFileType properties:imageProperties];
	}
	else if (IS_bmp(ibuf)) {
		dataToWrite = [blBitmapFormatImage representationUsingType:NSBMPFileType properties:nil];
	}
	else {/* JPEG by default */
		int quality;
		
		quality = ibuf->ftype & 0xff;
		if (quality <= 0) quality = 90; /* Standard quality if wrong supplied*/
		if (quality > 100) quality = 100;
		
		imageProperties = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithFloat:quality], NSImageCompressionFactor,
						   [NSNumber numberWithBool:true], NSImageProgressive, 
						   nil];
		dataToWrite = [blBitmapFormatImage representationUsingType:NSJPEGFileType properties:imageProperties];
	}

	/* Write the file */
	success = [dataToWrite writeToFile:[NSString stringWithCString:name encoding:NSISOLatin1StringEncoding]
				  atomically:YES];

	[blBitmapFormatImage release];
	[pool drain];
	
	return success;
}

#pragma mark format checking functions

/* Currently, only tiff format is handled, so need to include here function that was previously in tiff.c */

/**
 * Checks whether a given memory buffer contains a TIFF file.
 *
 * FIXME: Possible memory leak if mem is less than IMB_TIFF_NCB bytes long.
 *        However, changing this will require up-stream modifications.
 *
 * This method uses the format identifiers from:
 *     http://www.faqs.org/faqs/graphics/fileformats-faq/part4/section-9.html
 * The first four bytes of big-endian and little-endian TIFF files
 * respectively are (hex):
 * 	4d 4d 00 2a
 * 	49 49 2a 00
 * Note that TIFF files on *any* platform can be either big- or little-endian;
 * it's not platform-specific.
 *
 * AFAICT, libtiff doesn't provide a method to do this automatically, and
 * hence my manual comparison. - Jonathan Merritt (lancelet) 4th Sept 2005.
 */
#define IMB_TIFF_NCB 4		/* number of comparison bytes used */
int imb_is_a_tiff(void *mem)
{
	char big_endian[IMB_TIFF_NCB] = { 0x4d, 0x4d, 0x00, 0x2a };
	char lil_endian[IMB_TIFF_NCB] = { 0x49, 0x49, 0x2a, 0x00 };
	
	return ( (memcmp(big_endian, mem, IMB_TIFF_NCB) == 0) ||
			(memcmp(lil_endian, mem, IMB_TIFF_NCB) == 0) );
}
