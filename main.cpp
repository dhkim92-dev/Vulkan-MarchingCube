#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include "scan.h"
#include "vk_core.h"
#include "marchingcube.h"

using namespace std;
using namespace VKEngine;

struct MetaInfo2{
	uint32_t x,y,z;
	float isovalue;
}dim;

#define PROFILING(FPTR, FNAME) ({ \
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now(); \
		FPTR; \
		std::chrono::duration<double> t = std::chrono::system_clock::now() - start; \
		printf("%s operation time : %.4lf seconds\n",FNAME, t.count()); \
})

void loadVolume(const char *file_path, uint32_t mem_size, void *data)
{
	ifstream is(file_path, ios::in | ios::binary | ios::ate);
	if(is.is_open()){
		size_t size = is.tellg();
		if(mem_size!=size){
			std::runtime_error("fail to read binary. size is not matched with actual.\n");
		}
		assert(size > 0);
		is.seekg(0, ios::beg);
		is.read((char *)data, size);
	}else{
		std::runtime_error("fail to open file. maybe not exist in path.\n");
	}
}

uint32_t * createTestData(uint32_t nr_elem){
	uint32_t *data = new uint32_t[nr_elem];
	for(uint32_t i = 0 ; i < nr_elem; ++i)
		data[i] = 1;
	return data;
}

void saveAsObj(const char *path, float *vertices, uint32_t* faces, uint32_t nr_vertices, uint32_t nr_faces){
	ofstream os(path);

	for(uint32_t i = 0 ; i < nr_vertices ; i++){
		os << "v " << vertices[i*3] << " " << vertices[i*3+1] << " " << vertices[i*3+2] << endl;
	}

	for(uint32_t i = 0 ; i < nr_faces ; i++){
		os << "f " << faces[i*3] + 1 << " " << faces[i*3+1] + 1 << " " << faces[i*3+2] + 1<< endl;
	}

	os.close();
}

int main(int argc, char* argv[]){
	vector<const char *> instance_extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, "VK_EXT_debug_report", "VK_EXT_debug_utils"};
	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	Engine engine(
		"Marching Cube",
		"Vulkan Renderer",
		instance_extensions,
		device_extensions,
		validations
	);
	engine.init();
	Context context;
	context.create(&engine, 0, VK_QUEUE_COMPUTE_BIT, VK_NULL_HANDLE);
	CommandQueue queue(&context, VK_QUEUE_COMPUTE_BIT);
	MarchingCube mc;
	mc.create(&context, &queue);
	float *data = new float[128*128*64];
	uint32_t *h_edgetype = new uint32_t[128*128*64*3];
	uint32_t *h_tricount = new uint32_t[128*128*64];
	uint32_t h_edgescan, h_cellscan;
	loadVolume("data/dragon_vrip_FLT32_128_128_64.raw", 128*128*64*sizeof(float), (void *)data);
	// for(int i = 0 ; i < 128*128*64 ; i++){
		// data[i] = 0.0;
	// }
	// data[1 + 128 + 128*128] = 0.5;
	mc.setVolumeSize(128,128,64);
	mc.setupDescriptorPool();
	mc.setupBuffers();
	mc.setVolume((void *)data);
	mc.setIsovalue(0.02f);
	LOG("setup Kernels\n");
	mc.setupKernels();
	LOG("setup Kernel done\n");
	mc.setupCommandBuffers();
	LOG("marching cube run!\n");
	PROFILING(mc.run(), "marching_cube");
	LOG("enqueueCopy start!\n");
	queue.enqueueCopy(&mc.edge_test.d_output, h_edgetype, 0, 0, 128*128*64*3*sizeof(uint32_t), false);
	queue.enqueueCopy(&mc.cell_test.d_tricount, h_tricount, 0, 0, 128*128*64*sizeof(uint32_t), false);
	queue.enqueueCopy(&mc.scan.d_epsum, &h_edgescan, sizeof(uint32_t) * 128*128*64*3 - 4, 0, 4, false);
	queue.enqueueCopy(&mc.scan.d_cpsum, &h_cellscan, sizeof(uint32_t) * 128*128*64 - 4, 0, 4, false);
	mc.general.d_metainfo.copyTo(&dim, sizeof(MetaInfo2));
	queue.waitIdle();
	float *vertices = new float[ 3 * h_edgescan ];
	uint32_t *faces = new uint32_t[h_cellscan*3];
	queue.enqueueCopy(&mc.outputs.vertices, vertices, 0, 0, h_edgescan*sizeof(float)*3);
	queue.enqueueCopy(&mc.outputs.indices, faces, 0, 0, h_cellscan*sizeof(uint32_t)*3);
	queue.waitIdle();
	uint32_t nr_triangle = 0, nr_edge = 0;
	for(uint32_t i = 0 ; i < 128*128*64*3 ; i++ ){
		nr_edge += h_edgetype[i];
	}
	for(uint32_t i = 0 ; i < 128*128*64 ; i++ ){
		nr_triangle += h_tricount[i];
	}
	LOG("Meta info dim (%d, %d, %d), isovalue : %f\n", dim.x , dim.y ,dim.z ,dim.isovalue);
	LOG("Host Triangle Count : %d\n", nr_triangle);
	LOG("Host Edge Count : %d\n", nr_edge);
	LOG("GPU Edge Scan : %d\n", h_edgescan);
	LOG("GPU Cell Scan : %d\n", h_cellscan);
	LOG("Save As Object\n");
	saveAsObj("test.obj", vertices , faces, h_edgescan,h_cellscan);

	delete [] vertices;
	delete [] data;
	delete [] h_edgetype;
	delete [] h_tricount;
	LOG("queue destroyed\n");
	queue.destroy();
	LOG("context destroyed\n");
	context.destroy();
	LOG("engine destroyed\n");
	engine.destroy();
    LOG("All resource is released\n");
    return 0;
}
