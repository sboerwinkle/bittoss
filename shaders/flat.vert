#version 330
uniform mat3 u_camera;
uniform vec2 u_offset;
uniform vec2 u_scale;
uniform vec3 u_color;

layout(location=0) in vec2 a_loc;
out vec3 v_color;

void main()
{
	vec2 v = u_offset + u_scale*a_loc;

	// This used to be one 4x4 matrix mult. IDK if this is more efficient, but this whole thing is for fun anyway
	vec3 pos = u_camera*vec3(v.xy, 1.0);
	gl_Position = vec4(pos.xy, 0, 1.0);

	v_color = u_color;
}
