#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config activity_config(){
  return (config) {
    .name = "activity",

    .includes = {
      sqlite_config,
      session_auth_config
    },

    .databases = {{
      .name = "activity_db",
      .engine = sqlite,
      .connect = "file:activity.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_activity_table.sql"
      }}
    }},

    .events = {
      {"todo_created", {
        db((d){{{
          .db = "activity_db",
          .query = (asset){
            #embed "insert_activity.sql"
          }
        }}})
      }}
    },

    .resources = {
      .before = {
        session(),
        logged_in()
      },

      .urls = {
        {"/activity",
          .get = {{
            db((d){{{
              .set = "activity",
              .db = "activity_db",
              .query = (asset){
                #embed "get_activities.sql"
              }
            }}}),
            render((r){(asset){
              #embed "activity.mustache.html"
            }})
          }}
        }
      }
    }

};}
