#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config mach(){
  return (config) {
    .resources = {{
      {"home", "/",
        .get = {{
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<p>home</p>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      },

      {"about", "/about",
        .get = {{
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<p>about us</p>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      },

      {"contact", "/contact",
        .get = {{
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<p>contact us</p>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      },

      {"todos", "/todos",
        .get = {{
          query((d){
            .set = "todos",
            .db = "todos_db",
            .query =
              "select id, title, finished "
              "from todos "
              "where user_id = {{user_id}};"
          }),
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<form action='{{url:todos}}' method='post'>"
                  "<input type='text' name='title' placeholder='new todo' required>"
                  "<button type='submit'>add</button>"
                "</form>"
                "{{^todos}}"
                  "<p>no todos</p>"
                "{{/todos}}"
                "{{#todos}}"
                  "{{#.}}"
                    "{{> todo}}"
                  "{{/.}}"
                "{{/todos}}"
              "{{/body}}"
            "{{/layout}}"
          })
        }},

        .post = {{
          validate((v){
            .name = "title",
            .validation = "^\\S{1,16}$",
            .message = "must be 1-16 characters, no spaces"
          }),
          query((d){
            .db = "todos_db",
            .query =
              "insert into todos(user_id, title) "
              "values({{user_id}}, {{title}});"
          }),
          redirect((u){"todos"})
        }},

        .before = {logged_in()},

        .context = {
          {"todo",
            "<div>"
              "<form action='{{url:todo:id}}' method='post' style='display:inline'>"
                "<input type='hidden' name='http_method' value='patch'>"
                "{{^finished}}"
                  "<input type='checkbox' name='finished' value='1'>"
                "{{/finished}}"
                "{{#finished}}"
                  "<input type='checkbox' name='finished' value='1' checked>"
                "{{/finished}}"
                "{{title}}"
                "<button type='submit'>save</button>"
              "</form>"
              "<form action='{{url:todo:id}}' method='post' style='display:inline'>"
                "<input type='hidden' name='http_method' value='delete'>"
                "<button type='submit'>delete</button>"
              "</form>"
            "</div>"
          }
        }
      },

      {"todo", "/todos/:id",
        .patch = {{
          validate((v){
            .name = "finished",
            .optional = true,
            .validation = "1",
            .message = "must be 1"
          }),
          query((d){
            .db = "todos_db",
            .query =
              "update todos "
              "set finished = {{finished}} "
              "where user_id = {{user_id}} "
                "and id = {{id}};"
          }),
          redirect((u){"todos"})
        }},

        .delete = {{
          query((d){
            .db = "todos_db",
            .query =
              "delete from todos "
              "where user_id = {{user_id}} "
                "and id = {{id}};"
          }),
          redirect((u){"todos"})
        }},

        .before = {
          logged_in(),
          validate((v){
            .name = "id",
            .validation = "^\\d{1,10}$",
            .message = "must be between 1-9999999999"
          })
        }
      }},

      .before = {session()},

      .context = {
        {"layout",
          "<html>"
            "<head>"
              "<link rel='icon' href='{{asset:favicon.png}}'>"
            "</head>"
            "<body>"
              "<p>"
                "{{^user}}"
                  "<a href='{{url:login}}'>sign in</a>"
                "{{/user}}"
                "{{#user}}"
                  "welcome, {{short_name}}"
                "{{/user}}"
              "</p>"
              "<nav>"
                "<a href='{{url:home}}'>home</a>"
                "<a href='{{url:about}}'>about us</a>"
                "<a href='{{url:contact}}'>contact us</a>"
                "<a href='{{url:todos}}'>todos</a>"
              "</nav>"
              "{{$body}}"
              "{{/body}}"
            "</body>"
          "</html>"
        }
      },

      .errors = {
        {http_error, {
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<p>error</p>"
              "{{/body}}"
            "{{/layout}}"
          })
        }},

        {http_not_found, {
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<p>not found</p>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      }
    },
      
    .databases = {{
      .engine = sqlite_db,
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
      
    .modules = {sqlite, session_auth}
  };
}
