#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
#include <datastar.h>
#include <session_auth.h>

void list_todos(){
  await_assign_db_query("todos", (q){.db = "todos_db",
    "select id, title, finished "
    "from todos "
    "where user_id = {{user_id}};"
  });
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "todos"});
}

void create_todo(){
  await_assign_db_query("todo", (q){.db = "todos_db", 
    "insert into todos(user_id, title) "
    "values({{user_id}}, {{title}}) "
    "returning id, title, finished;"
  });
  respond_ds_elements((d){
    .target = "todos", 
    .mode = mode_prepend, 
    .client = format("todos/{{user_id}}"),
    .elements = (b){.engine = mustache, .resource = "todo"} 
  });
}

void update_todo(){
  await_assign_db_find("todo", (q){.db = "todos_db",
    "update todos "
    "set finished = {{finished}} "
    "where user_id = {{user_id}} "
      "and id = {{id}} "
    "returning id, title, finished;"
  });
  respond_ds_elements((d){
    .mode = mode_replace,
    .target = format("todos_{{id}}"),
    .client = format("todos/{{user_id}}"),
    .elements = (b){.engine = mustache, .resource = "todo"} 
  });
}

void delete_todo(){
  await_assign_db_find("todo", (q){.db = "todos_db",
    "delete from todos "
    "where user_id = {{user_id}} "
      "and id = {{id}} "
    "returning id;"
  });
  respond_ds_elements((d){
    .mode = mode_remove,
    .target = format("todos_{{id}}"),
    .client = format("todos/{{user_id}}")
  });
}

config mach(){
  return (config) {
    .uses = {"session"},
    .includes = {
      sqlite_config,
      mustache_config, 
      datastar_config, 
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
      {"/todos", http_get, "todos", .call = list_todos, .uses = {"logged_in"}},
      {"/todos_sse", http_sse, "todos_sse", .client = "todos/:user_id", .uses = {"logged_in"}}
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
      }},
    }
  };
}

