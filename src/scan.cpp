#ifndef __SCAN_CPP__
#define __SCAN_CPP__

#include "scan.h"

using namespace VKEngine;

Scan::Scan(){
};


void Scan::create(Context *context, CommandQueue* command_queue){
	ctx = context;
	queue = command_queue;
}

void Scan::init(uint32_t sz_mem){
	nr_element = sz_mem;
	setupBuffers();
	setupBuilders();
	setupDescriptorPool();
	setupDescriptors();
	setupPipelineLayouts();
}

void Scan::setupBuffers(){
    LOG("Scan::setupBuffers()\n");
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
    LOG("Scan::setupBuffers() end\n");
}


void Scan::setupBuilders()
{
    LOG("Scan::setupBuilders()\n");
	builders.descriptor = new DescriptorSetBuilder(ctx);
	builders.scan4 = new ComputePipelineBuilder(ctx);
	builders.scan_ed = new ComputePipelineBuilder(ctx);
	builders.uniform_update = new ComputePipelineBuilder(ctx);
    LOG("Scan::setupBuilders() end\n");
}

void Scan::setupDescriptorPool(){
    LOG("Scan::setupDescriptorPool()\n");
	vector<VkDescriptorPoolSize> pool_size(2);
	pool_size[0] = DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 18);
	pool_size[1] = DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8);
	VK_CHECK_RESULT(builders.descriptor->setDescriptorPool(pool_size, 10, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT));
    LOG("Scan::setupDescriptorPool() end\n");
}

void Scan::setupDescriptors(){
    LOG("Scan::setupDescriptorSetLayouts()\n");
	for(count = 0 ; d_grps[count] != nullptr ; count++){}

	progs.scan4.sets = new VkDescriptorSet[count];
	progs.uniform_update.sets = new VkDescriptorSet[count];

	vector<VkDescriptorSetLayoutBinding> bindings(4);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.scan4.d_layout, bindings));
	for(int i = 0 ; i < count ; i++)
		VK_CHECK_RESULT(builders.descriptor->build(&progs.scan4.sets[i], &progs.scan4.d_layout, 1));
	bindings.resize(2);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.scan_ed.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.scan_ed.set, &progs.scan_ed.d_layout, 1));
	bindings.resize(2);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.uniform_update.d_layout, bindings));
	for(int i = 0 ; i < count ; i++)
		VK_CHECK_RESULT(builders.descriptor->build(&progs.uniform_update.sets[i], &progs.uniform_update.d_layout, 1));
    LOG("Scan::setupDescriptorSetLayouts() end\n");
}
    
void Scan::setupPipelineLayouts()
{
    LOG("Scan::setupPipelineLayout()\n");
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.scan4.p_layout, &progs.scan4.d_layout, 1, nullptr, 0));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.scan_ed.p_layout, &progs.scan_ed.d_layout, 1, nullptr, 0));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.uniform_update.p_layout, &progs.uniform_update.d_layout, 1, nullptr, 0));
    LOG("Scan::setupPipelineLayout() end\n");
}

void Scan::setupPipelineCaches()
{
    LOG("Scan::setupPipelineCache()\n");
	VK_CHECK_RESULT(PipelineCacheBuilder::build(ctx, &cache));
    LOG("Scan::setupPipelineCache() end\n");
}

void Scan::setupPipelines(){
    LOG("Scan::setupPipelines\n");
	uint32_t data[2] = {2*h_limits.back(), h_limits.back()};
	VkSpecializationMapEntry entries[2];
	entries[0].constantID = 1;
	entries[0].offset = 0;
	entries[0].size = sizeof(uint32_t);
	entries[1].constantID = 2;
	entries[1].offset = sizeof(uint32_t);
	entries[1].size = sizeof(uint32_t);
	VkSpecializationInfo sinfo = {};
	sinfo.mapEntryCount = 2;
	sinfo.pMapEntries = entries;
	sinfo.dataSize = sizeof(uint32_t)*2;
	sinfo.pData = data;

	builders.scan4->setComputeShader("shaders/scan.comp.spv");
	builders.scan_ed->setComputeShader("shaders/scan_ed.comp.spv", &sinfo);
	builders.uniform_update->setComputeShader("shaders/uniform_update.comp.spv");
	VK_CHECK_RESULT(builders.scan4->build(&progs.scan4.pipeline, progs.scan4.p_layout, cache));
	VK_CHECK_RESULT(builders.scan_ed->build(&progs.scan_ed.pipeline, progs.scan_ed.p_layout, cache));
	VK_CHECK_RESULT(builders.uniform_update->build(&progs.uniform_update.pipeline, progs.uniform_update.p_layout, cache));
    LOG("Scan::setupPipelines end\n");
}

void Scan::build(){
	LOG("Scan::build()\n");
	setupPipelines();
	LOG("Scan::build() done\n");
}

void Scan::setupCommandBuffer(Buffer *d_input, Buffer *d_output, VkEvent wait_event){
	LOG("Scan::setupCommandBuffer()\n");
	VK_CHECK_RESULT(ctx->createEvent(&event));
	LOG("Scan::event created\n");
	d_inputs.push_back(d_input);
	d_outputs.push_back(d_output);
	for(uint32_t i = 0 ; i < d_grps.size(); i++){
		if(d_grps[i] != nullptr){
			d_inputs.push_back(d_grps[i]);
			d_outputs.push_back(d_grps[i]);
		}
	}

	VK_CHECK_RESULT( queue->createCommandBuffer(&command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY) );
	VK_CHECK_RESULT( queue->beginCommandBuffer(command, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	LOG("Scan::setupCommandBuffer begin()\n");
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

	Program &scan4 = progs.scan4;
	Program &scan_ed = progs.scan_ed;
	Program &uniform_update = progs.uniform_update;

	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr){
			VkWriteDescriptorSet writes[4] ={
				DescriptorWriter::writeBuffer(scan4.sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, d_inputs[i]),
				DescriptorWriter::writeBuffer(scan4.sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, d_outputs[i]),
				DescriptorWriter::writeBuffer(scan4.sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, d_grps[i]),
				DescriptorWriter::writeBuffer(scan4.sets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &d_limit[i])
			};
			DescriptorWriter::update(ctx, writes, 4);
			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, scan4.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, scan4.p_layout, 0, &scan4.sets[i], 1);

			if(i == 0){
				VkBufferMemoryBarrier input_barrier = d_inputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
				queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,nullptr, 0,&input_barrier,1,nullptr, 0);
			}
			queue->dispatch(command, (g_sizes[i] + 63 )/ 64, 1, 1);
			VkBufferMemoryBarrier output_barrier = d_outputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0, nullptr, 0, &output_barrier, 1, nullptr, 0);
		}else{
			VkWriteDescriptorSet writes[2] ={
				DescriptorWriter::writeBuffer(scan_ed.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, d_inputs[i]),
				DescriptorWriter::writeBuffer(scan_ed.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, d_outputs[i])
			};
			DescriptorWriter::update(ctx, writes, 2);
			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, scan_ed.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, scan_ed.p_layout, 0, &scan_ed.set, 1);
			
			VkBufferMemoryBarrier input_barrier = d_inputs[i]->barrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, &input_barrier, 1, nullptr, 0);
			queue->dispatch(command, g_sizes[i], 1, 1);
			VkBufferMemoryBarrier output_barrier = d_outputs[i]->barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			queue->barrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, &output_barrier, 1, nullptr, 0);
			// LOG("Output bind dispatch\n");
		}
	}
	for(int i = static_cast<uint32_t>(d_grps.size()) - 1 ; i >= 0 ; i--){
		if(d_grps[i] != nullptr){
			// LOG("uniform update %d\n",i);
			VkWriteDescriptorSet writes[2] = {
				DescriptorWriter::writeBuffer(uniform_update.sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, d_outputs[i]),
				DescriptorWriter::writeBuffer(uniform_update.sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, d_grps[i])
			};
			DescriptorWriter::update(ctx, writes, 2);

			queue->bindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, uniform_update.pipeline);
			queue->bindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, uniform_update.p_layout, 0, &uniform_update.sets[i], 1, 0, nullptr);
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
	LOG("Scan::setupCommandBuffer() end\n");
}

Scan::~Scan(){
	destroy();
}

void Scan::destroy()
{
    LOG("Scan::destroy()\n");
	ctx->destroyEvent(&event);
	destroyCommand();
	destroyPipeline();
	destroyDescriptor();
	destroyBuffers();
    LOG("Scan::destory() end\n");

}

void Scan::destroyCommand(){
	ctx->destroyEvent(&event);
	queue->free(command);
	command = VK_NULL_HANDLE;
}

void Scan::destroyDescriptor()
{
	LOG("Scan::destroyDescriptor()\n");
	builders.descriptor->free(progs.scan4.sets, count);
	builders.descriptor->free(&progs.scan_ed.set, 1);
	builders.descriptor->free(progs.uniform_update.sets, count);
	delete [] progs.scan4.sets;
	delete [] progs.uniform_update.sets;
	// layout destroy
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.scan4.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.scan_ed.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.uniform_update.d_layout);
	delete builders.descriptor;
	LOG("Scan::destroyDescriptor() end\n");
}

void Scan::destroyPipeline()
{
	// pipeline destroy
	LOG("Scan::destroyPipeline()\n");
	ComputePipelineBuilder::destroy(ctx, &progs.scan4.pipeline);
	ComputePipelineBuilder::destroy(ctx, &progs.uniform_update.pipeline);
	ComputePipelineBuilder::destroy(ctx, &progs.scan_ed.pipeline);

	PipelineCacheBuilder::destroy(ctx, &cache);

	PipelineLayoutBuilder::destroy(ctx, &progs.scan4.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.scan_ed.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.uniform_update.p_layout);

	delete builders.scan4;
	delete builders.scan_ed;
	delete builders.uniform_update;
	LOG("Scan::destroyPipeline() end\n");
}

void Scan::destroyBuffers()
{
	LOG("Scan::destroyBufers()\n");
	for(uint32_t i = 0 ; i < d_grps.size() ; i++){
		if(d_grps[i] != nullptr)
			d_grps[i]->destroy();
	}
	d_grps.clear();	

	delete [] d_limit;
	LOG("Scan::destroyBufers() end\n");
}


#endif 
