#include <mach.h>
#include <sqlite.h>
#include <session_auth.h>

config activity(){
  return (config) {
    .name = "activity",

    .resources = {{
      {"activity", "/activity",
        .get = {{
          query((d){
            .set = "activity",
            .db = "activity_db",
            .query = (asset){
              #embed "get_activities.sql"
            }
          }),
          render((r){(asset){
            #embed "activity.mustache.html"
          }})
        }}
      }},

      .before = {
        session(),
        logged_in()
      }
    },
      
    .events = {
      {"todo_created", {
        query((d){
          .db = "activity_db",
          .query = (asset){
            #embed "insert_activity.sql"
          }
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
