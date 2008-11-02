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

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include <string>

#include "Exception.h"
#include "VideoFFmpeg.h"

#ifdef WITH_FFMPEG

// default framerate
const double defFrameRate = 25.0;
// time scale constant
const long timeScale = 1000;

// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); m_status = SourceError; }

extern "C" void do_init_ffmpeg();

// class RenderVideo

// constructor
VideoFFmpeg::VideoFFmpeg (HRESULT * hRslt) : VideoBase(), 
m_codec(NULL), m_formatCtx(NULL), m_codecCtx(NULL), 
m_frame(NULL), m_frameDeinterlaced(NULL), m_frameBGR(NULL), m_imgConvertCtx(NULL),
m_deinterlace(false), m_preseek(0),	m_videoStream(-1), m_baseFrameRate(25.0),
m_lastFrame(-1),  m_curPosition(-1), m_startTime(0), 
m_captWidth(0), m_captHeight(0), m_captRate(0.f)
{
	// set video format
	m_format = RGB24;
	// force flip because ffmpeg always return the image in the wrong orientation for texture
	setFlip(true);
	// construction is OK
	*hRslt = S_OK;
}

// destructor
VideoFFmpeg::~VideoFFmpeg () 
{
}


// release components
bool VideoFFmpeg::release()
{
	// release
	if (m_codecCtx)
	{
		avcodec_close(m_codecCtx);
	}
	if (m_formatCtx)
	{
		av_close_input_file(m_formatCtx);
	}
	if (m_frame)
	{
		av_free(m_frame);
	}
	if (m_frameDeinterlaced)
	{
		MEM_freeN(m_frameDeinterlaced->data[0]);
		av_free(m_frameDeinterlaced);
	}
	if (m_frameBGR)
	{
		MEM_freeN(m_frameBGR->data[0]);
		av_free(m_frameBGR);
	}
	if (m_imgConvertCtx)
	{
		sws_freeContext(m_imgConvertCtx);
	}

	m_codec = NULL;
	m_codecCtx = NULL;
	m_formatCtx = NULL;
	m_frame = NULL;
	m_frame = NULL;
	m_frameBGR = NULL;
	m_imgConvertCtx = NULL;

	// object will be deleted after that
	return true;
}


// set initial parameters
void VideoFFmpeg::initParams (short width, short height, float rate)
{
	m_captWidth = width;
	m_captHeight = height;
	m_captRate = rate;
}

int VideoFFmpeg::openStream(const char *filename, AVInputFormat *inputFormat, AVFormatParameters *formatParams)
{
	AVFormatContext *formatCtx;
	int				i, videoStream;
	AVCodec			*codec;
	AVCodecContext	*codecCtx;

	if(av_open_input_file(&formatCtx, filename, inputFormat, 0, formatParams)!=0)
		return -1;

	if(av_find_stream_info(formatCtx)<0) 
	{
		av_close_input_file(formatCtx);
		return -1;
	}

	/* Find the first video stream */
	videoStream=-1;
	for(i=0; i<formatCtx->nb_streams; i++)
	{
		if(formatCtx->streams[i] &&
			get_codec_from_stream(formatCtx->streams[i]) && 
			(get_codec_from_stream(formatCtx->streams[i])->codec_type==CODEC_TYPE_VIDEO))
		{
			videoStream=i;
			break;
		}
	}

	if(videoStream==-1) 
	{
		av_close_input_file(formatCtx);
		return -1;
	}

	codecCtx = get_codec_from_stream(formatCtx->streams[videoStream]);

	/* Find the decoder for the video stream */
	codec=avcodec_find_decoder(codecCtx->codec_id);
	if(codec==NULL) 
	{
		av_close_input_file(formatCtx);
		return -1;
	}
	codecCtx->workaround_bugs = 1;
	if(avcodec_open(codecCtx, codec)<0) 
	{
		av_close_input_file(formatCtx);
		return -1;
	}

#ifdef FFMPEG_OLD_FRAME_RATE
	if(codecCtx->frame_rate>1000 && codecCtx->frame_rate_base==1)
		codecCtx->frame_rate_base=1000;
	m_baseFrameRate = (double)codecCtx->frame_rate / (double)codecCtx->frame_rate_base;
#else
	m_baseFrameRate = av_q2d(formatCtx->streams[videoStream]->r_frame_rate);
#endif
	if (m_baseFrameRate <= 0.0) 
		m_baseFrameRate = defFrameRate;

	m_codec = codec;
	m_codecCtx = codecCtx;
	m_formatCtx = formatCtx;
	m_videoStream = videoStream;
	m_frame = avcodec_alloc_frame();
	m_frameDeinterlaced = avcodec_alloc_frame();
	m_frameBGR = avcodec_alloc_frame();


	// allocate buffer if deinterlacing is required
	avpicture_fill((AVPicture*)m_frameDeinterlaced, 
		(uint8_t*)MEM_callocN(avpicture_get_size(
		m_codecCtx->pix_fmt,
		m_codecCtx->width, m_codecCtx->height), 
		"ffmpeg deinterlace"), 
		m_codecCtx->pix_fmt, m_codecCtx->width, m_codecCtx->height);

	// allocate buffer to store final decoded frame
	avpicture_fill((AVPicture*)m_frameBGR, 
		(uint8_t*)MEM_callocN(avpicture_get_size(
		PIX_FMT_BGR24,
		m_codecCtx->width, m_codecCtx->height),
		"ffmpeg bgr"),
		PIX_FMT_BGR24, m_codecCtx->width, m_codecCtx->height);
	// allocate sws context
	m_imgConvertCtx = sws_getContext(
		m_codecCtx->width,
		m_codecCtx->height,
		m_codecCtx->pix_fmt,
		m_codecCtx->width,
		m_codecCtx->height,
		PIX_FMT_BGR24,
		SWS_FAST_BILINEAR,
		NULL, NULL, NULL);

	if (!m_imgConvertCtx) {
		avcodec_close(m_codecCtx);
		av_close_input_file(m_formatCtx);
		av_free(m_frame);
		MEM_freeN(m_frameDeinterlaced->data[0]);
		av_free(m_frameDeinterlaced);
		MEM_freeN(m_frameBGR->data[0]);
		av_free(m_frameBGR);
		return -1;
	}
	return 0;
}

// open video file
void VideoFFmpeg::openFile (char * filename)
{
	do_init_ffmpeg();

	if (openStream(filename, NULL, NULL) != 0)
		return;

	if (m_codecCtx->gop_size)
		m_preseek = (m_codecCtx->gop_size < 25) ? m_codecCtx->gop_size+1 : 25;
	else if (m_codecCtx->has_b_frames)		
		m_preseek = 25;	// should determine gopsize
	else
		m_preseek = 0;

	// get video time range
	m_range[0] = 0.0;
	m_range[1] = (double)m_formatCtx->duration / AV_TIME_BASE;

	// open base class
	VideoBase::openFile(filename);

	if (
#ifdef FFMPEG_PB_IS_POINTER
        m_formatCtx->pb->is_streamed
#else
        m_formatCtx->pb.is_streamed
#endif
        )
	{
		// the file is in fact a streaming source, prevent seeking
		m_isFile = false;
		// for streaming it is important to do non blocking read
		m_formatCtx->flags |= AVFMT_FLAG_NONBLOCK;
	}
}


// open video capture device
void VideoFFmpeg::openCam (char * file, short camIdx)
{
	// open camera source
	AVInputFormat		*inputFormat;
	AVFormatParameters	formatParams;
	AVRational			frameRate;
	char				*p, filename[28], rateStr[20];

	do_init_ffmpeg();

	memset(&formatParams, 0, sizeof(formatParams));
#ifdef WIN32
	// video capture on windows only through Video For Windows driver
	inputFormat = av_find_input_format("vfwcap");
	if (!inputFormat)
		// Video For Windows not supported??
		return;
	sprintf(filename, "%d", camIdx);
#else
	// In Linux we support two types of devices: VideoForLinux and DV1394. 
	// the user specify it with the filename:
	// [<device_type>][:<standard>]
	// <device_type> : 'v4l' for VideoForLinux, 'dv1394' for DV1394. By default 'v4l'
	// <standard>    : 'pal', 'secam' or 'ntsc'. By default 'ntsc'
	// The driver name is constructed automatically from the device type:
	// v4l   : /dev/video<camIdx>
	// dv1394: /dev/dv1394/<camIdx>
	// If you have different driver name, you can specify the driver name explicitely 
	// instead of device type. Examples of valid filename:
	//    /dev/v4l/video0:pal
	//    /dev/ieee1394/1:ntsc
	//    dv1394:secam
	//    v4l:pal
	if (file && strstr(file, "1394") != NULL) 
	{
		// the user specifies a driver, check if it is v4l or d41394
		inputFormat = av_find_input_format("dv1394");
		sprintf(filename, "/dev/dv1394/%d", camIdx);
	} else 
	{
		inputFormat = av_find_input_format("video4linux");
		sprintf(filename, "/dev/video%d", camIdx);
	}
	if (!inputFormat)
		// these format should be supported, check ffmpeg compilation
		return;
	if (file && strncmp(file, "/dev", 4) == 0) 
	{
		// user does not specify a driver
		strncpy(filename, file, sizeof(filename));
		filename[sizeof(filename)-1] = 0;
		if ((p = strchr(filename, ':')) != 0)
			*p = 0;
	}
	if (file && (p = strchr(file, ':')) != NULL)
		formatParams.standard = p+1;
#endif
	//frame rate
	if (m_captRate <= 0.f)
		m_captRate = defFrameRate;
	sprintf(rateStr, "%f", m_captRate);
	av_parse_video_frame_rate(&frameRate, rateStr);
	// populate format parameters
	// need to specify the time base = inverse of rate
	formatParams.time_base.num = frameRate.den;
	formatParams.time_base.den = frameRate.num;
	formatParams.width = m_captWidth;
	formatParams.height = m_captHeight;

	if (openStream(filename, inputFormat, &formatParams) != 0)
		return;

	// for video capture it is important to do non blocking read
	m_formatCtx->flags |= AVFMT_FLAG_NONBLOCK;
	// open base class
	VideoBase::openCam(file, camIdx);
}


// play video
bool VideoFFmpeg::play (void)
{
	try
	{
		// if object is able to play
		if (VideoBase::play())
		{
			// set video position
			setPositions();
			// return success
			return true;
		}
	}
	CATCH_EXCP;
	return false;
}


// stop video
bool VideoFFmpeg::stop (void)
{
	try
	{
		if (VideoBase::stop())
		{
			return true;
		}
	}
	CATCH_EXCP;
	return false;
}


// set video range
void VideoFFmpeg::setRange (double start, double stop)
{
	try
	{
		// set range
		VideoBase::setRange(start, stop);
		// set range for video
		setPositions();
	}
	CATCH_EXCP;
}

// set framerate
void VideoFFmpeg::setFrameRate (float rate)
{
	VideoBase::setFrameRate(rate);
}


// image calculation
void VideoFFmpeg::calcImage (unsigned int texId)
{
	loadFrame();
}


// load frame from video
void VideoFFmpeg::loadFrame (void)
{
	// get actual time
	double actTime = PIL_check_seconds_timer() - m_startTime;
	// if video has ended
	if (m_isFile && actTime * m_frameRate >= m_range[1])
	{
		// if repeats are set, decrease them
		if (m_repeat > 0) 
			--m_repeat;
		// if video has to be replayed
		if (m_repeat != 0)
		{
			// reset its position
			actTime -= (m_range[1] - m_range[0]) / m_frameRate;
			m_startTime += (m_range[1] - m_range[0]) / m_frameRate;
		}
		// if video has to be stopped, stop it
		else 
			m_status = SourceStopped;
	}
	// if video is playing
	if (m_status == SourcePlaying)
	{
		// actual frame
		long actFrame = m_isFile ? long(actTime * actFrameRate()) : m_lastFrame + 1;
		// if actual frame differs from last frame
		if (actFrame != m_lastFrame)
		{
			// get image
			if(grabFrame(actFrame))
			{
				AVFrame* frame = getFrame();
				// save actual frame
				m_lastFrame = actFrame;
				// init image, if needed
				init(short(m_codecCtx->width), short(m_codecCtx->height));
				// process image
				process((BYTE*)(frame->data[0]));
			}
		}
	}
}


// set actual position
void VideoFFmpeg::setPositions (void)
{
	// set video start time
	m_startTime = PIL_check_seconds_timer();
	// if file is played and actual position is before end position
	if (m_isFile && m_lastFrame >= 0 && m_lastFrame < m_range[1] * actFrameRate())
		// continue from actual position
		m_startTime -= double(m_lastFrame) / actFrameRate();
	else
		m_startTime -= m_range[0];
}

// position pointer in file, position in second
bool VideoFFmpeg::grabFrame(long position)
{
	AVPacket packet;
	int frameFinished;
	int posFound = 1;
	bool frameLoaded = false;
	long long targetTs = 0;

	// first check if the position that we are looking for is in the preseek range
	// if so, just read the frame until we get there
	if (position > m_curPosition + 1 
		&& m_preseek 
		&& position - (m_curPosition + 1) < m_preseek) 
	{
		while(av_read_frame(m_formatCtx, &packet)>=0) 
		{
			if (packet.stream_index == m_videoStream) 
			{
				avcodec_decode_video(
					m_codecCtx, 
					m_frame, &frameFinished, 
					packet.data, packet.size);
				if (frameFinished)
					m_curPosition++;
			}
			av_free_packet(&packet);
			if (position == m_curPosition+1)
				break;
		}
	}
	// if the position is not in preseek, do a direct jump
	if (position != m_curPosition + 1) { 
		double timeBase = av_q2d(m_formatCtx->streams[m_videoStream]->time_base);
		long long pos = (long long)
			((long long) (position - m_preseek) * AV_TIME_BASE / m_baseFrameRate);
		long long startTs = m_formatCtx->streams[m_videoStream]->start_time;

		if (pos < 0)
			pos = 0;

		if (startTs != AV_NOPTS_VALUE)
			pos += (long long)(startTs * AV_TIME_BASE * timeBase);

		av_seek_frame(m_formatCtx, -1, pos, AVSEEK_FLAG_BACKWARD);
		// current position is now lost, guess a value. 
		// It's not important because it will be set at this end of this function
		m_curPosition = position - m_preseek - 1;
		// this is the timestamp of the frame we're looking for
		targetTs = (long long)(((double) position) / m_baseFrameRate / timeBase);
		if (startTs != AV_NOPTS_VALUE)
			targetTs += startTs;

		posFound = 0;
		avcodec_flush_buffers(m_codecCtx);
	}

	while(av_read_frame(m_formatCtx, &packet)>=0) 
	{
		if(packet.stream_index == m_videoStream) 
		{
			avcodec_decode_video(m_codecCtx, 
				m_frame, &frameFinished, 
				packet.data, packet.size);

			if (frameFinished && !posFound) 
			{
				if (packet.dts >= targetTs)
					posFound = 1;
			} 

			if(frameFinished && posFound == 1) 
			{
				AVFrame * input = m_frame;

				/* This means the data wasnt read properly, 
				this check stops crashing */
				if (   input->data[0]==0 && input->data[1]==0 
					&& input->data[2]==0 && input->data[3]==0)
				{
					av_free_packet(&packet);
					break;
				}

				if (m_deinterlace) 
				{
					if (avpicture_deinterlace(
						(AVPicture*) m_frameDeinterlaced,
						(const AVPicture*) m_frame,
						m_codecCtx->pix_fmt,
						m_codecCtx->width,
						m_codecCtx->height) >= 0)
					{
						input = m_frameDeinterlaced;
					}
				}
				// convert to BGR24
				sws_scale(m_imgConvertCtx,
					input->data,
					input->linesize,
					0,
					m_codecCtx->height,
					m_frameBGR->data,
					m_frameBGR->linesize);
				av_free_packet(&packet);
				frameLoaded = true;
				break;
			}
		}
		av_free_packet(&packet);
	}
	if (frameLoaded)
		m_curPosition = position;
	return frameLoaded;
}


// python methods


// cast Image pointer to VideoFFmpeg
inline VideoFFmpeg * getVideoFFmpeg (PyImage * self)
{ return static_cast<VideoFFmpeg*>(self->m_image); }


// object initialization
static int VideoFFmpeg_init (PyObject * pySelf, PyObject * args, PyObject * kwds)
{
	PyImage * self = reinterpret_cast<PyImage*>(pySelf);
	// parameters - video source
	// file name or format type for capture (only for Linux: video4linux or dv1394)
	char * file = NULL;
	// capture device number
	short capt = -1;
	// capture width, only if capt is >= 0
	short width = 0;
	// capture height, only if capt is >= 0
	short height = 0;
	// capture rate, only if capt is >= 0
	float rate = 25.f;

	static char *kwlist[] = {"file", "capture", "rate", "width", "height", NULL};

	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|hfhh", kwlist, &file, &capt,
		&rate, &width, &height))
		return -1; 

	try
	{
		// create video object
		Video_init<VideoFFmpeg>(self);

		// set thread usage
		getVideoFFmpeg(self)->initParams(width, height, rate);

		// open video source
		Video_open(getVideo(self), file, capt);
	}
	catch (Exception & exp)
	{
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}

PyObject * VideoFFmpeg_getPreseek (PyImage *self, void * closure)
{
	return Py_BuildValue("h", getFFmpeg(self)->getPreseek());
}

// set range
int VideoFFmpeg_setPreseek (PyImage * self, PyObject * value, void * closure)
{
	// check validity of parameter
	if (value == NULL || !PyInt_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be an integer");
		return -1;
	}
	// set preseek
	getFFmpeg(self)->setPreseek(PyInt_AsLong(value));
	// success
	return 0;
}

// get deinterlace
PyObject * VideoFFmpeg_getDeinterlace (PyImage * self, void * closure)
{
	if (getFFmpeg(self)->getDeinterlace())
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

// set flip
int VideoFFmpeg_setDeinterlace (PyImage * self, PyObject * value, void * closure)
{
	// check parameter, report failure
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	// set deinterlace
	getFFmpeg(self)->setDeinterlace(value == Py_True);
	// success
	return 0;
}

// methods structure
static PyMethodDef videoMethods[] =
{ // methods from VideoBase class
	{"play", (PyCFunction)Video_play, METH_NOARGS, "Play video"},
	{"stop", (PyCFunction)Video_stop, METH_NOARGS, "Stop (pause) video"},
	{"refresh", (PyCFunction)Video_refresh, METH_NOARGS, "Refresh video - get its status"},
	{NULL}
};
// attributes structure
static PyGetSetDef videoGetSets[] =
{ // methods from VideoBase class
	{"status", (getter)Video_getStatus, NULL, "video status", NULL},
	{"range", (getter)Video_getRange, (setter)Video_setRange, "replay range", NULL},
	{"repeat", (getter)Video_getRepeat, (setter)Video_setRepeat, "repeat count, -1 for infinite repeat", NULL},
	{"framerate", (getter)Video_getFrameRate, (setter)Video_setFrameRate, "frame rate", NULL},
	// attributes from ImageBase class
	{"image", (getter)Image_getImage, NULL, "image data", NULL},
	{"size", (getter)Image_getSize, NULL, "image size", NULL},
	{"scale", (getter)Image_getScale, (setter)Image_setScale, "fast scale of image (near neighbour)", NULL},
	{"flip", (getter)Image_getFlip, (setter)Image_setFlip, "flip image vertically", NULL},
	{"filter", (getter)Image_getFilter, (setter)Image_setFilter, "pixel filter", NULL},
	{"preseek", (getter)VideoFFmpeg_getPreseek, (setter)VideoFFmpeg_setPreseek, "nb of frames of preseek", NULL},
	{"deinterlace", (getter)VideoFFmpeg_getDeinterlace, (setter)VideoFFmpeg_setDeinterlace, "deinterlace image", NULL},
	{NULL}
};

// python type declaration
PyTypeObject VideoFFmpegType =
{ 
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"VideoTexture.VideoFFmpeg",   /*tp_name*/
	sizeof(PyImage),          /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Image_dealloc, /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"FFmpeg video source",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	videoMethods,    /* tp_methods */
	0,                   /* tp_members */
	videoGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)VideoFFmpeg_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};



#endif	//WITH_FFMPEG


