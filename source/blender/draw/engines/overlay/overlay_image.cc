/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_camera.h"
#include "BKE_image.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"

#include "BLI_listbase.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "IMB_imbuf_types.h"

#include "overlay_private.hh"

void OVERLAY_image_init(OVERLAY_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  DRWView *default_view = (DRWView *)DRW_view_default_get();
  pd->view_reference_images = DRW_view_create_with_zoffset(default_view, draw_ctx->rv3d, -1.0f);
}

void OVERLAY_image_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DRWState state;

  state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_GREATER | DRW_STATE_BLEND_ALPHA_PREMUL;
  DRW_PASS_CREATE(psl->image_background_ps, state);
  state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_UNDER_PREMUL;
  DRW_PASS_CREATE(psl->image_background_scene_ps, state);

  state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
  DRW_PASS_CREATE(psl->image_empties_ps, state | pd->clipping_state);

  state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA_PREMUL;
  DRW_PASS_CREATE(psl->image_empties_back_ps, state | pd->clipping_state);
  DRW_PASS_CREATE(psl->image_empties_blend_ps, state | pd->clipping_state);

  state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
  DRW_PASS_CREATE(psl->image_empties_front_ps, state);
  DRW_PASS_CREATE(psl->image_foreground_ps, state);
  DRW_PASS_CREATE(psl->image_foreground_scene_ps, state);
}

static void overlay_image_calc_aspect(Image *ima, const int size[2], float r_image_aspect[2])
{
  float ima_x, ima_y;
  if (ima) {
    ima_x = size[0];
    ima_y = size[1];
  }
  else {
    /* if no image, make it a 1x1 empty square, honor scale & offset */
    ima_x = ima_y = 1.0f;
  }
  /* Get the image aspect even if the buffer is invalid */
  float sca_x = 1.0f, sca_y = 1.0f;
  if (ima) {
    if (ima->aspx > ima->aspy) {
      sca_y = ima->aspy / ima->aspx;
    }
    else if (ima->aspx < ima->aspy) {
      sca_x = ima->aspx / ima->aspy;
    }
  }

  const float scale_x_inv = ima_x * sca_x;
  const float scale_y_inv = ima_y * sca_y;
  if (scale_x_inv > scale_y_inv) {
    r_image_aspect[0] = 1.0f;
    r_image_aspect[1] = scale_y_inv / scale_x_inv;
  }
  else {
    r_image_aspect[0] = scale_x_inv / scale_y_inv;
    r_image_aspect[1] = 1.0f;
  }
}

static eStereoViews camera_background_images_stereo_eye(const Scene *scene, const View3D *v3d)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return STEREO_LEFT_ID;
  }
  if (v3d->stereo3d_camera != STEREO_3D_ID) {
    /* show only left or right camera */
    return eStereoViews(v3d->stereo3d_camera);
  }

  return eStereoViews(v3d->multiview_eye);
}

static void camera_background_images_stereo_setup(const Scene *scene,
                                                  const View3D *v3d,
                                                  Image *ima,
                                                  ImageUser *iuser)
{
  if (BKE_image_is_stereo(ima)) {
    iuser->flag |= IMA_SHOW_STEREO;
    iuser->multiview_eye = camera_background_images_stereo_eye(scene, v3d);
    BKE_image_multiview_index(ima, iuser);
  }
  else {
    iuser->flag &= ~IMA_SHOW_STEREO;
  }
}

static GPUTexture *image_camera_background_texture_get(CameraBGImage *bgpic,
                                                       const DRWContextState *draw_ctx,
                                                       OVERLAY_PrivateData *pd,
                                                       float *r_aspect,
                                                       bool *r_use_alpha_premult,
                                                       bool *r_use_view_transform)
{
  void *lock;
  Image *image = bgpic->ima;
  ImageUser *iuser = &bgpic->iuser;
  MovieClip *clip = nullptr;
  GPUTexture *tex = nullptr;
  Scene *scene = draw_ctx->scene;
  float aspect_x, aspect_y;
  int width, height;
  int ctime = int(DEG_get_ctime(draw_ctx->depsgraph));
  *r_use_alpha_premult = false;
  *r_use_view_transform = false;

  switch (bgpic->source) {
    case CAM_BGIMG_SOURCE_IMAGE: {
      if (image == nullptr) {
        return nullptr;
      }
      *r_use_alpha_premult = (image->alpha_mode == IMA_ALPHA_PREMUL);
      *r_use_view_transform = (image->flag & IMA_VIEW_AS_RENDER) != 0;

      BKE_image_user_frame_calc(image, iuser, ctime);
      if (image->source == IMA_SRC_SEQUENCE && !(iuser->flag & IMA_USER_FRAME_IN_RANGE)) {
        /* Frame is out of range, don't show. */
        return nullptr;
      }

      camera_background_images_stereo_setup(scene, draw_ctx->v3d, image, iuser);

      iuser->scene = draw_ctx->scene;
      ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, &lock);
      if (ibuf == nullptr) {
        BKE_image_release_ibuf(image, ibuf, lock);
        iuser->scene = nullptr;
        return nullptr;
      }
      width = ibuf->x;
      height = ibuf->y;
      tex = BKE_image_get_gpu_texture(image, iuser, ibuf);
      BKE_image_release_ibuf(image, ibuf, lock);
      iuser->scene = nullptr;

      if (tex == nullptr) {
        return nullptr;
      }

      aspect_x = bgpic->ima->aspx;
      aspect_y = bgpic->ima->aspy;
      break;
    }

    case CAM_BGIMG_SOURCE_MOVIE: {
      if (bgpic->flag & CAM_BGIMG_FLAG_CAMERACLIP) {
        if (scene->camera) {
          clip = BKE_object_movieclip_get(scene, scene->camera, true);
        }
      }
      else {
        clip = bgpic->clip;
      }

      if (clip == nullptr) {
        return nullptr;
      }

      BKE_movieclip_user_set_frame(&bgpic->cuser, ctime);
      tex = BKE_movieclip_get_gpu_texture(clip, &bgpic->cuser);
      if (tex == nullptr) {
        return nullptr;
      }

      aspect_x = clip->aspx;
      aspect_y = clip->aspy;
      *r_use_view_transform = true;

      BKE_movieclip_get_size(clip, &bgpic->cuser, &width, &height);

      /* Save for freeing. */
      BLI_addtail(&pd->bg_movie_clips, BLI_genericNodeN(clip));
      break;
    }

    default:
      /* Unsupported type. */
      return nullptr;
  }

  *r_aspect = (width * aspect_x) / (height * aspect_y);
  return tex;
}

static void OVERLAY_image_free_movieclips_textures(OVERLAY_Data *data)
{
  /* Free Movie clip textures after rendering */
  while (LinkData *link = static_cast<LinkData *>(BLI_pophead(&data->stl->pd->bg_movie_clips))) {
    MovieClip *clip = (MovieClip *)link->data;
    BKE_movieclip_free_gputexture(clip);
    MEM_freeN(link);
  }
}

static void image_camera_background_matrix_get(const Camera *cam,
                                               const CameraBGImage *bgpic,
                                               const DRWContextState *draw_ctx,
                                               const float image_aspect,
                                               float rmat[4][4])
{
  float rotate[4][4], scale[4][4], translate[4][4];

  axis_angle_to_mat4_single(rotate, 'Z', -bgpic->rotation);
  unit_m4(scale);
  unit_m4(translate);

  /* Normalized Object space camera frame corners. */
  float cam_corners[4][3];
  BKE_camera_view_frame(draw_ctx->scene, cam, cam_corners);
  float cam_width = fabsf(cam_corners[0][0] - cam_corners[3][0]);
  float cam_height = fabsf(cam_corners[0][1] - cam_corners[1][1]);
  float cam_aspect = cam_width / cam_height;

  if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_CROP) {
    /* Crop. */
    if (image_aspect > cam_aspect) {
      scale[0][0] *= cam_height * image_aspect;
      scale[1][1] *= cam_height;
    }
    else {
      scale[0][0] *= cam_width;
      scale[1][1] *= cam_width / image_aspect;
    }
  }
  else if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_ASPECT) {
    /* Fit. */
    if (image_aspect > cam_aspect) {
      scale[0][0] *= cam_width;
      scale[1][1] *= cam_width / image_aspect;
    }
    else {
      scale[0][0] *= cam_height * image_aspect;
      scale[1][1] *= cam_height;
    }
  }
  else {
    /* Stretch. */
    scale[0][0] *= cam_width;
    scale[1][1] *= cam_height;
  }

  translate[3][0] = bgpic->offset[0];
  translate[3][1] = bgpic->offset[1];
  translate[3][2] = cam_corners[0][2];
  if (cam->type == CAM_ORTHO) {
    mul_v2_fl(translate[3], cam->ortho_scale);
  }
  /* These lines are for keeping 2.80 behavior and could be removed to keep 2.79 behavior. */
  translate[3][0] *= min_ff(1.0f, cam_aspect);
  translate[3][1] /= max_ff(1.0f, cam_aspect) * (image_aspect / cam_aspect);
  /* quad is -1..1 so divide by 2. */
  scale[0][0] *= 0.5f * bgpic->scale * ((bgpic->flag & CAM_BGIMG_FLAG_FLIP_X) ? -1.0 : 1.0);
  scale[1][1] *= 0.5f * bgpic->scale * ((bgpic->flag & CAM_BGIMG_FLAG_FLIP_Y) ? -1.0 : 1.0);
  /* Camera shift. (middle of cam_corners) */
  translate[3][0] += (cam_corners[0][0] + cam_corners[2][0]) * 0.5f;
  translate[3][1] += (cam_corners[0][1] + cam_corners[2][1]) * 0.5f;

  mul_m4_series(rmat, translate, rotate, scale);
}

void OVERLAY_image_camera_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const View3D *v3d = draw_ctx->v3d;
  const Scene *scene = draw_ctx->scene;
  Camera *cam = static_cast<Camera *>(ob->data);

  const bool show_frame = BKE_object_empty_image_frame_is_visible_in_view3d(ob, draw_ctx->rv3d);

  if (!show_frame || DRW_state_is_select()) {
    return;
  }

  const bool stereo_eye = camera_background_images_stereo_eye(scene, v3d) == STEREO_LEFT_ID;
  const char *viewname = (stereo_eye == STEREO_LEFT_ID) ? STEREO_RIGHT_NAME : STEREO_LEFT_NAME;
  float modelmat[4][4];
  BKE_camera_multiview_model_matrix(&scene->r, ob, viewname, modelmat);

  LISTBASE_FOREACH (CameraBGImage *, bgpic, &cam->bg_images) {
    if (bgpic->flag & CAM_BGIMG_FLAG_DISABLED) {
      continue;
    }

    float aspect = 1.0;
    bool use_alpha_premult;
    bool use_view_transform = false;
    float mat[4][4];

    /* retrieve the image we want to show, continue to next when no image could be found */
    GPUTexture *tex = image_camera_background_texture_get(
        bgpic, draw_ctx, pd, &aspect, &use_alpha_premult, &use_view_transform);

    if (tex) {
      image_camera_background_matrix_get(cam, bgpic, draw_ctx, aspect, mat);

      const bool is_foreground = (bgpic->flag & CAM_BGIMG_FLAG_FOREGROUND) != 0;
      /* Alpha is clamped just below 1.0 to fix background images to interfere with foreground
       * images. Without this a background image with 1.0 will be rendered on top of a transparent
       * foreground image due to the different blending modes they use. */
      const float color_premult_alpha[4] = {1.0f, 1.0f, 1.0f, std::min(bgpic->alpha, 0.999999f)};

      DRWPass *pass = is_foreground ? (use_view_transform ? psl->image_foreground_scene_ps :
                                                            psl->image_foreground_ps) :
                                      (use_view_transform ? psl->image_background_scene_ps :
                                                            psl->image_background_ps);

      GPUShader *sh = OVERLAY_shader_image();
      DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
      DRW_shgroup_uniform_texture(grp, "imgTexture", tex);
      DRW_shgroup_uniform_bool_copy(grp, "imgPremultiplied", use_alpha_premult);
      DRW_shgroup_uniform_bool_copy(grp, "imgAlphaBlend", true);
      DRW_shgroup_uniform_bool_copy(grp, "isCameraBackground", true);
      DRW_shgroup_uniform_bool_copy(grp, "depthSet", true);
      DRW_shgroup_uniform_vec4_copy(grp, "ucolor", color_premult_alpha);
      DRW_shgroup_call_obmat(grp, DRW_cache_quad_get(), mat);
    }
  }
}

void OVERLAY_image_empty_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PassList *psl = vedata->psl;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const RegionView3D *rv3d = draw_ctx->rv3d;
  GPUTexture *tex = nullptr;
  Image *ima = static_cast<Image *>(ob->data);
  float mat[4][4];

  const bool show_frame = BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d);
  const bool show_image = show_frame && BKE_object_empty_image_data_is_visible_in_view3d(ob, rv3d);
  const bool use_alpha_blend = (ob->empty_image_flag & OB_EMPTY_IMAGE_USE_ALPHA_BLEND) != 0;
  const bool use_alpha_premult = ima && (ima->alpha_mode == IMA_ALPHA_PREMUL);

  if (!show_frame) {
    return;
  }

  {
    /* Calling 'BKE_image_get_size' may free the texture. Get the size from 'tex' instead,
     * see: #59347 */
    int size[2] = {0};
    if (ima != nullptr) {
      ImageUser iuser = *ob->iuser;
      camera_background_images_stereo_setup(draw_ctx->scene, draw_ctx->v3d, ima, &iuser);
      tex = BKE_image_get_gpu_texture(ima, &iuser, nullptr);
      if (tex) {
        size[0] = GPU_texture_original_width(tex);
        size[1] = GPU_texture_original_height(tex);
      }
    }
    CLAMP_MIN(size[0], 1);
    CLAMP_MIN(size[1], 1);

    float image_aspect[2];
    overlay_image_calc_aspect(ima, size, image_aspect);

    copy_m4_m4(mat, ob->object_to_world);
    mul_v3_fl(mat[0], image_aspect[0] * 0.5f * ob->empty_drawsize);
    mul_v3_fl(mat[1], image_aspect[1] * 0.5f * ob->empty_drawsize);
    madd_v3_v3fl(mat[3], mat[0], ob->ima_ofs[0] * 2.0f + 1.0f);
    madd_v3_v3fl(mat[3], mat[1], ob->ima_ofs[1] * 2.0f + 1.0f);
  }

  /* Use the actual depth if we are doing depth tests to determine the distance to the object */
  char depth_mode = DRW_state_is_depth() ? char(OB_EMPTY_IMAGE_DEPTH_DEFAULT) :
                                           ob->empty_image_depth;
  DRWPass *pass = nullptr;
  if ((ob->dtx & OB_DRAW_IN_FRONT) != 0) {
    /* Object In Front overrides image empty depth mode. */
    pass = psl->image_empties_front_ps;
  }
  else {
    switch (depth_mode) {
      case OB_EMPTY_IMAGE_DEPTH_DEFAULT:
        pass = (use_alpha_blend) ? psl->image_empties_blend_ps : psl->image_empties_ps;
        break;
      case OB_EMPTY_IMAGE_DEPTH_BACK:
        pass = psl->image_empties_back_ps;
        break;
      case OB_EMPTY_IMAGE_DEPTH_FRONT:
        pass = psl->image_empties_front_ps;
        break;
    }
  }

  if (show_frame) {
    OVERLAY_ExtraCallBuffers *cb = OVERLAY_extra_call_buffer_get(vedata, ob);
    float *color;
    DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);
    OVERLAY_empty_shape(cb, mat, 1.0f, OB_EMPTY_IMAGE, color);
  }

  if (show_image && tex && ((ob->color[3] > 0.0f) || !use_alpha_blend)) {
    GPUShader *sh = OVERLAY_shader_image();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
    DRW_shgroup_uniform_texture(grp, "imgTexture", tex);
    DRW_shgroup_uniform_bool_copy(grp, "imgPremultiplied", use_alpha_premult);
    DRW_shgroup_uniform_bool_copy(grp, "imgAlphaBlend", use_alpha_blend);
    DRW_shgroup_uniform_bool_copy(grp, "isCameraBackground", false);
    DRW_shgroup_uniform_bool_copy(grp, "depthSet", depth_mode != OB_EMPTY_IMAGE_DEPTH_DEFAULT);
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", ob->color);
    DRW_shgroup_call_obmat(grp, DRW_cache_quad_get(), mat);
  }
}

void OVERLAY_image_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_pass_sort_shgroup_z(psl->image_empties_blend_ps);
  DRW_pass_sort_shgroup_z(psl->image_empties_front_ps);
  DRW_pass_sort_shgroup_z(psl->image_empties_back_ps);
}

void OVERLAY_image_scene_background_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (DRW_state_is_fbo() && (!DRW_pass_is_empty(psl->image_background_scene_ps) ||
                             !DRW_pass_is_empty(psl->image_foreground_scene_ps)))
  {
    const DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->default_fb);

    DRW_draw_pass(psl->image_background_scene_ps);
    DRW_draw_pass(psl->image_foreground_scene_ps);
  }
}

void OVERLAY_image_background_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->image_background_ps);
  DRW_draw_pass(psl->image_empties_back_ps);
}

void OVERLAY_image_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  DRW_view_set_active(pd->view_reference_images);

  DRW_draw_pass(psl->image_empties_ps);
  DRW_draw_pass(psl->image_empties_blend_ps);

  DRW_view_set_active(nullptr);
}

void OVERLAY_image_in_front_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  DRW_view_set_active(pd->view_reference_images);

  DRW_draw_pass(psl->image_empties_front_ps);
  DRW_draw_pass(psl->image_foreground_ps);

  DRW_view_set_active(nullptr);

  OVERLAY_image_free_movieclips_textures(vedata);
}
