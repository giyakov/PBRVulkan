#include "SaveWidget.h"

#include <imgui.h>

namespace Interface
{
	void SaveWidget::Render(Settings& settings)
	{
		ImGui::Text("Save result");
		ImGui::Separator();

		constexpr auto BUF_SIZE = 256;
		static char buf[BUF_SIZE] = "image.ppm";
		ImGui::InputText("Image file name", buf, BUF_SIZE);
		settings.SavedImageName = buf;

		settings.ShouldSaveImage = ImGui::Button("Save");
	}
}
