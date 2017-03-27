
uniform int light_count;

struct LightData {
	vec4 positionAndInfluence;     /* w : InfluenceRadius */
	vec4 colorAndSpec;          /* w : Spec Intensity */
	vec4 spotDataRadiusShadow;  /* x : spot size, y : spot blend */
	vec4 rightVecAndSizex;         /* xyz: Normalized up vector, w: Lamp Type */
	vec4 upVecAndSizey;      /* xyz: Normalized right vector, w: Lamp Type */
	vec4 forwardVecAndType;     /* xyz: Normalized forward vector, w: Lamp Type */
};

/* convenience aliases */
#define lampColor     colorAndSpec.rgb
#define lampSpec      colorAndSpec.a
#define lampPosition  positionAndInfluence.xyz
#define lampInfluence positionAndInfluence.w
#define lampSizeX     rightVecAndSizex.w
#define lampSizeY     upVecAndSizey.w
#define lampRight     rightVecAndSizex.xyz
#define lampUp        upVecAndSizey.xyz
#define lampForward   forwardVecAndType.xyz
#define lampType      forwardVecAndType.w
#define lampSpotSize  spotDataRadiusShadow.x
#define lampSpotBlend spotDataRadiusShadow.y

layout(std140) uniform light_block {
	LightData   lights_data[MAX_LIGHT];
};

in vec3 worldPosition;
in vec3 worldNormal;

out vec4 fragColor;

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

vec3 light_diffuse(LightData ld, vec3 N, vec3 W, vec3 color) {
	vec3 light, wL, L;

	if (ld.lampType == SUN) {
		L = -ld.lampForward;
		light = color * direct_diffuse_sun(N, L) * ld.lampColor;
	}
	else {
		wL = ld.lampPosition - W;
		float dist = length(wL);
		light = color * direct_diffuse_point(N, wL / dist, dist) * ld.lampColor;
	}

	if (ld.lampType == SPOT) {
		float z = dot(ld.lampForward, wL);
		vec3 lL = wL / z;
		float x = dot(ld.lampRight, lL) / ld.lampSizeX;
		float y = dot(ld.lampUp, lL) / ld.lampSizeY;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.lampSpotSize) / ld.lampSpotBlend);

		light *= spotmask;
	}

	return light;
}

vec3 light_specular(LightData ld, vec3 V, vec3 N, vec3 T, vec3 B, vec3 spec, float roughness) {
	vec3 L = normalize(ld.lampPosition - worldPosition);
	vec3 light = L;

	return light;
}

void main() {
	vec3 n = normalize(worldNormal);
	vec3 diffuse = vec3(0.0);

	vec3 albedo = vec3(1.0, 1.0, 1.0);

	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		diffuse += light_diffuse(lights_data[i], n, worldPosition, albedo);
	}

	fragColor = vec4(diffuse,1.0);
}