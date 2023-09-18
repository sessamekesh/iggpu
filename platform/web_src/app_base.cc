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

AppBase::AppBase(GLFWwindow* window, wgpu::Device device, wgpu::Surface surface,
                 wgpu::Queue queue, wgpu::SwapChain swapChain, uint32_t width,
                 uint32_t height)
    : Window(window),
      Device(device),
      Surface(surface),
      Queue(queue),
      SwapChain(swapChain),
      Width(width),
      Height(height) {}

AppBase::~AppBase() {
  if (Window != nullptr) {
    glfwDestroyWindow(Window);
    Window = nullptr;
  }

  glfwTerminate();
}

AppBase::AppBaseCreateRsl AppBase::Create(const char* canvas_name,
                                          bool prefer_high_power) {
  using promise_t = std::variant<std::unique_ptr<AppBase>, AppBaseCreateError>;

  glfwSetErrorCallback(::glfw_error);
  if (!glfwInit()) {
    return igasync::Promise<promise_t>::Immediate(
        AppBaseCreateError::GLFWInitError);
  }

  int width, height;
  auto rsl = emscripten_get_canvas_element_size(canvas_name, &width, &height);
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
    const char* canvas_name;
    int width;
    int height;
    wgpu::Instance instance;
  };
  RequestAdapterUserData* request_adapter_user_data =
      new RequestAdapterUserData{
          window, result_promise, canvas_name, width, height, instance,
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
          const char* canvas_name;
          int width;
          int height;
          wgpu::Instance instance;
          wgpu::Adapter adapter;
        };
        RequestDeviceUserData* request_device_user_data =
            new RequestDeviceUserData{
                ud->window, ud->result_promise, ud->canvas_name, ud->width,
                ud->height, ud->instance,       adapter,
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
              canv_desc.selector = ud->canvas_name;

              wgpu::SurfaceDescriptor surface_desc = {};
              surface_desc.nextInChain =
                  reinterpret_cast<wgpu::ChainedStruct*>(&canv_desc);
              wgpu::Surface surface = ud->instance.CreateSurface(&surface_desc);

              wgpu::SwapChainDescriptor swap_desc{};
              swap_desc.usage = wgpu::TextureUsage::RenderAttachment;
              swap_desc.format = wgpu::TextureFormat::BGRA8Unorm;
              swap_desc.width = ud->width;
              swap_desc.height = ud->height;
              swap_desc.presentMode = wgpu::PresentMode::Fifo;
              wgpu::SwapChain swap_chain =
                  device.CreateSwapChain(surface, &swap_desc);

              wgpu::Queue queue = device.GetQueue();

              if (!swap_chain) {
                glfwTerminate();
                ud->result_promise->resolve(
                    AppBaseCreateError::WGPUSwapChainCreateFailed);
                return;
              }

              ud->result_promise->resolve(
                  std::make_unique<AppBase>(ud->window, device, surface, queue,
                                            swap_chain, ud->width, ud->height));

              delete ud;
            },
            request_device_user_data);
      },
      request_adapter_user_data);

  return result_promise;
}

wgpu::TextureFormat AppBase::preferred_swap_chain_texture_format() const {
  return ::kDefaultPreferredTextureFormat;
}

void AppBase::resize_swap_chain(uint32_t width, uint32_t height) {
  // SwapChain.Configure(preferred_swap_chain_texture_format(),
  //                    wgpu::TextureUsage::RenderAttachment, width, height);
  wgpu::SwapChainDescriptor swap_desc{};
  swap_desc.usage = wgpu::TextureUsage::RenderAttachment;
  swap_desc.format = wgpu::TextureFormat::BGRA8Unorm;
  swap_desc.width = width;
  swap_desc.height = height;
  swap_desc.presentMode = wgpu::PresentMode::Mailbox;

  SwapChain = Device.CreateSwapChain(Surface, &swap_desc);

  Width = width;
  Height = height;

  if (SwapChain == nullptr) {
    iggpu::log(LogLevel::Error, "[IGGPU] Failed to recreate swap chain");
  }
}

}  // namespace iggpu
