#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
#include <session_auth.h>

void list_todos(){
  await_assign_db_query("todos", (q){.db = "todos_db", .resource = "get_todos"});
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "todos"});
}

void create_todo(){
  db_query((q){.db = "todos_db", .resource = "create_todo"});
  redirect("todos");
}

void update_todo(){
  db_find((q){.db = "todos_db", .resource = "update_todo"});
  redirect("todos");
}

void delete_todo(){
  db_find((q){.db = "todos_db", .resource = "delete_todo"});
  redirect("todos");
}

config mach(){
  return (config) {
    .uses = {"session"},
    .includes = {
      mustache_config, 
      sqlite_config,
      session_auth_config
    },
    .databases = {{
      .engine = sqlite,
      .name = "todos_db",
      .connect = "file:todo.db?mode=rwc",
      .migrations = {
        "CREATE TABLE IF NOT EXISTS todos ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "user_id INTEGER NOT NULL, "
          "title TEXT NOT NULL, "
          "finished INTEGER CHECK(finished IN (1))"
        "); "
        "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);"
      }
    }},
    .routes = {
      {.status = http_error, .render = "5xx"},
      {.status = http_not_found, .render = "404"},
      {"/", http_get, "home", .render = "home"},
      {"/about", http_get, "about", .render = "about"},
      {"/contact", http_get, "contact", .render = "contact"},
      {"/todos", http_post, .call = create_todo, .uses = {"logged_in"}},
      {"/todos/:id", http_patch, .call = update_todo, .uses = {"logged_in"}},
      {"/todos/:id", http_delete, .call = delete_todo, .uses = {"logged_in"}},
      {"/todos", http_get, "todos", .call = list_todos, .uses = {"logged_in"}}
    },
    .resources = {
      {"get_todos", (resource){
        #embed "get_todos.sql"
      }},
      {"create_todos", (resource){
        #embed "create_todo.sql"
      }},
      {"update_todos", (resource){
        #embed "update_todo.sql"
      }},
      {"delete_todos", (resource){
        #embed "delete_todo.sql"
      }},
      {"layout", (resource){
        #embed "layout.mustache.html"
      }},
      {"404", (resource){
        #embed "404.mustache.html"
      }},
      {"5xx", (resource){
        #embed "5xx.mustache.html"
      }},
      {"home", (resource){
        #embed "home.mustache.html"
      }},
      {"about", (resource){
        #embed "about.mustache.html"
      }},
      {"contact", (resource){
        #embed "contact.mustache.html"
      }},
      {"todos", (resource){
        #embed "todos.mustache.html"
      }},
      {"todo", (resource){
        #embed "todo.mustache.html"
      }}
    }
  };
}
