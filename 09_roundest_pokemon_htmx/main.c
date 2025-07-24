#include <mach.h>
#include <htmx.h>
#include <sqlite.h>
#include <tailwind.h>

void assign_opponents(){
  auto const t = get("challengers");
  auto const p0 = table_get(t, 0);
  auto const p1 = table_get(t, 1);
  record_set(p0, "opponent_id", record_get(p1, "id"));
  record_set(p1, "opponent_id", record_get(p0, "id"));
}

config mach(){
  return (config) {
    .resources = {{
      {"home", "/",
        .get = {{
          query((d){
            .set = "challengers",
            .db = "pokemon_db",
            .query =
              "select id, name, sprite "
              "from pokemons "
              "order by random() "
              "limit 2;"
          }),
          invoke((i){assign_opponents}),
          render((r){.asset = "home"})
        }},

        .post = {{
          validate((v){
            .name = "winner",
            .validation = "^\\d{1,8}$",
            .message = "must be 1-99999999"
          }),
          validate((v){
            .name = "loser",
            .validation = "^\\d{1,8}$",
            .message = "must be 1-99999999"
          }),
          find((d){
            .db = "pokemon_db",
            .query =
              "begin transaction; "
              "update pokemons "
                "set wins = wins + 1 "
                "where id = {{winner}}; "
              "update pokemons "
                "set loses = loses + 1 "
                "where id = {{loser}}; "
              "commit;"
          }),
          reroute((u){"home"})
        }}
      },

      {"result", "/results",
        .get = {{
          query((d){
            .set = "results",
            .db = "pokemon_db",
            .query =
              "select id, name, sprite, wins, loses, "
                "cast(wins as real) / nullif(loses, 0) as ratio_of_wins_to_loses, "
                "row_number() over (order by (cast(wins as real) / nullif(loses, 0)) desc) as rank "
              "from pokemons "
              "order by ratio_of_wins_to_loses desc;"
          }),
          render((r){
            "{{< home}}"
              "{{$body}}"
                "<div class='container mx-auto px-4 py-8 text-white'>"
                  "<div class='grid gap-4'>"
                    "{{#results}}"
                      "<div class='flex items-center gap-6 p-6 bg-gray-800/40 rounded-lg shadow hover:shadow-md transition-shadow'>"
                        "<div class='text-2xl font-bold text-gray-400 w-8'>"
                          "{{rank}}"
                        "</div>"
                        "<img src='{{sprite}}' alt='{{name}}' loading='lazy' class='w-20 h-20'>"
                        "<div class='flex-grow'>"
                          "<div class='text-gray-400 text-sm'>#{{id}}</div>"
                          "<h2 class='text-xl font-semibold capitalize'>{{name}}</h2>"
                        "</div>"
                        "<div class='text-right'>"
                          "<div class='text-2xl font-bold text-blue-400'>"
                            "{{ratio_of_wins_to_loses}}"
                          "</div>"
                          "<div class='text-sm text-gray-400'>"
                            "{{wins}}W - {{loses}}L"
                          "</div>"
                        "</div>"
                      "</div>"
                    "{{/results}}"
                  "</div>"
                "</div>"
              "{{/body}}"
            "{{/home}}"
          })
        }}
      }},

      .context = {
        {"home",
          "<!DOCTYPE html>"
          "<html>"
            "<head>"
              "<title>Roundest (MaCH Version)</title>"
              "{{> htmx_script}}"
              "{{> tailwind_script}}"
            "</head>"
            "<body hx-boost='true'>"
              "<div class='bg-gray-950 text-white flex flex-col justify-between font-geist min-h-screen border-t-2 border-red-300'>"
                "<nav class='flex flex-row justify-between items-center py-4 px-8'>"
                  "<div class='flex items-baseline'>"
                    "<a href='{{url:home}}' class='font-bold text-3xl'>"
                      "Round<span class='text-red-300'>est</span>"
                    "</a>"
                    "<p class='text-gray-400 font-extralight pl-2 text-2xl'>"
                      "(MaCH Version)"
                    "</p>"
                  "</div>"
                  "<div class='flex flex-row items-center gap-8'>"
                    "<a href='{{url:results}}' class='hover:underline text-lg'>Results</a>"
                  "</div>"
                "</nav>"
                "<main>"
                  "{{$body}}"
                    "<div class='container mx-auto px-4'>"
                      "<h1 class='text-3xl font-bold text-center mb-8'>Vote for which is roundest</h1>"
                      "<div class='flex justify-center gap-16 items-center min-h-[80vh]'>"
                        "{{#challengers}}"
                          "<form action='{{url:home}}' method='post'>"
                            "<div class='flex flex-col items-center gap-4'>"
                              "<img src='{{sprite}}' alt='{{name}}' class='w-64 h-64 sprite'>"
                              "<div class='text-center'>"
                                "<span class='text-gray-500 text-lg'>#{{id}}</span>"
                                "<h2 class='text-2xl font-bold capitalize'>{{name}}</h2>"
                                "<input type='hidden' name='winner' value='{{id}}'>"
                                "<input type='hidden' name='loser' value='{{opponent_id}}'>"
                                "<button class='mt-4 px-8 py-3 bg-blue-500 text-white rounded-lg text-lg font-semibold hover:bg-blue-600 transition-colors'>"
                                  "Vote"
                                "</button>"
                              "</div>"
                            "</div>"
                          "</form>"
                        "{{/challengers}}"
                      "</div>"
                    "</div>"
                  "{{/body}}"
                "</main>"
                "<footer class='font-light text-center py-3 text-gray-500'>"
                  "<a href='https://code.nightshadecoder.dev/nightshade/mach_examples' target='_blank'>GitHub</a>"
                "</footer>"
              "</div>"
            "</body>"
          "</html>"
        }
      }
    },
      
    .databases = {{
      .name = "pokemon_db",
      .engine = sqlite_db,
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
    }},

    .modules = {htmx, sqlite, tailwind}
  };
}
