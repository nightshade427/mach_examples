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
      {"/", http_get, "home", .render = "home"},
      {"/about", http_get, "about", .render = "about"},
      {"/contact", http_get, "contact", .render = "contact"},
    },
    .resources = {
      {"layout", (resource){
        #embed "static/layout.mustache.html"
      }},
      {"404", (resource){
        #embed "static/404.mustache.html"
      }},
      {"5xx", (resource){
        #embed "static/5xx.mustache.html"
      }},
      {"home", (resource){
        #embed "static/home.mustache.html"
      }},
      {"about", (resource){
        #embed "static/about.mustache.html"
      }},
      {"contact", (resource){
        #embed "static/contact.mustache.html"
      }},
    }
  };
}
