
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

flat in int finalAxis;

out vec4 fragColor;

void main()
{
	if (finalAxis == -1) {
		fragColor.rgb = prim_color;
		fragColor.a = 1.0;
	}
	else {
		vec4 col = vec4(0.0);
		col[finalAxis] = 1.0;
		col.a = 1.0;
		fragColor = col;
	}
}
