#include <mach.h>
#include <sqlite.h>
#include <datastar.h>
#include <session_auth.h>

config mach(){
  return (config) {
    .resources = {
      {"home", "/", {session()},
        .get = {
          render("home")
        }
      },

      {"about", "/about", {session()},
        .get = {
          render("about")
        }
      },

      {"contact", "/contact", {session()},
        .get = {
          render("contact")
        }
      },

      {"todos", "/todos", {logged_in()},
        .sse = {"todos:{{user_id}}"},

        .get = {
          query({"get_todos",
            .set_key = "todos",
            .db = "todos_db"
          }),
          render("todos")
        },

        .post = {
          validate({"title",
            .validation = "^\\S{1,16}$",
            .message = "must be 1-16 characters, no spaces"
          }),
          query({"create_todo",
            .set_key = "todo",
            .db = "todos_db"
          }),
          ds_sse("todos:{{user_id}}",
            .target = "todos",
            .mode = mode_prepend,
            .elements = {"todo"}
          )
        }
      },

      {"todo", "/todos/:id", {
        logged_in(),
        validate({"id",
          .validation = "^\\d{1,10}$",
          .message = "must be between 1-9999999999"
        })},

        .patch = {
          validate({"finished",
            .optional = true,
            .validation = "1",
            .message = "must be 1"
          }),
          find({"update_todo",
            .set_key = "todo",
            .db = "todos_db"
          }),
          ds_sse("todos:{{user_id}}",
            .target = "todo_{{id}}",
            .mode = mode_replace,
            .elements = {"todo"}
          )
        },

        .delete = {
          find({"delete_todo",
            .db = "todos_db"
          }),
          ds_sse("todos:{{user_id}}",
            .target = "todo_{{id}}",
            .mode = mode_remove
          )
        }
      }
    },

    .errors = {
      {http_error, {
        render("5xx")
      }},

      {http_not_found, {
        render("404")
      }}
    },

    .context = {
      {"layout", (asset){
        #embed "layout.mustache.html"
      }},
      {"home", (asset){
        #embed "home.mustache.html"
      }},
      {"about", (asset){
        #embed "about.mustache.html"
      }},
      {"contact", (asset){
        #embed "contact.mustache.html"
      }},
      {"5xx", (asset){
        #embed "5xx.mustache.html"
      }},
      {"404", (asset){
        #embed "404.mustache.html"
      }},
      {"todos", (asset){
        #embed "todos.mustache.html"
      }},
      {"todo", (asset){
        #embed "todo.mustache.html"
      }},
      {"get_todos", (asset){
        #embed "get_todos.sql"
      }},
      {"create_todo", (asset){
        #embed "create_todo.sql"
      }},
      {"update_todo", (asset){
        #embed "update_todo.sql"
      }},
      {"delete_todo", (asset){
        #embed "delete_todo.sql"
      }}
    },

    .databases = {{
      .engine = sqlite_db,
      .name = "todos_db",
      .connect = "file:todo.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_todos_table.sql"
      }}
    }},

    .modules = {sqlite, datastar, session_auth}
  };
}
