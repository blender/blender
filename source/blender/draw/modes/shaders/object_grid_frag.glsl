
/* Infinite grid
 * Clément Foucault */

in vec3 wPos;

out vec4 FragColor;

uniform mat4 ProjectionMatrix;
uniform vec3 cameraPos;
uniform vec4 gridSettings;

#define gridDistance      gridSettings.x
#define gridResolution    gridSettings.y
#define gridScale         gridSettings.z
#define gridSubdiv        gridSettings.w

uniform int gridFlag;

#define AXIS_X    (1 << 0)
#define AXIS_Y    (1 << 1)
#define AXIS_Z    (1 << 2)
#define GRID      (1 << 3)
#define PLANE_XY  (1 << 4)
#define PLANE_XZ  (1 << 5)
#define PLANE_YZ  (1 << 6)

#define GRID_LINE_SMOOTH 1.15

float grid(vec3 uv, vec3 fwidthCos, float grid_size)
{
	float half_size = grid_size / 2.0;
	/* triangular wave pattern, amplitude is [0, grid_size] */
	vec3 grid_domain = abs(mod(uv + half_size, grid_size) - half_size);
	/* modulate by the absolute rate of change of the uvs
	 * (make lines have the same width under perspective) */
	grid_domain /= fwidthCos;

	/* collapse waves and normalize */
	grid_domain.x = min(grid_domain.x, min(grid_domain.y, grid_domain.z)) / grid_size;

	return 1.0 - smoothstep(0.0, GRID_LINE_SMOOTH / grid_size, grid_domain.x);
}

float axis(float u, float fwidthU, float line_size)
{
	float axis_domain = abs(u);
	/* modulate by the absolute rate of change of the uvs
	 * (make line have the same width under perspective) */
	axis_domain /= fwidthU;

	return 1.0 - smoothstep(0.0, GRID_LINE_SMOOTH, axis_domain - line_size);
}

void main()
{
	vec3 fwidthCos = fwidth(wPos);

	float fade, grid_res;
	/* if persp */
	if (ProjectionMatrix[3][3] == 0.0) {
		float dist = distance(cameraPos, wPos);
		float dist_norm = dist / (2.0 * gridDistance);
		grid_res = log(dist * gridResolution) / log(gridSubdiv);
		fade = 1.0 - smoothstep(0.0, gridDistance, dist - gridDistance);
	}
	else {
		float dist = abs(gl_FragCoord.z * 2.0 - 1.0);
		grid_res = log(gridResolution) / log(gridSubdiv);
		fade = 1.0 - smoothstep(0.0, 0.5, dist - 0.5);
	}

	/* fix division by 0 (log(1) = 0) */
	if (gridSubdiv == 1.0) {
		grid_res = 0.0;
	}

	if ((gridFlag & GRID) > 0) {
		float blend = fract(-max(grid_res, 0.0));
		float lvl = floor(grid_res);

		/* from smallest to biggest */
		float scaleA = gridScale * pow(gridSubdiv, max(lvl - 1.0, 0.0));
		float scaleB = gridScale * pow(gridSubdiv, max(lvl + 0.0, 0.0));
		float scaleC = gridScale * pow(gridSubdiv, max(lvl + 1.0, 1.0));

		float gridA = grid(wPos, fwidthCos, scaleA);
		float gridB = grid(wPos, fwidthCos, scaleB);
		float gridC = grid(wPos, fwidthCos, scaleC);

		FragColor = vec4(colorGrid.rgb, gridA * blend);
		FragColor = mix(FragColor, vec4(mix(colorGrid.rgb, colorGridEmphasise.rgb, blend), 1.0), gridB);
		FragColor = mix(FragColor, vec4(colorGridEmphasise.rgb, 1.0), gridC);
	}
	else {
		FragColor = vec4(colorGrid.rgb, 0.0);
	}

	if ((gridFlag & AXIS_X) > 0) {
		float xAxis;
		if ((gridFlag & AXIS_Y) > 0) {
			xAxis = axis(wPos.y, fwidthCos.y, 0.1);
		}
		else {
			xAxis = axis(wPos.z, fwidthCos.z, 0.1);
		}
		FragColor = mix(FragColor, colorGridAxisX, xAxis);
	}
	if ((gridFlag & AXIS_Y) > 0) {
		float yAxis;
		if ((gridFlag & AXIS_X) > 0) {
			yAxis = axis(wPos.x, fwidthCos.x, 0.1);
		}
		else {
			yAxis = axis(wPos.z, fwidthCos.z, 0.1);
		}
		FragColor = mix(FragColor, colorGridAxisY, yAxis);
	}
	if ((gridFlag & AXIS_Z) > 0) {
		float zAxis;
		if ((gridFlag & AXIS_Y) > 0) {
			zAxis = axis(wPos.y, fwidthCos.y, 0.1);
		}
		else {
			zAxis = axis(wPos.x, fwidthCos.x, 0.1);
		}
		FragColor = mix(FragColor, colorGridAxisZ, zAxis);
	}

	FragColor.a *= fade;
}