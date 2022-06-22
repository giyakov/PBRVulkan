#pragma once

#include "Vulkan_api.h"

namespace Vulkan
{
	class Framebuffer final
	{
	public:
		NON_COPIABLE(Framebuffer)

		Framebuffer(const class ImageView& imageView,
		            const class SwapChain& swapChain,
		            const class DepthBuffer& depthBuffer,
		            VkRenderPass renderPass);
		~Framebuffer();

		[[nodiscard]] VkFramebuffer Get() const
		{
			return framebuffer;
		}

	private:
		const ImageView& imageView;
		VkFramebuffer framebuffer{};
	};
}
