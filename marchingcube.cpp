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
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							(meta_info.x * meta_info.y * meta_info.z * sizeof(float) * 3), nullptr);
	outputs.indices.create(ctx, 
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							meta_info.x * meta_info.y * meta_info.z, nullptr);

	uint32_t h_cast_table[12] = {
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
	vector<VkDescriptorPoolSize> pool_sizes = {
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 15),
		infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6)
	};
	VkDescriptorPoolCreateInfo desc_pool_CI = infos::descriptorPoolCreateInfo(
		static_cast<uint32_t>(pool_sizes.size()),
		pool_sizes.data(),
		7
	);
	VK_CHECK_RESULT(vkCreateDescriptorPool(VkDevice(*ctx), &desc_pool_CI, nullptr, &desc_pool));
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


	edge_scan.init(meta_info.x * meta_info.y * meta_info.z * 3);
	cell_scan.init(meta_info.x * meta_info.y * meta_info.z);
	LOG("MarchingCube::allocateDscriptorSet()\n");
	edge_test.kernel.allocateDescriptorSet(desc_pool);
	cell_test.kernel.allocateDescriptorSet(desc_pool);
	edge_compact.kernel.allocateDescriptorSet(desc_pool);
	gen_vertices.kernel.allocateDescriptorSet(desc_pool);
	gen_faces.kernel.allocateDescriptorSet(desc_pool);

	LOG("MachingCube::buildKernels()\n");
	edge_test.kernel.build(cache, nullptr);
	cell_test.kernel.build(cache, nullptr);
	edge_compact.kernel.build(cache, nullptr);
	gen_vertices.kernel.build(cache, nullptr);
	gen_faces.kernel.build(cache, nullptr);
	edge_scan.build();
	cell_scan.build();
	LOG("MarchingCube::setupKernel() end()\n");
}

void MarchingCube::destroy(){
	VkDevice device = VkDevice(*ctx);
	destroyBuffers();
	
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
	edge_test.kernel.setKernelArgs({
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &edge_test.d_output.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	queue->bindKernel(edge_test.command, &edge_test.kernel);
	queue->dispatch(edge_test.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->endCommandBuffer(edge_test.command);
}

void MarchingCube::setupCellTestCommand(){
	cell_test.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	cell_test.kernel.setKernelArgs({
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_celltype.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_tricount.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	queue->bindKernel(cell_test.command, &cell_test.kernel);
	queue->dispatch(cell_test.command, (meta_info.x + 3) / 4, (meta_info.y+3)/4, (meta_info.z+3)/4 );
	queue->endCommandBuffer(cell_test.command);
}

void MarchingCube::setupEdgeCompactCommand(){
	edge_compact.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	edge_compact.kernel.setKernelArgs({
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &edge_test.d_output.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr},
	});
	queue->bindKernel(edge_compact.command, &edge_compact.kernel);
	queue->dispatch(edge_compact.command, (3*meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3/4));
	queue->endCommandBuffer(edge_compact.command);
}

void MarchingCube::setupGenVerticesCommand(){
	gen_vertices.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	gen_vertices.kernel.setKernelArgs({
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &general.d_volume.descriptor},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.vertices.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr},
	});
	queue->bindKernel(gen_vertices.command, &gen_vertices.kernel);
	queue->dispatch(gen_vertices.command, (3*meta_info.x + 3)/4, (meta_info.y+3)/4,(meta_info.z+3)/4);
	queue->endCommandBuffer(gen_vertices.command);
}

void MarchingCube::setupGenFacesCommand(){
	gen_faces.command = queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, true);
	gen_faces.kernel.setKernelArgs({
		{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputs.indices.descriptor, nullptr},
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_celltype.descriptor, nullptr},
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &cell_test.d_tricount.descriptor, nullptr},
		{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_cpsum.descriptor, nullptr},
		{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &scan.d_epsum.descriptor, nullptr},
		{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &general.d_cast_table.descriptor, nullptr},
		{6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &general.d_metainfo.descriptor, nullptr}
	});
	queue->bindKernel(gen_faces.command, &gen_faces.kernel);
	// queue->dispatch(gen_faces.command, (meta_info.x + 3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->dispatch(gen_faces.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->endCommandBuffer(gen_faces.command);
}

void MarchingCube::setupCommandBuffers(){
	fences[0] = queue->createFence();
	setupEdgeTestCommand();
	setupCellTestCommand();
	setupEdgeCompactCommand();
	setupGenVerticesCommand();
	setupGenFacesCommand();
}

void MarchingCube::run(){
	VkCommandBuffer testing[2] = {
		edge_test.command,
		cell_test.command
	};
	queue->resetFences(&fences[0],1);
	queue->submit(testing, 2, 0, nullptr, 0, nullptr, 0, fences[0]);
	queue->waitFences(&fences[0],1);

	edge_scan.run(&edge_test.d_output, &scan.d_epsum);
	cell_scan.run(&cell_test.d_tricount, &scan.d_cpsum);
	//queue->waitIdle();
	queue->resetFences(&fences[0], 1);
	queue->submit(&edge_compact.command, 1, 0 , nullptr, 0, nullptr, 0, fences[0]);
	queue->waitFences(&fences[0], 1);
	//queue->waitIdle();
	// gen 
	VkCommandBuffer gen[2] = {
		gen_vertices.command,
		gen_faces.command
	};
	queue->resetFences(&fences[0], 1);
	queue->submit(gen, 2, 0, nullptr, 0, nullptr, 0, fences[0]);
	queue->waitFences(&fences[0], 1);
}

void MarchingCube::destroyBuffers(){
	general.d_volume.destroy();
}


}

#endif