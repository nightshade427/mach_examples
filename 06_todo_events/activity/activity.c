#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config activity(){
  return (config) {
    .name = "activity",

    .resources = {
      {"activity", "/activity", {logged_in()},
        .get = {
          query({"get_activities",
            .set_key = "activity",
            .db = "activity_db"
          }),
          render("activity")
        }
      }
    },

    .context = {
      {"activity", (asset){
        #embed "activity.mustache.html"
      }},
      {"get_activities", (asset){
        #embed "get_activities.sql"
      }},
      {"insert_activity", (asset){
        #embed "insert_activity.sql"
      }}
    },

    .events = {
      {"todo_created", {
        query({"insert_activity",
          .db = "activity_db"
        })
      }}
    },

    .databases = {{
      .name = "activity_db",
      .engine = sqlite_db,
      .connect = "file:activity.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_activity_table.sql"
      }}
    }},

    .modules = {sqlite, session_auth}
  };
}
