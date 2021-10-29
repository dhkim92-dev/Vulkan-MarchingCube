#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include "scan.h"
#include "vk_core.h"
#include "marchingcube.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define GLFW_INCLUDE_VULKAN 1

using namespace std;
using namespace VKEngine;

struct MetaInfo2{
	uint32_t x,y,z;
	float isovalue;
}dim;

float *h_volume;


/*
vector<const char *> getRequiredExtensions(  ){
	glfwInit();
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if(validationEnable) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	glfwTerminate();
	return extensions;
}

struct Vertex {
	glm::vec3 pos;
	
	static vector<VkVertexInputBindingDescription> vertexInputBinding(){
		vector <VkVertexInputBindingDescription> bindings;
		VkVertexInputBindingDescription binding = {};
		binding.binding = 0;
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		binding.stride = sizeof(Vertex);
		bindings.push_back(binding);
		return bindings;
	}

	static vector<VkVertexInputAttributeDescription> vertexInputAttributes(){
		vector<VkVertexInputAttributeDescription> attributes(1);
		attributes[0].binding = 0;
		attributes[0].location = 0;
		attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributes[0].offset = offsetof(Vertex, pos);
		return attributes;
	}
};

*/
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


void saveAsObj(const char *path, float *vertices, uint32_t* faces, float *normals, uint32_t nr_vertices, uint32_t nr_faces){
	ofstream os(path);

	for(uint32_t i = 0 ; i < nr_vertices ; i++){
		os << "v " << vertices[i*3] << " " << vertices[i*3+1] << " " << vertices[i*3+2] << endl;
	}

	for(uint32_t i = 0 ; i < nr_vertices ; i++){
		os << "fn " << normals[i*3] << " " << normals[i*3+1] << " " << normals[i*3+2] << endl;
	}

	for(uint32_t i = 0 ; i < nr_faces ; i++){
		os << "f " << faces[i*3] + 1 << " " << faces[i*3+1] + 1 << " " << faces[i*3+2] + 1<< endl;
	}

	os.close();
}

/*
class MarchingCubeRenderer : public Application{
	public :
	MarchingCube mc;
	explicit MarchingCubeRenderer(string app_name, string engine_name, int h, int w, vector<const char *> instance_extensions, vector<const char*> device_extensions, vector<const char*> validations) : Application(app_name, engine_name, h, w, instance_extensions, device_extensions, validations){

	}
	~MarchingCubeRenderer(){
		mc.destroy();
	}
	
	protected:
	Program *draw_program;
	VkSemaphore compute_complete;
	
	virtual void initWindow(){
		LOG("App Init Window\n");
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	}

	virtual void createSurface(){
		LOG("createSurface()\n");
		glfwCreateWindowSurface(VkInstance(*engine), window, nullptr, &surface);
	}

	virtual void mainLoop(){
		while(!glfwWindowShouldClose(window)){
			glfwPollEvents();
			draw();
			//updateUniform();
		}
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void draw(){
		//compute();
		render();
	}

	void prepareCompute(){
		VkSemaphoreCreateInfo semaphore_CI = infos::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(VkDevice(*context), &semaphore_CI, nullptr, &compute_complete));
		mc.create(context, compute_queue);
		mc.setupDescriptorPool();
		mc.setVolumeSize(dim.x,dim.y ,dim.z);
		mc.setupBuffers();
		mc.setIsovalue(dim.isovalue);
		mc.setVolume((void *)h_volume);
		mc.setupKernels();
		mc.setupCommandBuffers();
	}

	void prepareGraphicsProgram(){
		draw_program = new Program(context);
		draw_program->attachShader("./shaders/mc.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		draw_program->attachShader("./shaders/mc.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// draw_program->setupDescriptorSetLayout({
		// 	infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0, 1)
		// });
		// draw_program->createDescriptorPool({
		//  	infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3)
		// });
		// draw_program->allocDescriptorSet(&uniform_matrix.desc_set, 0, 1);
		auto attributes = Vertex::vertexInputAttributes();
		auto bindings = Vertex::vertexInputBinding();
		draw_program->graphics.vertex_input = infos::vertexInputStateCreateInfo(attributes, bindings);
		draw_program->build(render_pass, cache);
		// draw_program->uniformUpdate(
		// 		uniform_matrix.desc_set,
		// 		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
		// 		&mc.output.ubo.descriptor, nullptr
		// );
	}

	void prepareGraphicsCommandBuffer(){
		std::array<VkClearValue, 2> clear_values;
		clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
		clear_values[1].depthStencil = {1.0f, 0};
		draw_command_buffers.resize(swapchain.buffers.size());
		VkRenderPassBeginInfo render_pass_BI = infos::renderPassBeginInfo();
		render_pass_BI.clearValueCount = static_cast<uint32_t>(clear_values.size());
		render_pass_BI.pClearValues = clear_values.data();
		render_pass_BI.renderArea.offset = {0,0};
		render_pass_BI.renderArea.extent.height = height;
		render_pass_BI.renderArea.extent.width = width;
		render_pass_BI.renderPass = render_pass;

		for(uint32_t i = 0 ; i < draw_command_buffers.size() ; ++i){
			draw_command_buffers[i] = graphics_queue->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		}

		uint sz_indices = dim.x * dim.y * dim.z;
		LOG("sz indices : %d\n", sz_indices);
		for(uint32_t i = 0 ; i < draw_command_buffers.size() ; ++i){
			graphics_queue->beginCommandBuffer(draw_command_buffers[i]);
			render_pass_BI.framebuffer = framebuffers[i];
			VkViewport viewport = infos::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
			VkRect2D scissor = infos::rect2D(width, height, 0, 0);
			vkCmdBeginRenderPass(draw_command_buffers[i], &render_pass_BI, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(draw_command_buffers[i], 0, 1, &viewport);
			vkCmdSetScissor(draw_command_buffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(draw_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, draw_program->pipeline);
			VkBuffer vertex_buffer[] = {VkBuffer(mc.outputs.vertices)};
			VkBuffer indices_buffer[] = {VkBuffer(mc.outputs.indices)};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(draw_command_buffers[i], 0, 1, vertex_buffer ,offsets);
			vkCmdBindIndexBuffer(draw_command_buffers[i], indices_buffer[0], 0, VK_INDEX_TYPE_UINT32);
			// vkCmdBindDescriptorSets(draw_command_buffers[i], 
			// 						VK_PIPELINE_BIND_POINT_GRAPHICS,
			// 						draw_program->pipeline_layout, 
			// 						0, 
			// 						1, &uniform_matrix.desc_set,
			// 						0, nullptr);
			vkCmdDrawIndexed(draw_command_buffers[i], sz_indices, 1, 0, 0, 0);
			vkCmdEndRenderPass(draw_command_buffers[i]);
			graphics_queue->endCommandBuffer(draw_command_buffers[i]);
		}
	}
	void destroy(){
		mc.destroy();
		if(compute_complete)
			vkDestroySemaphore(VkDevice(*context), compute_complete, nullptr);
		compute_complete = VK_NULL_HANDLE;
		Application::destroy();
	}

	void compute(){
		LOG("compute run\n");
		mc.run(&semaphores.render_complete, 1, &compute_complete, 1);
	}

	virtual void render(){
		prepareFrame();
		VkSemaphore graphics_wait_semaphores[1] = {
			compute_complete
		};
		graphics_queue->resetFences(&draw_fence, 1);
		graphics_queue->submit(&draw_command_buffers[current_frame_index], 1, 
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, graphics_wait_semaphores, 1,
					&semaphores.render_complete, 1, draw_fence);
		graphics_queue->waitFences(&draw_fence, 1);
		submitFrame();
		//compute();
	}

	public :
	void run(){
		LOG("Application Init\n");
		Application::init();
		LOG("Application Init Complete\n");
		prepareCompute();
		LOG("MarchingCubeRenderer::prepareCompute done\n");
		prepareGraphicsProgram();
		prepareGraphicsCommandBuffer();
		mainLoop();
		destroy();
	}

};
*/

int main(int argc, char* argv[]){
	//vector<const char *> instance_extensions(getRequiredExtensions());//
	vector<const char*> instance_extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, "VK_EXT_debug_report", "VK_EXT_debug_utils"};
	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	string _name = "vulkan";
	string engine_name = "engine";
	
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

	dim.x = 128;
	dim.y = 128;
	dim.z = 64;
	dim.isovalue = 0.02f;
	h_volume = new float[dim.x * dim.y * dim.z];
	loadVolume("data/dragon_vrip_FLT32_128_128_64.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
	/*
	try {
		MarchingCubeRenderer app(_name, engine_name,600, 800, instance_extensions, device_extensions, validations);
		app.run();
	} catch(std::exception& e) {
		cout << "error occured " << e.what() << " on File " << __FILE__ << " line : " << __LINE__ << endl;
		exit(EXIT_FAILURE);
	}
	*/
	uint32_t *edge_psum = new uint32_t[dim.x * dim.y * dim.z*3];
	uint32_t *tri_psum = new uint32_t[dim.x * dim.y * dim.z];
	uint32_t h_edgescan, h_cellscan;
	mc.setVolumeSize(dim.x, dim.y, dim.z);
	mc.setupDescriptorPool();
	mc.setupBuffers();
	mc.setVolume((void *)h_volume);
	mc.setIsovalue(dim.isovalue);
	LOG("setup Kernels\n");
	mc.setupKernels();
	LOG("setup Kernel done\n");
	mc.setupCommandBuffers();
	LOG("marching cube run!\n");
	PROFILING(mc.run(nullptr, 0, nullptr, 0), "marching_cube");
	queue.waitIdle();
	
	queue.enqueueCopy(&mc.edge_test.d_output, edge_psum ,0, 0, dim.x*dim.y*dim.z*3*sizeof(uint32_t));
	queue.waitIdle();
	uint32_t s=0;
	for(uint32_t i = 0 ; i < dim.x * dim.y * dim.z * 3 ; i++){
		s+=edge_psum[i];
	}
	printf("cpu edge_scan: %d\n", s);
	
	queue.enqueueCopy(&mc.scan.d_epsum, edge_psum ,0, 0, dim.x*dim.y*dim.z*3*sizeof(uint32_t));
	queue.waitIdle();
	printf("gpu edge_scan : %d\n", edge_psum[128*128*64*3-1]);
	// printf("gpu edge_scan : %d\n", edge_psum[128*128*64*3]);
	exit(1);

	//queue.enqueueCopy(&mc.scan.d_epsum, &h_edgescan, dim.x * dim.y*dim.z*3*sizeof(uint32_t) - sizeof(uint32_t), 0, sizeof(uint32_t),false);
	//queue.enqueueCopy(&mc.scan.d_cpsum, &h_cellscan, dim.x*dim.y*dim.z*sizeof(uint32_t) - sizeof(uint32_t), 0, sizeof(uint32_t), false);
	//mc.general.d_metainfo.copyTo(&dim, sizeof(MetaInfo2));
	queue.waitIdle();
	float *vertices = new float[ 3 * h_edgescan ];
	uint32_t *faces = new uint32_t[h_cellscan*3];
	float *normals = new float[3 * h_edgescan];
	LOG("nr_vertices : %d\n", h_edgescan);
	LOG("nr_faces : %d\n", h_cellscan);
	//queue.enqueueCopy(&mc.outputs.vertices, vertices, 0, 0, h_edgescan*sizeof(float)*3);
	//queue.enqueueCopy(&mc.outputs.indices, faces, 0, 0, h_cellscan*sizeof(uint32_t)*3);
	//queue.enqueueCopy(&mc.outputs.normals, normals, 0, 0, sizeof(float) * h_edgescan*3);
	queue.waitIdle();
	LOG("Meta info dim (%d, %d, %d), isovalue : %f\n", dim.x , dim.y ,dim.z ,dim.isovalue);
	LOG("GPU Edge Scan : %d\n", h_edgescan);
	LOG("GPU Cell Scan : %d\n", h_cellscan);
	LOG("Save As Object\n");
	saveAsObj("test.obj", vertices , faces, normals, h_edgescan,h_cellscan);
	delete [] vertices;
	delete [] faces;
	delete[] h_volume;
    mc.destroy();
	queue.destroy();
	context.destroy();
	engine.destroy();
    return 0;
}
