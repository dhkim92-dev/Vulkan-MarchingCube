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

void MarchingCube::setVolume(float *data, int x, int y, int z)
{
	h_volume.data = data;
	h_volume.x = x;
	h_volume.y = y;
	h_volume.z = z;
}

void MarchingCube::setupBuffers(){
	general.volume.create(ctx);
	VK_CHECK_RESULT(general.volume.createImage(h_volume.x, h_volume.y, h_volume.z, VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
								VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, 1, 1));
	VK_CHECK_RESULT(general.volume.alloc(sizeof(float) * h_volume.x * h_volume.y * h_volume.z, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	VK_CHECK_RESULT(general.volume.createImageView(VK_IMAGE_VIEW_TYPE_3D, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}));
	VkSamplerCreateInfo sampler_CI = infos::samplerCreateInfo();
	sampler_CI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_CI.anisotropyEnable = VK_TRUE;
	sampler_CI.maxAnisotropy = 1.0f;
	sampler_CI.minLod = 0.0f;
	sampler_CI.mipLodBias = 0.0f;
	sampler_CI.compareOp = VK_COMPARE_OP_NEVER;
	sampler_CI.magFilter = VK_FILTER_LINEAR;
	sampler_CI.unnormalizedCoordinates = VK_TRUE;
}




}

#endif