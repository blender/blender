
flat in vec4 color_flat;
flat in vec4 texCoord_rect;
noperspective in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D glyph;

const vec2 offsets4[4] = vec2[4](
	vec2(-0.5,  0.5), vec2( 0.5,  0.5),
	vec2(-0.5, -0.5), vec2(-0.5, -0.5)
);

const vec2 offsets16[16] = vec2[16](
	vec2(-1.5,  1.5), vec2(-0.5,  1.5), vec2( 0.5,  1.5), vec2( 1.5,  1.5),
	vec2(-1.5,  0.5), vec2(-0.5,  0.5), vec2( 0.5,  0.5), vec2( 1.5,  0.5),
	vec2(-1.5, -0.5), vec2(-0.5, -0.5), vec2( 0.5, -0.5), vec2( 1.5, -0.5),
	vec2(-1.5, -1.5), vec2(-0.5, -1.5), vec2( 0.5, -1.5), vec2( 1.5, -1.5)
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
			fragColor.a += sample_glyph_offset(texco, texel, offsets4[0]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets4[1]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets4[2]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets4[3]);
			fragColor.a *= (1.0 / 4.0);
		}
		else {
			/* 5x5 blur */
			/* Manual unroll for perf. (stupid glsl compiler) */
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 0]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 1]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 2]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 3]);

			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 4]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 5]) * 2.0;
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 6]) * 2.0;
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 7]);

			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 8]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[ 9]) * 2.0;
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[10]) * 2.0;
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[11]);

			fragColor.a += sample_glyph_offset(texco, texel, offsets16[12]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[13]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[14]);
			fragColor.a += sample_glyph_offset(texco, texel, offsets16[15]);
			fragColor.a *= (1.0 / 20.0);
		}
	}

	fragColor.a *= color_flat.a;
}
