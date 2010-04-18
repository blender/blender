/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation;
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef MOD_UTIL_H
#define MOD_UTIL_H

struct Tex;
struct TexResult;
struct CustomData;
struct DerivedMesh;
struct Object;
struct Scene;
struct EditMesh;
struct ModifierData;

void get_texture_value(struct Tex *texture, float *tex_co, struct TexResult *texres);
void modifier_vgroup_cache(struct ModifierData *md, float (*vertexCos)[3]);
void validate_layer_name(const struct CustomData *data, int type, char *name, char *outname);
struct DerivedMesh *get_cddm(struct Scene *scene, struct Object *ob, struct EditMesh *em, struct DerivedMesh *dm, float (*vertexCos)[3]);
struct DerivedMesh *get_dm(struct Scene *scene, struct Object *ob, struct EditMesh *em, struct DerivedMesh *dm, float (*vertexCos)[3], int orco);
#endif /* MOD_UTIL_H */
