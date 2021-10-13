#ifndef __SCAN_CPP__
#define __SCAN_CPP__

#include "scan.h"

namespace VKEngine{

Scan::Scan(){
};

Scan::~Scan(){
	destroy();
}


void Scan::destroy(){
	if(cache){
		VkDevice device = VkDevice(*ctx);
		vkDestroyPipelineCache(device, cache, nullptr);
	}
	//vkFreeDescriptorSets(device, desc_pool, 1, &program.scan4.descriptors.set);
	//vkFreeDescriptorSets(device, desc_pool, 1, &program.scan_ed.descriptors.set);
	//vkFreeDescriptorSets(device, desc_pool, 1, &program.uniform_update.descriptors.set);
	program.scan4.destroy();
	LOG("scan4 destroyed\n");
	program.scan_ed.destroy();
	LOG("scan_ed destroyed\n");
	program.uniform_update.destroy();
	LOG("uniform update destroyed\n");

	if(desc_pool != VK_NULL_HANDLE)
	vkDestroyDescriptorPool(VkDevice(*ctx), desc_pool, nullptr);
	desc_pool = VK_NULL_HANDLE;
	LOG("descriptor Pool destroyed\n");
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr)
			d_grps[i]->destroy();
	}
	d_limit.destroy();
}

void Scan::create(Context *context, CommandQueue* command_queue){
	ctx = context;
	queue = command_queue;
}

void Scan::init(uint32_t sz_mem){
	nr_element = sz_mem;

	setupBuffers();
	setupDescriptorPool();
	setupKernels();
}


void Scan::setupBuffers(){
	uint32_t sm = 64;
	uint32_t size = nr_element;

	while(size > 4 * sm){
		uint32_t g_siz = (size + 3) / 4;
		h_limits.push_back(g_siz);
		g_sizes.push_back( (g_siz + sm - 1) / sm * sm );
		l_sizes.push_back( sm );
		size = (g_siz + sm - 1) / sm;
		d_grps.push_back( new Buffer(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									(size + 1) * sizeof(uint32_t), nullptr) );
	}

	if(size){
		d_grps.push_back(nullptr);
		g_sizes.push_back(size);
		l_sizes.push_back(size);
		h_limits.push_back(size);
	}
	d_limit.create(ctx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 4, nullptr);
}

void Scan::setupDescriptorPool(){
    LOG("setup Descriptor Pool\n");
	VkDescriptorPoolSize pool_size[2] = {
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
	};
	VkDescriptorPoolCreateInfo desc_pool_CI = infos::descriptorPoolCreateInfo(2, pool_size,  4);
	VK_CHECK_RESULT(vkCreateDescriptorPool(VkDevice(*ctx), &desc_pool_CI, nullptr, &desc_pool));

    LOG("setup Descriptor Pool Done\n");
}

void Scan::setupKernels(){
	program.scan4.create(ctx, "shaders/scan.comp.spv");
	program.scan_ed.create(ctx, "shaders/scan_ed.comp.spv");
	program.uniform_update.create(ctx, "shaders/uniform_update.comp.spv");

	VkDescriptorSetLayoutBinding binding;
	program.scan4.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3)
	});

	program.scan_ed.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
	});

	program.uniform_update.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
	});
    LOG("descriptor pool : %p \n", desc_pool);
	LOG("scan allocate\n");
	program.scan4.allocateDescriptorSet(desc_pool);
	LOG("scan_ed allocate\n");
	program.scan_ed.allocateDescriptorSet(desc_pool);
	LOG("uniform_update allocate\n");
	program.uniform_update.allocateDescriptorSet(desc_pool);
}

void Scan::build(){
	uint32_t data[2] = {2*h_limits.back(), h_limits.back()};
    LOG("scan_ed local_mem_size : %d\n", 4 * data[0]);
    LOG("scan_ed local_dispatch_size : %d\n", data[1]);

	VkSpecializationMapEntry scan_ed_map[2];
	scan_ed_map[0].constantID = 1;
	scan_ed_map[0].offset = 0;
	scan_ed_map[0].size = sizeof(uint32_t);
	scan_ed_map[1].constantID = 2;
	scan_ed_map[1].offset = sizeof(uint32_t);
	scan_ed_map[1].size = sizeof(uint32_t);
	VkSpecializationInfo scan_ed_SI = {};
	scan_ed_SI.mapEntryCount = 2;
	scan_ed_SI.pMapEntries = scan_ed_map;
	scan_ed_SI.dataSize = sizeof(uint32_t)*2;
	scan_ed_SI.pData = data;

	if(!cache){
		VkPipelineCacheCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(VkDevice(*ctx), &info, nullptr, &cache));
	}
	program.scan4.build(cache, nullptr);
	program.scan_ed.build(cache, &scan_ed_SI);
	program.uniform_update.build(cache, nullptr);
}

void Scan::run(Buffer *d_input, Buffer *d_output){
	VkFence fence = queue->createFence(VK_FENCE_CREATE_SIGNALED_BIT);
	LOG("fence created \n");
	vector<Buffer *> d_inputs = {d_input};
	vector<Buffer *> d_outputs = {d_output};
	LOG("Buffer initialized\n");
	for(uint32_t i = 0 ; i < d_grps.size(); i++){
		if(d_grps[i] != nullptr){
			d_inputs.push_back(d_grps[i]);
			d_outputs.push_back(d_grps[i]);
		}
	}


	printf("d_inputs : ");
	for(uint32_t i = 0 ; i < d_inputs.size() ; i++){
		printf("%p ", d_inputs[i]);
	}
	printf("\nd_outputs : ");
	for(uint32_t i = 0 ; i < d_outputs.size() ; i++){
		printf("%p ", d_outputs[i]);
	}
	
	printf("\nd_grps : ");
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		printf("%p ", d_grps[i]);
	}
	printf("\n");

	printf("\ng_sizes : ");
	for(uint32_t i = 0 ; i < g_sizes.size() ; i++){
		printf("%d ", g_sizes[i]);
	}
	printf("\n");

	vector<VkCommandBuffer> used_buffers;
	LOG("GPUProcess start!\n");
	for(uint32_t i = 0 ; i < d_grps.size() ; ++i){
		if(d_grps[i] != nullptr){
			LOG("Scan4\n");
			VkCommandBuffer command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, true);
			d_limit.copyFrom(&h_limits[i], 4);
			program.scan4.setKernelArgs({
				{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_inputs[i]->descriptor,nullptr},
				{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor,nullptr},
				{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_grps[i]->descriptor, nullptr},
				{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &d_limit.descriptor, nullptr}
			});
			queue->bindKernel(command, &program.scan4);
			queue->dispatch(command, (g_sizes[i]+63)/64, 1, 1);
			// LOG("dispatch\n");
			queue->endCommandBuffer(command);
			queue->resetFences(&fence, 1);
			queue->submit(&command, 1, 0, nullptr, 0, nullptr, 0, fence);
			// LOG("submith\n");
			queue->waitFences(&fence, 1);
			// LOG("waitIdle\n");
			used_buffers.push_back(command);
			// LOG("free command buffer\n");

		}else{
			LOG("scan_ed\n");
			VkCommandBuffer command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, true);
			program.scan_ed.setKernelArgs({
				{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_inputs[i]->descriptor, nullptr},
				{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor, nullptr},
			});
			queue->bindKernel(command, &program.scan_ed);
			queue->dispatch(command, g_sizes[i], 1, 1);
			// LOG("dispatch\n");
			queue->endCommandBuffer(command);
			queue->resetFences(&fence, 1);
			queue->submit(&command, 1, 0, nullptr, 0, nullptr, 0, fence);
			// LOG("submit\n");
			queue->waitFences(&fence, 1);
			used_buffers.push_back(command);

		}
	}

	int nr_g = (int)d_grps.size();
	for(int i = nr_g-1; i >= 0 ; --i){
		LOG("uniform update %d start\n", i);
		if(d_grps[i] != nullptr){
			LOG("uniform_update d_grps : %p\n",d_grps[i]);
			VkCommandBuffer command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, true);
			LOG("command buffer create done\n");
			program.uniform_update.setKernelArgs({
				{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor, nullptr},
				{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_grps[i]->descriptor, nullptr},
			});
			queue->bindKernel(command, &program.uniform_update);
			queue->dispatch(command, (g_sizes[i]+63)/64, 1, 1);
			queue->endCommandBuffer(command);
			queue->resetFences(&fence, 1);
			queue->submit(&command, 1, 0, nullptr, 0, nullptr, 0, fence);
			queue->waitFences(&fence, 1);
			used_buffers.push_back(command);
		}
	}
	queue->destroyFence(fence);
	queue->waitIdle();
	for(auto cmd : used_buffers){
		queue->free(cmd);
	}
	LOG("scan process done\n");
}


}
#endif 
