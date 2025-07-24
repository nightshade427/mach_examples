#include <mach.h>
#include <mustache.h>
#include <session_auth.h>
#include <sqlite.h>

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

config mach(){
  return (config) {
    .uses = {"session"},
    .databases = {
      {"todos_db", .engine = "sqlite", "file:todo.db?mode=rwc",
        .migrations = {create_todos_table}
      }
    },
    .includes = {
      mustache_config, 
      sqlite_config,
      session_auth_config
    },
    .routes = {
      {.status = http_error, .render = "5xx"},
      {.status = http_not_found, .render = "404"},
      {http_get, "/", "home", .render = "home"},
      {http_get, "/about", "about", .render = "about"},
      {http_get, "/contact", "contact", .render = "contact"},
      {http_get, "/todos", "todos", .call = list_todos, .uses = {"logged_in"}},
      {http_post, "/todos", .call = create_todo, .uses = {"logged_in"}},
      {http_patch, "/todos/:id", .call = update_todo, .uses = {"logged_in"}},
      {http_delete, "/todos/:id", .call = delete_todo, .uses = {"logged_in"}}
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"layout", (string_embed){
          #embed "layout.mustache.html"
        }},
        {"404", (string_embed){
          #embed "404.mustache.html"
        }},
        {"5xx", (string_embed){
          #embed "5xx.mustache.html"
        }},
        {"home", (string_embed){
          #embed "home.mustache.html"
        }},
        {"about", (string_embed){
          #embed "about.mustache.html"
        }},
        {"contact", (string_embed){
          #embed "contact.mustache.html"
        }},
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

