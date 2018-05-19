#define EPSILON 0.0001
#define INFINITE 10000.0

uniform mat4 ModelMatrixInverse;
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ViewProjectionMatrix;
uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in vec3 pos;

out VertexData {
	vec3 pos;           /* local position */
	vec4 frontPosition; /* final ndc position */
	vec4 backPosition;
} vData;

void main()
{
	/* TODO precompute light_direction */
	vec3 light_direction = mat3(ModelMatrixInverse) * lightDirection;
	vData.pos = pos;
	vData.frontPosition = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vData.backPosition  = ModelViewProjectionMatrix * vec4(pos + light_direction * INFINITE, 1.0);
}
