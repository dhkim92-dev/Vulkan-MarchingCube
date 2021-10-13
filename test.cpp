#include "scan.h"
#include "vk_core.h"
#include <GLFW/glfw3.h>
#include <chrono>

using namespace std;
using namespace VKEngine;




#define PROFILING(FPTR, FNAME) ({ \
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now(); \
		FPTR; \
		std::chrono::duration<double> t = std::chrono::system_clock::now() - start; \
		printf("%s operation time : %.4lf seconds\n",FNAME, t.count()); \
})


uint32_t * createTestData(uint32_t nr_elem){
	uint32_t *data = new uint32_t[nr_elem];

	for(uint32_t i = 0 ; i < nr_elem; ++i)
		data[i] = 1;

	return data;
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
	Scan scan;

	uint32_t sz_test = 128*128*64*3;
	uint32_t *h_inputs = createTestData(sz_test);
	uint32_t *h_outputs =	new uint32_t[sz_test];
	Buffer *d_input = new Buffer(&context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sz_test*sizeof(uint32_t), nullptr);
	Buffer *d_output = new  Buffer(&context, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sz_test*sizeof(uint32_t), nullptr);
	scan.create(&context, &queue);
	scan.init(sz_test);
	scan.build();
	queue.enqueueCopy(h_inputs, d_input, 0, 0, sizeof(uint32_t)*sz_test);
	queue.waitIdle();
	PROFILING(scan.run(d_input, d_output), "scan()");
	queue.enqueueCopy(d_output, h_outputs, (sz_test-1)*4, 0, 4);
	queue.waitIdle();
	printf("prefix sum result : %d ", h_outputs[0]);
	printf("\n");

	delete d_input;
	delete d_output;

	scan.destroy();
	LOG("scan destroyed\n");
	LOG("queue destroyed\n");
	queue.destroy();
	LOG("context destroyed\n");
	context.destroy();
	LOG("engine destroyed\n");
	engine.destroy();
}
