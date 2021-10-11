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
	vector<Buffer *> d_grps; // device group_sum buffer
	Buffer d_limit; // device buffer to save limits
	vector<uint32_t> h_limits;
	vector<uint32_t> g_sizes;
	vector<uint32_t> l_sizes;
	uint32_t nr_element;

	struct{
		Kernel scan4;
		Kernel scan_ed;
		Kernel uniform_update;
	}program;

	struct{
		vector<VkCommandBuffer> scan4;
		vector<VkCommandBuffer> scan_ed;
		vector<VkCommandBuffer> uniform_update;
	}commands;

	public :
	Scan();
	~Scan();
	void create(Context *context, CommandQueue* command_queue);
	void destroy();
	void init(uint32_t sz_mem);
	void setupBuffers();
	void setupKernels();
	void setupDescriptorPool();
	void build();
	void run(Buffer *d_input, Buffer *d_output);
};


}

#endif