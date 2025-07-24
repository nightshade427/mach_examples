#include <mach.h>
#include <session_auth.h>
#include "todos/todos.c"
#include "activity/activity.c"

config mach(){
  return (config) {"main", {session()},
    .resources = {
      {"home", "/",
        .get = {render("home")}
      }
    },

    .errors = {
      {m_error, {render("5xx")}},
      {m_not_found, {render("404")}}
    },

    .context = {
      {"layout", (asset){
        #embed "static/layout.mustache.html"
      }},
      {"home", (asset){
        #embed "static/home.mustache.html"
      }},
      {"5xx", (asset){
        #embed "static/5xx.mustache.html"
      }},
      {"404", (asset){
        #embed "static/404.mustache.html"
      }}
    },

    .modules = {todos, activity, session_auth}
  };
}
