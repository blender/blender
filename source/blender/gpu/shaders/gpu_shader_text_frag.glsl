
flat in vec4 color_flat;
flat in vec4 texCoord_rect;
noperspective in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D glyph;

const vec2 offsets9[9] = vec2[9](
	vec2(-1.0, -1.0), vec2( 0.0, -1.0), vec2( 1.0, -1.0),
	vec2(-1.0,  0.0), vec2( 0.0,  0.0), vec2( 1.0,  0.0),
	vec2(-1.0,  1.0), vec2( 0.0,  1.0), vec2( 1.0,  1.0)
);

const vec2 offsets25[25] = vec2[25](
	vec2(-2.0, -2.0), vec2(-1.0, -2.0), vec2( 0.0, -2.0), vec2( 1.0, -2.0), vec2( 2.0, -2.0),
	vec2(-2.0, -1.0), vec2(-1.0, -1.0), vec2( 0.0, -1.0), vec2( 1.0, -1.0), vec2( 2.0, -1.0),
	vec2(-2.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  0.0), vec2( 1.0,  0.0), vec2( 2.0,  0.0),
	vec2(-2.0,  1.0), vec2(-1.0,  1.0), vec2( 0.0,  1.0), vec2( 1.0,  1.0), vec2( 2.0,  1.0),
	vec2(-2.0,  2.0), vec2(-1.0,  2.0), vec2( 0.0,  2.0), vec2( 1.0,  2.0), vec2( 2.0,  2.0)
);

const float weights9[9] = float[9](
	1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
	2.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0,
	1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0
);

const float weights25[25] = float[25](
	1.0 / 60.0, 1.0 / 60.0, 2.0 / 60.0, 1.0 / 60.0, 1.0 / 60.0,
	1.0 / 60.0, 3.0 / 60.0, 5.0 / 60.0, 3.0 / 60.0, 1.0 / 60.0,
	2.0 / 60.0, 5.0 / 60.0, 8.0 / 60.0, 5.0 / 60.0, 2.0 / 60.0,
	1.0 / 60.0, 3.0 / 60.0, 5.0 / 60.0, 3.0 / 60.0, 1.0 / 60.0,
	1.0 / 60.0, 1.0 / 60.0, 2.0 / 60.0, 1.0 / 60.0, 1.0 / 60.0
);

#define sample_glyph_offset(texco, texel, ofs) texture(glyph, texco + ofs * texel).r

void main()
{
	// input color replaces texture color
	fragColor.rgb = color_flat.rgb;

	vec2 texel = 1.0 / vec2(textureSize(glyph, 0));
	vec2 texco = mix(abs(texCoord_rect.xy), abs(texCoord_rect.zw), texCoord_interp);

	// modulate input alpha & texture alpha
	if (texCoord_rect.x > 0) {
		fragColor.a = texture(glyph, texco).r;
	}
	else {
		fragColor.a = 0.0;

		if (texCoord_rect.w > 0) {
			/* 3x3 blur */
			/* Manual unroll for perf. (stupid glsl compiler) */
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[0]) * weights9[0];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[1]) * weights9[1];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[2]) * weights9[2];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[3]) * weights9[3];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[4]) * weights9[4];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[5]) * weights9[5];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[6]) * weights9[6];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[7]) * weights9[7];
			fragColor.a += sample_glyph_offset(texco, texel, offsets9[8]) * weights9[8];
		}
		else {
			/* 5x5 blur */
			/* Manual unroll for perf. (stupid glsl compiler) */
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 0]) * weights25[ 0];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 1]) * weights25[ 1];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 2]) * weights25[ 2];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 3]) * weights25[ 3];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 4]) * weights25[ 4];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 5]) * weights25[ 5];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 6]) * weights25[ 6];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 7]) * weights25[ 7];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 8]) * weights25[ 8];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[ 9]) * weights25[ 9];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[10]) * weights25[10];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[11]) * weights25[11];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[12]) * weights25[12];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[13]) * weights25[13];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[14]) * weights25[14];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[15]) * weights25[15];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[16]) * weights25[16];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[17]) * weights25[17];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[18]) * weights25[18];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[19]) * weights25[19];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[20]) * weights25[20];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[21]) * weights25[21];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[22]) * weights25[22];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[23]) * weights25[23];
			fragColor.a += sample_glyph_offset(texco, texel, offsets25[24]) * weights25[24];
		}
	}

	fragColor.a *= color_flat.a;
}
