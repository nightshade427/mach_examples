#include <mach.h>
#include <mustache.h>
#include <sqlite.h>
#include <session_auth.h>

void list_todos(){
  auto const q =
    "select title, finished "
    "from todos "
    "where user_id = ?;";
  auto const uid = get("user_id");
  db_query_async_await("todos", "todos_db", q, uid);
  render_res_async(http_ok, "todos");
}

void create_todo(){
  auto const q =
    "insert into todos(user_id, title, finished) "
    "values(?, ?, false) "
    "returning id, title, finished;";
  auto const t = get("title");
  auto const uid = get("user_id");
  db_query_async_await("todo", "todos_db", q, uid, t);
  redirect_async("home");
}

void update_todo(){
  auto const q =
    "update todos "
    "set finished = ? "
    "where user_id = ? "
      "and id = ? "
    "returning id, title, finished;";
  auto const tid = get("id");
  auto const f = get("finished");
  auto const uid = get("user_id");
  db_find_async_await("todo", "todos_db", q, f, uid, tid);
  redirect_async("home");
}

void delete_todo(){
  auto const q =
    "delete from todos "
    "where user_id = ? "
      "and id = ? "
    "returning id;";
  auto const tid = get("id");
  auto const uid = get("user_id");
  db_find_async_await("todo", "todos_db", q, uid, tid);
  redirect_async("home");
}

void create_todos_table(){
  auto const q =
    "CREATE TABLE IF NOT EXISTS todos ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "user_id INTEGER NOT NULL, "
      "title TEXT NOT NULL, "
      "finished INTEGER NOT NULL DEFAULT 0 CHECK(finished IN (0, 1))"
    "); "
    "CREATE INDEX IF NOT EXISTS idx_todos_user_id ON todos(user_id);";
  migrate(q);
}

config todos_config(){
  return (config) {
    "todos",
    .uses = {
      "session",
      "logged_in"
    },
    .includes = {
      mustache_config,
      sqlite_config,
      session_auth_config
    },
    .databases = {
      {"todos_db", .engine = "sqlite", "file:todo.db?mode=rwc",
        .migrations = {create_todos_table}
      }
    },
    .routes = {
      {http_get, "/todos", "todos", .call = list_todos},
      {http_post, "/todos", .call = create_todo},
      {http_patch, "/todos/:id", .call = update_todo},
      {http_delete, "/todos/:id", .call = delete_todo}
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"todos", (string_embed){
          #embed "todos.mustache.html"
        }},
        {"todo", (string_embed){
          #embed "todo.mustache.html"
        }}
      }
    }}
  };
}
