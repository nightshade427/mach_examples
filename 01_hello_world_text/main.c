#include <mach.h>

config mach(){
  return (config) {

    .resources = {
      .urls = {
        {"/", mime_txt,
          .get = {{
            param((p){
              .name = "name",
              .validation = "^\\S{1,16}$",
              .fallback = "world",
              .message = "must be 1-16 characters, no spaces"
            }),
            render((r){"Hello {{name}}"})
          }}
        }
      }
    }

};}
