
flat in int cellOffset;
in vec3 worldNormal;

out vec4 FragColor;

void main()
{
	IrradianceData ir_data = load_irradiance_cell(cellOffset, worldNormal);
	FragColor = vec4(compute_irradiance(worldNormal, ir_data), 1.0);
}
