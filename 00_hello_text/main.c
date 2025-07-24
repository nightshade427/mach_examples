#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/", .mime = m_txt,
        .get = {render(.template= "hello")}
      }
    }
  };
}
