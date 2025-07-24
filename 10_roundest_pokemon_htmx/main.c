#include <mach.h>
#include <htmx.h>
#include <sqlite.h>
#include <mustache.h>
#include <tailwind.h>

void battle(){
  await_assign_db_query("challengers", (q){.db = "pokemon_db",
    "select id, name, sprite "
    "from pokemons "
    "order by random() "
    "limit 2;"
  });
  auto const t = get("challengers");
  auto const p0 = table_get(t, 0);
  auto const p1 = table_get(t, 1);
  record_assign(p0, "opponent_id", record_get(p1, "id"));
  record_assign(p1, "opponent_id", record_get(p0, "id"));
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "home"});
}

void vote(){
  db_query((q){.db = "pokemon_db",
    "begin transaction; "
    "update pokemons "
      "set wins = wins + 1 "
      "where id = {{winner}}; "
    "update pokemons "
      "set loses = loses + 1 "
      "where id = {{loser}}; "
    "commit;"
  });
  transfer(battle);
}

void results(){
  await_assign_db_query("results", (q){.db = "pokemon_db",
    "select id, name, sprite, wins, loses, "
      "cast(wins as real) / nullif(loses, 0) as ratio_of_wins_to_loses, "
      "row_number() over (order by (cast(wins as real) / nullif(loses, 0)) desc) as rank "
    "from pokemons "
    "order by ratio_of_wins_to_loses desc;"
  });
  respond(http_ok, mime_html, (b){.engine = mustache, .resource = "results"});
}

config mach(){
  return (config) {
    .includes = {
      htmx_config,
      sqlite_config,
      mustache_config,
      tailwind_config
    },
    .routes = {
      {"/", http_get, "home", .call = battle},
      {"/vote", http_post, "vote", .call = vote},
      {"/results", http_get, "results", .call = results},
    },
    .resources = {
      {"home", (resource){
        #embed "home.mustache.html"
      }}, 
      {"results", (resource){
        #embed "results.mustache.html"
      }},
    },
    .databases = {{
      .name = "pokemon_db",
      .engine = sqlite,
      .connect = "file::memory:?cache=shared",
      .migrations = {
        "CREATE TABLE IF NOT EXISTS pokemons ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "name TEXT NOT NULL, "
          "sprite TEXT NOT NULL, "
          "wins INTEGER NOT NULL DEFAULT 0, "
          "loses INTEGER NOT NULL DEFAULT 0 "
        ");"
      }
    }}
  };
}
