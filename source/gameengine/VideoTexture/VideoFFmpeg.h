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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of VideoTexture library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file VideoFFmpeg.h
 *  \ingroup bgevideotex
 */

#ifndef __VIDEOFFMPEG_H__
#define __VIDEOFFMPEG_H__

#ifdef WITH_FFMPEG
/* this needs to be parsed with __cplusplus defined before included through ffmpeg_compat.h */
#if defined(__FreeBSD__)
#  include <inttypes.h>
#endif
extern "C" {
#include <pthread.h>
#include "ffmpeg_compat.h"
#include "DNA_listBase.h"
#include "BLI_threads.h"
#include "BLI_blenlib.h"
}

#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#  define FFMPEG_OLD_FRAME_RATE 1
#else
#  define FFMPEG_CODEC_IS_POINTER 1
#endif

#ifdef FFMPEG_CODEC_IS_POINTER
static inline AVCodecContext *get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static inline AVCodecContext *get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif

#include "VideoBase.h"

#define CACHE_FRAME_SIZE	10
#define CACHE_PACKET_SIZE	30

// type VideoFFmpeg declaration
class VideoFFmpeg : public VideoBase
{
public:
	/// constructor
	VideoFFmpeg (HRESULT * hRslt);
	/// destructor
	virtual ~VideoFFmpeg ();

	/// set initial parameters
	void initParams (short width, short height, float rate, bool image=false);
	/// open video/image file
	virtual void openFile(char *file);
	/// open video capture device
	virtual void openCam(char *driver, short camIdx);

	/// release video source
	virtual bool release (void);

	/// play video
	virtual bool play (void);
	/// pause video
	virtual bool pause (void);
	/// stop video
	virtual bool stop (void);
	/// set play range
	virtual void setRange (double start, double stop);
	/// set frame rate
	virtual void setFrameRate (float rate);
	// some specific getters and setters
	int getPreseek(void) { return m_preseek; }
	void setPreseek(int preseek) { if (preseek >= 0) m_preseek = preseek; }
	bool getDeinterlace(void) { return m_deinterlace; }
	void setDeinterlace(bool deinterlace) { m_deinterlace = deinterlace; }
	char *getImageName(void) { return (m_isImage) ? m_imageName.Ptr() : NULL; }

protected:
	// format and codec information
	AVCodec	*m_codec;
	AVFormatContext *m_formatCtx;
	AVCodecContext *m_codecCtx;
	// raw frame extracted from video file
	AVFrame	*m_frame;
	// deinterlaced frame if codec requires it
	AVFrame	*m_frameDeinterlaced;
	// decoded RGB24 frame if codec requires it
	AVFrame	*m_frameRGB;
	// conversion from raw to RGB is done with sws_scale
	struct SwsContext *m_imgConvertCtx;
	// should the codec be deinterlaced?
	bool m_deinterlace;
	// number of frame of preseek
	int m_preseek;
	// order number of stream holding the video in format context
	int m_videoStream;

	// the actual frame rate
	double m_baseFrameRate;

	/// last displayed frame
	long m_lastFrame;

	/// end of file reached
	bool m_eof;

	/// flag to indicate that time is coming from application
	bool m_externTime;

	/// current file pointer position in file expressed in frame number
	long m_curPosition;

	/// time of video play start
	double m_startTime;

	/// width of capture in pixel
	short m_captWidth;
	
	/// height of capture in pixel
	short m_captHeight;

	/// frame rate of capture in frames per seconds
	float m_captRate;

	/// is file an image?
	bool m_isImage;

	/// is image loading done in a separate thread?
	bool m_isThreaded;

	/// is streaming or camera?
	bool m_isStreaming;

	/// keep last image name
	STR_String m_imageName;

	/// image calculation
	virtual void calcImage (unsigned int texId, double ts);

	/// set actual position
	void setPositions (void);

	/// get actual framerate
	double actFrameRate (void) { return m_frameRate * m_baseFrameRate; }

	/// common function to video file and capture
	int openStream(const char *filename, AVInputFormat *inputFormat, AVDictionary **formatParams);

	/// check if a frame is available and load it in pFrame, return true if a frame could be retrieved
	AVFrame* grabFrame(long frame);

	/// in case of caching, put the frame back in free queue
	void releaseFrame(AVFrame* frame);

	/// start thread to load the video file/capture/stream 
	bool startCache();
	void stopCache();

private:
	typedef struct {
		Link link;
		long framePosition;
		AVFrame *frame;
	} CacheFrame;
	typedef struct {
		Link link;
		AVPacket packet;
	} CachePacket;

	bool m_stopThread;
	bool m_cacheStarted;
	ListBase m_thread;
	ListBase m_frameCacheBase;	// list of frames that are ready
	ListBase m_frameCacheFree;	// list of frames that are unused
	ListBase m_packetCacheBase;	// list of packets that are ready for decoding
	ListBase m_packetCacheFree;	// list of packets that are unused
	pthread_mutex_t m_cacheMutex;

	AVFrame	*allocFrameRGB();
	static void *cacheThread(void *);
};

inline VideoFFmpeg *getFFmpeg(PyImage *self)
{
	return static_cast<VideoFFmpeg*>(self->m_image); 
}

#endif  /* WITH_FFMPEG */

#endif  /* __VIDEOFFMPEG_H__ */
