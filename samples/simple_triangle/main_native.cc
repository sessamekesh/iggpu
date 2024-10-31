#include <iostream>

#include "simple_triangle_app.h"

int main() {
  auto app_create_rsl = iggpu::AppBase::Create();

  if (std::holds_alternative<iggpu::AppBaseCreateError>(app_create_rsl)) {
    std::cerr << "Failed to create app: "
              << iggpu::app_base_create_error_text(
                     std::get<iggpu::AppBaseCreateError>(app_create_rsl))
              << std::endl;
    return -1;
  }

  std::unique_ptr<iggpu::AppBase> app_base =
      std::move(std::get<std::unique_ptr<iggpu::AppBase>>(app_create_rsl));

  iggpu::sample::SimpleTriangleApp app(app_base.get());
  if (!app.load_app()) {
    std::cerr << "Failed to load app - see console for more info" << std::endl;
    return -1;
  }

  std::cout << "Successfully loaded app - a triangle should be rendering now"
            << std::endl;

  while (!glfwWindowShouldClose(app_base->Window)) {
    app_base->process_events();
    app.render();
    app_base->Surface.Present();

    glfwPollEvents();
  }

  return 0;
}
