#ifndef __VK_APPLICATION_H__
#define __VK_APPLICATION_H__
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "vk_core.h"
#include "tools.h"

using namespace glm;
namespace VKEngine{
	class Application{
		public :
		GLFWwindow *window;
		uint32_t height, width;
		protected :
		Engine *engine;
        PhysicalDevice *device;
		Context *context;
		
		VkSurfaceKHR surface;
		
		CommandQueue *graphics_queue, *compute_queue;
		Swapchain *swapchain;
		
		VkPipelineCache cache;
		VkGraphicsPipelineCreateInfo graphics_pipeline_CI_preset;
		ImageAttachment color_attachment, depth_attachment;
		VkRenderPass render_pass = VK_NULL_HANDLE;
		vector<VkFramebuffer> framebuffers;
		VkSubmitInfo render_SI;
		VkPipelineStageFlags submit_pipeline_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		vector<VkCommandBuffer> draw_command_buffers;
		VkFence draw_fence = VK_NULL_HANDLE;

		GraphicTools::Camera camera;
		struct Semaphores
		{
			VkSemaphore present_complete;
			VkSemaphore render_complete;	
		}semaphores;

		uint32_t current_frame_index = 0;
		public :
		explicit Application(
			string name,
			uint32_t _height, uint32_t _width, 
			vector<const char *>_instance_extension_names,
			vector<const char *> device_extension_names,
			vector<const char *>_validation_names);
		~Application();
		virtual void initVulkan();
		virtual void init();
		
		protected :
		virtual void destroy();
		virtual void initWindow();
		virtual void setupCamera();
		virtual void createSurface();
        virtual void createPhysicalDevice();
		virtual void createContext();
		virtual void mainLoop(){};
		virtual void initSwapchain();
		virtual void setupSwapchain();
		virtual void setupCommandQueue();
		virtual void setupPipelineCache();
		virtual void setupDepthStencilAttachment();
		virtual void setupRenderPass();
		virtual void setupFramebuffer();
		virtual void setupGraphicsPipeline(){};
		virtual void setupSemaphores();
		virtual void setupSubmitInfo();
		virtual void render();
		virtual void destroyFramebuffers();
		void prepareFrame();
		void submitFrame();
	};
};
#endif
