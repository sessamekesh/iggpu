#include "simple_triangle_app.h"

#include <sstream>

namespace {

static const char shaderCode[] = R"SMS(
@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4<f32> {
  var pos = array<vec2<f32>, 3>(vec2<f32>(0.0, 0.5), vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5));
  return vec4<f32>(pos[idx], 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return vec4<f32>(0.294, 0.0, 0.51, 1.0);
}
)SMS";

}

namespace iggpu::sample {

bool SimpleTriangleApp::load_app() {
  wgpu::Device device = app_base_->Device;

  wgpu::ShaderModule shaderModule{};
  {
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor desc{};
    desc.nextInChain = &wgslDesc;
    shaderModule = device.CreateShaderModule(&desc);
  }

  {
    wgpu::PipelineLayoutDescriptor pl{};
    pl.bindGroupLayoutCount = 0;
    pl.bindGroupLayouts = nullptr;

    wgpu::ColorTargetState colorTargetState{};
    colorTargetState.format = wgpu::TextureFormat::BGRA8Unorm;

    wgpu::FragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    wgpu::DepthStencilState dss{};
    dss.format = wgpu::TextureFormat::Depth24PlusStencil8;
    dss.depthCompare = wgpu::CompareFunction::Always;

    wgpu::RenderPipelineDescriptor rpd{};
    rpd.layout = device.CreatePipelineLayout(&pl);
    rpd.vertex.module = shaderModule;
    rpd.vertex.entryPoint = "vs_main";
    rpd.fragment = &fragmentState;
    rpd.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rpd.depthStencil = &dss;

    {
      wgpu::TextureDescriptor td{};
      td.dimension = wgpu::TextureDimension::e2D;
      td.size.width = app_base_->Width;
      td.size.height = app_base_->Height;
      td.size.depthOrArrayLayers = 1;
      td.sampleCount = 1;
      td.format = wgpu::TextureFormat::Depth24PlusStencil8;
      td.mipLevelCount = 1;
      td.usage = wgpu::TextureUsage::RenderAttachment;
      depth_stencil_ = device.CreateTexture(&td);
      if (!depth_stencil_) {
        return false;
      }
    }

    {
      depth_stencil_view_ = depth_stencil_.CreateView();
      if (!depth_stencil_view_) {
        return false;
      }
    }

    render_pipeline_ = device.CreateRenderPipeline(&rpd);
    if (!render_pipeline_) {
      return false;
    }

    return true;
  }
}

void SimpleTriangleApp::render() {
  if (!render_pipeline_ || !depth_stencil_view_) return;

  wgpu::Device device = app_base_->Device;

  wgpu::TextureView backbufferView =
      app_base_->SwapChain.GetCurrentTextureView();

  wgpu::RenderPassColorAttachment colorAttachment{};
  colorAttachment.clearValue = {0.f, 0.f, 0.f, 1.f};
  colorAttachment.loadOp = wgpu::LoadOp::Clear;
  colorAttachment.storeOp = wgpu::StoreOp::Store;
  colorAttachment.view = backbufferView;

  wgpu::RenderPassDepthStencilAttachment dsa{};
  dsa.view = depth_stencil_view_;
  dsa.depthClearValue = 0;
  dsa.depthLoadOp = wgpu::LoadOp::Clear;
  dsa.depthStoreOp = wgpu::StoreOp::Store;
  dsa.stencilLoadOp = wgpu::LoadOp::Clear;
  dsa.stencilStoreOp = wgpu::StoreOp::Discard;

  wgpu::RenderPassDescriptor rpd{};
  rpd.colorAttachmentCount = 1;
  rpd.colorAttachments = &colorAttachment;
  rpd.depthStencilAttachment = &dsa;

  wgpu::CommandBuffer commands;
  {
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
      wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rpd);
      pass.SetPipeline(render_pipeline_);
      pass.Draw(3);
      pass.End();
    }
    commands = encoder.Finish();
  }

  app_base_->Queue.Submit(1, &commands);
}

}  // namespace iggpu::sample