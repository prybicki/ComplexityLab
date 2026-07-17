// Placeholder app — testApp keeps the apps/ layer wired up and buildable until
// cl_engine has a real entry point to drive. It touches imgui/implot only to
// prove the dependency chain reaches an app through cl_engine; deliberately no
// window, so it stays runnable headless. Replace this body with the real thing.

#include <imgui.h>
#include <implot.h>

#include <cstdio>

int main()  // _AI
{
    ImGui::CreateContext();
    ImPlot::CreateContext();
    std::printf("imgui %s / implot %s\n", ImGui::GetVersion(), IMPLOT_VERSION);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    return 0;
}
