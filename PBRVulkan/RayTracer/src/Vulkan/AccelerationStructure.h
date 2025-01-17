#pragma once

#include <memory>
#include <vector>
#include <cstring>
#include "Vulkan_api.h"

namespace Vulkan
{
	class AccelerationStructure
	{
	public:
		AccelerationStructure() = delete;
		AccelerationStructure(const AccelerationStructure&) = delete;
		AccelerationStructure& operator =(const AccelerationStructure&) = delete;
		AccelerationStructure& operator =(AccelerationStructure&&) = delete;
		AccelerationStructure(AccelerationStructure&& other) noexcept;
		virtual ~AccelerationStructure();

		void Create(const class Buffer& blasBuffer, VkDeviceSize resultOffset);

		static void MemoryBarrier(VkCommandBuffer commandBuffer);

		template<class T>
		static VkAccelerationStructureBuildSizesInfoKHR Reduce(
			const std::vector<T>& structures)
		{
			VkAccelerationStructureBuildSizesInfoKHR total{};

			for (const auto& accelerationStructure : structures)
			{
				total.accelerationStructureSize += accelerationStructure.buildSizesInfo.accelerationStructureSize;
				total.buildScratchSize += accelerationStructure.buildSizesInfo.buildScratchSize;
				total.updateScratchSize += accelerationStructure.buildSizesInfo.updateScratchSize;
			}

			return total;
		}
		
		[[nodiscard]] VkAccelerationStructureBuildSizesInfoKHR GetMemorySizes(const uint32_t* count) const;

		[[nodiscard]] const class Device& GetDevice() const
		{
			return device;
		}

		[[nodiscard]] const class Extensions& GetExtensions() const
		{
			return *extensions;
		}

		[[nodiscard]] VkAccelerationStructureKHR Get() const
		{
			return accelerationStructure;
		}

		VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationProperties{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR pipelineRTProperties{};

		VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};

	protected:
		AccelerationStructure(const class Device& device);

		// Rounding up to the nearest multiple of a number
		static uint64_t RoundUp(uint64_t numToRound, uint64_t multiple);

		const class Device& device;
		std::unique_ptr<class Extensions> extensions;

		VkAccelerationStructureKHR accelerationStructure{};
	};
}
