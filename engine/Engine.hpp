#pragma once

#include "Platform.hpp"
#include "VkInit.hpp"

struct Engine
{
	std::unique_ptr<Fact<GlfwInitialization>> glfw;
	std::unique_ptr<Fact<GlfwWindow>>         window;
	VkAll                                     vulkan;

	static Engine init(WindowInitConfig windowConfig) {
		Engine engine {};
		engine.glfw = std::make_unique<Fact<GlfwInitialization>>();
		engine.window = std::make_unique<Fact<GlfwWindow>>(std::in_place, engine.glfw->proof(), windowConfig);
		engine.vulkan = VkAll::init(engine.glfw->proof(), engine.window->proof());
		return engine;
	}
};

bool isShutdownRequested(const Engine& engine) {
	return windowShouldClose(**engine.window);
}

// Blocks till completion.
int execute(Engine& engine) {
	while (!isShutdownRequested(engine)) {
		std::vector<GlfwEvent> events = windowGetEvents(**engine.window);
		// TODO handle window events;
	}
	return 0;
}
