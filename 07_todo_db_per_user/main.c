#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
#include <session_auth.h>

void list_todos(){
  await_assign_db_query("todos", (q){.db = "todos_db",
    "select id, title, finished "
    "from todos;"
  });
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "todos"});
}

void create_todo(){
  db_query((q){.db = "todos_db", 
    "insert into todos(title) "
    "values({{title}});"
  });
  redirect("todos");
}

void update_todo(){
  db_find((q){.db = "todos_db",
    "update todos "
    "set finished = {{finished}} "
    "where id = {{id}};"
  });
  redirect("todos");
}

void delete_todo(){
  db_find((q){.db = "todos_db",
    "delete from todos "
    "where id = {{id}};"
  });
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
      .name = "todos_db",
      .engine = sqlite,
      .connect = "file:todo_{{user_id}}.db?mode=rwc",
      .migrations = {
        "CREATE TABLE IF NOT EXISTS todos ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "title TEXT NOT NULL, "
          "finished INTEGER CHECK(finished IN (1))"
        ");"
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
