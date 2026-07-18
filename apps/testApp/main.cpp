#include <imgui.h>
#include <implot.h>

#include <cstdio>

#include "Engine.hpp"

int main()  // _AI
{
    ImGui::CreateContext();
    ImPlot::CreateContext();
    std::printf("imgui %s / implot %s\n", ImGui::GetVersion(), IMPLOT_VERSION);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    Engine engine = Engine::init(WindowInitConfig::default_());
    return execute(engine);
}
