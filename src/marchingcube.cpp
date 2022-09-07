#ifndef __MARCHINGCUBE_CPP__
#define __MARCHINGCUBE_CPP__

#include "marchingcube.h"

using namespace VKEngine;

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
	edge_scan.create(ctx, queue);
	cell_scan.create(ctx, queue);
}

void MarchingCube::setupBuilders()
{
	LOG("MarchingCube::setupBuilders() \n");
	builders.edge = new ComputePipelineBuilder(ctx);
	builders.cell = new ComputePipelineBuilder(ctx);
	builders.compact = new ComputePipelineBuilder(ctx);
	builders.vertex = new ComputePipelineBuilder(ctx);
	builders.face = new ComputePipelineBuilder(ctx);
	builders.normal = new ComputePipelineBuilder(ctx);
	builders.descriptor = new DescriptorSetBuilder(ctx);
	
	builders.edge->setComputeShader("shaders/edge_test.comp.spv");
	builders.cell->setComputeShader("shaders/cell_test.comp.spv");
	builders.compact->setComputeShader("shaders/edge_compact.comp.spv");
	builders.vertex->setComputeShader("shaders/gen_vertices.comp.spv");
	builders.face->setComputeShader("shaders/gen_faces.comp.spv");
	builders.normal->setComputeShader("shaders/gen_normals.comp.spv");

	VkDescriptorPoolSize size[3];
	size[0] = DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4);
	size[1] = DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 19);
	size[2] = DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 9);
	builders.descriptor->setDescriptorPool(size, 3, 10);
	LOG("MarchingCube::setupBuilders() end\n");
}

void MarchingCube::setupDescriptors()
{
	LOG("MarchingCube::setupDescriptors() \n");
	vector<VkDescriptorSetLayoutBinding> bindings(3);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.edge.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.edge.set, &progs.edge.d_layout, 1));
	bindings.resize(4);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.cell.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.cell.set, &progs.cell.d_layout, 1));
	bindings.resize(4);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.compact.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.compact.set, &progs.compact.d_layout, 1));
	bindings.resize(5);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[4] = DescriptorSetLayoutBuilder::bind(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.vertex.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.vertex.set, &progs.vertex.d_layout, 1));
	bindings.resize(7);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[4] = DescriptorSetLayoutBuilder::bind(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[5] = DescriptorSetLayoutBuilder::bind(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[6] = DescriptorSetLayoutBuilder::bind(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.face.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.face.set, &progs.face.d_layout, 1));
	bindings.resize(5);
	bindings[0] = DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[1] = DescriptorSetLayoutBuilder::bind(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[2] = DescriptorSetLayoutBuilder::bind(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[3] = DescriptorSetLayoutBuilder::bind(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	bindings[4] = DescriptorSetLayoutBuilder::bind(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(ctx, &progs.normal.d_layout, bindings));
	VK_CHECK_RESULT(builders.descriptor->build(&progs.normal.set, &progs.normal.d_layout, 1));
	LOG("MarchingCube::setupDescriptors() end \n");

	edge_scan.init(meta_info.x * meta_info.y * meta_info.z * 3);
	cell_scan.init(meta_info.x * meta_info.y * meta_info.z);
}

void MarchingCube::writeDescriptors()
{
	LOG("MarchingCube::writeDescriptors() \n");
	vector<VkWriteDescriptorSet> writes(3);
	writes[0]=DescriptorWriter::writeImage(progs.edge.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &buffers.d_volume);
	writes[1]=DescriptorWriter::writeBuffer(progs.edge.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &buffers.d_edgeout);
	writes[2]=DescriptorWriter::writeBuffer(progs.edge.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 3);
	writes.resize(4);
	writes[0]=DescriptorWriter::writeImage(progs.cell.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &buffers.d_volume);
	writes[1]=DescriptorWriter::writeBuffer(progs.cell.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &buffers.d_celltype);
	writes[2]=DescriptorWriter::writeBuffer(progs.cell.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &buffers.d_tricount);
	writes[3]=DescriptorWriter::writeBuffer(progs.cell.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 4);
	writes.resize(4);
	writes[0]=DescriptorWriter::writeBuffer(progs.compact.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &buffers.d_edgeout);
	writes[1]=DescriptorWriter::writeBuffer(progs.compact.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &buffers.d_epsum);
	writes[2]=DescriptorWriter::writeBuffer(progs.compact.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &buffers.vertices);
	writes[3]=DescriptorWriter::writeBuffer(progs.compact.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 4);
	writes.resize(5);
	writes[0]=DescriptorWriter::writeBuffer(progs.vertex.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &buffers.vertices);
	writes[1]=DescriptorWriter::writeImage(progs.vertex.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &buffers.d_volume);
	writes[2]=DescriptorWriter::writeBuffer(progs.vertex.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &buffers.vertices);
	writes[3]=DescriptorWriter::writeBuffer(progs.vertex.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &buffers.d_epsum);
	writes[4]=DescriptorWriter::writeBuffer(progs.vertex.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 5);
	writes.resize(7);
	writes[0]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &buffers.indices);
	writes[1]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &buffers.d_celltype);
	writes[2]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &buffers.d_tricount);
	writes[3]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &buffers.d_cpsum);
	writes[4]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &buffers.d_epsum);
	writes[5]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &buffers.d_caster);
	writes[6]=DescriptorWriter::writeBuffer(progs.face.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 7);
	writes.resize(5);
	writes[0]=DescriptorWriter::writeBuffer(progs.normal.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &buffers.normals);
	writes[1]=DescriptorWriter::writeBuffer(progs.normal.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &buffers.vertices);
	writes[2]=DescriptorWriter::writeBuffer(progs.normal.set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &buffers.d_epsum);
	writes[3]=DescriptorWriter::writeImage(progs.normal.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &buffers.d_volume);
	writes[4]=DescriptorWriter::writeBuffer(progs.normal.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &buffers.d_metainfo);
	DescriptorWriter::update(ctx, writes.data(), 5);
	LOG("MarchingCube::writeDescriptors() end\n");
}

void MarchingCube::setupPipelineLayouts()
{
	LOG("MarchingCube::setupPipelineLayouts() \n");
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.edge.p_layout, &progs.edge.d_layout, 1));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.cell.p_layout, &progs.cell.d_layout, 1));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.compact.p_layout, &progs.compact.d_layout, 1));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.vertex.p_layout, &progs.vertex.d_layout, 1));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.face.p_layout, &progs.face.d_layout, 1));
	VK_CHECK_RESULT(PipelineLayoutBuilder::build(ctx, &progs.normal.p_layout, &progs.normal.d_layout, 1));
	LOG("MarchingCube::setupPipelineLayouts() end\n");
}

void MarchingCube::setupPipelines()
{
	LOG("MarchingCube::setupPipelines() \n");
	VK_CHECK_RESULT(PipelineCacheBuilder::build(ctx, &cache));
	LOG("cache complete\n");
	VK_CHECK_RESULT(builders.edge->build(&progs.edge.pipeline, progs.edge.p_layout, cache));
	LOG("edge complete\n");
	VK_CHECK_RESULT(builders.cell->build(&progs.cell.pipeline, progs.cell.p_layout, cache));
	VK_CHECK_RESULT(builders.compact->build(&progs.compact.pipeline, progs.compact.p_layout, cache));
	VK_CHECK_RESULT(builders.vertex->build(&progs.vertex.pipeline, progs.vertex.p_layout, cache));
	VK_CHECK_RESULT(builders.face->build(&progs.face.pipeline, progs.face.p_layout, cache));
	VK_CHECK_RESULT(builders.normal->build(&progs.normal.pipeline, progs.normal.p_layout, cache));
	LOG("MarchingCube::setupPipelines() end \n");
	
	edge_scan.build();
	cell_scan.build();

}

void MarchingCube::setVolumeSize(uint32_t x, uint32_t y, uint32_t z)
{
	meta_info.x = x;
	meta_info.y = y;
	meta_info.z = z;
	meta_info.isovalue = 0.0f;
}

void MarchingCube::setIsovalue(float value){
	LOG("MarchingCube::setIsoValue() \n");
	meta_info.isovalue = value;
	buffers.d_metainfo.copyFrom(&meta_info, sizeof(MetaInfo));
	LOG("MarchingCube::setIsoValue() end\n");
}

void MarchingCube::setVolume(void *ptr){
	LOG("MarchingCube::setVolume() \n");
	VkFence _fence;
	VK_CHECK_RESULT(ctx->createFence(&_fence));
	Buffer staging = Buffer(ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		meta_info.x * meta_info.y * meta_info.z * sizeof(float), ptr);
	
	//staging.map();
	
	VkCommandBuffer copy;
	VK_CHECK_RESULT(queue->createCommandBuffer(&copy, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));
	VK_CHECK_RESULT(queue->beginCommandBuffer(copy));
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;
	buffers.d_volume.setLayout(copy, VK_IMAGE_ASPECT_COLOR_BIT, 
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
	queue->copyBufferToImage(copy, &staging, &buffers.d_volume, &buffer_image);
	buffers.d_volume.setLayout(copy, VK_IMAGE_ASPECT_COLOR_BIT, 
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								range, 
								VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);

	queue->endCommandBuffer(copy);
	queue->resetFences(&_fence, 1);
	queue->submit(&copy, 1, 0, nullptr, 0, nullptr, 0, _fence);
	queue->waitFences(&_fence, 1);
	queue->waitIdle();
	queue->free(copy);
	ctx->destroyFence(&_fence);
	staging.destroy();
	LOG("MarchingCube::setVolume() end\n");
}

void MarchingCube::setupBuffers(){
	LOG("MarchingCube::setupBuffers() \n");
	buffers.d_volume.create(ctx);
	VK_CHECK_RESULT(buffers.d_volume.createImage(meta_info.x, meta_info.y, meta_info.z, VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
								VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, 1, 1));
	LOG("MarchingCube::createImage()\n");
	VK_CHECK_RESULT(buffers.d_volume.alloc(sizeof(float) * meta_info.x * meta_info.y * meta_info.z, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	LOG("MarchingCube::alloc()\n");
	VK_CHECK_RESULT(buffers.d_volume.bind(0));
	LOG("MarchingCube::bind()\n");
	VK_CHECK_RESULT(buffers.d_volume.createImageView(VK_IMAGE_VIEW_TYPE_3D, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
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
	VK_CHECK_RESULT(buffers.d_volume.createSampler(&sampler_CI));
	buffers.d_volume.setupDescriptor();

	buffers.d_metainfo.create(ctx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
								sizeof(MetaInfo), &meta_info);
	LOG("MarchingCube::createSampler()\n");

	buffers.d_edgeout.create(ctx,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t) * 3, nullptr);
	buffers.d_celltype.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	buffers.d_tricount.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	buffers.d_epsum.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t) * 3, nullptr);
	buffers.d_cpsum.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t), nullptr);
	buffers.vertices.create(ctx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,6*(meta_info.x * meta_info.y * meta_info.z * sizeof(float)), nullptr);
	buffers.indices.create(ctx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,3*(meta_info.x * meta_info.y * meta_info.z * sizeof(uint32_t)), nullptr);
	buffers.normals.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,4, nullptr);

	uint32_t h_cast_table[12] ={0,4,meta_info.x*3,1,3*meta_info.x*meta_info.y,3*(meta_info.x*meta_info.y + 1) + 1,3*(meta_info.x*meta_info.y + meta_info.x),3*(meta_info.x*meta_info.y) + 1,2,5,3*(meta_info.x+ 1) + 2,3*meta_info.x + 2};
	buffers.d_caster.create(ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,12*sizeof(uint32_t), nullptr);
	enqueueCopyBuffer(queue, h_cast_table, &buffers.d_caster, 0, 0, sizeof(uint32_t) * 12);
	LOG("MarchingCube::setupBuffers() end\n");
}

void MarchingCube::setupEdgeTestCommand(){
	LOG("MarchingCube::setupEdgeTestCommand() \n");
	VK_CHECK_RESULT(queue->createCommandBuffer(&progs.edge.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0));
	VK_CHECK_RESULT(queue->beginCommandBuffer(progs.edge.command));
	vkCmdFillBuffer(progs.edge.command, buffers.vertices.getBuffer(), 0, buffers.vertices.getSize(), 0);
	vkCmdFillBuffer(progs.edge.command, buffers.indices.getBuffer(), 0, buffers.indices.getSize(), 0);
	queue->bindPipeline(progs.edge.command, VK_PIPELINE_BIND_POINT_COMPUTE, progs.edge.pipeline);
	queue->bindDescriptorSets(progs.edge.command, VK_PIPELINE_BIND_POINT_COMPUTE, progs.edge.p_layout, 0, &progs.edge.set, 1, 0, nullptr);
	queue->dispatch(progs.edge.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	queue->setEvent(progs.edge.command, progs.edge.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	VK_CHECK_RESULT(queue->endCommandBuffer(progs.edge.command));
	LOG("MarchingCube::setupEdgeTestCommand() end \n");
}

void MarchingCube::setupCellTestCommand(){
	LOG("MarchingCube::setupCellTestCommand() \n");
	Program &cell=progs.cell;
	VK_CHECK_RESULT( queue->createCommandBuffer(&cell.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY) );
	VK_CHECK_RESULT(queue->beginCommandBuffer(cell.command));
	queue->bindPipeline(cell.command, VK_PIPELINE_BIND_POINT_COMPUTE, cell.pipeline);
	queue->bindDescriptorSets(cell.command, VK_PIPELINE_BIND_POINT_COMPUTE, cell.p_layout, 0, &cell.set, 1, 0, nullptr);
	queue->dispatch(cell.command, (meta_info.x + 3) / 4, (meta_info.y+3)/4, (meta_info.z+3)/4 );
	queue->setEvent(cell.command, cell.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	VK_CHECK_RESULT(queue->endCommandBuffer(cell.command));
	LOG("MarchingCube::setupCellTestCommand() end\n");
}

void MarchingCube::setupEdgeCompactCommand(){
	LOG("MarchingCube::setupEdgeCompactCommand() \n");
	Program &compact = progs.compact;
	VK_CHECK_RESULT(queue->createCommandBuffer(&compact.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	VK_CHECK_RESULT(queue->beginCommandBuffer(compact.command));
	queue->bindPipeline(compact.command, VK_PIPELINE_BIND_POINT_COMPUTE, compact.pipeline);
	queue->bindDescriptorSets(compact.command, VK_PIPELINE_BIND_POINT_COMPUTE, compact.p_layout, 0, &compact.set, 1, 0, nullptr);
	VkBufferMemoryBarrier input_barriers[2] = {
		buffers.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		buffers.d_edgeout.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[2] = {
		progs.edge.event,
		edge_scan.event
	};
	queue->waitEvents(compact.command, wait_events,  2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, input_barriers, 2, nullptr, 0 );
	queue->dispatch(compact.command, (3*meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3/4));
	queue->setEvent(compact.command, compact.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	VK_CHECK_RESULT(queue->endCommandBuffer(compact.command));
	LOG("MarchingCube::setupEdgeCompactCommand() end\n");
}

void MarchingCube::setupGenVerticesCommand(){
	LOG("MarchingCube::setupGenVerticesCommand() \n");
	Program &vertices = progs.vertex;
	VK_CHECK_RESULT(queue->createCommandBuffer(&vertices.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	VK_CHECK_RESULT(queue->beginCommandBuffer(vertices.command));
	queue->bindPipeline(vertices.command, VK_PIPELINE_BIND_POINT_COMPUTE, vertices.pipeline);
	queue->bindDescriptorSets(vertices.command, VK_PIPELINE_BIND_POINT_COMPUTE, vertices.p_layout, 0, &vertices.set, 1, 0, nullptr);
	VkBufferMemoryBarrier barriers[2] = {
		buffers.vertices.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		buffers.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[2] = {
		edge_scan.event,
		progs.compact.event
	};
	queue->waitEvents(vertices.command, wait_events, 2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, barriers, 2, nullptr, 0 );
	queue->dispatch(vertices.command, (3*meta_info.x + 3)/4, (meta_info.y+3)/4,(meta_info.z+3)/4);
	queue->setEvent(vertices.command, vertices.event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	VK_CHECK_RESULT(queue->endCommandBuffer(vertices.command));
	LOG("MarchingCube::setupGenVerticesCommand() end()\n");
}

void MarchingCube::setupGenFacesCommand(){
	LOG("MarchingCube::setupGenFacesCommand() \n");
	Program &face = progs.face;
	VK_CHECK_RESULT(queue->createCommandBuffer(&face.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	VK_CHECK_RESULT(queue->beginCommandBuffer(face.command));
	VkBufferMemoryBarrier barriers[4] = {
		buffers.d_celltype.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		buffers.d_tricount.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		buffers.d_cpsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		buffers.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[3] = {
		progs.cell.event,
		edge_scan.event,
		cell_scan.event
	};
	queue->waitEvents(face.command, wait_events, 3, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, barriers, 4, nullptr ,0 );
	queue->bindPipeline(face.command, VK_PIPELINE_BIND_POINT_COMPUTE, face.pipeline);
	queue->bindDescriptorSets(face.command, VK_PIPELINE_BIND_POINT_COMPUTE, face.p_layout, 0, &face.set, 1, 0, nullptr);
	queue->dispatch(face.command, (meta_info.x+3)/4, (meta_info.y+3)/4, (meta_info.z+3)/4);
	VK_CHECK_RESULT(queue->endCommandBuffer(face.command));
	LOG("MarchingCube::setupGenFacesCommand() end\n");
}

void MarchingCube::setupGenNormalCommand(){
	LOG("MarchingCube::setupGenNormalsCommand() \n");
	Program &normal = progs.normal;
	VK_CHECK_RESULT(queue->createCommandBuffer(&normal.command, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	VK_CHECK_RESULT(queue->beginCommandBuffer(normal.command));
	VkBufferMemoryBarrier barrier[2] = {
		buffers.vertices.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		buffers.d_epsum.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
	};
	VkEvent wait_events[2] ={
		progs.vertex.event,
		edge_scan.event
	};
	queue->waitEvents(normal.command, wait_events, 2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, nullptr, 0, barrier, 2, nullptr, 0);
	queue->bindPipeline(normal.command, VK_PIPELINE_BIND_POINT_COMPUTE, normal.pipeline);
	queue->bindDescriptorSets(normal.command, VK_PIPELINE_BIND_POINT_COMPUTE, normal.p_layout, 0, &normal.set, 1, 0, nullptr);
	queue->dispatch(normal.command, (meta_info.x * 3 +3)/4 , (meta_info.y + 3)/ 4, (meta_info.z + 3)/ 4);
	VK_CHECK_RESULT(queue->endCommandBuffer(normal.command));
	LOG("MarchingCube::setupGenNormalsCommand() end\n");
}

void MarchingCube::setupEvents()
{
	LOG("MarchingCube::setupEvents() \n");
	VK_CHECK_RESULT(ctx->createFence(&fence));
	VK_CHECK_RESULT(ctx->createEvent(&progs.edge.event));
	VK_CHECK_RESULT(ctx->createEvent(&progs.cell.event));
	VK_CHECK_RESULT(ctx->createEvent(&progs.compact.event));
	VK_CHECK_RESULT(ctx->createEvent(&progs.vertex.event));
	LOG("MarchingCube::setupEvents() end\n");
}

void MarchingCube::setupCommandBuffers(){
	LOG("MarchingCube::setupCommandBuffers() \n");
	setupEdgeTestCommand();
	setupCellTestCommand();
	edge_scan.setupCommandBuffer(&buffers.d_edgeout, &buffers.d_epsum, progs.edge.event);
	cell_scan.setupCommandBuffer(&buffers.d_tricount, &buffers.d_cpsum, progs.cell.event);
	setupEdgeCompactCommand();
	setupGenVerticesCommand();
	setupGenFacesCommand();
	setupGenNormalCommand();
	LOG("MarchingCube::setupCommandBuffers() end \n");
}

void MarchingCube::run(VkSemaphore *wait_semaphores, uint32_t nr_waits, VkSemaphore *signal_semaphores, uint32_t nr_signals){
	VkCommandBuffer commands[8] = {
		progs.edge.command,
		progs.cell.command,
		cell_scan.command,
		edge_scan.command,
		progs.compact.command,
		progs.vertex.command,
		progs.face.command,
		progs.normal.command,
	};
	queue->resetFences(&fence, 1);
	VkPipelineStageFlags wait_flags = (nr_signals > 0) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0x0;
	VK_CHECK_RESULT(queue->submit(commands, 8, &wait_flags, wait_semaphores, nr_waits, signal_semaphores, nr_signals, fence));
	queue->waitFences(&fence, 1);
}


void MarchingCube::destroy(){
	LOG("MarchingCube::destroy()\n");		
	//edge_scan.destroy();
	//cell_scan.destroy();
	freeCommandBuffers();
	destroyPipelines();
	destroyDescriptors();
	destroyEvents();
	destroyBuffers();
	destroyBuilders();
	LOG("MarchingCube::destroy() end\n");		
}

void MarchingCube::destroyEvents(){
	ctx->destroyFence(&fence);
	ctx->destroyEvent(&progs.edge.event);
	ctx->destroyEvent(&progs.cell.event);
	ctx->destroyEvent(&progs.compact.event);
	ctx->destroyEvent(&progs.vertex.event);
}


void MarchingCube::destroyBuffers(){
	LOG("MarchingCube::destroyBuffers()\n");
	buffers.d_volume.destroy();
	buffers.d_metainfo.destroy();
	buffers.d_caster.destroy();
	buffers.d_edgeout.destroy();
	buffers.d_celltype.destroy();
	buffers.d_tricount.destroy();
	buffers.d_cpsum.destroy();
	buffers.d_epsum.destroy();
	buffers.vertices.destroy();
	buffers.indices.destroy();
	// buffers.normals.destroy();
	LOG("MarchingCube::destroyBuffers() end\n");
}

void MarchingCube::destroyDescriptors(){
	LOG("MarchingCube::destroyKernels()\n");
	//descriptorLayout
	builders.descriptor->free(&progs.edge.set,1);
	builders.descriptor->free(&progs.cell.set,1);
	builders.descriptor->free(&progs.compact.set,1);
	builders.descriptor->free(&progs.vertex.set,1);
	builders.descriptor->free(&progs.face.set,1);
	builders.descriptor->free(&progs.normal.set,1);

	DescriptorSetLayoutBuilder::destroy(ctx, &progs.edge.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.cell.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.compact.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.vertex.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.face.d_layout);
	DescriptorSetLayoutBuilder::destroy(ctx, &progs.normal.d_layout);
	LOG("MarchingCube::destroyKernels() end\n");
}

void MarchingCube::freeCommandBuffers(){
	LOG("MarchingCube::freeCommandBuffers()\n");
	queue->free(&progs.edge.command, 1);
	queue->free(&progs.cell.command, 1);
	queue->free(&progs.compact.command, 1);
	queue->free(&progs.vertex.command, 1);
	queue->free(&progs.face.command, 1);
	queue->free(&progs.normal.command, 1);
	LOG("MarchingCube::freeCommandBuffers() end\n");
}

void MarchingCube::destroyPipelines()
{
	builders.edge->destroy(&progs.edge.pipeline);
	builders.cell->destroy(&progs.cell.pipeline);
	builders.compact->destroy(&progs.compact.pipeline);
	builders.vertex->destroy(&progs.vertex.pipeline);
	builders.face->destroy(&progs.face.pipeline);
	builders.normal->destroy(&progs.normal.pipeline);
	PipelineCacheBuilder::destroy(ctx, &cache);
	PipelineLayoutBuilder::destroy(ctx, &progs.edge.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.cell.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.compact.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.vertex.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.face.p_layout);
	PipelineLayoutBuilder::destroy(ctx, &progs.normal.p_layout);
}

void MarchingCube::destroyBuilders()
{
	delete builders.cell;
	delete builders.compact;
	delete builders.edge;
	delete builders.face;
	delete builders.normal;
	delete builders.vertex;
	delete builders.descriptor;
}



#endif
