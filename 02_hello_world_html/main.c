#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          input({"name", .matches = m_not_empty, .fallback = "world"}),
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
