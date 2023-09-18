#include <emscripten.h>
#include <iggpu/app_base.h>

#include <iostream>

#include "simple_triangle_app.h"

std::unique_ptr<iggpu::AppBase> gAppBase;
std::unique_ptr<iggpu::sample::SimpleTriangleApp> gApp;

void main_loop() { gApp->render(); }

int main(int, char**) {
  iggpu::AppBase::Create("#canvas")->consume([](auto app_create_rsl) {
    if (std::holds_alternative<iggpu::AppBaseCreateError>(app_create_rsl)) {
      std::cerr << "Failed to create app: "
                << iggpu::app_base_create_error_text(
                       std::get<iggpu::AppBaseCreateError>(app_create_rsl));
      exit(-1);
    }

    gAppBase =
        std::move(std::get<std::unique_ptr<iggpu::AppBase>>(app_create_rsl));

    gApp = std::make_unique<iggpu::sample::SimpleTriangleApp>(gAppBase.get());
    if (!gApp->load_app()) {
      std::cerr << "Failed to load triangle app - see console for more info"
                << std::endl;
      exit(-1);
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    std::cout << "Success!" << std::endl;
  });

  return 0;
}