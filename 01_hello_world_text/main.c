#include <mach.h>

config mach(){
  return (config) {
    .resources = {{
      {"home", "/", mime_txt,
        .get = {{
          validate((v){
            .name = "name",
            .validation = "^\\S{1,16}$",
            .fallback = "world",
            .message = "must be 1-16 characters, no spaces"
          }),
          render((r){"Hello {{name}}"})
        }}
      }
    }}
  };
}
