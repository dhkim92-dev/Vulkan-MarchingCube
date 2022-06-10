#ifndef __DH_TOOLS_H__
#define __DH_TOOLS_H__
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace GraphicTools
{
class Camera{
private :
	struct{
		float fov, znear, zfar, aspect;
	}perspective;

	struct{
		bool updated = false;
		bool flipY = false;
	}status;

	struct{
		float movement = 1.0;
		float rotation = 1.0f;
	}speed;

	struct {
		float right;
		float up;
	}angles;
	
	glm::vec3 position; // camera position in world space. 
	glm::vec3 rotation; // camera pitch, yaw, roll
	glm::quat orientation; // camera view direction

	void updateViewMatrix(){
		glm::quat yaw = glm::angleAxis(glm::radians(-angles.right), glm::vec3(0.0f, 1.0f, 0.0f) ); // yaw
		glm::quat pitch = glm::angleAxis(glm::radians(angles.up), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
		orientation = yaw * pitch;
		glm::mat4 rotation_matrix = glm::mat4_cast( glm::conjugate(orientation) );
		glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), -position);
		matrices.view = rotation_matrix * translation_matrix;
	}

public : 
	struct{
		glm::mat4 proj;
		glm::mat4 view;
	}matrices;


	Camera(glm::vec3 _position, glm::vec3 target){
		init(_position, target);
		//updateViewMatrix();
	}

	Camera(){};

	void setPerspective(float fov, float aspect, float znear, float zfar){
		perspective.fov = fov;
		perspective.aspect = aspect;
		perspective.znear = znear;
		perspective.zfar = zfar;

		matrices.proj = glm::perspective(fov, aspect, znear, zfar);

		if(status.flipY){
			matrices.proj[1][1] *= -1.0f;
		}
	}


	void init(glm::vec3 _position, glm::vec3 target){
		position = _position;
		glm::vec3 look = glm::normalize(target - _position);
		orientation = glm::quat(0, look);
		angles.right = 0.0f;
		angles.up = 0.0f;
	}

	void setRotation(glm::vec3 rotate){
		rotation = rotate;
		updateViewMatrix();
	}

	void setMovementSpeed(float value){
		speed.movement = value;
	}

	void setRotateSpeed(float value){
		speed.rotation = value;
	}

	void setFlipY(bool flip){
		status.flipY = flip;
	}

	void rotate(float angle, glm::vec3 vec){
		orientation *= glm::angleAxis(angle, vec * orientation);
		updateViewMatrix();
	}

	void rotate(float angle, float x, float y, float z){
		rotate(angle, glm::vec3(x,y, z));
	}

	void translate(glm::vec3 delta){
		position += delta;
		updateViewMatrix();
	}

	void translate(float x, float y, float z){
		translate(glm::vec3(x,y,z));
	}
};
};
#endif