/**
 * $Id$
 *
 * quicktime_export.c
 *
 * Code to create QuickTime Movies with Blender
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 *
 * The Original Code is written by Rob Haarsma (phase)
 *
 * Contributor(s): Stefan Gartner (sgefant)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
/*	
DONE:

*  structurize file & compression data

*  fix 23.98, 29.97, 59.94 framerates
*  fix framerate button
*  fix mac compatibility

*  fix fallthrough to codecselector  // buttons.c
*  fix playback qt movie             // playanim.c
*  fix setting fps thru blenderbutton as well as codec dialog
*  fix saving of compressionsettings

*/

#ifdef WITH_QUICKTIME

#if defined(_WIN32) || defined(__APPLE__)

/************************************************************
*                                                           *
*    INCLUDE FILES                                          *
*                                                           *
*************************************************************/

#include "BKE_global.h"
#include "BKE_scene.h"
#include "BLI_blenlib.h"
#include "BLO_sys_types.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "MEM_guardedalloc.h"
#include "render.h"

#include "quicktime_export.h"

#ifdef _WIN32
#include <FixMath.h>
#include <QTML.h>
#include <TextUtils.h> 
#include <Movies.h>
#include <QuicktimeComponents.h>
#include <MoviesFormat.h>
#endif /* _WIN32 */

#ifdef __APPLE__
#undef NDEBUG
#include <QuickTime/Movies.h>
#include <QuickTime/QuicktimeComponents.h>
#include <fcntl.h> /* open() */
#include <unistd.h> /* close() */
#include <sys/stat.h> /* file permissions */
#endif /* __APPLE__ */


/************************************************************
*                                                           *
*    FUNCTION PROTOTYPES                                    *
*                                                           *
*************************************************************/

static void QT_StartAddVideoSamplesToMedia (const Rect *trackFrame);
static void QT_DoAddVideoSamplesToMedia (int frame);
static void QT_EndAddVideoSamplesToMedia (void);
static void QT_CreateMyVideoTrack (void);
static void QT_EndCreateMyVideoTrack (void);

static void check_renderbutton_framerate(void);

/************************************************************
*                                                           *
*    STRUCTS                                                *
*                                                           *
*************************************************************/

typedef struct _QuicktimeExport {

	FSSpec		theSpec;
	short		resRefNum;
	short		resId;
	short		movieResId;
	Str255		qtfilename;

	Media		theMedia;
	Movie		theMovie;
	Track		theTrack;

	GWorldPtr	theGWorld;
	PixMapHandle	thePixMap;

	ImageDescription	**anImageDescription;
	ImageSequence		anImageSequence;

	ImBuf		*ibuf;	//for Qtime's Gworld
	ImBuf		*ibuf2;	//copy of renderdata, to be Y-flipped

} QuicktimeExport;

typedef struct _QuicktimeCodecDataExt {

	ComponentInstance	theComponent;
	SCTemporalSettings  gTemporalSettings;
	SCSpatialSettings   gSpatialSettings;
	SCDataRateSettings  aDataRateSetting;
	TimeValue			duration;
	long				kVideoTimeScale;

} QuicktimeCodecDataExt;	//qtopts


struct _QuicktimeExport *qte;
struct _QuicktimeCodecDataExt *qcdx;

/************************************************************
*                                                           *
*    VARIABLES                                              *
*                                                           *
*************************************************************/

#define	kMyCreatorType	FOUR_CHAR_CODE('TVOD')
#define	kPixelDepth 	32	/* use 32-bit depth */
#define	kTrackStart		0
#define	kMediaStart		0

static int	sframe;
static char	qtcdname[128];


/************************************************************
*                                                           *
*    SaveExporterSettingsToMem                              *
*                                                           *
*************************************************************/

OSErr SaveExporterSettingsToMem (QuicktimeCodecData *qcd)
{	
	QTAtomContainer		myContainer = NULL;
	ComponentResult		myErr = noErr;
	Ptr					myPtr;
	long				mySize = 0;

	// check if current scene already has qtcodec settings, and erase them
	if (qcd) {
		free_qtcodecdata(qcd);
	} else {
		qcd = G.scene->r.qtcodecdata = MEM_callocN(sizeof(QuicktimeCodecData), "QuicktimeCodecData");
	}

	// obtain all current codec settings
	SCSetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
	SCSetInfo(qcdx->theComponent, scSpatialSettingsType,	&qcdx->gSpatialSettings);
	SCSetInfo(qcdx->theComponent, scDataRateSettingsType,	&qcdx->aDataRateSetting);

	// retreive codecdata from quicktime in a atomcontainer
	myErr = SCGetSettingsAsAtomContainer(qcdx->theComponent,  &myContainer);
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
		sprintf(qcd->qtcodecname, qtcdname);
	} else {
		printf("Quicktime: SaveExporterSettingsToMem failed\n"); 
	}

	QTUnlockContainer(myContainer);

bail:
	if (myContainer != NULL)
		QTDisposeAtomContainer(myContainer);
		
	return((OSErr)myErr);
}

/************************************************************
*                                                           *
*    GetExporterSettingsFromMem                             *
*                                                           *
*************************************************************/

OSErr GetExporterSettingsFromMem (QuicktimeCodecData *qcd)
{	
	Handle				myHandle = NULL;
	ComponentResult		myErr = noErr;
//	CodecInfo ci;
//	char str[255];

	// if there is codecdata in the blendfile, convert it to a Quicktime handle 
	if (qcd) {
		myHandle = NewHandle(qcd->cdSize);
		PtrToHand( qcd->cdParms, &myHandle, qcd->cdSize);
	}
		
	// restore codecsettings to the quicktime component
	if(qcd->cdParms && qcd->cdSize) {
		myErr = SCSetSettingsFromAtomContainer((GraphicsExportComponent)qcdx->theComponent, (QTAtomContainer)myHandle);
		if (myErr != noErr) {
			printf("Quicktime: SCSetSettingsFromAtomContainer failed\n"); 
			goto bail;
		}

		// update runtime codecsettings for use with the codec dialog
		SCGetInfo(qcdx->theComponent, scDataRateSettingsType,	&qcdx->aDataRateSetting);
		SCGetInfo(qcdx->theComponent, scSpatialSettingsType,	&qcdx->gSpatialSettings);
		SCGetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);

//		GetCodecInfo (&ci, qcdx->gSpatialSettings.codecType, 0);
//		CopyPascalStringToC(ci.typeName, str);
//		printf("restored Codec: %s\n", str);
	} else {
		printf("Quicktime: GetExporterSettingsFromMem failed\n"); 
	}
bail:
	if (myHandle != NULL)
		DisposeHandle(myHandle);
		
	return((OSErr)myErr);
}


/************************************************************
*                                                           *
*    CheckError(OSErr err, char *msg)                       *
*                                                           *
*    prints errors in console, doesnt interrupt Blender     *
*                                                           *
*************************************************************/

void CheckError(OSErr err, char *msg)
{
	if(err != noErr) printf("%s: %d\n", msg, err);
}


/************************************************************
*                                                           *
*    QT_CreateMyVideoTrack()                                *
*    QT_EndCreateMyVideoTrack()                             *
*                                                           *
*    Creates/finishes a video track for the QuickTime movie *
*                                                           *
*************************************************************/

static void QT_CreateMyVideoTrack(void)
{
	OSErr err = noErr;
	Rect trackFrame;

	trackFrame.top = 0;
	trackFrame.left = 0;
	trackFrame.bottom = R.recty;
	trackFrame.right = R.rectx;
	
	qte->theTrack = NewMovieTrack (qte->theMovie, 
							FixRatio(trackFrame.right,1),
							FixRatio(trackFrame.bottom,1), 
							kNoVolume);
	CheckError( GetMoviesError(), "NewMovieTrack error" );

	qte->theMedia = NewTrackMedia (qte->theTrack,
							VideoMediaType,
							qcdx->kVideoTimeScale,
							nil,
							0);
	CheckError( GetMoviesError(), "NewTrackMedia error" );

	err = BeginMediaEdits (qte->theMedia);
	CheckError( err, "BeginMediaEdits error" );

	QT_StartAddVideoSamplesToMedia (&trackFrame);
} 


static void QT_EndCreateMyVideoTrack(void)
{
	OSErr err = noErr;

	QT_EndAddVideoSamplesToMedia ();

	err = EndMediaEdits (qte->theMedia);
	CheckError( err, "EndMediaEdits error" );

	err = InsertMediaIntoTrack (qte->theTrack,
								kTrackStart,/* track start time */
								kMediaStart,/* media start time */
								GetMediaDuration (qte->theMedia),
								fixed1);
	CheckError( err, "InsertMediaIntoTrack error" );
} 


/************************************************************
*                                                           *
*    QT_StartAddVideoSamplesToMedia()                       *
*    QT_DoAddVideoSamplesToMedia()                          *
*    QT_EndAddVideoSamplesToMedia()                         *
*                                                           *
*    Creates video samples for the media in a track         *
*                                                           *
*************************************************************/

static void QT_StartAddVideoSamplesToMedia (const Rect *trackFrame)
{
	OSErr err = noErr;

	qte->ibuf = IMB_allocImBuf (R.rectx, R.recty, 32, IB_rect, 0);
	qte->ibuf2 = IMB_allocImBuf (R.rectx, R.recty, 32, IB_rect, 0);

	err = NewGWorldFromPtr( &qte->theGWorld,
							k32ARGBPixelFormat,
							trackFrame,
							NULL, NULL, 0,
							(unsigned char *)qte->ibuf->rect,
							R.rectx * 4 );
	CheckError (err, "NewGWorldFromPtr error");

	qte->thePixMap = GetGWorldPixMap(qte->theGWorld);
	LockPixels(qte->thePixMap);

	SCDefaultPixMapSettings (qcdx->theComponent, qte->thePixMap, true);

	SCSetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
	SCSetInfo(qcdx->theComponent, scSpatialSettingsType,	&qcdx->gSpatialSettings);
	SCSetInfo(qcdx->theComponent, scDataRateSettingsType,	&qcdx->aDataRateSetting);

	err = SCCompressSequenceBegin(qcdx->theComponent, qte->thePixMap, NULL, &qte->anImageDescription); 
	CheckError (err, "SCCompressSequenceBegin error" );
}


static void QT_DoAddVideoSamplesToMedia (int frame)
{
	OSErr	err = noErr;
	Rect	imageRect;

	register int		index;
	register int		boxsize;
	register uint32_t	*readPos;
	register uint32_t	*changePos;
	Ptr					myPtr;

	short	syncFlag;
	long	dataSize;
	Handle	compressedData;

// copy and flip the renderdata
	if(qte->ibuf2) {
		memcpy(qte->ibuf2->rect, R.rectot, 4*R.rectx*R.recty);
		IMB_flipy(qte->ibuf2);
	}

//get pointers to parse bitmapdata
	myPtr = GetPixBaseAddr(qte->thePixMap);
	imageRect = (**qte->thePixMap).bounds;

	boxsize = R.rectx * R.recty;
	readPos = (uint32_t *) qte->ibuf2->rect;
	changePos = (uint32_t *) myPtr;

#ifdef __APPLE__
// Swap alpha byte to the end, so ARGB become RGBA; note this is big endian-centric.
	for( index = 0; index < boxsize; index++, changePos++, readPos++ )
		*( changePos ) = ( ( *readPos & 0xFFFFFFFF ) >> 8 ) |
                         ( ( *readPos << 24 ) & 0xFF );
#endif

#ifdef _WIN32
// poked around a little... this seems to work for windows, dunno if it's legal
	for( index = 0; index < boxsize; index++, changePos++, readPos++ )
		*( changePos ) = ( ( *readPos & 0xFFFFFFFF ) << 8 ) |
						 ( ( *readPos >> 24 ) & 0xFF ); // & ( ( *readPos << 8 ) & 0xFF );
#endif

	err = SCCompressSequenceFrame(qcdx->theComponent,
		qte->thePixMap,
		&imageRect,
		&compressedData,
		&dataSize,
		&syncFlag);
	CheckError(err, "SCCompressSequenceFrame error");

	err = AddMediaSample(qte->theMedia,
		compressedData,
		0,
		dataSize,
		qcdx->duration,
		(SampleDescriptionHandle)qte->anImageDescription,
		1,
		syncFlag,
		NULL);
	CheckError(err, "AddMediaSample error");

	printf ("added frame %3d (frame %3d in movie): ", frame, frame-sframe);
}


static void QT_EndAddVideoSamplesToMedia (void)
{
	SCCompressSequenceEnd(qcdx->theComponent);

	UnlockPixels(qte->thePixMap);
	if (qte->theGWorld)	DisposeGWorld (qte->theGWorld);
	if (qte->ibuf)		IMB_freeImBuf(qte->ibuf);
	if (qte->ibuf2)		IMB_freeImBuf(qte->ibuf2);
} 


/************************************************************
*                                                           *
*    makeqtstring (char *string)                            *
*                                                           *
*    Function to generate output filename                   *
*                                                           *
*************************************************************/

void makeqtstring (char *string) {
	char txt[64];

	if (string==0) return;

	strcpy(string, G.scene->r.pic);
	BLI_convertstringcode(string, G.sce, G.scene->r.cfra);

	RE_make_existing_file(string);

	if (strcasecmp(string + strlen(string) - 4, ".mov")) {
		sprintf(txt, "%04d_%04d.mov", (G.scene->r.sfra) , (G.scene->r.efra) );
		strcat(string, txt);
	}
}


/************************************************************
*                                                           *
*    start_qt(void)                                         *
*    append_qt(int frame)                                   *
*    end_qt(int frame)                                      *
*                                                           *
*    Quicktime Export functions for Blender's initrender.c  *
*                                                           *
************************************************************/

void start_qt(void) {
	OSErr err = noErr;

	char name[2048];
	char theFullPath[255];

#ifdef __APPLE__
	int		myFile;
	FSRef	myRef;
#endif

	if(qte == NULL) qte = MEM_callocN(sizeof(QuicktimeExport), "QuicktimeExport");

	if(qcdx) {
		if(qcdx->theComponent) CloseComponent(qcdx->theComponent);
		free_qtcodecdataExt();
	}

	qcdx = MEM_callocN(sizeof(QuicktimeCodecDataExt), "QuicktimeCodecDataExt");

	if(G.scene->r.qtcodecdata == NULL && G.scene->r.qtcodecdata->cdParms == NULL) {
		get_qtcodec_settings();
	} else {
		qcdx->theComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);

//		printf("getting from blend\n");
		GetExporterSettingsFromMem (G.scene->r.qtcodecdata);
		check_renderbutton_framerate();
	}
	
	if (G.afbreek != 1) {
		sframe = (G.scene->r.sfra);

		makeqtstring(name);
		sprintf(theFullPath, "%s", name);

#ifdef __APPLE__
		/* hack: create an empty file to make FSPathMakeRef() happy */
		myFile = open(theFullPath, O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRUSR|S_IWUSR);
		if (myFile < 0) {
			printf("error while creating file!\n");
			/* do something? */
		}
		close(myFile);
		err = FSPathMakeRef(theFullPath, &myRef, 0);
		CheckError(err, "FsPathMakeRef error");
		err = FSGetCatalogInfo(&myRef, kFSCatInfoNone, NULL, NULL, &qte->theSpec, NULL);
		CheckError(err, "FsGetCatalogInfoRef error");
#else
		CopyCStringToPascal(theFullPath, qte->qtfilename);
		err = FSMakeFSSpec(0, 0L, qte->qtfilename, &qte->theSpec);
#endif

		err = CreateMovieFile (&qte->theSpec, 
							kMyCreatorType,
							smCurrentScript, 
							createMovieFileDeleteCurFile | createMovieFileDontCreateResFile,
							&qte->resRefNum, 
							&qte->theMovie );
		CheckError(err, "CreateMovieFile error");

		printf("Created QuickTime movie: %s\n", name);

		QT_CreateMyVideoTrack();
	}
}


void append_qt(int frame) {
	QT_DoAddVideoSamplesToMedia(frame);
}

void end_qt(void) {
	OSErr err = noErr;

	if(qte->theMovie) {
		QT_EndCreateMyVideoTrack ();

		qte->resId = movieInDataForkResID;
		err = AddMovieResource (qte->theMovie, qte->resRefNum, &qte->resId, qte->qtfilename);
		CheckError(err, "AddMovieResource error");

		if (qte->resRefNum)	CloseMovieFile (qte->resRefNum);

		DisposeMovie (qte->theMovie);
	}

	if(qte) {
		MEM_freeN(qte);
		qte = NULL;
	}
};


/************************************************************
*                                                           *
*    free_qtcodecdataExt(void)                              *
*                                                           *
*    Function to release codec memory, since it remains     *
*    resident after allocation.                             *
*                                                           *
*************************************************************/

void free_qtcodecdataExt(void) {
	if(qcdx) {
		if(qcdx->theComponent) CloseComponent(qcdx->theComponent);
		MEM_freeN(qcdx);
		qcdx = NULL;
	}
}


/************************************************************
*                                                           *
*    check_renderbutton_framerate ( void )                  *
*                                                           *
*    To keep float framerates consistent between the codec  *
*    dialog and frs/sec button.                             *
*                                                           *
*************************************************************/

static void check_renderbutton_framerate(void) {
	OSErr	err;	

	err = SCGetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
	CheckError(err, "SCGetInfo fr error");

	if( (G.scene->r.frs_sec == 24 || G.scene->r.frs_sec == 30 || G.scene->r.frs_sec == 60) &&
		(qcdx->gTemporalSettings.frameRate == 1571553 ||
		 qcdx->gTemporalSettings.frameRate == 1964113 ||
		 qcdx->gTemporalSettings.frameRate == 3928227)) {;} else
	qcdx->gTemporalSettings.frameRate = G.scene->r.frs_sec << 16;

	err = SCSetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
	CheckError( err, "SCSetInfo error" );

	if(qcdx->gTemporalSettings.frameRate == 1571553) {			// 23.98 fps
		qcdx->kVideoTimeScale = 2398;
		qcdx->duration = 100;
	} else if (qcdx->gTemporalSettings.frameRate == 1964113) {	// 29.97 fps
		qcdx->kVideoTimeScale = 2997;
		qcdx->duration = 100;
	} else if (qcdx->gTemporalSettings.frameRate == 3928227) {	// 59.94 fps
		qcdx->kVideoTimeScale = 5994;
		qcdx->duration = 100;
	} else {
		qcdx->kVideoTimeScale = (qcdx->gTemporalSettings.frameRate >> 16) * 100;
		qcdx->duration = 100;
	}
}

/********************************************************************
*                                                                   *
*    get_qtcodec_settings()                                         *
*                                                                   *
*    Displays Codec Dialog and retrieves Quicktime Codec settings.  *
*                                                                   *
********************************************************************/

int get_qtcodec_settings(void) 
{
	OSErr	err = noErr;
	CodecInfo ci;
	char str[255];

	// erase any existing codecsetting
	if(qcdx) {
		if(qcdx->theComponent) CloseComponent(qcdx->theComponent);
		free_qtcodecdataExt();
	}

	// allocate new
	qcdx = MEM_callocN(sizeof(QuicktimeCodecDataExt), "QuicktimeCodecDataExt");
	qcdx->theComponent = OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);

	// get previous selected codecsetting, if any 
	if(G.scene->r.qtcodecdata && G.scene->r.qtcodecdata->cdParms) {
//		printf("getting from MEM\n");
		GetExporterSettingsFromMem (G.scene->r.qtcodecdata);
		check_renderbutton_framerate();
	} else {

	// configure the standard image compression dialog box
	// set some default settings
//		qcdx->gSpatialSettings.codecType = nil;     
		qcdx->gSpatialSettings.codec = anyCodec;         
//		qcdx->gSpatialSettings.depth;         
		qcdx->gSpatialSettings.spatialQuality = codecMaxQuality;

		qcdx->gTemporalSettings.temporalQuality = codecMaxQuality;
//		qcdx->gTemporalSettings.frameRate;      
		qcdx->gTemporalSettings.keyFrameRate = 25;   

		qcdx->aDataRateSetting.dataRate = 90 * 1024;          
//		qcdx->aDataRateSetting.frameDuration;     
//		qcdx->aDataRateSetting.minSpatialQuality; 
//		qcdx->aDataRateSetting.minTemporalQuality;

		err = SCSetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
		CheckError(err, "SCSetInfo1 error");
		err = SCSetInfo(qcdx->theComponent, scSpatialSettingsType,	&qcdx->gSpatialSettings);
		CheckError(err, "SCSetInfo2 error");
		err = SCSetInfo(qcdx->theComponent, scDataRateSettingsType,	&qcdx->aDataRateSetting);
		CheckError(err, "SCSetInfo3 error");
	}

	check_renderbutton_framerate();

	// put up the dialog box
	err = SCRequestSequenceSettings(qcdx->theComponent);
 
	if (err == scUserCancelled) {
		G.afbreek = 1;
		return 0;
	}

	// get user selected data
	SCGetInfo(qcdx->theComponent, scTemporalSettingsType,	&qcdx->gTemporalSettings);
	SCGetInfo(qcdx->theComponent, scSpatialSettingsType,	&qcdx->gSpatialSettings);
	SCGetInfo(qcdx->theComponent, scDataRateSettingsType,	&qcdx->aDataRateSetting);

	GetCodecInfo (&ci, qcdx->gSpatialSettings.codecType, 0);
	CopyPascalStringToC(ci.typeName, str);
	sprintf(qtcdname,"Codec: %s", str);

	SaveExporterSettingsToMem(G.scene->r.qtcodecdata);

	// framerate jugglin'
	if(qcdx->gTemporalSettings.frameRate == 1571553) {			// 23.98 fps
		qcdx->kVideoTimeScale = 2398;
		qcdx->duration = 100;

		G.scene->r.frs_sec = 24;
	} else if (qcdx->gTemporalSettings.frameRate == 1964113) {	// 29.97 fps
		qcdx->kVideoTimeScale = 2997;
		qcdx->duration = 100;

		G.scene->r.frs_sec = 30;
	} else if (qcdx->gTemporalSettings.frameRate == 3928227) {	// 59.94 fps
		qcdx->kVideoTimeScale = 5994;
		qcdx->duration = 100;

		G.scene->r.frs_sec = 60;
	} else {
		qcdx->kVideoTimeScale = 600;
		qcdx->duration = qcdx->kVideoTimeScale / (qcdx->gTemporalSettings.frameRate / 65536);

		G.scene->r.frs_sec = (qcdx->gTemporalSettings.frameRate / 65536);
	}

	return 1;
}

#endif /* _WIN32 || __APPLE__ */

#endif /* WITH_QUICKTIME */

