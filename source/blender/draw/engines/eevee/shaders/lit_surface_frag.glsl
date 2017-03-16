
uniform int light_count;

struct LightData {
	vec4 position;
	vec4 colorAndSpec; /* w : Spec Intensity */
	vec4 spotAndAreaData;
};

layout(std140) uniform light_block {
	LightData   lights_data[MAX_LIGHT];
};

in vec3 worldPosition;
in vec3 worldNormal;

out vec4 fragColor;

void main() {
	vec3 n = normalize(worldNormal);
	vec3 diffuse = vec3(0.0);

	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];
		vec3 l = normalize(ld.position.xyz - worldPosition);
		diffuse += max(0.0, dot(l, n)) * ld.colorAndSpec.rgb / 3.14159;
	}

	fragColor = vec4(diffuse,1.0);
}