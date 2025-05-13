#!/bin/sh
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

BASE_ARGS="-loglevel warning -hide_banner -y -framerate 3 -loop 1 -i color_chart.png -frames:v 10"
H264_ARGS="-c:v libx264 -preset veryslow -crf 2"
VP9_ARGS="-c:v libvpx-vp9 -lossless 1"

FMT_ARGS="-pix_fmt yuv420p"
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS h264_untagged.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=dst_range=1" -color_range pc h264_untagged_pc.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc iec61966-2-1 -colorspace bt709 h264_bt709_srgb.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc bt709 -colorspace bt709 h264_bt709.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709:dst_range=1" -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range pc h264_bt709_pc.mp4
ffmpeg $BASE_ARGS -pix_fmt yuv420p10le $H264_ARGS h264_10bpp_untagged.mp4
ffmpeg $BASE_ARGS -pix_fmt yuv420p10le $H264_ARGS -vf "scale=out_color_matrix=bt2020" -color_primaries bt2020 -color_trc bt2020-10 -colorspace bt2020nc h264_10bpp_bt2020.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $VP9_ARGS vp9_untagged.webm
ffmpeg $BASE_ARGS $FMT_ARGS $VP9_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc bt709 -colorspace bt709 vp9_bt709.webm

FMT_ARGS="-pix_fmt yuv444p"
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS h264_444_untagged.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=dst_range=1" -color_range pc h264_444_untagged_pc.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc iec61966-2-1 -colorspace bt709 h264_444_bt709_srgb.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc bt709 -colorspace bt709 h264_444_bt709.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $H264_ARGS -vf "scale=out_color_matrix=bt709:dst_range=1" -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range pc h264_444_bt709_pc.mp4
ffmpeg $BASE_ARGS -pix_fmt yuv444p10le $H264_ARGS h264_444_10bpp_untagged.mp4
ffmpeg $BASE_ARGS -pix_fmt yuv444p10le $H264_ARGS -vf "scale=out_color_matrix=bt2020" -color_primaries bt2020 -color_trc bt2020-10 -colorspace bt2020nc h264_444_10bpp_bt2020.mp4
ffmpeg $BASE_ARGS $FMT_ARGS $VP9_ARGS vp9_444_untagged.webm
ffmpeg $BASE_ARGS $FMT_ARGS $VP9_ARGS -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc bt709 -colorspace bt709 vp9_444_bt709.webm

BASE_ARGS="-loglevel warning -hide_banner -y -framerate 3 -loop 1 -i gradients16.png -frames:v 10"
ffmpeg $BASE_ARGS $H264_ARGS -pix_fmt yuv420p grad_h264.mp4
ffmpeg $BASE_ARGS $H264_ARGS -pix_fmt yuv420p -vf "scale=dst_range=1" -color_range pc grad_h264_pc.mp4
ffmpeg $BASE_ARGS $H264_ARGS -pix_fmt yuv420p10le grad_h264_10bpp.mp4
ffmpeg $BASE_ARGS $H264_ARGS -pix_fmt yuv420p -vf "scale=out_color_matrix=bt709" -color_primaries bt709 -color_trc bt709 -colorspace bt709 grad_h264_bt709.mp4
