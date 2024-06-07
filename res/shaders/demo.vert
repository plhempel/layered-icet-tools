#version 330 core


in vec3 center;

uniform mat4 mvp_mat;


void main() {
	gl_Position = vec4(center, 1) * mvp_mat;
	}
