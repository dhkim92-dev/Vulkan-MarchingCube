#ifndef __MARCHINGCUBE_H__
#define __MARCHINGCUBE_H__

#include <vector>
#include "vk_core.h"
#include "scan.h"

using namespace std;
using namespace VKEngine;

class MarchingCube{
private :
	Context *ctx = nullptr;
	CommandQueue *queue = nullptr;
	VkPipelineCache cache = VK_NULL_HANDLE;
	// VkFence fence = VK_NULL_HANDLE;
	void *h_volume;	
	struct MetaInfo{
		uint32_t x,y,z;
		float isovalue;
	}meta_info;
	VkFence fence = VK_NULL_HANDLE;
public :
	struct Builders{
		ComputePipelineBuilder *edge;
		ComputePipelineBuilder *cell;
		ComputePipelineBuilder *compact;
		ComputePipelineBuilder *vertex;
		ComputePipelineBuilder *face;
		ComputePipelineBuilder *normal;
		DescriptorSetBuilder *descriptor;
	}builders;

	struct Programs{
		Program edge;
		Program cell;
		Program compact;
		Program vertex;
		Program face;
		Program normal;
	}progs;

	struct Buffers{
		Buffer d_edgeout;
		Buffer d_celltype;
		Buffer d_tricount;
		Buffer d_epsum;
		Buffer d_cpsum;
		Buffer vertices;
		Buffer indices;
		Buffer normals;
		Buffer d_caster;
		Buffer d_metainfo;
		Image d_volume;
	}buffers;
	Scan cell_scan;
	Scan edge_scan;
public :
	MarchingCube();
	MarchingCube(Context *context, CommandQueue *command_queue);
	~MarchingCube();
	void create(Context *context, CommandQueue *command_queue);

	void destroy();
	void destroyBuffers();
	void destroyDescriptors();
	void destroyEvents();
	void destroyPipelines();
	void destroyBuilders();
	void freeCommandBuffers();

	void setupBuilders();
	void setupBuffers();
	void setupDescriptors();
	void writeDescriptors();
	void setupPipelineLayouts();
	void setupPipelines();
	void setupCommands();
	void setupEvents();
	void setVolumeSize(uint32_t x, uint32_t y, uint32_t z);
	void setIsovalue(float value);
	void setVolume(void *ptr);

	void setupCommandBuffers();
	void setupEdgeTestCommand();
	void setupCellTestCommand();
	void setupEdgeCompactCommand();
	void setupGenVerticesCommand();
	void setupGenFacesCommand();
	void setupGenNormalCommand();
	void run(VkSemaphore *wait_semaphores, uint32_t nr_waits, VkSemaphore *signal_semaphores, uint32_t nr_signals);
};



#endif