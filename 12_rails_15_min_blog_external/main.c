#include <mach.h>
#include <sqlite.h>
#include <tailwind.h>

// ─────────────────────────────────────────────
// 15 Minute Blog in MACH
// ─────────────────────────────────────────────
// Resources:
//   GET   /              → redirect to posts
//   GET   /posts         → list all posts
//   POST  /posts         → create a post
//   GET   /posts/new     → new post form
//   GET   /posts/:id     → show a post
//   PATCH /posts/:id     → update a post
//   DEL   /posts/:id     → delete a post
//   GET   /posts/:id/edit→ edit post form
// ─────────────────────────────────────────────

config mach() {
  return (config) {

    // ── Resources ──────────────────────────────

    .resources = {{

      {"home", "/",
        .get = {{
          redirect((u){"posts"})
        }}
      },

      {"posts", "/posts",
        .get = {{
          query((d){
            .set = "posts",
            .db  = "blog_db",
            (asset){
              #embed "get_posts.sql"
            }
          }),
          render((r){(asset){
            #embed "posts.mustache.html"
          }})
        }},

        .post = {{
          validate((v){
            .name       = "title",
            .validation = "^.{1,200}$",
            .message    = "Title is required (max 200 characters)"
          }),
          validate((v){
            .name       = "body",
            .validation = "^[\\s\\S]{1,50000}$",
            .message    = "Body is required"
          }),
          query((d){
            .db = "blog_db",
            (asset){
              #embed "insert_post.sql"
            }
          }),
          redirect((u){"posts"})
        }}
      },

      {"new_post", "/posts/new",
        .get = {{
          render((r){(asset){
            #embed "new_post.mustache.html"
          }})
        }}
      },

      {"edit_post", "/posts/:id/edit",
        .get = {{
          find((d){
            .set = "post",
            .db  = "blog_db",
            (asset){
              #embed "get_post.sql"
            }
          }),
          render((r){(asset){
            #embed "edit_post.mustache.html"
          }})
        }},

        .before = {
          validate((v){
            .name       = "id",
            .validation = "^\\d{1,10}$",
            .message    = "Invalid post ID"
          })
        }
      },
        
      {"post", "/posts/:id",
        .get = {{
          find((d){
            .set = "post",
            .db  = "blog_db",
            (asset){
              #embed "get_post.sql"
            }
          }),
          render((r){(asset){
            #embed "post_show.mustache.html"
          }})
        }},

        .patch = {{
          validate((v){
            .name       = "title",
            .validation = "^.{1,200}$",
            .message    = "Title is required (max 200 characters)"
          }),
          validate((v){
            .name       = "body",
            .validation = "^[\\s\\S]{1,50000}$",
            .message    = "Body is required"
          }),
          query((d){
            .db = "blog_db",
            (asset){
              #embed "update_post.sql"
            }
          }),
          redirect((u){"post"})
        }},

        .delete = {{
          query((d){
            .db = "blog_db",
            (asset){
              #embed "delete_post.sql"
            }
          }),
          redirect((u){"posts"})
        }},

        .before = {
          validate((v){
            .name       = "id",
            .validation = "^\\d{1,10}$",
            .message    = "Invalid post ID"
          })
        }
      }

    },

    // ── Shared Layout & Partials ───────────────

    .context = {
      {"layout", (asset){
        #embed "layout.mustache.html"
      }},
      {"post_card", (asset){
        #embed "post_card.mustache.html"
      }},
      {"post_form", (asset){
        #embed "post_form.mustache.html"
      }}
    },

    // ── Error Handling ─────────────────────────

    .errors = {
      {http_not_found, {
        render((r){(asset){
          #embed "404.mustache.html"
        }})
      }},
      {http_bad_request, {
        render((r){
          .status = http_bad_request,
          (asset){
            #embed "400.mustache.html"
          }
        })
      }}
    }

    },

    // ── Database ───────────────────────────────

    .databases = {{
      .engine  = sqlite_db,
      .name    = "blog_db",
      .connect = "file:blog.db?mode=rwc",
      .migrations = {(asset){
        #embed "create_posts.sql"
      }},
      .seeds = {(asset){
        #embed "seed_posts.sql"
      }}
    }},

    // ── Modules ────────────────────────────────

    .modules = {sqlite, tailwind}
  };
}
