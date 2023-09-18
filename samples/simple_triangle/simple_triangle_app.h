#ifndef IGGPU_SAMPLES_SIMPLE_TRIANGLE_APP_H
#define IGGPU_SAMPLES_SIMPLE_TRIANGLE_APP_H

#include <igasync/promise.h>
#include <iggpu/app_base.h>

namespace iggpu::sample {

class SimpleTriangleApp {
 public:
  SimpleTriangleApp(AppBase* app_base)
      : app_base_(app_base), render_pipeline_(nullptr) {}

  bool load_app();
  void render();

 private:
  AppBase* app_base_;

  wgpu::RenderPipeline render_pipeline_;
  wgpu::Texture depth_stencil_;
  wgpu::TextureView depth_stencil_view_;
};

}  // namespace iggpu::sample

#endif
