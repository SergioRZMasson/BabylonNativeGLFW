#include <filesystem>
#include <iostream>
#include <stdio.h>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics/Device.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/NativeOptimizations.h>
#include <Babylon/Plugins/NativeInput.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Canvas.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if GLFW_VERSION_MINOR < 2
#	error "GLFW 3.2 or later is required"
#endif // GLFW_VERSION_MINOR < 2

#if TARGET_PLATFORM_LINUX
#	define GLFW_EXPOSE_NATIVE_X11
#	define GLFW_EXPOSE_NATIVE_GLX
#elif TARGET_PLATFORM_OSX
#	define GLFW_EXPOSE_NATIVE_COCOA
#	define GLFW_EXPOSE_NATIVE_NSGL
#elif TARGET_PLATFORM_WINDOWS
#	define GLFW_EXPOSE_NATIVE_WIN32
#	define GLFW_EXPOSE_NATIVE_WGL
#endif //
#include <GLFW/glfw3native.h>

std::unique_ptr<Babylon::AppRuntime> runtime{};
std::unique_ptr<Babylon::Graphics::Device> device{};
std::unique_ptr<Babylon::Graphics::DeviceUpdate> update{};
Babylon::Plugins::NativeInput* nativeInput{};
std::unique_ptr<Babylon::Polyfills::Canvas> nativeCanvas{};
bool minimized = false;

#define INITIAL_WIDTH 1920
#define INITIAL_HEIGHT 1080

static void* glfwNativeWindowHandle(GLFWwindow* _window)
{
#if TARGET_PLATFORM_LINUX
		return (void*)(uintptr_t)glfwGetX11Window(_window);
#elif TARGET_PLATFORM_OSX
		return ((NSWindow*)glfwGetCocoaWindow(_window)).contentView;
#elif TARGET_PLATFORM_WINDOWS
		return glfwGetWin32Window(_window);
#endif // TARGET_PLATFORM_
}

void Uninitialize()
{
	if (device)
	{
		update->Finish();
		device->FinishRenderingCurrentFrame();
	}

	nativeInput = {};
	runtime.reset();
	nativeCanvas.reset();
	update.reset();
	device.reset();
}

void RefreshBabylon(GLFWwindow* window)
{
	Uninitialize();

	int width, height;
	glfwGetWindowSize(window, &width, &height);

	Babylon::Graphics::WindowConfiguration graphicsConfig{};
	graphicsConfig.Window = (Babylon::Graphics::WindowType)glfwNativeWindowHandle(window);
	graphicsConfig.Width = width;
	graphicsConfig.Height = height;
	graphicsConfig.MSAASamples = 4;

	device = Babylon::Graphics::Device::Create(graphicsConfig);
	update = std::make_unique<Babylon::Graphics::DeviceUpdate>(device->GetUpdate("update"));
	device->StartRenderingCurrentFrame();
	update->Start();

	runtime = std::make_unique<Babylon::AppRuntime>();

	runtime->Dispatch([](Napi::Env env)
		{
			device->AddToJavaScript(env);

			Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
				std::cout << message << std::endl;
			});

			Babylon::Polyfills::Window::Initialize(env);

			Babylon::Polyfills::XMLHttpRequest::Initialize(env);
			nativeCanvas = std::make_unique <Babylon::Polyfills::Canvas>(Babylon::Polyfills::Canvas::Initialize(env));

			Babylon::Plugins::NativeEngine::Initialize(env);

			Babylon::Plugins::NativeOptimizations::Initialize(env);

			nativeInput = &Babylon::Plugins::NativeInput::CreateForJavaScript(env); });

	Babylon::ScriptLoader loader{ *runtime };
	loader.Eval("document = {}", "");
	loader.LoadScript("app:///Scripts/ammo.js");
	// Commenting out recast.js for now because v8jsi is incompatible with asm.js.
	// loader.LoadScript("app:///Scripts/recast.js");
	loader.LoadScript("app:///Scripts/babylon.max.js");
	loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
	loader.LoadScript("app:///Scripts/babylonjs.materials.js");
	loader.LoadScript("app:///Scripts/babylon.gui.js");
	loader.LoadScript("app:///Scripts/meshwriter.min.js");
	loader.LoadScript("app:///Scripts/game.js");
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		RefreshBabylon(window);
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	int32_t x = static_cast<int32_t>(xpos);
	int32_t y = static_cast<int32_t>(ypos);

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		nativeInput->MouseDown(Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID, x, y);
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		nativeInput->MouseUp(Babylon::Plugins::NativeInput::LEFT_MOUSE_BUTTON_ID, x, y);
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		nativeInput->MouseDown(Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID, x, y);
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		nativeInput->MouseUp(Babylon::Plugins::NativeInput::RIGHT_MOUSE_BUTTON_ID, x, y);
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
		nativeInput->MouseDown(Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID, x, y);
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
		nativeInput->MouseUp(Babylon::Plugins::NativeInput::MIDDLE_MOUSE_BUTTON_ID, x, y);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	int32_t x = static_cast<int32_t>(xpos);
	int32_t y = static_cast<int32_t>(ypos);

	nativeInput->MouseMove(x, y);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	nativeInput->MouseWheel(Babylon::Plugins::NativeInput::MOUSEWHEEL_Y_ID, -yoffset * 100.0);
}

static void window_resize_callback(GLFWwindow* window, int width, int height)
{
	device->UpdateSize(width, height);
}

int main()
{
	if (!glfwInit())
		exit(EXIT_FAILURE);

	auto window = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT, "Simple example", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetKeyCallback(window, key_callback);
	glfwSetWindowSizeCallback(window, window_resize_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);

	RefreshBabylon(window);

	while (!glfwWindowShouldClose(window))
	{
		if (device)
		{
			update->Finish();
			device->FinishRenderingCurrentFrame();
			device->StartRenderingCurrentFrame();
			update->Start();
		}
		glfwPollEvents();
	}

	Uninitialize();
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}