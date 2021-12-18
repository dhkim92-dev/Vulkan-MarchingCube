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
	VkDescriptorPool desc_pool = VK_NULL_HANDLE;
	VkPipelineCache cache = VK_NULL_HANDLE;
	VkFence fence;
	void *h_volume;
	
	struct MetaInfo{
		uint32_t x,y,z;
		float isovalue;
	}meta_info;

public :
	struct{
		VkDescriptorSet edge_test = VK_NULL_HANDLE;
		VkDescriptorSet	cell_test = VK_NULL_HANDLE;
		VkDescriptorSet edge_compact = VK_NULL_HANDLE;
		VkDescriptorSet gen_vertices = VK_NULL_HANDLE;
		VkDescriptorSet gen_faces = VK_NULL_HANDLE;
		VkDescriptorSet gen_normals = VK_NULL_HANDLE;
	}desc_sets;

	struct{
		Kernel kernel;
		Buffer d_output;
		VkCommandBuffer command = VK_NULL_HANDLE;
		VkEvent event = VK_NULL_HANDLE;
	}edge_test;

	struct{
		Kernel kernel;
		Buffer d_celltype;
		Buffer d_tricount;
		VkCommandBuffer command = VK_NULL_HANDLE;
		VkEvent event = VK_NULL_HANDLE;
	}cell_test;

	struct{
		Kernel kernel;
		VkCommandBuffer command = VK_NULL_HANDLE;
		VkEvent event = VK_NULL_HANDLE;
	}edge_compact;

	struct GenFaces{
		Kernel kernel;
		VkCommandBuffer command = VK_NULL_HANDLE;
	}gen_faces;

	struct GenVertex{
		Kernel kernel;
		VkEvent event=VK_NULL_HANDLE;
		VkCommandBuffer command = VK_NULL_HANDLE;
	}gen_vertices;

	struct GenNormal{
		Kernel kernel;
		VkCommandBuffer command = VK_NULL_HANDLE;
	}gen_normals;

	Scan cell_scan;
	Scan edge_scan;

	struct PrefixSum{
		Buffer d_epsum;
		Buffer d_cpsum;
	}scan;
	
	struct{
		Buffer vertices;
		Buffer indices;
		Buffer normals;
	}outputs;

	struct{
		Buffer d_cast_table;
		Buffer d_metainfo;
		Image d_volume;
	}general;
public :
	MarchingCube();
	MarchingCube(Context *context, CommandQueue *command_queue);
	~MarchingCube();
	void create(Context *context, CommandQueue *commandQueue);
	void destroy();
	void destroyEvents();
	void destroyBuffers();
	void destroyKernels();
	void freeCommandBuffers();
	void setVolumeSize(uint32_t x, uint32_t y, uint32_t z);
	void setIsovalue(float value);
	void setVolume(void *ptr);
	void setupBuffers();
	void setupDescriptorPool();
	void setupKernels();
	void setupCommandBuffers();
	void setupEdgeTestCommand();
	void setupCellTestCommand();
	void setupEdgeCompactCommand();
	void setupGenVerticesCommand();
	void setupGenFacesCommand();
	void setupGenNormalCommand();
	void run(VkSemaphore *wait_semaphores, uint32_t nr_waits, VkSemaphore *signal_semaphores, uint32_t nr_signals);
};

}



#endif