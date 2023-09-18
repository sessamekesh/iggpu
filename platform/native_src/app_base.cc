#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <iggpu/app_base.h>
#include <iggpu/iggpu_config.h>
#include <iggpu/log.h>
#include <webgpu/webgpu_glfw.h>

#include <format>

namespace {

void print_wgpu_device_error(WGPUErrorType error_type, const char* message,
                             void*) {
  const char* error_type_name = "";
  switch (error_type) {
    case WGPUErrorType_Validation:
      error_type_name = "Validation";
      break;
    case WGPUErrorType_OutOfMemory:
      error_type_name = "OutOfMemory";
      break;
    case WGPUErrorType_DeviceLost:
      error_type_name = "DeviceLost";
      break;
    case WGPUErrorType_Unknown:
      error_type_name = "Unknown";
      break;
    default:
      error_type_name = "UNEXPECTED (this is bad)";
      break;
  }

  iggpu::log(iggpu::LogLevel::Error, std::format("[IGGPU] Dawn error - {} - {}",
                                                 error_type_name, message));
}

void device_lost_callback(WGPUDeviceLostReason reason, const char* msg, void*) {
  const char* lost_reason = "";
  switch (reason) {
    case WGPUDeviceLostReason_Destroyed:
      lost_reason = "WGPUDeviceLostReason_Destroyed";
      break;
    default:
      lost_reason = "WGPUDeviceLostReason_Unknown";
      break;
  }

  auto formatted_string =
      std::format("[IGGPU] Dawn device lost - {} - {}", lost_reason, msg);
  iggpu::log(iggpu::LogLevel::Error, formatted_string);
}

void glfw_error(int code, const char* msg) {
  iggpu::log(iggpu::LogLevel::Error,
             std::format("[IGGPU] GLFW error {}: {}", code, msg));
}

void device_log_callback(WGPULoggingType type, const char* msg, void*) {
  switch (type) {
    case WGPULoggingType_Error:
      iggpu::log(iggpu::LogLevel::Error, std::format("[IGGPU/Dawn] {}", msg));
      return;
    case WGPULoggingType_Warning:
      iggpu::log(iggpu::LogLevel::Warning, std::format("[IGGPU/Dawn] {}", msg));
      return;
    case WGPULoggingType_Info:
      iggpu::log(iggpu::LogLevel::Info, std::format("[IGGPU/Dawn] {}", msg));
      return;
    default:  // Do not log verbose messages (by default)
      return;
  }
}

dawn::native::Adapter get_adapter(
    const std::vector<dawn::native::Adapter>& adapters) {
  wgpu::AdapterType adapter_type_order[] = {
      wgpu::AdapterType::DiscreteGPU,
      wgpu::AdapterType::IntegratedGPU,
      wgpu::AdapterType::CPU,
  };

  wgpu::BackendType backend_type_order[] = {
#ifdef _WIN32
      // TODO (sessamekesh): Prefer D3D12 support macro directly to WIN32
      wgpu::BackendType::D3D12,
#endif
      wgpu::BackendType::Vulkan,
  };

  // Pass 1: Get a discrete GPU if available
  for (int preferred_type_idx = 0;
       preferred_type_idx < std::size(adapter_type_order);
       preferred_type_idx++) {
    for (int i = 0; i < adapters.size(); i++) {
      const auto& adapter = adapters[i];
      wgpu::AdapterProperties properties{};
      adapter.GetProperties(&properties);

      for (int backend_type_idx = 0;
           backend_type_idx < std::size(backend_type_order);
           backend_type_idx++) {
        if (properties.backendType == backend_type_order[backend_type_idx] &&
            properties.adapterType == adapter_type_order[preferred_type_idx]) {
          return adapter;
        }
      }
    }
  }

  // No suitable adapter found!
  return {};
}

// Later: Return the adapter's preferred format (see TODO(dawn:1362))
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

AppBase::AppBaseCreateRsl AppBase::Create(uint32_t width, uint32_t height,
                                          const char* window_title) {
  glfwSetErrorCallback(::glfw_error);
  if (!glfwInit()) {
    return AppBaseCreateError::GLFWInitError;
  }

  if (width == 0u || height == 0u) {
    int i_width = 0, i_height = 0;
    auto primary_monitor = glfwGetPrimaryMonitor();
    glfwGetMonitorWorkarea(primary_monitor, nullptr, nullptr, &i_width,
                           &i_height);

    if (i_width > 1980 && i_height >= 1080) {
      width = 1980u;
      height = 1080u;
    } else if (i_width >= 1280 && i_height >= 720) {
      width = 1280u;
      height = 720u;
    } else if (i_width >= 640 && i_height >= 480) {
      width = 640u;
      height = 480u;
    } else {
      width = 320u;
      height = 200u;
    }
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
  auto window = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return AppBaseCreateError::WindowCreationError;
  }

  DawnProcTable procs_table = dawn::native::GetProcs();
  dawnProcSetProcs(&procs_table);

  WGPUInstanceDescriptor instance_descriptor{};
  instance_descriptor.features.timedWaitAnyEnable = true;
  auto instance =
      std::make_unique<dawn::native::Instance>(&instance_descriptor);

  wgpu::RequestAdapterOptions options = {};
  options.powerPreference = wgpu::PowerPreference::HighPerformance;
  auto adapters = instance->EnumerateAdapters(&options);

  auto adapter = ::get_adapter(adapters);
  if (!adapter) {
    glfwTerminate();
    return AppBaseCreateError::WGPUNoSuitableAdapters;
  }

  // Feature toggles
  wgpu::DawnTogglesDescriptor feature_toggles{};
  std::vector<const char*> enabled_toggles;

  // Prevents accidental use of SPIR-V, since that isn't supported in
  //  web targets, allegedly because Apple is a piece of shit company that
  //  doesn't give a flying fuck about graphics developers.
  enabled_toggles.push_back("disallow_spirv");

#ifdef IGGPU_GRAPHICS_DEBUGGING
  enabled_toggles.push_back("emit_hlsl_debug_symbols");
  enabled_toggles.push_back("disable_symbol_renaming");
#endif

  feature_toggles.enabledToggleCount = enabled_toggles.size();
  feature_toggles.enabledToggles = &enabled_toggles[0];

  WGPUDeviceDescriptor device_desc = {};
  device_desc.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&feature_toggles);

  WGPUDevice raw_device = adapter.CreateDevice(&device_desc);
  if (!raw_device) {
    glfwTerminate();
    return AppBaseCreateError::WGPUDeviceCreationFailed;
  }

  procs_table.deviceSetUncapturedErrorCallback(
      raw_device, ::print_wgpu_device_error, nullptr);
  procs_table.deviceSetDeviceLostCallback(raw_device, ::device_lost_callback,
                                          nullptr);
  procs_table.deviceSetLoggingCallback(raw_device, ::device_log_callback,
                                       nullptr);

  wgpu::Device device = wgpu::Device::Acquire(raw_device);

  // Queue (easy)
  wgpu::Queue queue = device.GetQueue();

  // Surface and swap chain
  auto surfaceChainedDesc =
      wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(window);
  WGPUSurfaceDescriptor surface_desc = {};
  surface_desc.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get());
  WGPUSurface raw_surface =
      procs_table.instanceCreateSurface(instance->Get(), &surface_desc);
  if (!raw_surface) {
    glfwTerminate();
    return AppBaseCreateError::WGPUSwapChainCreateFailed;
  }
  wgpu::Surface surface = wgpu::Surface::Acquire(raw_surface);

  wgpu::SwapChainDescriptor swap_chain_desc = {};
  swap_chain_desc.usage = wgpu::TextureUsage::RenderAttachment;
  swap_chain_desc.format = ::kDefaultPreferredTextureFormat;
  swap_chain_desc.width = width;
  swap_chain_desc.height = height;
  swap_chain_desc.presentMode = wgpu::PresentMode::Mailbox;
  wgpu::SwapChain swap_chain =
      device.CreateSwapChain(surface, &swap_chain_desc);

  if (!swap_chain) {
    glfwTerminate();
    return AppBaseCreateError::WGPUSwapChainCreateFailed;
  }

  auto rsl = std::make_unique<AppBase>(window, device, surface, queue,
                                       swap_chain, width, height);
  rsl->instance_ = std::move(instance);
  return std::move(rsl);
}

wgpu::TextureFormat AppBase::preferred_swap_chain_texture_format() const {
  return ::kDefaultPreferredTextureFormat;
}

void AppBase::resize_swap_chain(uint32_t width, uint32_t height) {
  auto surfaceChainedDesc =
      wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(Window);

  WGPUSurfaceDescriptor desc = {};
  desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceChainedDesc);

  wgpu::SwapChainDescriptor swap_chain_desc = {};
  swap_chain_desc.usage = wgpu::TextureUsage::RenderAttachment;
  swap_chain_desc.format = ::kDefaultPreferredTextureFormat;
  swap_chain_desc.width = width;
  swap_chain_desc.height = height;
  swap_chain_desc.presentMode = wgpu::PresentMode::Mailbox;
  wgpu::SwapChain swap_chain =
      Device.CreateSwapChain(Surface, &swap_chain_desc);
  SwapChain = Device.CreateSwapChain(Surface, &swap_chain_desc);
  Width = width;
  Height = height;

  if (SwapChain == nullptr) {
    iggpu::log(LogLevel::Error, "Failed to recreate swap chain");
  }
}

void AppBase::process_events() {
  dawn::native::InstanceProcessEvents(instance_->Get());
}

}  // namespace iggpu