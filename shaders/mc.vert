#version 450

// layout(binding = 0) uniform Matrices{
// 	mat4 model;
// 	mat4 view;
// 	mat4 proj;
// }MVP;
layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outColor;

void main(){
	// gl_Position = MVP.proj * MVP.view * MVP.model * vec4(inPos, 1.0);
	gl_Position=vec4(inPos, 1.0);
	outColor =  vec3(inPos+0.5);
}