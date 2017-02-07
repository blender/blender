
uniform mat4 ViewProjectionMatrix;
uniform vec3 screen_vecs[2];
uniform float size;
uniform float pixel_size;

in vec2 pos;
in mat4 InstanceModelMatrix;

#define lamp_pos InstanceModelMatrix[3].xyz

float mul_project_m4_v3_zfac(in vec3 co)
{
	return (ViewProjectionMatrix[0][3] * co.x) +
	       (ViewProjectionMatrix[1][3] * co.y) +
	       (ViewProjectionMatrix[2][3] * co.z) + ViewProjectionMatrix[3][3];
}

void main()
{
	float pix_size = mul_project_m4_v3_zfac(lamp_pos) * pixel_size;
	vec3 screen_pos = screen_vecs[0].xyz * pos.x + screen_vecs[1].xyz * pos.y;
	gl_Position = ViewProjectionMatrix * vec4(lamp_pos + screen_pos * size * pix_size, 1.0);
}
