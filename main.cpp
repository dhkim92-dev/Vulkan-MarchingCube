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
	glm::vec3 normal;
	
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
		vector<VkVertexInputAttributeDescription> attributes(2);
		attributes[0].binding = 0;
		attributes[0].location = 0;
		attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributes[0].offset = offsetof(Vertex, pos);
		attributes[1].binding = 0;
		attributes[1].location = 1;
		attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributes[1].offset = offsetof(Vertex, normal);

		return attributes;
	}
};

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
		os << "vn " << normals[i*3] << " " << normals[i*3+1] << " " << normals[i*3+2] << endl;
	}
	for(uint32_t i = 0 ; i < nr_faces ; i++){
		os << "f " << faces[i*3] + 1 << " " << faces[i*3+1] + 1 << " " << faces[i*3+2] + 1<< endl;
	}

	os.close();
}

class MarchingCubeRenderer : public Application{
	private :
	MarchingCube mc;
	struct UniformMatrix{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	}h_uniforms;
	Buffer d_uniforms;

	struct Descriptors{
		VkDescriptorSet uniforms;
		VkDescriptorSet normals;
	}descriptor_sets;

	struct{
		VkSemaphore ready;
		VkSemaphore complete;
	}compute;

	bool first_draw = true;
	//VkFence draw_fence = VK_NULL_HANDLE;
	public :
	explicit MarchingCubeRenderer(string app_name, string engine_name, int h, int w, vector<const char *> instance_extensions, vector<const char*> device_extensions, vector<const char*> validations) : Application(app_name, engine_name, h, w, instance_extensions, device_extensions, validations){

	}
	~MarchingCubeRenderer(){
		mc.destroy();
	}
	
	protected:
	Program *draw_program;
	
	virtual void initWindow(){
		LOG("App Init Window\n");
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	}

	virtual void createSurface(){
		LOG("createSurface()\n");
		glfwCreateWindowSurface(engine->getInstance(), window, nullptr, &surface);
	}

	virtual void mainLoop(){
		float isovalue = 0.02f;
		float delta = +1.0f;
		while(!glfwWindowShouldClose(window)){
			glfwPollEvents();
			draw();
			updateUniforms();
			mc.setIsovalue(isovalue);
			isovalue += 0.001 * delta;
			if(abs(isovalue) > 0.05){
				delta = -delta;
			}
			// printf("current_frame_index : %d\n", current_frame_index);
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
		VkDevice device = context->getDevice();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_CI, nullptr, &compute.ready));
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_CI, nullptr, &compute.complete));
		// dim.x = 256;
		// dim.y = 256;
		// dim.z = 256;
		// dim.isovalue = 0.02f;
		dim.x =128;
		dim.y =128;
		dim.z =128;
		dim.isovalue = 0.03f;

		h_volume = new float[dim.x * dim.y * dim.z];
		//loadVolume("data/dragon_vrip_FLT32_128_128_64.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		//loadVolume("data/head_FLT32_256_256_225.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		// loadVolume("data/atom_FLT32_128_128_128.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		//loadVolume("data/skull_FLT32_256_256_256.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		//loadVolume("data/skull_FLT32_256_256_256.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		// loadVolume("data/Engine_FLT32_256_256_256.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		loadVolume("data/Engine_FLT32_128_128_128.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		// loadVolume("data/Genus3_FLT32_512_512_256.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		//loadVolume("data/horse_FLT32_128_256_256.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);

		mc.create(context, compute_queue);
		mc.setupDescriptorPool();
		mc.setVolumeSize(dim.x,dim.y ,dim.z);
		mc.setupBuffers();
		mc.setIsovalue(dim.isovalue);
		mc.setVolume((void *)h_volume);
		mc.setupKernels();
		mc.setupCommandBuffers();
	}

	void setupUniforms(){
		d_uniforms.create(context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(h_uniforms), nullptr);
		d_uniforms.copyFrom(&h_uniforms, sizeof(h_uniforms));
		draw_program->uniformUpdate(
		 		descriptor_sets.uniforms,
		 		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
		 		&d_uniforms.descriptor, nullptr);
	}

	void updateUniforms(){
		static auto startTime = std::chrono::high_resolution_clock::now();
		//static float z_fix = static_cast<float>(dim.z)/static_cast<float>(dim.x);
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		h_uniforms.model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
		h_uniforms.model = glm::rotate(h_uniforms.model, time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		h_uniforms.model = glm::translate(h_uniforms.model, glm::vec3(0.0f, 0.0f, 0.25f));
		h_uniforms.view = glm::lookAt(glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.1f, 0.0f));
		h_uniforms.proj =  glm::perspective(glm::radians(45.0f), width / (float)height, 0.1f, 10.0f);
		h_uniforms.proj[1][1] *= -1;
		d_uniforms.copyFrom(&h_uniforms, sizeof(h_uniforms));
	}

	void prepareGraphicsProgram(){
		draw_program = new Program(context);
		draw_program->attachShader("./shaders/mc.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		draw_program->attachShader("./shaders/mc.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		draw_program->setupDescriptorSetLayout({
			infos::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0, 1),
		});
		draw_program->createDescriptorPool({
		  	infos::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		});
		VK_CHECK_RESULT(draw_program->allocDescriptorSet(&descriptor_sets.uniforms, 0, 1));
		auto attributes = Vertex::vertexInputAttributes();
		auto bindings = Vertex::vertexInputBinding();
		draw_program->graphics.vertex_input = infos::vertexInputStateCreateInfo(attributes, bindings);
		draw_program->build(render_pass, cache);
	}

	void prepareGraphicsCommandBuffer(){
		std::array<VkClearValue, 2> clear_values{};
		clear_values[0].color = {0.3f, 0.3f, 0.3f, 1.0f};
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

		uint32_t sz_indices = static_cast<uint32_t>(mc.outputs.indices.getSize())/sizeof(uint32_t);
		//LOG("sz_vertices : %d sz indices : %d\n", sz_vertices, sz_indices);
		for(uint32_t i = 0 ; i < draw_command_buffers.size() ; ++i){
			graphics_queue->beginCommandBuffer(draw_command_buffers[i]);
			render_pass_BI.framebuffer = framebuffers[i];
			VkViewport viewport = infos::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
			VkRect2D scissor = infos::rect2D(width, height, 0, 0);
			// VkBufferMemoryBarrier barriers[2] = {
			// 	mc.outputs.vertices.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
			// 	mc.outputs.indices.barrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
			// };
			// graphics_queue->barrier(draw_command_buffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 1, nullptr, 0, barriers, 2, nullptr, 0 );
			vkCmdBeginRenderPass(draw_command_buffers[i], &render_pass_BI, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(draw_command_buffers[i], 0, 1, &viewport);
			vkCmdSetScissor(draw_command_buffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(draw_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, draw_program->pipeline);
			// VkBuffer vertex_buffer[] = {VkBuffer(mc.outputs.vertices)};
			// VkBuffer indices_buffer[] = {VkBuffer(mc.outputs.indices)};
			VkBuffer vertex_buffer[] = {mc.outputs.vertices.getBuffer()};
			VkBuffer indices_buffer[] = {mc.outputs.indices.getBuffer()};

			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(draw_command_buffers[i], 0, 1, vertex_buffer ,offsets);
			vkCmdBindIndexBuffer(draw_command_buffers[i], indices_buffer[0], 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(draw_command_buffers[i], 
			 						VK_PIPELINE_BIND_POINT_GRAPHICS,
			 						draw_program->pipeline_layout, 
			 						0, 
			 						1, &descriptor_sets.uniforms,
			 						0, nullptr);
			vkCmdDrawIndexed(draw_command_buffers[i], sz_indices, 1, 0, 0, 0);
			vkCmdEndRenderPass(draw_command_buffers[i]);
			/*
			VkBufferMemoryBarrier end_barriers[2] = {
				mc.outputs.vertices.barrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
				mc.outputs.indices.barrier(VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			};

			graphics_queue->barrier(draw_command_buffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 1, nullptr, 0, end_barriers, 2, nullptr, 0 );
			*/
			VK_CHECK_RESULT(graphics_queue->endCommandBuffer(draw_command_buffers[i]));
			printf("graphics : %p\n", draw_command_buffers[i]);
		}
	}
	void destroy(){
		mc.destroy();
		context->destroySemaphore(&compute.ready);
		context->destroySemaphore(&compute.complete);
		Application::destroy();
	}

	void runCompute(VkSemaphore *wait_semaphores, uint32_t nr_wait_semaphores, VkSemaphore *signal_semaphores, uint32_t nr_signal_semaphores){
		mc.run(wait_semaphores, nr_wait_semaphores, signal_semaphores, nr_signal_semaphores);
	}

	
	virtual void render(){
		// printf("swapchain_buffers size : %d, inFlight : %d\n", swapchain.buffers.size(),current_frame_index);
		if(!first_draw){
			runCompute(&compute.ready, 1, &compute.complete, 1);
			// printf("compute end\n");
		}else{
			first_draw = false;
			runCompute(nullptr, 0, &compute.complete, 1);
		}

		prepareFrame();
		VkSemaphore waits[2] = {
			compute.complete,
			semaphores.present_complete,
		};
		VkSemaphore signals[2] = {
			compute.ready,
			semaphores.render_complete,
		};

		
		VkPipelineStageFlags waitDstStageMask[2] = {
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};

		VK_CHECK_RESULT(graphics_queue->submit(&draw_command_buffers[current_frame_index], 1, 
			waitDstStageMask, //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
			waits, 2, 
			signals, 2, VK_NULL_HANDLE));
		submitFrame();
	}
	

	public :
	void run(){
		LOG("Application Init\n");
		//Application::init();
		init();
		LOG("Application Init Complete\n");
		prepareCompute();
		LOG("MarchingCubeRenderer::prepareCompute done\n");
		prepareGraphicsProgram();
		setupUniforms();
		prepareGraphicsCommandBuffer();
		mainLoop();
		destroy();
	}
};

void benchmark(){
	//vector<const char *> instance_extensions(getRequiredExtensions());
	vector<const char*> instance_extensions = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, "VK_EXT_debug_report", "VK_EXT_debug_utils"};
	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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

int main(int argc, char* argv[]){
	vector<const char *> instance_extensions(getRequiredExtensions());
	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	string _name = "vulkan";
	string engine_name = "engine";
	
		
	try {
		MarchingCubeRenderer app(_name, engine_name,600, 800, instance_extensions, device_extensions, validations);
		app.run();
	} catch(std::exception& e) {
		cout << "error occured " << e.what() << " on File " << __FILE__ << " line : " << __LINE__ << endl;
		exit(EXIT_FAILURE);
	}
	
	    return 0;
}
