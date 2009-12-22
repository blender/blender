/**
 * $Id$
 *
 * quicktime_export.c
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)

#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "BLI_blenlib.h"

#include "BLO_sys_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

#include "quicktime_import.h"
#include "quicktime_export.h"

#ifdef _WIN32
#include <QTML.h>
#include <Movies.h>
#include <QuickTimeComponents.h>
#include <TextUtils.h> 
#endif /* _WIN32 */

#ifdef __APPLE__
/* evil */
#ifndef __AIFF__
#define __AIFF__
#endif
#include <QuickTime/Movies.h>
#include <QuickTime/QuickTimeComponents.h>
#include <fcntl.h> /* open() */
#include <unistd.h> /* close() */
#include <sys/stat.h> /* file permissions */
#endif /* __APPLE__ */

#define	kMyCreatorType	FOUR_CHAR_CODE('TVOD')
#define	kTrackStart		0
#define	kMediaStart		0

static void QT_StartAddVideoSamplesToMedia (const Rect *trackFrame, int rectx, int recty, struct ReportList *reports);
static void QT_DoAddVideoSamplesToMedia (int frame, int *pixels, int rectx, int recty, struct ReportList *reports);
static void QT_EndAddVideoSamplesToMedia (void);
static void QT_CreateMyVideoTrack (int rectx, int recty, struct ReportList *reports);
static void QT_EndCreateMyVideoTrack (struct ReportList *reports);
static void check_renderbutton_framerate(struct RenderData *rd, struct ReportList *reports);
static int get_qtcodec_settings(struct RenderData *rd, struct ReportList *reports);

typedef struct QuicktimeExport {

	FSSpec		theSpec;
	short		resRefNum;
	Str255		qtfilename;

	Media		theMedia;
	Movie		theMovie;
	Track		theTrack;

	GWorldPtr			theGWorld;
	PixMapHandle		thePixMap;
	ImageDescription	**anImageDescription;

	ImBuf		*ibuf;	//imagedata for Quicktime's Gworld
	ImBuf		*ibuf2;	//copy of renderdata, to be Y-flipped

} QuicktimeExport;

typedef struct QuicktimeComponentData {

	ComponentInstance	theComponent;
	SCTemporalSettings  gTemporalSettings;
	SCSpatialSettings   gSpatialSettings;
	SCDataRateSettings  aDataRateSetting;
	TimeValue			duration;
	long				kVideoTimeScale;

} QuicktimeComponentData;

static struct QuicktimeExport *qtexport;
static struct QuicktimeComponentData *qtdata;

static int	sframe;

/* RNA functions */

static QuicktimeCodecTypeDesc qtCodecList[] = {
	{kRawCodecType, 1, "Uncompressed"},
	{kJPEGCodecType, 2, "JPEG"},
	{kMotionJPEGACodecType, 3, "M-JPEG A"},
	{kMotionJPEGBCodecType, 4, "M-JPEG B"},
	{kDVCPALCodecType, 5, "DV PAL"},
	{kDVCNTSCCodecType, 6, "DV/DVCPRO NTSC"},
	{kDVCPROHD720pCodecType, 7, "DVCPRO HD 720p"},
	{kDVCPROHD1080i50CodecType, 8, "DVCPRO HD 1080i50"},
	{kDVCPROHD1080i60CodecType, 9, "DVCPRO HD 1080i60"},
	{kMPEG4VisualCodecType, 10, "MPEG4"},
	{kH263CodecType, 11, "H.263"},
	{kH264CodecType, 12, "H.264"},
	{0,0,NULL}};

static int qtCodecCount = 12;

int quicktime_get_num_codecs() {
	return qtCodecCount;
}

QuicktimeCodecTypeDesc* quicktime_get_codecType_desc(int indexValue) {
	if ((indexValue>=0) && (indexValue < qtCodecCount))
		return &qtCodecList[indexValue];
	else
		return NULL;
}

int quicktime_rnatmpvalue_from_codectype(int codecType) {
	int i;
	for (i=0;i<qtCodecCount;i++) {
		if (qtCodecList[i].codecType == codecType)
			return qtCodecList[i].rnatmpvalue;
	}
	
	return 0;
}

int quicktime_codecType_from_rnatmpvalue(int rnatmpvalue) {
	int i;
	for (i=0;i<qtCodecCount;i++) {
		if (qtCodecList[i].rnatmpvalue == rnatmpvalue)
			return qtCodecList[i].codecType;
	}
	
	return 0;	
}



static void CheckError(OSErr err, char *msg, ReportList *reports)
{
	if(err != noErr) {
		printf("%s: %d\n", msg, err);
		BKE_reportf(reports, RPT_ERROR, "%s: %d", msg, err);
	}
}


static OSErr QT_SaveCodecSettingsToScene(RenderData *rd)
{	
	QTAtomContainer		myContainer = NULL;
	ComponentResult		myErr = noErr;
	Ptr					myPtr;
	long				mySize = 0;

	CodecInfo			ci;

	QuicktimeCodecData *qcd = rd->qtcodecdata;
	
	// check if current scene already has qtcodec settings, and clear them
	if (qcd) {
		free_qtcodecdata(qcd);
	} else {
		qcd = rd->qtcodecdata = MEM_callocN(sizeof(QuicktimeCodecData), "QuicktimeCodecData");
	}

	// obtain all current codec settings
	SCSetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
	SCSetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
	SCSetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);

	// retreive codecdata from quicktime in a atomcontainer
	myErr = SCGetSettingsAsAtomContainer(qtdata->theComponent,  &myContainer);
	if (myErr != noErr) {
		printf("Quicktime: SCGetSettingsAsAtomContainer failed\n"); 
		goto bail;
	}

	// get the size of the atomcontainer
	mySize = GetHandleSize((Handle)myContainer);

	// lock and convert the atomcontainer to a *valid* pointer
	QTLockContainer(myContainer);
	myPtr = *(Handle)myContainer;

	// copy the Quicktime data into the blender qtcodecdata struct
	if (myPtr) {
		qcd->cdParms = MEM_mallocN(mySize, "qt.cdParms");
		memcpy(qcd->cdParms, myPtr, mySize);
		qcd->cdSize = mySize;

		GetCodecInfo (&ci, qtdata->gSpatialSettings.codecType, 0);
	} else {
		printf("Quicktime: QT_SaveCodecSettingsToScene failed\n"); 
	}

	QTUnlockContainer(myContainer);

bail:
	if (myContainer != NULL)
		QTDisposeAtomContainer(myContainer);
		
	return((OSErr)myErr);
}


static OSErr QT_GetCodecSettingsFromScene(RenderData *rd)
{	
	Handle				myHandle = NULL;
	ComponentResult		myErr = noErr;

	QuicktimeCodecData *qcd = rd->qtcodecdata;

	// if there is codecdata in the blendfile, convert it to a Quicktime handle 
	if (qcd) {
		myHandle = NewHandle(qcd->cdSize);
		PtrToHand( qcd->cdParms, &myHandle, qcd->cdSize);
	}
		
	// restore codecsettings to the quicktime component
	if(qcd->cdParms && qcd->cdSize) {
		myErr = SCSetSettingsFromAtomContainer((GraphicsExportComponent)qtdata->theComponent, (QTAtomContainer)myHandle);
		if (myErr != noErr) {
			printf("Quicktime: SCSetSettingsFromAtomContainer failed\n"); 
			goto bail;
		}

		// update runtime codecsettings for use with the codec dialog
		SCGetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
		SCGetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
		SCGetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);


		//Fill the render QuicktimeCodecSettigns struct
		rd->qtcodecsettings.codecTemporalQuality = (qtdata->gTemporalSettings.temporalQuality * 100) / codecLosslessQuality;
		//Do not override scene frame rate (qtdata->gTemporalSettings.framerate)
		rd->qtcodecsettings.keyFrameRate = qtdata->gTemporalSettings.keyFrameRate;
		
		rd->qtcodecsettings.codecType = qtdata->gSpatialSettings.codecType;
		rd->qtcodecsettings.codec = (int)qtdata->gSpatialSettings.codec;
		rd->qtcodecsettings.colorDepth = qtdata->gSpatialSettings.depth;
		rd->qtcodecsettings.codecSpatialQuality = (qtdata->gSpatialSettings.spatialQuality * 100) / codecLosslessQuality;
		
		rd->qtcodecsettings.bitRate = qtdata->aDataRateSetting.dataRate;
		rd->qtcodecsettings.minSpatialQuality = (qtdata->aDataRateSetting.minSpatialQuality * 100) / codecLosslessQuality;
		rd->qtcodecsettings.minTemporalQuality = (qtdata->aDataRateSetting.minTemporalQuality * 100) / codecLosslessQuality;
		//Frame duration is already known (qtdata->aDataRateSetting.frameDuration)
		
	} else {
		printf("Quicktime: QT_GetCodecSettingsFromScene failed\n"); 
	}
bail:
	if (myHandle != NULL)
		DisposeHandle(myHandle);
		
	return((OSErr)myErr);
}


static OSErr QT_AddUserDataTextToMovie (Movie theMovie, char *theText, OSType theType)
{
	UserData					myUserData = NULL;
	Handle						myHandle = NULL;
	long						myLength = strlen(theText);
	OSErr						myErr = noErr;

	// get the movie's user data list
	myUserData = GetMovieUserData(theMovie);
	if (myUserData == NULL)
		return(paramErr);
	
	// copy the specified text into a new handle
	myHandle = NewHandleClear(myLength);
	if (myHandle == NULL)
		return(MemError());

	BlockMoveData(theText, *myHandle, myLength);

	// add the data to the movie's user data
	myErr = AddUserDataText(myUserData, myHandle, theType, 1, (short)GetScriptManagerVariable(smRegionCode));

	// clean up
	DisposeHandle(myHandle);
	return(myErr);
}


static void QT_CreateMyVideoTrack(int rectx, int recty, ReportList *reports)
{
	OSErr err = noErr;
	Rect trackFrame;
//	MatrixRecord myMatrix;

	trackFrame.top = 0;
	trackFrame.left = 0;
	trackFrame.bottom = recty;
	trackFrame.right = rectx;
	
	qtexport->theTrack = NewMovieTrack (qtexport->theMovie, 
							FixRatio(trackFrame.right,1),
							FixRatio(trackFrame.bottom,1), 
							0);
	CheckError( GetMoviesError(), "NewMovieTrack error", reports );

//	SetIdentityMatrix(&myMatrix);
//	ScaleMatrix(&myMatrix, fixed1, Long2Fix(-1), 0, 0);
//	TranslateMatrix(&myMatrix, 0, Long2Fix(trackFrame.bottom));
//	SetMovieMatrix(qtexport->theMovie, &myMatrix);

	qtexport->theMedia = NewTrackMedia (qtexport->theTrack,
							VideoMediaType,
							qtdata->kVideoTimeScale,
							nil,
							0);
	CheckError( GetMoviesError(), "NewTrackMedia error", reports );

	err = BeginMediaEdits (qtexport->theMedia);
	CheckError( err, "BeginMediaEdits error", reports );

	QT_StartAddVideoSamplesToMedia (&trackFrame, rectx, recty, reports);
} 


static void QT_EndCreateMyVideoTrack(ReportList *reports)
{
	OSErr err = noErr;

	QT_EndAddVideoSamplesToMedia ();

	err = EndMediaEdits (qtexport->theMedia);
	CheckError( err, "EndMediaEdits error", reports );

	err = InsertMediaIntoTrack (qtexport->theTrack,
								kTrackStart,/* track start time */
								kMediaStart,/* media start time */
								GetMediaDuration (qtexport->theMedia),
								fixed1);
	CheckError( err, "InsertMediaIntoTrack error", reports );
} 


static void QT_StartAddVideoSamplesToMedia (const Rect *trackFrame, int rectx, int recty, ReportList *reports)
{
	SCTemporalSettings gTemporalSettings;
	OSErr err = noErr;

	qtexport->ibuf = IMB_allocImBuf (rectx, recty, 32, IB_rect, 0);
	qtexport->ibuf2 = IMB_allocImBuf (rectx, recty, 32, IB_rect, 0);

	err = NewGWorldFromPtr( &qtexport->theGWorld,
							k32ARGBPixelFormat,
							trackFrame,
							NULL, NULL, 0,
							(Ptr)qtexport->ibuf->rect,
							rectx * 4 );
	CheckError (err, "NewGWorldFromPtr error", reports);

	qtexport->thePixMap = GetGWorldPixMap(qtexport->theGWorld);
	LockPixels(qtexport->thePixMap);

	SCDefaultPixMapSettings (qtdata->theComponent, qtexport->thePixMap, true);

	// workaround for crash with H.264, which requires an upgrade to
	// the new callback based api for proper encoding, but that's not
	// really compatible with rendering out frames sequentially
	gTemporalSettings = qtdata->gTemporalSettings;
	if(qtdata->gSpatialSettings.codecType == kH264CodecType) {
		if(gTemporalSettings.temporalQuality != codecMinQuality) {
			fprintf(stderr, "Only minimum quality compression supported for QuickTime H.264.\n");
			gTemporalSettings.temporalQuality = codecMinQuality;
		}
	}

	SCSetInfo(qtdata->theComponent, scTemporalSettingsType,	&gTemporalSettings);
	SCSetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
	SCSetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);

	err = SCCompressSequenceBegin(qtdata->theComponent, qtexport->thePixMap, NULL, &qtexport->anImageDescription); 
	CheckError (err, "SCCompressSequenceBegin error", reports );
}


static void QT_DoAddVideoSamplesToMedia (int frame, int *pixels, int rectx, int recty, ReportList *reports)
{
	OSErr	err = noErr;
	Rect	imageRect;

	int		index;
	int		boxsize;
	unsigned char *from, *to;

	short	syncFlag;
	long	dataSize;
	Handle	compressedData;
	Ptr		myPtr;


	//copy and flip renderdata
	memcpy(qtexport->ibuf2->rect, pixels, 4*rectx*recty);
	IMB_flipy(qtexport->ibuf2);

	//get pointers to parse bitmapdata
	myPtr = GetPixBaseAddr(qtexport->thePixMap);
	imageRect = (**qtexport->thePixMap).bounds;

	from = (unsigned char *) qtexport->ibuf2->rect;
	to = (unsigned char *) myPtr;

	//parse RGBA bitmap into Quicktime's ARGB GWorld
	boxsize = rectx * recty;
	for( index = 0; index < boxsize; index++) {
		to[0] = from[3];
		to[1] = from[0];
		to[2] = from[1];
		to[3] = from[2];
		to +=4, from += 4;
	}

	err = SCCompressSequenceFrame(qtdata->theComponent,
		qtexport->thePixMap,
		&imageRect,
		&compressedData,
		&dataSize,
		&syncFlag);
	CheckError(err, "SCCompressSequenceFrame error", reports);

	err = AddMediaSample(qtexport->theMedia,
		compressedData,
		0,
		dataSize,
		qtdata->duration,
		(SampleDescriptionHandle)qtexport->anImageDescription,
		1,
		syncFlag,
		NULL);
	CheckError(err, "AddMediaSample error", reports);

	printf ("added frame %3d (frame %3d in movie): ", frame, frame-sframe);
}


static void QT_EndAddVideoSamplesToMedia (void)
{
	SCCompressSequenceEnd(qtdata->theComponent);

	UnlockPixels(qtexport->thePixMap);
	if (qtexport->theGWorld)
		DisposeGWorld (qtexport->theGWorld);

	if (qtexport->ibuf)
		IMB_freeImBuf(qtexport->ibuf);

	if (qtexport->ibuf2)
		IMB_freeImBuf(qtexport->ibuf2);
} 


void makeqtstring (RenderData *rd, char *string) {
	char txt[64];

	if (string==0) return;

	strcpy(string, rd->pic);
	BLI_convertstringcode(string, G.sce);

	BLI_make_existing_file(string);

	if (BLI_strcasecmp(string + strlen(string) - 4, ".mov")) {
		sprintf(txt, "%04d_%04d.mov", (rd->sfra) , (rd->efra) );
		strcat(string, txt);
	}
}


int start_qt(struct Scene *scene, struct RenderData *rd, int rectx, int recty, ReportList *reports) {
	OSErr err = noErr;

	char name[2048];
	char theFullPath[255];

#ifdef __APPLE__
	int		myFile;
	FSRef	myRef;
#else
	char	*qtname;
#endif
	int success= 1;

	if(qtexport == NULL) qtexport = MEM_callocN(sizeof(QuicktimeExport), "QuicktimeExport");

	if(qtdata) {
		if(qtdata->theComponent) CloseComponent(qtdata->theComponent);
		free_qtcomponentdata();
	}

	qtdata = MEM_callocN(sizeof(QuicktimeComponentData), "QuicktimeCodecDataExt");

	if(rd->qtcodecdata == NULL || rd->qtcodecdata->cdParms == NULL) {
		get_qtcodec_settings(rd, reports);
	} else {
		qtdata->theComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);

		QT_GetCodecSettingsFromScene(rd, reports);
		check_renderbutton_framerate(rd, reports);
	}
	
	sframe = (rd->sfra);

	makeqtstring(rd, name);

#ifdef __APPLE__
	EnterMoviesOnThread(0);
	sprintf(theFullPath, "%s", name);

	/* hack: create an empty file to make FSPathMakeRef() happy */
	myFile = open(theFullPath, O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRUSR|S_IWUSR);
	if (myFile < 0) {
		printf("error while creating file!\n");
		/* do something? */
	}
	close(myFile);
	err = FSPathMakeRef((const UInt8 *)theFullPath, &myRef, 0);
	CheckError(err, "FsPathMakeRef error", reports);
	err = FSGetCatalogInfo(&myRef, kFSCatInfoNone, NULL, NULL, &qtexport->theSpec, NULL);
	CheckError(err, "FsGetCatalogInfoRef error", reports);
#endif
#ifdef _WIN32
	qtname = get_valid_qtname(name);
	sprintf(theFullPath, "%s", qtname);
	strcpy(name, qtname);
	MEM_freeN(qtname);
	
	CopyCStringToPascal(theFullPath, qtexport->qtfilename);
	err = FSMakeFSSpec(0, 0L, qtexport->qtfilename, &qtexport->theSpec);
#endif

	err = CreateMovieFile (&qtexport->theSpec, 
						kMyCreatorType,
						smCurrentScript, 
						createMovieFileDeleteCurFile | createMovieFileDontCreateResFile,
						&qtexport->resRefNum, 
						&qtexport->theMovie );
	CheckError(err, "CreateMovieFile error", reports);

	if(err != noErr) {
		BKE_reportf(reports, RPT_ERROR, "Unable to create Quicktime movie: %s", name);
		success= 0;
#ifdef __APPLE__
		ExitMoviesOnThread();
#endif
	} else {
		printf("Created QuickTime movie: %s\n", name);

		QT_CreateMyVideoTrack(rectx, recty, reports);
	}

	return success;
}


int append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty, ReportList *reports) {
	QT_DoAddVideoSamplesToMedia(frame, pixels, rectx, recty, reports);
	return 1;
}


void end_qt(void) {
	OSErr err = noErr;
	short resId = movieInDataForkResID;

	if(qtexport->theMovie) {
		QT_EndCreateMyVideoTrack(NULL);

		err = AddMovieResource (qtexport->theMovie, qtexport->resRefNum, &resId, qtexport->qtfilename);
		CheckError(err, "AddMovieResource error", NULL);

		err = QT_AddUserDataTextToMovie(qtexport->theMovie, "Made with Blender", kUserDataTextInformation);
		CheckError(err, "AddUserDataTextToMovie error", NULL);

		err = UpdateMovieResource(qtexport->theMovie, qtexport->resRefNum, resId, qtexport->qtfilename);
		CheckError(err, "UpdateMovieResource error", NULL);

		if(qtexport->resRefNum) CloseMovieFile(qtexport->resRefNum);

		DisposeMovie(qtexport->theMovie);

		printf("Finished QuickTime movie.\n");
	}

	ExitMoviesOnThread();
	
	if(qtexport) {
		MEM_freeN(qtexport);
		qtexport = NULL;
	}
}


void free_qtcomponentdata(void) {
	if(qtdata) {
		if(qtdata->theComponent) CloseComponent(qtdata->theComponent);
		MEM_freeN(qtdata);
		qtdata = NULL;
	}
}


static void check_renderbutton_framerate(RenderData *rd, ReportList *reports) 
{
	// to keep float framerates consistent between the codec dialog and frs/sec button.
	OSErr	err;	

	err = SCGetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
	CheckError(err, "SCGetInfo fr error", reports);

	if( (rd->frs_sec == 24 || rd->frs_sec == 30 || rd->frs_sec == 60) &&
		(qtdata->gTemporalSettings.frameRate == 1571553 ||
		 qtdata->gTemporalSettings.frameRate == 1964113 ||
		 qtdata->gTemporalSettings.frameRate == 3928227)) {;} 
	else {
		if (rd->frs_sec_base > 0)
			qtdata->gTemporalSettings.frameRate = 
			((float)(rd->frs_sec << 16) / rd->frs_sec_base) ;
	}
	
	err = SCSetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
	CheckError( err, "SCSetInfo error", reports );

	if(qtdata->gTemporalSettings.frameRate == 1571553) {			// 23.98 fps
		qtdata->kVideoTimeScale = 24000;
		qtdata->duration = 1001;
	} else if (qtdata->gTemporalSettings.frameRate == 1964113) {	// 29.97 fps
		qtdata->kVideoTimeScale = 30000;
		qtdata->duration = 1001;
	} else if (qtdata->gTemporalSettings.frameRate == 3928227) {	// 59.94 fps
		qtdata->kVideoTimeScale = 60000;
		qtdata->duration = 1001;
	} else {
		qtdata->kVideoTimeScale = (qtdata->gTemporalSettings.frameRate >> 16) * 100;
		qtdata->duration = 100;
	}
}

void quicktime_verify_image_type(RenderData *rd)
{
	if (rd->imtype == R_QUICKTIME) {
		if ((rd->qtcodecsettings.codecType== 0) ||
			(rd->qtcodecsettings.codecSpatialQuality <0) ||
			(rd->qtcodecsettings.codecSpatialQuality > 100)) {
			
			rd->qtcodecsettings.codecType = kJPEGCodecType;
			rd->qtcodecsettings.codec = (int)anyCodec;
			rd->qtcodecsettings.codecSpatialQuality = (codecHighQuality*100)/codecLosslessQuality;
			rd->qtcodecsettings.codecTemporalQuality = (codecHighQuality*100)/codecLosslessQuality;
			rd->qtcodecsettings.keyFrameRate = 25;
			rd->qtcodecsettings.bitRate = 5000000; //5 Mbps
		}
	}
}

int get_qtcodec_settings(RenderData *rd, ReportList *reports) 
{
	OSErr err = noErr;
		// erase any existing codecsetting
	if(qtdata) {
		if(qtdata->theComponent) CloseComponent(qtdata->theComponent);
		free_qtcomponentdata();
	}

	// allocate new
	qtdata = MEM_callocN(sizeof(QuicktimeComponentData), "QuicktimeComponentData");
	qtdata->theComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);

	// get previous selected codecsetting, from qtatom or detailed settings
	if(rd->qtcodecdata && rd->qtcodecdata->cdParms) {
		QT_GetCodecSettingsFromScene(rd);
	} else {
		SCGetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
		SCGetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
		SCGetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);

		qtdata->gSpatialSettings.codecType = rd->qtcodecsettings.codecType;
		qtdata->gSpatialSettings.codec = (CodecComponent)rd->qtcodecsettings.codec;      
		qtdata->gSpatialSettings.spatialQuality = (rd->qtcodecsettings.codecSpatialQuality * codecLosslessQuality) /100;
		qtdata->gTemporalSettings.temporalQuality = (rd->qtcodecsettings.codecTemporalQuality * codecLosslessQuality) /100;
		qtdata->gTemporalSettings.keyFrameRate = rd->qtcodecsettings.keyFrameRate;   
		qtdata->aDataRateSetting.dataRate = rd->qtcodecsettings.bitRate;
		qtdata->gSpatialSettings.depth = rd->qtcodecsettings.colorDepth;
		qtdata->aDataRateSetting.minSpatialQuality = (rd->qtcodecsettings.minSpatialQuality * codecLosslessQuality) / 100;
		qtdata->aDataRateSetting.minTemporalQuality = (rd->qtcodecsettings.minTemporalQuality * codecLosslessQuality) / 100;
		
		qtdata->aDataRateSetting.frameDuration = rd->frs_sec;
		SetMovieTimeScale(qtexport->theMovie, rd->frs_sec_base*1000);
		
		
		err = SCSetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
		CheckError(err, "SCSetInfo1 error", reports);
		err = SCSetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
		CheckError(err, "SCSetInfo2 error", reports);
		err = SCSetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
		CheckError(err, "SCSetInfo3 error", reports);
	}

	check_renderbutton_framerate(rd, reports);
	
	return err;
}

static int request_qtcodec_settings(bContext *C, wmOperator *op)
{
	OSErr	err = noErr;
	Scene *scene = CTX_data_scene(C);
	RenderData *rd = &scene->r;

	// erase any existing codecsetting
	if(qtdata) {
		if(qtdata->theComponent) CloseComponent(qtdata->theComponent);
		free_qtcomponentdata();
	}
	
	// allocate new
	qtdata = MEM_callocN(sizeof(QuicktimeComponentData), "QuicktimeComponentData");
	qtdata->theComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
	
	// get previous selected codecsetting, from qtatom or detailed settings
	if(rd->qtcodecdata && rd->qtcodecdata->cdParms) {
		QT_GetCodecSettingsFromScene(rd);
	} else {
		SCGetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
		SCGetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
		SCGetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
		
		qtdata->gSpatialSettings.codecType = rd->qtcodecsettings.codecType;
		qtdata->gSpatialSettings.codec = (CodecComponent)rd->qtcodecsettings.codec;      
		qtdata->gSpatialSettings.spatialQuality = (rd->qtcodecsettings.codecSpatialQuality * codecLosslessQuality) /100;
		qtdata->gTemporalSettings.temporalQuality = (rd->qtcodecsettings.codecTemporalQuality * codecLosslessQuality) /100;
		qtdata->gTemporalSettings.keyFrameRate = rd->qtcodecsettings.keyFrameRate;
		qtdata->gTemporalSettings.frameRate = ((float)(rd->frs_sec << 16) / rd->frs_sec_base);
		qtdata->aDataRateSetting.dataRate = rd->qtcodecsettings.bitRate;
		qtdata->gSpatialSettings.depth = rd->qtcodecsettings.colorDepth;
		qtdata->aDataRateSetting.minSpatialQuality = (rd->qtcodecsettings.minSpatialQuality * codecLosslessQuality) / 100;
		qtdata->aDataRateSetting.minTemporalQuality = (rd->qtcodecsettings.minTemporalQuality * codecLosslessQuality) / 100;
		
		qtdata->aDataRateSetting.frameDuration = rd->frs_sec;		
		
		err = SCSetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
		CheckError(err, "SCSetInfo1 error", op->reports);
		err = SCSetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
		CheckError(err, "SCSetInfo2 error", op->reports);
		err = SCSetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
		CheckError(err, "SCSetInfo3 error", op->reports);
	}
		// put up the dialog box - it needs to be called from the main thread
	err = SCRequestSequenceSettings(qtdata->theComponent);
 
	if (err == scUserCancelled) {
		return OPERATOR_FINISHED;
	}

		// update runtime codecsettings for use with the codec dialog
	SCGetInfo(qtdata->theComponent, scDataRateSettingsType,	&qtdata->aDataRateSetting);
	SCGetInfo(qtdata->theComponent, scSpatialSettingsType,	&qtdata->gSpatialSettings);
	SCGetInfo(qtdata->theComponent, scTemporalSettingsType,	&qtdata->gTemporalSettings);
	
	
		//Fill the render QuicktimeCodecSettings struct
	rd->qtcodecsettings.codecTemporalQuality = (qtdata->gTemporalSettings.temporalQuality * 100) / codecLosslessQuality;
		//Do not override scene frame rate (qtdata->gTemporalSettings.framerate)
	rd->qtcodecsettings.keyFrameRate = qtdata->gTemporalSettings.keyFrameRate;
	
	rd->qtcodecsettings.codecType = qtdata->gSpatialSettings.codecType;
	rd->qtcodecsettings.codec = (int)qtdata->gSpatialSettings.codec;
	rd->qtcodecsettings.colorDepth = qtdata->gSpatialSettings.depth;
	rd->qtcodecsettings.codecSpatialQuality = (qtdata->gSpatialSettings.spatialQuality * 100) / codecLosslessQuality;
	
	rd->qtcodecsettings.bitRate = qtdata->aDataRateSetting.dataRate;
	rd->qtcodecsettings.minSpatialQuality = (qtdata->aDataRateSetting.minSpatialQuality * 100) / codecLosslessQuality;
	rd->qtcodecsettings.minTemporalQuality = (qtdata->aDataRateSetting.minTemporalQuality * 100) / codecLosslessQuality;
		//Frame duration is already known (qtdata->aDataRateSetting.frameDuration)
	
	QT_SaveCodecSettingsToScene(rd);

	// framerate jugglin'
	if(qtdata->gTemporalSettings.frameRate == 1571553) {			// 23.98 fps
		qtdata->kVideoTimeScale = 24000;
		qtdata->duration = 1001;

		rd->frs_sec = 24;
		rd->frs_sec_base = 1.001;
	} else if (qtdata->gTemporalSettings.frameRate == 1964113) {	// 29.97 fps
		qtdata->kVideoTimeScale = 30000;
		qtdata->duration = 1001;

		rd->frs_sec = 30;
		rd->frs_sec_base = 1.001;
	} else if (qtdata->gTemporalSettings.frameRate == 3928227) {	// 59.94 fps
		qtdata->kVideoTimeScale = 60000;
		qtdata->duration = 1001;

		rd->frs_sec = 60;
		rd->frs_sec_base = 1.001;
	} else {
		double fps = qtdata->gTemporalSettings.frameRate;

		qtdata->kVideoTimeScale = 60000;
		qtdata->duration = qtdata->kVideoTimeScale / (qtdata->gTemporalSettings.frameRate / 65536);

		if ((qtdata->gTemporalSettings.frameRate & 0xffff) == 0) {
			rd->frs_sec = fps / 65536;
			rd->frs_sec_base = 1.0;
		} else {
			/* we do our very best... */
			rd->frs_sec = fps  / 65536;
			rd->frs_sec_base = 1.0;
		}
	}

	return OPERATOR_FINISHED;
}

static int ED_operator_setqtcodec(bContext *C)
{
	return G.have_quicktime != FALSE;
}

#if defined(__APPLE__) && defined(GHOST_COCOA)
//Need to set up a Cocoa NSAutoReleasePool to avoid memory leak
//And it must be done in an objC file, so use a GHOST_SystemCocoa.mm function for that
extern int cocoa_request_qtcodec_settings(bContext *C, wmOperator *op);

int fromcocoa_request_qtcodec_settings(bContext *C, wmOperator *op)
{
	return request_qtcodec_settings(C, op);
}
#endif


void SCENE_OT_render_data_set_quicktime_codec(wmOperatorType *ot)
{
	/* identifiers */
    ot->name= "Change codec";
    ot->description= "Change Quicktime codec Settings";
    ot->idname= "SCENE_OT_render_data_set_quicktime_codec";
	
    /* api callbacks */
#if defined(__APPLE__) && defined(GHOST_COCOA)
	ot->exec = cocoa_request_qtcodec_settings;
#else
    ot->exec= request_qtcodec_settings;
#endif
    ot->poll= ED_operator_setqtcodec;
	
    /* flags */
    ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

#endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

