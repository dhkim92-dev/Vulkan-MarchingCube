#define GLFW_INCLUDE_VULKAN 1
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include "scan.h"
#include "vk_core.h"
#include "vk_application.h"
#include "marchingcube.h"


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

void loadVolume(const char *file_path, uint32_t mem_size, void *data)
{
	ifstream is(file_path, ios::in | ios::binary | ios::ate);
	if(is.is_open()){
		size_t size = is.tellg();
		if(mem_size!=size){
			std::runtime_error("fail to read binary. size is not matched with actual.\n");
		}
		assert(size > 0);
		LOG("Volume read, size : %d\n", size);
		is.seekg(0, ios::beg);
		is.read((char *)data, size);
	}else{
		std::runtime_error("fail to open file. maybe not exist in path.\n");
	}
}

class MarchingCubeRenderer : public Application{
	private :
	MarchingCube mc;
	struct UniformMatrix{
		glm::mat4 model = glm::mat4(1.0f);
		glm::mat4 view = glm::lookAt(glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.1f, 0.0f));
		glm::mat4 proj;
	}h_uniforms;
	Buffer d_uniforms;

	struct{
		VkSemaphore ready;
		VkSemaphore complete;
	}compute;

	struct Builders{
		GraphicsPipelineBuilder *pipeline;
		DescriptorSetBuilder *descriptor;
	}builders;
	Program prog;

	bool first_draw = true;
	VkFence draw_fence = VK_NULL_HANDLE;
	
	public :
	explicit MarchingCubeRenderer(string _name, int h, int w, 
		vector<const char *> instance_extensions, 
		vector<const char*> device_extensions, 
		vector<const char*> validations) : Application(_name, h, w, instance_extensions, device_extensions, validations
	){
		this->engine->setDebug(true);
	}
	
	protected:
	
	virtual void initWindow(){
		LOG("App Init Window\n");
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
		if(!window){
			throw std::runtime_error("glfwWindowCreate failed error.\n");
		}
	}

	virtual void createSurface(){
		VK_CHECK_RESULT(glfwCreateWindowSurface(engine->getInstance(), window, nullptr, &surface));
	}

	void runCompute(VkSemaphore *wait_semaphores, uint32_t nr_wait_semaphores, VkSemaphore *signal_semaphores, uint32_t nr_signal_semaphores){
		mc.run(wait_semaphores, nr_wait_semaphores, signal_semaphores, nr_signal_semaphores);
	}

	void render() override{
		if(!first_draw){
			runCompute(&compute.ready, 1, &compute.complete, 1);
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
	

	void mainLoop() override {
		float isovalue = 0.02f;
		float delta = 1.0f;
		while(!glfwWindowShouldClose(window)){
			glfwPollEvents();
			render();
			updateUniforms();
		}
		glfwDestroyWindow(window);
		glfwTerminate();
	}


	void prepareCompute(){
		VkDevice device = context->getDevice();
		VK_CHECK_RESULT(context->createSemaphore(&compute.ready));
		VK_CHECK_RESULT(context->createSemaphore(&compute.complete));
		dim={128, 128, 64, 0.02f};
		h_volume = new float[dim.x * dim.y * dim.z];
		loadVolume("data/dragon_vrip_FLT32_128_128_64.raw", dim.x*dim.y*dim.z*sizeof(float), (void *)h_volume);
		mc.create(context, compute_queue);
		mc.setVolumeSize(dim.x,dim.y ,dim.z);
		mc.setupBuffers();
		mc.setupBuilders();
		mc.setupDescriptors();
		mc.setIsovalue(dim.isovalue);
		mc.setVolume((void *)h_volume);
		mc.writeDescriptors();
		mc.setupPipelineLayouts();
		mc.setupPipelines();
		mc.setupEvents();
		mc.setupCommandBuffers();
	}

	void prepareBuffers(){
		d_uniforms.create(context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(h_uniforms), nullptr);
		d_uniforms.copyFrom(&h_uniforms, sizeof(h_uniforms));
		builders.descriptor = new DescriptorSetBuilder(context);
		builders.pipeline = new GraphicsPipelineBuilder(context);
	}

	void prepareDescriptors(){
		VkDescriptorPoolSize size=DescriptorSetBuilder::createDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		builders.descriptor->setDescriptorPool(&size, 1, 1);
		VkDescriptorSetLayoutBinding _binding=DescriptorSetLayoutBuilder::bind(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
		VK_CHECK_RESULT(DescriptorSetLayoutBuilder::build(context, &prog.d_layout, &_binding, 1));
		builders.descriptor->build(&prog.set, &prog.d_layout, 1, nullptr);
		VkWriteDescriptorSet write_set = DescriptorWriter::writeBuffer(prog.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &d_uniforms);
		DescriptorWriter::update(context, &write_set, 1);
	}

	void preparePipelineLayouts(){
		VK_CHECK_RESULT(PipelineLayoutBuilder::build(context, &prog.p_layout, &prog.d_layout, 1));
	}

	void preparePipelines(){
		builders.pipeline->createVertexShader("shaders/mc.vert.spv");
		builders.pipeline->createFragmentShader("shaders/mc.frag.spv");
		
		VkPipelineColorBlendAttachmentState color_blend_state[1]={infos::colorBlendAttachmentState(0xf, VK_FALSE)};
		VkPipelineColorBlendStateCreateInfo color_blend_CI = infos::colorBlendStateCreateInfo(1, color_blend_state);
		VkPipelineDepthStencilStateCreateInfo depth_stencil_CI = infos::depthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkDynamicState enabled_dynamics[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state = infos::dynamicStateCreateInfo(enabled_dynamics, 2);
		VkPipelineRasterizationStateCreateInfo raster_CI = infos::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineViewportStateCreateInfo viewport_CI = infos::viewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisample_CI = infos::multisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT,0);
		auto attributes = Vertex::vertexInputAttributes();
		auto bindings = Vertex::vertexInputBinding();
		VkPipelineVertexInputStateCreateInfo input_CI = infos::vertexInputStateCreateInfo(attributes, bindings);

		builders.pipeline->setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
		builders.pipeline->setColorBlendState(color_blend_CI);
		builders.pipeline->setDepthStencilState(depth_stencil_CI);
		builders.pipeline->setDynamicState(dynamic_state);
		builders.pipeline->setRasterizationState(raster_CI);
		builders.pipeline->setViewportState(viewport_CI);
		builders.pipeline->setViewportState(infos::viewportStateCreateInfo(1, 1, 0));
		builders.pipeline->setMultiSampleState(multisample_CI);
		builders.pipeline->setVertexInputState(input_CI);
		builders.pipeline->build(&prog.pipeline, render_pass, prog.p_layout, cache);
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

	void prepareCommandBuffer(){
		std::array<VkClearValue, 2> clear_values{};
		clear_values[0].color = {0.3f, 0.3f, 0.3f, 1.0f};
		clear_values[1].depthStencil = {1.0f, 0};
		draw_command_buffers.resize(swapchain->getImageCount());
		VkRenderPassBeginInfo render_pass_BI = infos::renderPassBeginInfo();
		render_pass_BI.clearValueCount = static_cast<uint32_t>(clear_values.size());
		render_pass_BI.pClearValues = clear_values.data();
		render_pass_BI.renderArea.offset = {0,0};
		render_pass_BI.renderArea.extent.height = height;
		render_pass_BI.renderArea.extent.width = width;
		render_pass_BI.renderPass = render_pass;

		VK_CHECK_RESULT(graphics_queue->createCommandBuffer(draw_command_buffers.data(), draw_command_buffers.size(), VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		uint32_t sz_indices = static_cast<uint32_t>(mc.buffers.indices.getSize())/sizeof(uint32_t);
		for(uint32_t i = 0 ; i < draw_command_buffers.size() ; ++i){
			graphics_queue->beginCommandBuffer(draw_command_buffers[i]);
			render_pass_BI.framebuffer = framebuffers[i];
			VkViewport viewport = infos::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
			VkRect2D scissor = infos::rect2D(width, height, 0, 0);
			vkCmdBeginRenderPass(draw_command_buffers[i], &render_pass_BI, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(draw_command_buffers[i], 0, 1, &viewport);
			vkCmdSetScissor(draw_command_buffers[i], 0, 1, &scissor);
			vkCmdBindPipeline(draw_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, prog.pipeline);
			VkBuffer vertex_buffer[] = {mc.buffers.vertices.getBuffer()};
			VkBuffer indices_buffer[] = {mc.buffers.indices.getBuffer()};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(draw_command_buffers[i], 0, 1, vertex_buffer ,offsets);
			vkCmdBindIndexBuffer(draw_command_buffers[i], indices_buffer[0], 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(draw_command_buffers[i], 
			 						VK_PIPELINE_BIND_POINT_GRAPHICS,
			 						prog.p_layout, 
			 						0, 
			 						1, &prog.set,
			 						0, nullptr);
			vkCmdDrawIndexed(draw_command_buffers[i], sz_indices, 1, 0, 0, 0);
			vkCmdEndRenderPass(draw_command_buffers[i]);
			VK_CHECK_RESULT(graphics_queue->endCommandBuffer(draw_command_buffers[i]));
		}
	}

	public:
	~MarchingCubeRenderer(){
		//mc.destroy();
		d_uniforms.destroy();
		PipelineCacheBuilder::destroy(context, &cache);
		PipelineLayoutBuilder::destroy(context, &prog.p_layout);
		builders.pipeline->destroy(&prog.pipeline);
		builders.descriptor->free(&prog.set, 1);
		DescriptorSetLayoutBuilder::destroy(context, &prog.d_layout);
		delete builders.pipeline;
		delete builders.descriptor;
		graphics_queue->free(draw_command_buffers.data(), draw_command_buffers.size());
		context->destroySemaphore(&compute.ready);
		context->destroySemaphore(&compute.complete);
	}

	public :
	void run(){
		//Application::init();
		init();
		prepareCompute();
		prepareBuffers();
		prepareDescriptors();
		preparePipelineLayouts();
		preparePipelines();
		prepareCommandBuffer();
		mainLoop();
	}
};

int main(int argc, char* argv[]){
	vector<const char *> instance_extensions(getRequiredExtensions());
 	vector<const char *> validations={"VK_LAYER_KHRONOS_validation"};
	vector<const char *> device_extensions= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	string _name = "Vulkan-MarchingCube";

    for(auto s : instance_extensions) {
        cout << "instance extension name : " << s << endl;
    }
	try {
		MarchingCubeRenderer app(_name, 600, 800, instance_extensions, device_extensions, validations);
		app.run();
	} catch(std::exception& e) {
		cout << "error occured " << e.what() << " on File " << __FILE__ << " line : " << __LINE__ << endl;
		exit(EXIT_FAILURE);
	}
	return 0;
}
