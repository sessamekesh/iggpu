#ifndef IGGPU_PLATFORM_APP_BASE_H
#define IGGPU_PLATFORM_APP_BASE_H

#include <glfw/glfw3.h>
#include <webgpu/webgpu_cpp.h>

#include <memory>
#include <string>
#include <variant>

#ifdef __EMSCRIPTEN__
#include <igasync/promise.h>
#else
#include <dawn/native/DawnNative.h>
#endif

namespace iggpu {

enum class AppBaseCreateError {
  GLFWInitError,
  WindowCreationError,
  WGPUDeviceCreationFailed,
  WGPUNoSuitableAdapters,
  WGPUSwapChainCreateFailed,
};

struct AppBase {
 public:
  AppBase() = delete;
  AppBase(const AppBase&) = delete;
  AppBase& operator=(const AppBase&) = delete;
  AppBase(AppBase&&) = default;
  AppBase& operator=(AppBase&&) = default;

  AppBase(GLFWwindow* window, wgpu::Device device, wgpu::Surface surface,
          wgpu::Queue queue, wgpu::SwapChain swapChain, uint32_t width,
          uint32_t height);
  ~AppBase();

#ifdef __EMSCRIPTEN__
  using AppBaseCreateRsl = std::shared_ptr<igasync::Promise<
      std::variant<std::unique_ptr<AppBase>, AppBaseCreateError>>>;
  static AppBaseCreateRsl Create(std::string canvas_name,
                                 bool prefer_high_power = true);
#else
  using AppBaseCreateRsl =
      std::variant<std::unique_ptr<AppBase>, AppBaseCreateError>;
  static AppBaseCreateRsl Create(uint32_t width = 0u, uint32_t height = 0u,
                                 const char* window_title = "IGGPU App");

 private:
  std::unique_ptr<dawn::native::Instance> instance_;

 public:
  void process_events();
#endif
  wgpu::TextureFormat preferred_swap_chain_texture_format() const;
  void resize_swap_chain(uint32_t width, uint32_t height);

 public:
  GLFWwindow* Window;
  wgpu::Instance Instance;
  wgpu::Device Device;
  wgpu::Surface Surface;
  wgpu::Queue Queue;
  wgpu::SwapChain SwapChain;
  uint32_t Width;
  uint32_t Height;
};

inline constexpr std::string app_base_create_error_text(
    AppBaseCreateError err) {
  switch (err) {
    case AppBaseCreateError::GLFWInitError:
      return "GLFWInitializationError";
    case AppBaseCreateError::WindowCreationError:
      return "WindowCreationError";
    case AppBaseCreateError::WGPUDeviceCreationFailed:
      return "WGPUDeviceCreationFailed";
    case AppBaseCreateError::WGPUNoSuitableAdapters:
      return "WGPUNoSuitableAdapters";
    case AppBaseCreateError::WGPUSwapChainCreateFailed:
      return "WGPUSwapChainCreateFailed";
    default:
      return "UNKNOWN";
  }
}

}  // namespace iggpu

#endif
