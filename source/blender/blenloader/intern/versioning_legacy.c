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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup blenloader
 */

#include <limits.h>

#ifndef WIN32
#  include <unistd.h>  // for read close
#else
#  include <zlib.h> /* odd include order-issue */
#  include <io.h>   // for open close read
#  include "winsock2.h"
#  include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_effect_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_main.h"  // for Main
#include "BKE_mesh.h"  // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"

#include "NOD_socket.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "PIL_time.h"

#include <errno.h>

static void vcol_to_fcol(Mesh *me)
{
  MFace *mface;
  uint *mcol, *mcoln, *mcolmain;
  int a;

  if (me->totface == 0 || me->mcol == NULL) {
    return;
  }

  mcoln = mcolmain = MEM_malloc_arrayN(me->totface, 4 * sizeof(int), "mcoln");
  mcol = (uint *)me->mcol;
  mface = me->mface;
  for (a = me->totface; a > 0; a--, mface++) {
    mcoln[0] = mcol[mface->v1];
    mcoln[1] = mcol[mface->v2];
    mcoln[2] = mcol[mface->v3];
    mcoln[3] = mcol[mface->v4];
    mcoln += 4;
  }

  MEM_freeN(me->mcol);
  me->mcol = (MCol *)mcolmain;
}

static void do_version_bone_head_tail_237(Bone *bone)
{
  Bone *child;
  float vec[3];

  /* head */
  copy_v3_v3(bone->arm_head, bone->arm_mat[3]);

  /* tail is in current local coord system */
  copy_v3_v3(vec, bone->arm_mat[1]);
  mul_v3_fl(vec, bone->length);
  add_v3_v3v3(bone->arm_tail, bone->arm_head, vec);

  for (child = bone->childbase.first; child; child = child->next) {
    do_version_bone_head_tail_237(child);
  }
}

static void bone_version_238(ListBase *lb)
{
  Bone *bone;

  for (bone = lb->first; bone; bone = bone->next) {
    if (bone->rad_tail == 0.0f && bone->rad_head == 0.0f) {
      bone->rad_head = 0.25f * bone->length;
      bone->rad_tail = 0.1f * bone->length;

      bone->dist -= bone->rad_head;
      if (bone->dist <= 0.0f) {
        bone->dist = 0.0f;
      }
    }
    bone_version_238(&bone->childbase);
  }
}

static void bone_version_239(ListBase *lb)
{
  Bone *bone;

  for (bone = lb->first; bone; bone = bone->next) {
    if (bone->layer == 0) {
      bone->layer = 1;
    }
    bone_version_239(&bone->childbase);
  }
}

static void ntree_version_241(bNodeTree *ntree)
{
  bNode *node;

  if (ntree->type == NTREE_COMPOSIT) {
    for (node = ntree->nodes.first; node; node = node->next) {
      if (node->type == CMP_NODE_BLUR) {
        if (node->storage == NULL) {
          NodeBlurData *nbd = MEM_callocN(sizeof(NodeBlurData), "node blur patch");
          nbd->sizex = node->custom1;
          nbd->sizey = node->custom2;
          nbd->filtertype = R_FILTER_QUAD;
          node->storage = nbd;
        }
      }
      else if (node->type == CMP_NODE_VECBLUR) {
        if (node->storage == NULL) {
          NodeBlurData *nbd = MEM_callocN(sizeof(NodeBlurData), "node blur patch");
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
  bNode *node;

  if (ntree->type == NTREE_COMPOSIT) {
    for (node = ntree->nodes.first; node; node = node->next) {
      if (node->type == CMP_NODE_HUE_SAT) {
        if (node->storage) {
          NodeHueSat *nhs = node->storage;
          if (nhs->val == 0.0f) {
            nhs->val = 1.0f;
          }
        }
      }
    }
  }
}

static void ntree_version_245(FileData *fd, Library *lib, bNodeTree *ntree)
{
  bNode *node;
  NodeTwoFloats *ntf;
  ID *nodeid;
  Image *image;
  ImageUser *iuser;

  if (ntree->type == NTREE_COMPOSIT) {
    for (node = ntree->nodes.first; node; node = node->next) {
      if (node->type == CMP_NODE_ALPHAOVER) {
        if (!node->storage) {
          ntf = MEM_callocN(sizeof(NodeTwoFloats), "NodeTwoFloats");
          node->storage = ntf;
          if (node->custom1) {
            ntf->x = 1.0f;
          }
        }
      }

      /* fix for temporary flag changes during 245 cycle */
      nodeid = blo_do_versions_newlibadr(fd, lib, node->id);
      if (node->storage && nodeid && GS(nodeid->name) == ID_IM) {
        image = (Image *)nodeid;
        iuser = node->storage;
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

  for (loop = prop->data.group.first, i = 0; loop; loop = loop->next, i++) {
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

  for (id = idlist.first; id; id = id->next) {
    if (id->properties) {
      idproperties_fix_groups_lengths_recurse(id->properties);
    }
  }
}

static void customdata_version_242(Mesh *me)
{
  CustomDataLayer *layer;
  MTFace *mtf;
  MCol *mcol;
  TFace *tf;
  int a, mtfacen, mcoln;

  if (!me->vdata.totlayer) {
    CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, me->mvert, me->totvert);

    if (me->dvert) {
      CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_ASSIGN, me->dvert, me->totvert);
    }
  }

  if (!me->edata.totlayer) {
    CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, me->medge, me->totedge);
  }

  if (!me->fdata.totlayer) {
    CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, me->mface, me->totface);

    if (me->tface) {
      if (me->mcol) {
        MEM_freeN(me->mcol);
      }

      me->mcol = CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
      me->mtface = CustomData_add_layer(&me->fdata, CD_MTFACE, CD_CALLOC, NULL, me->totface);

      mtf = me->mtface;
      mcol = me->mcol;
      tf = me->tface;

      for (a = 0; a < me->totface; a++, mtf++, tf++, mcol += 4) {
        memcpy(mcol, tf->col, sizeof(tf->col));
        memcpy(mtf->uv, tf->uv, sizeof(tf->uv));
      }

      MEM_freeN(me->tface);
      me->tface = NULL;
    }
    else if (me->mcol) {
      me->mcol = CustomData_add_layer(&me->fdata, CD_MCOL, CD_ASSIGN, me->mcol, me->totface);
    }
  }

  if (me->tface) {
    MEM_freeN(me->tface);
    me->tface = NULL;
  }

  for (a = 0, mtfacen = 0, mcoln = 0; a < me->fdata.totlayer; a++) {
    layer = &me->fdata.layers[a];

    if (layer->type == CD_MTFACE) {
      if (layer->name[0] == 0) {
        if (mtfacen == 0) {
          strcpy(layer->name, "UVMap");
        }
        else {
          BLI_snprintf(layer->name, sizeof(layer->name), "UVMap.%.3d", mtfacen);
        }
      }
      mtfacen++;
    }
    else if (layer->type == CD_MCOL) {
      if (layer->name[0] == 0) {
        if (mcoln == 0) {
          strcpy(layer->name, "Col");
        }
        else {
          BLI_snprintf(layer->name, sizeof(layer->name), "Col.%.3d", mcoln);
        }
      }
      mcoln++;
    }
  }

  BKE_mesh_update_customdata_pointers(me, true);
}

/*only copy render texface layer from active*/
static void customdata_version_243(Mesh *me)
{
  CustomDataLayer *layer;
  int a;

  for (a = 0; a < me->fdata.totlayer; a++) {
    layer = &me->fdata.layers[a];
    layer->active_rnd = layer->active;
  }
}

/* struct NodeImageAnim moved to ImageUser, and we make it default available */
static void do_version_ntree_242_2(bNodeTree *ntree)
{
  bNode *node;

  if (ntree->type == NTREE_COMPOSIT) {
    for (node = ntree->nodes.first; node; node = node->next) {
      if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        /* only image had storage */
        if (node->storage) {
          NodeImageAnim *nia = node->storage;
          ImageUser *iuser = MEM_callocN(sizeof(ImageUser), "ima user node");

          iuser->frames = nia->frames;
          iuser->sfra = nia->sfra;
          iuser->offset = nia->nr - 1;
          iuser->cycl = nia->cyclic;
          iuser->ok = 1;

          node->storage = iuser;
          MEM_freeN(nia);
        }
        else {
          ImageUser *iuser = node->storage = MEM_callocN(sizeof(ImageUser), "node image user");
          iuser->sfra = 1;
          iuser->ok = 1;
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
  Effect *eff;

  while ((eff = BLI_pophead(lb))) {
    do_version_free_effect_245(eff);
  }
}

static void do_version_constraints_245(ListBase *lb)
{
  bConstraint *con;
  bConstraintTarget *ct;

  for (con = lb->first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_PYTHON) {
      bPythonConstraint *data = (bPythonConstraint *)con->data;
      if (data->tar) {
        /* version patching needs to be done */
        ct = MEM_callocN(sizeof(bConstraintTarget), "PyConTarget");

        ct->tar = data->tar;
        BLI_strncpy(ct->subtarget, data->subtarget, sizeof(ct->subtarget));
        ct->space = con->tarspace;

        BLI_addtail(&data->targets, ct);
        data->tarnum++;

        /* clear old targets to avoid problems */
        data->tar = NULL;
        data->subtarget[0] = '\0';
      }
    }
    else if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
      bLocateLikeConstraint *data = (bLocateLikeConstraint *)con->data;

      /* new headtail functionality makes Bone-Tip function obsolete */
      if (data->flag & LOCLIKE_TIP) {
        con->headtail = 1.0f;
      }
    }
  }
}

PartEff *blo_do_version_give_parteff_245(Object *ob)
{
  PartEff *paf;

  paf = ob->effect.first;
  while (paf) {
    if (paf->type == EFF_PARTICLE) {
      return paf;
    }
    paf = paf->next;
  }
  return NULL;
}

/* NOTE: this version patch is intended for versions < 2.52.2,
 * but was initially introduced in 2.27 already. */
void blo_do_version_old_trackto_to_constraints(Object *ob)
{
  /* create new trackto constraint from the relationship */
  if (ob->track) {
    bConstraint *con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);
    bTrackToConstraint *data = con->data;

    /* copy tracking settings from the object */
    data->tar = ob->track;
    data->reserved1 = ob->trackflag;
    data->reserved2 = ob->upflag;
  }

  /* clear old track setting */
  ob->track = NULL;
}

void blo_do_versions_pre250(FileData *fd, Library *lib, Main *bmain)
{
  /* WATCH IT!!!: pointers from libdata have not been converted */

  if (bmain->versionfile == 100) {
    /* tex->extend and tex->imageflag have changed: */
    Tex *tex = bmain->textures.first;
    while (tex) {
      if (tex->id.tag & LIB_TAG_NEED_LINK) {

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
      tex = tex->id.next;
    }
  }

  if (bmain->versionfile <= 101) {
    /* frame mapping */
    Scene *sce = bmain->scenes.first;
    while (sce) {
      sce->r.framapto = 100;
      sce->r.images = 100;
      sce->r.framelen = 1.0;
      sce = sce->id.next;
    }
  }

  if (bmain->versionfile <= 103) {
    /* new variable in object: colbits */
    Object *ob = bmain->objects.first;
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
      ob = ob->id.next;
    }
  }

  if (bmain->versionfile <= 104) {
    /* timeoffs moved */
    Object *ob = bmain->objects.first;
    while (ob) {
      if (ob->transflag & 1) {
        ob->transflag -= 1;
      }
      ob = ob->id.next;
    }
  }

  if (bmain->versionfile <= 106) {
    /* mcol changed */
    Mesh *me = bmain->meshes.first;
    while (me) {
      if (me->mcol) {
        vcol_to_fcol(me);
      }
      me = me->id.next;
    }
  }

  if (bmain->versionfile <= 107) {
    Object *ob;
    ob = bmain->objects.first;
    while (ob) {
      if (ob->dt == 0) {
        ob->dt = OB_SOLID;
      }
      ob = ob->id.next;
    }
  }

  if (bmain->versionfile <= 109) {
    /* new variable: gridlines */
    bScreen *sc = bmain->screens.first;
    while (sc) {
      ScrArea *sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl = sa->spacedata.first;
        while (sl) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;

            if (v3d->gridlines == 0) {
              v3d->gridlines = 20;
            }
          }
          sl = sl->next;
        }
        sa = sa->next;
      }
      sc = sc->id.next;
    }
  }

  if (bmain->versionfile <= 134) {
    Tex *tex = bmain->textures.first;
    while (tex) {
      if ((tex->rfac == 0.0f) && (tex->gfac == 0.0f) && (tex->bfac == 0.0f)) {
        tex->rfac = 1.0f;
        tex->gfac = 1.0f;
        tex->bfac = 1.0f;
        tex->filtersize = 1.0f;
      }
      tex = tex->id.next;
    }
  }

  if (bmain->versionfile <= 140) {
    /* r-g-b-fac in texture */
    Tex *tex = bmain->textures.first;
    while (tex) {
      if ((tex->rfac == 0.0f) && (tex->gfac == 0.0f) && (tex->bfac == 0.0f)) {
        tex->rfac = 1.0f;
        tex->gfac = 1.0f;
        tex->bfac = 1.0f;
        tex->filtersize = 1.0f;
      }
      tex = tex->id.next;
    }
  }

  if (bmain->versionfile <= 153) {
    Scene *sce = bmain->scenes.first;
    while (sce) {
      if (sce->r.blurfac == 0.0f) {
        sce->r.blurfac = 1.0f;
      }
      sce = sce->id.next;
    }
  }

  if (bmain->versionfile <= 163) {
    Scene *sce = bmain->scenes.first;
    while (sce) {
      if (sce->r.frs_sec == 0) {
        sce->r.frs_sec = 25;
      }
      sce = sce->id.next;
    }
  }

  if (bmain->versionfile <= 164) {
    Mesh *me = bmain->meshes.first;
    while (me) {
      me->smoothresh = 30;
      me = me->id.next;
    }
  }

  if (bmain->versionfile <= 165) {
    Mesh *me = bmain->meshes.first;
    TFace *tface;
    int nr;
    char *cp;

    while (me) {
      if (me->tface) {
        nr = me->totface;
        tface = me->tface;
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
      me = me->id.next;
    }
  }

  if (bmain->versionfile <= 169) {
    Mesh *me = bmain->meshes.first;
    while (me) {
      if (me->subdiv == 0) {
        me->subdiv = 1;
      }
      me = me->id.next;
    }
  }

  if (bmain->versionfile <= 169) {
    bScreen *sc = bmain->screens.first;
    while (sc) {
      ScrArea *sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl = sa->spacedata.first;
        while (sl) {
          if (sl->spacetype == SPACE_GRAPH) {
            SpaceGraph *sipo = (SpaceGraph *)sl;
            sipo->v2d.max[0] = 15000.0;
          }
          sl = sl->next;
        }
        sa = sa->next;
      }
      sc = sc->id.next;
    }
  }

  if (bmain->versionfile <= 170) {
    Object *ob = bmain->objects.first;
    PartEff *paf;
    while (ob) {
      paf = blo_do_version_give_parteff_245(ob);
      if (paf) {
        if (paf->staticstep == 0) {
          paf->staticstep = 5;
        }
      }
      ob = ob->id.next;
    }
  }

  if (bmain->versionfile <= 171) {
    bScreen *sc = bmain->screens.first;
    while (sc) {
      ScrArea *sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl = sa->spacedata.first;
        while (sl) {
          if (sl->spacetype == SPACE_TEXT) {
            SpaceText *st = (SpaceText *)sl;
            st->lheight = 12;
          }
          sl = sl->next;
        }
        sa = sa->next;
      }
      sc = sc->id.next;
    }
  }

  if (bmain->versionfile <= 173) {
    int a, b;
    Mesh *me = bmain->meshes.first;
    while (me) {
      if (me->tface) {
        TFace *tface = me->tface;
        for (a = 0; a < me->totface; a++, tface++) {
          for (b = 0; b < 4; b++) {
            tface->uv[b][0] /= 32767.0f;
            tface->uv[b][1] /= 32767.0f;
          }
        }
      }
      me = me->id.next;
    }
  }

  if (bmain->versionfile <= 204) {
    bSound *sound;

    sound = bmain->sounds.first;
    while (sound) {
      if (sound->volume < 0.01f) {
        sound->volume = 1.0f;
      }
      sound = sound->id.next;
    }
  }

  if (bmain->versionfile <= 212) {
    bSound *sound;
    Mesh *me;

    sound = bmain->sounds.first;
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

      sound = sound->id.next;
    }

    /* me->subdiv changed to reflect the actual reparametization
     * better, and smeshes were removed - if it was a smesh make
     * it a subsurf, and reset the subdiv level because subsurf
     * takes a lot more work to calculate.
     */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      enum {
        ME_SMESH = (1 << 6),
        ME_SUBSURF = (1 << 7),
      };
      if (me->flag & ME_SMESH) {
        me->flag &= ~ME_SMESH;
        me->flag |= ME_SUBSURF;

        me->subdiv = 1;
      }
      else {
        if (me->subdiv < 2) {
          me->subdiv = 1;
        }
        else {
          me->subdiv--;
        }
      }
    }
  }

  if (bmain->versionfile <= 220) {
    Mesh *me;

    /* Began using alpha component of vertex colors, but
     * old file vertex colors are undefined, reset them
     * to be fully opaque. -zr
     */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      if (me->mcol) {
        int i;

        for (i = 0; i < me->totface * 4; i++) {
          MCol *mcol = &me->mcol[i];
          mcol->a = 255;
        }
      }
      if (me->tface) {
        int i, j;

        for (i = 0; i < me->totface; i++) {
          TFace *tf = &((TFace *)me->tface)[i];

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
    for (vf = bmain->fonts.first; vf; vf = vf->id.next) {
      if (STREQ(vf->name + strlen(vf->name) - 6, ".Bfont")) {
        strcpy(vf->name, FO_BUILTIN_NAME);
      }
    }
  }

  if (bmain->versionfile <= 224) {
    bSound *sound;
    Scene *sce;
    Mesh *me;
    bScreen *sc;

    for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
      if (sound->packedfile) {
        if (sound->newpackedfile == NULL) {
          sound->newpackedfile = sound->packedfile;
        }
        sound->packedfile = NULL;
      }
    }
    /* Make sure that old subsurf meshes don't have zero subdivision level for rendering */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      enum { ME_SUBSURF = (1 << 7) };
      if ((me->flag & ME_SUBSURF) && (me->subdivr == 0)) {
        me->subdivr = me->subdiv;
      }
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      sce->r.stereomode = 1;  // no stereo
    }

    /* some oldfile patch, moved from set_func_space */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    bScreen *sc;
    Object *ob;

    /* As of now, this insures that the transition from the old Track system
     * to the new full constraint Track is painless for everyone. - theeth
     */
    ob = bmain->objects.first;

    while (ob) {
      ListBase *list;
      list = &ob->constraints;

      /* check for already existing TrackTo constraint
       * set their track and up flag correctly
       */

      if (list) {
        bConstraint *curcon;
        for (curcon = list->first; curcon; curcon = curcon->next) {
          if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
            bTrackToConstraint *data = curcon->data;
            data->reserved1 = ob->trackflag;
            data->reserved2 = ob->upflag;
          }
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          bConstraint *curcon;
          bPoseChannel *pchan;
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            for (curcon = pchan->constraints.first; curcon; curcon = curcon->next) {
              if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
                bTrackToConstraint *data = curcon->data;
                data->reserved1 = ob->trackflag;
                data->reserved2 = ob->upflag;
              }
            }
          }
        }
      }

      /* Change Ob->Track in real TrackTo constraint */
      blo_do_version_old_trackto_to_constraints(ob);

      ob = ob->id.next;
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      sce->audio.mixrate = 48000;
      sce->audio.flag |= AUDIO_SCRUB;
    }

    /* patch for old wrong max view2d settings, allows zooming out more */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    bScreen *sc;
    Object *ob;

    /* As of now, this insures that the transition from the old Track system
     * to the new full constraint Track is painless for everyone.
     */
    ob = bmain->objects.first;

    while (ob) {
      ListBase *list;
      list = &ob->constraints;

      /* check for already existing TrackTo constraint
       * set their track and up flag correctly */

      if (list) {
        bConstraint *curcon;
        for (curcon = list->first; curcon; curcon = curcon->next) {
          if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
            bTrackToConstraint *data = curcon->data;
            data->reserved1 = ob->trackflag;
            data->reserved2 = ob->upflag;
          }
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          bConstraint *curcon;
          bPoseChannel *pchan;
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            for (curcon = pchan->constraints.first; curcon; curcon = curcon->next) {
              if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
                bTrackToConstraint *data = curcon->data;
                data->reserved1 = ob->trackflag;
                data->reserved2 = ob->upflag;
              }
            }
          }
        }
      }

      ob = ob->id.next;
    }

    /* convert old mainb values for new button panels */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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

  /* ton: made this 230 instead of 229,
   * to be sure (tuho files) and this is a reliable check anyway
   * nevertheless, we might need to think over a fitness (initialize)
   * check apart from the do_versions()
   */

  if (bmain->versionfile <= 230) {
    bScreen *sc;

    /* new variable blockscale, for panels in any area */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    bScreen *sc = bmain->screens.first;

    /* new bit flags for showing/hiding grid floor and axes */

    while (sc) {
      ScrArea *sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl = sa->spacedata.first;
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
        sa = sa->next;
      }
      sc = sc->id.next;
    }
  }

  if (bmain->versionfile <= 232) {
    Tex *tex = bmain->textures.first;
    World *wrld = bmain->worlds.first;
    bScreen *sc;

    while (tex) {
      if ((tex->flag & (TEX_CHECKER_ODD + TEX_CHECKER_EVEN)) == 0) {
        tex->flag |= TEX_CHECKER_ODD;
      }
      /* copied from kernel texture.c */
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
      tex = tex->id.next;
    }

    while (wrld) {
      if (wrld->aodist == 0.0f) {
        wrld->aodist = 10.0f;
      }
      if (wrld->aoenergy == 0.0f) {
        wrld->aoenergy = 1.0f;
      }
      wrld = wrld->id.next;
    }

    /* new variable blockscale, for panels in any area, do again because new
     * areas didnt initialize it to 0.7 yet
     */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    bScreen *sc;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->flag |= V3D_SELECT_OUTLINE;
          }
        }
      }
    }
  }

  if (bmain->versionfile <= 234) {
    bScreen *sc;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    Tex *tex = bmain->textures.first;
    Scene *sce = bmain->scenes.first;
    Sequence *seq;
    Editing *ed;

    while (tex) {
      if (tex->nabla == 0.0f) {
        tex->nabla = 0.025f;
      }
      tex = tex->id.next;
    }
    while (sce) {
      ed = sce->ed;
      if (ed) {
        SEQ_BEGIN (sce->ed, seq) {
          if (seq->type == SEQ_TYPE_IMAGE || seq->type == SEQ_TYPE_MOVIE) {
            seq->alpha_mode = SEQ_ALPHA_STRAIGHT;
          }
        }
        SEQ_END;
      }

      sce = sce->id.next;
    }
  }

  if (bmain->versionfile <= 236) {
    Object *ob;
    Camera *cam = bmain->cameras.first;

    while (cam) {
      if (cam->ortho_scale == 0.0f) {
        cam->ortho_scale = 256.0f / cam->lens;
        if (cam->type == CAM_ORTHO) {
          printf("NOTE: ortho render has changed, tweak new Camera 'scale' value.\n");
        }
      }
      cam = cam->id.next;
    }
    /* force oops draw if depgraph was set*/
    /* set time line var */

    /* softbody init new vars */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->soft) {
        if (ob->soft->defgoal == 0.0f) {
          ob->soft->defgoal = 0.7f;
        }
        if (ob->soft->physics_speed == 0.0f) {
          ob->soft->physics_speed = 1.0f;
        }

        if (ob->soft->interval == 0) {
          ob->soft->interval = 2;
          ob->soft->sfra = 1;
          ob->soft->efra = 100;
        }
      }
      if (ob->soft && ob->soft->vertgroup == 0) {
        bDeformGroup *locGroup = defgroup_find_name(ob, "SOFTGOAL");
        if (locGroup) {
          /* retrieve index for that group */
          ob->soft->vertgroup = 1 + BLI_findindex(&ob->defbase, locGroup);
        }
      }
    }
  }

  if (bmain->versionfile <= 237) {
    bArmature *arm;
    bConstraint *con;
    Object *ob;
    Bone *bone;

    /* armature recode checks */
    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      BKE_armature_where_is(arm);

      for (bone = arm->bonebase.first; bone; bone = bone->next) {
        do_version_bone_head_tail_237(bone);
      }
    }
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->parent) {
        Object *parent = blo_do_versions_newlibadr(fd, lib, ob->parent);
        if (parent && parent->type == OB_LATTICE) {
          ob->partype = PARSKEL;
        }
      }

      /* btw. armature_rebuild_pose is further only called on leave editmode */
      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          BKE_pose_tag_recalc(bmain, ob->pose);
        }

        /* cannot call stuff now (pointers!), done in setup_app_data */
        ob->id.recalc |= ID_RECALC_ALL;

        /* new generic xray option */
        arm = blo_do_versions_newlibadr(fd, lib, ob->data);
        enum { ARM_DRAWXRAY = (1 << 1) };
        if (arm->flag & ARM_DRAWXRAY) {
          ob->dtx |= OB_DRAWXRAY;
        }
      }
      else if (ob->type == OB_MESH) {
        Mesh *me = blo_do_versions_newlibadr(fd, lib, ob->data);

        enum {
          ME_SUBSURF = (1 << 7),
          ME_OPT_EDGES = (1 << 8),
        };

        if ((me->flag & ME_SUBSURF)) {
          SubsurfModifierData *smd = (SubsurfModifierData *)modifier_new(eModifierType_Subsurf);

          smd->levels = MAX2(1, me->subdiv);
          smd->renderLevels = MAX2(1, me->subdivr);
          smd->subdivType = me->subsurftype;

          smd->modifier.mode = 0;
          if (me->subdiv != 0) {
            smd->modifier.mode |= 1;
          }
          if (me->subdivr != 0) {
            smd->modifier.mode |= 2;
          }

          if (me->flag & ME_OPT_EDGES) {
            smd->flags |= eSubsurfModifierFlag_ControlEdges;
          }

          BLI_addtail(&ob->modifiers, smd);

          modifier_unique_name(&ob->modifiers, (ModifierData *)smd);
        }
      }

      /* follow path constraint needs to set the 'path' option in curves... */
      for (con = ob->constraints.first; con; con = con->next) {
        if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
          bFollowPathConstraint *data = con->data;
          Object *obc = blo_do_versions_newlibadr(fd, lib, data->tar);

          if (obc && obc->type == OB_CURVE) {
            Curve *cu = blo_do_versions_newlibadr(fd, lib, obc->data);
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
    Mesh *me;
    Key *key;
    Scene *sce = bmain->scenes.first;

    while (sce) {
      if (sce->toolsettings == NULL) {
        sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings), "Tool Settings Struct");
        sce->toolsettings->doublimit = 0.001f;
      }
      sce = sce->id.next;
    }

    for (lt = bmain->lattices.first; lt; lt = lt->id.next) {
      if (lt->fu == 0.0f && lt->fv == 0.0f && lt->fw == 0.0f) {
        calc_lat_fudu(lt->flag, lt->pntsu, &lt->fu, &lt->du);
        calc_lat_fudu(lt->flag, lt->pntsv, &lt->fv, &lt->dv);
        calc_lat_fudu(lt->flag, lt->pntsw, &lt->fw, &lt->dw);
      }
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      PartEff *paf;

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Subsurf) {
          SubsurfModifierData *smd = (SubsurfModifierData *)md;

          smd->flags &= ~(eSubsurfModifierFlag_Incremental | eSubsurfModifierFlag_DebugIncr);
        }
      }

      if ((ob->softflag & OB_SB_ENABLE) && !modifiers_findByType(ob, eModifierType_Softbody)) {
        if (ob->softflag & OB_SB_POSTDEF) {
          md = ob->modifiers.first;

          while (md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform) {
            md = md->next;
          }

          BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(eModifierType_Softbody));
        }
        else {
          BLI_addhead(&ob->modifiers, modifier_new(eModifierType_Softbody));
        }

        ob->softflag &= ~OB_SB_ENABLE;
      }

      if (ob->pose) {
        bPoseChannel *pchan;
        bConstraint *con;
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          /* note, pchan->bone is also lib-link stuff */
          if (pchan->limitmin[0] == 0.0f && pchan->limitmax[0] == 0.0f) {
            pchan->limitmin[0] = pchan->limitmin[1] = pchan->limitmin[2] = -180.0f;
            pchan->limitmax[0] = pchan->limitmax[1] = pchan->limitmax[2] = 180.0f;

            for (con = pchan->constraints.first; con; con = con->next) {
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

      paf = blo_do_version_give_parteff_245(ob);
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

    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      bone_version_238(&arm->bonebase);
      arm->deformflag |= ARM_DEF_VGROUP;
    }

    for (me = bmain->meshes.first; me; me = me->id.next) {
      if (!me->medge) {
        BKE_mesh_calc_edges_legacy(me, true); /* true = use mface->edcode */
      }
      else {
        BKE_mesh_strip_loose_faces(me);
      }
    }

    for (key = bmain->shapekeys.first; key; key = key->id.next) {
      KeyBlock *kb;
      int index = 1;

      for (kb = key->block.first; kb; kb = kb->next) {
        if (kb == key->refkey) {
          if (kb->name[0] == 0) {
            strcpy(kb->name, "Basis");
          }
        }
        else {
          if (kb->name[0] == 0) {
            BLI_snprintf(kb->name, sizeof(kb->name), "Key %d", index);
          }
          index++;
        }
      }
    }
  }

  if (bmain->versionfile <= 239) {
    bArmature *arm;
    Object *ob;
    Scene *sce = bmain->scenes.first;
    Camera *cam = bmain->cameras.first;
    int set_passepartout = 0;

    /* deformflag is local in modifier now */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Armature) {
          ArmatureModifierData *amd = (ArmatureModifierData *)md;
          if (amd->object && amd->deformflag == 0) {
            Object *oba = blo_do_versions_newlibadr(fd, lib, amd->object);
            arm = blo_do_versions_newlibadr(fd, lib, oba->data);
            amd->deformflag = arm->deformflag;
          }
        }
      }
    }

    /* updating stepsize for ghost drawing */
    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      bone_version_239(&arm->bonebase);
      if (arm->layer == 0) {
        arm->layer = 1;
      }
    }

    for (; sce; sce = sce->id.next) {
      if (sce->r.scemode & R_PASSEPARTOUT) {
        set_passepartout = 1;
        sce->r.scemode &= ~R_PASSEPARTOUT;
      }
    }

    for (; cam; cam = cam->id.next) {
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
    Light *la;
    bArmature *arm;
    bNodeTree *ntree;

    /* updating layers still */
    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      bone_version_239(&arm->bonebase);
      if (arm->layer == 0) {
        arm->layer = 1;
      }
    }
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->audio.mixrate == 0) {
        sce->audio.mixrate = 48000;
      }

      /* We don't add default layer since blender2.8 because the layers
       * are now in Scene->view_layers and a default layer is created in
       * the doversion later on.
       */
      SceneRenderLayer *srl;
      /* new layer flag for sky, was default for solid */
      for (srl = sce->r.layers.first; srl; srl = srl->next) {
        if (srl->layflag & SCE_LAY_SOLID) {
          srl->layflag |= SCE_LAY_SKY;
        }
        srl->passflag &= (SCE_PASS_COMBINED | SCE_PASS_Z | SCE_PASS_NORMAL | SCE_PASS_VECTOR);
      }

      /* node version changes */
      if (sce->nodetree) {
        ntree_version_241(sce->nodetree);
      }

      /* uv calculation options moved to toolsettings */
      if (sce->toolsettings->unwrapper == 0) {
        sce->toolsettings->uvcalc_flag = UVCALC_FILLHOLES;
        sce->toolsettings->unwrapper = 1;
      }
    }

    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      ntree_version_241(ntree);
    }

    for (la = bmain->lights.first; la; la = la->id.next) {
      if (la->buffers == 0) {
        la->buffers = 1;
      }
    }

    /* for empty drawsize and drawtype */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->empty_drawsize == 0.0f) {
        ob->empty_drawtype = OB_ARROWS;
        ob->empty_drawsize = 1.0;
      }
    }

    /* during 2.41 images with this name were used for viewer node output, lets fix that */
    if (bmain->versionfile == 241) {
      Image *ima;
      for (ima = bmain->images.first; ima; ima = ima->id.next) {
        if (STREQ(ima->name, "Compositor")) {
          strcpy(ima->id.name + 2, "Viewer Node");
          strcpy(ima->name, "Viewer Node");
        }
      }
    }
  }

  if (bmain->versionfile <= 242) {
    Scene *sce;
    bScreen *sc;
    Object *ob;
    Curve *cu;
    Material *ma;
    Mesh *me;
    Collection *collection;
    Nurb *nu;
    BezTriple *bezt;
    BPoint *bp;
    bNodeTree *ntree;
    int a;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            if (v3d->gridsubdiv == 0) {
              v3d->gridsubdiv = 10;
            }
          }
        }
        sa = sa->next;
      }
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
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

    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      ntree_version_242(ntree);
    }

    /* add default radius values to old curve points */
    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      for (nu = cu->nurb.first; nu; nu = nu->next) {
        if (nu) {
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
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      ListBase *list;
      list = &ob->constraints;

      /* check for already existing MinMax (floor) constraint
       * and update the sticky flagging */

      if (list) {
        bConstraint *curcon;
        for (curcon = list->first; curcon; curcon = curcon->next) {
          switch (curcon->type) {
            case CONSTRAINT_TYPE_ROTLIKE: {
              bRotateLikeConstraint *data = curcon->data;

              /* version patch from buttons_object.c */
              if (data->flag == 0) {
                data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
              }

              break;
            }
          }
        }
      }

      if (ob->type == OB_ARMATURE) {
        if (ob->pose) {
          bConstraint *curcon;
          bPoseChannel *pchan;
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            for (curcon = pchan->constraints.first; curcon; curcon = curcon->next) {
              switch (curcon->type) {
                case CONSTRAINT_TYPE_KINEMATIC: {
                  bKinematicConstraint *data = curcon->data;
                  if (!(data->flag & CONSTRAINT_IK_POS)) {
                    data->flag |= CONSTRAINT_IK_POS;
                    data->flag |= CONSTRAINT_IK_STRETCH;
                  }
                  break;
                }
                case CONSTRAINT_TYPE_ROTLIKE: {
                  bRotateLikeConstraint *data = curcon->data;

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

      /* copy old object level track settings to curve modifers */
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Curve) {
          CurveModifierData *cmd = (CurveModifierData *)md;

          if (cmd->defaxis == 0) {
            cmd->defaxis = ob->trackflag + 1;
          }
        }
      }
    }

    for (ma = bmain->materials.first; ma; ma = ma->id.next) {
      if (ma->nodetree) {
        ntree_version_242(ma->nodetree);
      }
    }

    for (me = bmain->meshes.first; me; me = me->id.next) {
      customdata_version_242(me);
    }

    for (collection = bmain->collections.first; collection; collection = collection->id.next) {
      if (collection->layer == 0) {
        collection->layer = (1 << 20) - 1;
      }
    }

    /* now, subversion control! */
    if (bmain->subversionfile < 3) {
      Image *ima;
      Tex *tex;

      /* Image refactor initialize */
      for (ima = bmain->images.first; ima; ima = ima->id.next) {
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
      for (tex = bmain->textures.first; tex; tex = tex->id.next) {
        enum {
          TEX_ANIMCYCLIC = (1 << 6),
          TEX_ANIM5 = (1 << 7),
        };

        if (tex->type == TEX_IMAGE && tex->ima) {
          ima = blo_do_versions_newlibadr(fd, lib, tex->ima);
          if (tex->imaflag & TEX_ANIM5) {
            ima->source = IMA_SRC_MOVIE;
          }
        }
        tex->iuser.frames = tex->frames;
        tex->iuser.offset = tex->offset;
        tex->iuser.sfra = tex->sfra;
        tex->iuser.cycl = (tex->imaflag & TEX_ANIMCYCLIC) != 0;
      }
      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        if (sce->nodetree) {
          do_version_ntree_242_2(sce->nodetree);
        }
      }
      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        do_version_ntree_242_2(ntree);
      }
      for (ma = bmain->materials.first; ma; ma = ma->id.next) {
        if (ma->nodetree) {
          do_version_ntree_242_2(ma->nodetree);
        }
      }
    }

    if (bmain->subversionfile < 4) {
      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        sce->r.bake_mode = 1; /* prevent to include render stuff here */
        sce->r.bake_filter = 16;
        sce->r.bake_flag = R_BAKE_CLEAR;
      }
    }
  }

  if (bmain->versionfile <= 243) {
    Object *ob = bmain->objects.first;

    for (; ob; ob = ob->id.next) {
      bDeformGroup *curdef;

      for (curdef = ob->defbase.first; curdef; curdef = curdef->next) {
        /* replace an empty-string name with unique name */
        if (curdef->name[0] == '\0') {
          defgroup_unique_name(curdef, ob);
        }
      }

      if (bmain->versionfile < 243 || bmain->subversionfile < 1) {
        ModifierData *md;

        /* translate old mirror modifier axis values to new flags */
        for (md = ob->modifiers.first; md; md = md->next) {
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
      Mesh *me;
      for (me = bmain->meshes.first; me; me = me->id.next) {
        customdata_version_243(me);
      }
    }
  }

  if (bmain->versionfile <= 244) {
    bScreen *sc;

    if (bmain->versionfile != 244 || bmain->subversionfile < 2) {
      /* correct older action editors - incorrect scrolling */
      for (sc = bmain->screens.first; sc; sc = sc->id.next) {
        ScrArea *sa;
        sa = sc->areabase.first;
        while (sa) {
          SpaceLink *sl;

          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_ACTION) {
              SpaceAction *saction = (SpaceAction *)sl;

              saction->v2d.tot.ymin = -1000.0;
              saction->v2d.tot.ymax = 0.0;

              saction->v2d.cur.ymin = -75.0;
              saction->v2d.cur.ymax = 5.0;
            }
          }
          sa = sa->next;
        }
      }
    }
  }

  if (bmain->versionfile <= 245) {
    Scene *sce;
    Object *ob;
    Image *ima;
    Light *la;
    Material *ma;
    ParticleSettings *part;
    Mesh *me;
    bNodeTree *ntree;
    Tex *tex;
    ModifierData *md;
    ParticleSystem *psys;

    /* unless the file was created 2.44.3 but not 2.45, update the constraints */
    if (!(bmain->versionfile == 244 && bmain->subversionfile == 3) &&
        ((bmain->versionfile < 245) ||
         (bmain->versionfile == 245 && bmain->subversionfile == 0))) {
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ListBase *list;
        list = &ob->constraints;

        /* fix up constraints due to constraint recode changes (originally at 2.44.3) */
        if (list) {
          bConstraint *curcon;
          for (curcon = list->first; curcon; curcon = curcon->next) {
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
        }

        /* correctly initialize constinv matrix */
        unit_m4(ob->constinv);

        if (ob->type == OB_ARMATURE) {
          if (ob->pose) {
            bConstraint *curcon;
            bPoseChannel *pchan;

            for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
              /* make sure constraints are all up to date */
              for (curcon = pchan->constraints.first; curcon; curcon = curcon->next) {
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

      /* repair preview from 242 - 244*/
      for (ima = bmain->images.first; ima; ima = ima->id.next) {
        ima->preview = NULL;
      }
    }

    /* add point caches */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->soft && !ob->soft->pointcache) {
        ob->soft->pointcache = BKE_ptcache_add(&ob->soft->ptcaches);
      }

      for (psys = ob->particlesystem.first; psys; psys = psys->next) {
        if (psys->pointcache) {
          if (psys->pointcache->flag & PTCACHE_BAKED &&
              (psys->pointcache->flag & PTCACHE_DISK_CACHE) == 0) {
            printf("Old memory cache isn't supported for particles, so re-bake the simulation!\n");
            psys->pointcache->flag &= ~PTCACHE_BAKED;
          }
        }
        else {
          psys->pointcache = BKE_ptcache_add(&psys->ptcaches);
        }
      }

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;
          if (!clmd->point_cache) {
            clmd->point_cache = BKE_ptcache_add(&clmd->ptcaches);
            clmd->point_cache->step = 1;
          }
        }
      }
    }

    /* Copy over old per-level multires vertex data
     * into a single vertex array in struct Multires */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      if (me->mr && !me->mr->verts) {
        MultiresLevel *lvl = me->mr->levels.last;
        if (lvl) {
          me->mr->verts = lvl->verts;
          lvl->verts = NULL;
          /* Don't need the other vert arrays */
          for (lvl = lvl->prev; lvl; lvl = lvl->prev) {
            MEM_freeN(lvl->verts);
            lvl->verts = NULL;
          }
        }
      }
    }

    if (bmain->versionfile != 245 || bmain->subversionfile < 1) {
      for (la = bmain->lights.first; la; la = la->id.next) {
        la->falloff_type = LA_FALLOFF_INVLINEAR;

        if (la->curfalloff == NULL) {
          la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
          curvemapping_initialize(la->curfalloff);
        }
      }
    }

    for (ma = bmain->materials.first; ma; ma = ma->id.next) {
      if (ma->gloss_mir == 0.0f) {
        ma->gloss_mir = 1.0f;
      }
    }

    for (part = bmain->particles.first; part; part = part->id.next) {
      if (part->ren_child_nbr == 0) {
        part->ren_child_nbr = part->child_nbr;
      }
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->nodetree) {
        ntree_version_245(fd, lib, sce->nodetree);
      }

      if (sce->r.simplify_subsurf == 0) {
        sce->r.simplify_subsurf = 6;
        sce->r.simplify_particles = 1.0f;
      }
    }

    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      ntree_version_245(fd, lib, ntree);
    }

    /* fix for temporary flag changes during 245 cycle */
    for (ima = bmain->images.first; ima; ima = ima->id.next) {
      if (ima->flag & IMA_OLD_PREMUL) {
        ima->flag &= ~IMA_OLD_PREMUL;
        ima->alpha_mode = IMA_ALPHA_STRAIGHT;
      }
    }

    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      if (tex->iuser.flag & IMA_OLD_PREMUL) {
        tex->iuser.flag &= ~IMA_OLD_PREMUL;
      }

      ima = blo_do_versions_newlibadr(fd, lib, tex->ima);
      if (ima && (tex->iuser.flag & IMA_DO_PREMUL)) {
        ima->flag &= ~IMA_OLD_PREMUL;
        ima->alpha_mode = IMA_ALPHA_STRAIGHT;
      }
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 2)) {
    Image *ima;

    /* initialize 1:1 Aspect */
    for (ima = bmain->images.first; ima; ima = ima->id.next) {
      ima->aspx = ima->aspy = 1.0f;
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 4)) {
    bArmature *arm;
    ModifierData *md;
    Object *ob;

    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      arm->deformflag |= ARM_DEF_B_BONE_REST;
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Armature) {
          ((ArmatureModifierData *)md)->deformflag |= ARM_DEF_B_BONE_REST;
        }
      }
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 5)) {
    /* foreground color needs to be something other then black */
    Scene *sce;
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      sce->r.fg_stamp[0] = sce->r.fg_stamp[1] = sce->r.fg_stamp[2] = 0.8f;
      sce->r.fg_stamp[3] = 1.0f;  /* don't use text alpha yet */
      sce->r.bg_stamp[3] = 0.25f; /* make sure the background has full alpha */
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 6)) {
    Scene *sce;
    /* fix frs_sec_base */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->r.frs_sec_base == 0) {
        sce->r.frs_sec_base = 1;
      }
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 7)) {
    Object *ob;
    bPoseChannel *pchan;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->pose) {
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
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

        sb->keys = NULL;
        sb->totkey = 0;
      }
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 8)) {
    Scene *sce;
    Object *ob;
    PartEff *paf = NULL;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->soft && ob->soft->keys) {
        SoftBody *sb = ob->soft;
        int k;

        for (k = 0; k < sb->totkey; k++) {
          if (sb->keys[k]) {
            MEM_freeN(sb->keys[k]);
          }
        }

        MEM_freeN(sb->keys);

        sb->keys = NULL;
        sb->totkey = 0;
      }

      /* convert old particles to new system */
      if ((paf = blo_do_version_give_parteff_245(ob))) {
        ParticleSystem *psys;
        ModifierData *md;
        ParticleSystemModifierData *psmd;
        ParticleSettings *part;

        /* create new particle system */
        psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
        psys->pointcache = BKE_ptcache_add(&psys->ptcaches);

        part = psys->part = BKE_particlesettings_add(bmain, "ParticleSettings");

        /* needed for proper libdata lookup */
        blo_do_versions_oldnewmap_insert(fd->libmap, psys->part, psys->part, 0);
        part->id.lib = ob->id.lib;

        part->id.us--;
        part->id.tag |= (ob->id.tag & LIB_TAG_NEED_LINK);

        psys->totpart = 0;
        psys->flag = PSYS_CURRENT;

        BLI_addtail(&ob->particlesystem, psys);

        md = modifier_new(eModifierType_ParticleSystem);
        BLI_snprintf(md->name,
                     sizeof(md->name),
                     "ParticleSystem %i",
                     BLI_listbase_count(&ob->particlesystem));
        psmd = (ParticleSystemModifierData *)md;
        psmd->psys = psys;
        BLI_addtail(&ob->modifiers, md);

        /* convert settings from old particle system */
        /* general settings */
        part->totpart = MIN2(paf->totpart, 100000);
        part->sta = paf->sta;
        part->end = paf->end;
        part->lifetime = paf->lifetime;
        part->randlife = paf->randlife;
        psys->seed = paf->seed;
        part->disp = paf->disp;
        part->omat = paf->mat[0];
        part->hair_step = paf->totkey;

        part->eff_group = paf->group;

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

        /* dupliobjects */
        if (ob->transflag & OB_DUPLIVERTS) {
          Object *dup = bmain->objects.first;

          for (; dup; dup = dup->id.next) {
            if (ob == blo_do_versions_newlibadr(fd, lib, dup->parent)) {
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
          FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(
              ob, eModifierType_Fluidsim);
          if (fluidmd && fluidmd->fss && fluidmd->fss->type == OB_FLUIDSIM_PARTICLE) {
            part->type = PART_FLUID;
          }
        }

        do_version_free_effects_245(&ob->effect);

        printf("Old particle system converted to new system.\n");
      }
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
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

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 10)) {
    Object *ob;

    /* dupliface scale */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ob->instance_faces_scale = 1.0f;
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 11)) {
    Object *ob;
    bActionStrip *strip;

    /* nla-strips - scale */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      for (strip = ob->nlastrips.first; strip; strip = strip->next) {
        float length, actlength, repeat;

        if (strip->flag & ACTSTRIP_USESTRIDE) {
          repeat = 1.0f;
        }
        else {
          repeat = strip->repeat;
        }

        length = strip->end - strip->start;
        if (length == 0.0f) {
          length = 1.0f;
        }
        actlength = strip->actend - strip->actstart;

        strip->scale = length / (repeat * actlength);
        if (strip->scale == 0.0f) {
          strip->scale = 1.0f;
        }
      }
      if (ob->soft) {
        ob->soft->inpush = ob->soft->inspring;
        ob->soft->shearstiff = 1.0f;
      }
    }
  }

  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 14)) {
    Scene *sce;
    Sequence *seq;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      SEQ_BEGIN (sce->ed, seq) {
        if (seq->blend_mode == 0) {
          seq->blend_opacity = 100.0f;
        }
      }
      SEQ_END;
    }
  }

  /* fix broken group lengths in id properties */
  if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 15)) {
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
    idproperties_fix_group_lengths(bmain->ipo);
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
  if (bmain->versionfile < 246 || (bmain->versionfile == 246 && bmain->subversionfile < 1)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->fluidsimSettings) {
        FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifier_new(
            eModifierType_Fluidsim);
        BLI_addhead(&ob->modifiers, (ModifierData *)fluidmd);

        MEM_freeN(fluidmd->fss);
        fluidmd->fss = MEM_dupallocN(ob->fluidsimSettings);
        fluidmd->fss->ipo = blo_do_versions_newlibadr_us(
            fd, ob->id.lib, ob->fluidsimSettings->ipo);
        MEM_freeN(ob->fluidsimSettings);

        fluidmd->fss->lastgoodframe = INT_MAX;
        fluidmd->fss->flag = 0;
        fluidmd->fss->meshVelocities = NULL;
      }
    }
  }

  if (bmain->versionfile < 246 || (bmain->versionfile == 246 && bmain->subversionfile < 1)) {
    Object *ob;
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->pd && (ob->pd->forcefield == PFIELD_WIND)) {
        ob->pd->f_noise = 0.0f;
      }
    }
  }

  /* set the curve radius interpolation to 2.47 default - easy */
  if (bmain->versionfile < 247 || (bmain->versionfile == 247 && bmain->subversionfile < 6)) {
    Curve *cu;
    Nurb *nu;

    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      for (nu = cu->nurb.first; nu; nu = nu->next) {
        if (nu) {
          nu->radius_interp = 3;

          /* resolu and resolv are now used differently for surfaces
           * rather than using the resolution to define the entire number of divisions,
           * use it for the number of divisions per segment
           */
          if (nu->pntsv > 1) {
            nu->resolu = MAX2(1, (int)(((float)nu->resolu / (float)nu->pntsu) + 0.5f));
            nu->resolv = MAX2(1, (int)(((float)nu->resolv / (float)nu->pntsv) + 0.5f));
          }
        }
      }
    }
  }

  if (bmain->versionfile < 248 || (bmain->versionfile == 248 && bmain->subversionfile < 2)) {
    Scene *sce;

    /* Note, these will need to be added for painting */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      sce->toolsettings->imapaint.seam_bleed = 2;
      sce->toolsettings->imapaint.normal_angle = 80;
    }
  }

  if (bmain->versionfile < 248 || (bmain->versionfile == 248 && bmain->subversionfile < 3)) {
    bScreen *sc;

    /* adjust default settings for Animation Editors */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->pd) {
        ob->pd->seed = ((uint)(ceil(PIL_check_seconds_timer())) + 1) % 128;
      }
    }
  }

  if (bmain->versionfile < 249 && bmain->subversionfile < 2) {
    Scene *sce = bmain->scenes.first;
    Sequence *seq;
    Editing *ed;

    while (sce) {
      ed = sce->ed;
      if (ed) {
        SEQP_BEGIN (ed, seq) {
          if (seq->strip && seq->strip->proxy) {
            seq->strip->proxy->quality = 90;
          }
        }
        SEQ_END;
      }

      sce = sce->id.next;
    }
  }
}
