#include <mach.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  auto const msg = sfmt("Hello %s", name);
  res_async(mime_txt, http_ok, msg);
}

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .call = hello}
    }
  };
}
