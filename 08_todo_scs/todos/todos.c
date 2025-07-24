#include <mach.h>
#include <sqlite.h>
#include <mustache.h>
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
  db_query((q){.db = "todos_db", 
    "insert into todos(user_id, title) "
    "values({{user_id}}, {{title}});"
  });
  redirect("todos");
}

void update_todo(){
  db_find((q){.db = "todos_db",
    "update todos "
    "set finished = {{finished}} "
    "where user_id = {{user_id}} "
      "and id = {{id}};"
  });
  redirect("todos");
}

void delete_todo(){
  db_find((q){.db = "todos_db",
    "delete from todos "
    "where user_id = {{user_id}} "
      "and id = {{id}};"
  });
  redirect("todos");
}

config todos_config(){
  return (config) {
    "todos",
    .uses = {
      "session",
      "logged_in"
    },
    .includes = {
      sqlite_config,
      mustache_config,
      session_auth_config
    },
    .routes = {
      {"/todos", http_post, .call = create_todo},
      {"/todos/:id", http_patch, .call = update_todo},
      {"/todos/:id", http_delete, .call = delete_todo},
      {"/todos", http_get, "todos", .call = list_todos}
    },
    .databases = {{
      .name = "todos_db",
      .engine = sqlite,
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
    .resources = {
      {"todos", (resource){
        #embed "todos.mustache.html"
      }},
      {"todo", (resource){
        #embed "todo.mustache.html"
      }},
    }
  };
}
