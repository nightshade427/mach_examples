#include <mach.h>
#include <mustache.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  set("name", name);
  render_res_async(http_ok, "hello_world");
}

config mach(){
  return (config) {
    .includes = {mustache_config},
    .routes = {
      {http_get, "/", .call = hello}
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"hello_world",
            "<html>"
              "<head></head>"
              "<body>"
                "<p>Hello {{name}}</p>"
              "</body>"
            "</html>"
        }
      }
    }}
  };
}
