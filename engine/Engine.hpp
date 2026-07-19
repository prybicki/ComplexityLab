#pragma once

#include <optional>

#include "Platform.hpp"
#include "VkInit.hpp"

struct Engine
{
	std::unique_ptr<Fact<GlfwInitialization>> glfw;
	std::optional<GlfwWindow>  window;

	std::unique_ptr<Fact<Instance>> instance;

	static Engine init(std::optional<WindowInitConfig> windowConfig) {
		Engine engine {};
		// We always initialize GLFW to init Vulkan surface-ready, even in windowless mode.
		engine.glfw = std::make_unique<Fact<GlfwInitialization>>();
		if (windowConfig.has_value()) {
			engine.window.emplace(engine.glfw->proof(), *windowConfig);
		}
		engine.instance = std::make_unique<Fact<Instance>>(Instance::init(engine.glfw->proof()));
		return engine;
	}
};

bool isShutdownRequested(const Engine& engine) {
	if (engine.window.has_value()) {
		return windowShouldClose(*engine.window);
	}
	return false;
}

// Blocks till completion.
int execute(Engine& engine) {
	while (!isShutdownRequested(engine)) {
		if (engine.window.has_value()) {
			std::vector<GlfwEvent> events = windowGetEvents(*engine.window);
			// TODO handle window events;
		}

	}
	return 0;
}
