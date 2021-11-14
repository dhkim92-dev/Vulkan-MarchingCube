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
    LOG("Scan::destroy()\n");
	VkDevice device = ctx->getDevice();
	if(cache){
        // LOG("cache destroy!\n");
		vkDestroyPipelineCache(device, cache, nullptr);
        cache = VK_NULL_HANDLE;
	}

	if(scan4_sets!=nullptr){
		vkFreeDescriptorSets(device, desc_pool, nr_desc_alloc, scan4_sets);
		delete [] scan4_sets;
		scan4_sets = nullptr;
	}
	if(uniform_update_sets != nullptr){
		vkFreeDescriptorSets(device, desc_pool, nr_desc_alloc, uniform_update_sets);
		delete [] uniform_update_sets;
		uniform_update_sets = nullptr;
	}
	if(scan_ed_set){
		vkFreeDescriptorSets(device, desc_pool, 1, &scan_ed_set);
		scan_ed_set = VK_NULL_HANDLE;
	}
	program.scan4.destroy();
	program.scan_ed.destroy();
	program.uniform_update.destroy();

	if(event){
		ctx->destroyEvent(&event);
	}

	if(desc_pool != VK_NULL_HANDLE){
        vkDestroyDescriptorPool(device, desc_pool, nullptr);
        desc_pool = VK_NULL_HANDLE;
    }
    
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i])
			d_grps[i]->destroy();
	}
    d_grps.clear();
	for(uint32_t i = 0 ; i < nr_desc_alloc ; i++){
		d_limit[i].destroy();
	}
	d_inputs.clear();
	d_outputs.clear();
	nr_desc_alloc = 0;

    LOG("Scan::destory() end\n");
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
		d_grps.push_back( new Buffer(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									(size + 1) * sizeof(uint32_t), nullptr) );
	}

	if(size){
		d_grps.push_back(nullptr);
		g_sizes.push_back(size);
		l_sizes.push_back(size);
		h_limits.push_back(size);
	}
	d_limit = new Buffer[d_grps.size() - 1];
	for(uint32_t i = 0 ; i < d_grps.size() - 1; i++)
		d_limit[i].create(ctx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 4, &h_limits[i]);
}

void Scan::setupDescriptorPool(){
    LOG("Scan::setup Descriptor Pool\n");
	VkDevice device = ctx->getDevice();
	VkDescriptorPoolSize pool_size[2] = {
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 18),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8),
	};
	VkDescriptorPoolCreateInfo desc_pool_CI = infos::descriptorPoolCreateInfo(2, pool_size,  12);
	desc_pool_CI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &desc_pool_CI, nullptr, &desc_pool));

    LOG("Scan::setup Descriptor Pool Done\n");
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
    // LOG("descriptor pool : %p \n", desc_pool);
	uint32_t nr_alloc = 0;
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr){
			nr_alloc++;
		}
	}
	scan4_sets = new VkDescriptorSet[nr_alloc];
	uniform_update_sets = new VkDescriptorSet[nr_alloc];
	program.scan_ed.allocateDescriptorSet(desc_pool, &scan_ed_set, 1 );
	for(uint32_t i = 0 ; i < nr_alloc ; i++)
		program.scan4.allocateDescriptorSet(desc_pool, &scan4_sets[i], 1);

	for(uint32_t i = 0 ; i < nr_alloc ; i++)
		program.uniform_update.allocateDescriptorSet(desc_pool, &uniform_update_sets[i], 1);
	nr_desc_alloc = nr_alloc;

}

void Scan::build(){
	LOG("Scan::build()\n");
	VkDevice device = ctx->getDevice();
	uint32_t data[2] = {2*h_limits.back(), h_limits.back()};
    // LOG("scan_ed local_mem_size : %d\n", 4 * data[0]);
    // LOG("scan_ed local_dispatch_size : %d\n", data[1]);

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
		VK_CHECK_RESULT(vkCreatePipelineCache(device, &info, nullptr, &cache));
	}
	program.scan4.build(cache, nullptr);
	program.scan_ed.build(cache, &scan_ed_SI);
	program.uniform_update.build(cache, nullptr);
	LOG("Scan::build() done\n");
}

void Scan::setupCommandBuffer(Buffer *d_input, Buffer *d_output, VkEvent wait_event){
	LOG("Scan::setupCommandBuffer()\n");
	VK_CHECK_RESULT(ctx->createEvent(&event));
	d_inputs.push_back(d_input);
	d_outputs.push_back(d_output);
	for(uint32_t i = 0 ; i < d_grps.size(); i++){
		if(d_grps[i] != nullptr){
			d_inputs.push_back(d_grps[i]);
			d_outputs.push_back(d_grps[i]);
		}
	}
	command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);


	VkBufferMemoryBarrier input_barrier = d_inputs[0]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	queue->waitEvents(command, &wait_event, 1, 
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				nullptr, 0 ,
				&input_barrier, 1,
				nullptr, 0);
	vkCmdFillBuffer(command, d_outputs[0]->getBuffer(), 0, d_outputs[0]->getSize(), 0);
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr){
			vkCmdFillBuffer(command, d_grps[i]->getBuffer(), 0, d_grps[i]->getSize(), 0);
		}
	}


	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr){
			// LOG("scan4 iter %d\n",i);
			// LOG("h_limits[%d] : %d\n", i, h_limits[i]);
			//d_limit[i].copyFrom(&h_limits[i], 4);
			program.scan4.setKernelArgs(scan4_sets[i], {
				{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_inputs[i]->descriptor, nullptr},
				{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor, nullptr},
				{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_grps[i]->descriptor, nullptr},
				{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &d_limit[i].descriptor, nullptr}
			});
			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.scan4.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.scan4.layouts.pipeline,
				0, &scan4_sets[i], 1, 0, nullptr);

			if(i == 0){
				//}else{
				VkBufferMemoryBarrier input_barrier = d_inputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
				queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,
				nullptr, 0,&input_barrier,1,nullptr, 0);
			}
			queue->dispatch(command, (g_sizes[i] + 63 )/ 64, 1, 1);
			VkBufferMemoryBarrier output_barrier = d_outputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,
			nullptr, 0, &output_barrier, 1, nullptr, 0);
		}else{
			// LOG("scan_ed %d\n",i);
			program.scan_ed.setKernelArgs(scan_ed_set, {
				{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_inputs[i]->descriptor, nullptr},
				{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor, nullptr},
			});
			// LOG("scaned set Kernel Args done\n");
			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.scan_ed.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.scan_ed.layouts.pipeline, 0,
									&scan_ed_set, 1, 0, nullptr);
			// LOG("scaned bind Kernel done\n");
			VkBufferMemoryBarrier input_barrier = d_inputs[i]->barrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,
				nullptr, 0, &input_barrier, 1, nullptr, 0);
			// LOG("scaned barrier create\n");
			queue->dispatch(command, g_sizes[i], 1, 1);
			// LOG("scaned dispatch\n");
			VkBufferMemoryBarrier output_barrier = d_outputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			 			nullptr, 0, &output_barrier, 1, nullptr, 0);
			// LOG("Output bind dispatch\n");
		}
	}

	for(int i = static_cast<uint32_t>(d_grps.size()) - 1 ; i >= 0 ; i--){
		if(d_grps[i] != nullptr){
			// LOG("uniform update %d\n",i);
			program.uniform_update.setKernelArgs(uniform_update_sets[i], {
				{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_outputs[i]->descriptor, nullptr},
				{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &d_grps[i]->descriptor, nullptr}
			});

			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.uniform_update.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, program.uniform_update.layouts.pipeline,
									 0, &uniform_update_sets[i], 1, 0, nullptr);
			// LOG("uniform update bindDescriptorSets\n");
			VkBufferMemoryBarrier input_barrier = d_grps[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
			// LOG("uniform update barrier\n");
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, &input_barrier, 1, nullptr,  0);
			// LOG("uniform update dispatch\n");
			queue->dispatch(command, (g_sizes[i] + 63)/64, 1, 1);
			VkBufferMemoryBarrier output_barrier = d_outputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
			//LOG("uniform update barrier\n");
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, &output_barrier, 1, nullptr, 0);
		}
	}
	VkBufferMemoryBarrier output_barrier = d_output->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, &output_barrier, 1 , nullptr, 0);
	queue->setEvent(command, event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	VK_CHECK_RESULT(queue->endCommandBuffer(command));
	LOG("Scan::setupCommandBuffer() done\n");
}

}
#endif 
