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
 */

/** \file
 * \ingroup blenloader
 */

#include "BLI_utildefines.h"

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_genfile.h"
#include "DNA_key_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_smoke_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BKE_anim.h"
#include "BKE_image.h"
#include "BKE_main.h"  // for Main
#include "BKE_mesh.h"  // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_text.h"  // for txt_extended_ascii_as_utf8
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_writeffmpeg.h"

#include "IMB_imbuf.h"  // for proxy / timecode versioning stuff

#include "NOD_common.h"
#include "NOD_texture.h"

#include "BLO_readfile.h"

#include "readfile.h"

static void do_versions_nodetree_image_default_alpha_output(bNodeTree *ntree)
{
  bNode *node;
  bNodeSocket *sock;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
      /* default Image output value should have 0 alpha */
      sock = node->outputs.first;
      ((bNodeSocketValueRGBA *)(sock->default_value))->value[3] = 0.0f;
    }
  }
}

static void do_versions_nodetree_convert_angle(bNodeTree *ntree)
{
  bNode *node;
  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_ROTATE) {
      /* Convert degrees to radians. */
      bNodeSocket *sock = ((bNodeSocket *)node->inputs.first)->next;
      ((bNodeSocketValueFloat *)sock->default_value)->value = DEG2RADF(
          ((bNodeSocketValueFloat *)sock->default_value)->value);
    }
    else if (node->type == CMP_NODE_DBLUR) {
      /* Convert degrees to radians. */
      NodeDBlurData *ndbd = node->storage;
      ndbd->angle = DEG2RADF(ndbd->angle);
      ndbd->spin = DEG2RADF(ndbd->spin);
    }
    else if (node->type == CMP_NODE_DEFOCUS) {
      /* Convert degrees to radians. */
      NodeDefocus *nqd = node->storage;
      /* XXX DNA char to float conversion seems to map the char value
       * into the [0.0f, 1.0f] range. */
      nqd->rotation = DEG2RADF(nqd->rotation * 255.0f);
    }
    else if (node->type == CMP_NODE_CHROMA_MATTE) {
      /* Convert degrees to radians. */
      NodeChroma *ndc = node->storage;
      ndc->t1 = DEG2RADF(ndc->t1);
      ndc->t2 = DEG2RADF(ndc->t2);
    }
    else if (node->type == CMP_NODE_GLARE) {
      /* Convert degrees to radians. */
      NodeGlare *ndg = node->storage;
      /* XXX DNA char to float conversion seems to map the char value
       * into the [0.0f, 1.0f] range. */
      ndg->angle_ofs = DEG2RADF(ndg->angle_ofs * 255.0f);
    }
    /* XXX TexMapping struct is used by other nodes too (at least node_composite_mapValue),
     *     but not the rot part...
     */
    else if (node->type == SH_NODE_MAPPING) {
      /* Convert degrees to radians. */
      TexMapping *tmap = node->storage;
      tmap->rot[0] = DEG2RADF(tmap->rot[0]);
      tmap->rot[1] = DEG2RADF(tmap->rot[1]);
      tmap->rot[2] = DEG2RADF(tmap->rot[2]);
    }
  }
}

static void do_versions_image_settings_2_60(Scene *sce)
{
  /* note: rd->subimtype is moved into individual settings now and no longer
   * exists */
  RenderData *rd = &sce->r;
  ImageFormatData *imf = &sce->r.im_format;

  /* we know no data loss happens here, the old values were in char range */
  imf->imtype = (char)rd->imtype;
  imf->planes = (char)rd->planes;
  imf->compress = (char)rd->quality;
  imf->quality = (char)rd->quality;

  /* default, was stored in multiple places, may override later */
  imf->depth = R_IMF_CHAN_DEPTH_8;

  /* openexr */
  imf->exr_codec = rd->quality & 7; /* strange but true! 0-4 are valid values, OPENEXR_COMPRESS */

  switch (imf->imtype) {
    case R_IMF_IMTYPE_OPENEXR:
      imf->depth = (rd->subimtype & R_OPENEXR_HALF) ? R_IMF_CHAN_DEPTH_16 : R_IMF_CHAN_DEPTH_32;
      if (rd->subimtype & R_PREVIEW_JPG) {
        imf->flag |= R_IMF_FLAG_PREVIEW_JPG;
      }
      if (rd->subimtype & R_OPENEXR_ZBUF) {
        imf->flag |= R_IMF_FLAG_ZBUF;
      }
      break;
    case R_IMF_IMTYPE_TIFF:
      if (rd->subimtype & R_TIFF_16BIT) {
        imf->depth = R_IMF_CHAN_DEPTH_16;
      }
      break;
    case R_IMF_IMTYPE_JP2:
      if (rd->subimtype & R_JPEG2K_16BIT) {
        imf->depth = R_IMF_CHAN_DEPTH_16;
      }
      else if (rd->subimtype & R_JPEG2K_12BIT) {
        imf->depth = R_IMF_CHAN_DEPTH_12;
      }

      if (rd->subimtype & R_JPEG2K_YCC) {
        imf->jp2_flag |= R_IMF_JP2_FLAG_YCC;
      }
      if (rd->subimtype & R_JPEG2K_CINE_PRESET) {
        imf->jp2_flag |= R_IMF_JP2_FLAG_CINE_PRESET;
      }
      if (rd->subimtype & R_JPEG2K_CINE_48FPS) {
        imf->jp2_flag |= R_IMF_JP2_FLAG_CINE_48;
      }
      break;
    case R_IMF_IMTYPE_CINEON:
    case R_IMF_IMTYPE_DPX:
      if (rd->subimtype & R_CINEON_LOG) {
        imf->cineon_flag |= R_IMF_CINEON_FLAG_LOG;
      }
      break;
  }
}

/* socket use flags were only temporary before */
static void do_versions_nodetree_socket_use_flags_2_62(bNodeTree *ntree)
{
  bNode *node;
  bNodeSocket *sock;
  bNodeLink *link;

  for (node = ntree->nodes.first; node; node = node->next) {
    for (sock = node->inputs.first; sock; sock = sock->next) {
      sock->flag &= ~SOCK_IN_USE;
    }
    for (sock = node->outputs.first; sock; sock = sock->next) {
      sock->flag &= ~SOCK_IN_USE;
    }
  }
  for (sock = ntree->inputs.first; sock; sock = sock->next) {
    sock->flag &= ~SOCK_IN_USE;
  }
  for (sock = ntree->outputs.first; sock; sock = sock->next) {
    sock->flag &= ~SOCK_IN_USE;
  }

  for (link = ntree->links.first; link; link = link->next) {
    link->fromsock->flag |= SOCK_IN_USE;
    link->tosock->flag |= SOCK_IN_USE;
  }
}

static void do_versions_nodetree_multi_file_output_format_2_62_1(Scene *sce, bNodeTree *ntree)
{
  bNode *node;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_OUTPUT_FILE) {
      /* previous CMP_NODE_OUTPUT_FILE nodes get converted to multi-file outputs */
      NodeImageFile *old_data = node->storage;
      NodeImageMultiFile *nimf = MEM_callocN(sizeof(NodeImageMultiFile), "node image multi file");
      bNodeSocket *old_image = BLI_findlink(&node->inputs, 0);
      bNodeSocket *old_z = BLI_findlink(&node->inputs, 1);
      bNodeSocket *sock;
      char filename[FILE_MAXFILE];

      /* ugly, need to remove the old inputs list to avoid bad pointer
       * checks when adding new sockets.
       * sock->storage is expected to contain path info in ntreeCompositOutputFileAddSocket.
       */
      BLI_listbase_clear(&node->inputs);

      node->storage = nimf;

      /* looks like storage data can be messed up somehow, stupid check here */
      if (old_data) {
        char basepath[FILE_MAXDIR];

        /* split off filename from the old path, to be used as socket sub-path */
        BLI_split_dirfile(old_data->name, basepath, filename, sizeof(basepath), sizeof(filename));

        BLI_strncpy(nimf->base_path, basepath, sizeof(nimf->base_path));
        nimf->format = old_data->im_format;
      }
      else {
        BLI_strncpy(filename, old_image->name, sizeof(filename));
      }

      /* if z buffer is saved, change the image type to multilayer exr.
       * XXX this is slightly messy, Z buffer was ignored before for anything but EXR and IRIS ...
       * i'm just assuming here that IRIZ means IRIS with z buffer ...
       */
      if (old_data && ELEM(old_data->im_format.imtype, R_IMF_IMTYPE_IRIZ, R_IMF_IMTYPE_OPENEXR)) {
        char sockpath[FILE_MAX];

        nimf->format.imtype = R_IMF_IMTYPE_MULTILAYER;

        BLI_snprintf(sockpath, sizeof(sockpath), "%s_Image", filename);
        sock = ntreeCompositOutputFileAddSocket(ntree, node, sockpath, &nimf->format);
        /* XXX later do_versions copies path from socket name, need to set this explicitly */
        BLI_strncpy(sock->name, sockpath, sizeof(sock->name));
        if (old_image->link) {
          old_image->link->tosock = sock;
          sock->link = old_image->link;
        }

        BLI_snprintf(sockpath, sizeof(sockpath), "%s_Z", filename);
        sock = ntreeCompositOutputFileAddSocket(ntree, node, sockpath, &nimf->format);
        /* XXX later do_versions copies path from socket name, need to set this explicitly */
        BLI_strncpy(sock->name, sockpath, sizeof(sock->name));
        if (old_z->link) {
          old_z->link->tosock = sock;
          sock->link = old_z->link;
        }
      }
      else {
        sock = ntreeCompositOutputFileAddSocket(ntree, node, filename, &nimf->format);
        /* XXX later do_versions copies path from socket name, need to set this explicitly */
        BLI_strncpy(sock->name, filename, sizeof(sock->name));
        if (old_image->link) {
          old_image->link->tosock = sock;
          sock->link = old_image->link;
        }
      }

      nodeRemoveSocket(ntree, node, old_image);
      nodeRemoveSocket(ntree, node, old_z);
      if (old_data) {
        MEM_freeN(old_data);
      }
    }
    else if (node->type == CMP_NODE_OUTPUT_MULTI_FILE__DEPRECATED) {
      NodeImageMultiFile *nimf = node->storage;
      bNodeSocket *sock;

      /* CMP_NODE_OUTPUT_MULTI_FILE has been redeclared as CMP_NODE_OUTPUT_FILE */
      node->type = CMP_NODE_OUTPUT_FILE;

      /* initialize the node-wide image format from render data, if available */
      if (sce) {
        nimf->format = sce->r.im_format;
      }

      /* transfer render format toggle to node format toggle */
      for (sock = node->inputs.first; sock; sock = sock->next) {
        NodeImageMultiFileSocket *simf = sock->storage;
        simf->use_node_format = simf->use_render_format;
      }

      /* we do have preview now */
      node->flag |= NODE_PREVIEW;
    }
  }
}

/* blue and red are swapped pre 2.62.1, be sane (red == red) now! */
static void do_versions_mesh_mloopcol_swap_2_62_1(Mesh *me)
{
  CustomDataLayer *layer;
  MLoopCol *mloopcol;
  int a;
  int i;

  for (a = 0; a < me->ldata.totlayer; a++) {
    layer = &me->ldata.layers[a];

    if (layer->type == CD_MLOOPCOL) {
      mloopcol = (MLoopCol *)layer->data;
      for (i = 0; i < me->totloop; i++, mloopcol++) {
        SWAP(uchar, mloopcol->r, mloopcol->b);
      }
    }
  }
}

static void do_versions_nodetree_multi_file_output_path_2_63_1(bNodeTree *ntree)
{
  bNode *node;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_OUTPUT_FILE) {
      bNodeSocket *sock;
      for (sock = node->inputs.first; sock; sock = sock->next) {
        NodeImageMultiFileSocket *input = sock->storage;
        /* input file path is stored in dedicated struct now instead socket name */
        BLI_strncpy(input->path, sock->name, sizeof(input->path));
      }
    }
  }
}

static void do_versions_nodetree_file_output_layers_2_64_5(bNodeTree *ntree)
{
  bNode *node;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_OUTPUT_FILE) {
      bNodeSocket *sock;
      for (sock = node->inputs.first; sock; sock = sock->next) {
        NodeImageMultiFileSocket *input = sock->storage;

        /* multilayer names are stored as separate strings now,
         * used the path string before, so copy it over.
         */
        BLI_strncpy(input->layer, input->path, sizeof(input->layer));

        /* paths/layer names also have to be unique now, initial check */
        ntreeCompositOutputFileUniquePath(&node->inputs, sock, input->path, '_');
        ntreeCompositOutputFileUniqueLayer(&node->inputs, sock, input->layer, '_');
      }
    }
  }
}

static void do_versions_nodetree_image_layer_2_64_5(bNodeTree *ntree)
{
  bNode *node;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_IMAGE) {
      bNodeSocket *sock;
      for (sock = node->outputs.first; sock; sock = sock->next) {
        NodeImageLayer *output = MEM_callocN(sizeof(NodeImageLayer), "node image layer");

        /* take pass index both from current storage ptr (actually an int) */
        output->pass_index = POINTER_AS_INT(sock->storage);

        /* replace socket data pointer */
        sock->storage = output;
      }
    }
  }
}

static void do_versions_nodetree_frame_2_64_6(bNodeTree *ntree)
{
  bNode *node;

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == NODE_FRAME) {
      /* initialize frame node storage data */
      if (node->storage == NULL) {
        NodeFrame *data = (NodeFrame *)MEM_callocN(sizeof(NodeFrame), "frame node storage");
        node->storage = data;

        /* copy current flags */
        data->flag = node->custom1;

        data->label_size = 20;
      }
    }

    /* initialize custom node color */
    node->color[0] = node->color[1] = node->color[2] = 0.608f; /* default theme color */
  }
}

static void do_versions_affine_tracker_track(MovieTrackingTrack *track)
{
  int i;

  for (i = 0; i < track->markersnr; i++) {
    MovieTrackingMarker *marker = &track->markers[i];

    if (is_zero_v2(marker->pattern_corners[0]) && is_zero_v2(marker->pattern_corners[1]) &&
        is_zero_v2(marker->pattern_corners[2]) && is_zero_v2(marker->pattern_corners[3])) {
      marker->pattern_corners[0][0] = track->pat_min[0];
      marker->pattern_corners[0][1] = track->pat_min[1];

      marker->pattern_corners[1][0] = track->pat_max[0];
      marker->pattern_corners[1][1] = track->pat_min[1];

      marker->pattern_corners[2][0] = track->pat_max[0];
      marker->pattern_corners[2][1] = track->pat_max[1];

      marker->pattern_corners[3][0] = track->pat_min[0];
      marker->pattern_corners[3][1] = track->pat_max[1];
    }

    if (is_zero_v2(marker->search_min) && is_zero_v2(marker->search_max)) {
      copy_v2_v2(marker->search_min, track->search_min);
      copy_v2_v2(marker->search_max, track->search_max);
    }
  }
}

static const char *node_get_static_idname(int type, int treetype)
{
  /* use static type info header to map static int type to identifier string */
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc) \
  case ID: \
    return #Category #StructName;

  /* XXX hack, group types share a single static integer identifier,
   * but are registered as separate types */
  if (type == NODE_GROUP) {
    switch (treetype) {
      case NTREE_COMPOSIT:
        return "CompositorNodeGroup";
      case NTREE_SHADER:
        return "ShaderNodeGroup";
      case NTREE_TEXTURE:
        return "TextureNodeGroup";
    }
  }
  else {
    switch (type) {
#include "NOD_static_types.h"
    }
  }
  return "";
}

static const char *node_socket_get_static_idname(bNodeSocket *sock)
{
  switch (sock->type) {
    case SOCK_FLOAT: {
      bNodeSocketValueFloat *dval = sock->default_value;
      return nodeStaticSocketType(SOCK_FLOAT, dval->subtype);
    }
    case SOCK_INT: {
      bNodeSocketValueInt *dval = sock->default_value;
      return nodeStaticSocketType(SOCK_INT, dval->subtype);
    }
    case SOCK_BOOLEAN: {
      return nodeStaticSocketType(SOCK_BOOLEAN, PROP_NONE);
    }
    case SOCK_VECTOR: {
      bNodeSocketValueVector *dval = sock->default_value;
      return nodeStaticSocketType(SOCK_VECTOR, dval->subtype);
    }
    case SOCK_RGBA: {
      return nodeStaticSocketType(SOCK_RGBA, PROP_NONE);
    }
    case SOCK_STRING: {
      bNodeSocketValueString *dval = sock->default_value;
      return nodeStaticSocketType(SOCK_STRING, dval->subtype);
    }
    case SOCK_SHADER: {
      return nodeStaticSocketType(SOCK_SHADER, PROP_NONE);
    }
  }
  return "";
}

static void do_versions_nodetree_customnodes(bNodeTree *ntree, int UNUSED(is_group))
{
  /* initialize node tree type idname */
  {
    bNode *node;
    bNodeSocket *sock;

    ntree->typeinfo = NULL;

    /* tree type idname */
    switch (ntree->type) {
      case NTREE_COMPOSIT:
        strcpy(ntree->idname, "CompositorNodeTree");
        break;
      case NTREE_SHADER:
        strcpy(ntree->idname, "ShaderNodeTree");
        break;
      case NTREE_TEXTURE:
        strcpy(ntree->idname, "TextureNodeTree");
        break;
    }

    /* node type idname */
    for (node = ntree->nodes.first; node; node = node->next) {
      BLI_strncpy(
          node->idname, node_get_static_idname(node->type, ntree->type), sizeof(node->idname));

      /* existing old nodes have been initialized already */
      node->flag |= NODE_INIT;

      /* sockets idname */
      for (sock = node->inputs.first; sock; sock = sock->next) {
        BLI_strncpy(sock->idname, node_socket_get_static_idname(sock), sizeof(sock->idname));
      }
      for (sock = node->outputs.first; sock; sock = sock->next) {
        BLI_strncpy(sock->idname, node_socket_get_static_idname(sock), sizeof(sock->idname));
      }
    }
    /* tree sockets idname */
    for (sock = ntree->inputs.first; sock; sock = sock->next) {
      BLI_strncpy(sock->idname, node_socket_get_static_idname(sock), sizeof(sock->idname));
    }
    for (sock = ntree->outputs.first; sock; sock = sock->next) {
      BLI_strncpy(sock->idname, node_socket_get_static_idname(sock), sizeof(sock->idname));
    }
  }

  /* initialize socket in_out values */
  {
    bNode *node;
    bNodeSocket *sock;

    for (node = ntree->nodes.first; node; node = node->next) {
      for (sock = node->inputs.first; sock; sock = sock->next) {
        sock->in_out = SOCK_IN;
      }
      for (sock = node->outputs.first; sock; sock = sock->next) {
        sock->in_out = SOCK_OUT;
      }
    }
    for (sock = ntree->inputs.first; sock; sock = sock->next) {
      sock->in_out = SOCK_IN;
    }
    for (sock = ntree->outputs.first; sock; sock = sock->next) {
      sock->in_out = SOCK_OUT;
    }
  }

  /* initialize socket identifier strings */
  {
    bNode *node;
    bNodeSocket *sock;

    for (node = ntree->nodes.first; node; node = node->next) {
      for (sock = node->inputs.first; sock; sock = sock->next) {
        BLI_strncpy(sock->identifier, sock->name, sizeof(sock->identifier));
        BLI_uniquename(&node->inputs,
                       sock,
                       "socket",
                       '.',
                       offsetof(bNodeSocket, identifier),
                       sizeof(sock->identifier));
      }
      for (sock = node->outputs.first; sock; sock = sock->next) {
        BLI_strncpy(sock->identifier, sock->name, sizeof(sock->identifier));
        BLI_uniquename(&node->outputs,
                       sock,
                       "socket",
                       '.',
                       offsetof(bNodeSocket, identifier),
                       sizeof(sock->identifier));
      }
    }
    for (sock = ntree->inputs.first; sock; sock = sock->next) {
      BLI_strncpy(sock->identifier, sock->name, sizeof(sock->identifier));
      BLI_uniquename(&ntree->inputs,
                     sock,
                     "socket",
                     '.',
                     offsetof(bNodeSocket, identifier),
                     sizeof(sock->identifier));
    }
    for (sock = ntree->outputs.first; sock; sock = sock->next) {
      BLI_strncpy(sock->identifier, sock->name, sizeof(sock->identifier));
      BLI_uniquename(&ntree->outputs,
                     sock,
                     "socket",
                     '.',
                     offsetof(bNodeSocket, identifier),
                     sizeof(sock->identifier));
    }
  }
}

void blo_do_versions_260(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  if (bmain->versionfile < 260) {
    {
      /* set default alpha value of Image outputs in image and render layer nodes to 0 */
      Scene *sce;
      bNodeTree *ntree;

      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        /* there are files with invalid audio_channels value, the real cause
         * is unknown, but we fix it here anyway to avoid crashes */
        if (sce->r.ffcodecdata.audio_channels == 0) {
          sce->r.ffcodecdata.audio_channels = 2;
        }

        if (sce->nodetree) {
          do_versions_nodetree_image_default_alpha_output(sce->nodetree);
        }
      }

      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        do_versions_nodetree_image_default_alpha_output(ntree);
      }
    }

    {
      /* support old particle dupliobject rotation settings */
      ParticleSettings *part;

      for (part = bmain->particles.first; part; part = part->id.next) {
        if (ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
          part->draw |= PART_DRAW_ROTATE_OB;

          if (part->rotmode == 0) {
            part->rotmode = PART_ROT_VEL;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 1)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ob->collision_boundtype = ob->boundtype;
    }

    {
      Camera *cam;
      for (cam = bmain->cameras.first; cam; cam = cam->id.next) {
        if (cam->sensor_x < 0.01f) {
          cam->sensor_x = DEFAULT_SENSOR_WIDTH;
        }

        if (cam->sensor_y < 0.01f) {
          cam->sensor_y = DEFAULT_SENSOR_HEIGHT;
        }
      }
    }
  }

  if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 2)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == SH_NODE_MAPPING) {
            TexMapping *tex_mapping;

            tex_mapping = node->storage;
            tex_mapping->projx = PROJ_X;
            tex_mapping->projy = PROJ_Y;
            tex_mapping->projz = PROJ_Z;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 4)) {
    {
      /* Convert node angles to radians! */
      Scene *sce;
      Material *mat;
      bNodeTree *ntree;

      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        if (sce->nodetree) {
          do_versions_nodetree_convert_angle(sce->nodetree);
        }
      }

      for (mat = bmain->materials.first; mat; mat = mat->id.next) {
        if (mat->nodetree) {
          do_versions_nodetree_convert_angle(mat->nodetree);
        }
      }

      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        do_versions_nodetree_convert_angle(ntree);
      }
    }

    {
      /* Tomato compatibility code. */
      bScreen *sc;
      MovieClip *clip;

      for (sc = bmain->screens.first; sc; sc = sc->id.next) {
        ScrArea *sa;
        for (sa = sc->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl;
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              if (v3d->bundle_size == 0.0f) {
                v3d->bundle_size = 0.2f;
                v3d->flag2 |= V3D_SHOW_RECONSTRUCTION;
              }

              if (v3d->bundle_drawtype == 0) {
                v3d->bundle_drawtype = OB_PLAINAXES;
              }
            }
            else if (sl->spacetype == SPACE_CLIP) {
              SpaceClip *sclip = (SpaceClip *)sl;
              if (sclip->scopes.track_preview_height == 0) {
                sclip->scopes.track_preview_height = 120;
              }
            }
          }
        }
      }

      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        MovieTrackingTrack *track;

        if (clip->aspx < 1.0f) {
          clip->aspx = 1.0f;
          clip->aspy = 1.0f;
        }

        clip->proxy.build_tc_flag = IMB_TC_RECORD_RUN | IMB_TC_FREE_RUN |
                                    IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN;

        if (clip->proxy.build_size_flag == 0) {
          clip->proxy.build_size_flag = IMB_PROXY_25;
        }

        if (clip->proxy.quality == 0) {
          clip->proxy.quality = 90;
        }

        if (clip->tracking.camera.pixel_aspect < 0.01f) {
          clip->tracking.camera.pixel_aspect = 1.0f;
        }

        track = clip->tracking.tracks.first;
        while (track) {
          if (track->minimum_correlation == 0.0f) {
            track->minimum_correlation = 0.75f;
          }

          track = track->next;
        }
      }
    }
  }

  if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 6)) {
    Scene *sce;
    MovieClip *clip;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      do_versions_image_settings_2_60(sce);
    }

    for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
      MovieTrackingSettings *settings = &clip->tracking.settings;

      if (settings->default_pattern_size == 0.0f) {
        settings->default_motion_model = TRACK_MOTION_MODEL_TRANSLATION;
        settings->default_minimum_correlation = 0.75;
        settings->default_pattern_size = 11;
        settings->default_search_size = 51;
      }
    }

    {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        /* convert delta addition into delta scale */
        int i;
        for (i = 0; i < 3; i++) {
          if ((ob->dsize[i] == 0.0f) || /* simple case, user never touched dsize */
              (ob->scale[i] == 0.0f))   /* cant scale the dsize to give a non zero result,
                                         * so fallback to 1.0f */
          {
            ob->dscale[i] = 1.0f;
          }
          else {
            ob->dscale[i] = (ob->scale[i] + ob->dsize[i]) / ob->scale[i];
          }
        }
      }
    }
  }
  /* sigh, this dscale vs dsize version patching was not done right, fix for fix,
   * this intentionally checks an exact subversion, also note this was never in a release,
   * at some point this could be removed. */
  else if (bmain->versionfile == 260 && bmain->subversionfile == 6) {
    Object *ob;
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (is_zero_v3(ob->dscale)) {
        copy_vn_fl(ob->dscale, 3, 1.0f);
      }
    }
  }

  if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 8)) {
    Brush *brush;

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
        brush->alpha = 1.0f;
      }
    }
  }

  if (bmain->versionfile < 261 || (bmain->versionfile == 261 && bmain->subversionfile < 1)) {
    {
      /* update use flags for node sockets (was only temporary before) */
      Scene *sce;
      Material *mat;
      Tex *tex;
      World *world;
      bNodeTree *ntree;

      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        if (sce->nodetree) {
          do_versions_nodetree_socket_use_flags_2_62(sce->nodetree);
        }
      }

      for (mat = bmain->materials.first; mat; mat = mat->id.next) {
        if (mat->nodetree) {
          do_versions_nodetree_socket_use_flags_2_62(mat->nodetree);
        }
      }

      for (tex = bmain->textures.first; tex; tex = tex->id.next) {
        if (tex->nodetree) {
          do_versions_nodetree_socket_use_flags_2_62(tex->nodetree);
        }
      }

      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        if (la->nodetree) {
          do_versions_nodetree_socket_use_flags_2_62(la->nodetree);
        }
      }

      for (world = bmain->worlds.first; world; world = world->id.next) {
        if (world->nodetree) {
          do_versions_nodetree_socket_use_flags_2_62(world->nodetree);
        }
      }

      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        do_versions_nodetree_socket_use_flags_2_62(ntree);
      }
    }
    {
      MovieClip *clip;
      Object *ob;

      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        MovieTracking *tracking = &clip->tracking;
        MovieTrackingObject *tracking_object = tracking->objects.first;

        clip->proxy.build_tc_flag |= IMB_TC_RECORD_RUN_NO_GAPS;

        if (!tracking->settings.object_distance) {
          tracking->settings.object_distance = 1.0f;
        }

        if (BLI_listbase_is_empty(&tracking->objects)) {
          BKE_tracking_object_add(tracking, "Camera");
        }

        while (tracking_object) {
          if (!tracking_object->scale) {
            tracking_object->scale = 1.0f;
          }

          tracking_object = tracking_object->next;
        }
      }

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        bConstraint *con;
        for (con = ob->constraints.first; con; con = con->next) {
          if (con->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
            bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;

            if (data->invmat[3][3] == 0.0f) {
              unit_m4(data->invmat);
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 261 || (bmain->versionfile == 261 && bmain->subversionfile < 2)) {
    {
      /* convert deprecated sculpt_paint_unified_* fields to
       * UnifiedPaintSettings */
      Scene *scene;
      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        ToolSettings *ts = scene->toolsettings;
        UnifiedPaintSettings *ups = &ts->unified_paint_settings;
        ups->size = ts->sculpt_paint_unified_size;
        ups->unprojected_radius = ts->sculpt_paint_unified_unprojected_radius;
        ups->alpha = ts->sculpt_paint_unified_alpha;
        ups->flag = ts->sculpt_paint_settings;
      }
    }
  }

  if (bmain->versionfile < 261 || (bmain->versionfile == 261 && bmain->subversionfile < 3)) {
    {
      /* convert extended ascii to utf-8 for text editor */
      Text *text;
      for (text = bmain->texts.first; text; text = text->id.next) {
        if (!(text->flags & TXT_ISEXT)) {
          TextLine *tl;

          for (tl = text->lines.first; tl; tl = tl->next) {
            int added = txt_extended_ascii_as_utf8(&tl->line);
            tl->len += added;

            /* reset cursor position if line was changed */
            if (added && tl == text->curl) {
              text->curc = 0;
            }
          }
        }
      }
    }
    {
      /* set new dynamic paint values */
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_DynamicPaint) {
            DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
            if (pmd->canvas) {
              DynamicPaintSurface *surface = pmd->canvas->surfaces.first;
              for (; surface; surface = surface->next) {
                surface->color_dry_threshold = 1.0f;
                surface->influence_scale = 1.0f;
                surface->radius_scale = 1.0f;
                surface->flags |= MOD_DPAINT_USE_DRYING;
              }
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 262) {
    Object *ob;
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;
          if (clmd->sim_parms) {
            clmd->sim_parms->vel_damping = 1.0f;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 263) {
    /* set fluidsim rate. the version patch for this in 2.62 was wrong, so
     * try to correct it, if rate is 0.0 that's likely not intentional */
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Fluidsim) {
          FluidsimModifierData *fmd = (FluidsimModifierData *)md;
          if (fmd->fss->animRate == 0.0f) {
            fmd->fss->animRate = 1.0f;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 262 || (bmain->versionfile == 262 && bmain->subversionfile < 1)) {
    /* update use flags for node sockets (was only temporary before) */
    Scene *sce;
    bNodeTree *ntree;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->nodetree) {
        do_versions_nodetree_multi_file_output_format_2_62_1(sce, sce->nodetree);
      }
    }

    /* XXX can't associate with scene for group nodes, image format will stay uninitialized */
    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      do_versions_nodetree_multi_file_output_format_2_62_1(NULL, ntree);
    }
  }

  /* only swap for pre-release bmesh merge which had MLoopCol red/blue swap */
  if (bmain->versionfile == 262 && bmain->subversionfile == 1) {
    {
      Mesh *me;
      for (me = bmain->meshes.first; me; me = me->id.next) {
        do_versions_mesh_mloopcol_swap_2_62_1(me);
      }
    }
  }

  if (bmain->versionfile < 262 || (bmain->versionfile == 262 && bmain->subversionfile < 2)) {
    /* Set new idname of keyingsets from their now "label-only" name. */
    Scene *scene;
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      KeyingSet *ks;
      for (ks = scene->keyingsets.first; ks; ks = ks->next) {
        if (!ks->idname[0]) {
          BLI_strncpy(ks->idname, ks->name, sizeof(ks->idname));
        }
      }
    }
  }

  if (bmain->versionfile < 262 || (bmain->versionfile == 262 && bmain->subversionfile < 3)) {
    Object *ob;
    ModifierData *md;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Lattice) {
          LatticeModifierData *lmd = (LatticeModifierData *)md;
          lmd->strength = 1.0f;
        }
      }
    }
  }

  if (bmain->versionfile < 262 || (bmain->versionfile == 262 && bmain->subversionfile < 4)) {
    /* Read Viscosity presets from older files */
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Fluidsim) {
          FluidsimModifierData *fmd = (FluidsimModifierData *)md;
          if (fmd->fss->viscosityMode == 3) {
            fmd->fss->viscosityValue = 5.0;
            fmd->fss->viscosityExponent = 5;
          }
          else if (fmd->fss->viscosityMode == 4) {
            fmd->fss->viscosityValue = 2.0;
            fmd->fss->viscosityExponent = 3;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 263) {
    /* Default for old files is to save particle rotations to pointcache */
    ParticleSettings *part;
    for (part = bmain->particles.first; part; part = part->id.next) {
      part->flag |= PART_ROTATIONS;
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 1)) {
    /* file output node paths are now stored in the file info struct instead socket name */
    Scene *sce;
    bNodeTree *ntree;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->nodetree) {
        do_versions_nodetree_multi_file_output_path_2_63_1(sce->nodetree);
      }
    }
    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      do_versions_nodetree_multi_file_output_path_2_63_1(ntree);
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 3)) {
    Scene *scene;
    Brush *brush;

    /* For weight paint, each brush now gets its own weight;
     * unified paint settings also have weight. Update unified
     * paint settings and brushes with a default weight value. */

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ToolSettings *ts = scene->toolsettings;
      if (ts) {
        ts->unified_paint_settings.weight = ts->vgroup_weight;
        ts->unified_paint_settings.flag |= UNIFIED_PAINT_WEIGHT;
      }
    }

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      brush->weight = 0.5;
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 2)) {
    bScreen *sc;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_CLIP) {
            SpaceClip *sclip = (SpaceClip *)sl;
            ARegion *ar;
            bool hide = false;

            for (ar = sa->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_PREVIEW) {
                if (ar->alignment != RGN_ALIGN_NONE) {
                  ar->flag |= RGN_FLAG_HIDDEN;
                  ar->v2d.flag &= ~V2D_IS_INITIALISED;
                  ar->alignment = RGN_ALIGN_NONE;

                  hide = true;
                }
              }
            }

            if (hide) {
              sclip->view = SC_VIEW_CLIP;
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 4)) {
    Camera *cam;
    Curve *cu;

    for (cam = bmain->cameras.first; cam; cam = cam->id.next) {
      if (cam->flag & CAM_PANORAMA) {
        cam->type = CAM_PANO;
        cam->flag &= ~CAM_PANORAMA;
      }
    }

    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      if (cu->bevfac2 == 0.0f) {
        cu->bevfac1 = 0.0f;
        cu->bevfac2 = 1.0f;
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 5)) {
    {
      /* file output node paths are now stored in the file info struct instead socket name */
      Scene *sce;
      bNodeTree *ntree;

      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        if (sce->nodetree) {
          do_versions_nodetree_file_output_layers_2_64_5(sce->nodetree);
          do_versions_nodetree_image_layer_2_64_5(sce->nodetree);
        }
      }
      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        do_versions_nodetree_file_output_layers_2_64_5(ntree);
        do_versions_nodetree_image_layer_2_64_5(ntree);
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 6)) {
    /* update use flags for node sockets (was only temporary before) */
    Scene *sce;
    Material *mat;
    Tex *tex;
    World *world;
    bNodeTree *ntree;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->nodetree) {
        do_versions_nodetree_frame_2_64_6(sce->nodetree);
      }
    }

    for (mat = bmain->materials.first; mat; mat = mat->id.next) {
      if (mat->nodetree) {
        do_versions_nodetree_frame_2_64_6(mat->nodetree);
      }
    }

    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      if (tex->nodetree) {
        do_versions_nodetree_frame_2_64_6(tex->nodetree);
      }
    }

    for (Light *la = bmain->lights.first; la; la = la->id.next) {
      if (la->nodetree) {
        do_versions_nodetree_frame_2_64_6(la->nodetree);
      }
    }

    for (world = bmain->worlds.first; world; world = world->id.next) {
      if (world->nodetree) {
        do_versions_nodetree_frame_2_64_6(world->nodetree);
      }
    }

    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      do_versions_nodetree_frame_2_64_6(ntree);
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 7)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Smoke) {
          SmokeModifierData *smd = (SmokeModifierData *)md;
          if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
            int maxres = max_iii(smd->domain->res[0], smd->domain->res[1], smd->domain->res[2]);
            smd->domain->scale = smd->domain->dx * maxres;
            smd->domain->dx = 1.0f / smd->domain->scale;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 9)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT)) {
            NodeTexImage *tex = node->storage;

            tex->iuser.frames = 1;
            tex->iuser.sfra = 1;
            tex->iuser.ok = 1;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 10)) {
    {
      Scene *scene;
      // composite redesign
      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        if (scene->nodetree) {
          if (scene->nodetree->chunksize == 0) {
            scene->nodetree->chunksize = 256;
          }
        }
      }

      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          bNode *node;
          for (node = ntree->nodes.first; node; node = node->next) {
            if (node->type == CMP_NODE_DEFOCUS) {
              NodeDefocus *data = node->storage;
              if (data->maxblur == 0.0f) {
                data->maxblur = 16.0f;
              }
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    {
      bScreen *sc;

      for (sc = bmain->screens.first; sc; sc = sc->id.next) {
        ScrArea *sa;

        for (sa = sc->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl;

          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_CLIP) {
              SpaceClip *sclip = (SpaceClip *)sl;

              if (sclip->around == 0) {
                sclip->around = V3D_AROUND_CENTER_MEDIAN;
              }
            }
          }
        }
      }
    }

    {
      MovieClip *clip;

      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        clip->start_frame = 1;
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 11)) {
    MovieClip *clip;

    for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
      MovieTrackingTrack *track;

      track = clip->tracking.tracks.first;
      while (track) {
        do_versions_affine_tracker_track(track);

        track = track->next;
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 13)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_DILATEERODE) {
            if (node->storage == NULL) {
              NodeDilateErode *data = MEM_callocN(sizeof(NodeDilateErode), __func__);
              data->falloff = PROP_SMOOTH;
              node->storage = data;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 14)) {
    ParticleSettings *part;

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_KEYING) {
            NodeKeyingData *data = node->storage;

            if (data->despill_balance == 0.0f) {
              data->despill_balance = 0.5f;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    /* keep compatibility for dupliobject particle size */
    for (part = bmain->particles.first; part; part = part->id.next) {
      if (ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
        if ((part->draw & PART_DRAW_ROTATE_OB) == 0) {
          part->draw |= PART_DRAW_NO_SCALE_OB;
        }
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 17)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_MASK) {
            if (node->storage == NULL) {
              NodeMask *data = MEM_callocN(sizeof(NodeMask), __func__);
              /* move settings into own struct */
              data->size_x = (int)node->custom3;
              data->size_y = (int)node->custom4;
              node->custom3 = 0.5f; /* default shutter */
              node->storage = data;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 18)) {
    Scene *scene;

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->ed) {
        Sequence *seq;

        SEQ_BEGIN (scene->ed, seq) {
          Strip *strip = seq->strip;

          if (strip && strip->color_balance) {
            SequenceModifierData *smd;
            ColorBalanceModifierData *cbmd;

            smd = BKE_sequence_modifier_new(seq, NULL, seqModifierType_ColorBalance);
            cbmd = (ColorBalanceModifierData *)smd;

            cbmd->color_balance = *strip->color_balance;

            /* multiplication with color balance used is handled differently,
             * so we need to move multiplication to modifier so files would be
             * compatible
             */
            cbmd->color_multiply = seq->mul;
            seq->mul = 1.0f;

            MEM_freeN(strip->color_balance);
            strip->color_balance = NULL;
          }
        }
        SEQ_END;
      }
    }
  }

  /* color management pipeline changes compatibility code */
  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 19)) {
    Scene *scene;
    Image *ima;
    bool colormanagement_disabled = false;

    /* make scenes which are not using color management have got None as display device,
     * so they wouldn't perform linear-to-sRGB conversion on display
     */
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if ((scene->r.color_mgt_flag & R_COLOR_MANAGEMENT) == 0) {
        ColorManagedDisplaySettings *display_settings = &scene->display_settings;

        if (display_settings->display_device[0] == 0) {
          BKE_scene_disable_color_management(scene);
        }

        colormanagement_disabled = true;
      }
    }

    for (ima = bmain->images.first; ima; ima = ima->id.next) {
      if (ima->source == IMA_SRC_VIEWER) {
        ima->flag |= IMA_VIEW_AS_RENDER;
      }
      else if (colormanagement_disabled) {
        /* if color-management not used, set image's color space to raw, so no sRGB->linear
         * conversion would happen on display and render there's no clear way to check whether
         * color management is enabled or not in render engine so set all images to raw if there's
         * at least one scene with color management disabled this would still behave incorrect in
         * cases when color management was used for only some of scenes, but such a setup is
         * crazy anyway and think it's fair enough to break compatibility in that cases.
         */

        BLI_strncpy(ima->colorspace_settings.name, "Raw", sizeof(ima->colorspace_settings.name));
      }
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 20)) {
    Key *key;
    for (key = bmain->shapekeys.first; key; key = key->id.next) {
      blo_do_versions_key_uidgen(key);
    }
  }

  if (bmain->versionfile < 263 || (bmain->versionfile == 263 && bmain->subversionfile < 21)) {
    {
      Mesh *me;
      for (me = bmain->meshes.first; me; me = me->id.next) {
        CustomData_update_typemap(&me->vdata);
        CustomData_free_layers(&me->vdata, CD_MSTICKY, me->totvert);
      }
    }
  }

  /* correction for files saved in blender version when BKE_pose_copy_data
   * didn't copy animation visualization, which lead to deadlocks on motion
   * path calculation for proxied armatures, see [#32742]
   */
  if (bmain->versionfile < 264) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->pose) {
        if (ob->pose->avs.path_step == 0) {
          animviz_settings_init(&ob->pose->avs);
        }
      }
    }
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 1)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == SH_NODE_TEX_COORD) {
            node->flag |= NODE_OPTIONS;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 2)) {
    MovieClip *clip;

    for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object;

      for (tracking_object = tracking->objects.first; tracking_object;
           tracking_object = tracking_object->next) {
        if (tracking_object->keyframe1 == 0 && tracking_object->keyframe2 == 0) {
          tracking_object->keyframe1 = tracking->settings.keyframe1;
          tracking_object->keyframe2 = tracking->settings.keyframe2;
        }
      }
    }
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 3)) {
    /* smoke branch */
    {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Smoke) {
            SmokeModifierData *smd = (SmokeModifierData *)md;
            if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
              /* keep branch saves if possible */
              if (!smd->domain->flame_max_temp) {
                smd->domain->burning_rate = 0.75f;
                smd->domain->flame_smoke = 1.0f;
                smd->domain->flame_vorticity = 0.5f;
                smd->domain->flame_ignition = 1.25f;
                smd->domain->flame_max_temp = 1.75f;
                smd->domain->adapt_threshold = 0.02f;
                smd->domain->adapt_margin = 4;
                smd->domain->flame_smoke_color[0] = 0.7f;
                smd->domain->flame_smoke_color[1] = 0.7f;
                smd->domain->flame_smoke_color[2] = 0.7f;
              }
            }
            else if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
              if (!smd->flow->texture_size) {
                smd->flow->fuel_amount = 1.0;
                smd->flow->surface_distance = 1.5;
                smd->flow->color[0] = 0.7f;
                smd->flow->color[1] = 0.7f;
                smd->flow->color[2] = 0.7f;
                smd->flow->texture_size = 1.0f;
              }
            }
          }
        }
      }
    }

    /* render border for viewport */
    {
      bScreen *sc;

      for (sc = bmain->screens.first; sc; sc = sc->id.next) {
        ScrArea *sa;
        for (sa = sc->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl;
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              if (v3d->render_border.xmin == 0.0f && v3d->render_border.ymin == 0.0f &&
                  v3d->render_border.xmax == 0.0f && v3d->render_border.ymax == 0.0f) {
                v3d->render_border.xmax = 1.0f;
                v3d->render_border.ymax = 1.0f;
              }
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 5)) {
    /* set a unwrapping margin and ABF by default */
    Scene *scene;

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->toolsettings->uvcalc_margin == 0.0f) {
        scene->toolsettings->uvcalc_margin = 0.001f;
        scene->toolsettings->unwrapper = 0;
      }
    }
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 6)) {
    /* Fix for bug #32982, internal_links list could get corrupted from r51630 onward.
     * Simply remove bad internal_links lists to avoid NULL pointers.
     */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      bNode *node;
      bNodeLink *link, *nextlink;

      for (node = ntree->nodes.first; node; node = node->next) {
        for (link = node->internal_links.first; link; link = nextlink) {
          nextlink = link->next;
          if (!link->fromnode || !link->fromsock || !link->tonode || !link->tosock) {
            BLI_remlink(&node->internal_links, link);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 7)) {
    /* convert tiles size from resolution and number of tiles */
    {
      Scene *scene;

      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        if (scene->r.tilex == 0 || scene->r.tiley == 1) {
          scene->r.tilex = scene->r.tiley = 64;
        }
      }
    }

    /* collision masks */
    {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->col_group == 0) {
          ob->col_group = 0x01;
          ob->col_mask = 0xff;
        }
      }
    }
  }

  if (bmain->versionfile < 264 || (bmain->versionfile == 264 && bmain->subversionfile < 7)) {
    MovieClip *clip;

    for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
      MovieTrackingTrack *track;
      MovieTrackingObject *object;

      for (track = clip->tracking.tracks.first; track; track = track->next) {
        do_versions_affine_tracker_track(track);
      }

      for (object = clip->tracking.objects.first; object; object = object->next) {
        for (track = object->tracks.first; track; track = track->next) {
          do_versions_affine_tracker_track(track);
        }
      }
    }
  }

  if (bmain->versionfile < 265 || (bmain->versionfile == 265 && bmain->subversionfile < 3)) {
    bScreen *sc;
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          switch (sl->spacetype) {
            case SPACE_VIEW3D: {
              View3D *v3d = (View3D *)sl;
              v3d->flag2 |= V3D_SHOW_ANNOTATION;
              break;
            }
            case SPACE_SEQ: {
              SpaceSeq *sseq = (SpaceSeq *)sl;
              sseq->flag |= SEQ_SHOW_GPENCIL;
              break;
            }
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->flag |= SI_SHOW_GPENCIL;
              break;
            }
            case SPACE_NODE: {
              SpaceNode *snode = (SpaceNode *)sl;
              snode->flag |= SNODE_SHOW_GPENCIL;
              break;
            }
            case SPACE_CLIP: {
              SpaceClip *sclip = (SpaceClip *)sl;
              sclip->flag |= SC_SHOW_ANNOTATION;
              break;
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 265 || (bmain->versionfile == 265 && bmain->subversionfile < 5)) {
    Scene *scene;
    Tex *tex;

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      Sequence *seq;

      SEQ_BEGIN (scene->ed, seq) {
        enum { SEQ_MAKE_PREMUL = (1 << 6) };
        if (seq->flag & SEQ_MAKE_PREMUL) {
          seq->alpha_mode = SEQ_ALPHA_STRAIGHT;
        }
        else {
          BKE_sequence_alpha_mode_from_extension(seq);
        }
      }
      SEQ_END;

      if (scene->r.bake_samples == 0) {
        scene->r.bake_samples = 256;
      }
    }

    for (Image *image = bmain->images.first; image; image = image->id.next) {
      if (image->flag & IMA_DO_PREMUL) {
        image->alpha_mode = IMA_ALPHA_STRAIGHT;
      }
      else {
        BKE_image_alpha_mode_from_extension(image);
      }
    }

    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      if (tex->type == TEX_IMAGE && (tex->imaflag & TEX_USEALPHA) == 0) {
        Image *image = blo_do_versions_newlibadr(fd, tex->id.lib, tex->ima);

        if (image && (image->flag & IMA_DO_PREMUL) == 0) {
          image->flag |= IMA_IGNORE_ALPHA;
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_IMAGE) {
            Image *image = blo_do_versions_newlibadr(fd, ntree->id.lib, node->id);

            if (image) {
              if ((image->flag & IMA_DO_PREMUL) == 0 && image->alpha_mode == IMA_ALPHA_STRAIGHT) {
                node->custom1 |= CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT;
              }
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }
  else if (bmain->versionfile < 266 || (bmain->versionfile == 266 && bmain->subversionfile < 1)) {
    /* texture use alpha was removed for 2.66 but added back again for 2.66a,
     * for compatibility all textures assumed it to be enabled */
    Tex *tex;

    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      if (tex->type == TEX_IMAGE) {
        tex->imaflag |= TEX_USEALPHA;
      }
    }
  }

  if (bmain->versionfile < 265 || (bmain->versionfile == 265 && bmain->subversionfile < 7)) {
    Curve *cu;

    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      if (cu->flag & (CU_FRONT | CU_BACK)) {
        if (cu->ext1 != 0.0f || cu->ext2 != 0.0f) {
          Nurb *nu;

          for (nu = cu->nurb.first; nu; nu = nu->next) {
            int a;

            if (nu->bezt) {
              BezTriple *bezt = nu->bezt;
              a = nu->pntsu;

              while (a--) {
                bezt->radius = 1.0f;
                bezt++;
              }
            }
            else if (nu->bp) {
              BPoint *bp = nu->bp;
              a = nu->pntsu * nu->pntsv;

              while (a--) {
                bp->radius = 1.0f;
                bp++;
              }
            }
          }
        }
      }
    }
  }

  if (MAIN_VERSION_OLDER(bmain, 265, 9)) {
    Mesh *me;
    for (me = bmain->meshes.first; me; me = me->id.next) {
      BKE_mesh_do_versions_cd_flag_init(me);
    }
  }

  if (MAIN_VERSION_OLDER(bmain, 265, 10)) {
    Brush *br;
    for (br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode & OB_MODE_TEXTURE_PAINT) {
        br->mtex.brush_map_mode = MTEX_MAP_MODE_TILED;
      }
    }
  }

  // add storage for compositor translate nodes when not existing
  if (MAIN_VERSION_OLDER(bmain, 265, 11)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_TRANSLATE && node->storage == NULL) {
            node->storage = MEM_callocN(sizeof(NodeTranslateData), "node translate data");
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (MAIN_VERSION_OLDER(bmain, 266, 2)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      do_versions_nodetree_customnodes(ntree, ((ID *)ntree == id));
    }
    FOREACH_NODETREE_END;
  }

  if (MAIN_VERSION_OLDER(bmain, 266, 2)) {
    bScreen *sc;
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)sl;

            /* reset pointers to force tree path update from context */
            snode->nodetree = NULL;
            snode->edittree = NULL;
            snode->id = NULL;
            snode->from = NULL;

            /* convert deprecated treetype setting to tree_idname */
            switch (snode->treetype) {
              case NTREE_COMPOSIT:
                strcpy(snode->tree_idname, "CompositorNodeTree");
                break;
              case NTREE_SHADER:
                strcpy(snode->tree_idname, "ShaderNodeTree");
                break;
              case NTREE_TEXTURE:
                strcpy(snode->tree_idname, "TextureNodeTree");
                break;
            }
          }
        }
      }
    }
  }

  if (MAIN_VERSION_OLDER(bmain, 266, 3)) {
    {
      /* Fix for a very old issue:
       * Node names were nominally made unique in r24478 (2.50.8), but the do_versions check
       * to update existing node names only applied to bmain->nodetree (i.e. group nodes).
       * Uniqueness is now required for proper preview mapping,
       * so do this now to ensure old files don't break.
       */
      bNode *node;
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (id == &ntree->id) {
          continue; /* already fixed for node groups */
        }

        for (node = ntree->nodes.first; node; node = node->next) {
          nodeUniqueName(ntree, node);
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 266, 4)) {
    Brush *brush;
    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      BKE_texture_mtex_default(&brush->mask_mtex);

      if (brush->ob_mode & OB_MODE_TEXTURE_PAINT) {
        brush->spacing /= 2;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 266, 6)) {
    Brush *brush;
#define BRUSH_TEXTURE_OVERLAY (1 << 21)

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      brush->overlay_flags = 0;
      if (brush->flag & BRUSH_TEXTURE_OVERLAY) {
        brush->overlay_flags |= (BRUSH_OVERLAY_PRIMARY | BRUSH_OVERLAY_CURSOR);
      }
    }
#undef BRUSH_TEXTURE_OVERLAY
  }

  if (bmain->versionfile < 267) {
    // if (!DNA_struct_elem_find(fd->filesdna, "Brush", "int", "stencil_pos")) {
    Brush *brush;

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->stencil_dimension[0] == 0) {
        brush->stencil_dimension[0] = 256;
        brush->stencil_dimension[1] = 256;
        brush->stencil_pos[0] = 256;
        brush->stencil_pos[1] = 256;
      }
      if (brush->mask_stencil_dimension[0] == 0) {
        brush->mask_stencil_dimension[0] = 256;
        brush->mask_stencil_dimension[1] = 256;
        brush->mask_stencil_pos[0] = 256;
        brush->mask_stencil_pos[1] = 256;
      }
    }

    /* TIP: to initialize new variables added, use the new function
     * DNA_struct_elem_find(fd->filesdna, "structname", "typename", "varname")
     * example:
     * if (!DNA_struct_elem_find(fd->filesdna, "UserDef", "short", "image_gpubuffer_limit"))
     *     user->image_gpubuffer_limit = 10;
     */
  }

  /* default values in Freestyle settings */
  if (bmain->versionfile < 267) {
    Scene *sce;
    SceneRenderLayer *srl;
    FreestyleLineStyle *linestyle;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->r.line_thickness_mode == 0) {
        sce->r.line_thickness_mode = R_LINE_THICKNESS_ABSOLUTE;
        sce->r.unit_line_thickness = 1.0f;
      }
      for (srl = sce->r.layers.first; srl; srl = srl->next) {
        if (srl->freestyleConfig.mode == 0) {
          srl->freestyleConfig.mode = FREESTYLE_CONTROL_EDITOR_MODE;
        }
        if (srl->freestyleConfig.raycasting_algorithm ==
                FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE ||
            srl->freestyleConfig.raycasting_algorithm ==
                FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL) {
          srl->freestyleConfig.raycasting_algorithm = 0; /* deprecated */
          srl->freestyleConfig.flags |= FREESTYLE_CULLING;
        }
      }

      /* not freestyle */
      {
        MeshStatVis *statvis = &sce->toolsettings->statvis;
        if (statvis->thickness_samples == 0) {
          statvis->overhang_axis = OB_NEGZ;
          statvis->overhang_min = 0;
          statvis->overhang_max = DEG2RADF(45.0f);

          statvis->thickness_max = 0.1f;
          statvis->thickness_samples = 1;

          statvis->distort_min = DEG2RADF(5.0f);
          statvis->distort_max = DEG2RADF(45.0f);

          statvis->sharp_min = DEG2RADF(90.0f);
          statvis->sharp_max = DEG2RADF(180.0f);
        }
      }
    }
    for (linestyle = bmain->linestyles.first; linestyle; linestyle = linestyle->id.next) {
#if 1
      /* disable the Misc panel for now */
      if (linestyle->panel == LS_PANEL_MISC) {
        linestyle->panel = LS_PANEL_STROKES;
      }
#endif
      if (linestyle->thickness_position == 0) {
        linestyle->thickness_position = LS_THICKNESS_CENTER;
        linestyle->thickness_ratio = 0.5f;
      }
      if (linestyle->chaining == 0) {
        linestyle->chaining = LS_CHAINING_PLAIN;
      }
      if (linestyle->rounds == 0) {
        linestyle->rounds = 3;
      }
    }
  }

  if (bmain->versionfile < 267) {
    /* Initialize the active_viewer_key for compositing */
    bScreen *screen;
    Scene *scene;
    bNodeInstanceKey active_viewer_key = {0};
    /* simply pick the first node space and use that for the active viewer key */
    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      ScrArea *sa;
      for (sa = screen->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)sl;
            bNodeTreePath *path = snode->treepath.last;
            if (!path) {
              continue;
            }

            active_viewer_key = path->parent_key;
            break;
          }
        }
        if (active_viewer_key.value != 0) {
          break;
        }
      }
      if (active_viewer_key.value != 0) {
        break;
      }
    }

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      /* NB: scene->nodetree is a local ID block, has been direct_link'ed */
      if (scene->nodetree) {
        scene->nodetree->active_viewer_key = active_viewer_key;
      }
    }
  }

  if (MAIN_VERSION_OLDER(bmain, 267, 1)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Smoke) {
          SmokeModifierData *smd = (SmokeModifierData *)md;
          if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
            if (smd->domain->flags & MOD_SMOKE_HIGH_SMOOTH) {
              smd->domain->highres_sampling = SM_HRES_LINEAR;
            }
            else {
              smd->domain->highres_sampling = SM_HRES_NEAREST;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 268, 1)) {
    Brush *brush;
    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      brush->spacing = MAX2(1, brush->spacing);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 268, 2)) {
    Brush *brush;
#define BRUSH_FIXED (1 << 6)
    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      brush->flag &= ~BRUSH_FIXED;

      if (brush->cursor_overlay_alpha < 2) {
        brush->cursor_overlay_alpha = 33;
      }
      if (brush->texture_overlay_alpha < 2) {
        brush->texture_overlay_alpha = 33;
      }
      if (brush->mask_overlay_alpha < 2) {
        brush->mask_overlay_alpha = 33;
      }
    }
#undef BRUSH_FIXED
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 268, 4)) {
    bScreen *sc;
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      bConstraint *con;
      for (con = ob->constraints.first; con; con = con->next) {
        if (con->type == CONSTRAINT_TYPE_SHRINKWRAP) {
          bShrinkwrapConstraint *data = (bShrinkwrapConstraint *)con->data;
          if (data->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) {
            data->projAxis = OB_POSX;
          }
          else if (data->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) {
            data->projAxis = OB_POSY;
          }
          else {
            data->projAxis = OB_POSZ;
          }
          data->projAxisSpace = CONSTRAINT_SPACE_LOCAL;
        }
      }
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Smoke) {
          SmokeModifierData *smd = (SmokeModifierData *)md;
          if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
            if (!smd->flow->particle_size) {
              smd->flow->particle_size = 1.0f;
            }
          }
        }
      }
    }

    /*
     * FIX some files have a zoom level of 0, and was checked during the drawing of the node space
     *
     * We moved this check to the do versions to be sure the value makes any sense.
     */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)sl;
            if (snode->zoom < 0.02f) {
              snode->zoom = 1.0;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 268, 5)) {
    bScreen *sc;
    ScrArea *sa;

    /* add missing (+) expander in node editor */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        ARegion *ar, *arnew;

        if (sa->spacetype == SPACE_NODE) {
          ar = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS);

          if (ar) {
            continue;
          }

          /* add subdiv level; after header */
          ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

          /* is error! */
          if (ar == NULL) {
            continue;
          }

          arnew = MEM_callocN(sizeof(ARegion), "node tools");

          BLI_insertlinkafter(&sa->regionbase, ar, arnew);
          arnew->regiontype = RGN_TYPE_TOOLS;
          arnew->alignment = RGN_ALIGN_LEFT;

          arnew->flag = RGN_FLAG_HIDDEN;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 1)) {
    /* Removal of Cycles SSS Compatible falloff */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == SH_NODE_SUBSURFACE_SCATTERING) {
            if (node->custom1 == SHD_SUBSURFACE_COMPATIBLE) {
              node->custom1 = SHD_SUBSURFACE_CUBIC;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 2)) {
    /* Initialize CDL settings for Color Balance nodes */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_COLORBALANCE) {
            NodeColorBalance *n = node->storage;
            if (node->custom1 == 0) {
              /* LGG mode stays the same, just init CDL settings */
              ntreeCompositColorBalanceSyncFromLGG(ntree, node);
            }
            else if (node->custom1 == 1) {
              /* CDL previously used same variables as LGG, copy them over
               * and then sync LGG for comparable results in both modes.
               */
              copy_v3_v3(n->offset, n->lift);
              copy_v3_v3(n->power, n->gamma);
              copy_v3_v3(n->slope, n->gain);
              ntreeCompositColorBalanceSyncFromCDL(ntree, node);
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 3)) {
    bScreen *sc;
    ScrArea *sa;
    SpaceLink *sl;
    Scene *scene;

    /* Update files using invalid (outdated) outlinevis Outliner values. */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *so = (SpaceOutliner *)sl;

            if (!ELEM(so->outlinevis, SO_SCENES, SO_LIBRARIES, SO_SEQUENCE, SO_DATA_API)) {
              so->outlinevis = SO_SCENES;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingTrack", "float", "weight")) {
      MovieClip *clip;
      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        MovieTracking *tracking = &clip->tracking;
        MovieTrackingObject *tracking_object;
        for (tracking_object = tracking->objects.first; tracking_object;
             tracking_object = tracking_object->next) {
          ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
          MovieTrackingTrack *track;
          for (track = tracksbase->first; track; track = track->next) {
            track->weight = 1.0f;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "TriangulateModifierData", "int", "quad_method")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Triangulate) {
            TriangulateModifierData *tmd = (TriangulateModifierData *)md;
            if ((tmd->flag & MOD_TRIANGULATE_BEAUTY)) {
              tmd->quad_method = MOD_TRIANGULATE_QUAD_BEAUTY;
              tmd->ngon_method = MOD_TRIANGULATE_NGON_BEAUTY;
            }
            else {
              tmd->quad_method = MOD_TRIANGULATE_QUAD_FIXED;
              tmd->ngon_method = MOD_TRIANGULATE_NGON_EARCLIP;
            }
          }
        }
      }
    }

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      /* this can now be turned off */
      ToolSettings *ts = scene->toolsettings;
      if (ts->sculpt) {
        ts->sculpt->flags |= SCULPT_DYNTOPO_SUBDIVIDE;
      }

      /* 'Increment' mode disabled for nodes, use true grid snapping instead */
      if (scene->toolsettings->snap_node_mode == SCE_SNAP_MODE_INCREMENT) {
        scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID;
      }

#ifdef WITH_FFMPEG
      /* Update for removed "sound-only" option in FFMPEG export settings. */
      if (scene->r.ffcodecdata.type >= FFMPEG_INVALID) {
        scene->r.ffcodecdata.type = FFMPEG_AVI;
      }
#endif
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 4)) {
    /* Internal degrees to radians conversions... */
    {
      Scene *scene;
      Object *ob;

      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->spotsize = DEG2RADF(la->spotsize);
      }

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;

        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_EdgeSplit) {
            EdgeSplitModifierData *emd = (EdgeSplitModifierData *)md;
            emd->split_angle = DEG2RADF(emd->split_angle);
          }
          else if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bmd->bevel_angle = DEG2RADF(bmd->bevel_angle);
          }
        }
      }

      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        Sequence *seq;
        SEQ_BEGIN (scene->ed, seq) {
          if (seq->type == SEQ_TYPE_WIPE) {
            WipeVars *wv = seq->effectdata;
            wv->angle = DEG2RADF(wv->angle);
          }
        }
        SEQ_END;
      }

      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          bNode *node;
          for (node = ntree->nodes.first; node; node = node->next) {
            if (node->type == CMP_NODE_BOKEHIMAGE) {
              NodeBokehImage *n = node->storage;
              n->angle = DEG2RADF(n->angle);
            }
            if (node->type == CMP_NODE_MASK_BOX) {
              NodeBoxMask *n = node->storage;
              n->rotation = DEG2RADF(n->rotation);
            }
            if (node->type == CMP_NODE_MASK_ELLIPSE) {
              NodeEllipseMask *n = node->storage;
              n->rotation = DEG2RADF(n->rotation);
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingPlaneTrack", "float", "image_opacity")) {
      MovieClip *clip;
      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        MovieTrackingPlaneTrack *plane_track;
        for (plane_track = clip->tracking.plane_tracks.first; plane_track;
             plane_track = plane_track->next) {
          plane_track->image_opacity = 1.0f;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 7)) {
    Scene *scene;
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      Sculpt *sd = scene->toolsettings->sculpt;

      if (sd) {
        enum {
          SCULPT_SYMM_X = (1 << 0),
          SCULPT_SYMM_Y = (1 << 1),
          SCULPT_SYMM_Z = (1 << 2),
          SCULPT_SYMMETRY_FEATHER = (1 << 6),
        };
        int symmetry_flags = sd->flags & 7;

        if (symmetry_flags & SCULPT_SYMM_X) {
          sd->paint.symmetry_flags |= PAINT_SYMM_X;
        }
        if (symmetry_flags & SCULPT_SYMM_Y) {
          sd->paint.symmetry_flags |= PAINT_SYMM_Y;
        }
        if (symmetry_flags & SCULPT_SYMM_Z) {
          sd->paint.symmetry_flags |= PAINT_SYMM_Z;
        }
        if (symmetry_flags & SCULPT_SYMMETRY_FEATHER) {
          sd->paint.symmetry_flags |= PAINT_SYMMETRY_FEATHER;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 8)) {
    Curve *cu;

    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      if (cu->str) {
        cu->len_wchar = BLI_strlen_utf8(cu->str);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 9)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Build) {
          BuildModifierData *bmd = (BuildModifierData *)md;
          if (bmd->randomize) {
            bmd->flag |= MOD_BUILD_FLAG_RANDOMIZE;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 269, 11)) {
    bScreen *sc;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *space_link;

        for (space_link = sa->spacedata.first; space_link; space_link = space_link->next) {
          if (space_link->spacetype == SPACE_IMAGE) {
            ARegion *ar;
            ListBase *lb;

            if (space_link == sa->spacedata.first) {
              lb = &sa->regionbase;
            }
            else {
              lb = &space_link->regionbase;
            }

            for (ar = lb->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_PREVIEW) {
                ar->regiontype = RGN_TYPE_TOOLS;
                ar->alignment = RGN_ALIGN_LEFT;
              }
              else if (ar->regiontype == RGN_TYPE_UI) {
                ar->alignment = RGN_ALIGN_RIGHT;
              }
            }
          }
        }
      }
    }
  }
}

void do_versions_after_linking_260(Main *bmain)
{
  /* Convert the previously used ntree->inputs/ntree->outputs lists to interface nodes.
   * Pre 2.56.2 node trees automatically have all unlinked sockets exposed already,
   * see do_versions_after_linking_250.
   *
   * This assumes valid typeinfo pointers, as set in lib_link_ntree.
   *
   * Note: theoretically only needed in node groups (main->nodetree),
   * but due to a temporary bug such links could have been added in all trees,
   * so have to clean up all of them ...
   *
   * Note: this always runs, without it links with NULL fromnode and tonode remain
   * which causes problems.
   */
  if (!MAIN_VERSION_ATLEAST(bmain, 266, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      bNode *input_node = NULL, *output_node = NULL;
      int num_inputs = 0, num_outputs = 0;
      bNodeLink *link, *next_link;
      /* Only create new interface nodes for actual older files.
       * New file versions already have input/output nodes with duplicate links,
       * in that case just remove the invalid links.
       */
      const bool create_io_nodes = MAIN_VERSION_OLDER(bmain, 266, 2);

      float input_locx = 1000000.0f, input_locy = 0.0f;
      float output_locx = -1000000.0f, output_locy = 0.0f;
      /* rough guess, not nice but we don't have access to UI constants here ... */
      static const float offsetx = 42 + 3 * 20 + 20;
      /*static const float offsety = 0.0f;*/

      if (create_io_nodes) {
        if (ntree->inputs.first) {
          input_node = nodeAddStaticNode(NULL, ntree, NODE_GROUP_INPUT);
        }

        if (ntree->outputs.first) {
          output_node = nodeAddStaticNode(NULL, ntree, NODE_GROUP_OUTPUT);
        }
      }

      /* Redirect links from/to the node tree interface to input/output node.
       * If the fromnode/tonode pointers are NULL, this means a link from/to
       * the ntree interface sockets, which need to be redirected to new interface nodes.
       */
      for (link = ntree->links.first; link; link = next_link) {
        bool free_link = false;
        next_link = link->next;

        if (link->fromnode == NULL) {
          if (input_node) {
            link->fromnode = input_node;
            link->fromsock = node_group_input_find_socket(input_node, link->fromsock->identifier);
            ++num_inputs;

            if (link->tonode) {
              if (input_locx > link->tonode->locx - offsetx) {
                input_locx = link->tonode->locx - offsetx;
              }
              input_locy += link->tonode->locy;
            }
          }
          else {
            free_link = true;
          }
        }

        if (link->tonode == NULL) {
          if (output_node) {
            link->tonode = output_node;
            link->tosock = node_group_output_find_socket(output_node, link->tosock->identifier);
            ++num_outputs;

            if (link->fromnode) {
              if (output_locx < link->fromnode->locx + offsetx) {
                output_locx = link->fromnode->locx + offsetx;
              }
              output_locy += link->fromnode->locy;
            }
          }
          else {
            free_link = true;
          }
        }

        if (free_link) {
          nodeRemLink(ntree, link);
        }
      }

      if (num_inputs > 0) {
        input_locy /= num_inputs;
        input_node->locx = input_locx;
        input_node->locy = input_locy;
      }
      if (num_outputs > 0) {
        output_locy /= num_outputs;
        output_node->locx = output_locx;
        output_node->locy = output_locy;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 60)) {
    /* From this point we no longer write incomplete links for forward
     * compatibility with 2.66, we have to clean them up for all previous
     * versions. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      bNodeLink *link, *next_link;

      for (link = ntree->links.first; link; link = next_link) {
        next_link = link->next;
        if (link->fromnode == NULL || link->tonode == NULL) {
          nodeRemLink(ntree, link);
        }
      }
    }
    FOREACH_NODETREE_END;
  }
}
