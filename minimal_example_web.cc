#include <emscripten.h>
#include <iggpu/app_base.h>

#include <iostream>

std::unique_ptr<iggpu::AppBase> gApp;

void main_loop() {}

int main(int, char**) {
  iggpu::AppBase::Create("#canvas")->consume([](auto app_create_rsl) {
    if (std::holds_alternative<iggpu::AppBaseCreateError>(app_create_rsl)) {
      std::cerr << "Failed to create app: "
                << iggpu::app_base_create_error_text(
                       std::get<iggpu::AppBaseCreateError>(app_create_rsl));
      exit(-1);
    }

    gApp = std::move(std::get<std::unique_ptr<iggpu::AppBase>>(app_create_rsl));

    emscripten_set_main_loop(main_loop, 0, 1);

    std::cout << "Success!" << std::endl;
  });
}
