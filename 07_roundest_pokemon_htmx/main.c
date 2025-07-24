#include <stdlib.h>
#include <mach.h>
#include <htmx.h>
#include <sqlite.h>
#include <tailwind.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/",
        .get = {
          query({"get_challengers", .set_key = "challengers", .db = "pokemon_db"}),
          exec(^(){
            auto const t = get("challengers");
            auto const p0 = table_get(t, 0);
            auto const p1 = table_get(t, 1);
            record_set(p0, "opponent_id", record_get(p1, "id"));
            record_set(p1, "opponent_id", record_get(p0, "id"));
          }),
          render("home")
        },

        .post = {
          input(
            {"winner", .matches = m_positive},
            {"loser", .matches = m_positive}
          ),
          query({"vote", .db = "pokemon_db"}),
          reroute("home")
        }
      },

      {"result", "/results",
        .get = {
          query({"get_results", .set_key = "results", .db = "pokemon_db"}),
          render("results")
        }
      }
    },

    .context = {
      {"home", (asset){
        #embed "home.mustache.html"
      }},
      {"results", (asset){
        #embed "results.mustache.html"
      }},
      {"get_challengers", (asset){
        #embed "get_challengers.sql"
      }},
      {"vote", (asset){
        #embed "vote.sql"
      }},
      {"get_results", (asset){
        #embed "get_results.sql"
      }}
    },

    .databases = {{
      .name = "pokemon_db",
      .engine = sqlite_db,
      .connect = "file::memory:?cache=shared",
      .migrations = {(asset){
        #embed "create_pokemons_table.sql"
      }}
    }},

    .modules = {htmx, sqlite, tailwind}
  };
}
