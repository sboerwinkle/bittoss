#version 330
uniform mat4 u_camera;
uniform vec2 u_offset;
uniform vec2 u_scale;
uniform vec3 u_color;

layout(location=0) in vec2 a_loc;
out vec3 v_color;

void main()
{
	vec2 v = u_offset + u_scale*a_loc;
	gl_Position = u_camera*vec4(v.xy, 0.0, 1.0);
	v_color = u_color;
}
