#ifndef __redcode_codec_h_included__
#define __redcode_codec_h_included__

struct redcode_frame;

struct redcode_frame_raw {
	void * data;
	int width;
	int height;
};

/* do the JPEG2000 decompression into YCbCrY planes */
struct redcode_frame_raw * redcode_decode_video_raw(
	struct redcode_frame * frame, int scale);

/* finally decode RAW frame into out-buffer (which has to be allocated
   in advance)

   Keep in mind: frame_raw-width + height is half sized. 
   (one pixel contains 2x2 bayer-sensor data)

   output-buffer should have room for

   scale = 1 : width * height * 4 * 4 * sizeof(float)
   scale = 2 : width * height * 4 * sizeof(float)
   scale = 4 : width * height * sizeof(float)

*/

int redcode_decode_video_float(struct redcode_frame_raw * frame, 
			       float * out, int scale);


#endif
