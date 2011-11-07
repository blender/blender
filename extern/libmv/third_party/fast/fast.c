#include <stdlib.h>
#include "fast.h"


xy* fast9_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners)
{
	xy* corners;
	int num_corners;
	int* scores;
	xy* nonmax;

	corners = fast9_detect(im, xsize, ysize, stride, b, &num_corners);
	scores = fast9_score(im, stride, corners, num_corners, b);
	nonmax = nonmax_suppression(corners, scores, num_corners, ret_num_corners);

	free(corners);
	free(scores);

	return nonmax;
}

xy* fast10_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners)
{
	xy* corners;
	int num_corners;
	int* scores;
	xy* nonmax;

	corners = fast10_detect(im, xsize, ysize, stride, b, &num_corners);
	scores = fast10_score(im, stride, corners, num_corners, b);
	nonmax = nonmax_suppression(corners, scores, num_corners, ret_num_corners);

	free(corners);
	free(scores);

	return nonmax;
}

xy* fast11_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners)
{
	xy* corners;
	int num_corners;
	int* scores;
	xy* nonmax;

	corners = fast11_detect(im, xsize, ysize, stride, b, &num_corners);
	scores = fast11_score(im, stride, corners, num_corners, b);
	nonmax = nonmax_suppression(corners, scores, num_corners, ret_num_corners);

	free(corners);
	free(scores);

	return nonmax;
}

xy* fast12_detect_nonmax(const byte* im, int xsize, int ysize, int stride, int b, int* ret_num_corners)
{
	xy* corners;
	int num_corners;
	int* scores;
	xy* nonmax;

	corners = fast12_detect(im, xsize, ysize, stride, b, &num_corners);
	scores = fast12_score(im, stride, corners, num_corners, b);
	nonmax = nonmax_suppression(corners, scores, num_corners, ret_num_corners);

	free(corners);
	free(scores);

	return nonmax;
}
