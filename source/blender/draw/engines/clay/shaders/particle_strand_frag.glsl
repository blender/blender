
uniform vec3 light;

/* Material Parameters packed in an UBO */
struct Material {
	vec4 one;
	vec4 two;
	vec4 hair_diffuse_color;
	vec4 hair_specular_color;
};

layout(std140) uniform material_block {
	Material shader_param[MAX_MATERIAL];
};

uniform int mat_id;

#define world				shader_param[mat_id].one.x
#define diffuse				shader_param[mat_id].one.y
#define specular			shader_param[mat_id].one.z
#define hardness			shader_param[mat_id].one.w
#define randomicity			shader_param[mat_id].two.x
#define diffColor			shader_param[mat_id].hair_diffuse_color
#define specColor			shader_param[mat_id].hair_specular_color

in vec3 normal;
in vec3 viewPosition;
flat in float colRand;
out vec4 fragColor;

void main()
{
	vec3 normal = normalize(normal);
	vec3 specVec = normalize(normalize(light) + normalize(viewPosition));
	float specCos = dot(specVec, normal);
	float diffCos = dot(normalize(light), normal);
	float maxChan = max(max(diffColor.r, diffColor.g), diffColor.b);
	float diff;
	float spec;
	diff = world; /* world */
	diff += sqrt(1.0 - diffCos*diffCos) * diffuse; /* diffuse */
	spec = pow(1.0 - abs(specCos), hardness) * specular; /* specular */
	fragColor = (diffColor - (colRand * maxChan * randomicity)) * diff; /* add diffuse */
	fragColor += specColor * spec; /* add specular */
	fragColor = clamp(fragColor, 0.0, 1.0);
}
