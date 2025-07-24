#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config todos_config(){
  return (config) {
    .name = "todos",

    .includes = {
      sqlite_config,
      session_auth_config
    },

    .databases = {{
      .engine = sqlite,
      .name = "todos_db",
      .connect = "file:todo.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_todos_table.sql"
      }}
    }},

    .publishes = {
      {"todo_created",
        .with = {
          "user_id",
          "title"
        }
      }
    },

    .resources = {
      .before = {
        session(),
        logged_in()
      },

      .urls = {
        {"/todos",
          .context = {
            {"todo", (asset){
              #embed "todo.mustache.html"
            }}
          },

          .get = {{
            db((d){{{
              .set = "todos",
              .db = "todos_db",
              .query = (asset){
                #embed "get_todos.sql"
              }
            }}}),
            render((r){(asset){
              #embed "todos.mustache.html"
            }})
          }},

          .post = {{
            param((p){
              .name = "title",
              .validation = "^\\S{1,16}$",
              .message = "must be 1-16 characters, no spaces"
            }),
            db((d){{{
              .db = "todos_db",
              .query = (asset){
                #embed "create_todo.sql"
              }
            }}}),
            emit((e){"todo_created"}),
            redirect((u){"/todos"})
          }}
        },

        {"/todos/:id",
          .before = {
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
              .find = (asset){
                #embed "update_todo.sql"
              }
            }}}),
            redirect((u){"/todos"})
          }},
          .delete = {{
            db((d){{{
              .db = "todos_db",
              .find = (asset){
                #embed "delete_todo.sql"
              }
            }}}),
            redirect((u){"/todos"})
          }}
        }
      }
    }

};}
