#include <mach.h>
#include <htmx.h>
#include "session/session.c"
#include "todos/todos.c"
#define m_layout "layout"
#define m_404 "404"
#define m_5xx "5xx"
#define m_root "root"
#define m_about "about"
#define m_contact "contact"
#define m_session "session"

ma_config mach(){
  return (ma_config) {
    .use = {m_session},
    .includes = {todos_config, session_config},
    .templates = {
      {m_layout, (ma_embed){
        #embed "static/layout.mustache.html"
      }},
      {m_404, (ma_embed){
        #embed "static/404.mustache.html"
      }},
      {m_5xx, (ma_embed){
        #embed "static/5xx.mustache.html"
      }},
      {m_root, (ma_embed){
        #embed "static/root.mustache.html"
      }},
      {m_about, (ma_embed){
        #embed "static/about.mustache.html"
      }},
      {m_contact, (ma_embed){
        #embed "static/contact.mustache.html"
      }}
    },
    .routes = {
      {.status = ma_error, .render = m_5xx},
      {.status = ma_not_found, .render = m_404},
      {ma_get, "/", m_root, .render = m_root},
      {ma_get, "/about", m_about, .render = m_about},
      {ma_get, "/contact", m_contact, .render = m_contact}
    }
  };
}
