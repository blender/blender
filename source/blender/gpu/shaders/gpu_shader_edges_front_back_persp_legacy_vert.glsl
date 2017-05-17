
// Draw "fancy" wireframe, displaying front-facing, back-facing and
// silhouette lines differently.
// Mike Erwin, April 2015

// After working with this shader a while, convinced we should make
// separate shaders for perpective & ortho. (Oct 2016)

// This shader is an imperfect stepping stone until all platforms are
// ready for geometry shaders.

// Due to perspective, the line segment's endpoints might disagree on
// whether the adjacent faces are front facing. Need to use a geometry
// shader or pass in an extra position attribute (the other endpoint)
// to do this properly.

uniform bool drawFront = true;
uniform bool drawBack = true;
uniform bool drawSilhouette = true;

uniform vec4 frontColor;
uniform vec4 backColor;
uniform vec4 silhouetteColor;

uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;

in vec3 pos;

// normals of faces this edge joins (object coords)
in vec3 N1;
in vec3 N2;

flat out vec4 finalColor;

// TODO: in float angle; // [-pi .. +pi], + peak, 0 flat, - valley

// to discard an entire line, set its color to invisible
// (must have GL_BLEND enabled, or discard in fragment shader)
const vec4 invisible = vec4(0.0);

bool front(vec3 N)
{
	vec4 xformed = ModelViewMatrix * vec4(pos, 1.0);
	return dot(NormalMatrix * N, normalize(-xformed.xyz)) > 0.0;
}

void main()
{
	bool face_1_front = front(N1);
	bool face_2_front = front(N2);

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	if (face_1_front && face_2_front) {
		// front-facing edge
		finalColor = drawFront ? frontColor : invisible;
	}
	else if (face_1_front || face_2_front) {
		// exactly one face is front-facing, silhouette edge
		finalColor = drawSilhouette ? silhouetteColor : invisible;
	}
	else {
		// back-facing edge
		finalColor = drawBack ? backColor : invisible;
	}
}
