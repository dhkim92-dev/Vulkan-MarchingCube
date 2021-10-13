#ifndef __MARCHINGCUBE_H__
#define __MARCHINGCUBE_H__

#include <vector>
#include "vk_core.h"
#include "scan.h"

using namespace std;

namespace VKEngine{

class MarchingCube{
private :
	Context *ctx = nullptr;
	CommandQueue *queue = nullptr;
	VkSemaphore compute_complete = VK_NULL_HANDLE;
	VkDescriptorPool desc_pool = VK_NULL_HANDLE;

	struct Volumes{
		float *data;
		int x,y,z;
	}h_volume;

public :
	struct{
		Kernel kernel;
		Buffer d_output;
		void destroy(){
			kernel.destroy();
			d_output.destroy();
		}
	}volume_test;

	struct{
		Kernel kernel;
		Buffer d_output;
		
		void destroy(){
			kernel.destroy();
			d_output.destroy();
		}
	}cell_test;

	struct{
		Kernel kernel;
		Buffer d_output;
		void destroy(){
			kernel.destroy();
			d_output.destroy();
		}
	}edge_test;

	struct{
		Kernel kernel;
		Buffer d_table;
		void destroy(){
			kernel.destroy();
			d_table.destroy();
		}
	}edge_compact;

	Kernel gen_vertices;
	Kernel gen_faces;
	Kernel gen_vnorm;

	Scan cell_scan;
	Scan edge_scan;
	
	struct{
		Buffer vertices;
		Buffer indices;

		void destroy(){
			vertices.destroy();
			indices.destroy();
		}
	}outputs;

	struct{
		Buffer d_cast_table;
		Image volume;

	}general;

public :
	MarchingCube();
	MarchingCube(Context *context, CommandQueue *command_queue);
	~MarchingCube();
	void create(Context *context, CommandQueue *commandQueue);
	void destroy();
	void setVolume(float *data, int x, int y, int z);
	void setupBuffers();
	void setupDescriptorPool();
	void setupKernels();
	void setupCommandBuffers();
	void volumeTest();
	void edgeTest();
	void cellTest();
	void edgeCompact();
	void genVertices();
	void genFaces();
	void run();
};

}



#endif