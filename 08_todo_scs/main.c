#include <mach.h>
#include <mustache.h>
#include <session_auth.h>
#include "todos/todos.c"

config mach(){
  return (config) {
    .uses = {"session"},
    .includes = {
      mustache_config,
      todos_config,
      session_auth_config
    },
    .routes = {
      {.status = http_error, .render = "5xx"},
      {.status = http_not_found, .render = "404"},
      {http_get, "/", "home", .render = "home"},
      {http_get, "/about", "about", .render = "about"},
      {http_get, "/contact", "contact", .render = "contact"}
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"layout", (string_embed){
          #embed "static/layout.mustache.html"
        }},
        {"404", (string_embed){
          #embed "static/404.mustache.html"
        }},
        {"5xx", (string_embed){
          #embed "static/5xx.mustache.html"
        }},
        {"home", (string_embed){
          #embed "static/home.mustache.html"
        }},
        {"about", (string_embed){
          #embed "static/about.mustache.html"
        }},
        {"contact", (string_embed){
          #embed "static/contact.mustache.html"
        }}
      }
    }}
  };
}
