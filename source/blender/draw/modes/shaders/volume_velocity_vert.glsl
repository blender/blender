
uniform mat4 ModelViewProjectionMatrix;

uniform sampler3D velocityX;
uniform sampler3D velocityY;
uniform sampler3D velocityZ;
uniform float displaySize = 1.0;
uniform float slicePosition;
uniform int sliceAxis; /* -1 is no slice, 0 is X, 1 is Y, 2 is Z. */

flat out vec4 finalColor;

const vec3 corners[4] = vec3[4](
    vec3(0.0, 0.2, -0.5),
    vec3(-0.2 * 0.866, -0.2 * 0.5, -0.5),
    vec3(0.2 * 0.866, -0.2 * 0.5, -0.5),
    vec3(0.0, 0.0, 0.5)
);

const int indices[12] = int[12](0, 1,  1, 2,  2, 0,  0, 3,  1, 3,  2, 3);

/* Straight Port from BKE_defvert_weight_to_rgb()
 * TODO port this to a color ramp. */
vec3 weight_to_color(float weight)
{
	vec3 r_rgb = vec3(0.0);
	float blend = ((weight / 2.0) + 0.5);

	if (weight <= 0.25) {    /* blue->cyan */
		r_rgb.g = blend * weight * 4.0;
		r_rgb.b = blend;
	}
	else if (weight <= 0.50) {  /* cyan->green */
		r_rgb.g = blend;
		r_rgb.b = blend * (1.0 - ((weight - 0.25) * 4.0));
	}
	else if (weight <= 0.75) {  /* green->yellow */
		r_rgb.r = blend * ((weight - 0.50) * 4.0);
		r_rgb.g = blend;
	}
	else if (weight <= 1.0) {  /* yellow->red */
		r_rgb.r = blend;
		r_rgb.g = blend * (1.0 - ((weight - 0.75) * 4.0));
	}
	else {
		/* exceptional value, unclamped or nan,
		 * avoid uninitialized memory use */
		r_rgb = vec3(1.0, 0.0, 1.0);
	}

	return r_rgb;
}

mat3 rotation_from_vector(vec3 v)
{
	/* Add epsilon to avoid NaN. */
	vec3 N = normalize(v + 1e-8);
	vec3 UpVector = abs(N.z) < 0.99999 ? vec3(0.0,0.0,1.0) : vec3(1.0,0.0,0.0);
	vec3 T = normalize(cross(UpVector, N));
	vec3 B = cross(N, T);
	return mat3(T, B, N);
}

void main()
{
#ifdef USE_NEEDLE
	int cell = gl_VertexID / 12;
#else
	int cell = gl_VertexID / 2;
#endif

	ivec3 volume_size = textureSize(velocityX, 0);
	float voxel_size = 1.0 / float(max(max(volume_size.x, volume_size.y), volume_size.z));

	ivec3 cell_ofs = ivec3(0);
	ivec3 cell_div = volume_size;
	if (sliceAxis == 0) {
		cell_ofs.x = int(slicePosition * float(volume_size.x));
		cell_div.x = 1;
	}
	else if (sliceAxis == 1) {
		cell_ofs.y = int(slicePosition * float(volume_size.y));
		cell_div.y = 1;
	}
	else if (sliceAxis == 2) {
		cell_ofs.z = int(slicePosition * float(volume_size.z));
		cell_div.z = 1;
	}

	ivec3 cell_co;
	cell_co.x = cell % cell_div.x;
	cell_co.y = (cell / cell_div.x) % cell_div.y;
	cell_co.z = cell / (cell_div.x * cell_div.y);
	cell_co += cell_ofs;

	vec3 pos = (vec3(cell_co) + 0.5) / vec3(volume_size);
	pos = pos * 2.0 - 1.0;

	vec3 velocity;
	velocity.x = texelFetch(velocityX, cell_co, 0).r;
	velocity.y = texelFetch(velocityY, cell_co, 0).r;
	velocity.z = texelFetch(velocityZ, cell_co, 0).r;

	finalColor = vec4(weight_to_color(length(velocity)), 1.0);

#ifdef USE_NEEDLE
	mat3 rot_mat = rotation_from_vector(velocity);
	vec3 rotated_pos = rot_mat * corners[indices[gl_VertexID % 12]];
	pos += rotated_pos * length(velocity) * displaySize * voxel_size;
#else
	pos += (((gl_VertexID % 2) == 1) ? velocity : vec3(0.0)) * displaySize * voxel_size;
#endif

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
