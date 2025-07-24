#include <mach.h>

ma_config mach(){
  return (ma_config) {.routes = {{ma_get, "/", .text = "hello"}}};
}
