uniform float opacity;

in vec4 finalColor;
out vec4 fragColor;

/* Blend Mode goal:
 *     First multiply the foreground and background and then mix the result
 *     of that with the background based on a opacity value.
 *
 *     result = background * foreground * opacity + background * (1 - opacity)
 *            = background * (foreground * opacity + (1 - opacity))
 *                           <------------------------------------>
 *                                 computed in this shader
 *
 * Afterwards the background and the new foreground only have to be multiplied.
 */

void main()
{
	fragColor = finalColor * opacity + (1 - opacity);
	fragColor.a = finalColor.a;
}
