#version 430
layout(location=0) uniform mat4 u_camera;
// No modelview matrix because complex meshes are for CHUMPS
//   (2025-05-24: I may be adding a modelview matrix...)

// skip location=1 b/c it's used by the texture in the frag shader
layout(location=2) uniform vec3 u_offset;
layout(location=3) uniform vec3 u_scale;
layout(location=4) uniform vec3 u_color;

// I don't think I *need* explicit locations on these, since graphics.c asks GL what location they got anyway.
// However, it is nice to have them explicit, as I do re-use attribute locations between programs that share
// this vertex shader.
layout(location=0) in vec3 a_loc;
layout(location=1) in float a_bright;
layout(location=2) in mat3x2 a_tex_mat;

layout(location=0) out vec3 v_color;
layout(location=1) out vec2 v_uv;

void main()
{
	vec3 scaled_pos = u_scale*a_loc;
	gl_Position = u_camera * vec4(u_offset + scaled_pos, 1.0);
	v_color = a_bright * u_color;
	v_uv = a_tex_mat * scaled_pos;
}
