#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/", .mime = mime_txt,
        .get = {
          validate({"name",
            .validation = "^\\S{1,16}$",
            .fallback = "world",
            .message = "must be 1-16 characters, no spaces"
          }),
          render(.template = "Hello {{name}}")
        }
      }
    }
  };
}
