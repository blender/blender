#ifndef __redcode_format_h_included__
#define __redcode_format_h_included__

struct redcode_handle;
struct redcode_frame {
	unsigned int length;
	unsigned int offset;
	unsigned char * data;
};

struct redcode_handle * redcode_open(const char * fname);
void redcode_close(struct redcode_handle * handle);

long redcode_get_length(struct redcode_handle * handle);

struct redcode_frame * redcode_read_video_frame(
	struct redcode_handle * handle, long frame);
struct redcode_frame * redcode_read_audio_frame(
	struct redcode_handle * handle, long frame);

void redcode_free_frame(struct redcode_frame * frame);


#endif
