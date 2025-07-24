#include <mach.h>
#include <htmx.h>
#include <tailwind.h>
#define m_root "root"
#define m_results "results"
#define m_pokemon "pokemon"
#define m_vote "vote"
#define m_challengers "challengers"

ma_state root(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_challengers)){
    auto const q = 
      "select id, name, sprite "
      "from pokemons "
      "order by random() "
      "limit 2;";
    ma_db_query(ctx, root, m_challengers, m_pokemon, q);
    return ma_processing;
  }
  auto const t = ma_ctx_get(ctx, m_challengers);
  auto const p0 = ma_table_record(t, 0);
  auto const p1 = ma_table_record(t, 1);
  ma_record_set(p0, "opponent_id", ma_record_get(p1, "id"));
  ma_record_set(p1, "opponent_id", ma_record_get(p0, "id"));
  ma_render(ctx, ma_ok, m_root);
  return ma_complete;
}

ma_state vote(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_vote)){
    auto const q = 
      "begin transaction; "
      "update pokemons "
        "set wins = wins + 1 "
        "where id = ?; "
      "update pokemons "
        "set loses = loses + 1 "
        "where id = ?; "
      "commit;";
    auto const winner = ma_params_get(ctx, "winner");
    auto const loser = ma_params_get(ctx, "loser");
    ma_db_query(ctx, vote, m_vote, m_pokemon, q, winner, loser);
    return ma_processing;
  }
  ma_transfer(ctx, root);
  return ma_proceed;
}

ma_state results(const ma_ctx ctx){
  if(!ma_ctx_has(ctx, m_results)){
    auto const q = 
      "select id, name, sprite, wins, loses, "
        "cast(wins as real) / nullif(loses, 0) as ratio_of_wins_to_loses, "
        "row_number() over (order by (cast(wins as real) / nullif(loses, 0)) desc) as rank "
      "from pokemons "
      "order by ratio_of_wins_to_loses desc;";
    ma_db_query(ctx, results, m_results, m_pokemon, q);
    return ma_processing;
  }
  ma_render(ctx, ma_ok, m_results);
  return ma_complete;
}

void create_pokemon_table(const ma_ctx ctx, const ma_db db){
  auto const q =
    "CREATE TABLE IF NOT EXISTS pokemons ( "
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "name TEXT NOT NULL, "
      "sprite TEXT NOT NULL, "
      "wins INTEGER NOT NULL DEFAULT 0, "
      "loses INTEGER NOT NULL DEFAULT 0 "
    ");";
  ma_db_migrate(ctx, db, q);
}

ma_config mach(){
  return (ma_config) {
    .includes = {htmx_config, tailwind_config},
    .templates = {
      {m_root, (ma_embed){
        #embed "root.mustache.html"
      }}, 
      {m_results, (ma_embed){
        #embed "results.mustache.html"
      }}
    },
    .databases = {
      {m_pokemon, ma_sqlite, "file::memory:?cache=shared",
        .migrations = {create_pokemon_table}
      }
    },
    .routes = {
      {ma_get, "/", m_root, .call = root},
      {ma_post, "/vote", m_vote, .call = vote},
      {ma_get, "/results", m_results, .call = results}
    }
  };
}
