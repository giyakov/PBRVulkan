#pragma once

#include "Vulkan_api.h"

namespace Vulkan
{
	class Semaphore final
	{
	public:
		NON_COPIABLE(Semaphore)

		explicit Semaphore(const class Device& device);
		~Semaphore();

		[[nodiscard]] VkSemaphore Get() const
		{
			return semaphore;
		}

	private:
		const Device& device;
		VkSemaphore semaphore;
	};
}
