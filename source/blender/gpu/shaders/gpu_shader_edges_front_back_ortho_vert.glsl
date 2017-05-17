
// Draw "fancy" wireframe, displaying front-facing, back-facing and
// silhouette lines differently.
// Mike Erwin, April 2015

uniform bool drawFront = true;
uniform bool drawBack = true;
uniform bool drawSilhouette = true;

uniform vec4 frontColor;
uniform vec4 backColor;
uniform vec4 silhouetteColor;

uniform vec3 eye; // direction we are looking

uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;

// normals of faces this edge joins (object coords)
in vec3 N1;
in vec3 N2;

flat out vec4 finalColor;

// TODO: in float angle; // [-pi .. +pi], + peak, 0 flat, - valley

// to discard an entire line, set both endpoints to nowhere
// and it won't produce any fragments
const vec4 nowhere = vec4(vec3(0.0), 1.0);

void main()
{
	bool face_1_front = dot(N1, eye) > 0.0;
	bool face_2_front = dot(N2, eye) > 0.0;

	vec4 position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	if (face_1_front && face_2_front) {
		// front-facing edge
		gl_Position = drawFront ? position : nowhere;
		finalColor = frontColor;
	}
	else if (face_1_front || face_2_front) {
		// exactly one face is front-facing, silhouette edge
		gl_Position = drawSilhouette ? position : nowhere;
		finalColor = silhouetteColor;
	}
	else {
		// back-facing edge
		gl_Position = drawBack ? position : nowhere;
		finalColor = backColor;
	}
}
