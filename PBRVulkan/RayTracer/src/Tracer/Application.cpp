#include "Application.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>
#include <iostream>

#include "Menu.h"
#include "Scene.h"
#include "Camera.h"
#include "Compiler.h"

#include "../Geometry/Global.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/Buffer.h"
#include "../Vulkan/Device.h"
#include "../Vulkan/Semaphore.h"
#include "../Vulkan/CommandBuffers.h"
#include "../Vulkan/Instance.h"
#include "../Vulkan/Image.h"
#include "../Vulkan/SwapChain.h"
#include "../Vulkan/ImageView.h"
#include "../Vulkan/Command.cpp"
#include "../Vulkan/Memory.h"

#include "../path.h"

#include "Widgets/CinemaWidget.h"
#include "Widgets/RendererWidget.h"
#include "Widgets/SceneWidget.h"
#include "Widgets/SaveWidget.h"

#include <iostream>
#include <fstream>
#include <chrono>

class Timer {
public:
    static void start()
    {
        m_start = std::chrono::system_clock::now();
    }

    static auto stop()
    {
        m_stop = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_stop - m_start);
        return duration.count();
    }
private:
    static std::chrono::time_point<std::chrono::system_clock> m_start;
    static std::chrono::time_point<std::chrono::system_clock> m_stop;
};
std::chrono::time_point<std::chrono::system_clock> Timer::m_start = std::chrono::system_clock::now();
std::chrono::time_point<std::chrono::system_clock> Timer::m_stop = std::chrono::system_clock::now();


namespace Tracer
{
	Application::Application()
	{
		PrintGPUInfo();
		CheckScenesFolder();
		if (terminate) return;

		LoadScene();
		compiler.reset(new Compiler());
		CompileShaders();
		CreateAS();
		RegisterCallbacks();
		Raytracer::CreateSwapChain();
		CreateMenu();
		CreateComputePipeline();
		CreateTmpImage();
	}

	Application::~Application()
	{
	}

	void Application::CreateTmpImage()
	{
		tmpImage.reset(new Vulkan::Image(*device, swapChain->Extent, swapChain->Format, VK_IMAGE_TILING_LINEAR,
			VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	}

	void Application::LoadScene()
	{
		scene.reset(new Scene(Interface::SceneWidget::GetScenePath(settings.SceneId), *device, *commandPool));
	}

	void Application::UpdateSettings()
	{
		if (settings.SceneId != menu->GetSettings().SceneId) {
			RecreateSwapChain();
			newScene = true;
		}

		if (settings.RequiresShaderRecompliation(menu->GetSettings()))
			RecompileShaders();

		if (settings.RequiresAccumulationReset(menu->GetSettings()))
			ResetAccumulation();

		settings = menu->GetSettings();
		if (glfwGetKey(instance->GetWindow().Get(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&
			glfwGetKey(instance->GetWindow().Get(), GLFW_KEY_S) == GLFW_PRESS)
		{
			settings.ShouldSaveImage = true;
		}
	}

	void Application::CompileShaders() const
	{
		std::vector<Parser::Define> defines;
		std::vector<Parser::Include> includes;

		if (scene->UseHDR())
			defines.push_back(Parser::Define::USE_HDR);

		if (settings.UseGammaCorrection)
			defines.push_back(Parser::Define::USE_GAMMA_CORRECTION);

		includes.push_back(static_cast<Parser::Include>(settings.IntegratorType));

		compiler->Compile(includes, defines);
	}

	void Application::RecreateSwapChain()
	{
		Vulkan::Command::Submit(*commandPool, [this](VkCommandBuffer commandBuffer)
		{
			Clear(commandBuffer, this->imageIndex);
		});

		if (!scene->IsValid(Interface::SceneWidget::GetScenePath(menu->GetSettings().SceneId)))
			return;

		settings = menu->GetSettings();
		device->WaitIdle();
		menu.reset();
		Raytracer::DeleteSwapChain();
		LoadScene();
		ResizeWindow();
		CompileShaders();
		CreateAS();
		Raytracer::CreateSwapChain();
		CreateMenu();
		ResetAccumulation();
		CreateComputePipeline();
		CreateTmpImage();
	}

	void Application::RecompileShaders()
	{
		settings = menu->GetSettings();
		device->WaitIdle();
		CompileShaders();
		Raytracer::CreateGraphicsPipeline();
		ResetAccumulation();
	}

	void Application::CreateMenu()
	{
		const auto renderer = scene->GetRendererOptions();
		settings.MaxDepth = renderer.maxDepth;
		settings.UseEnvMap = renderer.useEnvMap;
		settings.HdrMultiplier = renderer.hdrMultiplier;

		menu.reset(new Menu(*device, *swapChain, *commandPool, settings));

		menu->AddWidget(std::make_shared<Interface::SceneWidget>());
		menu->AddWidget(std::make_shared<Interface::RendererWidget>());
		menu->AddWidget(std::make_shared<Interface::CinemaWidget>());
		menu->AddWidget(std::make_shared<Interface::SaveWidget>());
	}

	void Application::ResetAccumulation()
	{
		frame = 0;
	}

	void Application::ResizeWindow() const
	{
		const auto resolution = scene->GetRendererOptions().resolution;
		glfwSetWindowSize(window->Get(), resolution.x, resolution.y);
	}

	void Application::CheckScenesFolder()
	{
		auto root = Path::Root({ "PBRVulkan", "Assets", "PBRScenes" });
		terminate = !std::filesystem::exists(root);
		
		if (terminate)
		{
			std::cout << "[ERROR] Scenes folder does not exists. Download https://github.com/Zielon/PBRScenes!" <<
				std::endl;
		}
	}

	void Application::UpdateUniformBuffer(uint32_t imageIndex)
	{
		Uniforms::Global uniform{};

		uniform.view = scene->GetCamera().GetView();
		uniform.projection = scene->GetCamera().GetProjection();
		uniform.cameraPos = scene->GetCamera().GetPosition();
		uniform.lights = scene->GetLightsSize();
		uniform.ssp = settings.SSP;
		uniform.maxDepth = settings.MaxDepth;
		uniform.aperture = settings.Aperture;
		uniform.focalDistance = settings.FocalDistance;
		uniform.hdrMultiplier = scene->UseHDR() ? settings.HdrMultiplier : 0.f;
		uniform.hdrResolution = scene->UseHDR() ? scene->GetHDRResolution() : 0.f;
		uniform.frame = frame;
		uniform.AORayLength = settings.AORayLength;
		uniform.integratorType = settings.IntegratorType;
		uniform.doubleSided = settings.DoubleSidedLight;

		uniformBuffers[imageIndex]->Fill(&uniform);
	}

	void Application::Render(VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		Camera::TimeDeltaUpdate();

		this->imageIndex = imageIndex;

		if (scene->GetCamera().OnBeforeRender())
			ResetAccumulation();

		if (settings.UseRasterizer)
		{
			Clear(commandBuffer, imageIndex);
			Rasterizer::Render(framebuffer, commandBuffer, imageIndex);
		}
		else
			Raytracer::Render(framebuffer, commandBuffer, imageIndex);

		ComputePipeline(commandBuffer, imageIndex);

		SaveImage(settings.SavedImageName, imageIndex);
		menu->Render(framebuffer, commandBuffer);

		++frame;
	}

	void Application::RegisterCallbacks()
	{
		window->AddOnCursorPositionChanged([this](const double xpos, const double ypos)-> void
		{
			OnCursorPositionChanged(xpos, ypos);
		});

		window->AddOnKeyChanged([this](const int key, const int scancode, const int action, const int mods)-> void
		{
			OnKeyChanged(key, scancode, action, mods);
		});

		window->AddOnMouseButtonChanged([this](const int button, const int action, const int mods)-> void
		{
			OnMouseButtonChanged(button, action, mods);
		});

		window->AddOnScrollChanged([this](const double xoffset, const double yoffset)-> void
		{
			OnScrollChanged(xoffset, yoffset);
		});
	}

	void Application::OnKeyChanged(int key, int scanCode, int action, int mods)
	{
		if (menu->WantCaptureKeyboard() || !swapChain)
			return;

		if (glfwGetKey(instance->GetWindow().Get(), GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS)
			scene->GetCamera().OnKeyChanged(key, scanCode, action, mods);
	}

	void Application::OnCursorPositionChanged(double xpos, double ypos)
	{
		if (menu->WantCaptureKeyboard() || menu->WantCaptureMouse() || !swapChain)
			return;

		if (scene->GetCamera().OnCursorPositionChanged(xpos, ypos))
			ResetAccumulation();
	}

	void Application::OnMouseButtonChanged(int button, int action, int mods)
	{
		if (menu->WantCaptureMouse() || !swapChain)
			return;

		scene->GetCamera().OnMouseButtonChanged(button, action, mods);
	}

	void Application::OnScrollChanged(double xoffset, double yoffset)
	{
		if (menu->WantCaptureMouse())
			return;
	}

	void Application::PrintGPUInfo() const
	{
		// Selected by default CreatePhysicalDevice() in Core.cpp
		auto* physicalDevice = instance->GetDevices().front();

		uint32_t instanceVersion = VK_API_VERSION_1_2;
		vkEnumerateInstanceVersion(&instanceVersion );
		uint32_t major = VK_VERSION_MAJOR(instanceVersion);
		uint32_t minor = VK_VERSION_MINOR(instanceVersion);
		uint32_t patch = VK_VERSION_PATCH(instanceVersion);

		std::cout << "[INFO] Vulkan SDK version: " << major << "." << minor << "." << patch << std::endl;
		std::cout << "[INFO] Available Vulkan devices:" << std::endl;
		for (const auto& device : instance->GetDevices())
		{
			VkPhysicalDeviceProperties prop;
			vkGetPhysicalDeviceProperties(device, &prop);
			const auto* selected = device == physicalDevice ? "	 (x) " : " ( ) ";
			std::cout << selected << prop.deviceName << std::endl;
		}
	}

	void Application::CreateComputePipeline()
	{
		computer.reset(new Vulkan::Computer(
			*swapChain,
			*device,
			GetOutputImageView(),
			GetNormalsImageView(),
			GetPositionImageView()));
	}

	void Application::ComputePipeline(VkCommandBuffer commandBuffer, uint32_t imageIndex) const
	{
		// It uses the previous frame buffers
		if (settings.UseComputeShaders)
		{
			// Compute pipeline
			{
				computer->BuildCommand(settings.ComputeShaderId);

				VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
				VkCommandBuffer commandBuffers[]{computer->GetCommandBuffers()[0]};

				VkSubmitInfo computeSubmitInfo{};
				computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				computeSubmitInfo.pWaitDstStageMask = waitStages;
				computeSubmitInfo.commandBufferCount = 1;
				computeSubmitInfo.pCommandBuffers = commandBuffers;

				Vulkan::VK_CHECK(vkQueueSubmit(device->ComputeQueue, 1, &computeSubmitInfo, nullptr),
				                 "Compute shader submit failed!");
			}

			Copy(commandBuffer, computer->GetOutputImage().Get(), swapChain->GetImage()[imageIndex]);
		}
	}

	void insertImageMemoryBarrier(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	void Application::SaveImage(std::string const &name, uint32_t imageIndex)
	{
		if (!settings.ShouldSaveImage) {
			return;
		}

		std::cout << "Save image called with name = [" << name << "]\n";

		bool supportsBlit = true;

		// Check blit support for source and destination
		VkFormatProperties formatProps;

		// Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
		vkGetPhysicalDeviceFormatProperties(device->GetPhysical(), swapChain->Format, &formatProps);
		if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
			supportsBlit = false;
		}

		// Check if the device supports blitting to linear images
		vkGetPhysicalDeviceFormatProperties(device->GetPhysical(), VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
		if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
			supportsBlit = false;
		}

		// Source for the copy is the last rendered swapchain image
		VkImage srcImage = swapChain->GetImage()[imageIndex];

		// Create the linear tiled destination image to copy to and to read the memory from
		VkImageCreateInfo imageCreateCI = {};
		imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
		// Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
		imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateCI.extent.width = swapChain->Extent.width;
		imageCreateCI.extent.height = swapChain->Extent.height;
		imageCreateCI.extent.depth = 1;
		imageCreateCI.arrayLayers = 1;
		imageCreateCI.mipLevels = 1;
		imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		// Create the image
		VkImage dstImage;
		Vulkan::VK_CHECK(vkCreateImage(device->Get(), &imageCreateCI, nullptr, &dstImage), "");
		// Create memory to back up the image
		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo = {};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkDeviceMemory dstImageMemory;
		vkGetImageMemoryRequirements(device->Get(), dstImage, &memRequirements);
		memAllocInfo.allocationSize = memRequirements.size;
		// Memory must be host visible to copy from
		memAllocInfo.memoryTypeIndex = tmpImage->GetMemory().FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		Vulkan::VK_CHECK(vkAllocateMemory(device->Get(), &memAllocInfo, nullptr, &dstImageMemory), "");
		Vulkan::VK_CHECK(vkBindImageMemory(device->Get(), dstImage, dstImageMemory, 0), "");


		auto cmdBuffers = Vulkan::CommandBuffers(*commandPool, 1);
		// Do the actual blit from the swapchain image to our host visible destination image
		VkCommandBuffer copyCmd = cmdBuffers[0];
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(copyCmd, &beginInfo);
		// Transition destination image to transfer destination layout
		insertImageMemoryBarrier(
			copyCmd,
			dstImage,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		// Transition swapchain image from present to transfer source layout
		insertImageMemoryBarrier(
			copyCmd,
			srcImage,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		// If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
		if (supportsBlit)
		{
			// Define the region to blit (we will blit the whole swapchain image)
			VkOffset3D blitSize;
			blitSize.x = swapChain->Extent.width;
			blitSize.y = swapChain->Extent.height;
			blitSize.z = 1;
			VkImageBlit imageBlitRegion{};
			imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlitRegion.srcSubresource.layerCount = 1;
			imageBlitRegion.srcOffsets[1] = blitSize;
			imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlitRegion.dstSubresource.layerCount = 1;
			imageBlitRegion.dstOffsets[1] = blitSize;

			// Issue the blit command
			vkCmdBlitImage(
				copyCmd,
				srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageBlitRegion,
				VK_FILTER_NEAREST);
		}
		else
		{
			// Otherwise use image copy (requires us to manually flip components)
			VkImageCopy imageCopyRegion{};
			imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.srcSubresource.layerCount = 1;
			imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.dstSubresource.layerCount = 1;
			imageCopyRegion.extent.width = swapChain->Extent.width;
			imageCopyRegion.extent.height = swapChain->Extent.height;
			imageCopyRegion.extent.depth = 1;

			// Issue the copy command
			vkCmdCopyImage(
				copyCmd,
				srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageCopyRegion);
		}

		// Transition destination image to general layout, which is the required layout for mapping the image memory later on
		insertImageMemoryBarrier(
			copyCmd,
			dstImage,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		// Transition back the swap chain image after the blit is done
		insertImageMemoryBarrier(
			copyCmd,
			srcImage,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		vkEndCommandBuffer(copyCmd);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &copyCmd;
		submitInfo.commandBufferCount = 1;

		vkQueueSubmit(device->GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device->GraphicsQueue);

		// Get layout of the image (including row pitch)
		VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout subResourceLayout;
		vkGetImageSubresourceLayout(device->Get(), dstImage, &subResource, &subResourceLayout);

		// Map image memory so we can start copying from it
		const char* data;
		vkMapMemory(device->Get(), dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
		data += subResourceLayout.offset;

		std::ofstream file(name, std::ios::out | std::ios::binary);

		// ppm header
		file << "P6\n" << swapChain->Extent.width << "\n" << swapChain->Extent.height << "\n255\n";

		// If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
		bool colorSwizzle = false;
		// Check if source is BGR
		// Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
		if (!supportsBlit)
		{
			std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
			colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain->Format) != formatsBGR.end());
		}

		// ppm binary pixel data
		for (uint32_t y = 0; y < swapChain->Extent.height; y++)
		{
			unsigned int *row = (unsigned int*)data;
			for (uint32_t x = 0; x < swapChain->Extent.width; x++)
			{
				if (colorSwizzle)
				{
					file.write((char*)row+2, 1);
					file.write((char*)row+1, 1);
					file.write((char*)row, 1);
				}
				else
				{
					file.write((char*)row, 3);
				}
				row++;
			}
			data += subResourceLayout.rowPitch;
		}
		file.close();

		// Clean up resources
		vkUnmapMemory(device->Get(), dstImageMemory);
		vkFreeMemory(device->Get(), dstImageMemory, nullptr);
		vkDestroyImage(device->Get(), dstImage, nullptr);
	}

	const char* scenes[18] = {
		"Ajax",
		"Bedroom",
		"Boy",
		"Coffee cart",
		"Coffee maker",
		"Cornell box",
		"Dining room",
		"Dragon",
		"Hyperion",
		"Panther",
		"Spaceship",
		"Staircase",
		"Stormtroopers",
		"Teapot",
		"Hyperion II",
		"Mustang",
		"Mustang Red",
		"Furnace"
	};

	void Application::Run()
	{
		static int frameCounter = 0;
		static int64_t allFrameTimes = 0;

		while (!glfwWindowShouldClose(window->Get()) && !terminate)
		{
			glfwPollEvents();
			UpdateSettings();

			if (frameCounter < 100) {
				Timer::start();
				DrawFrame();
				allFrameTimes += Timer::stop();
				frameCounter++;
			} else {
				double avgFrameTime = allFrameTimes / 100.0;
				frameCounter = 0;
				allFrameTimes = 0;
				std::cout << "Scene " << scenes[settings.SceneId] << " avg{100} = " << avgFrameTime << '\n';
				DrawFrame();
			}
		}

		device->WaitIdle();
	}
}
