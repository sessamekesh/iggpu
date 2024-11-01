#include <iggpu/app_base.h>
#include <iggpu/log.h>

#include <cstdio>
#include <sstream>

/**
 * Make sure Emscripten is up to date enough to support WebGPU
 */
#if __EMSCRIPTEN_major__ == 1 &&  \
    (__EMSCRIPTEN_minor__ < 40 || \
     (__EMSCRIPTEN_minor__ == 40 && __EMSCRIPTEN_tiny__ < 1))
#error "Emscripten 1.40.1 or higher required"
#endif
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

namespace {

void glfw_error(int code, const char* msg) {
  char buff[256];
  std::sprintf(buff, "[IGGPU] GLFW error %d: %s", code, msg);
  iggpu::log(iggpu::LogLevel::Error, buff);
}

wgpu::TextureFormat kDefaultPreferredTextureFormat =
    wgpu::TextureFormat::BGRA8Unorm;

}  // namespace

namespace iggpu {

AppBase::AppBase(GLFWwindow* window, wgpu::Device device, wgpu::Adapter adapter,
                 wgpu::Surface surface, wgpu::TextureFormat surface_format,
                 wgpu::Queue queue, uint32_t width, uint32_t height)
    : Window(window),
      Adapter(adapter),
      Device(device),
      Surface(surface),
      SurfaceFormat(surface_format),
      Queue(queue),
      Width(width),
      Height(height) {}

AppBase::~AppBase() {
  if (Window != nullptr) {
    glfwDestroyWindow(Window);
    Window = nullptr;
  }

  glfwTerminate();
}

AppBase::AppBaseCreateRsl AppBase::Create(std::string canvas_name,
                                          wgpu::TextureFormat preferred_format,
                                          bool prefer_high_power) {
  using promise_t = std::variant<std::unique_ptr<AppBase>, AppBaseCreateError>;

  glfwSetErrorCallback(::glfw_error);
  if (!glfwInit()) {
    return igasync::Promise<promise_t>::Immediate(
        AppBaseCreateError::GLFWInitError);
  }

  int width, height;
  auto rsl =
      emscripten_get_canvas_element_size(canvas_name.c_str(), &width, &height);
  if (rsl != EMSCRIPTEN_RESULT_SUCCESS) {
    return igasync::Promise<promise_t>::Immediate(
        AppBaseCreateError::WindowCreationError);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto window = glfwCreateWindow(width, height, "IGGPU App", nullptr, nullptr);

  const wgpu::Instance instance = wgpu::CreateInstance();
  auto result_promise = igasync::Promise<promise_t>::Create();

  wgpu::RequestAdapterOptions adapter_options = {};
  adapter_options.powerPreference = prefer_high_power
                                        ? wgpu::PowerPreference::HighPerformance
                                        : wgpu::PowerPreference::LowPower;

  struct RequestAdapterUserData {
    GLFWwindow* window;
    std::shared_ptr<igasync::Promise<promise_t>> result_promise;
    std::string canvas_name;
    int width;
    int height;
    wgpu::Instance instance;
    wgpu::TextureFormat preferred_format;
  };
  RequestAdapterUserData* request_adapter_user_data =
      new RequestAdapterUserData{
          window, result_promise, std::move(canvas_name), width,
          height, instance,       preferred_format,
      };

  instance.RequestAdapter(
      &adapter_options,
      [](WGPURequestAdapterStatus status, WGPUAdapter raw_adapter,
         const char* msg, void* user_data) -> void {
        RequestAdapterUserData* ud =
            reinterpret_cast<RequestAdapterUserData*>(user_data);

        if (msg) {
          std::stringstream ss;
          ss << "[iggpu] Request adapter message: " << msg << std::endl;
          iggpu::log(LogLevel::Warning, ss.str());
        }

        if (status == WGPURequestAdapterStatus_Unavailable) {
          iggpu::log(LogLevel::Error, "[iggpu] WebGPU unavailable, exiting");
          glfwTerminate();
          ud->result_promise->resolve(
              AppBaseCreateError::WGPUNoSuitableAdapters);
          return;
        }

        if (status != WGPURequestAdapterStatus_Success) {
          iggpu::log(LogLevel::Error,
                     "[iggpu] Could not create WebGPU adapter, exiting");
          glfwTerminate();
          ud->result_promise->resolve(
              AppBaseCreateError::WGPUNoSuitableAdapters);
          return;
        }

        wgpu::Adapter adapter = wgpu::Adapter::Acquire(raw_adapter);

        struct RequestDeviceUserData {
          GLFWwindow* window;
          std::shared_ptr<igasync::Promise<promise_t>> result_promise;
          std::string canvas_name;
          int width;
          int height;
          wgpu::Instance instance;
          wgpu::Adapter adapter;
          wgpu::TextureFormat preferred_format;
        };
        RequestDeviceUserData* request_device_user_data =
            new RequestDeviceUserData{
                ud->window, ud->result_promise,   std::move(ud->canvas_name),
                ud->width,  ud->height,           ud->instance,
                adapter,    ud->preferred_format,
            };
        delete ud;

        adapter.RequestDevice(
            nullptr,
            [](WGPURequestDeviceStatus status, WGPUDevice raw_device,
               const char* msg, void* user_data) -> void {
              RequestDeviceUserData* ud =
                  reinterpret_cast<RequestDeviceUserData*>(user_data);

              if (msg) {
                std::stringstream ss;
                ss << "[iggpu] Request device message: " << msg << std::endl;
                iggpu::log(LogLevel::Error, ss.str());
              }

              if (status != WGPURequestDeviceStatus_Success) {
                std::stringstream ss;
                ss << "[iggpu] Failed to request device." << std::endl;
                iggpu::log(LogLevel::Error, ss.str());
                glfwTerminate();
                ud->result_promise->resolve(
                    AppBaseCreateError::WGPUDeviceCreationFailed);
                return;
              }

              wgpu::Device device = wgpu::Device::Acquire(raw_device);

              wgpu::SurfaceDescriptorFromCanvasHTMLSelector canv_desc = {};
              canv_desc.selector = ud->canvas_name.c_str();

              wgpu::SurfaceDescriptor surface_desc = {};
              surface_desc.nextInChain =
                  reinterpret_cast<wgpu::ChainedStruct*>(&canv_desc);
              wgpu::Surface surface = ud->instance.CreateSurface(&surface_desc);

              // Configure the surface
              wgpu::SurfaceCapabilities surfaceCaps{};
              surface.GetCapabilities(ud->adapter.Get(), &surfaceCaps);

              wgpu::TextureFormat surfaceFormat = surfaceCaps.formats[0];
              for (size_t i = 1; i < surfaceCaps.formatCount; i++) {
                if (surfaceCaps.formats[i] == ud->preferred_format) {
                  surfaceFormat = surfaceCaps.formats[i];
                }
              }

              wgpu::SurfaceConfiguration surfaceConfig = {};
              surfaceConfig.device = device;
              surfaceConfig.format = surfaceFormat;
              surfaceConfig.width = ud->width;
              surfaceConfig.height = ud->height;
              surface.Configure(&surfaceConfig);

              wgpu::Queue queue = device.GetQueue();

              ud->result_promise->resolve(std::make_unique<AppBase>(
                  ud->window, device, ud->adapter, surface, surfaceFormat,
                  queue, ud->width, ud->height));

              delete ud;
            },
            request_device_user_data);
      },
      request_adapter_user_data);

  return result_promise;
}

void AppBase::resize_surface(uint32_t width, uint32_t height) {
  wgpu::SurfaceCapabilities surfaceCaps{};
  Surface.GetCapabilities(Adapter.Get(), &surfaceCaps);
  wgpu::SurfaceConfiguration surfaceConfig = {};
  surfaceConfig.device = Device;
  surfaceConfig.format = SurfaceFormat;
  surfaceConfig.width = width;
  surfaceConfig.height = height;
  Surface.Configure(&surfaceConfig);
}

}  // namespace iggpu
