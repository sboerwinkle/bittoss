#version 430 core
layout(location=1) uniform sampler2D u_tex;
in vec3 v_color;
in vec2 v_uv;
layout(location = 0) out vec4 out_color;

void main()
{
	float tex = texture(u_tex, v_uv).r;
	tex = (tex*0.4)+0.8; // between 0.8-1.2
	out_color = vec4(tex*v_color, 1.0);
}
