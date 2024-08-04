#version 330
uniform mat4 u_camera;
uniform vec3 u_loc;
uniform vec3 u_color;
uniform float u_scale_x;
uniform float u_scale_y;

in vec2 a_offset;

out vec3 v_color;

void main()
{
	vec4 off = vec4(u_scale_x*a_offset.x, u_scale_y*a_offset.y, 0, 0);
	gl_Position = u_camera * vec4(u_loc, 1.0) + off;

	v_color = u_color;
}
