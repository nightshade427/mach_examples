#include <mach.h>
#include <mustache.h>
#include <htmx.h>
#include <tailwind.h>
#include <sqlite.h>

void battle(){
  auto const q = 
    "select id, name, sprite "
    "from pokemons "
    "order by random() "
    "limit 2;";
  db_query_async_await("challengers", "pokemon_db", q);
  auto const t = get("challengers");
  auto const p0 = table_get(t, 0);
  auto const p1 = table_get(t, 1);
  record_set(p0, "opponent_id", record_get(p1, "id"));
  record_set(p1, "opponent_id", record_get(p0, "id"));
  render_res_async(http_ok, "home");
}

void vote(){
  auto const q = 
    "begin transaction; "
    "update pokemons "
      "set wins = wins + 1 "
      "where id = ?; "
    "update pokemons "
      "set loses = loses + 1 "
      "where id = ?; "
    "commit;";
  auto const loser = get("loser");
  auto const winner = get("winner");
  db_query_async_await("vote", "pokemon_db", q, winner, loser);
  transfer_async(battle);
}

void results(){
  auto const q = 
    "select id, name, sprite, wins, loses, "
      "cast(wins as real) / nullif(loses, 0) as ratio_of_wins_to_loses, "
      "row_number() over (order by (cast(wins as real) / nullif(loses, 0)) desc) as rank "
    "from pokemons "
    "order by ratio_of_wins_to_loses desc;";
  db_query_async_await("stats", "pokemon_db", q);
  render_res_async(http_ok, "results");
}

void create_pokemon_table(){
  auto const q =
    "CREATE TABLE IF NOT EXISTS pokemons ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "name TEXT NOT NULL, "
      "sprite TEXT NOT NULL, "
      "wins INTEGER NOT NULL DEFAULT 0, "
      "loses INTEGER NOT NULL DEFAULT 0 "
    ");";
  migrate(q);
}

config mach(){
  return (config) {
    .includes = {
      mustache_config,
      sqlite_config,
      htmx_config,
      tailwind_config
    },
    .databases = {
      {"pokemon_db", .engine = "sqlite", "file::memory:?cache=shared",
        .migrations = {create_pokemon_table}
      }
    },
    .routes = {
      {http_get, "/", "home", .call = battle},
      {http_post, "/vote", "vote", .call = vote},
      {http_get, "/results", "results", .call = results}
    },
    .rendering = {{
      .engine = "mustache",
      .templates = {
        {"home", (string_embed){
          #embed "home.mustache.html"
        }}, 
        {"results", (string_embed){
          #embed "results.mustache.html"
        }}
      }
    }}
  };
}
