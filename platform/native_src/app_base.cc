#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <iggpu/app_base.h>
#include <iggpu/iggpu_config.h>
#include <iggpu/log.h>
#include <webgpu/webgpu_glfw.h>

#include <format>

namespace {

void print_wgpu_device_error(WGPUErrorType error_type, WGPUStringView message,
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

  iggpu::log(iggpu::LogLevel::Error,
             std::format("[IGGPU] Dawn error - {} - {}", error_type_name,
                         std::string_view(message.data, message.length)));
}

void device_lost_callback(WGPUDevice const* device, WGPUDeviceLostReason reason,
                          WGPUStringView msg, void*) {
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
      std::format("[IGGPU] Dawn device lost - {} - {}", lost_reason,
                  std::string_view(msg.data, msg.length));
  iggpu::log(iggpu::LogLevel::Error, formatted_string);
}

void glfw_error(int code, const char* msg) {
  iggpu::log(iggpu::LogLevel::Error,
             std::format("[IGGPU] GLFW error {}: {}", code, msg));
}

void device_log_callback(WGPULoggingType type, WGPUStringView msg, void*) {
  switch (type) {
    case WGPULoggingType_Error:
      iggpu::log(iggpu::LogLevel::Error,
                 std::format("[IGGPU/Dawn] {}",
                             std::string_view(msg.data, msg.length)));
      return;
    case WGPULoggingType_Warning:
      iggpu::log(iggpu::LogLevel::Warning,
                 std::format("[IGGPU/Dawn] {}",
                             std::string_view(msg.data, msg.length)));
      return;
    case WGPULoggingType_Info:
      iggpu::log(iggpu::LogLevel::Info,
                 std::format("[IGGPU/Dawn] {}",
                             std::string_view(msg.data, msg.length)));
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
#ifdef __APPLE__
    wgpu::BackendType::Metal,
#endif
      wgpu::BackendType::Vulkan,
  };

  // Pass 1: Get a discrete GPU if available
  for (int preferred_type_idx = 0;
       preferred_type_idx < std::size(adapter_type_order);
       preferred_type_idx++) {
    for (int i = 0; i < adapters.size(); i++) {
      const auto& adapter = adapters[i];
      wgpu::AdapterInfo adapterInfo{};
      adapter.GetInfo(&adapterInfo);

      for (int backend_type_idx = 0;
           backend_type_idx < std::size(backend_type_order);
           backend_type_idx++) {
        if (adapterInfo.backendType == backend_type_order[backend_type_idx] &&
            adapterInfo.adapterType == adapter_type_order[preferred_type_idx]) {
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

AppBase::AppBaseCreateRsl AppBase::Create(uint32_t width, uint32_t height,
                                          wgpu::TextureFormat preferred_format,
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

  wgpu::DeviceDescriptor device_desc = {};
  device_desc.nextInChain =
      reinterpret_cast<wgpu::ChainedStruct*>(&feature_toggles);
  device_desc.deviceLostCallbackInfo.mode =
      wgpu::CallbackMode::AllowSpontaneous;
  device_desc.deviceLostCallbackInfo.callback = ::device_lost_callback;
  device_desc.deviceLostCallbackInfo.userdata = nullptr;

  device_desc.uncapturedErrorCallbackInfo.callback = ::print_wgpu_device_error;
  device_desc.uncapturedErrorCallbackInfo.userdata = nullptr;

  WGPUDevice raw_device = adapter.CreateDevice(&device_desc);
  if (!raw_device) {
    glfwTerminate();
    return AppBaseCreateError::WGPUDeviceCreationFailed;
  }

  wgpu::Device device = wgpu::Device::Acquire(raw_device);
  device.SetLoggingCallback(::device_log_callback, nullptr);

  // Queue (easy)
  wgpu::Queue queue = device.GetQueue();

  // Surface creation (replaces old swap chain creation flow)
  wgpu::Surface surface =
      wgpu::glfw::CreateSurfaceForWindow(instance->Get(), window);
  if (!surface) {
    glfwTerminate();
    return AppBaseCreateError::WGPUSurfaceCreateFailed;
  }

  // Configure the surface
  wgpu::SurfaceCapabilities surfaceCaps{};
  surface.GetCapabilities(adapter.Get(), &surfaceCaps);

  wgpu::TextureFormat surfaceFormat = surfaceCaps.formats[0];
  for (size_t i = 1; i < surfaceCaps.formatCount; i++) {
    if (surfaceCaps.formats[i] == preferred_format) {
      surfaceFormat = surfaceCaps.formats[i];
    }
  }

  wgpu::SurfaceConfiguration surfaceConfig = {};
  surfaceConfig.device = device;
  surfaceConfig.format = surfaceFormat;
  surfaceConfig.width = width;
  surfaceConfig.height = height;
  surface.Configure(&surfaceConfig);

  auto rsl = std::make_unique<AppBase>(
      window, device, wgpu::Adapter::Acquire(adapter.Get()), surface,
      surfaceFormat, queue, width, height);
  rsl->instance_ = std::move(instance);
  return std::move(rsl);
}

void AppBase::resize_surface(uint32_t width, uint32_t height) {
  auto surfaceChainedDesc =
      wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(Window);

  WGPUSurfaceDescriptor desc = {};
  desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceChainedDesc);

  wgpu::SurfaceCapabilities surfaceCaps{};
  Surface.GetCapabilities(Adapter.Get(), &surfaceCaps);
  wgpu::SurfaceConfiguration surfaceConfig = {};
  surfaceConfig.device = Device;
  surfaceConfig.format = SurfaceFormat;
  surfaceConfig.width = width;
  surfaceConfig.height = height;
  Surface.Configure(&surfaceConfig);
}

void AppBase::process_events() {
  dawn::native::InstanceProcessEvents(instance_->Get());
}

}  // namespace iggpu