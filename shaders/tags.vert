#version 330
uniform mat4 u_camera;
uniform vec3 u_loc;
uniform vec3 u_color;
uniform vec2 u_scale;
uniform vec2 u_offset;

layout(location=0) in vec2 a_offset;

out vec3 v_color;

void main()
{
	vec2 off = (a_offset+u_offset)*u_scale;
	gl_Position = u_camera * vec4(u_loc, 1.0) + vec4(off.xy, 0, 0);

	v_color = u_color;
}
