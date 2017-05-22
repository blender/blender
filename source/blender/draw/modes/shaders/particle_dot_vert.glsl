
/* Material Parameters packed in an UBO */
struct Material {
	vec4 prim_color;
	vec4 sec_color;
};

layout(std140) uniform material_block {
	Material shader_param[MAX_MATERIAL];
};

uniform int mat_id;

#define size		shader_param[mat_id].sec_color.w

uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
out vec4 radii;

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = size;

	// calculate concentric radii in pixels
	float radius = 0.5 * size;

	// start at the outside and progress toward the center
	radii[0] = radius;
	radii[1] = radius - 1.0;
	radii[2] = radius - 1.0;
	radii[3] = radius - 2.0;

	// convert to PointCoord units
	radii /= size;
}
