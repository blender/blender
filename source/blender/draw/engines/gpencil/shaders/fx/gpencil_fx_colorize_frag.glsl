uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;

uniform vec4 low_color;
uniform vec4 high_color;
uniform int mode;
uniform float factor;

out vec4 FragColor;

#define MODE_GRAYSCALE   0
#define MODE_SEPIA       1
#define MODE_BITONE      2
#define MODE_CUSTOM      3
#define MODE_TRANSPARENT 4

float get_luminance(vec4 color)
{
	float lum = (color.r * 0.2126) + (color.g * 0.7152) + (color.b * 0.723);
	return lum;
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);

	float stroke_depth = texelFetch(strokeDepth, uv.xy, 0).r;
	vec4 src_pixel= texelFetch(strokeColor, uv.xy, 0);
	float luminance = get_luminance(src_pixel);
	vec4 outcolor;
	
	/* is transparent */ 
	if (src_pixel.a == 0.0f) {
		discard;
	}
	
	switch(mode) {
		case MODE_GRAYSCALE:
			{
			outcolor = vec4(luminance, luminance, luminance, src_pixel.a);
			break;
			}
		case MODE_SEPIA:
			{
			float Red = (src_pixel.r * 0.393) + (src_pixel.g * 0.769) + (src_pixel.b * 0.189);
			float Green = (src_pixel.r * 0.349) + (src_pixel.g * 0.686) + (src_pixel.b * 0.168);
			float Blue = (src_pixel.r * 0.272) + (src_pixel.g * 0.534) + (src_pixel.b * 0.131);
			outcolor = vec4(Red, Green, Blue, src_pixel.a);
			break;
			}
		case MODE_BITONE:
			{
			if (luminance <= factor) {
				outcolor = low_color;
			}
			else {
				outcolor = high_color;
			}
			break;
			}
		case MODE_CUSTOM:
			{
			/* if below umbral, force custom color */
			if (luminance <= factor) {
				outcolor = low_color;
			}
			else {
				outcolor = vec4(luminance * low_color.r, luminance * low_color.b, luminance * low_color.b, src_pixel.a);
			}
			break;
			}
		case MODE_TRANSPARENT:
			{
			outcolor = vec4(src_pixel.rgb, src_pixel.a * factor);
			break;
			}
		default:
			{
			outcolor = src_pixel;
			}
	
	}

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
