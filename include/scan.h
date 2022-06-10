#ifndef __SCAN_H__
#define __SCAN_H__

#include <vector>
#include <vulkan/vulkan.h>
#include "vk_core.h"

using namespace std;
using namespace VKEngine;

struct Program{
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout p_layout = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VkDescriptorSet *sets;
	VkDescriptorSetLayout d_layout = VK_NULL_HANDLE;
	VkCommandBuffer command = VK_NULL_HANDLE;
	VkEvent event = VK_NULL_HANDLE;
};

class Scan{
private :
	Context *ctx;
	CommandQueue *queue;
	// VkDescriptorPool desc_pool = VK_NULL_HANDLE;
	// VkPipelineCache cache = VK_NULL_HANDLE;
public :
	Buffer *d_limit; // device buffer to save limits
	vector<uint32_t> h_limits;
	vector<uint32_t> g_sizes;
	vector<uint32_t> l_sizes;
	
	vector<Buffer *> d_inputs;
	vector<Buffer *> d_outputs;
	vector<Buffer *> d_grps; // device group_sum buffer

	uint32_t nr_desc_alloc = 0;
	uint32_t nr_element;
	uint32_t count=0;

	struct{
		ComputePipelineBuilder *scan4;
		ComputePipelineBuilder *scan_ed;
		ComputePipelineBuilder *uniform_update;
		DescriptorSetBuilder *descriptor;
	}builders;

	VkPipelineCache cache=VK_NULL_HANDLE;
	struct Progs{
		Program scan4;
		Program scan_ed;
		Program uniform_update;
	}progs;

	VkEvent event = VK_NULL_HANDLE;

	public :
	VkCommandBuffer command;
	Scan();
	~Scan();
	void create(Context *context, CommandQueue* command_queue);
	void init(uint32_t sz_mem);
	void setupBuffers();
	void setupBuilders();
	void setupPipelineLayouts();
	void setupPipelineCaches();
	void setupPipelines();
	void setupDescriptors();
	void setupDescriptorPool();
	void setupCommandBuffer(Buffer *d_input, Buffer *d_output, VkEvent wait_event);
	void build();
	void destroy();
	void destroyPipeline();
	void destroyCommand();
	void destroyBuilders();
	void destroyBuffers();
	void destroyDescriptor();
	//void run(Buffer *d_input, Buffer *d_output);
};


#endif