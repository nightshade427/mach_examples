#include <mach.h>
#include <session_auth.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config) {
    .name = "app",

    .includes = {
      todos_config,
      activity_config,
      session_auth_config
    },

    .resources = {
      .before = {session()},

      .context = {
        {"layout", (asset){
          #embed "static/layout.mustache.html"
        }}
      },

      .errors = {
        {http_error, {
          render((r){(asset){
            #embed "static/5xx.mustache.html"
          }})
        }},

        {http_not_found, {
          render((r){(asset){
            #embed "static/404.mustache.html"
          }})
        }}
      },

      .urls = {
        {"/",
          .get = {{
            render((r){(asset){
              #embed "static/home.mustache.html"
            }})
          }}
        }
      }
    }

};}
