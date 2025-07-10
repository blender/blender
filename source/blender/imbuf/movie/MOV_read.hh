/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * Movie file reading / playback functions.
 */

#pragma once

#include "IMB_imbuf_enums.h"

#include "MOV_enums.hh"

#include "BLI_set.hh"

#include <string>

struct IDProperty;
struct ImBuf;
struct MovieReader;
struct MovieProxyBuilder;

/**
 * Opens a movie file for reading / playback.
 * ib_flags are `IB_` ImBuf bitmask (only IB_animdeinterlace is taken into account).
 * streamindex is for multi-track movie files.
 *
 * Returned MovieReader object can be used in other playback related functions.
 * Note that a valid object will be returned even if file does not exist or is
 * not a video file. The actual initialization is delayed until
 * #MOV_decode_frame is called.
 *
 * When done with playback, use #MOV_close to delete it.
 */
MovieReader *MOV_open_file(const char *filepath,
                           int ib_flags,
                           int streamindex,
                           bool keep_original_colorspace,
                           char colorspace[IM_MAX_SPACE]);

/**
 * Release memory and other resources associated with movie playback.
 */
void MOV_close(MovieReader *anim);

/**
 * Fetches a frame from a movie at given frame position.
 *
 * Internally this will seek within the movie as/if needed. For most movie
 * files, decoding frames sequentially is much more efficient than decoding
 * random frames.
 *
 * If proxy_size is not IMB_PROXY_NONE, a proxy file of given size will
 * be attempted. If it exists, the frame will be decoded from it. If the
 * proxy does not exist, original file will be used.
 *
 * Movies that are <= 8 bits/color channel are returned as byte images;
 * higher bit depth movies are returned as float images. Note that the
 * color space is returned as-is, i.e. a float image might not be in
 * linear space.
 *
 * Returned image can be null if movie file does not exist, is not supported
 * or failed decoding.
 */
ImBuf *MOV_decode_frame(MovieReader *anim,
                        int position,
                        IMB_Timecode_Type tc /* = 1 = IMB_TC_RECORD_RUN */,
                        IMB_Proxy_Size preview_size /* = 0 = IMB_PROXY_NONE */);

/**
 * Fetches a frame from a movie used for preview/thumbnails.
 * The frame will be halfway into the file duration.
 * Thumbnail related metadata ("Thumb::Video::*") will be set on the
 * returned image.
 */
ImBuf *MOV_decode_preview_frame(MovieReader *anim);

/**
 * Return the length (in frames) of the movie.
 */
int MOV_get_duration_frames(MovieReader *anim, IMB_Timecode_Type tc);

/**
 * Return the encoded start offset (in seconds) of the movie.
 */
double MOV_get_start_offset_seconds(const MovieReader *anim);

/**
 * Returns the frames per second of the movie, or zero if
 * the information is not available. Note that if you want the
 * most accurate representation, use #MOV_get_fps_num_denom.
 */
float MOV_get_fps(const MovieReader *anim);

/**
 * Returns the frames per second of the movie as numerator and
 * denominator. False will be returned if the information is
 * not available.
 */
bool MOV_get_fps_num_denom(const MovieReader *anim, short &r_fps_num, float &r_fps_denom);

/**
 * Get movie image width in pixels.
 */
int MOV_get_image_width(const MovieReader *anim);

/**
 * Get movie image height in pixels.
 */
int MOV_get_image_height(const MovieReader *anim);

/**
 * Returns true if movie playback has been fully initialized
 * and is supported. Note that immediately after #MOV_open_file
 * the playback is not initialized yet.
 */
bool MOV_is_initialized_and_valid(const MovieReader *anim);

/**
 * Gets filename (without the folder) part of the movie.
 */
void MOV_get_filename(const MovieReader *anim, char *filename, int filename_maxncpy);

/**
 * Loads metadata of the movie.
 * Metadata is only loaded for already initialized movies.
 */
IDProperty *MOV_load_metadata(MovieReader *anim);

/*-------------------------------------------------------------------- */
/*
 * Movie proxy / timecode index related functionality.
 */

/**
 * Sets multi-view suffix to be used when building proxies for this movie.
 */
void MOV_set_multiview_suffix(MovieReader *anim, const char *suffix);

/**
 * Close any internally opened proxies of this movie.
 */
void MOV_close_proxies(MovieReader *anim);

/**
 * Custom directory to be used for loading or building proxies.
 * By default "BL_proxy" within the directory of the movie file is used.
 */
void MOV_set_custom_proxy_dir(MovieReader *anim, const char *dir);

/**
 * Given a frame index, calculate final frame index taking timecode into account.
 *
 * This does nothing (returns input frame position) if #IMB_TC_NONE is used,
 * or movie proxy/index file is not built.
 *
 * When a timecode index file is present and is requested to be used, this can
 * return a different frame index than input frame, particularly for
 * #IMB_TC_RECORD_RUN_NO_GAPS.
 */
int MOV_calc_frame_index_with_timecode(MovieReader *anim, IMB_Timecode_Type tc, int position);

/**
 * Queries which proxies exist for this movie.
 *
 * Note that it does not check whether proxies are up to date,
 * or valid files; just merely whether the expected files exist.
 *
 * Returns bitmask of #IMB_Proxy_Size flags.
 */
int MOV_get_existing_proxies(const MovieReader *anim);

/**
 * Initialize movie proxies / time-code indices builder.
 */
MovieProxyBuilder *MOV_proxy_builder_start(MovieReader *anim,
                                           IMB_Timecode_Type tcs_in_use,
                                           int proxy_sizes_in_use,
                                           int quality,
                                           const bool overwrite,
                                           blender::Set<std::string> *processed_paths,
                                           bool build_only_on_bad_performance);

/**
 * Will rebuild all used indices and proxies at once.
 */
void MOV_proxy_builder_process(MovieProxyBuilder *context,
                               bool *stop,
                               bool *do_update,
                               float *progress);

/**
 * Finish building proxies / time-codes indices, and delete the builder.
 */
void MOV_proxy_builder_finish(MovieProxyBuilder *context, bool stop);
