uniform sampler2D image_texture;
uniform sampler3D lut3d_texture;

#ifdef USE_DITHER
uniform float dither;
#endif

#ifdef USE_TEXTURE_SIZE
uniform float image_texture_width;
uniform float image_texture_height;
#endif

#ifdef USE_CURVE_MAPPING
/* Curve mapping parameters
 *
 * See documentation for OCIO_CurveMappingSettings to get fields descriptions.
 * (this ones pretyt much copies stuff from C structure.)
 */
uniform sampler1D curve_mapping_texture;
uniform int curve_mapping_lut_size;
uniform ivec4 use_curve_mapping_extend_extrapolate;
uniform vec4 curve_mapping_mintable;
uniform vec4 curve_mapping_range;
uniform vec4 curve_mapping_ext_in_x;
uniform vec4 curve_mapping_ext_in_y;
uniform vec4 curve_mapping_ext_out_x;
uniform vec4 curve_mapping_ext_out_y;
uniform vec4 curve_mapping_first_x;
uniform vec4 curve_mapping_first_y;
uniform vec4 curve_mapping_last_x;
uniform vec4 curve_mapping_last_y;
uniform vec3 curve_mapping_black;
uniform vec3 curve_mapping_bwmul;

float read_curve_mapping(int table, int index)
{
	/* TODO(sergey): Without -1 here image is getting darken after applying unite curve.
	 *               But is it actually correct to subtract 1 here?
	 */
	float texture_index = float(index) / float(curve_mapping_lut_size  - 1);
	return texture1D(curve_mapping_texture, texture_index) [table];
}

float curvemap_calc_extend(int table, float x, vec2 first, vec2 last)
{
	if (x <= first[0]) {
		if (use_curve_mapping_extend_extrapolate[table] == 0) {
			/* no extrapolate */
			return first[1];
		}
		else {
			if (curve_mapping_ext_in_x[table] == 0.0)
				return first[1] + curve_mapping_ext_in_y[table] * 10000.0;
			else
				return first[1] + curve_mapping_ext_in_y[table] * (x - first[0]) / curve_mapping_ext_in_x[table];
		}
	}
	else if (x >= last[0]) {
		if (use_curve_mapping_extend_extrapolate[table] == 0) {
			/* no extrapolate */
			return last[1];
		}
		else {
			if (curve_mapping_ext_out_x[table] == 0.0)
				return last[1] - curve_mapping_ext_out_y[table] * 10000.0;
			else
				return last[1] + curve_mapping_ext_out_y[table] * (x - last[0]) / curve_mapping_ext_out_x[table];
		}
	}
	return 0.0;
}

float curvemap_evaluateF(int table, float value)
{
	float mintable_ = curve_mapping_mintable[table];
	float range = curve_mapping_range[table];
	float mintable = 0.0;
	int CM_TABLE = curve_mapping_lut_size - 1;

	float fi;
	int i;

	/* index in table */
	fi = (value - mintable) * range;
	i = int(fi);

	/* fi is table float index and should check against table range i.e. [0.0 CM_TABLE] */
	if (fi < 0.0 || fi > float(CM_TABLE)) {
		return curvemap_calc_extend(table, value,
		                            vec2(curve_mapping_first_x[table], curve_mapping_first_y[table]),
		                            vec2(curve_mapping_last_x[table], curve_mapping_last_y[table]));
	}
	else {
		if (i < 0) return read_curve_mapping(table, 0);
		if (i >= CM_TABLE) return read_curve_mapping(table, CM_TABLE);

		fi = fi - float(i);
		return (1.0 - fi) * read_curve_mapping(table, i) + fi * read_curve_mapping(table, i + 1);
	}
}

vec4 curvemapping_evaluate_premulRGBF(vec4 col)
{
	vec4 result = col;
	result[0] = curvemap_evaluateF(0, (col[0] - curve_mapping_black[0]) * curve_mapping_bwmul[0]);
	result[1] = curvemap_evaluateF(1, (col[1] - curve_mapping_black[1]) * curve_mapping_bwmul[1]);
	result[2] = curvemap_evaluateF(2, (col[2] - curve_mapping_black[2]) * curve_mapping_bwmul[2]);
	result[3] = col[3];
	return result;
}
#endif

#ifdef USE_DITHER
float dither_random_value(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453) * 0.005 * dither;
}

vec2 round_to_pixel(vec2 st)
{
	vec2 result;
#ifdef USE_TEXTURE_SIZE
	vec2 size = vec2(image_texture_width, image_texture_height);
#else
	vec2 size = textureSize(image_texture, 0);
#endif
	result.x = float(int(st.x * size.x)) / size.x;
	result.y = float(int(st.y * size.y)) / size.y;
	return result;
}

vec4 apply_dither(vec2 st, vec4 col)
{
	vec4 result;
	float random_value = dither_random_value(round_to_pixel(st));
	result.r = col.r + random_value;
	result.g = col.g + random_value;
	result.b = col.b + random_value;
	result.a = col.a;
	return result;
}
#endif

void main()
{
	vec4 col = texture2D(image_texture, gl_TexCoord[0].st);
#ifdef USE_CURVE_MAPPING
	col = curvemapping_evaluate_premulRGBF(col);
#endif

#ifdef USE_PREDIVIDE
	if (col[3] > 0.0 && col[3] < 1.0) {
		float inv_alpha = 1.0 / col[3];
		col[0] *= inv_alpha;
		col[1] *= inv_alpha;
		col[2] *= inv_alpha;
	}
#endif

	/* NOTE: This is true we only do de-premul here and NO premul
	 *       and the reason is simple -- opengl is always configured
	 *       for straight alpha at this moment
	 */

	vec4 result = OCIODisplay(col, lut3d_texture);

#ifdef USE_DITHER
	result = apply_dither(gl_TexCoord[0].st, result);
#endif

	gl_FragColor = result;
}
