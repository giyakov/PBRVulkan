#pragma once

#include "../Vulkan/Raytracer.h"
#include "../Vulkan/Computer.h"

#include <string>

namespace Tracer
{
	/**
	 * The inheritance chain is as follows:
	 *
	 * [Application -> Raytracer -> Rasterizer -> Core]
	 */
	class Application final : public Vulkan::Raytracer
	{
	public:
		Application();
		~Application();

		void Run() override;

	private:
		void RegisterCallbacks();
		void UpdateUniformBuffer(uint32_t imageIndex) override;
		void Render(VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, uint32_t imageIndex) override;
		void LoadScene() override;
		void UpdateSettings();
		void CompileShaders() const;
		void RecreateSwapChain();
		void RecompileShaders();
		void CreateMenu();
		void ResetAccumulation();
		void ResizeWindow() const;
		void CheckScenesFolder();
		void PrintGPUInfo() const;
		void CreateComputePipeline();
		void ComputePipeline(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;
		void SaveImage(std::string const &name,  uint32_t imageIndex);
		void CreateTmpImage();

		// User interface API
		void OnKeyChanged(int key, int scanCode, int action, int mods) override;
		void OnCursorPositionChanged(double xpos, double ypos) override;
		void OnMouseButtonChanged(int button, int action, int mods) override;
		void OnScrollChanged(double xoffset, double yoffset) override;

		std::unique_ptr<class Menu> menu;
		std::unique_ptr<class Compiler> compiler;
		std::unique_ptr<class Vulkan::Computer> computer;
		std::unique_ptr<class Vulkan::Image> tmpImage;

		uint32_t frame = 0;
		uint32_t imageIndex = 0;
		bool terminate;
		bool newScene = false;
	};
}
