/* SPDX-FileCopyrightText: 2005 `Gernot Ziegler <gz@lysator.liu.se>`. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup openexr
 */

#include "BLI_string_ref.hh"
#include "IMB_openexr.hh"

ExrHandle *IMB_exr_get_handle(bool /*write_multipart*/)
{
  return nullptr;
}
void IMB_exr_add_channels(ExrHandle * /*handle*/,
                          blender::StringRefNull /*layerpassname*/,
                          blender::StringRefNull /*channelnames*/,
                          blender::StringRefNull /*viewname*/,
                          blender::StringRefNull /*colorspace*/,
                          size_t /*xstride*/,
                          size_t /*ystride*/,
                          float * /*rect*/,
                          bool /*use_half_float*/)
{
}

bool IMB_exr_begin_read(ExrHandle * /*handle*/,
                        const char * /*filepath*/,
                        int * /*width*/,
                        int * /*height*/,
                        const bool /*add_channels*/)
{
  return false;
}
bool IMB_exr_begin_write(ExrHandle * /*handle*/,
                         const char * /*filepath*/,
                         int /*width*/,
                         int /*height*/,
                         const double /*ppm*/[2],
                         int /*compress*/,
                         int /*quality*/,
                         const StampData * /*stamp*/)
{
  return false;
}

bool IMB_exr_set_channel(ExrHandle * /*handle*/,
                         blender::StringRefNull /*full_name*/,
                         int /*xstride*/,
                         int /*ystride*/,
                         float * /*rect*/)
{
  return false;
}

void IMB_exr_read_channels(ExrHandle * /*handle*/) {}
void IMB_exr_write_channels(ExrHandle * /*handle*/) {}

void IMB_exr_multilayer_convert(ExrHandle * /*handle*/,
                                void * /*base*/,
                                void *(* /*addview*/)(void *base, const char *str),
                                void *(* /*addlayer*/)(void *base, const char *str),
                                void (* /*addpass*/)(void *base,
                                                     void *lay,
                                                     const char *str,
                                                     float *rect,
                                                     int totchan,
                                                     const char *chan_id,
                                                     const char *view))
{
}

void IMB_exr_close(ExrHandle * /*handle*/) {}

void IMB_exr_add_view(ExrHandle * /*handle*/, const char * /*name*/) {}
bool IMB_exr_has_multilayer(ExrHandle * /*handle*/)
{
  return false;
}

bool IMB_exr_get_ppm(ExrHandle * /*handle*/, double /*ppm*/[2])
{
  return false;
}
