#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config mach(){
  return (config) {"main", {session()},
    .resources = {
      {"home", "/",
        .get = {render("home")}
      },

      {"about", "/about",
        .get = {render("about")}
      },

      {"contact", "/contact",
        .get = {render("contact")}
      },

      {"todos", "/todos", {logged_in()},
        .get = {
          query({"get_todos", .set_key = "todos", .db = "todos_db"}),
          render("todos")
        },

        .post = {
          input({"title", .matches = m_not_empty}),
          query({"create_todo", .db = "todos_db"}),
          redirect("todos")
        }
      },

      {"todo", "/todos/:id", {
        logged_in(),
        input({"id", .matches = m_positive})},

        .patch = {
          input({"finished", .optional = true, .matches = "1", .message = "must be 1"}),
          query({"update_todo", .db = "todos_db"}),
          redirect("todos")
        },

        .delete = {
          query({"delete_todo", .db = "todos_db"}),
          redirect("todos")
        }
      }
    },

    .errors = {
      {m_error, { render("5xx") }},
      {m_not_found, { render("404")}}
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
      .connect = "file:{{user_id}}_todo.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_todos_table.sql"
      }}
    }},

    .modules = {sqlite, session_auth}
  };
}
