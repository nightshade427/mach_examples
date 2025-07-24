#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          validate({"name",
            .validation = "^\\S{1,16}$",
            .fallback = "world",
            .message = "must be 1-16 characters, no spaces"
          }),
          render(.template =
            "<html>"
              "<head></head>"
              "<body>"
                "<p>Hello {{name}}</p>"
              "</body>"
            "</html>"
          )
        }
      }
    }
  };
}
