
float fresnel_dielectric(vector Incoming, normal Normal, float eta)
{
	/* compute fresnel reflectance without explicitly computing
	   the refracted direction */
	float c = fabs(dot(Incoming, Normal));
	float g = eta * eta - 1 + c * c;
	float result;

	if (g > 0) {
		g = sqrt(g);
		float A = (g - c) / (g + c);
		float B = (c * (g + c) - 1) / (c * (g - c) + 1);
		result = 0.5 * A * A * (1 + B * B);
	}
	else
		result = 1.0;  /* TIR (no refracted component) */

	return result;
}

