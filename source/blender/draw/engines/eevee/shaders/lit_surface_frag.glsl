
uniform int light_count;
uniform vec3 cameraPos;
uniform vec3 eye;
uniform mat4 ProjectionMatrix;

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

vec3 light_diffuse(LightData ld, vec3 N, vec3 W, vec3 wL, vec3 L, float Ldist, vec3 color)
{
	vec3 light;

	if (ld.lampType == SUN) {
		L = -ld.lampForward;
		light = color * direct_diffuse_sun(N, L) * ld.lampColor;
	}
	else if (ld.lampType == AREA) {
		light = color * direct_diffuse_rectangle(W, N, L, Ldist,
		                                         ld.lampRight, ld.lampUp, ld.lampForward,
		                                         ld.lampSizeX, ld.lampSizeY) * ld.lampColor;
	}
	else {
		// light = color * direct_diffuse_point(N, L, Ldist) * ld.lampColor;
		light = color * direct_diffuse_sphere(N, L, Ldist, ld.lampSizeX) * ld.lampColor;
	}

	return light;
}

vec3 light_specular(
        LightData ld, vec3 V, vec3 N, vec3 W, vec3 wL,
        vec3 L, float Ldist, vec3 spec, float roughness)
{
	vec3 light;

	if (ld.lampType == SUN) {
		L = -ld.lampForward;
		light = spec * direct_ggx_point(N, L, V, roughness) * ld.lampColor;
	}
	else if (ld.lampType == AREA) {
		light = spec * direct_ggx_rectangle(W, N, L, V, Ldist, ld.lampRight, ld.lampUp, ld.lampForward,
		                                    ld.lampSizeX, ld.lampSizeY, roughness) * ld.lampColor;
	}
	else {
		light = spec * direct_ggx_sphere(N, L, V, Ldist, ld.lampSizeX, roughness) * ld.lampColor;
	}

	return light;
}

float light_visibility(
        LightData ld, vec3 V, vec3 N, vec3 W, vec3 wL, vec3 L, float Ldist)
{
	float vis = 1.0;

	if (ld.lampType == SPOT) {
		float z = dot(ld.lampForward, wL);
		vec3 lL = wL / z;
		float x = dot(ld.lampRight, lL) / ld.lampSizeX;
		float y = dot(ld.lampUp, lL) / ld.lampSizeY;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.lampSpotSize) / ld.lampSpotBlend);

		vis *= spotmask;
	}
	else if (ld.lampType == AREA) {
		vis *= step(0.0, -dot(L, ld.lampForward));
	}

	return vis;
}

void main()
{
	vec3 N = normalize(worldNormal);

	vec3 V;
	if (ProjectionMatrix[3][3] == 0.0) {
		V = normalize(cameraPos - worldPosition);
	}
	else {
		V = normalize(eye);
	}
	vec3 radiance = vec3(0.0);

	vec3 albedo = vec3(1.0, 1.0, 1.0);
	vec3 specular = mix(vec3(0.03), vec3(1.0), pow(max(0.0, 1.0 - dot(N,V)), 5.0));

	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		vec3 wL = lights_data[i].lampPosition - worldPosition;
		float dist = length(wL);
		vec3 L = wL / dist;

		float vis = light_visibility(lights_data[i], V, N, worldPosition, wL, L, dist);
		vec3 spec = light_specular(lights_data[i], V, N, worldPosition, wL, L, dist, vec3(1.0), .2);
		vec3 diff = light_diffuse(lights_data[i], N, worldPosition, wL, L, dist, albedo);

		radiance += vis * (diff + spec);
	}

	fragColor = vec4(radiance, 1.0);
}