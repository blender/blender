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
 * \ingroup bli
 */

/* TODO,
 * currently there are some cases we don't support.
 * - passing output paths to the visitor?, like render out.
 * - passing sequence strips with many images.
 * - passing directory paths - visitors don't know which path is a dir or a file.
 * */

#include <sys/stat.h>

#include <assert.h>
#include <string.h>

/* path/file handling stuff */
#ifndef WIN32
#  include <dirent.h>
#  include <unistd.h>
#else
#  include "BLI_winstuff.h"
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_fluid_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcache_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"
#include "DNA_volume_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_font.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"

#include "BKE_bpath.h" /* own include */

#include "CLG_log.h"

#ifndef _MSC_VER
#  include "BLI_strict_flags.h"
#endif

static CLG_LogRef LOG = {"bke.bpath"};

/* -------------------------------------------------------------------- */
/** \name Check Missing Files
 * \{ */

static bool checkMissingFiles_visit_cb(void *userdata,
                                       char *UNUSED(path_dst),
                                       const char *path_src)
{
  ReportList *reports = (ReportList *)userdata;

  if (!BLI_exists(path_src)) {
    BKE_reportf(reports, RPT_WARNING, "Path '%s' not found", path_src);
  }

  return false;
}

/* high level function */
void BKE_bpath_missing_files_check(Main *bmain, ReportList *reports)
{
  BKE_bpath_traverse_main(bmain,
                          checkMissingFiles_visit_cb,
                          BKE_BPATH_TRAVERSE_ABS | BKE_BPATH_TRAVERSE_SKIP_PACKED,
                          reports);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rebase Relative Paths
 * \{ */

typedef struct BPathRebase_Data {
  const char *basedir_src;
  const char *basedir_dst;
  ReportList *reports;

  int count_tot;
  int count_changed;
  int count_failed;
} BPathRebase_Data;

static bool bpath_relative_rebase_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
  BPathRebase_Data *data = (BPathRebase_Data *)userdata;

  data->count_tot++;

  if (BLI_path_is_rel(path_src)) {
    char filepath[(FILE_MAXDIR * 2) + FILE_MAXFILE];
    BLI_strncpy(filepath, path_src, FILE_MAX);
    if (BLI_path_abs(filepath, data->basedir_src)) {
      BLI_path_normalize(NULL, filepath);

      /* This may fail, if so it's fine to leave absolute since the path is still valid. */
      BLI_path_rel(filepath, data->basedir_dst);

      BLI_strncpy(path_dst, filepath, FILE_MAX);
      data->count_changed++;
      return true;
    }
    else {
      /* Failed to make relative path absolute. */
      BLI_assert(0);
      BKE_reportf(data->reports, RPT_WARNING, "Path '%s' cannot be made absolute", path_src);
      data->count_failed++;
      return false;
    }
    return false;
  }
  else {
    /* Absolute, leave this as-is. */
    return false;
  }
}

void BKE_bpath_relative_rebase(Main *bmain,
                               const char *basedir_src,
                               const char *basedir_dst,
                               ReportList *reports)
{
  BPathRebase_Data data = {NULL};
  const int flag = BKE_BPATH_TRAVERSE_SKIP_LIBRARY;

  BLI_assert(basedir_src[0] != '\0');
  BLI_assert(basedir_dst[0] != '\0');

  data.basedir_src = basedir_src;
  data.basedir_dst = basedir_dst;
  data.reports = reports;

  BKE_bpath_traverse_main(bmain, bpath_relative_rebase_visit_cb, flag, (void *)&data);

  BKE_reportf(reports,
              data.count_failed ? RPT_WARNING : RPT_INFO,
              "Total files %d | Changed %d | Failed %d",
              data.count_tot,
              data.count_changed,
              data.count_failed);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Paths Relative
 * \{ */

typedef struct BPathRemap_Data {
  const char *basedir;
  ReportList *reports;

  int count_tot;
  int count_changed;
  int count_failed;
} BPathRemap_Data;

static bool bpath_relative_convert_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
  BPathRemap_Data *data = (BPathRemap_Data *)userdata;

  data->count_tot++;

  if (BLI_path_is_rel(path_src)) {
    return false; /* already relative */
  }
  else {
    strcpy(path_dst, path_src);
    BLI_path_rel(path_dst, data->basedir);
    if (BLI_path_is_rel(path_dst)) {
      data->count_changed++;
    }
    else {
      BKE_reportf(data->reports, RPT_WARNING, "Path '%s' cannot be made relative", path_src);
      data->count_failed++;
    }
    return true;
  }
}

void BKE_bpath_relative_convert(Main *bmain, const char *basedir, ReportList *reports)
{
  BPathRemap_Data data = {NULL};
  const int flag = BKE_BPATH_TRAVERSE_SKIP_LIBRARY;

  if (basedir[0] == '\0') {
    CLOG_ERROR(&LOG, "basedir='', this is a bug");
    return;
  }

  data.basedir = basedir;
  data.reports = reports;

  BKE_bpath_traverse_main(bmain, bpath_relative_convert_visit_cb, flag, (void *)&data);

  BKE_reportf(reports,
              data.count_failed ? RPT_WARNING : RPT_INFO,
              "Total files %d | Changed %d | Failed %d",
              data.count_tot,
              data.count_changed,
              data.count_failed);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Paths Absolute
 * \{ */

static bool bpath_absolute_convert_visit_cb(void *userdata, char *path_dst, const char *path_src)
{
  BPathRemap_Data *data = (BPathRemap_Data *)userdata;

  data->count_tot++;

  if (BLI_path_is_rel(path_src) == false) {
    return false; /* already absolute */
  }
  else {
    strcpy(path_dst, path_src);
    BLI_path_abs(path_dst, data->basedir);
    if (BLI_path_is_rel(path_dst) == false) {
      data->count_changed++;
    }
    else {
      BKE_reportf(data->reports, RPT_WARNING, "Path '%s' cannot be made absolute", path_src);
      data->count_failed++;
    }
    return true;
  }
}

/* similar to BKE_bpath_relative_convert - keep in sync! */
void BKE_bpath_absolute_convert(Main *bmain, const char *basedir, ReportList *reports)
{
  BPathRemap_Data data = {NULL};
  const int flag = BKE_BPATH_TRAVERSE_SKIP_LIBRARY;

  if (basedir[0] == '\0') {
    CLOG_ERROR(&LOG, "basedir='', this is a bug");
    return;
  }

  data.basedir = basedir;
  data.reports = reports;

  BKE_bpath_traverse_main(bmain, bpath_absolute_convert_visit_cb, flag, (void *)&data);

  BKE_reportf(reports,
              data.count_failed ? RPT_WARNING : RPT_INFO,
              "Total files %d | Changed %d | Failed %d",
              data.count_tot,
              data.count_changed,
              data.count_failed);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Missing Files
 * \{ */

/**
 * find this file recursively, use the biggest file so thumbnails don't get used by mistake
 * \param filename_new: the path will be copied here, caller must initialize as empty string.
 * \param dirname: subdir to search
 * \param filename: set this filename
 * \param filesize: filesize for the file
 *
 * \returns found: 1/0.
 */
#define MAX_RECUR 16
static bool missing_files_find__recursive(char *filename_new,
                                          const char *dirname,
                                          const char *filename,
                                          int64_t *r_filesize,
                                          int *r_recur_depth)
{
  /* file searching stuff */
  DIR *dir;
  struct dirent *de;
  BLI_stat_t status;
  char path[FILE_MAX];
  int64_t size;
  bool found = false;

  dir = opendir(dirname);

  if (dir == NULL) {
    return found;
  }

  if (*r_filesize == -1) {
    *r_filesize = 0; /* dir opened fine */
  }

  while ((de = readdir(dir)) != NULL) {

    if (FILENAME_IS_CURRPAR(de->d_name)) {
      continue;
    }

    BLI_join_dirfile(path, sizeof(path), dirname, de->d_name);

    if (BLI_stat(path, &status) == -1) {
      continue; /* cant stat, don't bother with this file, could print debug info here */
    }

    if (S_ISREG(status.st_mode)) {                              /* is file */
      if (BLI_path_ncmp(filename, de->d_name, FILE_MAX) == 0) { /* name matches */
        /* open the file to read its size */
        size = status.st_size;
        if ((size > 0) && (size > *r_filesize)) { /* find the biggest file */
          *r_filesize = size;
          BLI_strncpy(filename_new, path, FILE_MAX);
          found = true;
        }
      }
    }
    else if (S_ISDIR(status.st_mode)) { /* is subdir */
      if (*r_recur_depth <= MAX_RECUR) {
        (*r_recur_depth)++;
        found |= missing_files_find__recursive(
            filename_new, path, filename, r_filesize, r_recur_depth);
        (*r_recur_depth)--;
      }
    }
  }
  closedir(dir);
  return found;
}

typedef struct BPathFind_Data {
  const char *basedir;
  const char *searchdir;
  ReportList *reports;
  bool find_all;
} BPathFind_Data;

static bool missing_files_find__visit_cb(void *userdata, char *path_dst, const char *path_src)
{
  BPathFind_Data *data = (BPathFind_Data *)userdata;
  char filename_new[FILE_MAX];

  int64_t filesize = -1;
  int recur_depth = 0;
  bool found;

  if (data->find_all == false) {
    if (BLI_exists(path_src)) {
      return false;
    }
  }

  filename_new[0] = '\0';

  found = missing_files_find__recursive(
      filename_new, data->searchdir, BLI_path_basename(path_src), &filesize, &recur_depth);

  if (filesize == -1) { /* could not open dir */
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Could not open directory '%s'",
                BLI_path_basename(data->searchdir));
    return false;
  }
  else if (found == false) {
    BKE_reportf(data->reports,
                RPT_WARNING,
                "Could not find '%s' in '%s'",
                BLI_path_basename(path_src),
                data->searchdir);
    return false;
  }
  else {
    bool was_relative = BLI_path_is_rel(path_dst);

    BLI_strncpy(path_dst, filename_new, FILE_MAX);

    /* keep path relative if the previous one was relative */
    if (was_relative) {
      BLI_path_rel(path_dst, data->basedir);
    }

    return true;
  }
}

void BKE_bpath_missing_files_find(Main *bmain,
                                  const char *searchpath,
                                  ReportList *reports,
                                  const bool find_all)
{
  struct BPathFind_Data data = {NULL};
  const int flag = BKE_BPATH_TRAVERSE_ABS | BKE_BPATH_TRAVERSE_RELOAD_EDITED;

  data.basedir = BKE_main_blendfile_path(bmain);
  data.reports = reports;
  data.searchdir = searchpath;
  data.find_all = find_all;

  BKE_bpath_traverse_main(bmain, missing_files_find__visit_cb, flag, (void *)&data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic File Path Traversal API
 * \{ */

/**
 * Run a visitor on a string, replacing the contents of the string as needed.
 */
static bool rewrite_path_fixed(char *path,
                               BPathVisitor visit_cb,
                               const char *absbase,
                               void *userdata)
{
  char path_src_buf[FILE_MAX];
  const char *path_src;
  char path_dst[FILE_MAX];

  if (absbase) {
    BLI_strncpy(path_src_buf, path, sizeof(path_src_buf));
    BLI_path_abs(path_src_buf, absbase);
    path_src = path_src_buf;
  }
  else {
    path_src = path;
  }

  /* so functions can check old value */
  BLI_strncpy(path_dst, path, FILE_MAX);

  if (visit_cb(userdata, path_dst, path_src)) {
    BLI_strncpy(path, path_dst, FILE_MAX);
    return true;
  }
  else {
    return false;
  }
}

static bool rewrite_path_fixed_dirfile(char path_dir[FILE_MAXDIR],
                                       char path_file[FILE_MAXFILE],
                                       BPathVisitor visit_cb,
                                       const char *absbase,
                                       void *userdata)
{
  char path_src[FILE_MAX];
  char path_dst[FILE_MAX];

  BLI_join_dirfile(path_src, sizeof(path_src), path_dir, path_file);

  /* so functions can check old value */
  BLI_strncpy(path_dst, path_src, FILE_MAX);

  if (absbase) {
    BLI_path_abs(path_src, absbase);
  }

  if (visit_cb(userdata, path_dst, (const char *)path_src)) {
    BLI_split_dirfile(path_dst, path_dir, path_file, FILE_MAXDIR, FILE_MAXFILE);
    return true;
  }
  else {
    return false;
  }
}

static bool rewrite_path_alloc(char **path,
                               BPathVisitor visit_cb,
                               const char *absbase,
                               void *userdata)
{
  char path_src_buf[FILE_MAX];
  const char *path_src;
  char path_dst[FILE_MAX];

  if (absbase) {
    BLI_strncpy(path_src_buf, *path, sizeof(path_src_buf));
    BLI_path_abs(path_src_buf, absbase);
    path_src = path_src_buf;
  }
  else {
    path_src = *path;
  }

  if (visit_cb(userdata, path_dst, path_src)) {
    MEM_freeN(*path);
    (*path) = BLI_strdup(path_dst);
    return true;
  }
  else {
    return false;
  }
}

/**
 * Run visitor function 'visit' on all paths contained in 'id'.
 */
void BKE_bpath_traverse_id(
    Main *bmain, ID *id, BPathVisitor visit_cb, const int flag, void *bpath_user_data)
{
  const char *absbase = (flag & BKE_BPATH_TRAVERSE_ABS) ? ID_BLEND_PATH(bmain, id) : NULL;

  if ((flag & BKE_BPATH_TRAVERSE_SKIP_LIBRARY) && ID_IS_LINKED(id)) {
    return;
  }

  switch (GS(id->name)) {
    case ID_IM: {
      Image *ima;
      ima = (Image *)id;
      if (BKE_image_has_packedfile(ima) == false || (flag & BKE_BPATH_TRAVERSE_SKIP_PACKED) == 0) {
        /* Skip empty file paths, these are typically from generated images and
         * don't make sense to add directories to until the image has been saved
         * once to give it a meaningful value. */
        if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE, IMA_SRC_TILED) &&
            ima->name[0]) {
          if (rewrite_path_fixed(ima->name, visit_cb, absbase, bpath_user_data)) {
            if (flag & BKE_BPATH_TRAVERSE_RELOAD_EDITED) {
              if (!BKE_image_has_packedfile(ima) &&
                  /* image may have been painted onto (and not saved, T44543) */
                  !BKE_image_is_dirty(ima)) {
                BKE_image_signal(bmain, ima, NULL, IMA_SIGNAL_RELOAD);
              }
            }
          }
        }
      }
      break;
    }
    case ID_BR: {
      Brush *brush = (Brush *)id;
      if (brush->icon_filepath[0]) {
        rewrite_path_fixed(brush->icon_filepath, visit_cb, absbase, bpath_user_data);
      }
      break;
    }
    case ID_OB: {
      Object *ob = (Object *)id;
      ModifierData *md;
      ParticleSystem *psys;

#define BPATH_TRAVERSE_POINTCACHE(ptcaches) \
  { \
    PointCache *cache; \
    for (cache = (ptcaches).first; cache; cache = cache->next) { \
      if (cache->flag & PTCACHE_DISK_CACHE) { \
        rewrite_path_fixed(cache->path, visit_cb, absbase, bpath_user_data); \
      } \
    } \
  } \
  (void)0

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Fluidsim) {
          FluidsimModifierData *fluidmd = (FluidsimModifierData *)md;
          if (fluidmd->fss) {
            rewrite_path_fixed(fluidmd->fss->surfdataPath, visit_cb, absbase, bpath_user_data);
          }
        }
        else if (md->type == eModifierType_Fluid) {
          FluidModifierData *mmd = (FluidModifierData *)md;
          if (mmd->type & MOD_FLUID_TYPE_DOMAIN && mmd->domain) {
            rewrite_path_fixed(mmd->domain->cache_directory, visit_cb, absbase, bpath_user_data);
          }
        }
        else if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;
          BPATH_TRAVERSE_POINTCACHE(clmd->ptcaches);
        }
        else if (md->type == eModifierType_Ocean) {
          OceanModifierData *omd = (OceanModifierData *)md;
          rewrite_path_fixed(omd->cachepath, visit_cb, absbase, bpath_user_data);
        }
        else if (md->type == eModifierType_MeshCache) {
          MeshCacheModifierData *mcmd = (MeshCacheModifierData *)md;
          rewrite_path_fixed(mcmd->filepath, visit_cb, absbase, bpath_user_data);
        }
      }

      if (ob->soft) {
        BPATH_TRAVERSE_POINTCACHE(ob->soft->shared->ptcaches);
      }

      for (psys = ob->particlesystem.first; psys; psys = psys->next) {
        BPATH_TRAVERSE_POINTCACHE(psys->ptcaches);
      }

#undef BPATH_TRAVERSE_POINTCACHE

      break;
    }
    case ID_SO: {
      bSound *sound = (bSound *)id;
      if (sound->packedfile == NULL || (flag & BKE_BPATH_TRAVERSE_SKIP_PACKED) == 0) {
        rewrite_path_fixed(sound->name, visit_cb, absbase, bpath_user_data);
      }
      break;
    }
    case ID_VO: {
      Volume *volume = (Volume *)id;
      if (volume->packedfile == NULL || (flag & BKE_BPATH_TRAVERSE_SKIP_PACKED) == 0) {
        rewrite_path_fixed(volume->filepath, visit_cb, absbase, bpath_user_data);
      }
      break;
    }
    case ID_TXT:
      if (((Text *)id)->name) {
        rewrite_path_alloc(&((Text *)id)->name, visit_cb, absbase, bpath_user_data);
      }
      break;
    case ID_VF: {
      VFont *vfont = (VFont *)id;
      if (vfont->packedfile == NULL || (flag & BKE_BPATH_TRAVERSE_SKIP_PACKED) == 0) {
        if (BKE_vfont_is_builtin(vfont) == false) {
          rewrite_path_fixed(((VFont *)id)->name, visit_cb, absbase, bpath_user_data);
        }
      }
      break;
    }
    case ID_MA: {
      Material *ma = (Material *)id;
      bNodeTree *ntree = ma->nodetree;

      if (ntree) {
        bNode *node;

        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == SH_NODE_SCRIPT) {
            NodeShaderScript *nss = (NodeShaderScript *)node->storage;
            rewrite_path_fixed(nss->filepath, visit_cb, absbase, bpath_user_data);
          }
          else if (node->type == SH_NODE_TEX_IES) {
            NodeShaderTexIES *ies = (NodeShaderTexIES *)node->storage;
            rewrite_path_fixed(ies->filepath, visit_cb, absbase, bpath_user_data);
          }
        }
      }
      break;
    }
    case ID_NT: {
      bNodeTree *ntree = (bNodeTree *)id;
      bNode *node;

      if (ntree->type == NTREE_SHADER) {
        /* same as lines above */
        for (node = ntree->nodes.first; node; node = node->next) {
          if (node->type == SH_NODE_SCRIPT) {
            NodeShaderScript *nss = (NodeShaderScript *)node->storage;
            rewrite_path_fixed(nss->filepath, visit_cb, absbase, bpath_user_data);
          }
          else if (node->type == SH_NODE_TEX_IES) {
            NodeShaderTexIES *ies = (NodeShaderTexIES *)node->storage;
            rewrite_path_fixed(ies->filepath, visit_cb, absbase, bpath_user_data);
          }
        }
      }
      break;
    }
    case ID_SCE: {
      Scene *scene = (Scene *)id;
      if (scene->ed) {
        Sequence *seq;

        SEQ_BEGIN (scene->ed, seq) {
          if (SEQ_HAS_PATH(seq)) {
            StripElem *se = seq->strip->stripdata;

            if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM) && se) {
              rewrite_path_fixed_dirfile(
                  seq->strip->dir, se->name, visit_cb, absbase, bpath_user_data);
            }
            else if ((seq->type == SEQ_TYPE_IMAGE) && se) {
              /* might want an option not to loop over all strips */
              unsigned int len = (unsigned int)MEM_allocN_len(se) / (unsigned int)sizeof(*se);
              unsigned int i;

              if (flag & BKE_BPATH_TRAVERSE_SKIP_MULTIFILE) {
                /* only operate on one path */
                len = MIN2(1u, len);
              }

              for (i = 0; i < len; i++, se++) {
                rewrite_path_fixed_dirfile(
                    seq->strip->dir, se->name, visit_cb, absbase, bpath_user_data);
              }
            }
            else {
              /* simple case */
              rewrite_path_fixed(seq->strip->dir, visit_cb, absbase, bpath_user_data);
            }
          }
        }
        SEQ_END;
      }
      break;
    }
    case ID_ME: {
      Mesh *me = (Mesh *)id;
      if (me->ldata.external) {
        rewrite_path_fixed(me->ldata.external->filename, visit_cb, absbase, bpath_user_data);
      }
      break;
    }
    case ID_LI: {
      Library *lib = (Library *)id;
      /* keep packedfile paths always relative to the blend */
      if (lib->packedfile == NULL) {
        if (rewrite_path_fixed(lib->name, visit_cb, absbase, bpath_user_data)) {
          BKE_library_filepath_set(bmain, lib, lib->name);
        }
      }
      break;
    }
    case ID_MC: {
      MovieClip *clip = (MovieClip *)id;
      rewrite_path_fixed(clip->name, visit_cb, absbase, bpath_user_data);
      break;
    }
    case ID_CF: {
      CacheFile *cache_file = (CacheFile *)id;
      rewrite_path_fixed(cache_file->filepath, visit_cb, absbase, bpath_user_data);
      break;
    }
    default:
      /* Nothing to do for other IDs that don't contain file paths. */
      break;
  }
}

void BKE_bpath_traverse_id_list(
    Main *bmain, ListBase *lb, BPathVisitor visit_cb, const int flag, void *bpath_user_data)
{
  ID *id;
  for (id = lb->first; id; id = id->next) {
    BKE_bpath_traverse_id(bmain, id, visit_cb, flag, bpath_user_data);
  }
}

void BKE_bpath_traverse_main(Main *bmain,
                             BPathVisitor visit_cb,
                             const int flag,
                             void *bpath_user_data)
{
  ListBase *lbarray[MAX_LIBARRAY];
  int a = set_listbasepointers(bmain, lbarray);
  while (a--) {
    BKE_bpath_traverse_id_list(bmain, lbarray[a], visit_cb, flag, bpath_user_data);
  }
}

/**
 * Rewrites a relative path to be relative to the main file - unless the path is
 * absolute, in which case it is not altered.
 */
bool BKE_bpath_relocate_visitor(void *pathbase_v, char *path_dst, const char *path_src)
{
  /* be sure there is low chance of the path being too short */
  char filepath[(FILE_MAXDIR * 2) + FILE_MAXFILE];
  const char *base_new = ((char **)pathbase_v)[0];
  const char *base_old = ((char **)pathbase_v)[1];

  if (BLI_path_is_rel(base_old)) {
    CLOG_ERROR(&LOG, "old base path '%s' is not absolute.", base_old);
    return false;
  }

  /* Make referenced file absolute. This would be a side-effect of
   * BLI_path_normalize, but we do it explicitly so we know if it changed. */
  BLI_strncpy(filepath, path_src, FILE_MAX);
  if (BLI_path_abs(filepath, base_old)) {
    /* Path was relative and is now absolute. Remap.
     * Important BLI_path_normalize runs before the path is made relative
     * because it wont work for paths that start with "//../" */
    BLI_path_normalize(base_new, filepath);
    BLI_path_rel(filepath, base_new);
    BLI_strncpy(path_dst, filepath, FILE_MAX);
    return true;
  }
  else {
    /* Path was not relative to begin with. */
    return false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backup/Restore/Free functions,
 *
 * \note These functions assume the data won't change order.
 * \{ */

struct PathStore {
  struct PathStore *next, *prev;
};

static bool bpath_list_append(void *userdata, char *UNUSED(path_dst), const char *path_src)
{
  /* store the path and string in a single alloc */
  ListBase *ls = userdata;
  size_t path_size = strlen(path_src) + 1;
  struct PathStore *path_store = MEM_mallocN(sizeof(struct PathStore) + path_size, __func__);
  char *filepath = (char *)(path_store + 1);

  memcpy(filepath, path_src, path_size);
  BLI_addtail(ls, path_store);
  return false;
}

static bool bpath_list_restore(void *userdata, char *path_dst, const char *path_src)
{
  /* assume ls->first wont be NULL because the number of paths can't change!
   * (if they do caller is wrong) */
  ListBase *ls = userdata;
  struct PathStore *path_store = ls->first;
  const char *filepath = (char *)(path_store + 1);
  bool ret;

  if (STREQ(path_src, filepath)) {
    ret = false;
  }
  else {
    BLI_strncpy(path_dst, filepath, FILE_MAX);
    ret = true;
  }

  BLI_freelinkN(ls, path_store);
  return ret;
}

/* return ls_handle */
void *BKE_bpath_list_backup(Main *bmain, const int flag)
{
  ListBase *ls = MEM_callocN(sizeof(ListBase), __func__);

  BKE_bpath_traverse_main(bmain, bpath_list_append, flag, ls);

  return ls;
}

void BKE_bpath_list_restore(Main *bmain, const int flag, void *ls_handle)
{
  ListBase *ls = ls_handle;

  BKE_bpath_traverse_main(bmain, bpath_list_restore, flag, ls);
}

void BKE_bpath_list_free(void *ls_handle)
{
  ListBase *ls = ls_handle;
  BLI_assert(BLI_listbase_is_empty(ls)); /* assumes we were used */
  BLI_freelistN(ls);
  MEM_freeN(ls);
}

/** \} */
