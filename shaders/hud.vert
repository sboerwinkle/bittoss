#version 330
uniform mat4 u_camera;
uniform vec2 u_offset;
uniform float u_scale_x;
uniform float u_scale_y;
uniform vec3 u_color;

in vec2 a_loc;
out vec3 v_color;

void main()
{
	gl_Position = u_camera*vec4(u_offset.x+u_scale_x*a_loc.x, u_offset.y+u_scale_y*a_loc.y, 0.0, 1.0);
	v_color = u_color;
}
