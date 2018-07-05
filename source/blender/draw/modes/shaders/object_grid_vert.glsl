
/* Infinite grid
 * Clément Foucault */

uniform mat4 ViewProjectionMatrix;
uniform mat4 ProjectionMatrix;
uniform vec3 cameraPos;
uniform vec4 gridSettings;

#define gridDistance      gridSettings.x
#define gridResolution    gridSettings.y
#define gridScale         gridSettings.z
#define gridSubdiv        gridSettings.w

uniform int gridFlag;

#define PLANE_XY    (1 << 4)
#define PLANE_XZ    (1 << 5)
#define PLANE_YZ    (1 << 6)
#define CLIP_Z_POS  (1 << 7)
#define CLIP_Z_NEG  (1 << 8)

in vec3 pos;

void main()
{
	vec3 vert_pos, proj_camera_pos;

	/* Project camera pos to the needed plane */
	if ((gridFlag & PLANE_XY) != 0) {
		vert_pos = vec3(pos.x, pos.y, 0.0);
		proj_camera_pos = vec3(cameraPos.x, cameraPos.y, 0.0);
	}
	else if ((gridFlag & PLANE_XZ) != 0) {
		vert_pos = vec3(pos.x, 0.0, pos.y);
		proj_camera_pos = vec3(cameraPos.x, 0.0, cameraPos.z);
	}
	else {
		vert_pos = vec3(0.0, pos.x, pos.y);
		proj_camera_pos = vec3(0.0, cameraPos.y, cameraPos.z);
	}

	/* if persp */
	if (ProjectionMatrix[3][3] == 0.0) {
		vert_pos *= gridDistance * 2.0;
	}
	else {
		float viewdist = 1.0 / min(abs(ProjectionMatrix[0][0]), abs(ProjectionMatrix[1][1]));
		vert_pos *= viewdist * gridDistance * 2.0;
	}

	vec3 realPos = proj_camera_pos + vert_pos;

	/* Used for additional Z axis */
	if ((gridFlag & CLIP_Z_POS) != 0) {
		realPos.z = max(realPos.z, 0.0);
	}
	if ((gridFlag & CLIP_Z_NEG) != 0) {
		realPos.z = min(-realPos.z, 0.0);
	}

	gl_Position = ViewProjectionMatrix * vec4(realPos, 1.0);
}
