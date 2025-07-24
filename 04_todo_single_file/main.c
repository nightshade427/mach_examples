#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config mach(){
  return (config) {

    .includes = {
      sqlite_config,
      session_auth_config
    },

    .databases = {{
      .engine = sqlite,
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

    .resources = {
      .before = {session()},

      .context = {
        {"layout",
          "<html>"
            "<head>"
              "<link rel='icon' href='/favicon.png'>"
            "</head>"
            "<body>"
              "<p>"
                "{{^user}}"
                  "<a href='/login'>sign in</a>"
                "{{/user}}"
                "{{#user}}"
                  "welcome, {{first_name}}"
                "{{/user}}"
              "</p>"
              "<nav>"
                "<a href='/'>home</a>"
                "<a href='/about'>about us</a>"
                "<a href='/contact'>contact us</a>"
                "<a href='/todos'>todos</a>"
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
      },

      .urls = {
        {"/",
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

        {"/about",
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

        {"/contact",
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

        {"/todos",
          .before = {logged_in()},

          .context = {
            {"todo",
              "<div>"
                "<form action='/todos/{{id}}' method='post' style='display:inline'>"
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
                "<form action='/todos/{{id}}' method='post' style='display:inline'>"
                  "<input type='hidden' name='http_method' value='delete'>"
                  "<button type='submit'>delete</button>"
                "</form>"
              "</div>"
            }
          },

          .get = {{
            db((d){{{
              .set = "todos",
              .db = "todos_db",
              .query =
                "select id, title, finished "
                "from todos "
                "where user_id = {{user_id}};"
            }}}),
            render((r){
              "{{< layout}}"
                "{{$body}}"
                  "<form action='/todos' method='post'>"
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
            param((p){
              .name = "title",
              .validation = "^\\S{1,16}$",
              .message = "must be 1-16 characters, no spaces"
            }),
            db((d){{{
              .db = "todos_db",
              .query =
                "insert into todos(user_id, title) "
                "values({{user_id}}, {{title}});"
            }}}),
            redirect((u){"/todos"})
          }}
        },

        {"/todos/:id",
          .before = {
            logged_in(),
            param((p){
              .name = "id",
              .validation = "^\\d{1,10}$",
              .message = "must be between 1-9999999999"
            })
          },

          .patch = {{
            param((p){
              .name = "finished",
              .optional = true,
              .validation = "1",
              .message = "must be 1"
            }),
            db((d){{{
              .db = "todos_db",
              .query =
                "update todos "
                "set finished = {{finished}} "
                "where user_id = {{user_id}} "
                  "and id = {{id}};"
            }}}),
            redirect((u){"/todos"})
          }},

          .delete = {{
            db((d){{{
              .db = "todos_db",
              .query =
                "delete from todos "
                "where user_id = {{user_id}} "
                  "and id = {{id}};"
            }}}),
            redirect((u){"/todos"})
          }}
        }
      }
    }

};}
