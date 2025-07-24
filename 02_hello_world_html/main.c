#include <mach.h>
#include <mustache.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  assign("name", name);
  respond(http_ok, mime_html, (b){.engine = mustache,
    "<html>"
      "<head></head>"
      "<body>"
        "<p>Hello {{name}}</p>"
      "</body>"
    "</html>"
  });
}

config mach(){
  return (config) {
    .includes = {mustache_config},
    .routes = {{"/", http_get, .call = hello}}
  };
}
