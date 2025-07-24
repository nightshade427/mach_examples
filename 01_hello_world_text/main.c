#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/", .mime = m_txt,
        .get = {
          input({"name", .matches = m_not_empty, .fallback = "world"}),
          render(.template = "Hello {{name}}")
        }
      }
    }
  };
}
