/*
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
#if defined(_WIN32) || defined(__APPLE__)

#include <stdio.h>
#include <string.h>

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_report.h"

#include "BLI_blenlib.h"

#include "BLO_sys_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

#ifdef __APPLE__
/* evil */
#ifndef __AIFF__
#define __AIFF__
#endif
#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>
#include <AudioToolbox/AudioToolbox.h>

#if (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4) || !__LP64__
#error 64 bit build & OSX 10.5 minimum are needed for QTKit
#endif

#include "quicktime_import.h"
#include "quicktime_export.h"

#endif /* __APPLE__ */

typedef struct QuicktimeExport {
	QTMovie *movie;
	
	NSString *filename;

	QTTime frameDuration;
	NSDictionary *frameAttributes;
	
	NSString *videoTempFileName;
	/* Audio section */
	AUD_Device *audioInputDevice;
	AudioFileID audioFile;
	NSString *audioFileName;
	AudioConverterRef audioConverter;
	AudioBufferList audioBufferList;
	AudioStreamBasicDescription audioInputFormat, audioOutputFormat;
	AudioStreamPacketDescription *audioOutputPktDesc;
	SInt64 audioFilePos;
	char* audioInputBuffer;
	char* audioOutputBuffer;
	UInt32 audioCodecMaxOutputPacketSize;
	UInt64 audioTotalExportedFrames, audioTotalSavedFrames;
	UInt64 audioLastFrame;
	SInt64 audioOutputPktPos;
	
} QuicktimeExport;

static struct QuicktimeExport *qtexport;

#define AUDIOOUTPUTBUFFERSIZE 65536

#pragma mark rna helper functions

/* Video codec */
static QuicktimeCodecTypeDesc qtVideoCodecList[] = {
	{kRawCodecType, 1, "Uncompressed"},
	{k422YpCbCr8CodecType, 2, "Uncompressed 8-bit 4:2:2"},
	{k422YpCbCr10CodecType, 3, "Uncompressed 10-bit 4:2:2"},
	{kComponentVideoCodecType, 4, "Component Video"},
	{kPixletCodecType, 5, "Pixlet"},
	{kJPEGCodecType, 6, "JPEG"},
	{kMotionJPEGACodecType, 7, "M-JPEG A"},
	{kMotionJPEGBCodecType, 8, "M-JPEG B"},
	{kDVCPALCodecType, 9, "DV PAL"},
	{kDVCNTSCCodecType, 10, "DV/DVCPRO NTSC"},
	{kDVCPROHD720pCodecType, 11, "DVCPRO HD 720p"},
	{kDVCPROHD1080i50CodecType, 12, "DVCPRO HD 1080i50"},
	{kDVCPROHD1080i60CodecType, 13, "DVCPRO HD 1080i60"},
	{kMPEG4VisualCodecType, 14, "MPEG4"},
	{kH263CodecType, 15, "H.263"},
	{kH264CodecType, 16, "H.264"},
	{kAnimationCodecType, 17, "Animation"},
	{0,0,NULL}};

static int qtVideoCodecCount = 17;

int quicktime_get_num_videocodecs() {
	return qtVideoCodecCount;
}

QuicktimeCodecTypeDesc* quicktime_get_videocodecType_desc(int indexValue) {
	if ((indexValue>=0) && (indexValue < qtVideoCodecCount))
		return &qtVideoCodecList[indexValue];
	else
		return NULL;
}

int quicktime_rnatmpvalue_from_videocodectype(int codecType) {
	int i;
	for (i=0;i<qtVideoCodecCount;i++) {
		if (qtVideoCodecList[i].codecType == codecType)
			return qtVideoCodecList[i].rnatmpvalue;
	}

	return 0;
}

int quicktime_videocodecType_from_rnatmpvalue(int rnatmpvalue) {
	int i;
	for (i=0;i<qtVideoCodecCount;i++) {
		if (qtVideoCodecList[i].rnatmpvalue == rnatmpvalue)
			return qtVideoCodecList[i].codecType;
	}
	
	return 0;	
}

/* Audio codec */
static QuicktimeCodecTypeDesc qtAudioCodecList[] = {
	{0, 0, "No audio"},
	{kAudioFormatLinearPCM, 1, "LPCM"},
	{kAudioFormatAppleLossless, 2, "Apple Lossless"},
	{kAudioFormatMPEG4AAC, 3, "AAC"},
	{0,0,NULL}};

static int qtAudioCodecCount = 4;

int quicktime_get_num_audiocodecs() {
	return qtAudioCodecCount;
}

QuicktimeCodecTypeDesc* quicktime_get_audiocodecType_desc(int indexValue) {
	if ((indexValue>=0) && (indexValue < qtAudioCodecCount))
		return &qtAudioCodecList[indexValue];
	else
		return NULL;
}

int quicktime_rnatmpvalue_from_audiocodectype(int codecType) {
	int i;
	for (i=0;i<qtAudioCodecCount;i++) {
		if (qtAudioCodecList[i].codecType == codecType)
			return qtAudioCodecList[i].rnatmpvalue;
	}
	
	return 0;
}

int quicktime_audiocodecType_from_rnatmpvalue(int rnatmpvalue) {
	int i;
	for (i=0;i<qtAudioCodecCount;i++) {
		if (qtAudioCodecList[i].rnatmpvalue == rnatmpvalue)
			return qtAudioCodecList[i].codecType;
	}
	
	return 0;	
}


static NSString *stringWithCodecType(int codecType) {
	char str[5];
	
	*((int*)str) = EndianU32_NtoB(codecType);
	str[4] = 0;
	
	return [NSString stringWithCString:str encoding:NSASCIIStringEncoding];
}

void makeqtstring (RenderData *rd, char *string) {
	char txt[64];

	strcpy(string, rd->pic);
	BLI_path_abs(string, G.main->name);

	BLI_make_existing_file(string);

	if (BLI_strcasecmp(string + strlen(string) - 4, ".mov")) {
		sprintf(txt, "%04d-%04d.mov", (rd->sfra) , (rd->efra) );
		strcat(string, txt);
	}
}

void filepath_qt(char *string, RenderData *rd) {
	if (string==NULL) return;
	
	strcpy(string, rd->pic);
	BLI_path_abs(string, G.main->name);
	
	BLI_make_existing_file(string);
	
	if (!BLI_testextensie(string, ".mov")) {
		/* if we dont have any #'s to insert numbers into, use 4 numbers by default */
		if (strchr(string, '#')==NULL)
			strcat(string, "####"); /* 4 numbers */

		BLI_path_frame_range(string, rd->sfra, rd->efra, 4);
		strcat(string, ".mov");
	}
}


#pragma mark audio export functions

static OSStatus	write_cookie(AudioConverterRef converter, AudioFileID outfile)
{
	// grab the cookie from the converter and write it to the file
	UInt32 cookieSize = 0;
	OSStatus err = AudioConverterGetPropertyInfo(converter, kAudioConverterCompressionMagicCookie, &cookieSize, NULL);
	// if there is an error here, then the format doesn't have a cookie, so on we go
	if (!err && cookieSize) {
		char* cookie = malloc(cookieSize);
		
		err = AudioConverterGetProperty(converter, kAudioConverterCompressionMagicCookie, &cookieSize, cookie);
		
		if (!err)
			err = AudioFileSetProperty (outfile, kAudioFilePropertyMagicCookieData, cookieSize, cookie);
			// even though some formats have cookies, some files don't take them
		
		free(cookie);
	}
	return err;
}

/* AudioConverter input stream callback */
static OSStatus AudioConverterInputCallback(AudioConverterRef inAudioConverter, 
						 UInt32* ioNumberDataPackets,
						 AudioBufferList* ioData,
						 AudioStreamPacketDescription**	outDataPacketDescription,
						 void* inUserData)
{	
	if (qtexport->audioTotalExportedFrames >= qtexport->audioLastFrame) { /* EOF */
		*ioNumberDataPackets = 0;
		return noErr;
	}

	if (qtexport->audioInputFormat.mBytesPerPacket * *ioNumberDataPackets > AUDIOOUTPUTBUFFERSIZE)
		*ioNumberDataPackets = AUDIOOUTPUTBUFFERSIZE / qtexport->audioInputFormat.mBytesPerPacket;
	
	if ((qtexport->audioTotalExportedFrames + *ioNumberDataPackets) > qtexport->audioLastFrame)
		*ioNumberDataPackets = (qtexport->audioLastFrame - qtexport->audioTotalExportedFrames) / qtexport->audioInputFormat.mFramesPerPacket;
	
	qtexport->audioTotalExportedFrames += *ioNumberDataPackets;
	
	AUD_readDevice(qtexport->audioInputDevice, (UInt8*)qtexport->audioInputBuffer, 
				   qtexport->audioInputFormat.mFramesPerPacket * *ioNumberDataPackets);
	
	ioData->mBuffers[0].mDataByteSize = qtexport->audioInputFormat.mBytesPerPacket * *ioNumberDataPackets;
	ioData->mBuffers[0].mData = qtexport->audioInputBuffer;
	ioData->mBuffers[0].mNumberChannels = qtexport->audioInputFormat.mChannelsPerFrame;
	
	return noErr;
}	


#pragma mark export functions

int start_qt(struct Scene *scene, struct RenderData *rd, int rectx, int recty, ReportList *reports)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSError *error;
	char name[1024];
	int success= 1;
	OSStatus err=noErr;

	if(qtexport == NULL) qtexport = MEM_callocN(sizeof(QuicktimeExport), "QuicktimeExport");
	
	[QTMovie enterQTKitOnThread];		
	
	/* Check first if the QuickTime 7.2.1 initToWritableFile: method is available */
	if ([[[[QTMovie alloc] init] autorelease] respondsToSelector:@selector(initToWritableFile:error:)] != YES) {
		BKE_report(reports, RPT_ERROR, "\nUnable to create quicktime movie, need Quicktime rev 7.2.1 or later");
		success= 0;
	}
	else {
		makeqtstring(rd, name);
		qtexport->filename = [[NSString alloc] initWithCString:name
								  encoding:[NSString defaultCStringEncoding]];
		qtexport->movie = nil;
		qtexport->audioFile = NULL;

		if (rd->qtcodecsettings.audiocodecType) {
			// generate a name for our video & audio files
			/* Init audio file */
			CFURLRef outputFileURL;
			char extension[32];
			AudioFileTypeID audioFileType;
			
			switch (rd->qtcodecsettings.audiocodecType) {
				case kAudioFormatLinearPCM:
					audioFileType = kAudioFileWAVEType;
					strcpy(extension,".wav");
					break;
				case kAudioFormatMPEG4AAC:
				case kAudioFormatAppleLossless:
					audioFileType = kAudioFileM4AType;
					strcpy(extension, ".m4a");
					break;
				default:
					audioFileType = kAudioFileAIFFType;
					strcpy(extension,".aiff");
					break;
			}
					
			tmpnam(name);
			strcat(name, extension);
			outputFileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,(UInt8*) name, strlen(name), false);
			
			if (outputFileURL) {
				
				qtexport->audioFileName = [[NSString alloc] initWithCString:name
															 encoding:[NSString defaultCStringEncoding]];
				
				qtexport->audioInputFormat.mSampleRate = U.audiorate;
				qtexport->audioInputFormat.mFormatID = kAudioFormatLinearPCM;
				qtexport->audioInputFormat.mChannelsPerFrame = U.audiochannels;
				switch (U.audioformat) {
					case AUD_FORMAT_U8:
						qtexport->audioInputFormat.mBitsPerChannel = 8;
						qtexport->audioInputFormat.mFormatFlags = 0;
						break;
					case AUD_FORMAT_S24:
						qtexport->audioInputFormat.mBitsPerChannel = 24;
						qtexport->audioInputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
						break;
					case AUD_FORMAT_S32:
						qtexport->audioInputFormat.mBitsPerChannel = 32;
						qtexport->audioInputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
						break;
					case AUD_FORMAT_FLOAT32:
						qtexport->audioInputFormat.mBitsPerChannel = 32;
						qtexport->audioInputFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
						break;
					case AUD_FORMAT_FLOAT64:
						qtexport->audioInputFormat.mBitsPerChannel = 64;
						qtexport->audioInputFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
						break;
					case AUD_FORMAT_S16:
					default:
						qtexport->audioInputFormat.mBitsPerChannel = 16;
						qtexport->audioInputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
						break;
				}
				qtexport->audioInputFormat.mBytesPerFrame = qtexport->audioInputFormat.mChannelsPerFrame * qtexport->audioInputFormat.mBitsPerChannel / 8;
				qtexport->audioInputFormat.mFramesPerPacket = 1; /*If not ==1, then need to check input callback for "rounding" issues"*/
				qtexport->audioInputFormat.mBytesPerPacket = qtexport->audioInputFormat.mBytesPerFrame;
				qtexport->audioInputFormat.mFormatFlags |= kLinearPCMFormatFlagIsPacked;
				
				
				/*Ouput format*/
				qtexport->audioOutputFormat.mFormatID = rd->qtcodecsettings.audiocodecType;
				//TODO: set audio channels
				qtexport->audioOutputFormat.mChannelsPerFrame = 2;
				qtexport->audioOutputFormat.mSampleRate = rd->qtcodecsettings.audioSampleRate;
				
				/* Default value for compressed formats, overriden after if not the case */
				qtexport->audioOutputFormat.mFramesPerPacket = 0;
				qtexport->audioOutputFormat.mBytesPerFrame = 0;
				qtexport->audioOutputFormat.mBytesPerPacket = 0;
				qtexport->audioOutputFormat.mBitsPerChannel = 0;

				switch (rd->qtcodecsettings.audiocodecType) {
					case kAudioFormatMPEG4AAC:
						qtexport->audioOutputFormat.mFormatFlags = kMPEG4Object_AAC_Main;
						/* AAC codec does not handle sample rates above 48kHz, force this limit instead of getting an error afterwards */
						if (qtexport->audioOutputFormat.mSampleRate > 48000) qtexport->audioOutputFormat.mSampleRate = 48000;
						break;
					case kAudioFormatAppleLossless:
						switch (U.audioformat) {
							case AUD_FORMAT_S16:
								qtexport->audioOutputFormat.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
								break;
							case AUD_FORMAT_S24:
								qtexport->audioOutputFormat.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
								break;
							case AUD_FORMAT_S32:
								qtexport->audioOutputFormat.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
								break;
							case AUD_FORMAT_U8:
							case AUD_FORMAT_FLOAT32:
							case AUD_FORMAT_FLOAT64:
							default:
								break;
						}
						break;
					case kAudioFormatLinearPCM:
					default:
						switch (rd->qtcodecsettings.audioBitDepth) {
							case AUD_FORMAT_U8:
								qtexport->audioOutputFormat.mBitsPerChannel = 8;
								qtexport->audioOutputFormat.mFormatFlags = 0;
								break;
							case AUD_FORMAT_S24:
								qtexport->audioOutputFormat.mBitsPerChannel = 24;
								qtexport->audioOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
								break;
							case AUD_FORMAT_S32:
								qtexport->audioOutputFormat.mBitsPerChannel = 32;
								qtexport->audioOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
								break;
							case AUD_FORMAT_FLOAT32:
								qtexport->audioOutputFormat.mBitsPerChannel = 32;
								qtexport->audioOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
								break;
							case AUD_FORMAT_FLOAT64:
								qtexport->audioOutputFormat.mBitsPerChannel = 64;
								qtexport->audioOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
								break;
							case AUD_FORMAT_S16:
							default:
								qtexport->audioOutputFormat.mBitsPerChannel = 16;
								qtexport->audioOutputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
								break;
						}
						qtexport->audioOutputFormat.mFormatFlags |= kLinearPCMFormatFlagIsPacked;
						qtexport->audioOutputFormat.mBytesPerPacket = qtexport->audioOutputFormat.mChannelsPerFrame * (qtexport->audioOutputFormat.mBitsPerChannel / 8);
						qtexport->audioOutputFormat.mFramesPerPacket = 1;
						qtexport->audioOutputFormat.mBytesPerFrame = qtexport->audioOutputFormat.mBytesPerPacket;
						break;
				}
												
				err = AudioFileCreateWithURL(outputFileURL, audioFileType, &qtexport->audioOutputFormat, kAudioFileFlags_EraseFile, &qtexport->audioFile);
				CFRelease(outputFileURL);
				
				if(err)
					BKE_report(reports, RPT_ERROR, "\nQuicktime: unable to create temporary audio file. Format error ?");
				else {
					err = AudioConverterNew(&qtexport->audioInputFormat, &qtexport->audioOutputFormat, &qtexport->audioConverter);
					if (err) {
						BKE_report(reports, RPT_ERROR, "\nQuicktime: unable to initialize audio codec converter. Format error ?");
						AudioFileClose(qtexport->audioFile);
						qtexport->audioFile = NULL;
						[qtexport->audioFileName release];
						qtexport->audioFileName = nil;
					} else {
						UInt32 prop,propSize;
						/* Set up codec properties */
						if (rd->qtcodecsettings.audiocodecType == kAudioFormatMPEG4AAC) { /*Lossy compressed format*/
							prop = rd->qtcodecsettings.audioBitRate;
							AudioConverterSetProperty(qtexport->audioConverter, kAudioConverterEncodeBitRate,
													  sizeof(prop), &prop);
							
							if (rd->qtcodecsettings.audioCodecFlags & QTAUDIO_FLAG_CODEC_ISCBR)
								prop = kAudioCodecBitRateControlMode_Constant;
							else
								prop = kAudioCodecBitRateControlMode_LongTermAverage;
							AudioConverterSetProperty(qtexport->audioConverter, kAudioCodecPropertyBitRateControlMode,
															sizeof(prop), &prop);
						}
						/* Conversion quality : if performance impact then offer degraded option */
						if ((rd->qtcodecsettings.audioCodecFlags & QTAUDIO_FLAG_RESAMPLE_NOHQ) == 0) {							
							prop = kAudioConverterSampleRateConverterComplexity_Mastering;
							AudioConverterSetProperty(qtexport->audioConverter, kAudioConverterSampleRateConverterComplexity,
													  sizeof(prop), &prop);
							
							prop = kAudioConverterQuality_Max;
							AudioConverterSetProperty(qtexport->audioConverter, kAudioConverterSampleRateConverterQuality,
													  sizeof(prop), &prop);
						}
						
						write_cookie(qtexport->audioConverter, qtexport->audioFile);
						
						/* Allocate output buffer */
						if (qtexport->audioOutputFormat.mBytesPerPacket ==0) /* VBR */
							AudioConverterGetProperty(qtexport->audioConverter, kAudioConverterPropertyMaximumOutputPacketSize,
												  &propSize, &qtexport->audioCodecMaxOutputPacketSize);
						else
							qtexport->audioCodecMaxOutputPacketSize = qtexport->audioOutputFormat.mBytesPerPacket;
						
						qtexport->audioInputBuffer = MEM_mallocN(AUDIOOUTPUTBUFFERSIZE, "qt_audio_inputPacket");
						qtexport->audioOutputBuffer = MEM_mallocN(AUDIOOUTPUTBUFFERSIZE, "qt_audio_outputPacket");
						qtexport->audioOutputPktDesc = MEM_mallocN(sizeof(AudioStreamPacketDescription)*AUDIOOUTPUTBUFFERSIZE/qtexport->audioCodecMaxOutputPacketSize,
																   "qt_audio_pktdesc");
					}
				}
			}
			
			if (err == noErr) {
				qtexport->videoTempFileName = [[NSString alloc] initWithCString:tmpnam(nil) 
															 encoding:[NSString defaultCStringEncoding]];			
				if (qtexport->videoTempFileName)
					qtexport->movie = [[QTMovie alloc] initToWritableFile:qtexport->videoTempFileName error:&error];

			}
		} else
			qtexport->movie = [[QTMovie alloc] initToWritableFile:qtexport->filename error:&error];
			
		if(qtexport->movie == nil) {
			BKE_report(reports, RPT_ERROR, "Unable to create quicktime movie.");
			success= 0;
			if (qtexport->filename) [qtexport->filename release];
			qtexport->filename = nil;
			if (qtexport->audioFileName) [qtexport->audioFileName release];
			qtexport->audioFileName = nil;
			if (qtexport->videoTempFileName) [qtexport->videoTempFileName release];
			qtexport->videoTempFileName = nil;
			[QTMovie exitQTKitOnThread];
		} else {
			[qtexport->movie retain];
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
			
			if (qtexport->audioFile) {
				/* Init audio input stream */
				AUD_DeviceSpecs specs;

				specs.channels = U.audiochannels;
				specs.format = U.audioformat;
				specs.rate = U.audiorate;
				qtexport->audioInputDevice = AUD_openReadDevice(specs);
				AUD_playDevice(qtexport->audioInputDevice, scene->sound_scene, rd->sfra * rd->frs_sec_base / rd->frs_sec);
								
				qtexport->audioOutputPktPos = 0;
				qtexport->audioTotalExportedFrames = 0;
				qtexport->audioTotalSavedFrames = 0;
				
				qtexport->audioLastFrame = (rd->efra - rd->sfra) * qtexport->audioInputFormat.mSampleRate * rd->frs_sec_base / rd->frs_sec;
			}
		}
	}
	
	[pool drain];

	return success;
}


int append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty, ReportList *reports)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSBitmapImageRep *blBitmapFormatImage;
	NSImage *frameImage;
	OSStatus err = noErr;
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
		return 0;
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
	
	
	if (qtexport->audioFile) {
		UInt32 audioPacketsConverted;
		/* Append audio */
		while (qtexport->audioTotalExportedFrames < qtexport->audioLastFrame) {	

			qtexport->audioBufferList.mNumberBuffers = 1;
			qtexport->audioBufferList.mBuffers[0].mNumberChannels = qtexport->audioOutputFormat.mChannelsPerFrame;
			qtexport->audioBufferList.mBuffers[0].mDataByteSize = AUDIOOUTPUTBUFFERSIZE;
			qtexport->audioBufferList.mBuffers[0].mData = qtexport->audioOutputBuffer;
			audioPacketsConverted = AUDIOOUTPUTBUFFERSIZE / qtexport->audioCodecMaxOutputPacketSize;
			
			err = AudioConverterFillComplexBuffer(qtexport->audioConverter, AudioConverterInputCallback,
											NULL, &audioPacketsConverted, &qtexport->audioBufferList, qtexport->audioOutputPktDesc);
			if (audioPacketsConverted) {
				AudioFileWritePackets(qtexport->audioFile, false, qtexport->audioBufferList.mBuffers[0].mDataByteSize,
									  qtexport->audioOutputPktDesc, qtexport->audioOutputPktPos, &audioPacketsConverted, qtexport->audioOutputBuffer);
				qtexport->audioOutputPktPos += audioPacketsConverted;
				
				if (qtexport->audioOutputFormat.mFramesPerPacket) { 
					// this is the common case: format has constant frames per packet
					qtexport->audioTotalSavedFrames += (audioPacketsConverted * qtexport->audioOutputFormat.mFramesPerPacket);
				} else {
					unsigned int i;
					// if there are variable frames per packet, then we have to do this for each packeet
					for (i = 0; i < audioPacketsConverted; ++i)
						qtexport->audioTotalSavedFrames += qtexport->audioOutputPktDesc[i].mVariableFramesInPacket;
				}
				
				
			}
			else {
				//Error getting audio packets
				BKE_reportf(reports, RPT_ERROR, "Unable to get further audio packets from frame %i, error = 0x%x",(int)qtexport->audioTotalExportedFrames,err);
				break;
			}

		}
	}
	[pool drain];	

	return 1;
}


void end_qt(void)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	if (qtexport->movie) {
		
		if (qtexport->audioFile)
		{
			NSDictionary *dict = nil;
			QTMovie *audioTmpMovie = nil;
			NSError *error;
			NSFileManager *fileManager;
			
			/* Mux video and audio then save file */
			
			/* Write last frames for VBR files */
			if (qtexport->audioOutputFormat.mBitsPerChannel == 0) {
				OSStatus err = noErr;
				AudioConverterPrimeInfo primeInfo;
				UInt32 primeSize = sizeof(primeInfo);
				
				err = AudioConverterGetProperty(qtexport->audioConverter, kAudioConverterPrimeInfo, &primeSize, &primeInfo);
				if (err == noErr) {
					// there's priming to write out to the file
					AudioFilePacketTableInfo pti;
					pti.mPrimingFrames = primeInfo.leadingFrames;
					pti.mRemainderFrames = primeInfo.trailingFrames;
					pti.mNumberValidFrames = qtexport->audioTotalSavedFrames - pti.mPrimingFrames - pti.mRemainderFrames;
					AudioFileSetProperty(qtexport->audioFile, kAudioFilePropertyPacketTableInfo, sizeof(pti), &pti);
				}
				
			}
			
			write_cookie(qtexport->audioConverter, qtexport->audioFile);
			AudioConverterDispose(qtexport->audioConverter);
			AudioFileClose(qtexport->audioFile);
			AUD_closeReadDevice(qtexport->audioInputDevice);
			qtexport->audioFile = NULL;
			qtexport->audioInputDevice = NULL;
			MEM_freeN(qtexport->audioInputBuffer);
			MEM_freeN(qtexport->audioOutputBuffer);
			MEM_freeN(qtexport->audioOutputPktDesc);
			
			/* Reopen audio file and merge it */
			audioTmpMovie = [QTMovie movieWithFile:qtexport->audioFileName error:&error];
			if (audioTmpMovie) {
				NSArray *audioTracks = [audioTmpMovie tracksOfMediaType:QTMediaTypeSound];
				QTTrack *audioTrack = nil;
				if( [audioTracks count] > 0 )
				{
					audioTrack = [audioTracks objectAtIndex:0];
				}
			
				if( audioTrack )
				{
					QTTimeRange totalRange;
					totalRange.time = QTZeroTime;
					totalRange.duration = [[audioTmpMovie attributeForKey:QTMovieDurationAttribute] QTTimeValue];
					
					[qtexport->movie insertSegmentOfTrack:audioTrack timeRange:totalRange atTime:QTZeroTime];
				}
			}
			
			/* Save file */
			dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] 
											   forKey:QTMovieFlatten];

			if (dict) {
				[qtexport->movie writeToFile:qtexport->filename withAttributes:dict];
			}
			
			/* Delete temp files */
			fileManager = [[NSFileManager alloc] init];
			[fileManager removeItemAtPath:qtexport->audioFileName error:&error];
			[fileManager removeItemAtPath:qtexport->videoTempFileName error:&error];
			[fileManager release];
		}
		else {
			/* Flush update of the movie file */
			[qtexport->movie updateMovieFile];
			
			[qtexport->movie invalidate];
		}
		
		/* Clean up movie structure */
		if (qtexport->filename) [qtexport->filename release];
		qtexport->filename = nil;
		if (qtexport->audioFileName) [qtexport->audioFileName release];
		qtexport->audioFileName = nil;
		if (qtexport->videoTempFileName) [qtexport->videoTempFileName release];
		qtexport->videoTempFileName = nil;
		[qtexport->frameAttributes release];
		[qtexport->movie release];
	}
	
	[QTMovie exitQTKitOnThread];

	if(qtexport) {
		MEM_freeN(qtexport);
		qtexport = NULL;
	}
	[pool drain];
}


void free_qtcomponentdata(void) {
}

void quicktime_verify_image_type(RenderData *rd, ImageFormatData *imf)
{
	if (imf->imtype == R_IMF_IMTYPE_QUICKTIME) {
		if ((rd->qtcodecsettings.codecType<= 0) ||
			(rd->qtcodecsettings.codecSpatialQuality <0) ||
			(rd->qtcodecsettings.codecSpatialQuality > 100)) {
			
			rd->qtcodecsettings.codecType = kJPEGCodecType;
			rd->qtcodecsettings.codecSpatialQuality = (codecHighQuality*100)/codecLosslessQuality;
		}
		if ((rd->qtcodecsettings.audioSampleRate < 21000) ||
			(rd->qtcodecsettings.audioSampleRate > 193000)) 
			rd->qtcodecsettings.audioSampleRate = 48000;
		
		if (rd->qtcodecsettings.audioBitDepth == 0)
			rd->qtcodecsettings.audioBitDepth = AUD_FORMAT_S16;
		
		if (rd->qtcodecsettings.audioBitRate == 0)
			rd->qtcodecsettings.audioBitRate = 256000;
	}
}

#endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

