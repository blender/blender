#define EPSILON 0.000001
#define INFINITE 100.0

uniform mat4 ModelMatrixInverse;
uniform mat4 ModelViewProjectionMatrix;
uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in vec4 pos;

out VertexData {
	flat vec4 lightDirectionMS;
	vec4 frontPosition;
	vec4 backPosition;
} vertexData;

void main()
{
	gl_Position = pos;
	vertexData.lightDirectionMS = normalize(ModelMatrixInverse * vec4(lightDirection, 0.0));
	vertexData.lightDirectionMS.w = 0.0;
	vertexData.frontPosition = ModelViewProjectionMatrix * (pos + vertexData.lightDirectionMS * EPSILON);
	vertexData.backPosition = ModelViewProjectionMatrix * (pos + vertexData.lightDirectionMS * INFINITE);
}
