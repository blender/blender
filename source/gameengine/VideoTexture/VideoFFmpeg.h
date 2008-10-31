/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/
#if !defined VIDEOFFMPEG_H
#define VIDEOFFMPEG_H

#ifdef WITH_FFMPEG
extern "C" {
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
#include <ffmpeg/rational.h>
#include <ffmpeg/swscale.h>
}

#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#endif

#ifdef FFMPEG_CODEC_IS_POINTER
static inline AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static inline AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif

#include "VideoBase.h"


// type VideoFFmpeg declaration
class VideoFFmpeg : public VideoBase
{
public:
	/// constructor
	VideoFFmpeg (HRESULT * hRslt);
	/// destructor
	virtual ~VideoFFmpeg ();

	/// set initial parameters
	void initParams (short width, short height, float rate);
	/// open video file
	virtual void openFile (char * file);
	/// open video capture device
	virtual void openCam (char * driver, short camIdx);

	/// release video source
	virtual bool release (void);

	/// play video
	virtual bool play (void);
	/// stop/pause video
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
	AVFrame	*m_frameBGR;
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

	/// image calculation
	virtual void calcImage (unsigned int texId);

	/// load frame from video
	void loadFrame (void);

	/// set actual position
	void setPositions (void);

	/// get actual framerate
	double actFrameRate (void) { return m_frameRate * m_baseFrameRate; }

	/// common function to video file and capture
	int openStream(const char *filename, AVInputFormat *inputFormat, AVFormatParameters *formatParams);

	/// check if a frame is available and load it in pFrame, return true if a frame could be retrieved
	bool grabFrame(long frame);

	/// return the frame in RGB24 format, the image data is found in AVFrame.data[0]
	AVFrame* getFrame(void) { return m_frameBGR; }
};

inline VideoFFmpeg * getFFmpeg (PyImage * self) 
{
	return static_cast<VideoFFmpeg*>(self->m_image); 
}

#endif	//WITH_FFMPEG

#endif
