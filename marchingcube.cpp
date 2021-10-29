#ifndef __MARCHINGCUBE_CPP__
#define __MARCHINGCUBE_CPP__

#include "marchingcube.h"

namespace VKEngine{

MarchingCube::MarchingCube(){}
MarchingCube::MarchingCube(Context *context, CommandQueue *command_queue){
	create(context, command_queue);
}
MarchingCube::~MarchingCube(){
	destroy();
}

void MarchingCube::create(Context *context, CommandQueue *command_queue){
	ctx = context;
	queue = command_queue;
}

void MarchingCube::setVolumeSize(uint32_t x, uint32_t y, uint32_t z)
{
	meta_info.x = x;
	meta_info.y = y;
	meta_info.z = z;
	meta_info.isovalue = 0.0f;
}

void MarchingCube::setIsovalue(float value){
	meta_info.isovalue = value;
	general.d_metainfo.copyFrom(&meta_info, sizeof(MetaInfo));
}

void MarchingCube::setVolume(void *ptr){
	LOG("MarchingCube::setVolume() \n");
	VkFence fence = queue->createFence();
	Buffer staging = Buffer(ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		meta_info.x * meta_info.y * meta_info.z * sizeof(float), ptr);
	
	//staging.map();
	
	VkCommandBuffer copy = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, true);
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;
	general.d_volume.setLayout(copy, VK_IMAGE_ASPECT_COLOR_BIT, 
								VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
								range, 
								VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	VkBufferImageCopy buffer_image = {};
	buffer_image.bufferOffset = 0 ;
	buffer_image.bufferRowLength = meta_info.x;
	buffer_image.bufferImageHeight = meta_info.y;
	buffer_image.imageOffset = {0,0,0};
	buffer_image.imageExtent = {meta_info.x, meta_info.y, meta_info.z};
	buffer_image.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	buffer_image.imageSubresource.baseArrayLayer = 0;
	buffer_image.imageSubresource.layerCount=1;
	buffer_image.imageSubresource.mipLevel=0;
	queue->copyBufferToImage(copy, &staging, &general.d_volume, &buffer_image);
	general.d_volume.setLayout(copy, VK_IMAGE_ASPECT_COLOR_BIT, 
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								range, 
								VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);

	queue->endCommandBuffer(copy);
	queue->resetFences(&fence, 1);
	queue->submit(&copy, 1, 0, nullptr, 0, nullptr, 0, fence);
	queue->waitFences(&fence, 1);
	queue->waitIdle();
	queue->free(copy);
	staging.destroy();
	queue->destroyFence(fence);
	LOG("MarchingCube::setVolume() end\n");
}

void MarchingCube::setupBuffers(){
	general.d_volume.create(ctx);
	VK_CHECK_RESULT(general.d_volume.createImage(meta_info.x, meta_info.y, meta_info.z, VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
								VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, 1, 1));
	LOG("MarchingCube::createImage()\n");
	VK_CHECK_RESULT(general.d_volume.alloc(sizeof(float) * meta_info.x * meta_info.y * meta_info.z, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	LOG("MarchingCube::alloc()\n");
	VK_CHECK_RESULT(general.d_volume.bind(0));
	LOG("MarchingCube::bind()\n");
	VK_CHECK_RESULT(general.d_volume.createImageView(VK_IMAGE_VIEW_TYPE_3D, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
	LOG("MarchingCube::createImgaeView()\n");
	VkSamplerCreateInfo sampler_CI = infos::samplerCreateInfo();
	sampler_CI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.anisotropyEnable = VK_FALSE;
	sampler_CI.maxLod=0.0f;
	sampler_CI.minLod = 0.0f;
	sampler_CI.mipLodBias = 0.0f;
	sampler_CI.compareOp = VK_COMPARE_OP_NEVER;
	sampler_CI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_CI.minFilter = VK_FILTER_NEAREST;
	sampler_CI.magFilter = VK_FILTER_NEAREST;
	sampler_CI.unnormalizedCoordinates = VK_FALSE;
	VK_CHECK_RESULT(general.d_volume.createSampler(&sampler_CI));
	general.d_volume.setupDescriptor();

	general.d_metainfo.create(ctx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
								sizeof(MetaInfo), &meta_info);
	LOG("MarchingCube::createSampler()\n");

	edge_test.d_output.create(ctx,
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t) * 3, nullptr);
	cell_test.d_celltype.create(ctx, 
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
							meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	cell_test.d_tricount.create(ctx, 
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	scan.d_epsum.create(ctx, 
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t) * 3, nullptr);
	scan.d_cpsum.create(ctx, 
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	outputs.vertices.create(ctx, 
							VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							(meta_info.x * meta_info.y * meta_info.z * sizeof(float) * 3), nullptr);
	outputs.indices.create(ctx, 
							VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							sizeof(uint32_t) * meta_info.x * meta_info.y * meta_info.z, nullptr);

	outputs.normals.create(ctx, 
						    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							(meta_info.x * meta_info.y * meta_info.z * sizeof(float) * 3), nullptr);

	uint32_t h_cast_table[12] ={
		0,
		4,
		meta_info.x*3,
		1,
		3*meta_info.x*meta_info.y,
		3*(meta_info.x*meta_info.y + 1) + 1,
		3*(meta_info.x*meta_info.y + meta_info.x),
		3*(meta_info.x*meta_info.y) + 1,
		2,
		5,
		3*(meta_info.x+ 1) + 2,
		3*meta_info.x + 2
	};

	general.d_cast_table.create(ctx,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						12*sizeof(uint32_t), nullptr);
	queue->enqueueCopy(h_cast_table, &general.d_cast_table, 0, 0, sizeof(uint32_t) * 12);
	
}

void MarchingCube::setupDescriptorPool(){
	LOG("MarchingCube::setupDescriptorPool()\n");
	vector<VkDescriptorPoolSize> pool_sizes = {
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 19),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 9)
	};
	VkDescriptorPoolCreateInfo desc_pool_CI = infos::descriptorPoolCreateInfo(
		static_cast<uint32_t>(pool_sizes.size()),
		pool_sizes.data(),
		10
	);
	LOG("descriptor pool : %p\n", desc_pool);
	VK_CHECK_RESULT(vkCreateDescriptorPool(VkDevice(*ctx), &desc_pool_CI, nullptr, &desc_pool));
	LOG("descriptor pool : %p\n", desc_pool);
	LOG("MarchingCube::setupDescriptorPool() end\n");
}

void MarchingCube::setupKernels(){
	if(cache == VK_NULL_HANDLE){
		VkPipelineCacheCreateInfo cache_CI = {};
		cache_CI.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(VkDevice(*ctx), &cache_CI, nullptr, &cache));
	}
	LOG("MarchingCube::setupKernel()\n");
	edge_test.kernel.create(ctx, "shaders/edge_test.comp.spv");
	cell_test.kernel.create(ctx, "shaders/cell_test.comp.spv");
	edge_compact.kernel.create(ctx, "shaders/edge_compact.comp.spv");
	gen_vertices.kernel.create(ctx, "shaders/gen_vertices.comp.spv");
	gen_faces.kernel.create(ctx, "shaders/gen_faces.comp.spv");
	gen_normals.kernel.create(ctx, "shaders/gen_normals.comp.spv");
	edge_scan.create(ctx, queue);
	cell_scan.create(ctx, queue);

	LOG("MarchingCube::setupDescriptorLayout()\n");
	edge_test.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2)
	});
	cell_test.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3)
	});	
	edge_compact.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3)
	});
	gen_vertices.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
	});
	gen_faces.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
	});
	gen_normals.kernel.setupDescriptorSetLayout({
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
	});


	LOG("MarchingCube::allocateDscriptorSet()\n");
	edge_scan.init(meta_info.x * meta_info.y * meta_info.z * 3);
	cell_scan.init(meta_info.x * meta_info.y * meta_info.z);
	LOG("MarchingCube::allocateDscriptorSet()\n");
	LOG("descriptor pool : %p\n", desc_pool);
	VK_CHECK_RESULT(edge_test.kernel.allocateDescriptorSet(desc_pool, &desc_sets.edge_test ,1));
	VK_CHECK_RESULT(cell_test.kernel.allocateDescriptorSet(desc_pool, &desc_sets.cell_test,1));
	VK_CHECK_RESULT(edge_compact.kernel.allocateDescriptorSet(desc_pool, &desc_sets.edge_compact,1));
	VK_CHECK_RESULT(gen_vertices.kernel.allocateDescriptorSet(desc_pool, &desc_sets.gen_vertices, 1));
	VK_CHECK_RESULT(gen_faces.kernel.allocateDescriptorSet(desc_pool, &desc_sets.gen_faces, 1));
	VK_CHECK_RESULT(gen_normals.kernel.allocateDescriptorSet(desc_pool, &desc_sets.gen_normals, 1));

	LOG("MachingCube::buildKernels()\n");
	edge_test.kernel.build(cache, nullptr);
	cell_test.kernel.build(cache, nullptr);
	edge_compact.kernel.build(cache, nullptr);
	gen_vertices.kernel.build(cache, nullptr);
	gen_faces.kernel.build(cache, nullptr);
	gen_normals.kernel.build(cache, nullptr);
	edge_scan.build();
	cell_scan.build();
	LOG("MarchingCube::setupKernel() end()\n");
}

void MarchingCube::destroy(){
	VkDevice device = VkDevice(*ctx);
	destroyBuffers();
	destroyKernels();
	freeCommandBuffers();
	if(fence)
		queue->destroyFence(fence);
		fence = VK_NULL_HANDLE;
	if(desc_pool){
		vkDestroyDescriptorPool(device, desc_pool, nullptr);
		desc_pool = VK_NULL_HANDLE;
	}

	if(cache){
		vkDestroyPipelineCache(device, cache,nullptr);
		cache = VK_NULL_HANDLE;
	}

}

void MarchingCube::setupEdgeTestCommand(){
	edge_test.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	edge_test.kernel.setKernelArgs(desc_sets.edge_test,{
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &edge_test.d_output.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	//queue->bindKernel(edge_test.command, &edge_test.kernel);
	queue->bindPipeline(edge_test.command, VK_PIPELINE_BIND_POINT_COMPUTE, edge_test.kernel.pipeline);
	queue->bindDescriptorSets(edge_test.command, VK_PIPELINE_BIND_POINT_COMPUTE, edge_test.kernel.layouts.pipeline, 0, &desc_sets.edge_test, 1, 0, nullptr);
	queue->dispatch(edge_test.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->setEvent(edge_test.command, edge_test.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	queue->endCommandBuffer(edge_test.command);
}

void MarchingCube::setupCellTestCommand(){
	cell_test.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	cell_test.kernel.setKernelArgs(desc_sets.cell_test,{
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_celltype.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_tricount.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	queue->bindPipeline(cell_test.command, VK_PIPELINE_BIND_POINT_COMPUTE, cell_test.kernel.pipeline);
	queue->bindDescriptorSets(cell_test.command, VK_PIPELINE_BIND_POINT_COMPUTE, cell_test.kernel.layouts.pipeline, 0, &desc_sets.cell_test, 1, 0, nullptr);
	queue->dispatch(cell_test.command, (meta_info.x + 3) / 4, (meta_info.y+3)/4, (meta_info.z+3)/4 );
	queue->setEvent(cell_test.command, cell_test.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	queue->endCommandBuffer(cell_test.command);
}

void MarchingCube::setupEdgeCompactCommand(){
	edge_compact.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	edge_compact.kernel.setKernelArgs(desc_sets.edge_compact,{
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &edge_test.d_output.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr},
	});
	queue->bindPipeline(edge_compact.command, VK_PIPELINE_BIND_POINT_COMPUTE, edge_compact.kernel.pipeline);
	queue->bindDescriptorSets(edge_compact.command, VK_PIPELINE_BIND_POINT_COMPUTE, edge_compact.kernel.layouts.pipeline, 0, &desc_sets.edge_compact, 1, 0, nullptr);
	VkBufferMemoryBarrier input_barriers[2] = {
		scan.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		edge_test.d_output.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[2] = {
		edge_test.event,
		edge_scan.event
	};
	queue->waitEvents(edge_compact.command, wait_events,  2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, input_barriers, 2, nullptr, 0 );
	queue->dispatch(edge_compact.command, (3*meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3/4));
	queue->setEvent(edge_compact.command, edge_compact.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	queue->endCommandBuffer(edge_compact.command);
}

void MarchingCube::setupGenVerticesCommand(){
	gen_vertices.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	gen_vertices.kernel.setKernelArgs(desc_sets.gen_vertices,{
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr},
	});
	queue->bindPipeline(gen_vertices.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_vertices.kernel.pipeline);
	queue->bindDescriptorSets(gen_vertices.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_vertices.kernel.layouts.pipeline, 0, &desc_sets.gen_vertices, 1, 0, nullptr);
	VkBufferMemoryBarrier barriers[2] = {
		outputs.vertices.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		scan.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[2] = {
		edge_scan.event,
		edge_compact.event
	};
	queue->waitEvents(gen_vertices.command, wait_events, 2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, barriers, 2, nullptr, 0 );
	queue->dispatch(gen_vertices.command, (3*meta_info.x + 3)/4, (meta_info.y+3)/4,(meta_info.z+3)/4);
	queue->endCommandBuffer(gen_vertices.command);
}

void MarchingCube::setupGenFacesCommand(){
	gen_faces.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	gen_faces.kernel.setKernelArgs(desc_sets.gen_faces,{
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.indices.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_celltype.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_tricount.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_cpsum.descriptor, nullptr},
		{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &general.d_cast_table.descriptor, nullptr},
		{6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	VkBufferMemoryBarrier barriers[4] = {
		cell_test.d_celltype.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		cell_test.d_tricount.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		scan.d_cpsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		scan.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[3] = {
		cell_test.event,
		edge_scan.event,
		cell_scan.event
	};
	queue->waitEvents(gen_faces.command, wait_events, 3, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, barriers, 4, nullptr ,0 );
	queue->bindPipeline(gen_faces.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_faces.kernel.pipeline);
	queue->bindDescriptorSets(gen_faces.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_faces.kernel.layouts.pipeline, 0, &desc_sets.gen_faces, 1, 0, nullptr);
	queue->dispatch(gen_faces.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->endCommandBuffer(gen_faces.command);
}

void MarchingCube::setupGenNormalCommand(){
	gen_normals.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	gen_normals.kernel.setKernelArgs(desc_sets.gen_normals,{
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.normals.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_cpsum.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.indices.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	queue->bindPipeline(gen_normals.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_normals.kernel.pipeline);
	queue->bindDescriptorSets(gen_normals.command, VK_PIPELINE_BIND_POINT_COMPUTE, gen_normals.kernel.layouts.pipeline, 0, &desc_sets.gen_normals, 1, 0, nullptr);
	queue->dispatch(gen_normals.command, (meta_info.x * 3 +3)/4 , (meta_info.y + 3)/ 4, (meta_info.z + 3)/ 4);
	queue->endCommandBuffer(gen_normals.command);
}

void MarchingCube::setupCommandBuffers(){
		
	VK_CHECK_RESULT(ctx->createFence(&fence));
	VK_CHECK_RESULT(ctx->createEvent(&edge_test.event));
	VK_CHECK_RESULT(ctx->createEvent(&cell_test.event));
	VK_CHECK_RESULT(ctx->createEvent(&edge_compact.event));
	setupEdgeTestCommand();
	setupCellTestCommand();
	edge_scan.setupCommandBuffer(&edge_test.d_output, &scan.d_epsum, edge_test.event);
	cell_scan.setupCommandBuffer(&cell_test.d_tricount, &scan.d_cpsum, cell_test.event);
	setupEdgeCompactCommand();
	setupGenVerticesCommand();
	setupGenFacesCommand();
	setupGenNormalCommand();
}

void MarchingCube::run(VkSemaphore *wait_semaphores, uint32_t nr_waits, VkSemaphore *signal_semaphores, uint32_t nr_signals){
	// VkCommandBuffer testing[2] = {
		// edge_test.command,
		// cell_test.command
	// };

	VkCommandBuffer commands[7] = {
		edge_test.command,
		cell_test.command,
		edge_scan.command,
		cell_scan.command,
		edge_compact.command,
		gen_vertices.command,
		gen_faces.command
	};
	//queue->resetFences(&fence,1);
	
	//queue->submit(testing, 2, 0, 
	//		wait_semaphores, nr_waits, nullptr, 0, fence);
	//queue->waitFences(&fence,1);

	//edge_scan.run(&edge_test.d_output, &scan.d_epsum);
	//cell_scan.run(&cell_test.d_tricount, &scan.d_cpsum);
	//queue->waitIdle();
	//queue->resetFences(&fence, 1)
	//VkCommandBuffer
	//queue->submit();
	//queue->submit(&edge_compact.command, 1, 0 , nullptr, 0, nullptr, 0, fence);
	//queue->waitFences(&fence, 1);
	//queue->waitIdle();
	// gen 
	// VkCommandBuffer gen[2] = {
		// gen_vertices.command,
		// gen_faces.command
	// };

	// queue->resetFences(&fence, 1);
	// queue->submit(gen, 2, 0, nullptr, 0, nullptr, 0, fence);
	// queue->waitFences(&fence, 1);
	queue->resetFences(&fence, 1);
	queue->submit(commands, 7, 0, nullptr, 0, nullptr, 0, fence);
	queue->waitFences(&fence, 1);
	// queue->resetFences(&fence, 1);
	// /queue->submit(&commands[2], 2, 0, nullptr, 0, nullptr, 0, fence);
	// queue->waitFences(&fence, 1);
	queue->waitIdle();
	//queue->resetFences(&fence, 1);
	//queue->submit(&gen_normals.command, 1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, signal_semaphores, nr_signals ,fence);
	//queue->waitFences(&fence, 1);
}

void MarchingCube::destroyBuffers(){
	LOG("MarchingCube::destroyBuffers()\n");
	general.d_volume.destroy();
	general.d_metainfo.destroy();
	general.d_cast_table.destroy();
	edge_test.d_output.destroy();
	cell_test.d_celltype.destroy();
	cell_test.d_tricount.destroy();
	scan.d_cpsum.destroy();
	scan.d_epsum.destroy();
	outputs.vertices.destroy();
	outputs.indices.destroy();
	outputs.normals.destroy();
}

void MarchingCube::destroyKernels(){
	LOG("MarchingCube::destroyKernels()\n");
	VkDevice device = VkDevice(*ctx);
	VkDescriptorSet* sets[6] = {
		&desc_sets.edge_test,
		&desc_sets.cell_test,
		&desc_sets.edge_compact,
		&desc_sets.gen_vertices,
		&desc_sets.gen_faces,
		&desc_sets.gen_normals
	};

	for(uint32_t i = 0 ; i <6 ; i++){
		if(sets[i] != VK_NULL_HANDLE){
			vkFreeDescriptorSets(device, desc_pool, 1, sets[i]);
			*sets[i] = VK_NULL_HANDLE;
		}
	}
	edge_test.kernel.destroy();
	cell_test.kernel.destroy();
	edge_compact.kernel.destroy();
	gen_vertices.kernel.destroy();
	gen_faces.kernel.destroy();
	gen_normals.kernel.destroy();
}

void MarchingCube::freeCommandBuffers(){
	LOG("MarchingCube::freeCommandBuffers()\n");
	VkDevice device = VkDevice(*ctx);
	VkCommandBuffer commands[6] = {
		edge_test.command,
		cell_test.command,
		edge_compact.command,
		gen_vertices.command,
		gen_faces.command,
		gen_normals.command
	};
	for(uint32_t i = 0 ; i < 6 ; i++){
		if(commands[i])
			queue->free(commands[i]);
	}
	edge_test.command=VK_NULL_HANDLE;
	cell_test.command=VK_NULL_HANDLE;
	edge_compact.command = VK_NULL_HANDLE;
	gen_vertices.command = VK_NULL_HANDLE;
	gen_faces.command = VK_NULL_HANDLE;
	gen_normals.command = VK_NULL_HANDLE;
}


}

#endif
