#pragma once

#include "Widget.h"

namespace Interface
{
	class SaveWidget final : public Widget
	{
	public:
		~SaveWidget() = default;

		void Render(Settings& settings) override;
	};
}
