/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#include <algorithm>
#include <climits>

#ifndef WIN32
#  include <unistd.h> /* for read close */
#else
#  include "BLI_winstuff.h"
#  include "winsock2.h"
#  include <io.h> /* for open close read */
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_constraint.h"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lattice.hh"
#include "BKE_main.hh" /* for Main */
#include "BKE_mesh.hh" /* for ME_ defines (patching) */
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "BLO_readfile.hh"

#include "readfile.hh"

#include <cerrno>

/* Make preferences read-only, use `versioning_userdef.cc`. */
#define U (*((const UserDef *)&U))

static void vcol_to_fcol(Mesh *mesh)
{
  MFace *mface;
  uint *mcol, *mcoln, *mcolmain;
  int a;

  if (mesh->totface_legacy == 0 || mesh->mcol == nullptr) {
    return;
  }

  mcoln = mcolmain = MEM_malloc_arrayN<uint>(4 * mesh->totface_legacy, "mcoln");
  mcol = (uint *)mesh->mcol;
  mface = mesh->mface;
  for (a = mesh->totface_legacy; a > 0; a--, mface++) {
    mcoln[0] = mcol[mface->v1];
    mcoln[1] = mcol[mface->v2];
    mcoln[2] = mcol[mface->v3];
    mcoln[3] = mcol[mface->v4];
    mcoln += 4;
  }

  MEM_freeN(mesh->mcol);
  mesh->mcol = (MCol *)mcolmain;
}

static void do_version_bone_head_tail_237(Bone *bone)
{
  float vec[3];

  /* head */
  copy_v3_v3(bone->arm_head, bone->arm_mat[3]);

  /* tail is in current local coord system */
  copy_v3_v3(vec, bone->arm_mat[1]);
  mul_v3_fl(vec, bone->length);
  add_v3_v3v3(bone->arm_tail, bone->arm_head, vec);

  LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
    do_version_bone_head_tail_237(child);
  }
}

static void bone_version_238(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    if (bone->rad_tail == 0.0f && bone->rad_head == 0.0f) {
      bone->rad_head = 0.25f * bone->length;
      bone->rad_tail = 0.1f * bone->length;

      bone->dist -= bone->rad_head;
      bone->dist = std::max(bone->dist, 0.0f);
    }
    bone_version_238(&bone->childbase);
  }
}

static void bone_version_239(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    if (bone->layer == 0) {
      bone->layer = 1;
    }
    bone_version_239(&bone->childbase);
  }
}

static void ntree_version_241(bNodeTree *ntree)
{
  if (ntree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->type_legacy == CMP_NODE_BLUR) {
        if (node->storage == nullptr) {
          NodeBlurData *nbd = MEM_callocN<NodeBlurData>("node blur patch");
          nbd->sizex = node->custom1;
          nbd->sizey = node->custom2;
          nbd->filtertype = R_FILTER_QUAD;
          node->storage = nbd;
        }
      }
      else if (node->type_legacy == CMP_NODE_VECBLUR) {
        if (node->storage == nullptr) {
          NodeBlurData *nbd = MEM_callocN<NodeBlurData>("node blur patch");
          nbd->samples = node->custom1;
          nbd->maxspeed = node->custom2;
          nbd->fac = 1.0f;
          node->storage = nbd;
        }
      }
    }
  }
}

static void ntree_version_242(bNodeTree *ntree)
{
  if (ntree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->type_legacy == CMP_NODE_HUE_SAT) {
        if (node->storage) {
          NodeHueSat *nhs = static_cast<NodeHueSat *>(node->storage);
          if (nhs->val == 0.0f) {
            nhs->val = 1.0f;
          }
        }
      }
    }
  }
}

static void ntree_version_245(FileData *fd, Library * /*lib*/, bNodeTree *ntree)
{
  NodeTwoFloats *ntf;
  ID *nodeid;
  Image *image;
  ImageUser *iuser;

  if (ntree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (node->type_legacy == CMP_NODE_ALPHAOVER) {
        if (!node->storage) {
          ntf = MEM_callocN<NodeTwoFloats>("NodeTwoFloats");
          node->storage = ntf;
          if (node->custom1) {
            ntf->x = 1.0f;
          }
        }
      }

      /* fix for temporary flag changes during 245 cycle */
      nodeid = static_cast<ID *>(
          blo_do_versions_newlibadr(fd, &ntree->id, ID_IS_LINKED(ntree), node->id));
      if (node->storage && nodeid && GS(nodeid->name) == ID_IM) {
        image = (Image *)nodeid;
        iuser = static_cast<ImageUser *>(node->storage);
        if (iuser->flag & IMA_OLD_PREMUL) {
          iuser->flag &= ~IMA_OLD_PREMUL;
        }
        if (iuser->flag & IMA_DO_PREMUL) {
          image->flag &= ~IMA_OLD_PREMUL;
          image->alpha_mode = IMA_ALPHA_STRAIGHT;
        }
      }
    }
  }
}

static void idproperties_fix_groups_lengths_recurse(IDProperty *prop)
{
  IDProperty *loop;
  int i;

  for (loop = static_cast<IDProperty *>(prop->data.group.first), i = 0; loop;
       loop = loop->next, i++)
  {
    if (loop->type == IDP_GROUP) {
      idproperties_fix_groups_lengths_recurse(loop);
    }
  }

  if (prop->len != i) {
    printf("Found and fixed bad id property group length.\n");
    prop->len = i;
  }
}

static void idproperties_fix_group_lengths(ListBase idlist)
{
  ID *id;

  for (id = static_cast<ID *>(idlist.first); id; id = static_cast<ID *>(id->next)) {
    if (id->properties) {
      idproperties_fix_groups_lengths_recurse(id->properties);
    }
  }
}

static void customdata_version_242(Mesh *mesh)
{
  CustomDataLayer *layer;
  MTFace *mtf;
  MCol *mcol;
  TFace *tf;
  int a, mtfacen, mcoln;

  if (!mesh->vert_data.totlayer) {
    CustomData_add_layer_with_data(
        &mesh->vert_data, CD_MVERT, mesh->mvert, mesh->verts_num, nullptr);

    if (mesh->dvert) {
      CustomData_add_layer_with_data(
          &mesh->vert_data, CD_MDEFORMVERT, mesh->dvert, mesh->verts_num, nullptr);
    }
  }

  if (!mesh->edge_data.totlayer) {
    CustomData_add_layer_with_data(
        &mesh->edge_data, CD_MEDGE, mesh->medge, mesh->edges_num, nullptr);
  }

  if (!mesh->fdata_legacy.totlayer) {
    CustomData_add_layer_with_data(
        &mesh->fdata_legacy, CD_MFACE, mesh->mface, mesh->totface_legacy, nullptr);

    if (mesh->tface) {
      if (mesh->mcol) {
        MEM_freeN(mesh->mcol);
      }

      mesh->mcol = static_cast<MCol *>(CustomData_add_layer(
          &mesh->fdata_legacy, CD_MCOL, CD_SET_DEFAULT, mesh->totface_legacy));
      mesh->mtface = static_cast<MTFace *>(CustomData_add_layer(
          &mesh->fdata_legacy, CD_MTFACE, CD_SET_DEFAULT, mesh->totface_legacy));

      mtf = mesh->mtface;
      mcol = mesh->mcol;
      tf = mesh->tface;

      for (a = 0; a < mesh->totface_legacy; a++, mtf++, tf++, mcol += 4) {
        memcpy(mcol, tf->col, sizeof(tf->col));
        memcpy(mtf->uv, tf->uv, sizeof(tf->uv));
      }

      MEM_freeN(mesh->tface);
      mesh->tface = nullptr;
    }
    else if (mesh->mcol) {
      CustomData_add_layer_with_data(
          &mesh->fdata_legacy, CD_MCOL, mesh->mcol, mesh->totface_legacy, nullptr);
    }
  }

  if (mesh->tface) {
    MEM_freeN(mesh->tface);
    mesh->tface = nullptr;
  }

  for (a = 0, mtfacen = 0, mcoln = 0; a < mesh->fdata_legacy.totlayer; a++) {
    layer = &mesh->fdata_legacy.layers[a];

    if (layer->type == CD_MTFACE) {
      if (layer->name[0] == 0) {
        if (mtfacen == 0) {
          STRNCPY_UTF8(layer->name, "UVMap");
        }
        else {
          SNPRINTF_UTF8(layer->name, "UVMap.%.3d", mtfacen);
        }
      }
      mtfacen++;
    }
    else if (layer->type == CD_MCOL) {
      if (layer->name[0] == 0) {
        if (mcoln == 0) {
          STRNCPY_UTF8(layer->name, "Col");
        }
        else {
          SNPRINTF_UTF8(layer->name, "Col.%.3d", mcoln);
        }
      }
      mcoln++;
    }
  }
}

/* Only copy render texface layer from active. */
static void customdata_version_243(Mesh *mesh)
{
  CustomDataLayer *layer;
  int a;

  for (a = 0; a < mesh->fdata_legacy.totlayer; a++) {
    layer = &mesh->fdata_legacy.layers[a];
    layer->active_rnd = layer->active;
  }
}

/* struct NodeImageAnim moved to ImageUser, and we make it default available */
static void do_version_ntree_242_2(bNodeTree *ntree)
{
  if (ntree->type == NTREE_COMPOSIT) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      if (ELEM(node->type_legacy, CMP_NODE_IMAGE, CMP_NODE_VIEWER)) {
        /* only image had storage */
        if (node->storage) {
          NodeImageAnim *nia = static_cast<NodeImageAnim *>(node->storage);
          ImageUser *iuser = MEM_callocN<ImageUser>("ima user node");

          iuser->frames = nia->frames;
          iuser->sfra = nia->sfra;
          iuser->offset = nia->nr - 1;
          iuser->cycl = nia->cyclic;

          node->storage = iuser;
          MEM_freeN(nia);
        }
        else {
          ImageUser *iuser = MEM_callocN<ImageUser>("node image user");
          iuser->sfra = 1;
          node->storage = iuser;
        }
      }
    }
  }
}

static void do_version_free_effect_245(Effect *eff)
{
  PartEff *paf;

  if (eff->type == EFF_PARTICLE) {
    paf = (PartEff *)eff;
    if (paf->keys) {
      MEM_freeN(paf->keys);
    }
  }
  MEM_freeN(eff);
}

static void do_version_free_effects_245(ListBase *lb)
{
  while (Effect *eff = static_cast<Effect *>(BLI_pophead(lb))) {
    do_version_free_effect_245(eff);
  }
}

static void do_version_constraints_245(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
      bLocateLikeConstraint *data = (bLocateLikeConstraint *)con->data;

      /* new headtail functionality makes Bone-Tip function obsolete */
      if (data->flag & LOCLIKE_TIP) {
        con->headtail = 1.0f;
      }
    }
  }
}

void blo_do_version_old_trackto_to_constraints(Object *ob)
{
  /* create new trackto constraint from the relationship */
  if (ob->track) {
    bConstraint *con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);
    bTrackToConstraint *data = static_cast<bTrackToConstraint *>(con->data);

    /* copy tracking settings from the object */
    data->tar = ob->track;
    data->reserved1 = ob->trackflag;
    data->reserved2 = ob->upflag;
  }

  /* clear old track setting */
  ob->track = nullptr;
}

static bool strip_set_alpha_mode_cb(Strip *strip, void * /*user_data*/)
{
  if (ELEM(strip->type, STRIP_TYPE_IMAGE, STRIP_TYPE_MOVIE)) {
    strip->alpha_mode = SEQ_ALPHA_STRAIGHT;
  }
  return true;
}

static bool strip_set_blend_mode_cb(Strip *strip, void * /*user_data*/)
{
  if (strip->blend_mode == STRIP_BLEND_REPLACE) {
    strip->blend_opacity = 100.0f;
  }
  return true;
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_pre250(FileData *fd, Library *lib, Main *bmain)
{
  /* WATCH IT!!!: pointers from libdata have not been converted */

  if (bmain->versionfile == 100) {
    /* tex->extend and tex->imageflag have changed: */
    Tex *tex = static_cast<Tex *>(bmain->textures.first);
    while (tex) {
      if (BLO_readfile_id_runtime_tags(tex->id).needs_linking) {
        if (tex->extend == 0) {
          if (tex->xrepeat || tex->yrepeat) {
            tex->extend = TEX_REPEAT;
          }
          else {
            tex->extend = TEX_EXTEND;
            tex->xrepeat = tex->yrepeat = 1;
          }
        }
      }
      tex = static_cast<Tex *>(tex->id.next);
    }
  }

  if (bmain->versionfile <= 101) {
    /* frame mapping */
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    while (sce) {
      sce->r.framapto = 100;
      sce->r.images = 100;
      sce->r.framelen = 1.0;
      sce = static_cast<Scene *>(sce->id.next);
    }
  }

  if (bmain->versionfile <= 103) {
    /* new variable in object: colbits */
    Object *ob = static_cast<Object *>(bmain->objects.first);
    int a;
    while (ob) {
      ob->colbits = 0;
      if (ob->totcol) {
        for (a = 0; a < ob->totcol; a++) {
          if (ob->mat[a]) {
            ob->colbits |= (1 << a);
          }
        }
      }
      ob = static_cast<Object *>(ob->id.next);
    }
  }

  if (bmain->versionfile <= 104) {
    /* timeoffs moved */
    Object *ob = static_cast<Object *>(bmain->objects.first);
    while (ob) {
      if (ob->transflag & 1) {
        ob->transflag -= 1;
      }
      ob = static_cast<Object *>(ob->id.next);
    }
  }

  if (bmain->versionfile <= 106) {
    /* mcol changed */
    Mesh *mesh = static_cast<Mesh *>(bmain->meshes.first);
    while (mesh) {
      if (mesh->mcol) {
        vcol_to_fcol(mesh);
      }
      mesh = static_cast<Mesh *>(mesh->id.next);
    }
  }

  if (bmain->versionfile <= 107) {
    Object *ob;
    ob = static_cast<Object *>(bmain->objects.first);
    while (ob) {
      if (ob->dt == 0) {
        ob->dt = OB_SOLID;
      }
      ob = static_cast<Object *>(ob->id.next);
    }
  }

  if (bmain->versionfile <= 109) {
    /* New variable: `gridlines`. */
    bScreen *screen = static_cast<bScreen *>(bmain->screens.first);
    while (screen) {
      ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
      while (area) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        while (sl) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;

            if (v3d->gridlines == 0) {
              v3d->gridlines = 20;
            }
          }
          sl = sl->next;
        }
        area = area->next;
      }
      screen = static_cast<bScreen *>(screen->id.next);
    }
  }

  if (bmain->versionfile <= 134) {
    Tex *tex = static_cast<Tex *>(bmain->textures.first);
    while (tex) {
      if ((tex->rfac == 0.0f) && (tex->gfac == 0.0f) && (tex->bfac == 0.0f)) {
        tex->rfac = 1.0f;
        tex->gfac = 1.0f;
        tex->bfac = 1.0f;
        tex->filtersize = 1.0f;
      }
      tex = static_cast<Tex *>(tex->id.next);
    }
  }

  if (bmain->versionfile <= 140) {
    /* r-g-b-fac in texture */
    Tex *tex = static_cast<Tex *>(bmain->textures.first);
    while (tex) {
      if ((tex->rfac == 0.0f) && (tex->gfac == 0.0f) && (tex->bfac == 0.0f)) {
        tex->rfac = 1.0f;
        tex->gfac = 1.0f;
        tex->bfac = 1.0f;
        tex->filtersize = 1.0f;
      }
      tex = static_cast<Tex *>(tex->id.next);
    }
  }

  if (bmain->versionfile <= 153) {
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    while (sce) {
      if (sce->r.motion_blur_shutter == 0.0f) {
        sce->r.motion_blur_shutter = 1.0f;
      }
      sce = static_cast<Scene *>(sce->id.next);
    }
  }

  if (bmain->versionfile <= 163) {
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    while (sce) {
      if (sce->r.frs_sec == 0) {
        sce->r.frs_sec = 25;
      }
      sce = static_cast<Scene *>(sce->id.next);
    }
  }

  if (bmain->versionfile <= 164) {
    Mesh *mesh = static_cast<Mesh *>(bmain->meshes.first);
    while (mesh) {
      mesh->smoothresh_legacy = 30;
      mesh = static_cast<Mesh *>(mesh->id.next);
    }
  }

  if (bmain->versionfile <= 165) {
    Mesh *mesh = static_cast<Mesh *>(bmain->meshes.first);
    TFace *tface;
    int nr;
    char *cp;

    while (mesh) {
      if (mesh->tface) {
        nr = mesh->totface_legacy;
        tface = mesh->tface;
        while (nr--) {
          int j;
          for (j = 0; j < 4; j++) {
            int k;
            cp = ((char *)&tface->col[j]) + 1;
            for (k = 0; k < 3; k++) {
              cp[k] = (cp[k] > 126) ? 255 : cp[k] * 2;
            }
          }

          tface++;
        }
      }
      mesh = static_cast<Mesh *>(mesh->id.next);
    }
  }

  if (bmain->versionfile <= 169) {
    Mesh *mesh = static_cast<Mesh *>(bmain->meshes.first);
    while (mesh) {
      if (mesh->subdiv == 0) {
        mesh->subdiv = 1;
      }
      mesh = static_cast<Mesh *>(mesh->id.next);
    }
  }

  if (bmain->versionfile <= 169) {
    bScreen *screen = static_cast<bScreen *>(bmain->screens.first);
    while (screen) {
      ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
      while (area) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        while (sl) {
          if (sl->spacetype == SPACE_GRAPH) {
            SpaceGraph *sipo = (SpaceGraph *)sl;
            sipo->v2d.max[0] = 15000.0;
          }
          sl = sl->next;
        }
        area = area->next;
      }
      screen = static_cast<bScreen *>(screen->id.next);
    }
  }

  if (bmain->versionfile <= 170) {
    Object *ob = static_cast<Object *>(bmain->objects.first);
    PartEff *paf;
    while (ob) {
      paf = BKE_object_do_version_give_parteff_245(ob);
      if (paf) {
        if (paf->staticstep == 0) {
          paf->staticstep = 5;
        }
      }
      ob = static_cast<Object *>(ob->id.next);
    }
  }

  if (bmain->versionfile <= 171) {
    bScreen *screen = static_cast<bScreen *>(bmain->screens.first);
    while (screen) {
      ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
      while (area) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        while (sl) {
          if (sl->spacetype == SPACE_TEXT) {
            SpaceText *st = (SpaceText *)sl;
            st->lheight = 12;
          }
          sl = sl->next;
        }
        area = area->next;
      }
      screen = static_cast<bScreen *>(screen->id.next);
    }
  }

  if (bmain->versionfile <= 173) {
    int a, b;
    Mesh *mesh = static_cast<Mesh *>(bmain->meshes.first);
    while (mesh) {
      if (mesh->tface) {
        TFace *tface = mesh->tface;
        for (a = 0; a < mesh->totface_legacy; a++, tface++) {
          for (b = 0; b < 4; b++) {
            tface->uv[b][0] /= 32767.0f;
            tface->uv[b][1] /= 32767.0f;
          }
        }
      }
      mesh = static_cast<Mesh *>(mesh->id.next);
    }
  }

  if (bmain->versionfile <= 204) {
    bSound *sound;

    sound = static_cast<bSound *>(bmain->sounds.first);
    while (sound) {
      if (sound->volume < 0.01f) {
        sound->volume = 1.0f;
      }
      sound = static_cast<bSound *>(sound->id.next);
    }
  }

  if (bmain->versionfile <= 212) {
    bSound *sound;
    Mesh *mesh;

    sound = static_cast<bSound *>(bmain->sounds.first);
    while (sound) {
      sound->max_gain = 1.0;
      sound->min_gain = 0.0;
      sound->distance = 1.0;

      if (sound->attenuation > 0.0f) {
        sound->flags |= SOUND_FLAGS_3D;
      }
      else {
        sound->flags &= ~SOUND_FLAGS_3D;
      }

      sound = static_cast<bSound *>(sound->id.next);
    }

    /* `mesh->subdiv` changed to reflect the actual reparametrization
     * better, and S-meshes were removed - if it was a S-mesh make
     * it a subsurf, and reset the subdivision level because subsurf
     * takes a lot more work to calculate. */
    for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
         mesh = static_cast<Mesh *>(mesh->id.next))
    {
      enum {
        ME_SMESH = (1 << 6),
        ME_SUBSURF = (1 << 7),
      };
      if (mesh->flag & ME_SMESH) {
        mesh->flag &= ~ME_SMESH;
        mesh->flag |= ME_SUBSURF;

        mesh->subdiv = 1;
      }
      else {
        if (mesh->subdiv < 2) {
          mesh->subdiv = 1;
        }
        else {
          mesh->subdiv--;
        }
      }
    }
  }

  if (bmain->versionfile <= 220) {
    Mesh *mesh;

    /* Began using alpha component of vertex colors, but
     * old file vertex colors are undefined, reset them
     * to be fully opaque. -zr
     */
    for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
         mesh = static_cast<Mesh *>(mesh->id.next))
    {
      if (mesh->mcol) {
        int i;

        for (i = 0; i < mesh->totface_legacy * 4; i++) {
          MCol *mcol = &mesh->mcol[i];
          mcol->a = 255;
        }
      }
      if (mesh->tface) {
        int i, j;

        for (i = 0; i < mesh->totface_legacy; i++) {
          TFace *tf = &((TFace *)mesh->tface)[i];

          for (j = 0; j < 4; j++) {
            char *col = (char *)&tf->col[j];

            col[0] = 255;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 223) {
    VFont *vf;
    for (vf = static_cast<VFont *>(bmain->fonts.first); vf; vf = static_cast<VFont *>(vf->id.next))
    {
      if (BLI_str_endswith(vf->filepath, ".Bfont")) {
        STRNCPY(vf->filepath, FO_BUILTIN_NAME);
      }
    }
  }

  if (bmain->versionfile <= 224) {
    bSound *sound;
    Mesh *mesh;
    bScreen *screen;

    for (sound = static_cast<bSound *>(bmain->sounds.first); sound;
         sound = static_cast<bSound *>(sound->id.next))
    {
      if (sound->packedfile) {
        if (sound->newpackedfile == nullptr) {
          sound->newpackedfile = sound->packedfile;
        }
        sound->packedfile = nullptr;
      }
    }
    /* Make sure that old subsurf meshes don't have zero subdivision level for rendering */
    for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
         mesh = static_cast<Mesh *>(mesh->id.next))
    {
      enum { ME_SUBSURF = (1 << 7) };
      if ((mesh->flag & ME_SUBSURF) && (mesh->subdivr == 0)) {
        mesh->subdivr = mesh->subdiv;
      }
    }

    /* some oldfile patch, moved from set_func_space */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_GRAPH) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->v2d.keeptot = 0;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 227) {
    Scene *sce;
    bScreen *screen;
    Object *ob;

    /* NOTE(@theeth): As of now, this insures that the transition from the old Track system
     * to the new full constraint Track is painless for everyone. */
    ob = static_cast<Object *>(bmain->objects.first);

    while (ob) {
      ListBase &list = ob->constraints;

      /* check for already existing TrackTo constraint
       * set their track and up flag correctly
       */

      LISTBASE_FOREACH (bConstraint *, curcon, &list) {
        if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
          bTrackToConstraint *data = static_cast<bTrackToConstraint *>(curcon->data);
          data->reserved1 = ob->trackflag;
          data->reserved2 = ob->upflag;
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            LISTBASE_FOREACH (bConstraint *, curcon, &pchan->constraints) {
              if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
                bTrackToConstraint *data = static_cast<bTrackToConstraint *>(curcon->data);
                data->reserved1 = ob->trackflag;
                data->reserved2 = ob->upflag;
              }
            }
          }
        }
      }

      /* Change Ob->Track in real TrackTo constraint */
      blo_do_version_old_trackto_to_constraints(ob);

      ob = static_cast<Object *>(ob->id.next);
    }

    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      sce->audio.mixrate = 48000;
      sce->audio.flag |= AUDIO_SCRUB;
    }

    /* patch for old wrong max view2d settings, allows zooming out more */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *sac = (SpaceAction *)sl;
            sac->v2d.max[0] = 32000;
          }
          else if (sl->spacetype == SPACE_NLA) {
            SpaceNla *sla = (SpaceNla *)sl;
            sla->v2d.max[0] = 32000;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 228) {
    bScreen *screen;
    Object *ob;

    /* As of now, this insures that the transition from the old Track system
     * to the new full constraint Track is painless for everyone.
     */
    ob = static_cast<Object *>(bmain->objects.first);

    while (ob) {
      ListBase &list = ob->constraints;

      /* check for already existing TrackTo constraint
       * set their track and up flag correctly */

      LISTBASE_FOREACH (bConstraint *, curcon, &list) {
        if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
          bTrackToConstraint *data = static_cast<bTrackToConstraint *>(curcon->data);
          data->reserved1 = ob->trackflag;
          data->reserved2 = ob->upflag;
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            LISTBASE_FOREACH (bConstraint *, curcon, &pchan->constraints) {
              if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
                bTrackToConstraint *data = static_cast<bTrackToConstraint *>(curcon->data);
                data->reserved1 = ob->trackflag;
                data->reserved2 = ob->upflag;
              }
            }
          }
        }
      }

      ob = static_cast<Object *>(ob->id.next);
    }

    /* convert old mainb values for new button panels */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_PROPERTIES) {
            SpaceProperties *sbuts = (SpaceProperties *)sl;

            sbuts->v2d.maxzoom = 1.2f;

            if (sbuts->mainb == BUTS_LAMP) {
              sbuts->mainb = CONTEXT_SHADING;
              // sbuts->tab[CONTEXT_SHADING] = TAB_SHADING_LAMP;
            }
            else if (sbuts->mainb == BUTS_MAT) {
              sbuts->mainb = CONTEXT_SHADING;
              // sbuts->tab[CONTEXT_SHADING] = TAB_SHADING_MAT;
            }
            else if (sbuts->mainb == BUTS_TEX) {
              sbuts->mainb = CONTEXT_SHADING;
              // sbuts->tab[CONTEXT_SHADING] = TAB_SHADING_TEX;
            }
            else if (sbuts->mainb == BUTS_ANIM) {
              sbuts->mainb = CONTEXT_OBJECT;
            }
            else if (sbuts->mainb == BUTS_WORLD) {
              sbuts->mainb = CONTEXT_SCENE;
              // sbuts->tab[CONTEXT_SCENE] = TAB_SCENE_WORLD;
            }
            else if (sbuts->mainb == BUTS_RENDER) {
              sbuts->mainb = CONTEXT_SCENE;
              // sbuts->tab[CONTEXT_SCENE] = TAB_SCENE_RENDER;
            }
            else if (sbuts->mainb == BUTS_FPAINT) {
              sbuts->mainb = CONTEXT_EDITING;
            }
            else if (sbuts->mainb == BUTS_RADIO) {
              sbuts->mainb = CONTEXT_SHADING;
              // sbuts->tab[CONTEXT_SHADING] = TAB_SHADING_RAD;
            }
            else if (sbuts->mainb == BUTS_CONSTRAINT) {
              sbuts->mainb = CONTEXT_OBJECT;
            }
            else if (sbuts->mainb == BUTS_SCRIPT) {
              sbuts->mainb = CONTEXT_OBJECT;
            }
            else if (sbuts->mainb == BUTS_EDIT) {
              sbuts->mainb = CONTEXT_EDITING;
            }
            else {
              sbuts->mainb = CONTEXT_SCENE;
            }
          }
        }
      }
    }
  }

  /* NOTE(@ton): made this 230 instead of 229,
   * to be sure (files from the `tuhopuu` branch) and this is a reliable check anyway
   * nevertheless, we might need to think over a fitness (initialize)
   * check apart from the do_versions(). */

  if (bmain->versionfile <= 230) {
    bScreen *screen;

    /* New variable block-scale, for panels in any area. */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          /* added: 5x better zoom in for action */
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *sac = (SpaceAction *)sl;
            sac->v2d.maxzoom = 50;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 231) {
    bScreen *screen = static_cast<bScreen *>(bmain->screens.first);

    /* new bit flags for showing/hiding grid floor and axes */

    while (screen) {
      ScrArea *area = static_cast<ScrArea *>(screen->areabase.first);
      while (area) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        while (sl) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;

            if (v3d->gridflag == 0) {
              v3d->gridflag |= V3D_SHOW_X;
              v3d->gridflag |= V3D_SHOW_Y;
              v3d->gridflag |= V3D_SHOW_FLOOR;
              v3d->gridflag &= ~V3D_SHOW_Z;
            }
          }
          sl = sl->next;
        }
        area = area->next;
      }
      screen = static_cast<bScreen *>(screen->id.next);
    }
  }

  if (bmain->versionfile <= 232) {
    Tex *tex = static_cast<Tex *>(bmain->textures.first);
    World *wrld = static_cast<World *>(bmain->worlds.first);
    bScreen *screen;

    while (tex) {
      if ((tex->flag & (TEX_CHECKER_ODD + TEX_CHECKER_EVEN)) == 0) {
        tex->flag |= TEX_CHECKER_ODD;
      }
      /* Copied from kernel `texture.cc`. */
      if (tex->ns_outscale == 0.0f) {
        /* musgrave */
        tex->mg_H = 1.0f;
        tex->mg_lacunarity = 2.0f;
        tex->mg_octaves = 2.0f;
        tex->mg_offset = 1.0f;
        tex->mg_gain = 1.0f;
        tex->ns_outscale = 1.0f;
        /* distnoise */
        tex->dist_amount = 1.0f;
        /* voronoi */
        tex->vn_w1 = 1.0f;
        tex->vn_mexp = 2.5f;
      }
      tex = static_cast<Tex *>(tex->id.next);
    }

    while (wrld) {
      if (wrld->aodist == 0.0f) {
        wrld->aodist = 10.0f;
      }
      if (wrld->aoenergy == 0.0f) {
        wrld->aoenergy = 1.0f;
      }
      wrld = static_cast<World *>(wrld->id.next);
    }

    /* New variable block-scale, for panels in any area, do again because new
     * areas didn't initialize it to 0.7 yet. */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          /* added: 5x better zoom in for nla */
          if (sl->spacetype == SPACE_NLA) {
            SpaceNla *snla = (SpaceNla *)sl;
            snla->v2d.maxzoom = 50;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 233) {
    bScreen *screen;

    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->flag |= V3D_SELECT_OUTLINE;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 234) {
    bScreen *screen;

    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_TEXT) {
            SpaceText *st = (SpaceText *)sl;
            if (st->tabnumber == 0) {
              st->tabnumber = 2;
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 235) {
    Tex *tex = static_cast<Tex *>(bmain->textures.first);
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    Editing *ed;

    while (tex) {
      if (tex->nabla == 0.0f) {
        tex->nabla = 0.025f;
      }
      tex = static_cast<Tex *>(tex->id.next);
    }
    while (sce) {
      ed = sce->ed;
      if (ed) {
        blender::seq::foreach_strip(&sce->ed->seqbase, strip_set_alpha_mode_cb, nullptr);
      }

      sce = static_cast<Scene *>(sce->id.next);
    }
  }

  if (bmain->versionfile <= 236) {
    Object *ob;
    Camera *cam = static_cast<Camera *>(bmain->cameras.first);

    while (cam) {
      if (cam->ortho_scale == 0.0f) {
        cam->ortho_scale = 256.0f / cam->lens;
        if (cam->type == CAM_ORTHO) {
          printf("NOTE: ortho render has changed, tweak new Camera 'scale' value.\n");
        }
      }
      cam = static_cast<Camera *>(cam->id.next);
    }
    /* Force oops draw if depsgraph was set. */
    /* Set time line var. */

    /* softbody init new vars */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->soft) {
        if (ob->soft->defgoal == 0.0f) {
          ob->soft->defgoal = 0.7f;
        }
        if (ob->soft->physics_speed == 0.0f) {
          ob->soft->physics_speed = 1.0f;
        }
      }
      if (ob->soft && ob->soft->vertgroup == 0) {
        bDeformGroup *locGroup = BKE_object_defgroup_find_name(ob, "SOFTGOAL");
        if (locGroup) {
          /* retrieve index for that group */
          ob->soft->vertgroup = 1 + BLI_findindex(&ob->defbase, locGroup);
        }
      }
    }
  }

  if (bmain->versionfile <= 237) {
    bArmature *arm;
    Object *ob;

    /* armature recode checks */
    for (arm = static_cast<bArmature *>(bmain->armatures.first); arm;
         arm = static_cast<bArmature *>(arm->id.next))
    {
      BKE_armature_where_is(arm);

      LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
        do_version_bone_head_tail_237(bone);
      }
    }
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->parent) {
        Object *parent = static_cast<Object *>(
            blo_do_versions_newlibadr(fd, &ob->id, ID_IS_LINKED(ob), ob->parent));
        if (parent && parent->type == OB_LATTICE) {
          ob->partype = PARSKEL;
        }
      }

      /* NOTE: #BKE_pose_rebuild is further only called on leave edit-mode. */
      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          BKE_pose_tag_recalc(bmain, ob->pose);
        }

        /* Cannot call stuff now (pointers!), done in #setup_app_data. */
        ob->id.recalc |= ID_RECALC_ALL;

        /* New generic X-ray option. */
        arm = static_cast<bArmature *>(
            blo_do_versions_newlibadr(fd, &ob->id, ID_IS_LINKED(ob), ob->data));
        enum { ARM_DRAWXRAY = (1 << 1) };
        if (arm->flag & ARM_DRAWXRAY) {
          ob->dtx |= OB_DRAW_IN_FRONT;
        }
      }
      else if (ob->type == OB_MESH) {
        Mesh *mesh = static_cast<Mesh *>(
            blo_do_versions_newlibadr(fd, &ob->id, ID_IS_LINKED(ob), ob->data));

        enum {
          ME_SUBSURF = (1 << 7),
          ME_OPT_EDGES = (1 << 8),
        };

        if (mesh->flag & ME_SUBSURF) {
          SubsurfModifierData *smd = (SubsurfModifierData *)BKE_modifier_new(
              eModifierType_Subsurf);

          smd->levels = std::max<short>(1, mesh->subdiv);
          smd->renderLevels = std::max<short>(1, mesh->subdivr);
          smd->subdivType = mesh->subsurftype;

          smd->modifier.mode = 0;
          if (mesh->subdiv != 0) {
            smd->modifier.mode |= 1;
          }
          if (mesh->subdivr != 0) {
            smd->modifier.mode |= 2;
          }

          if (mesh->flag & ME_OPT_EDGES) {
            smd->flags |= eSubsurfModifierFlag_ControlEdges;
          }

          BLI_addtail(&ob->modifiers, smd);

          BKE_modifier_unique_name(&ob->modifiers, (ModifierData *)smd);
        }
      }

      /* follow path constraint needs to set the 'path' option in curves... */
      LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
        if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
          bFollowPathConstraint *data = static_cast<bFollowPathConstraint *>(con->data);
          Object *obc = static_cast<Object *>(
              blo_do_versions_newlibadr(fd, &ob->id, ID_IS_LINKED(ob), data->tar));

          if (obc && obc->type == OB_CURVES_LEGACY) {
            Curve *cu = static_cast<Curve *>(
                blo_do_versions_newlibadr(fd, &obc->id, ID_IS_LINKED(obc), obc->data));
            if (cu) {
              cu->flag |= CU_PATH;
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 238) {
    Lattice *lt;
    Object *ob;
    bArmature *arm;
    Mesh *mesh;
    Key *key;
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);

    while (sce) {
      if (sce->toolsettings == nullptr) {
        sce->toolsettings = MEM_callocN<ToolSettings>("Tool Settings Struct");
        sce->toolsettings->doublimit = 0.001f;
      }
      sce = static_cast<Scene *>(sce->id.next);
    }

    for (lt = static_cast<Lattice *>(bmain->lattices.first); lt;
         lt = static_cast<Lattice *>(lt->id.next))
    {
      if (lt->fu == 0.0f && lt->fv == 0.0f && lt->fw == 0.0f) {
        calc_lat_fudu(lt->flag, lt->pntsu, &lt->fu, &lt->du);
        calc_lat_fudu(lt->flag, lt->pntsv, &lt->fv, &lt->dv);
        calc_lat_fudu(lt->flag, lt->pntsw, &lt->fw, &lt->dw);
      }
    }

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      PartEff *paf;

      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Subsurf) {
          SubsurfModifierData *smd = (SubsurfModifierData *)md;

          smd->flags &= ~(eSubsurfModifierFlag_Incremental | eSubsurfModifierFlag_DebugIncr);
        }
      }

      if ((ob->softflag & OB_SB_ENABLE) && !BKE_modifiers_findby_type(ob, eModifierType_Softbody))
      {
        if (ob->softflag & OB_SB_POSTDEF) {
          ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);

          while (md && BKE_modifier_get_info(ModifierType(md->type))->type ==
                           ModifierTypeType::OnlyDeform)
          {
            md = md->next;
          }

          BLI_insertlinkbefore(&ob->modifiers, md, BKE_modifier_new(eModifierType_Softbody));
        }
        else {
          BLI_addhead(&ob->modifiers, BKE_modifier_new(eModifierType_Softbody));
        }

        ob->softflag &= ~OB_SB_ENABLE;
      }

      if (ob->pose) {
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          /* NOTE: pchan->bone is also lib-link stuff. */
          if (pchan->limitmin[0] == 0.0f && pchan->limitmax[0] == 0.0f) {
            pchan->limitmin[0] = pchan->limitmin[1] = pchan->limitmin[2] = -180.0f;
            pchan->limitmax[0] = pchan->limitmax[1] = pchan->limitmax[2] = 180.0f;

            LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
              if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
                bKinematicConstraint *data = (bKinematicConstraint *)con->data;
                data->weight = 1.0f;
                data->orientweight = 1.0f;
                data->flag &= ~CONSTRAINT_IK_ROT;

                /* enforce conversion from old IK_TOPARENT to rootbone index */
                data->rootbone = -1;

                /* update_pose_etc handles rootbone == -1 */
                BKE_pose_tag_recalc(bmain, ob->pose);
              }
            }
          }
        }
      }

      paf = BKE_object_do_version_give_parteff_245(ob);
      if (paf) {
        if (paf->disp == 0) {
          paf->disp = 100;
        }
        if (paf->speedtex == 0) {
          paf->speedtex = 8;
        }
        if (paf->omat == 0) {
          paf->omat = 1;
        }
      }
    }

    for (arm = static_cast<bArmature *>(bmain->armatures.first); arm;
         arm = static_cast<bArmature *>(arm->id.next))
    {
      bone_version_238(&arm->bonebase);
      arm->deformflag |= ARM_DEF_VGROUP;
    }

    for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
         mesh = static_cast<Mesh *>(mesh->id.next))
    {
      if (!mesh->medge) {
        BKE_mesh_calc_edges_legacy(mesh);
      }
      else {
        BKE_mesh_strip_loose_faces(mesh);
      }
    }

    for (key = static_cast<Key *>(bmain->shapekeys.first); key;
         key = static_cast<Key *>(key->id.next))
    {
      int index = 1;

      LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
        if (kb == key->refkey) {
          if (kb->name[0] == 0) {
            STRNCPY_UTF8(kb->name, "Basis");
          }
        }
        else {
          if (kb->name[0] == 0) {
            SNPRINTF_UTF8(kb->name, "Key %d", index);
          }
          index++;
        }
      }
    }
  }

  if (bmain->versionfile <= 239) {
    bArmature *arm;
    Object *ob;
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    Camera *cam = static_cast<Camera *>(bmain->cameras.first);
    int set_passepartout = 0;

    /* deformflag is local in modifier now */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Armature) {
          ArmatureModifierData *amd = (ArmatureModifierData *)md;
          if (amd->object && amd->deformflag == 0) {
            Object *oba = static_cast<Object *>(
                blo_do_versions_newlibadr(fd, &ob->id, ID_IS_LINKED(ob), amd->object));
            arm = static_cast<bArmature *>(
                blo_do_versions_newlibadr(fd, &oba->id, ID_IS_LINKED(oba), oba->data));
            amd->deformflag = arm->deformflag;
          }
        }
      }
    }

    for (arm = static_cast<bArmature *>(bmain->armatures.first); arm;
         arm = static_cast<bArmature *>(arm->id.next))
    {
      bone_version_239(&arm->bonebase);
      if (arm->layer == 0) {
        arm->layer = 1;
      }
    }

    for (; sce; sce = static_cast<Scene *>(sce->id.next)) {
      if (sce->r.scemode & R_PASSEPARTOUT) {
        set_passepartout = 1;
        sce->r.scemode &= ~R_PASSEPARTOUT;
      }
    }

    for (; cam; cam = static_cast<Camera *>(cam->id.next)) {
      if (set_passepartout) {
        cam->flag |= CAM_SHOWPASSEPARTOUT;
      }

      /* make sure old cameras have title safe on */
      if (!(cam->flag & CAM_SHOW_SAFE_MARGINS)) {
        cam->flag |= CAM_SHOW_SAFE_MARGINS;
      }

      /* set an appropriate camera passepartout alpha */
      if (!(cam->passepartalpha)) {
        cam->passepartalpha = 0.2f;
      }
    }
  }

  if (bmain->versionfile <= 241) {
    Object *ob;
    Scene *sce;
    bArmature *arm;
    bNodeTree *ntree;

    /* updating layers still */
    for (arm = static_cast<bArmature *>(bmain->armatures.first); arm;
         arm = static_cast<bArmature *>(arm->id.next))
    {
      bone_version_239(&arm->bonebase);
      if (arm->layer == 0) {
        arm->layer = 1;
      }
    }
    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      if (sce->audio.mixrate == 0) {
        sce->audio.mixrate = 48000;
      }

      /* We don't add default layer since blender2.8 because the layers
       * are now in Scene->view_layers and a default layer is created in
       * the doversion later on. */

      /* new layer flag for sky, was default for solid */
      LISTBASE_FOREACH (SceneRenderLayer *, srl, &sce->r.layers) {
        if (srl->layflag & SCE_LAY_SOLID) {
          srl->layflag |= SCE_LAY_SKY;
        }
        srl->passflag &= (SCE_PASS_COMBINED | SCE_PASS_DEPTH | SCE_PASS_NORMAL | SCE_PASS_VECTOR);
      }

      /* node version changes */
      if (sce->nodetree) {
        ntree_version_241(sce->nodetree);
      }

      /* uv calculation options moved to toolsettings */
      if (sce->toolsettings->unwrapper == UVCALC_UNWRAP_METHOD_ANGLE) {
        sce->toolsettings->unwrapper = UVCALC_UNWRAP_METHOD_CONFORMAL;
        sce->toolsettings->uvcalc_flag = UVCALC_FILLHOLES;
      }
    }

    for (ntree = static_cast<bNodeTree *>(bmain->nodetrees.first); ntree;
         ntree = static_cast<bNodeTree *>(ntree->id.next))
    {
      ntree_version_241(ntree);
    }

    /* for empty drawsize and drawtype */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->empty_drawsize == 0.0f) {
        ob->empty_drawtype = OB_ARROWS;
        ob->empty_drawsize = 1.0;
      }
    }

    /* during 2.41 images with this name were used for viewer node output, lets fix that */
    if (bmain->versionfile == 241) {
      Image *ima;
      for (ima = static_cast<Image *>(bmain->images.first); ima;
           ima = static_cast<Image *>(ima->id.next))
      {
        if (STREQ(ima->filepath, "Compositor")) {
          BLI_strncpy_utf8(ima->id.name + 2, "Viewer Node", sizeof(ima->id.name) - 2);
          STRNCPY(ima->filepath, "Viewer Node");
        }
      }
    }
  }

  if (bmain->versionfile <= 242) {
    Scene *sce;
    bScreen *screen;
    Object *ob;
    Curve *cu;
    Material *ma;
    Mesh *mesh;
    Collection *collection;
    BezTriple *bezt;
    BPoint *bp;
    bNodeTree *ntree;
    int a;

    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      ScrArea *area;
      area = static_cast<ScrArea *>(screen->areabase.first);
      while (area) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            if (v3d->gridsubdiv == 0) {
              v3d->gridsubdiv = 10;
            }
          }
        }
        area = area->next;
      }
    }

    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      enum {
        R_THREADS = (1 << 19),
      };
      if (sce->toolsettings->select_thresh == 0.0f) {
        sce->toolsettings->select_thresh = 0.01f;
      }
      if (sce->r.threads == 0) {
        if (sce->r.mode & R_THREADS) {
          sce->r.threads = 2;
        }
        else {
          sce->r.threads = 1;
        }
      }
      if (sce->nodetree) {
        ntree_version_242(sce->nodetree);
      }
    }

    for (ntree = static_cast<bNodeTree *>(bmain->nodetrees.first); ntree;
         ntree = static_cast<bNodeTree *>(ntree->id.next))
    {
      ntree_version_242(ntree);
    }

    /* add default radius values to old curve points */
    for (cu = static_cast<Curve *>(bmain->curves.first); cu;
         cu = static_cast<Curve *>(cu->id.next))
    {
      LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
        if (nu->bezt) {
          for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
            if (!bezt->radius) {
              bezt->radius = 1.0;
            }
          }
        }
        else if (nu->bp) {
          for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
            if (!bp->radius) {
              bp->radius = 1.0;
            }
          }
        }
      }
    }

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      ListBase &list = ob->constraints;

      /* check for already existing MinMax (floor) constraint
       * and update the sticky flagging */

      LISTBASE_FOREACH (bConstraint *, curcon, &list) {
        switch (curcon->type) {
          case CONSTRAINT_TYPE_ROTLIKE: {
            bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(curcon->data);

            /* version patch from buttons_object.c */
            if (data->flag == 0) {
              data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
            }

            break;
          }
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            LISTBASE_FOREACH (bConstraint *, curcon, &pchan->constraints) {
              switch (curcon->type) {
                case CONSTRAINT_TYPE_KINEMATIC: {
                  bKinematicConstraint *data = static_cast<bKinematicConstraint *>(curcon->data);
                  if (!(data->flag & CONSTRAINT_IK_POS)) {
                    data->flag |= CONSTRAINT_IK_POS;
                    data->flag |= CONSTRAINT_IK_STRETCH;
                  }
                  break;
                }
                case CONSTRAINT_TYPE_ROTLIKE: {
                  bRotateLikeConstraint *data = static_cast<bRotateLikeConstraint *>(curcon->data);

                  /* version patch from buttons_object.c */
                  if (data->flag == 0) {
                    data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
                  }
                  break;
                }
              }
            }
          }
        }
      }

      /* copy old object level track settings to curve modifiers */
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Curve) {
          CurveModifierData *cmd = (CurveModifierData *)md;

          if (cmd->defaxis == 0) {
            cmd->defaxis = ob->trackflag + 1;
          }
        }
      }
    }

    for (ma = static_cast<Material *>(bmain->materials.first); ma;
         ma = static_cast<Material *>(ma->id.next))
    {
      if (ma->nodetree) {
        ntree_version_242(ma->nodetree);
      }
    }

    for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
         mesh = static_cast<Mesh *>(mesh->id.next))
    {
      customdata_version_242(mesh);
    }

    for (collection = static_cast<Collection *>(bmain->collections.first); collection;
         collection = static_cast<Collection *>(collection->id.next))
    {
      if (collection->layer == 0) {
        collection->layer = (1 << 20) - 1;
      }
    }

    /* now, subversion control! */
    if (bmain->subversionfile < 3) {
      Image *ima;
      Tex *tex;

      /* Image refactor initialize */
      for (ima = static_cast<Image *>(bmain->images.first); ima;
           ima = static_cast<Image *>(ima->id.next))
      {
        ima->source = IMA_SRC_FILE;
        ima->type = IMA_TYPE_IMAGE;

        ima->gen_x = 256;
        ima->gen_y = 256;
        ima->gen_type = 1;

        if (STREQLEN(ima->id.name + 2, "Viewer Node", sizeof(ima->id.name) - 2)) {
          ima->source = IMA_SRC_VIEWER;
          ima->type = IMA_TYPE_COMPOSITE;
        }
        if (STREQLEN(ima->id.name + 2, "Render Result", sizeof(ima->id.name) - 2)) {
          ima->source = IMA_SRC_VIEWER;
          ima->type = IMA_TYPE_R_RESULT;
        }
      }
      for (tex = static_cast<Tex *>(bmain->textures.first); tex;
           tex = static_cast<Tex *>(tex->id.next))
      {
        enum {
          TEX_ANIMCYCLIC = (1 << 6),
          TEX_ANIM5 = (1 << 7),
        };

        if (tex->type == TEX_IMAGE && tex->ima) {
          ima = static_cast<Image *>(
              blo_do_versions_newlibadr(fd, &tex->id, ID_IS_LINKED(tex), tex->ima));
          if (tex->imaflag & TEX_ANIM5) {
            ima->source = IMA_SRC_MOVIE;
          }
        }
        tex->iuser.frames = tex->frames;
        tex->iuser.offset = tex->offset;
        tex->iuser.sfra = tex->sfra;
        tex->iuser.cycl = (tex->imaflag & TEX_ANIMCYCLIC) != 0;
      }
      for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
           sce = static_cast<Scene *>(sce->id.next))
      {
        if (sce->nodetree) {
          do_version_ntree_242_2(sce->nodetree);
        }
      }
      for (ntree = static_cast<bNodeTree *>(bmain->nodetrees.first); ntree;
           ntree = static_cast<bNodeTree *>(ntree->id.next))
      {
        do_version_ntree_242_2(ntree);
      }
      for (ma = static_cast<Material *>(bmain->materials.first); ma;
           ma = static_cast<Material *>(ma->id.next))
      {
        if (ma->nodetree) {
          do_version_ntree_242_2(ma->nodetree);
        }
      }
    }
  }

  if (bmain->versionfile <= 243) {
    Object *ob = static_cast<Object *>(bmain->objects.first);

    for (; ob; ob = static_cast<Object *>(ob->id.next)) {
      LISTBASE_FOREACH (bDeformGroup *, curdef, &ob->defbase) {
        /* replace an empty-string name with unique name */
        if (curdef->name[0] == '\0') {
          BKE_object_defgroup_unique_name(curdef, ob);
        }
      }

      if (bmain->versionfile < 243 || bmain->subversionfile < 1) {
        /* translate old mirror modifier axis values to new flags */
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Mirror) {
            MirrorModifierData *mmd = (MirrorModifierData *)md;

            switch (mmd->axis) {
              case 0:
                mmd->flag |= MOD_MIR_AXIS_X;
                break;
              case 1:
                mmd->flag |= MOD_MIR_AXIS_Y;
                break;
              case 2:
                mmd->flag |= MOD_MIR_AXIS_Z;
                break;
            }

            mmd->axis = 0;
          }
        }
      }
    }

    /* render layer added, this is not the active layer */
    if (bmain->versionfile <= 243 || bmain->subversionfile < 2) {
      Mesh *mesh;
      for (mesh = static_cast<Mesh *>(bmain->meshes.first); mesh;
           mesh = static_cast<Mesh *>(mesh->id.next))
      {
        customdata_version_243(mesh);
      }
    }
  }

  if (bmain->versionfile <= 244) {
    bScreen *screen;

    if (bmain->versionfile != 244 || bmain->subversionfile < 2) {
      /* correct older action editors - incorrect scrolling */
      for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
           screen = static_cast<bScreen *>(screen->id.next))
      {
        ScrArea *area;
        area = static_cast<ScrArea *>(screen->areabase.first);
        while (area) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_ACTION) {
              SpaceAction *saction = (SpaceAction *)sl;

              saction->v2d.tot.ymin = -1000.0;
              saction->v2d.tot.ymax = 0.0;

              saction->v2d.cur.ymin = -75.0;
              saction->v2d.cur.ymax = 5.0;
            }
          }
          area = area->next;
        }
      }
    }
  }

  if (bmain->versionfile <= 245) {
    Scene *sce;
    Object *ob;
    Image *ima;
    Material *ma;
    ParticleSettings *part;
    bNodeTree *ntree;
    Tex *tex;

    /* unless the file was created 2.44.3 but not 2.45, update the constraints */
    if (!(bmain->versionfile == 244 && bmain->subversionfile == 3) &&
        ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile == 0)))
    {
      for (ob = static_cast<Object *>(bmain->objects.first); ob;
           ob = static_cast<Object *>(ob->id.next))
      {
        ListBase &list = ob->constraints;

        /* fix up constraints due to constraint recode changes (originally at 2.44.3) */
        LISTBASE_FOREACH (bConstraint *, curcon, &list) {
          /* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
          if (curcon->flag & 0x20) {
            curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
            curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
          }

          switch (curcon->type) {
            case CONSTRAINT_TYPE_LOCLIMIT: {
              bLocLimitConstraint *data = (bLocLimitConstraint *)curcon->data;

              /* old limit without parent option for objects */
              if (data->flag2) {
                curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
              }
              break;
            }
          }
        }

        /* correctly initialize constinv matrix */
        unit_m4(ob->constinv);

        if (ob->type == OB_ARMATURE) {
          if (ob->pose) {
            LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
              /* make sure constraints are all up to date */
              LISTBASE_FOREACH (bConstraint *, curcon, &pchan->constraints) {
                /* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
                if (curcon->flag & 0x20) {
                  curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
                  curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
                }

                switch (curcon->type) {
                  case CONSTRAINT_TYPE_ACTION: {
                    bActionConstraint *data = (bActionConstraint *)curcon->data;

                    /* 'data->local' used to mean that target was in local-space */
                    if (data->local) {
                      curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
                    }
                    break;
                  }
                }
              }

              /* correctly initialize constinv matrix */
              unit_m4(pchan->constinv);
            }
          }
        }
      }
    }

    /* fix all versions before 2.45 */
    if (bmain->versionfile != 245) {

      /* Repair preview from 242 - 244. */
      for (ima = static_cast<Image *>(bmain->images.first); ima;
           ima = static_cast<Image *>(ima->id.next))
      {
        ima->preview = nullptr;
      }
    }

    /* add point caches */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->soft && !ob->soft->pointcache) {
        ob->soft->pointcache = BKE_ptcache_add(&ob->soft->ptcaches);
      }

      LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
        if (psys->pointcache) {
          if (psys->pointcache->flag & PTCACHE_BAKED &&
              (psys->pointcache->flag & PTCACHE_DISK_CACHE) == 0)
          {
            printf("Old memory cache isn't supported for particles, so re-bake the simulation!\n");
            psys->pointcache->flag &= ~PTCACHE_BAKED;
          }
        }
        else {
          psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
        }
      }

      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;
          if (!clmd->point_cache) {
            clmd->point_cache = BKE_ptcache_add(&clmd->ptcaches);
            clmd->point_cache->step = 1;
          }
        }
      }
    }

    for (ma = static_cast<Material *>(bmain->materials.first); ma;
         ma = static_cast<Material *>(ma->id.next))
    {
      if (ma->gloss_mir == 0.0f) {
        ma->gloss_mir = 1.0f;
      }
    }

    for (part = static_cast<ParticleSettings *>(bmain->particles.first); part;
         part = static_cast<ParticleSettings *>(part->id.next))
    {
      if (part->child_render_percent == 0) {
        part->child_render_percent = part->child_percent;
      }
    }

    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      if (sce->nodetree) {
        ntree_version_245(fd, lib, sce->nodetree);
      }

      if (sce->r.simplify_subsurf == 0) {
        sce->r.simplify_subsurf = 6;
        sce->r.simplify_particles = 1.0f;
      }
    }

    for (ntree = static_cast<bNodeTree *>(bmain->nodetrees.first); ntree;
         ntree = static_cast<bNodeTree *>(ntree->id.next))
    {
      ntree_version_245(fd, lib, ntree);
    }

    /* fix for temporary flag changes during 245 cycle */
    for (ima = static_cast<Image *>(bmain->images.first); ima;
         ima = static_cast<Image *>(ima->id.next))
    {
      if (ima->flag & IMA_OLD_PREMUL) {
        ima->flag &= ~IMA_OLD_PREMUL;
        ima->alpha_mode = IMA_ALPHA_STRAIGHT;
      }
    }

    for (tex = static_cast<Tex *>(bmain->textures.first); tex;
         tex = static_cast<Tex *>(tex->id.next))
    {
      if (tex->iuser.flag & IMA_OLD_PREMUL) {
        tex->iuser.flag &= ~IMA_OLD_PREMUL;
      }

      ima = static_cast<Image *>(
          blo_do_versions_newlibadr(fd, &tex->id, ID_IS_LINKED(tex), tex->ima));
      if (ima && (tex->iuser.flag & IMA_DO_PREMUL)) {
        ima->flag &= ~IMA_OLD_PREMUL;
        ima->alpha_mode = IMA_ALPHA_STRAIGHT;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 2)) {
    Image *ima;

    /* initialize 1:1 Aspect */
    for (ima = static_cast<Image *>(bmain->images.first); ima;
         ima = static_cast<Image *>(ima->id.next))
    {
      ima->aspx = ima->aspy = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 4)) {
    bArmature *arm;
    Object *ob;

    for (arm = static_cast<bArmature *>(bmain->armatures.first); arm;
         arm = static_cast<bArmature *>(arm->id.next))
    {
      arm->deformflag |= ARM_DEF_B_BONE_REST;
    }

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Armature) {
          ((ArmatureModifierData *)md)->deformflag |= ARM_DEF_B_BONE_REST;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 5)) {
    /* foreground color needs to be something other than black */
    Scene *sce;
    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      sce->r.fg_stamp[0] = sce->r.fg_stamp[1] = sce->r.fg_stamp[2] = 0.8f;
      sce->r.fg_stamp[3] = 1.0f;  /* don't use text alpha yet */
      sce->r.bg_stamp[3] = 0.25f; /* make sure the background has full alpha */
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 6)) {
    Scene *sce;
    /* fix frs_sec_base */
    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      if (sce->r.frs_sec_base == 0) {
        sce->r.frs_sec_base = 1;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 7)) {
    Object *ob;

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->pose) {
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          do_version_constraints_245(&pchan->constraints);
        }
      }
      do_version_constraints_245(&ob->constraints);

      if (ob->soft && ob->soft->keys) {
        SoftBody *sb = ob->soft;
        int k;

        for (k = 0; k < sb->totkey; k++) {
          if (sb->keys[k]) {
            MEM_freeN(sb->keys[k]);
          }
        }

        MEM_freeN(sb->keys);

        sb->keys = nullptr;
        sb->totkey = 0;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 8)) {
    Scene *sce;
    Object *ob;

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->soft && ob->soft->keys) {
        SoftBody *sb = ob->soft;
        int k;

        for (k = 0; k < sb->totkey; k++) {
          if (sb->keys[k]) {
            MEM_freeN(sb->keys[k]);
          }
        }

        MEM_freeN(sb->keys);

        sb->keys = nullptr;
        sb->totkey = 0;
      }

      /* convert old particles to new system */
      PartEff *paf = BKE_object_do_version_give_parteff_245(ob);
      if (paf) {
        ParticleSystem *psys;
        ModifierData *md;
        ParticleSystemModifierData *psmd;
        ParticleSettings *part;

        /* create new particle system */
        psys = MEM_callocN<ParticleSystem>("particle_system");
        psys->pointcache = BKE_ptcache_add(&psys->ptcaches);

        /* Bad, but better not try to change this prehistorical code nowadays. */
        bmain->is_locked_for_linking = false;
        part = psys->part = BKE_particlesettings_add(bmain, "ParticleSettings");
        bmain->is_locked_for_linking = true;

        /* needed for proper libdata lookup */
        blo_do_versions_oldnewmap_insert(fd->libmap, psys->part, psys->part, 0);
        part->id.lib = ob->id.lib;

        part->id.us--;
        BLO_readfile_id_runtime_tags_for_write(part->id).needs_linking =
            BLO_readfile_id_runtime_tags(ob->id).needs_linking;

        psys->totpart = 0;
        psys->flag = PSYS_CURRENT;

        BLI_addtail(&ob->particlesystem, psys);

        md = BKE_modifier_new(eModifierType_ParticleSystem);
        SNPRINTF_UTF8(md->name, "ParticleSystem %i", BLI_listbase_count(&ob->particlesystem));
        psmd = (ParticleSystemModifierData *)md;
        psmd->psys = psys;
        BLI_addtail(&ob->modifiers, md);

        /* convert settings from old particle system */
        /* general settings */
        part->totpart = std::min(paf->totpart, 100000);
        part->sta = paf->sta;
        part->end = paf->end;
        part->lifetime = paf->lifetime;
        part->randlife = paf->randlife;
        psys->seed = paf->seed;
        part->disp = paf->disp;
        part->omat = paf->mat[0];
        part->hair_step = paf->totkey;

        part->force_group = paf->group;

        /* old system didn't interpolate between keypoints at render time */
        part->draw_step = part->ren_step = 0;

        /* physics */
        part->normfac = paf->normfac * 25.0f;
        part->obfac = paf->obfac;
        part->randfac = paf->randfac * 25.0f;
        part->dampfac = paf->damp;
        copy_v3_v3(part->acc, paf->force);

        /* flags */
        if (paf->stype & PAF_VECT) {
          if (paf->flag & PAF_STATIC) {
            /* new hair lifetime is always 100.0f */
            float fac = paf->lifetime / 100.0f;

            part->draw_as = PART_DRAW_PATH;
            part->type = PART_HAIR;
            psys->recalc |= ID_RECALC_PSYS_REDO;

            part->normfac *= fac;
            part->randfac *= fac;
          }
          else {
            part->draw_as = PART_DRAW_LINE;
            part->draw |= PART_DRAW_VEL_LENGTH;
            part->draw_line[1] = 0.04f;
          }
        }

        part->rotmode = PART_ROT_VEL;

        part->flag |= (paf->flag & PAF_BSPLINE) ? PART_HAIR_BSPLINE : 0;
        part->flag |= (paf->flag & PAF_TRAND) ? PART_TRAND : 0;
        part->flag |= (paf->flag & PAF_EDISTR) ? PART_EDISTR : 0;
        part->flag |= (paf->flag & PAF_UNBORN) ? PART_UNBORN : 0;
        part->flag |= (paf->flag & PAF_DIED) ? PART_DIED : 0;
        part->from |= (paf->flag & PAF_FACE) ? PART_FROM_FACE : 0;
        part->draw |= (paf->flag & PAF_SHOWE) ? PART_DRAW_EMITTER : 0;

        psys->vgroup[PSYS_VG_DENSITY] = paf->vertgroup;
        psys->vgroup[PSYS_VG_VEL] = paf->vertgroup_v;
        psys->vgroup[PSYS_VG_LENGTH] = paf->vertgroup_v;

        /* Dupli-objects. */
        if (ob->transflag & OB_DUPLIVERTS) {
          Object *dup = static_cast<Object *>(bmain->objects.first);

          for (; dup; dup = static_cast<Object *>(dup->id.next)) {
            if (ob == blo_do_versions_newlibadr(fd, &dup->id, ID_IS_LINKED(dup), dup->parent)) {
              part->instance_object = dup;
              ob->transflag |= OB_DUPLIPARTS;
              ob->transflag &= ~OB_DUPLIVERTS;

              part->draw_as = PART_DRAW_OB;

              /* needed for proper libdata lookup */
              blo_do_versions_oldnewmap_insert(fd->libmap, dup, dup, 0);
            }
          }
        }

        {
          FluidsimModifierData *fluidmd = (FluidsimModifierData *)BKE_modifiers_findby_type(
              ob, eModifierType_Fluidsim);
          if (fluidmd && fluidmd->fss && fluidmd->fss->type == OB_FLUIDSIM_PARTICLE) {
            part->type = PART_FLUID;
          }
        }

        do_version_free_effects_245(&ob->effect);

        printf("Old particle system converted to new system.\n");
      }
    }

    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      ParticleEditSettings *pset = &sce->toolsettings->particle;
      int a;

      if (pset->brush[0].size == 0) {
        pset->flag = PE_KEEP_LENGTHS | PE_LOCK_FIRST | PE_DEFLECT_EMITTER;
        pset->emitterdist = 0.25f;
        pset->totrekey = 5;
        pset->totaddkey = 5;

        for (a = 0; a < ARRAY_SIZE(pset->brush); a++) {
          pset->brush[a].strength = 50;
          pset->brush[a].size = 50;
          pset->brush[a].step = 10;
        }

        pset->brush[PE_BRUSH_CUT].strength = 100;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 10)) {
    Object *ob;

    /* dupliface scale */
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      ob->instance_faces_scale = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 14)) {
    Scene *sce;

    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      if (sce->ed) {
        blender::seq::foreach_strip(&sce->ed->seqbase, strip_set_blend_mode_cb, nullptr);
      }
    }
  }

  /* fix broken group lengths in id properties */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 245, 15)) {
    idproperties_fix_group_lengths(bmain->scenes);
    idproperties_fix_group_lengths(bmain->libraries);
    idproperties_fix_group_lengths(bmain->objects);
    idproperties_fix_group_lengths(bmain->meshes);
    idproperties_fix_group_lengths(bmain->curves);
    idproperties_fix_group_lengths(bmain->metaballs);
    idproperties_fix_group_lengths(bmain->materials);
    idproperties_fix_group_lengths(bmain->textures);
    idproperties_fix_group_lengths(bmain->images);
    idproperties_fix_group_lengths(bmain->lattices);
    idproperties_fix_group_lengths(bmain->lights);
    idproperties_fix_group_lengths(bmain->cameras);
    idproperties_fix_group_lengths(bmain->shapekeys);
    idproperties_fix_group_lengths(bmain->worlds);
    idproperties_fix_group_lengths(bmain->screens);
    idproperties_fix_group_lengths(bmain->fonts);
    idproperties_fix_group_lengths(bmain->texts);
    idproperties_fix_group_lengths(bmain->sounds);
    idproperties_fix_group_lengths(bmain->collections);
    idproperties_fix_group_lengths(bmain->armatures);
    idproperties_fix_group_lengths(bmain->actions);
    idproperties_fix_group_lengths(bmain->nodetrees);
    idproperties_fix_group_lengths(bmain->brushes);
    idproperties_fix_group_lengths(bmain->particles);
  }

  /* convert fluids to modifier */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 246, 1)) {
    Object *ob;

    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->fluidsimSettings) {
        FluidsimModifierData *fluidmd = (FluidsimModifierData *)BKE_modifier_new(
            eModifierType_Fluidsim);
        BLI_addhead(&ob->modifiers, (ModifierData *)fluidmd);

        MEM_freeN(fluidmd->fss);
        fluidmd->fss = static_cast<FluidsimSettings *>(MEM_dupallocN(ob->fluidsimSettings));
        MEM_freeN(ob->fluidsimSettings);

        fluidmd->fss->lastgoodframe = INT_MAX;
        fluidmd->fss->flag = 0;
        fluidmd->fss->meshVelocities = nullptr;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 246, 1)) {
    Object *ob;
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->pd && (ob->pd->forcefield == PFIELD_WIND)) {
        ob->pd->f_noise = 0.0f;
      }
    }
  }

  /* set the curve radius interpolation to 2.47 default - easy */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 247, 6)) {
    Curve *cu;

    for (cu = static_cast<Curve *>(bmain->curves.first); cu;
         cu = static_cast<Curve *>(cu->id.next))
    {
      LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
        nu->radius_interp = 3;

        /* resolu and resolv are now used differently for surfaces
         * rather than using the resolution to define the entire number of divisions,
         * use it for the number of divisions per segment
         */
        if (nu->pntsv > 1) {
          nu->resolu = std::max(1, int((float(nu->resolu) / float(nu->pntsu)) + 0.5f));
          nu->resolv = std::max(1, int((float(nu->resolv) / float(nu->pntsv)) + 0.5f));
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 248, 2)) {
    Scene *sce;

    /* NOTE: these will need to be added for painting. */
    for (sce = static_cast<Scene *>(bmain->scenes.first); sce;
         sce = static_cast<Scene *>(sce->id.next))
    {
      sce->toolsettings->imapaint.seam_bleed = 2;
      sce->toolsettings->imapaint.normal_angle = 80;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 248, 3)) {
    bScreen *screen;

    /* adjust default settings for Animation Editors */
    for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
         screen = static_cast<bScreen *>(screen->id.next))
    {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_ACTION: {
              SpaceAction *sact = (SpaceAction *)sl;

              sact->mode = SACTCONT_DOPESHEET;
              sact->autosnap = SACTSNAP_FRAME;
              break;
            }
            case SPACE_GRAPH: {
              SpaceGraph *sipo = (SpaceGraph *)sl;
              sipo->autosnap = SACTSNAP_FRAME;
              break;
            }
            case SPACE_NLA: {
              SpaceNla *snla = (SpaceNla *)sl;
              snla->autosnap = SACTSNAP_FRAME;
              break;
            }
          }
        }
      }
    }
  }

  /* correct introduce of seed for wind force */
  if (bmain->versionfile < 249 && bmain->subversionfile < 1) {
    Object *ob;
    for (ob = static_cast<Object *>(bmain->objects.first); ob;
         ob = static_cast<Object *>(ob->id.next))
    {
      if (ob->pd) {
        ob->pd->seed = (uint(ceil(BLI_time_now_seconds())) + 1) % 128;
      }
    }
  }

  if (bmain->versionfile < 249 && bmain->subversionfile < 2) {
    Scene *sce = static_cast<Scene *>(bmain->scenes.first);
    Editing *ed;

    while (sce) {
      ed = sce->ed;
      if (ed) {
        LISTBASE_FOREACH (Strip *, strip, blender::seq::active_seqbase_get(ed)) {
          if (strip->data && strip->data->proxy) {
            strip->data->proxy->quality = 90;
          }
        }
      }

      sce = static_cast<Scene *>(sce->id.next);
    }
  }
}
