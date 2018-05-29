

#include "workbench_private.h"

#include "BLI_dynstr.h"

#define HSV_SATURATION 0.5
#define HSV_VALUE 0.9

void workbench_material_get_solid_color(WORKBENCH_PrivateData *wpd, Object *ob, Material *mat, float *color)
{
	/* When in OB_TEXTURE always uyse V3D_SHADING_MATERIAL_COLOR as fallback when no texture could be determined */
	int color_type = wpd->drawtype == OB_SOLID ? wpd->shading.color_type : V3D_SHADING_MATERIAL_COLOR;
	static float default_color[] = {0.8f, 0.8f, 0.8f, 1.0f};
	color[3] = 1.0f;
	if (DRW_object_is_paint_mode(ob) || color_type == V3D_SHADING_SINGLE_COLOR) {
		copy_v3_v3(color, wpd->shading.single_color);
	}
	else if (color_type == V3D_SHADING_RANDOM_COLOR) {
		uint hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
		if (ob->id.lib) {
			hash = (hash * 13) ^ BLI_ghashutil_strhash_p_murmur(ob->id.lib->name);
		}
		float offset = fmodf((hash / 100000.0) * M_GOLDEN_RATION_CONJUGATE, 1.0);

		float hsv[3] = {offset, HSV_SATURATION, HSV_VALUE};
		hsv_to_rgb_v(hsv, color);
	}
	else if (color_type == V3D_SHADING_OBJECT_COLOR) {
		copy_v3_v3(color, ob->col);
	}
	else {
		/* V3D_SHADING_MATERIAL_COLOR */
		if (mat) {
			copy_v3_v3(color, &mat->r);
		}
		else {
			copy_v3_v3(color, default_color);
		}
	}
}

char *workbench_material_build_defines(WORKBENCH_PrivateData *wpd, int drawtype)
{
	char *str = NULL;

	DynStr *ds = BLI_dynstr_new();

	if (wpd->shading.flag & V3D_SHADING_OBJECT_OUTLINE) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_OBJECT_OUTLINE\n");
	}
	if (wpd->shading.flag & V3D_SHADING_SHADOW) {
		BLI_dynstr_appendf(ds, "#define V3D_SHADING_SHADOW\n");
	}
	if (wpd->shading.light & V3D_LIGHTING_STUDIO) {
		BLI_dynstr_appendf(ds, "#define V3D_LIGHTING_STUDIO\n");
		if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
			BLI_dynstr_appendf(ds, "#define STUDIOLIGHT_ORIENTATION_WORLD\n");
		}
		else {
			BLI_dynstr_appendf(ds, "#define STUDIOLIGHT_ORIENTATION_CAMERA\n");
		}
	}
	if (NORMAL_VIEWPORT_PASS_ENABLED(wpd)) {
		BLI_dynstr_appendf(ds, "#define NORMAL_VIEWPORT_PASS_ENABLED\n");
	}
	switch (drawtype) {
		case OB_SOLID:
			BLI_dynstr_appendf(ds, "#define OB_SOLID\n");
			break;
		case OB_TEXTURE:
			BLI_dynstr_appendf(ds, "#define OB_TEXTURE\n");
			break;
	}

	if (NORMAL_ENCODING_ENABLED()) {
		BLI_dynstr_appendf(ds, "#define WORKBENCH_ENCODE_NORMALS\n");
	}

#ifdef WORKBENCH_REVEALAGE_ENABLED
	BLI_dynstr_appendf(ds, "#define WORKBENCH_REVEALAGE_ENABLED\n");
#endif

	str = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return str;
}

uint workbench_material_get_hash(WORKBENCH_MaterialData *material_template)
{
	/* TODO: make a C-string with settings and hash the string */
	uint input[4];
	uint result;
	float *color = material_template->color;
	input[0] = (uint)(color[0] * 512);
	input[1] = (uint)(color[1] * 512);
	input[2] = (uint)(color[2] * 512);
	input[3] = material_template->object_id;
	result = BLI_ghashutil_uinthash_v4_murmur(input);

	if (material_template->drawtype == OB_TEXTURE) {
		/* add texture reference */
		result += BLI_ghashutil_inthash_p_murmur(material_template->ima);
	}
	return result;
}

int workbench_material_get_shader_index(WORKBENCH_PrivateData *wpd, int drawtype)
{
	const int DRAWOPTIONS_MASK = V3D_SHADING_OBJECT_OUTLINE | V3D_SHADING_SHADOW;
	int index = (wpd->shading.flag & DRAWOPTIONS_MASK);
	index = (index << 2) + wpd->shading.light;
	index = (index << 2);
	/* set the drawtype flag
	0 = OB_SOLID,
	1 = OB_TEXTURE
	2 = STUDIOLIGHT_ORIENTATION_WORLD
	*/
	SET_FLAG_FROM_TEST(index, wpd->studio_light->flag & STUDIOLIGHT_ORIENTATION_WORLD, 2);
	SET_FLAG_FROM_TEST(index, drawtype == OB_TEXTURE, 1);
	return index;
}

void workbench_material_set_normal_world_matrix(
        DRWShadingGroup *grp, WORKBENCH_PrivateData *wpd, float persistent_matrix[3][3])
{
	if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
		float view_matrix_inverse[4][4];
		float rot_matrix[4][4];
		float matrix[4][4];
		axis_angle_to_mat4_single(rot_matrix, 'Z', -wpd->shading.studiolight_rot_z);
		DRW_viewport_matrix_get(view_matrix_inverse, DRW_MAT_VIEWINV);
		mul_m4_m4m4(matrix, rot_matrix, view_matrix_inverse);
		copy_m3_m4(persistent_matrix, matrix);
		DRW_shgroup_uniform_mat3(grp, "normalWorldMatrix", persistent_matrix);
	}
}
