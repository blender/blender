#define INFINITE 1000.0

uniform mat4 ModelViewProjectionMatrix;

uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);
uniform float lightDistance = 1e4;

in vec3 pos;

out VertexData {
	vec3 pos;           /* local position */
	vec4 frontPosition; /* final ndc position */
	vec4 backPosition;
} vData;

void main()
{
	vData.pos = pos;
	vData.frontPosition = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vData.backPosition  = ModelViewProjectionMatrix * vec4(pos + lightDirection * lightDistance, 1.0);
}
