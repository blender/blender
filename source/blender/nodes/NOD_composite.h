/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Composite;

/* ****************** types array for all composite nodes ****************** */

void register_node_tree_type_cmp(void);

void register_node_type_cmp_group(void);

void register_node_type_cmp_rlayers(void);
void register_node_type_cmp_image(void);
void register_node_type_cmp_texture(void);
void register_node_type_cmp_value(void);
void register_node_type_cmp_rgb(void);
void register_node_type_cmp_curve_time(void);
void register_node_type_cmp_scene_time(void);
void register_node_type_cmp_movieclip(void);

void register_node_type_cmp_composite(void);
void register_node_type_cmp_viewer(void);
void register_node_type_cmp_splitviewer(void);
void register_node_type_cmp_output_file(void);
void register_node_type_cmp_view_levels(void);

void register_node_type_cmp_curve_rgb(void);
void register_node_type_cmp_mix_rgb(void);
void register_node_type_cmp_hue_sat(void);
void register_node_type_cmp_brightcontrast(void);
void register_node_type_cmp_gamma(void);
void register_node_type_cmp_exposure(void);
void register_node_type_cmp_invert(void);
void register_node_type_cmp_alphaover(void);
void register_node_type_cmp_zcombine(void);
void register_node_type_cmp_colorbalance(void);
void register_node_type_cmp_huecorrect(void);
void register_node_type_cmp_convert_color_space(void);

void register_node_type_cmp_normal(void);
void register_node_type_cmp_curve_vec(void);
void register_node_type_cmp_map_value(void);
void register_node_type_cmp_map_range(void);
void register_node_type_cmp_normalize(void);

void register_node_type_cmp_filter(void);
void register_node_type_cmp_blur(void);
void register_node_type_cmp_dblur(void);
void register_node_type_cmp_bilateralblur(void);
void register_node_type_cmp_vecblur(void);
void register_node_type_cmp_dilateerode(void);
void register_node_type_cmp_inpaint(void);
void register_node_type_cmp_despeckle(void);
void register_node_type_cmp_defocus(void);
void register_node_type_cmp_denoise(void);
void register_node_type_cmp_antialiasing(void);
void register_node_type_cmp_posterize(void);

void register_node_type_cmp_valtorgb(void);
void register_node_type_cmp_rgbtobw(void);
void register_node_type_cmp_setalpha(void);
void register_node_type_cmp_idmask(void);
void register_node_type_cmp_math(void);
void register_node_type_cmp_seprgba(void);
void register_node_type_cmp_combrgba(void);
void register_node_type_cmp_sephsva(void);
void register_node_type_cmp_combhsva(void);
void register_node_type_cmp_sepyuva(void);
void register_node_type_cmp_combyuva(void);
void register_node_type_cmp_sepycca(void);
void register_node_type_cmp_combycca(void);
void register_node_type_cmp_premulkey(void);

void register_node_type_cmp_diff_matte(void);
void register_node_type_cmp_distance_matte(void);
void register_node_type_cmp_chroma_matte(void);
void register_node_type_cmp_color_matte(void);
void register_node_type_cmp_channel_matte(void);
void register_node_type_cmp_color_spill(void);
void register_node_type_cmp_luma_matte(void);
void register_node_type_cmp_doubleedgemask(void);
void register_node_type_cmp_keyingscreen(void);
void register_node_type_cmp_keying(void);
void register_node_type_cmp_cryptomatte(void);
void register_node_type_cmp_cryptomatte_legacy(void);

void register_node_type_cmp_translate(void);
void register_node_type_cmp_rotate(void);
void register_node_type_cmp_scale(void);
void register_node_type_cmp_flip(void);
void register_node_type_cmp_crop(void);
void register_node_type_cmp_displace(void);
void register_node_type_cmp_mapuv(void);
void register_node_type_cmp_transform(void);
void register_node_type_cmp_stabilize2d(void);
void register_node_type_cmp_moviedistortion(void);
void register_node_type_cmp_mask(void);

void register_node_type_cmp_glare(void);
void register_node_type_cmp_tonemap(void);
void register_node_type_cmp_lensdist(void);
void register_node_type_cmp_sunbeams(void);

void register_node_type_cmp_colorcorrection(void);
void register_node_type_cmp_boxmask(void);
void register_node_type_cmp_ellipsemask(void);
void register_node_type_cmp_bokehimage(void);
void register_node_type_cmp_bokehblur(void);
void register_node_type_cmp_switch(void);
void register_node_type_cmp_switch_view(void);
void register_node_type_cmp_pixelate(void);
void register_node_type_cmp_trackpos(void);
void register_node_type_cmp_planetrackdeform(void);
void register_node_type_cmp_cornerpin(void);

void node_cmp_rlayers_outputs(struct bNodeTree *ntree, struct bNode *node);
void node_cmp_rlayers_register_pass(struct bNodeTree *ntree,
                                    struct bNode *node,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    const char *name,
                                    eNodeSocketDatatype type);
const char *node_cmp_rlayers_sock_to_pass(int sock_index);

void register_node_type_cmp_custom_group(bNodeType *ntype);

void ntreeCompositExecTree(struct Scene *scene,
                           struct bNodeTree *ntree,
                           struct RenderData *rd,
                           int rendering,
                           int do_previews,
                           const struct ColorManagedViewSettings *view_settings,
                           const struct ColorManagedDisplaySettings *display_settings,
                           const char *view_name);

/**
 * Called from render pipeline, to tag render input and output.
 * need to do all scenes, to prevent errors when you re-render 1 scene.
 */
void ntreeCompositTagRender(struct Scene *scene);

/**
 * Update the outputs of the render layer nodes.
 * Since the outputs depend on the render engine, this part is a bit complex:
 * - #ntreeCompositUpdateRLayers is called and loops over all render layer nodes.
 * - Each render layer node calls the update function of the
 *   render engine that's used for its scene.
 * - The render engine calls RE_engine_register_pass for each pass.
 * - #RE_engine_register_pass calls #node_cmp_rlayers_register_pass.
 */
void ntreeCompositUpdateRLayers(struct bNodeTree *ntree);

void ntreeCompositClearTags(struct bNodeTree *ntree);

struct bNodeSocket *ntreeCompositOutputFileAddSocket(struct bNodeTree *ntree,
                                                     struct bNode *node,
                                                     const char *name,
                                                     struct ImageFormatData *im_format);

int ntreeCompositOutputFileRemoveActiveSocket(struct bNodeTree *ntree, struct bNode *node);
void ntreeCompositOutputFileSetPath(struct bNode *node,
                                    struct bNodeSocket *sock,
                                    const char *name);
void ntreeCompositOutputFileSetLayer(struct bNode *node,
                                     struct bNodeSocket *sock,
                                     const char *name);
/* needed in do_versions */
void ntreeCompositOutputFileUniquePath(struct ListBase *list,
                                       struct bNodeSocket *sock,
                                       const char defname[],
                                       char delim);
void ntreeCompositOutputFileUniqueLayer(struct ListBase *list,
                                        struct bNodeSocket *sock,
                                        const char defname[],
                                        char delim);

void ntreeCompositColorBalanceSyncFromLGG(bNodeTree *ntree, bNode *node);
void ntreeCompositColorBalanceSyncFromCDL(bNodeTree *ntree, bNode *node);

void ntreeCompositCryptomatteSyncFromAdd(const Scene *scene, bNode *node);
void ntreeCompositCryptomatteSyncFromRemove(bNode *node);
bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node);
int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node);
void ntreeCompositCryptomatteLayerPrefix(const Scene *scene,
                                         const bNode *node,
                                         char *r_prefix,
                                         size_t prefix_len);

/**
 * Update the runtime layer names with the crypto-matte layer names of the references render layer
 * or image.
 */
void ntreeCompositCryptomatteUpdateLayerNames(const Scene *scene, bNode *node);
struct CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node);

#ifdef __cplusplus
}
#endif
