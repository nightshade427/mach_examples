#include <mach.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  assign("name", name);
  respond(http_ok, mime_txt, (b){.txt = format("Hello {{name}}")});
}

config mach(){
  return (config) {.routes = {{"/", http_get, .call = hello}}};
}
