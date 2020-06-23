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
 * \ingroup collada
 */

#include "COLLADABUURI.h"
#include "COLLADASWImage.h"

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_texture_types.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "IMB_imbuf_types.h"

#include "ImageExporter.h"
#include "MaterialExporter.h"

ImagesExporter::ImagesExporter(COLLADASW::StreamWriter *sw,
                               BCExportSettings &export_settings,
                               KeyImageMap &key_image_map)
    : COLLADASW::LibraryImages(sw), export_settings(export_settings), key_image_map(key_image_map)
{
  /* pass */
}

void ImagesExporter::export_UV_Image(Image *image, bool use_copies)
{
  std::string name(id_name(image));
  std::string translated_name(translate_id(name));

  ImBuf *imbuf = BKE_image_acquire_ibuf(image, NULL, NULL);
  if (!imbuf) {
    fprintf(stderr, "Collada export: image does not exist:\n%s\n", image->filepath);
    return;
  }

  bool is_dirty = BKE_image_is_dirty(image);

  ImageFormatData imageFormat;
  BKE_imbuf_to_image_format(&imageFormat, imbuf);

  short image_source = image->source;
  bool is_generated = image_source == IMA_SRC_GENERATED;
  bool is_packed = BKE_image_has_packedfile(image);

  char export_path[FILE_MAX];
  char source_path[FILE_MAX];
  char export_dir[FILE_MAX];
  char export_file[FILE_MAX];

  /* Destination folder for exported assets */
  BLI_split_dir_part(this->export_settings.get_filepath(), export_dir, sizeof(export_dir));

  if (is_generated || is_dirty || use_copies || is_packed) {

    /* make absolute destination path */

    BLI_strncpy(export_file, name.c_str(), sizeof(export_file));
    BKE_image_path_ensure_ext_from_imformat(export_file, &imageFormat);

    BLI_join_dirfile(export_path, sizeof(export_path), export_dir, export_file);

    /* make dest directory if it doesn't exist */
    BLI_make_existing_file(export_path);
  }

  if (is_generated || is_dirty || is_packed) {

    /* This image in its current state only exists in Blender memory.
     * So we have to export it. The export will keep the image state intact,
     * so the exported file will not be associated with the image. */

    if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == 0) {
      fprintf(stderr, "Collada export: Cannot export image to:\n%s\n", export_path);
      return;
    }
    BLI_strncpy(export_path, export_file, sizeof(export_path));
  }
  else {

    /* make absolute source path */
    BLI_strncpy(source_path, image->filepath, sizeof(source_path));
    BLI_path_abs(source_path, ID_BLEND_PATH_FROM_GLOBAL(&image->id));
    BLI_path_normalize(NULL, source_path);

    if (use_copies) {

      /* This image is already located on the file system.
       * But we want to create copies here.
       * To move images into the same export directory.
       * Note: If an image is already located in the export folder,
       * then skip the copy (as it would result in a file copy error). */

      if (BLI_path_cmp(source_path, export_path) != 0) {
        if (BLI_copy(source_path, export_path) != 0) {
          fprintf(stderr,
                  "Collada export: Cannot copy image:\n source:%s\ndest :%s\n",
                  source_path,
                  export_path);
          return;
        }
      }

      BLI_strncpy(export_path, export_file, sizeof(export_path));
    }
    else {

      /* Do not make any copies, but use the source path directly as reference
       * to the original image */

      BLI_strncpy(export_path, source_path, sizeof(export_path));
    }
  }

  /* Set name also to mNameNC.
   * This helps other viewers import files exported from Blender better. */
  COLLADASW::Image img(COLLADABU::URI(COLLADABU::URI::nativePathToUri(export_path)),
                       translated_name,
                       translated_name);
  img.add(mSW);
  fprintf(stdout, "Collada export: Added image: %s\n", export_file);

  BKE_image_release_ibuf(image, imbuf, NULL);
}

void ImagesExporter::exportImages(Scene *sce)
{
  bool use_texture_copies = this->export_settings.get_use_texture_copies();
  openLibrary();

  KeyImageMap::iterator iter;
  for (iter = key_image_map.begin(); iter != key_image_map.end(); iter++) {

    Image *image = iter->second;
    std::string uid(id_name(image));
    std::string key = translate_id(uid);

    export_UV_Image(image, use_texture_copies);
  }

  closeLibrary();
}
