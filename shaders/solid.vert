#version 430
layout(location=0) uniform mat4 u_camera;
layout(location=1) uniform sampler2D u_tex;
// No modelview matrix because complex meshes are for CHUMPS

// I don't think I *need* explicit locations on these, since graphics.c asks GL what location they got anyway.
// However, it is nice to have them explicit, as I do re-use attribute locations between programs that share
// this vertex shader.
layout(location=0) in vec3 a_loc;
layout(location=1) in vec3 a_color;
layout(location=2) in vec2 a_uv;

out vec3 v_color;
out vec2 v_uv;

void main()
{
	gl_Position = u_camera * vec4(a_loc, 1.0);
	v_color = a_color;
	v_uv = a_uv;
}
