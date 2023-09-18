#include <iggpu/app_base.h>

#include <iostream>

int main() {
  auto app_create_rsl = iggpu::AppBase::Create();

  if (std::holds_alternative<iggpu::AppBaseCreateError>(app_create_rsl)) {
    std::cerr << "Failed to create app: "
              << iggpu::app_base_create_error_text(
                     std::get<iggpu::AppBaseCreateError>(app_create_rsl))
              << std::endl;
    return -1;
  }

  std::unique_ptr<iggpu::AppBase> app =
      std::move(std::get<std::unique_ptr<iggpu::AppBase>>(app_create_rsl));

  while (!glfwWindowShouldClose(app->Window)) {
    app->process_events();
    glfwPollEvents();
  }

  return 0;
}