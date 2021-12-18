#ifndef __SCAN_H__
#define __SCAN_H__

#include <vector>
#include <vulkan/vulkan.h>
#include "vk_core.h"

using namespace std;

namespace VKEngine
{

class Scan{
private :
	Context *ctx;
	CommandQueue *queue;
	VkDescriptorPool desc_pool = VK_NULL_HANDLE;
	VkPipelineCache cache = VK_NULL_HANDLE;
public :
	Buffer *d_limit; // device buffer to save limits
	vector<uint32_t> h_limits;
	vector<uint32_t> g_sizes;
	vector<uint32_t> l_sizes;
	
	vector<Buffer *> d_inputs;
	vector<Buffer *> d_outputs;
	vector<Buffer *> d_grps; // device group_sum buffer

	uint32_t nr_desc_alloc = 0;
	VkDescriptorSet *scan4_sets = nullptr;
	VkDescriptorSet scan_ed_set = VK_NULL_HANDLE;
	VkDescriptorSet *uniform_update_sets = nullptr;
	uint32_t nr_element;

	struct{
		Kernel scan4;
		Kernel scan_ed;
		Kernel uniform_update;
	}program;
	VkEvent event = VK_NULL_HANDLE;

	/*
	struct{
		vector<VkCommandBuffer> scan4;
		VkCommandBuffer scan_ed = VK_NULL_HANDLE;
		vector<VkCommandBuffer> uniform_update;
	}commands;
	*/


	public :
	VkCommandBuffer command = VK_NULL_HANDLE;
	Scan();
	~Scan();
	void create(Context *context, CommandQueue* command_queue);
	void destroy();
	void init(uint32_t sz_mem);
	void setupBuffers();
	void setupKernels();
	void setupDescriptorPool();
	void setupCommandBuffer(Buffer *d_input, Buffer *d_output, VkEvent wait_event);
	void build();
	//void run(Buffer *d_input, Buffer *d_output);
};


}

#endif