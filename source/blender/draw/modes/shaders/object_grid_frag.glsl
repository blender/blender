
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

#define GRID_LINE_SMOOTH 1.15

float grid(vec2 uv, vec2 fwidthUvs, float grid_size)
{
	float half_size = grid_size / 2.0;
	/* triangular wave pattern, amplitude is [0, grid_size] */
	vec2 grid_domain = abs(mod(uv + half_size, grid_size) - half_size);
	/* modulate by the absolute rate of change of the uvs
	 * (make lines have the same width under perspective) */
	grid_domain /= fwidthUvs;

	/* collapse waves and normalize */
	grid_domain.x = min(grid_domain.x, grid_domain.y) / grid_size;

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
	vec2 fwidthUvs = fwidth(wPos.xy);

	float blend, lvl, fade;

	/* if persp */
	if (ProjectionMatrix[3][3] == 0.0) {
		float dist = distance(cameraPos, wPos);
		float log2dist = -log2(dist / (2.0 * gridDistance));

		blend = fract(log2dist / gridResolution);
		lvl = floor(log2dist / gridResolution);
		fade = 1.0 - smoothstep(0.0, gridDistance, dist - gridDistance);
	}
	else {
		/* todo find a better way */
		blend = 0.0;
		lvl = 0.0;
		fade = 1.0;
	}

	/* from smallest to biggest */
	float scaleA = gridScale * pow(gridSubdiv, min(-lvl + 1.0, 1.0));
	float scaleB = gridScale * pow(gridSubdiv, min(-lvl + 2.0, 1.0));
	float scaleC = gridScale * pow(gridSubdiv, min(-lvl + 3.0, 1.0));

	float gridA = grid(wPos.xy, fwidthUvs, scaleA);
	float gridB = grid(wPos.xy, fwidthUvs, scaleB);
	float gridC = grid(wPos.xy, fwidthUvs, scaleC);

	float xAxis = axis(wPos.y, fwidthUvs.y, 0.1); /* Swapped */
	float yAxis = axis(wPos.x, fwidthUvs.x, 0.1); /* Swapped */

	FragColor = vec4(colorGrid.rgb, gridA * blend);
	FragColor = mix(FragColor, vec4(mix(colorGrid.rgb, colorGridEmphasise.rgb, blend), 1.0), gridB);
	FragColor = mix(FragColor, vec4(colorGridEmphasise.rgb, 1.0), gridC);

	FragColor = mix(FragColor, colorGridAxisX, xAxis);
	FragColor = mix(FragColor, colorGridAxisY, yAxis);
	FragColor.a *= fade;
}