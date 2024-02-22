#version 330
uniform mat4 u_camera;
// No modelview matrix because complex meshes are for CHUMPS

in vec3 a_loc;
in vec3 a_color;

out vec3 v_color;

void main()
{
	gl_Position = u_camera * vec4(a_loc, 1.0);
	v_color = a_color;
}
