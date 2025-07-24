#include <mach.h>

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .text = "hello"}
    }
  };
}
