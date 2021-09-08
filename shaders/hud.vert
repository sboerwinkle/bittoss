#version 110
uniform float u_aspect;
uniform vec2 u_offset;
uniform float u_scale;
uniform mat4 u_lens;//This is the gl_perspective matrix equivalent
uniform vec4 u_baseColor;

attribute vec2 a_loc;
varying vec4 color;
void main()
{
	color = u_baseColor;
	gl_Position = u_lens*vec4(u_offset.x+u_scale*a_loc.x, u_offset.y+u_aspect*u_scale*a_loc.y, 0.0, 1.0);
}
