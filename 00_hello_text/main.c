#include <mach.h>

config mach(){
  return (config) {

    .resources = {
      .urls = {
        {"/", mime_txt,
          .get = {{
            render((r){"hello"})
          }}
        }
      }
    }

};}
