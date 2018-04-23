
out vec4 fragColor;
uniform usampler2D objectId;

#define CROSS_OFFSET 1

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	ivec2 uvNE = ivec2(uv.x-CROSS_OFFSET, uv.y+CROSS_OFFSET);
	ivec2 uvNW = ivec2(uv.x-CROSS_OFFSET, uv.y-CROSS_OFFSET);
	ivec2 uvSE = ivec2(uv.x+CROSS_OFFSET, uv.y+CROSS_OFFSET);
	ivec2 uvSW = ivec2(uv.x+CROSS_OFFSET, uv.y-CROSS_OFFSET);

	uint oid = texelFetch(objectId, uv, 0).r;
	uint oidNE = texelFetch(objectId, uvNE, 0).r;
	uint oidNW = texelFetch(objectId, uvNW, 0).r;
	uint oidSE = texelFetch(objectId, uvSE, 0).r;
	uint oidSW = texelFetch(objectId, uvSW, 0).r;

	float result = 1.0;
	int same = 0;
	if (oid == oidNE) same ++;
	if (oid == oidNW) same ++;
	if (oid == oidSE) same ++;
	if (oid == oidSW) same ++;

	result = float(same) / 4.0;
	fragColor = vec4(0.0, 0.0, 0.0, 1.0-result);
}
