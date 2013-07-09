#ifndef FAST_H
#define FAST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int x, y; } xy; 
typedef unsigned char byte;

int fast9_corner_score(const byte* p, const int pixel[], int bstart);
int fast10_corner_score(const byte* p, const int pixel[], int bstart);
int fast11_corner_score(const byte* p, const int pixel[], int bstart);
int fast12_corner_score(const byte* p, const int pixel[], int bstart);

xy* fast9_detect(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast10_detect(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast11_detect(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast12_detect(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);

int* fast9_score(const byte* i, int stride, xy* corners, int num_corners, int b);
int* fast10_score(const byte* i, int stride, xy* corners, int num_corners, int b);
int* fast11_score(const byte* i, int stride, xy* corners, int num_corners, int b);
int* fast12_score(const byte* i, int stride, xy* corners, int num_corners, int b);


xy* fast9_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast10_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast11_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);
xy* fast12_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners);

xy* nonmax_suppression(const xy* corners, const int* scores, int num_corners, int* ret_num_nonmax);


#ifdef __cplusplus
}
#endif

#endif
