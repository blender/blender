
/* Material Parameters packed in an UBO */
struct Material {
	vec4 prim_color;
	vec4 sec_color;
};

layout(std140) uniform material_block {
	Material shader_param[MAX_MATERIAL];
};

uniform int mat_id;

#define prim_color		shader_param[mat_id].prim_color.rgb
#define sec_color		shader_param[mat_id].sec_color.rgb

in vec4 radii;
out vec4 fragColor;

void main() {
	float dist = length(gl_PointCoord - vec2(0.5));

// transparent outside of point
// --- 0 ---
// smooth transition
// --- 1 ---
// pure outline color
// --- 2 ---
// smooth transition
// --- 3 ---
// pure point color
// ...
// dist = 0 at center of point

	float midStroke = 0.5 * (radii[1] + radii[2]);

	if (dist > midStroke) {
		fragColor.rgb = sec_color.rgb;
		fragColor.a = mix(1.0, 0.0, smoothstep(radii[1], radii[0], dist));
	}
	else {
		fragColor.rgb = mix(prim_color, sec_color, smoothstep(radii[3], radii[2], dist));
		fragColor.a = 1.0;
	}

	if (fragColor.a == 0.0) {
		discard;
	}
}
