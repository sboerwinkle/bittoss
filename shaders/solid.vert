#version 330
uniform mat4 u_camera;
// No modelview matrix because complex meshes are for CHUMPS

// I don't think I *need* explicit locations on these, since graphics.c asks GL what location they got anyway.
// However, it is nice to have them explicit, as I do re-use attribute locations between programs that share
// this vertex shader.
layout(location=0) in vec3 a_loc;
layout(location=1) in vec3 a_color;

out vec3 v_color;

void main()
{
	gl_Position = u_camera * vec4(a_loc, 1.0);
	v_color = a_color;
}
