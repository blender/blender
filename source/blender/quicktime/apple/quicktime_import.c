/*
 *
 * quicktime_import.c
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * The Original Code is written by Rob Haarsma (phase)
 *
 * Contributor(s): Stefan Gartner (sgefant)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/quicktime/apple/quicktime_import.c
 *  \ingroup quicktime
 */

#ifdef WITH_QUICKTIME

#if defined(_WIN32) || defined(__APPLE__)
#ifndef USE_QTKIT

#include "MEM_guardedalloc.h"
#include "IMB_anim.h"
#include "BLO_sys_types.h"
#include "BKE_global.h"
#include "BLI_dynstr.h"
#include "BLI_path_util.h"

#ifdef __APPLE__
#include <QuickTime/Movies.h>
#include <QuickTime/QuickTimeComponents.h>
#endif

#ifdef _WIN32
#include <Movies.h>
#include <QTML.h>
#include <TextUtils.h>
#include <QuickTimeComponents.h>
#include <QTLoadLibraryUtils.h>
#endif /* _WIN32 */


#include "quicktime_import.h"
#include "quicktime_export.h"

#define	RECT_WIDTH(r)	(r.right-r.left)
#define	RECT_HEIGHT(r)	(r.bottom-r.top)

#define QTIME_DEBUG 0

typedef struct _QuicktimeMovie {

	GWorldPtr	offscreenGWorld;
	PixMapHandle	offscreenPixMap;
	Movie		movie;
	Rect		movieBounds;
	short		movieRefNum;
	short		movieResId;
	int			movWidth, movHeight;

	
	int			framecount;
	
	
	ImBuf		*ibuf;
	

	TimeValue	*frameIndex;
	Media		theMedia;
	Track		theTrack;
	long		trackIndex;
	short		depth;
	
	int			have_gw;	//ugly
} QuicktimeMovie;



void quicktime_init(void)
{
	OSErr nerr;
#ifdef _WIN32
	QTLoadLibrary("QTCF.dll");
	nerr = InitializeQTML(0);
	if (nerr != noErr) {
		G.have_quicktime = FALSE;
	}
	else
		G.have_quicktime = TRUE;
#endif /* _WIN32 */

	/* Initialize QuickTime */
#if defined(_WIN32) || defined (__APPLE__)
	nerr = EnterMovies();
	if (nerr != noErr)
		G.have_quicktime = FALSE;
	else
#endif /* _WIN32 || __APPLE__ */
#ifdef __linux__
	/* inititalize quicktime codec registry */
		lqt_registry_init();
#endif
	G.have_quicktime = TRUE;
}


void quicktime_exit(void)
{
#if defined(_WIN32) || defined(__APPLE__)
#ifdef WITH_QUICKTIME
	if (G.have_quicktime) {
		free_qtcomponentdata();
		ExitMovies();
#ifdef _WIN32
		TerminateQTML();
#endif /* _WIN32 */
	}
#endif /* WITH_QUICKTIME */
#endif /* _WIN32 || __APPLE__ */
}


#ifdef _WIN32
char *get_valid_qtname(char *name)
{
	TCHAR Buffer[MAX_PATH];
	DWORD dwRet;
	char *qtname;
	DynStr *ds= BLI_dynstr_new();

	dwRet = GetCurrentDirectory(MAX_PATH, Buffer);

	if (name[1] != ':') {
		char drive[2];

		if (name[0] == '/' || name[0] == '\\') {
			drive[0] = Buffer[0];
			drive[1] = '\0';

			BLI_dynstr_append(ds, drive);
			BLI_dynstr_append(ds, ":");
			BLI_dynstr_append(ds, name);
		}
		else {
			BLI_dynstr_append(ds, Buffer);
			BLI_dynstr_append(ds, "/");
			BLI_dynstr_append(ds, name);
		}
	}
	else {
		BLI_dynstr_append(ds, name);
	}

	qtname= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return qtname;
}
#endif /* _WIN32 */


int anim_is_quicktime(const char *name)
{
	FSSpec	theFSSpec;
	char	theFullPath[255];

	Boolean						isMovieFile = false;
	AliasHandle					myAlias = NULL;
	Component					myImporter = NULL;
#ifdef __APPLE__
	FInfo						myFinderInfo;
	FSRef						myRef;
#else
	char *qtname;
	Str255  dst;
#endif
	OSErr						err = noErr;
			
	// don't let quicktime movie import handle these
	if ( BLI_testextensie(name, ".swf") ||
	     BLI_testextensie(name, ".txt") ||
	     BLI_testextensie(name, ".mpg") ||
	     BLI_testextensie(name, ".avi") ||	// wouldnt be appropriate ;)
	     BLI_testextensie(name, ".tga") ||
	     BLI_testextensie(name, ".png") ||
	     BLI_testextensie(name, ".bmp") ||
	     BLI_testextensie(name, ".jpg") ||
	     BLI_testextensie(name, ".wav") ||
	     BLI_testextensie(name, ".zip") ||
	     BLI_testextensie(name, ".mp3"))
	{
		return 0;
	}

	if (QTIME_DEBUG) printf("qt: checking as movie: %s\n", name);

#ifdef __APPLE__
	sprintf(theFullPath, "%s", name);

	err = FSPathMakeRef(theFullPath, &myRef, 0);
	err = FSGetCatalogInfo(&myRef, kFSCatInfoNone, NULL, NULL, &theFSSpec, NULL);
#else
	qtname = get_valid_qtname(name);
	sprintf(theFullPath, "%s", qtname);
	MEM_freeN(qtname);

	CopyCStringToPascal(theFullPath, dst);
	err = FSMakeFSSpec(0, 0L, dst, &theFSSpec);
#endif

#ifdef __APPLE__
	// see whether the file type is MovieFileType; to do this, get the Finder information
	err = FSpGetFInfo(&theFSSpec, &myFinderInfo);
	if (err == noErr) {
		if (myFinderInfo.fdType == kQTFileTypeMovie) {
			return(true);
		}
	}
#endif

/* on mac os x this results in using quicktime for other formats as well
 * not sure whether this is intended
 */
	// if it isn't a movie file, see whether the file can be imported as a movie
	err = QTNewAlias(&theFSSpec, &myAlias, true);
	if (err == noErr) {
		if (myAlias != NULL) {
			err = GetMovieImporterForDataRef(rAliasType, (Handle)myAlias, kGetMovieImporterDontConsiderGraphicsImporters, &myImporter);
			DisposeHandle((Handle)myAlias);
		}
	}
	
	if ((err == noErr) && (myImporter != NULL)) {		// this file is a movie file
		isMovieFile = true;
	}

	return(isMovieFile);
}


void free_anim_quicktime(struct anim *anim)
{
	if (anim == NULL) return;
	if (anim->qtime == NULL) return;

	UnlockPixels(anim->qtime->offscreenPixMap);

	if (anim->qtime->have_gw)
		DisposeGWorld(anim->qtime->offscreenGWorld);
	if (anim->qtime->ibuf)
		IMB_freeImBuf(anim->qtime->ibuf);

	DisposeMovie(anim->qtime->movie);
	CloseMovieFile(anim->qtime->movieRefNum);

	if (anim->qtime->frameIndex) MEM_freeN (anim->qtime->frameIndex);
	if (anim->qtime) MEM_freeN (anim->qtime);

	anim->qtime = NULL;

	anim->duration = 0;
}


static OSErr QT_get_frameIndexes(struct anim *anim)
{
	int i;
	OSErr	anErr = noErr;
	OSType	media = VideoMediaType;
	TimeValue nextTime = 0;
	TimeValue	startPoint;
	TimeValue	tmpstartPoint;
	long sampleCount = 0;

	startPoint = -1;

	GetMovieNextInterestingTime(anim->qtime->movie, nextTimeMediaSample+nextTimeEdgeOK, (TimeValue)1, &media, 0, 
								1, &startPoint, NULL);

	tmpstartPoint = startPoint;

	anim->qtime->framecount = 0;

	sampleCount = GetMediaSampleCount(anim->qtime->theMedia);
	anErr = GetMoviesError();
	if (anErr != noErr) return anErr;

	anim->qtime->framecount = sampleCount;

	anim->qtime->frameIndex = (TimeValue *) MEM_callocN(sizeof(TimeValue) * anim->qtime->framecount, "qtframeindex");

	//rewind
	GetMovieNextInterestingTime(anim->qtime->movie, nextTimeMediaSample, 1, &media, (TimeValue)1, 0, &tmpstartPoint, NULL);

	anim->qtime->frameIndex[0] = startPoint;
	for (i = 1; i < anim->qtime->framecount; i++) {
		nextTime = 0;
		GetMovieNextInterestingTime(anim->qtime->movie, nextTimeMediaSample, 1, &media, startPoint, 0, &nextTime, NULL);
		startPoint = nextTime;
		anim->qtime->frameIndex[i] = nextTime;
	}

	anErr = GetMoviesError();
	return anErr;
}


ImBuf * qtime_fetchibuf (struct anim *anim, int position)
{
	PixMapHandle			myPixMap = NULL;
	Ptr						myPtr;

	register int		index;
	register int		boxsize;

	register uint32_t	*readPos;
	register uint32_t	*changePos;

	ImBuf *ibuf = NULL;
	unsigned int *rect;
#ifdef __APPLE__
	unsigned char *from, *to;
#endif
#ifdef _WIN32
	unsigned char *crect;
#endif

	if (anim == NULL) {
		return (NULL);
	}

	ibuf = IMB_allocImBuf (anim->x, anim->y, 32, IB_rect);
	rect = ibuf->rect;

	SetMovieTimeValue(anim->qtime->movie, anim->qtime->frameIndex[position]);
	UpdateMovie(anim->qtime->movie);
	MoviesTask(anim->qtime->movie, 0);


	myPixMap = GetGWorldPixMap(anim->qtime->offscreenGWorld);
	myPtr = GetPixBaseAddr(myPixMap);

	if (myPtr == NULL) {
		printf ("Error reading frame from Quicktime");
		IMB_freeImBuf (ibuf);
		return NULL;
	}

	boxsize = anim->x * anim->y;
	readPos = (uint32_t *) myPtr;
	changePos = (uint32_t *) rect; //textureIMBuf *THE* data pointerrr

#ifdef __APPLE__
	// Swap alpha byte to the end, so ARGB become RGBA;
	from= (unsigned char *)readPos;
	to= (unsigned char *)changePos;
	
	for ( index = 0; index < boxsize; index++, from+=4, to+=4 ) {
		to[3] = from[0];
		to[0] = from[1];
		to[1] = from[2];
		to[2] = from[3];
	}
#endif

#ifdef _WIN32
	for ( index = 0; index < boxsize; index++, changePos++, readPos++ )
		*( changePos ) =  *(readPos );

	if (anim->qtime->depth < 32) {
		//add alpha to ibuf
		boxsize = anim->x * anim->y * 4;
		crect = (unsigned char *) rect;
		for ( index = 0; index < boxsize; index+=4, crect+=4 ) {
			crect[3] = 0xFF;
		}
	}
#endif

	ibuf->profile = IB_PROFILE_SRGB;
	
	IMB_flipy(ibuf);
	return ibuf;
}


// following two functions only here to get movie pixeldepth

static int GetFirstVideoMedia(struct anim *anim)
{
	long    numTracks;
	OSType  mediaType;

	numTracks = GetMovieTrackCount(anim->qtime->movie);

	for (anim->qtime->trackIndex=1; anim->qtime->trackIndex<=numTracks; (anim->qtime->trackIndex)++) {
		anim->qtime->theTrack = GetMovieIndTrack(anim->qtime->movie, anim->qtime->trackIndex);

		if (anim->qtime->theTrack)
			anim->qtime->theMedia = GetTrackMedia(anim->qtime->theTrack);

		if (anim->qtime->theMedia)
			GetMediaHandlerDescription(anim->qtime->theMedia, &mediaType, nil, nil);
		if (mediaType == VideoMediaType) return 1;
	}

	anim->qtime->trackIndex = 0;  // trackIndex can't be 0
	return 0;      // went through all tracks and no video
}

static short GetFirstVideoTrackPixelDepth(struct anim *anim)
{
	SampleDescriptionHandle imageDescH =	(SampleDescriptionHandle)NewHandle(sizeof(Handle));
//	long	trackIndex = 0; /*unused*/
	
	if (!GetFirstVideoMedia(anim))
		return -1;

	if (!anim->qtime->trackIndex || !anim->qtime->theMedia) return -1;  // we need both
	GetMediaSampleDescription(anim->qtime->theMedia, anim->qtime->trackIndex, imageDescH);

	return (*(ImageDescriptionHandle)imageDescH)->depth;
}


int startquicktime(struct anim *anim)
{
	FSSpec		theFSSpec;

	OSErr		err = noErr;
	char		theFullPath[255];
#ifdef __APPLE__
	FSRef		myRef;
#else
	char		*qtname;
	Str255		dst;
#endif
	short depth = 0;

	anim->qtime = MEM_callocN (sizeof(QuicktimeMovie), "animqt");
	anim->qtime->have_gw = FALSE;

	if (anim->qtime == NULL) {
		if (QTIME_DEBUG) printf("Can't alloc qtime: %s\n", anim->name);
		return -1;
	}

	if (QTIME_DEBUG) printf("qt: attempting to load as movie %s\n", anim->name);
	
#ifdef __APPLE__
	sprintf(theFullPath, "%s", anim->name);

	err = FSPathMakeRef(theFullPath, &myRef, 0);
	err = FSGetCatalogInfo(&myRef, kFSCatInfoNone, NULL, NULL, &theFSSpec, NULL);
#else
	qtname = get_valid_qtname(anim->name);
	sprintf(theFullPath, "%s", qtname);
	MEM_freeN(qtname);

	CopyCStringToPascal(theFullPath, dst);
	FSMakeFSSpec(0, 0L, dst, &theFSSpec);
#endif
	
	err = OpenMovieFile(&theFSSpec, &anim->qtime->movieRefNum, fsRdPerm);

	if (err == noErr) {
		if (QTIME_DEBUG) printf("qt: movie opened\n");
		err = NewMovieFromFile(&anim->qtime->movie,
						   anim->qtime->movieRefNum,
						   &anim->qtime->movieResId, NULL, newMovieActive, NULL);
	}

	if (err) {
		if (QTIME_DEBUG) printf("qt: bad movie %s\n", anim->name);
		if (anim->qtime->movie) {
			DisposeMovie(anim->qtime->movie);
			MEM_freeN(anim->qtime);
			if (QTIME_DEBUG) printf("qt: can't load %s\n", anim->name);
			return -1;
		}
	}

	GetMovieBox(anim->qtime->movie, &anim->qtime->movieBounds);
	anim->x = anim->qtime->movWidth = RECT_WIDTH(anim->qtime->movieBounds);
	anim->y = anim->qtime->movHeight = RECT_HEIGHT(anim->qtime->movieBounds);
	if (QTIME_DEBUG) printf("qt: got bounds %s\n", anim->name);

	if (anim->x == 0 && anim->y == 0) {
		if (QTIME_DEBUG) printf("qt: error, no dimensions\n");
		free_anim_quicktime(anim);
		return -1;
	}

	anim->qtime->ibuf = IMB_allocImBuf (anim->x, anim->y, 32, IB_rect);

#ifdef _WIN32
	err = NewGWorldFromPtr(&anim->qtime->offscreenGWorld,
		 k32RGBAPixelFormat,
		 &anim->qtime->movieBounds,
		 NULL, NULL, 0,
		(unsigned char *)anim->qtime->ibuf->rect,
		anim->x * 4);
#else
	err = NewGWorldFromPtr(&anim->qtime->offscreenGWorld,
		 k32ARGBPixelFormat,
		 &anim->qtime->movieBounds,
		 NULL, NULL, 0,
		(unsigned char *)anim->qtime->ibuf->rect,
		anim->x * 4);
#endif /* _WIN32 */

	if (err == noErr) {
		anim->qtime->have_gw = TRUE;

		SetMovieGWorld(anim->qtime->movie,
		               anim->qtime->offscreenGWorld,
		               GetGWorldDevice(anim->qtime->offscreenGWorld));
		SetMoviePlayHints(anim->qtime->movie, hintsHighQuality, hintsHighQuality);
		
		// sets Media and Track!
		depth = GetFirstVideoTrackPixelDepth(anim);

		QT_get_frameIndexes(anim);
	}

	anim->qtime->offscreenPixMap = GetGWorldPixMap(anim->qtime->offscreenGWorld);
	LockPixels(anim->qtime->offscreenPixMap);

	//fill blender's anim struct
	anim->qtime->depth = depth;
	
	anim->duration = anim->qtime->framecount;
	anim->params = 0;

	anim->interlacing = 0;
	anim->orientation = 0;
	anim->framesize = anim->x * anim->y * 4;

	anim->curposition = 0;

	if (QTIME_DEBUG) printf("qt: load %s %dx%dx%d frames %d\n", anim->name, anim->qtime->movWidth,
		anim->qtime->movHeight, anim->qtime->depth, anim->qtime->framecount);

	return 0;
}

int imb_is_a_quicktime (char *name)
{
	GraphicsImportComponent		theImporter = NULL;

	FSSpec	theFSSpec;
#ifdef _WIN32
	Str255  dst; /*unused*/
#endif
	char	theFullPath[255];

//	Boolean						isMovieFile = false; /*unused*/
//	AliasHandle					myAlias = NULL; /*unused*/
//	Component					myImporter = NULL; /*unused*/
#ifdef __APPLE__
//	FInfo						myFinderInfo; /*unused*/
	FSRef						myRef;
#endif
	OSErr						err = noErr;

	if (!G.have_quicktime) return 0;

	if (QTIME_DEBUG) printf("qt: checking as image %s\n", name);

	// don't let quicktime image import handle these
	if (BLI_testextensie(name, ".swf") ||
	    BLI_testextensie(name, ".txt") ||
	    BLI_testextensie(name, ".mpg") ||
	    BLI_testextensie(name, ".wav") ||
	    BLI_testextensie(name, ".mov") ||	// not as image, doesn't work
	    BLI_testextensie(name, ".avi") ||
	    BLI_testextensie(name, ".mp3"))
	{
		return 0;
	}

	sprintf(theFullPath, "%s", name);
#ifdef __APPLE__
	err = FSPathMakeRef(theFullPath, &myRef, 0);
	err = FSGetCatalogInfo(&myRef, kFSCatInfoNone, NULL, NULL, &theFSSpec, NULL);
#else
	CopyCStringToPascal(theFullPath, dst);
	err = FSMakeFSSpec(0, 0L, dst, &theFSSpec);
#endif

	GetGraphicsImporterForFile(&theFSSpec, &theImporter);

	if (theImporter != NULL) {
		if (QTIME_DEBUG) printf("qt: %s valid\n", name);
		CloseComponent(theImporter);
		return 1;
	}

	return 0;
}

ImBuf  *imb_quicktime_decode(unsigned char *mem, int size, int flags)
{
	Rect						myRect;
	OSErr						err = noErr;
	GraphicsImportComponent		gImporter = NULL;

	ImageDescriptionHandle		desc;

	ComponentInstance			dataHandler;
	PointerDataRef dataref;

	int x, y, depth;
	int have_gw = FALSE;
	ImBuf *ibuf = NULL;
//	ImBuf *imbuf = NULL; /*unused*/
	GWorldPtr	offGWorld;
	PixMapHandle		myPixMap = NULL;

#ifdef __APPLE__
	Ptr					myPtr;

	register int		index;
	register int		boxsize;

	register uint32_t	*readPos;
	register uint32_t	*changePos;

	ImBuf *wbuf = NULL;
	unsigned int *rect;
	unsigned char *from, *to;
#endif

	if (mem == NULL || !G.have_quicktime)
		goto bail;
	
	if (QTIME_DEBUG) printf("qt: attempt to load mem as image\n");

	dataref= (PointerDataRef)NewHandle(sizeof(PointerDataRefRecord));
	(**dataref).data = mem;
	(**dataref).dataLength = size;

	err = OpenADataHandler((Handle)dataref,
							PointerDataHandlerSubType,
							nil,
							(OSType)0,
							nil,
							kDataHCanRead,
							&dataHandler);
	if (err != noErr) {
		if (QTIME_DEBUG) printf("no datahandler\n");
		goto bail;
	}

	err = GetGraphicsImporterForDataRef((Handle)dataref, PointerDataHandlerSubType, &gImporter);
	if (err != noErr) {
		if (QTIME_DEBUG) printf("no graphimport\n");
		goto bail;
	}

	err = GraphicsImportGetNaturalBounds(gImporter, &myRect);
	if (err != noErr) {
		if (QTIME_DEBUG) printf("no bounds\n");
		goto bail;
	}

	err = GraphicsImportGetImageDescription (gImporter, &desc );
	if (err != noErr) {
		if (QTIME_DEBUG) printf("no imagedescription\n");
		goto bail;
	}

	x = RECT_WIDTH(myRect);
	y = RECT_HEIGHT(myRect);
	depth = (**desc).depth;

	if (flags & IB_test) {
		ibuf = IMB_allocImBuf(x, y, depth, 0);
		ibuf->ftype = QUICKTIME;
		DisposeHandle((Handle)dataref);
		if (gImporter != NULL)	CloseComponent(gImporter);
		return ibuf;
	}

#ifdef __APPLE__
	ibuf = IMB_allocImBuf (x, y, 32, IB_rect);
	wbuf = IMB_allocImBuf (x, y, 32, IB_rect);

	err = NewGWorldFromPtr(&offGWorld,
						k32ARGBPixelFormat,
						&myRect, NULL, NULL, 0,
						(unsigned char *)wbuf->rect, x * 4);
#else

	ibuf = IMB_allocImBuf (x, y, 32, IB_rect);	

	err = NewGWorldFromPtr(&offGWorld,
							k32RGBAPixelFormat,
							&myRect, NULL, NULL, 0,
							(unsigned char *)ibuf->rect, x * 4);
#endif
	
	if (err != noErr) {
		if (QTIME_DEBUG) printf("no newgworld\n");
		goto bail;
	}
	else {
		have_gw = TRUE;
	}

	GraphicsImportSetGWorld(gImporter, offGWorld, NULL);
	GraphicsImportDraw(gImporter);

#ifdef __APPLE__
	rect = ibuf->rect;

	myPixMap = GetGWorldPixMap(offGWorld);
	LockPixels(myPixMap);
	myPtr = GetPixBaseAddr(myPixMap);

	if (myPtr == NULL) {
		printf ("Error reading frame from Quicktime");
		IMB_freeImBuf (ibuf);
		return NULL;
	}

	boxsize = x * y;
	readPos = (uint32_t *) myPtr;
	changePos = (uint32_t *) rect;

	// Swap alpha byte to the end, so ARGB become RGBA;
	from= (unsigned char *)readPos;
	to= (unsigned char *)changePos;
	
	for ( index = 0; index < boxsize; index++, from+=4, to+=4 ) {
		to[3] = from[0];
		to[0] = from[1];
		to[1] = from[2];
		to[2] = from[3];
	}
#endif

bail:

	DisposeHandle((Handle)dataref);
	UnlockPixels(myPixMap);
	if (have_gw) DisposeGWorld(offGWorld);

#ifdef __APPLE__
	if (wbuf) {
		IMB_freeImBuf (wbuf);
		wbuf = NULL;
	}
#endif

	if (gImporter != NULL)	CloseComponent(gImporter);

	if (err != noErr) {
		if (QTIME_DEBUG) printf("quicktime import unsuccesfull\n");
		if (ibuf) {
			IMB_freeImBuf (ibuf);
			ibuf = NULL;
		}
	}

	if (ibuf) {

#ifdef _WIN32
// add non transparent alpha layer, so images without alpha show up in the sequence editor
// exception for GIF images since these can be transparent without being 32 bit
// (might also be nescessary for OSX)
		int i;
		int box = x * y;
		unsigned char *arect = (unsigned char *) ibuf->rect;

		if ( depth < 32 && (**desc).cType != kGIFCodecType) {
			for (i = 0; i < box; i++, arect+=4)
				arect[3] = 0xFF;
		}
#endif

		IMB_flipy(ibuf);
		ibuf->ftype = QUICKTIME;
	}
	return ibuf;
}

#endif /* USE_QTKIT */
#endif /* _WIN32 || __APPLE__ */

#endif /* WITH_QUICKTIME */


#if 0

struct ImageDescription {
	long         idSize;
	CodecType    cType;
	long         resvd1;
	short        resvd2;
	short        dataRefIndex;
	short        version;
	short        revisionLevel;
	long         vendor;
	CodecQ       temporalQuality;
	CodecQ       spatialQuality;
	short        width;
	short        height;
	Fixed        hRes;
	Fixed        vRes;
	long         dataSize;
	short        frameCount;
	Str31        name;
	short        depth;
	short        clutID;
};

#endif // 0
