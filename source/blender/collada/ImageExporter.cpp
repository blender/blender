/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/ImageExporter.cpp
 *  \ingroup collada
 */


#include "COLLADABUURI.h"
#include "COLLADASWImage.h"

extern "C" {
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h" 
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "IMB_imbuf_types.h"
}

#include "ImageExporter.h"
#include "MaterialExporter.h"


ImagesExporter::ImagesExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryImages(sw), export_settings(export_settings)
{
}

void ImagesExporter::export_UV_Image(Image *image, bool use_copies) 
{
	std::string name(id_name(image));
	std::string translated_name(translate_id(name));
	bool not_yet_exported = find(mImages.begin(), mImages.end(), translated_name) == mImages.end();

	if (not_yet_exported) {

		ImBuf *imbuf       = BKE_image_get_ibuf(image, NULL);
		bool  is_dirty     = imbuf->userflags & IB_BITMAPDIRTY;

		ImageFormatData imageFormat;
		BKE_imbuf_to_image_format(&imageFormat, imbuf);

		short image_source = image->source;
		bool  is_generated = image_source == IMA_SRC_GENERATED;

		char export_path[FILE_MAX];
		char source_path[FILE_MAX];
		char export_dir[FILE_MAX];
		char export_file[FILE_MAX];

		// Destination folder for exported assets
		BLI_split_dir_part(this->export_settings->filepath, export_dir, sizeof(export_dir));

		if (is_generated || is_dirty || use_copies) {

			// make absolute destination path

			BLI_strncpy(export_file, name.c_str(), sizeof(export_file));
			BKE_add_image_extension(export_file, imageFormat.imtype);

			BLI_join_dirfile(export_path, sizeof(export_path), export_dir, export_file);

			// make dest directory if it doesn't exist
			BLI_make_existing_file(export_path);
		}

		if (is_generated || is_dirty) {

			// This image in its current state only exists in Blender memory.
			// So we have to export it. The export will keep the image state intact,
			// so the exported file will not be associated with the image.

			if (BKE_imbuf_write_as(imbuf, export_path, &imageFormat, true) == 0) {
				fprintf(stderr, "Collada export: Cannot export image to:\n%s\n", export_path);
			}
			BLI_strncpy(export_path, export_file, sizeof(export_path));
		}
		else {

			// make absolute source path
			BLI_strncpy(source_path, image->name, sizeof(source_path));
			BLI_path_abs(source_path, G.main->name);
			BLI_cleanup_path(NULL, source_path);

			if (use_copies) {
			
				// This image is already located on the file system.
				// But we want to create copies here.
				// To avoid overwroting images with same file name but
				// differenet source locations

				if (BLI_copy(source_path, export_path) != 0) {
					fprintf(stderr, "Collada export: Cannot copy image:\n source:%s\ndest :%s\n", source_path, export_path);
				}

				BLI_strncpy(export_path, export_file, sizeof(export_path));

			}
			else {

				// Do not make any vopies, but use the source path directly as reference
				// to the original image

				BLI_strncpy(export_path, source_path, sizeof(export_path));
			}
		}

		COLLADASW::Image img(COLLADABU::URI(COLLADABU::URI::nativePathToUri(export_path)), translated_name, translated_name); /* set name also to mNameNC. This helps other viewers import files exported from Blender better */
		img.add(mSW);
		fprintf(stdout, "Collada export: Added image: %s\n",export_file);
		mImages.push_back(translated_name);
	}
}

void ImagesExporter::export_UV_Images()
{
	std::set<Image *> uv_textures;
	LinkNode *node;
	bool use_copies = this->export_settings->use_texture_copies;
	for (node=this->export_settings->export_set; node; node=node->next) {
		Object *ob = (Object *)node->link;
		if (ob->type == OB_MESH && ob->totcol) {
			Mesh *me     = (Mesh *) ob->data;
			BKE_mesh_tessface_ensure(me);
			for (int i = 0; i < me->pdata.totlayer; i++) {
				if (me->pdata.layers[i].type == CD_MTEXPOLY) {
					MTexPoly *txface = (MTexPoly *)me->pdata.layers[i].data;
					MFace *mface = me->mface;
					for (int j = 0; j < me->totpoly; j++, mface++, txface++) {

						Image *ima = txface->tpage;
						if (ima == NULL)
							continue;

						bool not_in_list = uv_textures.find(ima)==uv_textures.end();
						if (not_in_list) {
								uv_textures.insert(ima);
								export_UV_Image(ima, use_copies);
						}
					}
				}
			}
		}
	}
}


bool ImagesExporter::hasImages(Scene *sce)
{
	LinkNode *node;
	
	for (node=this->export_settings->export_set; node; node=node->next) {
		Object *ob = (Object *)node->link;
		int a;
		for (a = 0; a < ob->totcol; a++) {
			Material *ma = give_current_material(ob, a + 1);

			// no material, but check all of the slots
			if (!ma) continue;
			int b;
			for (b = 0; b < MAX_MTEX; b++) {
				MTex *mtex = ma->mtex[b];
				if (mtex && mtex->tex && mtex->tex->ima) return true;
			}

		}
		if (ob->type == OB_MESH) {
			Mesh *me     = (Mesh *) ob->data;
			BKE_mesh_tessface_ensure(me);
			bool has_uvs = (bool)CustomData_has_layer(&me->fdata, CD_MTFACE);
			if (has_uvs) {
				int num_layers = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
				for (int a = 0; a < num_layers; a++) {
					MTFace *tface = (MTFace *)CustomData_get_layer_n(&me->fdata, CD_MTFACE, a);
					Image *img = tface->tpage;
					if(img) return true;
				}
			}
		}

	}
	return false;
}

void ImagesExporter::exportImages(Scene *sce)
{
	openLibrary();

	MaterialFunctor mf;
	if (this->export_settings->include_material_textures) {
		mf.forEachMaterialInExportSet<ImagesExporter>(sce, *this, this->export_settings->export_set);
	}

	if (this->export_settings->include_uv_textures) {
		export_UV_Images();
	}

	closeLibrary();
}



void ImagesExporter::operator()(Material *ma, Object *ob)
{
	int a;
	bool use_texture_copies = this->export_settings->use_texture_copies;
	for (a = 0; a < MAX_MTEX; a++) {
		MTex *mtex = ma->mtex[a];
		if (mtex && mtex->tex && mtex->tex->ima) {
			Image *image = mtex->tex->ima;
			export_UV_Image(image, use_texture_copies);
		}
	}
}
