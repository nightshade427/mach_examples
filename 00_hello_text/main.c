#include <mach.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/", .mime = mime_txt,
        .get = {
          render(.template= "hello")
        }
      }
    }
  };
}
