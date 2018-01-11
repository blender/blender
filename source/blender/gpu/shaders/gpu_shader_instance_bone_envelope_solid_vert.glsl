

/* This shader essentially operates in Object space, where it aligns given geometry with bone, scales it accordingly
 * to given radii, and then does usual basic solid operations.
 * Note that if one of head/tail radius is negative, it assumes it only works on the other end of the bone
 * (used to draw head/tail spheres). */


uniform mat4 ViewMatrix;
uniform mat4 ViewProjectionMatrix;


/* ---- Instanciated Attribs ---- */
in vec4 pos;  /* w encodes head (== 0.0f), tail (== 1.0f) or in-between. */

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

in float radius_head;
in float radius_tail;


out vec3 normal;
flat out vec4 finalColor;


void main()
{
	/* We get head/tail in object space. */
	vec4 head = InstanceModelMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec4 tail = InstanceModelMatrix * vec4(0.0f, 1.0f, 0.0f, 1.0f);

	/* We need rotation from bone mat, but not scaling. */
	mat3 bone_mat = mat3(InstanceModelMatrix);
	bone_mat[0] = normalize(bone_mat[0]);
	bone_mat[1] = normalize(bone_mat[1]);
	bone_mat[2] = normalize(bone_mat[2]);

	mat3 nor_mat = transpose(inverse(mat3(ViewMatrix) * bone_mat));

	/* Where does this comes from???? Don't know why, but is mandatory anyway... :/ */
	const float size = 2.0f;

	head.xyz *= size;
	tail.xyz *= size;

	bool head_only = (radius_tail < 0.0f);
	bool tail_only = (radius_head < 0.0f);
	/* == 0: head; == 1: tail; in-between: along bone. */
	float head_fac = head_only ? 0.0f : (tail_only ? 1.0f : pos.w);

	vec4 ob_pos;
	vec4 ob_bone_origin;
	float radius;

	/* head */
	if (head_fac <= 0.0f) {
		if (!head_only) {
			/* We are drawing the body itself, need to adjust start/end positions and radius! */
			vec3 bone_vec = tail.xyz - head.xyz;
			float len = length(bone_vec);

			if (len > (radius_head + radius_tail)) {
				float fac = (len - radius_head) / len;
				radius = fac * radius_head + (1.0f - fac) * radius_tail;
				bone_vec /= len;
				ob_bone_origin = vec4(head.xyz + bone_vec * radius_head * size, 1.0f);
			}
			else {
				radius = (radius_head + radius_tail) / 2.0f;
				ob_bone_origin = (head + tail) / 2.0f;
			}
		}
		else {
			radius = radius_head;
			ob_bone_origin = head;
		}
	}
	/* tail */
	else if (head_fac >= 1.0f) {
		if (!tail_only) {
			/* We are drawing the body itself, need to adjust start/end positions and radius! */
			vec3 bone_vec = tail.xyz - head.xyz;
			float len = length(bone_vec);

			if (len > (radius_head + radius_tail)) {
				float fac = (len - radius_tail) / len;
				radius = fac * radius_tail + (1.0f - fac) * radius_head;
				bone_vec /= len;
				ob_bone_origin = vec4(tail.xyz - bone_vec * radius_tail * size, 1.0f);
			}
			else {
				radius = (radius_head + radius_tail) / 2.0f;
				ob_bone_origin = (head + tail) / 2.0f;
			}
		}
		else {
			radius = radius_tail;
			ob_bone_origin = tail;
		}
	}
	/* Body of the bone */
#if 0  /* Note: not used currently! */
	else {
		float tail_fac = 1.0f - head_fac;
		radius = radius_head * head_fac + radius_tail * tail_fac;
		ob_bone_origin = head * head_fac + tail * tail_fac;
	}
#endif

	/* Yep, since input pos is unit sphere coordinates, it's also our normal. */
	vec3 nor = pos.xyz;
	ob_pos = pos * radius * size;
	ob_pos.xyz = bone_mat * ob_pos.xyz;
	ob_pos.w = 1.0f;

	gl_Position = ViewProjectionMatrix * (ob_pos + ob_bone_origin);
	normal = normalize(nor_mat * nor);
	finalColor = color;
}
