#version 450

layout(std140, binding = 0) uniform Matrices{
	mat4 model;
	mat4 view;
	mat4 proj;
}matrices;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 outColor;
//layout(location = 1) out vec3 outNormal;
void main(){
	gl_Position = matrices.proj * matrices.view * matrices.model * vec4(inPos, 1.0);
	//outColor =  vec3(1.0f, 1.0f, 1.0f);
	outColor = inNormal;
}