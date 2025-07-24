#include <mach.h>

config mach(){
  return (config) {
    .resources = {{
      {"home", "/", mime_txt,
        .get = {{
          render((r){"hello", .mime = mime_txt})
        }}
      }
    }}
  };
}
