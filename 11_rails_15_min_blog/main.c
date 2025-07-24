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

      // Home: just redirect to the posts index
      {"home", "/",
        .get = {{
          redirect((u){"posts"})
        }}
      },

      // Posts index + create
      {"posts", "/posts",
        .get = {{
          query((d){
            .set = "posts",
            .db  = "blog_db",
              "select id, title, body, created_at "
              "from posts "
              "order by created_at desc;"
          }),
          render((r){
            "{{< layout}}"
              "{{$body}}"

                "<div class=\"flex items-center justify-between mb-8\">"
                  "<h1 class=\"text-3xl font-bold text-gray-900\">"
                    "All Posts"
                  "</h1>"
                  "<a href=\"{{url:new_post}}\" "
                     "class=\"bg-blue-600 text-white px-4 py-2 rounded-lg "
                            "hover:bg-blue-700 transition\">"
                    "New Post"
                  "</a>"
                "</div>"

                "{{#posts}}"
                  "{{> post_card}}"
                "{{/posts}}"

                "{{^posts}}"
                  "<div class=\"text-center py-16 text-gray-400\">"
                    "<p class=\"text-xl\">No posts yet.</p>"
                    "<a href=\"{{url:new_post}}\" "
                       "class=\"text-blue-600 hover:underline mt-2 inline-block\">"
                      "Write your first post"
                    "</a>"
                  "</div>"
                "{{/posts}}"

              "{{/body}}"
            "{{/layout}}"
          })
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
              "insert into posts (title, body) "
              "values ({{title}}, {{body}});"
          }),
          redirect((u){"posts"})
        }}
      },

      // New post form
      {"new_post", "/posts/new",
        .get = {{
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<h1 class=\"text-3xl font-bold text-gray-900 mb-8\">"
                  "New Post"
                "</h1>"

                "<form method=\"post\" action=\"{{url:posts}}\">"
                  "{{> post_form}}"
                "</form>"

                "<a href=\"{{url:posts}}\" "
                   "class=\"text-gray-500 hover:text-gray-700 mt-4 inline-block\">"
                  "&larr; Back to posts"
                "</a>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      },

      // Single post: show, update, delete
      {"post", "/posts/:id",
        .get = {{
          find((d){
            .set = "post",
            .db  = "blog_db",
              "select id, title, body, created_at "
              "from posts "
              "where id = {{id}};"
          }),
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "{{#post}}"

                  "<article class=\"max-w-none\">"
                    "<h1 class=\"text-4xl font-bold text-gray-900 mb-2\">"
                      "{{title}}"
                    "</h1>"
                    "<p class=\"text-sm text-gray-400 mb-8\">"
                      "Published {{created_at}}"
                    "</p>"
                    "<div class=\"prose prose-lg text-gray-700 "
                                "whitespace-pre-line leading-relaxed\">"
                      "{{body}}"
                    "</div>"
                  "</article>"

                  "<div class=\"flex gap-4 mt-10 pt-6 border-t border-gray-200\">"
                    "<a href=\"{{url:edit_post:id}}\" "
                       "class=\"bg-gray-100 text-gray-700 px-4 py-2 "
                              "rounded-lg hover:bg-gray-200 transition\">"
                      "Edit"
                    "</a>"
                    "<form method=\"post\" action=\"{{url:post:id}}\" "
                          "class=\"inline\">"
                      "<input type=\"hidden\" name=\"http_method\" value=\"delete\">"
                      "<button type=\"submit\" "
                              "class=\"bg-red-50 text-red-600 px-4 py-2 "
                                     "rounded-lg hover:bg-red-100 transition\" "
                              "onclick=\"return confirm('Delete this post?')\">"
                        "Delete"
                      "</button>"
                    "</form>"
                    "<a href=\"{{url:posts}}\" "
                       "class=\"text-gray-500 hover:text-gray-700 px-4 py-2\">"
                      "&larr; Back"
                    "</a>"
                  "</div>"

                "{{/post}}"
              "{{/body}}"
            "{{/layout}}"
          })
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
              "update posts "
              "set title = {{title}}, body = {{body}} "
              "where id = {{id}};"
          }),
          redirect((u){"post"})
        }},

        .delete = {{
          query((d){
            .db = "blog_db",
              "delete from posts "
              "where id = {{id}};"
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
      },

      // Edit post form
      {"edit_post", "/posts/:id/edit",
        .get = {{
          find((d){
            .set = "post",
            .db  = "blog_db",
              "select id, title, body "
              "from posts "
              "where id = {{id}};"
          }),
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "{{#post}}"

                  "<h1 class=\"text-3xl font-bold text-gray-900 mb-8\">"
                    "Edit Post"
                  "</h1>"

                  "<form method=\"post\" action=\"{{url:post:id}}\">"
                    "<input type=\"hidden\" name=\"http_method\" value=\"patch\">"
                    "{{> post_form}}"
                  "</form>"

                  "<a href=\"{{url:post:id}}\" "
                     "class=\"text-gray-500 hover:text-gray-700 mt-4 "
                            "inline-block\">"
                    "&larr; Back to post"
                  "</a>"

                "{{/post}}"
              "{{/body}}"
            "{{/layout}}"
          })
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
      // Layout — the HTML shell every page inherits from
      {"layout",
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head>"
          "<meta charset=\"utf-8\">"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
          "<title>MACH Blog</title>"
          "{{> tailwind_script}}"
        "</head>"
        "<body class=\"bg-gray-50 min-h-screen\">"
          "<nav class=\"bg-white border-b border-gray-200\">"
            "<div class=\"max-w-3xl mx-auto px-6 py-4 "
                        "flex items-center justify-between\">"
              "<a href=\"{{url:posts}}\" "
                 "class=\"text-xl font-bold text-gray-900 "
                        "hover:text-blue-600 transition\">"
                "MACH Blog"
              "</a>"
              "<span class=\"text-xs text-gray-400\">"
                "built in 15 minutes"
              "</span>"
            "</div>"
          "</nav>"
          "<main class=\"max-w-3xl mx-auto px-6 py-10\">"
            "{{$body}}{{/body}}"
          "</main>"
          "<footer class=\"max-w-3xl mx-auto px-6 py-6 text-center "
                         "text-xs text-gray-300\">"
            "Powered by MACH"
          "</footer>"
        "</body>"
        "</html>"
      },

      // Partial: post card for the index listing
      {"post_card",
        "<article class=\"bg-white rounded-xl shadow-sm border border-gray-100 "
                        "p-6 mb-4 hover:shadow-md transition\">"
          "<a href=\"{{url:post:id}}\" class=\"block\">"
            "<h2 class=\"text-xl font-semibold text-gray-900 "
                        "hover:text-blue-600 transition\">"
              "{{title}}"
            "</h2>"
            "<p class=\"text-sm text-gray-400 mt-1\">"
              "{{created_at}}"
            "</p>"
            "<p class=\"text-gray-600 mt-3 line-clamp-2\">"
              "{{body}}"
            "</p>"
          "</a>"
        "</article>"
      },

      // Partial: reusable form fields for new + edit
      {"post_form",
        "<div class=\"space-y-6\">"
          "<div>"
            "<label for=\"title\" "
                   "class=\"block text-sm font-medium text-gray-700 mb-1\">"
              "Title"
            "</label>"
            "<input type=\"text\" id=\"title\" name=\"title\" "
                   "value=\"{{title}}\" "
                   "placeholder=\"Your post title\" "
                   "class=\"w-full px-4 py-2 border border-gray-300 rounded-lg "
                          "focus:ring-2 focus:ring-blue-500 focus:border-blue-500 "
                          "outline-none transition\">"
          "</div>"
          "<div>"
            "<label for=\"body\" "
                   "class=\"block text-sm font-medium text-gray-700 mb-1\">"
              "Body"
            "</label>"
            "<textarea id=\"body\" name=\"body\" rows=\"12\" "
                      "placeholder=\"Write your post...\" "
                      "class=\"w-full px-4 py-2 border border-gray-300 rounded-lg "
                             "focus:ring-2 focus:ring-blue-500 "
                             "focus:border-blue-500 outline-none transition "
                             "resize-y\">"
              "{{body}}"
            "</textarea>"
          "</div>"
          "<button type=\"submit\" "
                  "class=\"bg-blue-600 text-white px-6 py-2 rounded-lg "
                         "hover:bg-blue-700 transition font-medium\">"
            "Save Post"
          "</button>"
        "</div>"
      }
    },

    // ── Error Handling ─────────────────────────

    .errors = {
      {http_not_found, {
        render((r){
          "{{< layout}}"
            "{{$body}}"
              "<div class=\"text-center py-20\">"
                "<h1 class=\"text-6xl font-bold text-gray-200\">404</h1>"
                "<p class=\"text-gray-500 mt-4\">Post not found.</p>"
                "<a href=\"{{url:posts}}\" "
                   "class=\"text-blue-600 hover:underline mt-4 inline-block\">"
                  "&larr; Back to all posts"
                "</a>"
              "</div>"
            "{{/body}}"
          "{{/layout}}"
        })
      }},
      {http_bad_request, {
        render((r){
          .status = http_bad_request,
          "{{< layout}}"
            "{{$body}}"
              "<div class=\"text-center py-20\">"
                "<h1 class=\"text-4xl font-bold text-gray-200\">"
                  "Validation Error"
                "</h1>"
                "<p class=\"text-red-500 mt-4\">"
                  "Please check your input and try again."
                "</p>"
                "<a href=\"{{url:posts}}\" "
                   "class=\"text-blue-600 hover:underline mt-4 inline-block\">"
                  "&larr; Back to all posts"
                "</a>"
              "</div>"
            "{{/body}}"
          "{{/layout}}"
        })
      }}
    }

    },

    // ── Database ───────────────────────────────

    .databases = {{
      .engine  = sqlite_db,
      .name    = "blog_db",
      .connect = "file:blog.db?mode=rwc",
      .migrations = {
        "CREATE TABLE posts ("
          "id         INTEGER PRIMARY KEY AUTOINCREMENT,"
          "title      TEXT    NOT NULL,"
          "body       TEXT    NOT NULL,"
          "created_at TEXT    NOT NULL "
                      "DEFAULT (strftime('%Y-%m-%d %H:%M', 'now'))"
        ");"
      },
      .seeds = {
        "INSERT INTO posts (title, body) VALUES"
        "('Hello World', "
        " 'Welcome to the MACH blog! This post was created by the "
        "database seed. MACH is a high-performance, declarative web "
        "framework for C that lets you build full CRUD apps in a "
        "single file with zero boilerplate.'),"
        "('Why Declarative C?', "
        " 'Traditional C web development means manual memory management, "
        "string wrangling, and security pitfalls at every turn. MACH "
        "flips the script: you declare resources, pipelines, and "
        "templates. The runtime handles allocation, prepared statements, "
        "XSS escaping, and async I/O. You get the speed of C with the "
        "ergonomics of a modern framework.');"
      }
    }},

    // ── Modules ────────────────────────────────

    .modules = {sqlite, tailwind}
  };
}
