#include <mach.h>
#include <sqlite.h>
#include <datastar.h>
#include <session_auth.h>

config mach(){
  return (config) {

    .includes = {
      sqlite_config,
      datastar_config,
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

    .resources = {
      .before = {session()},

      .context = {
        {"layout", (asset){
          #embed "layout.mustache.html"
        }},
        {"todo", (asset){
          #embed "todo.mustache.html"
        }}
      },

      .errors = {
        {http_error, {
          render((r){(asset){
            #embed "5xx.mustache.html"
          }})
        }},

        {http_not_found, {
          render((r){(asset){
            #embed "404.mustache.html"
          }})
        }}
      },

      .urls = {
        {"/",
          .get = {{
            render((r){(asset){
              #embed "home.mustache.html"
            }})
          }}
        },

        {"/about",
          .get = {{
            render((r){(asset){
              #embed "about.mustache.html"
            }})
          }}
        },

        {"/contact",
          .get = {{
            render((r){(asset){
              #embed "contact.mustache.html"
            }})
          }}
        },

        {"/todos",
          .before = {logged_in()},

          .sse = {.channel = "todos/{{user_id}}"},

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
              .set = "todo",
              .db = "todos_db",
              .query = (asset){
                #embed "create_todo.sql"
              }
            }}}),
            datastar((ds){
              .target = "todos",
              .mode = mode_prepend,
              .channel = "todos/{{user_id}}",
              .elements = {.asset = "todo"}
            })
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
              .set = "todo",
              .db = "todos_db",
              .find = (asset){
                #embed "update_todo.sql"
              }
            }}}),
            datastar((ds){
              .mode = mode_replace,
              .target = "todo_{{id}}",
              .channel = "todos/{{user_id}}",
              .elements = {.asset = "todo"}
            })
          }},

          .delete = {{
            db((d){{{
              .db = "todos_db",
              .find = (asset){
                #embed "delete_todo.sql"
              }
            }}}),
            datastar((ds){
              .mode = mode_remove,
              .target = "todo_{{id}}",
              .channel = "todos/{{user_id}}"
            })
          }}
        }
      }
    }

};}
