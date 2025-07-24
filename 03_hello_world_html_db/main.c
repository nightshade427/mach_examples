#include <mach.h>
#include <mustache.h>
#include <sqlite.h>

void hello(){
  auto const q =
    "select name "
    "from greetings "
    "limit ?;";
  auto const limit = "1";
  db_query_async_await("greeting", "hello_db", q, limit);
  render_res_async(http_ok, "hello_world");
}

void create_greetings_table(){
  auto const q =
    "CREATE TABLE IF NOT EXISTS greetings ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "name TEXT NOT NULL "
    "); ";
  migrate(q);
}

void seed_greetings(){
  auto const q = 
    "INSERT INTO greetings(name) "
    "VALUES('World');";
  seed(q);
}

config mach(){
  return (config) {
    .routes = {
      {http_get, "/", .call = hello}
    },
    .includes = {
      mustache_config, 
      sqlite_config
    },
    .databases = {
      {"hello_db", .engine = "sqlite", "file:hello.db?mode=rwc",
        .migrations = {create_greetings_table},
        .seeds = {seed_greetings}
      }
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"hello_world", (string_embed){
          #embed "hello_world.mustache.html"
        }}
      }
    }}
  };
}
