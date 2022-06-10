#include <iostream>
#include <vulkan/vulkan.h>
#include "vk_core.h"
#include "scan.h"
#include "marchingcube.h"

void benchmark();
void saveAsObj(const char *path, float *vertices, uint32_t* faces, float *normals, uint32_t nr_vertices, uint32_t nr_faces);

int main(void)
{
	benchmark();
	return 0;
}

void benchmark(){
	//vector<const char *> instance_extensions(getRequiredExtensions());
	vector<const char*> instance_extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, "VK_EXT_debug_report", "VK_EXT_debug_utils"};
	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	Engine engine(
		"Vulkan-MarchingCube",
		instance_extensions,
		device_extensions,
		validations
	);
	engine.init();
    PhysicalDevice device(&engine);
    device.useGPU(0);
    device.init();

	Context context(&device, VK_QUEUE_COMPUTE_BIT);
	CommandQueue queue(&context, VK_QUEUE_COMPUTE_BIT);
	MarchingCube mc;
	mc.create(&context, &queue);
	dim.x = 128;
	dim.y = 128;
	dim.z = 64;
	dim.isovalue = 0.02f;
	
	h_volume = new float[dim.x * dim.y * dim.z];
	loadVolume("data/dragon_vrip_FLT32_128_128_64.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
	uint32_t h_edgescan, h_cellscan;
	mc.setVolumeSize(dim.x, dim.y, dim.z);
	mc.setupDescriptorPool();
	mc.setupBuffers();
	mc.setVolume((void *)h_volume);
	mc.setIsovalue(dim.isovalue);
	mc.setupKernels();
	mc.setupCommandBuffers();
	PROFILING(mc.run(nullptr, 0, nullptr, 0), "marching_cube");
	queue.waitIdle();
	
	queue.enqueueCopy(&mc.scan.d_epsum, &h_edgescan, dim.x * dim.y*dim.z*3*sizeof(uint32_t) - sizeof(uint32_t), 0, sizeof(uint32_t),false);
	queue.enqueueCopy(&mc.scan.d_cpsum, &h_cellscan, dim.x*dim.y*dim.z*sizeof(uint32_t) - sizeof(uint32_t), 0, sizeof(uint32_t), false);
	queue.waitIdle();
	float *vertices = new float[ 3 * h_edgescan ];
	uint32_t *faces = new uint32_t[h_cellscan*3];
	float *normals = new float[3 * h_edgescan];
	queue.enqueueCopy(&mc.outputs.vertices, vertices, 0, 0, h_edgescan*sizeof(float)*3);
	queue.enqueueCopy(&mc.outputs.indices, faces, 0, 0, h_cellscan*sizeof(uint32_t)*3);
	queue.enqueueCopy(&mc.outputs.normals, normals, 0, 0, sizeof(float) * h_edgescan*3);
	queue.waitIdle();
	LOG("Meta info dim (%d, %d, %d), isovalue : %f\n", dim.x , dim.y ,dim.z ,dim.isovalue);
	LOG("GPU Edge Scan : %d\n", h_edgescan);
	LOG("GPU Cell Scan : %d\n", h_cellscan);
	// LOG("Save As Object\n");
	saveAsObj("test.obj", vertices , faces, normals, h_edgescan,h_cellscan);
	delete [] vertices;
	delete [] faces;
	delete [] normals;
	delete[] h_volume;
}


void saveAsObj(const char *path, float *vertices, uint32_t* faces, float *normals, uint32_t nr_vertices, uint32_t nr_faces){
	ofstream os(path);

	for(uint32_t i = 0 ; i < nr_vertices ; i++){
		os << "v " << vertices[i*3] << " " << vertices[i*3+1] << " " << vertices[i*3+2] << endl;
	}
	for(uint32_t i = 0 ; i < nr_vertices ; i++){
		os << "vn " << normals[i*3] << " " << normals[i*3+1] << " " << normals[i*3+2] << endl;
	}
	for(uint32_t i = 0 ; i < nr_faces ; i++){
		os << "f " << faces[i*3] + 1 << " " << faces[i*3+1] + 1 << " " << faces[i*3+2] + 1<< endl;
	}

	os.close();
}
