#ifndef __VK_APPLICATION_CPP__
#define __VK_APPLICATION_CPP__
#define GLFW_INCLUDE_VULKAN 1
#include "vk_application.h"
#include <GLFW/glfw3.h>

using namespace std;

namespace VKEngine{
	Application::Application(
		string name,
		uint32_t _height, uint32_t _width, 
		const vector<const char *> _instance_extension_names,
		const vector<const char *> _device_extension_names,
		const vector<const char *> _validation_names){
		height = _height;
		width = _width;
		engine = new Engine(name,_instance_extension_names, _device_extension_names ,_validation_names);
		camera.init( glm::vec3(0.0f, 0.0f, -3.0f), glm::vec3(0.0f, 0.0f, 0.0f) );
	}	

	Application::~Application(){
		destroy();
	}

	void Application::initVulkan(){
		LOG("VKEngine init!\n");
        //engine->createInstance();
		engine->init();
		LOG("Application::createSurcace()!\n");
		createSurface();
        LOG("Application::createPhysicalDevice()\n");
        createPhysicalDevice();
		LOG("Application::createContext()!\n");
		createContext();
		LOG("Application::initSwapchain()!\n");
		initSwapchain();
		LOG("Application::setupSwapchain()!\n");
		setupSwapchain();
		LOG("Application::setupCommandQueue()!\n");
		setupCommandQueue();
		LOG("Application::setupPipelineCache()!\n");
		setupPipelineCache();
		LOG("Application::setupDepthStencilAttachment()!\n");
		setupDepthStencilAttachment();
		LOG("Application::setupRenderpass()!\n");
		setupRenderPass();
		LOG("Application::setupFramebuffer()!\n");
		setupFramebuffer();
		LOG("Application::setupSemaphores()!\n");
		setupSemaphores();
		LOG("Application::setupSubmitInfo()!\n");
		setupSubmitInfo();
	}

	void Application::initWindow(){
		// LOG("App Init Window\n");
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
		if(!window){
			LOG("fail to create glfwWindow\n");
			exit(EXIT_FAILURE);
		}
	}

    void Application::createPhysicalDevice(){
        device = new PhysicalDevice(engine);
        device->useGPU(0);
        device->init();
    }

	void Application::createContext(){
		VkInstance instance = engine->getInstance();
        LOG("Application::createContext engine->getInstance() = %d\n", instance);
		context = new Context(device, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT);
        LOG("Application::createContext new Context complete\n");
        context->initDevice();
	   LOG("Application::createContext context initDevice complete\n");
    }
	void Application::createSurface(){
		VkInstance instance = engine->getInstance();
		VK_CHECK_RESULT(glfwCreateWindowSurface(instance, window, nullptr, &surface));
	}
	
	void Application::initSwapchain(){
		swapchain = new Swapchain(context, surface);
		swapchain->setImageSharingMode(VK_SHARING_MODE_EXCLUSIVE);
		swapchain->setVsync(true);
		swapchain->init();
	}

	void Application::setupCamera(){
		camera.init(glm::vec3(0.0f, 0.0f, -3.0f), glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width/(float)height, 0.1f, 10.0f);
	}

	void Application::setupSwapchain(){
		swapchain->create(&width, &height);
	}

	void Application::setupCommandQueue(){
		// LOG("Apllication::setupCommandQueue called\n");
		graphics_queue = new CommandQueue(context, VK_QUEUE_GRAPHICS_BIT);
		compute_queue = new CommandQueue(context, VK_QUEUE_COMPUTE_BIT);
	}
	
	void Application::init(){
		initWindow();
		initVulkan();
	}	

	void Application::setupPipelineCache(){
		VkPipelineCacheCreateInfo cache_CI = {};
		cache_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT( vkCreatePipelineCache(context->getDevice(), &cache_CI, nullptr, &cache) );
	}

	void Application::setupDepthStencilAttachment(){
		VkDevice device = context->getDevice();
		VkBool32 found = getDepthFormat( context->getPhysicalDevice()->getPhysicalDevice(), &depth_attachment.format);
		VkImageCreateInfo image_CI = infos::imageCreateInfo();
		image_CI.imageType = VK_IMAGE_TYPE_2D;
		image_CI.format = depth_attachment.format;
		image_CI.samples = VK_SAMPLE_COUNT_1_BIT;
		image_CI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_CI.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_CI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_CI.extent = {width, height, 1};
		image_CI.mipLevels = 1;
		image_CI.arrayLayers = 1;
		VK_CHECK_RESULT(vkCreateImage(device, &image_CI, nullptr, &depth_attachment.image));

		VkMemoryAllocateInfo mem_AI = infos::memoryAllocateInfo();
		VkMemoryRequirements mem_reqs;
		vkGetImageMemoryRequirements(device, depth_attachment.image, &mem_reqs);

		mem_AI.memoryTypeIndex = context->getPhysicalDevice()->getMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mem_AI.allocationSize = mem_reqs.size;
		VK_CHECK_RESULT(vkAllocateMemory(device, &mem_AI, nullptr, &depth_attachment.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, depth_attachment.image, depth_attachment.memory, 0));
		
		VkImageViewCreateInfo view_CI = infos::imageViewCreateInfo();
		view_CI.image = depth_attachment.image;
		view_CI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_CI.format = depth_attachment.format;
		view_CI.subresourceRange.baseArrayLayer = 0;
		view_CI.subresourceRange.baseMipLevel = 0;
		view_CI.subresourceRange.layerCount  = 1;
		view_CI.subresourceRange.levelCount  = 1;
		view_CI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if(depth_attachment.format >= VK_FORMAT_D16_UNORM_S8_UINT){
			view_CI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		VK_CHECK_RESULT(vkCreateImageView(device, &view_CI, nullptr, &depth_attachment.view));
	}
	
	void Application::setupRenderPass(){
		VkDevice device = context->getDevice();
		std::array<VkAttachmentDescription, 2> attachments={};
		//Color first
		attachments[0].format = swapchain->getFormat().format;
		// LOG("swapchain image format : %d\n", swapchain.image_format);
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		//Depth
		attachments[1].format = depth_attachment.format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depth_reference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkSubpassDescription subpass_description = {};
		subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_description.colorAttachmentCount = 1;
		subpass_description.pColorAttachments = &color_reference;
		subpass_description.pDepthStencilAttachment = &depth_reference;
		subpass_description.pInputAttachments = nullptr;
		subpass_description.preserveAttachmentCount = 0;
		subpass_description.pPreserveAttachments = nullptr;
		subpass_description.pResolveAttachments = nullptr;

		std::array<VkSubpassDependency, 2> dependencies = {};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderpass_CI = infos::renderPassCreateInfo();
		renderpass_CI.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderpass_CI.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderpass_CI.subpassCount = 1;
		renderpass_CI.pAttachments = attachments.data();
		renderpass_CI.pDependencies = dependencies.data();
		renderpass_CI.pSubpasses = &subpass_description;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderpass_CI, nullptr, &render_pass));
	}

	void Application::setupFramebuffer(){
		VkDevice device = context->getDevice();
		uint32_t nr_framebuffers = static_cast<uint32_t>(swapchain->getImageCount());
		LOG("framebuffer count : %d\n", nr_framebuffers);
		framebuffers.clear();
		framebuffers.resize(nr_framebuffers);

		std::array<VkImageView, 2> attachments = {};
		attachments[1] = depth_attachment.view;
		VkFramebufferCreateInfo framebuffer_CI = {};
		framebuffer_CI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_CI.renderPass = render_pass;
		framebuffer_CI.layers = 1;
		framebuffer_CI.width = width;
		framebuffer_CI.height = height;
		
		for(uint32_t i = 0 ; i < nr_framebuffers ; ++i){
			attachments[0] = swapchain->getImageViews()[i];
			framebuffer_CI.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebuffer_CI.pAttachments = attachments.data();
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebuffer_CI, nullptr, &framebuffers[i]));
		}
	}

	void Application::setupSemaphores(){
		VK_CHECK_RESULT(context->createSemaphore(&semaphores.render_complete));
		VK_CHECK_RESULT(context->createSemaphore(&semaphores.present_complete));
		VK_CHECK_RESULT(context->createFence(&draw_fence)); 
	}

	void Application::setupSubmitInfo(){
		render_SI = infos::submitInfo();
		render_SI.pWaitSemaphores = &semaphores.present_complete;
		render_SI.waitSemaphoreCount = 1;
		render_SI.pSignalSemaphores = &semaphores.render_complete;
		render_SI.signalSemaphoreCount = 1;
		render_SI.pWaitDstStageMask = &submit_pipeline_stages;
	}

	void Application::prepareFrame(){
		VkResult result = Presenter::acquire(context, swapchain, &current_frame_index, &semaphores.present_complete);
		if( (result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)){
			//TODO window resize
			LOG("Application::preprareFrame result is VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR\n");
		}else{
			VK_CHECK_RESULT(result);
		}
	}

	void Application::submitFrame(){
		// VkQueue queue = graphics_queue->getQueue();
		// VkResult result = swapchain.queuePresent(queue, current_frame_index, semaphores.render_complete);
		VkResult result = Presenter::present(graphics_queue, swapchain, &current_frame_index, &semaphores.render_complete);
		if(!(result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR)){
			LOG("Application::submitFrame result is VK_SUCCESS or VK_SUBOPTIMAL_KHR\n");
			//TODO window resize
			return ;
		}else{
			VK_CHECK_RESULT(result);
		}
		VK_CHECK_RESULT(graphics_queue->waitIdle());
	}

	void Application::render(){
		prepareFrame();
		render_SI.pCommandBuffers = &draw_command_buffers[current_frame_index];
		render_SI.commandBufferCount = 1;
		graphics_queue->resetFences(&draw_fence, 1);
		vkQueueSubmit( graphics_queue->getQueue(), 1, & render_SI, draw_fence);
		graphics_queue->waitFences(&draw_fence, 1, true, UINT64_MAX);
		submitFrame();
		current_frame_index+=1;
		current_frame_index%=swapchain->getImageCount();
	}

	void Application::destroyFramebuffers(){
		VkDevice device = context->getDevice();
		for(uint32_t i = 0 ; i < swapchain->getImageCount() ; ++i){
			vkDestroyFramebuffer(device, framebuffers[i], nullptr);
		}
		framebuffers.clear();
		vkDestroyImageView(device, depth_attachment.view, nullptr);
		vkFreeMemory(device, depth_attachment.memory, nullptr);
		vkDestroyImage(device, depth_attachment.image, nullptr);
		vkDestroyRenderPass(device, render_pass, nullptr);
	}
	
	void Application::destroy(){
		VkInstance instance = context->getPhysicalDevice()->getEngine()->getInstance();
		PipelineCacheBuilder::destroy(context, &cache);
		context->destroyFence(&draw_fence);
		delete swapchain;
		if(surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR( instance, surface, nullptr);
		context->destroySemaphore(&semaphores.present_complete);
		context->destroySemaphore(&semaphores.render_complete);
		destroyFramebuffers();
		delete graphics_queue;
		delete compute_queue;
		delete context;
		delete engine;
	}
}
#endif
