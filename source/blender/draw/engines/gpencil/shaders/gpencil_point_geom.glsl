uniform mat4 ModelViewProjectionMatrix;
uniform vec2 Viewport;
uniform int xraymode;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 finalColor[1];
in float finalThickness[1];
in vec2 finaluvdata[1];

out vec4 mColor;
out vec2 mTexCoord;

#define GP_XRAY_FRONT 0
#define GP_XRAY_3DSPACE 1
#define GP_XRAY_BACK  2

/* project 3d point to 2d on screen space */
vec2 toScreenSpace(vec4 vertex)
{
	return vec2(vertex.xy / vertex.w) * Viewport;
}

/* get zdepth value */
float getZdepth(vec4 point)
{
	if (xraymode == GP_XRAY_FRONT) {
		return 0.0;
	}
	if (xraymode == GP_XRAY_3DSPACE) {
		return (point.z / point.w);
	}
	if  (xraymode == GP_XRAY_BACK) {
		return 0.999999;
	}

	/* in front by default */
	return 0.0;
}

vec2 rotateUV(vec2 uv, float angle)
{
	/* translate center of rotation to the center of texture */
	vec2 new_uv = uv - vec2(0.5f, 0.5f);
	vec2 rot_uv;
	rot_uv.x = new_uv.x * cos(angle) - new_uv.y * sin(angle);
	rot_uv.y = new_uv.y * cos(angle) + new_uv.x * sin(angle);
	return rot_uv + vec2(0.5f, 0.5f);
}

void main(void)
{
	/* receive 4 points */
	vec4 P0 = gl_in[0].gl_Position;
	vec2 sp0 = toScreenSpace(P0);

	float size = finalThickness[0];
	float aspect = 1.0;
	/* generate the triangle strip */
	mTexCoord = rotateUV(vec2(0, 1), finaluvdata[0].y);
	mColor = finalColor[0];
	gl_Position = vec4(vec2(sp0.x - size, sp0.y + size * aspect) / Viewport, getZdepth(P0), 1.0);
	EmitVertex();

	mTexCoord = rotateUV(vec2(0, 0), finaluvdata[0].y);
	mColor = finalColor[0];
	gl_Position = vec4(vec2(sp0.x - size, sp0.y - size * aspect) / Viewport, getZdepth(P0), 1.0);
	EmitVertex();

	mTexCoord = rotateUV(vec2(1, 1), finaluvdata[0].y);
	mColor = finalColor[0];
	gl_Position = vec4(vec2(sp0.x + size, sp0.y + size * aspect) / Viewport, getZdepth(P0), 1.0);
	EmitVertex();

	mTexCoord = rotateUV(vec2(1, 0), finaluvdata[0].y);
	mColor = finalColor[0];
	gl_Position = vec4(vec2(sp0.x + size, sp0.y - size * aspect) / Viewport, getZdepth(P0), 1.0);
	EmitVertex();

	EndPrimitive();
}
