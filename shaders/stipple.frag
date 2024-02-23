#version 330 core
in vec3 v_color;
layout(location = 0) out vec4 out_color;

void main()
{
	if (
		(mod(gl_FragCoord.x, 2) > 1) !=
		(mod(gl_FragCoord.y, 2) > 1)
	) {
		discard;
	}
	out_color = vec4(v_color,1.0);
}
