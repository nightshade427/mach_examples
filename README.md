# MACH Web Platform

## Table of Contents
- [Overview](#overview)
- [Key Features](#key-features)
- [Hello World](#hello-world)
- [Quick Comparison](#quick-comparison)
- [Quick Start](#quick-start)
- [Guide](#guide)
  - [Configuration](#configuration)
  - [Registration Resolution](#registration-resolution)
  - [Routing](#routing)
  - [Templates](#templates)
  - [Middleware](#middleware)
  - [Databases](#databases)
  - [SSE & Realtime](#sse--realtime)
  - [Events](#events)
  - [Authentication](#authentication)
  - [Datastar](#datastar)
  - [HTMX](#htmx)
- [Architecture Overview](#architecture-overview)
  - [Why C?](#why-c)
  - [IDEAL Philosophy](#ideal-philosophy)
  - [Data Pipeline](#data-pipeline)
  - [Multi-Reactor & Coroutines](#multi-reactor--coroutines)
  - [Memory Management](#memory-management)
  - [Async Patterns](#async-patterns)
  - [Modules](#modules)
  - [Error Handling](#error-handling)
  - [Observability](#observability)
  - [AI](#ai)
- [API Reference](#api-reference)
  - [Types](#types)
  - [MACH API](#mach-api)
    - [Reading & Writing Context](#reading--writing-context)
    - [Strings](#strings)
    - [Responses](#responses)
    - [SSE](#sse)
    - [Events](#events-1)
    - [Database](#database)
    - [Tables & Records](#tables--records)
    - [Headers & Cookies](#headers--cookies)
    - [Errors](#errors)
    - [Memory](#memory)
  - [HTMX API](#htmx-api)
  - [Datastar API](#datastar-api)
- [Built Using](#built-using)

## Overview
MACH (Modern, Asynchronous, C, Hypermedia) is a batteries-included web platform that delivers the developer experience of a modern framework at the raw speed of C. No package managers, no build scripts, no toolchain setup. Launch the tui, write code, and MACH compiles, reloads, and serves automatically.

## Key Features
* **Performance:** Native C speed, zero garbage collection overhead, and fully asynchronous execution.
* **Modern C (C23):** Safe, simple C with automatic memory management, bounds checking, and type safety.
* **Docker First:** Zero host machine setup, dev/prod parity, databases, and caches.
* **Built-in Tooling:** ai assistance, hmr, apm, log aggregation, error reporting, and debugging.
* **Backend Capabilities:** Multiple simultaneous databases, database per tenant, eventing, first class sse support, background tasks, fetch api calls, send emails, authentication, and pdf generation.
* **Ecosystem:** Included modules for datastar, htmx, tailwind, daisyui, mustache, markdown, sqlite, postgres, s3, clerk, and stripe.

## Hello World
Three examples, simplest to most complete.

**Text Response**
```c
#include <mach.h>

config mach(){
  return (config) {.routes = {{"/", http_get, .txt = "hello"}}};
}
```

**Dynamic Text Response**
```c
#include <mach.h>

void hello(){
  auto name = get("name");
  if(!name) name = "World";
  assign("name", name);
  respond(http_ok, mime_txt, (b){.txt = format("Hello {{name}}")});
}

config mach(){
  return (config) {.routes = {{"/", http_get, .call = hello}}};
}
```

**Html Response With Sqlite**
```c
#include <mach.h>
#include <sqlite.h>
#include <mustache.h>

void hello(){
  assign("limit", "1");
  await_assign_db_query("greeting", (q){.db = "hello_db",
    "select name "
      "from greetings "
      "limit {{limit}};"
  });
  respond(http_ok, mime_html, (b){.engine = mustache,
    "<html>"
      "<body>"
        "{{#greeting}}"
          "<p>Hello {{name}}</p>"
        "{{/greeting}}"
      "</body>"
    "</html>"
  });
}

config mach(){
  return (config) {
    .routes = {
      {"/", http_get, .call = hello}
    },
    .includes = {
      sqlite_config,
      mustache_config
    },
    .databases = {{
      .engine = sqlite,
      .name = "hello_db",
      .connect = "file:hello.db?mode=rwc",
      .migrations = {
        "CREATE TABLE greetings ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "name TEXT NOT NULL"
        ");"
      },
      .seeds = {
        "INSERT INTO greetings(name)"
        "VALUES('World');"
      }
    }}
  };
}
```

## Quick Comparison
The same application built across five frameworks. A Pokemon voting app with a database, rankings page, and styled UI.

| Framework | Files | Lines of Code |
|---|---|---|
| **MACH (C)** | **3** | **161** |
| Rails (Ruby) | 82 | 1,012 |
| Phoenix (Elixir) | 48 | 1,542 |
| Next.js RSC (TypeScript) | 23 | 3,257 |
| Go GraphQL SPA (Go/TypeScript) | 32 | 5,534 |

Three files. One C file, two templates. You can read the entire application quickly.

## Quick Start
Download the compose file and bring up the dev environment. MACH handles file watching, compilation, and hot reloading automatically. Use the terminal ui for editing and ai assistance, or point your own editor at the project folder.

```bash
# Create development environment
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml

# Starts dev server (port 3000), log/telemetry/apm server (port 4000)
# Includes file watching, auto compilation, hot code reloading, and hmr
docker compose up

# Launches terminal ui: editor, ai, console
docker compose attach mach

# use mach console to run commands
# or from outside mach ui use:
#   docker compose exec mach {command}

# get application information
app_info
app_info routes
app_info middleware
app_info events
app_info databases
app_info module {module_name}

# run debugger attached to running dev server process
app_debug

# Build optimized production docker image
app_build

# Run criterion unit tests
unit_tests

# Run playwright e2e tests
e2e_tests
```

## Guide

### Configuration
Everything in MACH is declared in a `config` struct. Routes, middleware, databases, events, resources, and module dependencies are all registered in one place.

The config is self-documenting. Everything a module owns is visible in one place, no codebase hunting.

```c
config mach(){
  return (config) {
    .includes = { ... },
    .uses = { ... },
    .databases = { ... },
    .middleware = { ... },
    .resources = { ... },
    .routes = { ... },
    .publishes = { ... },
    .subscribes = { ... }
  };
}
```

`app_info` aggregates all modules into a complete application view. Developers and ai can use this to understand the full topology of the running application across all modules.

```bash
app_info
app_info routes
app_info middleware
app_info events
app_info databases
app_info module {module_name}
```

### Registration Resolution
Resources, databases, middleware, and routes are all registered by name. MACH resolves them top-down, meaning registrations in `main.c` take priority over those in included modules, which take priority over their own dependencies. Modules ship with sensible defaults, but anything can be overridden by registering the same name higher up the chain.

No forking, no subclassing, no patching. The first registration wins.

```
main.c → todos.c → session_auth.c
```

```c
// override session_auth's default login template
{"login", (resource){
  #embed "my_login.mustache.html"
}}

// point session_auth's SQLite user database at Postgres
{.name = "users", .engine = postgres, .connect = "postgresql://localhost/myapp"}

// replace a middleware implementation
{"logged_in", .before = my_custom_logged_in}
```

### Routing
Each route specifies a path, method, and how to respond. Inline responses, handler functions, template renders, sse channels, and error handlers are all declared the same way.

```c
.routes = {
  // inline responses
  {"/", http_get, .txt = "hello"},
  {"/", http_get, .render = "home"},
  {"/app.js", http_get, .js = "console.log('hello')"},

  // handler function
  {"/todos", http_get, .call = list_todos},

  // render a template directly, no handler needed
  {"/about", http_get, .render = "about"},

  // named route with handler and middleware
  {"/todos", http_get, "todos", .call = list_todos, .uses = {"logged_in"}},

  // route parameter
  {"/todos/:id", http_patch, .call = update_todo},

  // sse channel
  {"/todos_sse", http_sse, "todos_sse", .client = "todos/:user_id"},

  // error handlers
  {.status = http_not_found, .render = "404"},
  {.status = http_error, .render = "5xx"}
}
```

**Supported Methods**

`http_get`, `http_post`, `http_put`, `http_patch`, `http_delete`, `http_head`, `http_options`, `http_sse`

**Named Routes**

Giving a route a name registers it for reverse url lookup. Templates can reference named routes without hardcoding paths.

```html
<a href='{{:url:todos}}'>todos</a>
<form action='{{:url:create_session}}' method='post'>
```

If a path changes, the name stays the same and links update automatically.

**Route Parameters**

Parameters declared with `:` in the path are available in the handler via `get()`.

```c
{"/todos/:id", http_patch, .call = update_todo}

void update_todo(){
  auto const id = get("id");
  ...
}
```

### Templates
MACH ships with two template engines, `mustache` and `mdm`. Additional engines can be added as modules.

**Mustache**

`mustache` renders `.mustache.html` templates. MACH supports the full Mustache spec with the exception of lambdas and dot notation. See the [Mustache spec](https://mustache.github.io/mustache.5.html) for full usage.

**MDM**

`mdm` renders `.mdm.md` templates. MDM is GitHub Flavored Markdown with Mustache templating. See the [GitHub Markdown spec](https://github.github.com/gfm/) for Markdown usage.

**Configuration**

```c
#include <mustache.h>
#include <mdm.h>

config mach(){
  return (config) {
    .includes = {mustache_config, mdm_config},
    ...
  };
}
```

Templates are registered by name in the resources config. Resources resolve html templates, sql queries, layouts, and partials all by name.

Templates can be embedded from external files using `#embed`, or declared as string literals inline.

```c
.resources = {
  {"layout", (resource){
    #embed "layout.mustache.html"
  }},
  {"home", (resource){
    #embed "home.mustache.html"
  }},
  {"todo", (resource){
    #embed "todo.mustache.html"
  }},
  {"about", (resource){
    #embed "about.mdm.md"
  }},
  {"flash", "<p>{{message}}</p>"},
  {"spinner", "<div class='spinner'></div>"}
}
```

**MACH Template Commands**

MACH has commands that follow the format `:command:param1:param2:etc`.

`{{:url:route_name}}` outputs the path for a named route.

```html
<a href="{{:url:todos}}">todos</a>
<form action="{{:url:create_session}}" method="post">
```

`{{:raw:key}}` outputs a context value as unescaped html.

`{{:precision:key:decimal_places}}` outputs a number formatted to a given number of decimal places.

### Middleware
Middleware is just a handler, read from the context, set values, or short-circuit the request, just like any other handler.

Middleware is registered in the configuration section with a name, a `.before` and/or `.after` handler, and applied to routes via `.uses`.

```c
.middleware = {
  {"session", .before = session},
  {"logged_in", .before = logged_in},
  {"pretty", .after = pretty},
  {"compression", .before = deflate, .after = inflate}
}
```

Apply globally to all routes in the module.
```c
config mach(){
  return (config) {
    .uses = {"session"},
    ...
  };
}
```

Apply to a specific route.
```c
{"/session", http_delete, .call = logout, .uses = {"logged_in"}}
```

### Databases
MACH supports multiple simultaneous databases per application, including per-tenant databases. Each database is declared in the config with a name, engine, and connection string. Modules can declare their own databases, keeping data ownership aligned with module boundaries.

The connection string supports Mustache syntax and resolves values from the context at connection time. Any context value including session values can be used.

```c
// shared database
.databases = {
  {.name = "app_db", .engine = postgres, .connect = "postgresql://localhost/myapp"},
  {.name = "cache_db", .engine = sqlite, .connect = "file:cache.db?mode=rwc"}
}

// per-user database
.databases = {
  {.name = "todos_db", .engine = sqlite, .connect = "file:{{user_id}}_todos.db?mode=rwc"}
}
```

**Migrations & Seeds**

Migrations and seeds are declared as inline SQL strings. They run in order on startup.

MACH tracks migration and seed state in a `mach_schema_meta` table inside each database, storing the version and whether the database has been seeded. MACH also outputs a schema that ai and other tooling can use.

```c
.databases = {
  {.name = "app_db", .engine = sqlite, .connect = "file:app.db?mode=rwc",
    .migrations = {
      "CREATE TABLE IF NOT EXISTS users ( "
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE "
      ");"
    },
    .seeds = {
      "INSERT INTO users(username) "
      "VALUES('admin');"
    }
  }
}
```

**Querying**

Queries use the `q` struct, specifying the database and query string. Sql can be inline as the first positional field, or referenced by name with `.resource`. Query parameters are interpolated from the context using mustache syntax.

`await_assign_db_query` expects multiple rows and sets the result as a table on the context.

```c
assign("uid", uid);
await_assign_db_query("todos", (q){.db = "app_db",
  "select * from todos where user_id = {{uid}};"
});
```

`await_assign_db_find` expects a single row and sets the result as a record on the context. If no record is found, it adds a not found error to the context, this can be caught and handled in the handler, or left to bubble up to the `http_not_found` route handler automatically.

```c
assign("tid", tid);
await_assign_db_find("todo", (q){.db = "app_db",
  "select * from todos where id = {{tid}};"
});
```

There are also non-await versions that let you queue up several queries and execute them concurrently.

```c
assign("bid", bid);
assign_db_find("blog", (q){.db = "app_db",
  "select * from blogs where id = {{bid}};"
});
assign_db_query("comments", (q){.db = "app_db",
  "select * from comments where blog_id = {{bid}};"
});
await();
```

`db_query` and `db_find` execute queries without assigning results to the context. Useful for inserts, updates, and deletes.

```c
assign("tid", tid);
assign("title", title);
db_query((q){.db = "app_db",
  "update todos set title = {{title}} where id = {{tid}};"
});
db_find((q){.db = "app_db",
  "delete from todos where id = {{tid}};"
});
await();
```

Queries can also reference named sql templates from resources, keeping sql in dedicated files.

```sql
-- get_todos.sql
select id, title, finished
from todos
where user_id = {{user_id}};
```

```c
await_assign_db_query("todos", (q){.db = "app_db", .resource = "get_todos"});
```

Register sql templates the same way as html templates.

```c
.resources = {
  {"get_todos", (resource){
    #embed "get_todos.sql"
  }}
}
```

### SSE & Realtime
MACH has first-class sse support. A dedicated `http_sse` route type establishes a named client channel, keeping a persistent connection open to the browser.

```c
{"/todos_sse", http_sse, "todos_sse", .client = "todos/:user_id", .uses = {"logged_in"}}
```

`.client` defines the channel identifier. Because sse channels are identified by name, any handler, including event handlers from other modules, can push updates to a connected client.

Handlers push events to a channel using the sse response functions.

```c
auto const client = format("todos/{{user_id}}");

respond_sse_data(client, body);    // send a data event
respond_sse_event(client, body);   // send a named event
respond_sse_comment(client, body); // send a comment
```

### Events
Modules communicate across boundaries through events. A module declares what it publishes and what it subscribes to directly in the config, making inter-module contracts visible at a glance.

```c
.publishes = {
  {"order",
    .provides = {
      "user",
      "cart"
    }
  }
},
.subscribes = {
  {"shipped", .call = on_shipped}
}
```

`.provides` declares what keys the publishing module will set on the context when the event is published.

A handler publishes an event with `publish`.

```c
void create_order(){
  assign("user", user);
  assign("cart", cart);
  publish("order");
}
```

The subscriber runs when the event is published.

```c
void on_shipped(){
  auto const user = get("user");
  auto const cart = get("cart");
}
```

### Authentication
MACH ships with `session_auth` module. It provides a users database with migrations, password hashing with salts, session and logged_in middleware, login/register routes, and default templates.

Include it and apply the middleware:

```c
#include <session_auth.h>

config todos_config(){
  return (config) {
    "todos",
    .uses = {"session", "logged_in"},
    .includes = {session_auth_config},
    ...
  };
}
```

`session` reads the `user_id` cookie, queries the users database, and sets the `user` record on the context. `logged_in` redirects unauthenticated requests to the login page. Apply `session` globally to make user data available everywhere, and `logged_in` to routes or modules that require authentication.

Handlers access the current user through the context:

```c
auto const uid = get("user_id");
auto const user = get("user");
auto const name = record_get(table_get(user, 0), "first_name");
```

Because of registration resolution you can change any of the module defaults.

```c
// use Postgres instead of the default SQLite
{.name = "users", .engine = postgres, .connect = "postgresql://localhost/myapp"}

// replace the default login template
{"login", (resource){
  #embed "my_login.mustache.html"
}}

// replace the logged_in middleware
{"logged_in", .before = my_custom_logged_in}
```

### Datastar
MACH ships with `datastar` module. It provides a partial you can include in your templates. No cdn, no npm, no external dependencies.

Include it in the config.

```c
#include <datastar.h>

config mach(){
  return (config) {
    .includes = {datastar_config},
    ...
  };
}
```

Add the script to a template using the partial.

```html
<head>
  {{> datastar_script}}
</head>
```

Use `datastar()` to check if the current request is a Datastar request.

```c
if(datastar()){
  respond_ds_elements((d){
    .mode = mode_inner,
    .target = "todos",
    .elements = (b){.resource = "todos"}
  });
} else {
  respond(http_ok, mime_html, (b){.resource = "todos"});
}
```

Datastar supports merge modes that control how content is applied to the dom.

`mode_outer`, `mode_inner`, `mode_replace`, `mode_prepend`, `mode_append`, `mode_before`, `mode_after`, `mode_remove`

When `.client` is omitted, `respond_ds_elements` responds directly to the requesting client. When `.client` is set, it pushes to a named sse channel, allowing any handler or event subscriber to update any connected client.

```c
void create_todo(){
  // ... insert query ...
  respond_ds_elements((d){
    .client = client,
    .mode = mode_prepend,
    .target = "todos",
    .elements = (b){.resource = "todo"}
  });
}

void update_todo(){
  // ... update query ...
  respond_ds_elements((d){
    .client = client,
    .mode = mode_replace,
    .target = todo,
    .elements = (b){.resource = "todo"}
  });
}

void delete_todo(){
  // ... delete query ...
  respond_ds_elements((d){
    .client = client,
    .mode = mode_remove,
    .target = todo
  });
}
```

### HTMX
MACH ships with `htmx` module. It provides a partial you can include in your templates. No cdn, no npm, no external dependencies.

Include it in the config.

```c
#include <htmx.h>

config mach(){
  return (config) {
    .includes = {htmx_config},
    ...
  };
}
```

Add the script to a template using the partial.

```html
<head>
  {{> htmx_script}}
</head>
```

Use `htmx()` to check if the current request is an HTMX request.

```c
if(htmx()){
  respond(http_ok, mime_html, (b){.resource = "todo"});
} else {
  respond(http_ok, mime_html, (b){.resource = "todos"});
}
```

## Architecture Overview

### Why C?
C is what makes MACH's simplicity possible without trade-offs. The API is high-level and ergonomic. The runtime is bare metal with no garbage collector, no interpreter, and no VM. The path from handler to response is direct. No layers you didn't put there, no work you didn't ask for.

### IDEAL Philosophy
* **Interfaces are Deep:** Modules expose a small, simple interface backed by deep functionality.
* **Domain Centered:** Each module owns exactly one domain. One responsibility, no cross-cutting concerns.
* **Encapsulated:** Internal mechanics remain private. Modules do not leak state and communicate only through the hypermedia layer or events.
* **Autonomous:** Modules are fully self-contained units including their own routes, handlers, databases, business logic, events, ui templates, middleware.
* **Local:** All related code is co-located in a single file or folder to eliminate file explosion and codebase hunting.

### Data Pipeline
MACH models applications as a data transformation pipeline, inspired by data oriented design and self contained systems.

* **Input:** Creates a data context that acts as the single source of truth for the request.
* **Processing:** Update the context by executing asynchronous queries, making api calls, triggering events, running business logic, etc.
* **Output:** Renders the context into html, json, or sse.

Everything in MACH is a string. http is text, sql queries are text, json is text, html is text. When you need to interpret a value as anything other than string, you cast it yourself. You know your data better than the framework.

### Multi-Reactor & Coroutines
MACH lets you write simple, sequential code that runs with full multi-threaded, asynchronous performance underneath.

Each cpu core runs an independent reactor event loop. Application logic runs in coroutine handlers that yield control during async operations, allowing the reactor to process other requests concurrently. The reactor handles all i/o including network, databases, and files, while the handler focuses purely on application logic. Reactors communicate via lock free messaging for sse and events.

The result is a runtime that scales across all available cores without threads, locks, or callbacks in application code.

### Memory Management
MACH uses an arena pool allocator per reactor, which is pre-allocated at startup and fully managed by the system.

Each arena is split into two distinct regions:

1. The Stack: reserved for the coroutine, storing local variables and execution state for the handler function.
2. The Context: reserved for the context, storing all request data, including database results, fetched content, rendered templates, and queued async tasks.

### Async Patterns
MACH manages concurrency through a task queue system with two primary patterns:

1. Queue Only: methods without a prefix (e.g., `db_query`, `assign_db_query`) add a task to the queue without executing it. Returns instantly, allowing multiple tasks to be batched. Calling `await()` runs all queued tasks concurrently. Returning from a handler automatically executes all queued tasks.

2. Queue & Wait: methods prefixed with `await_` (e.g., `await_assign_db_query`, `await_assign_db_find`) queue a task and immediately wait for it to finish.

### Modules
Modules are self-contained systems. Each module owns its routes, handlers, databases, migrations, seeds, middleware, and resources, all co-located in a single file or folder. Modules communicate only through the hypermedia layer or events. This makes MACH suitable for large codebases where teams own individual modules (eg. billing, inventory, shipping, cart), with clean boundaries enforced by the framework.

A module, like an application, only need expose a method that returns a config struct.

```
.
├── public
│   └── favicon.png
├── static
│   ├── layout.mustache.html
│   ├── home.mustache.html
│   ├── about.mustache.html
│   ├── 404.mustache.html
│   └── 5xx.mustache.html
├── todos
│   ├── todos.c
│   ├── todos.mustache.html
│   └── todo.mustache.html
├── billing
│   ├── billing.c
│   └── billing.mustache.html
└── main.c
```

```c
// todos/todos.c
config todos_config(){
  return (config) {
    "todos",
    .databases = { ... },
    .resources = { ... },
    .routes = { ... }
  };
}
```

Including a module in your app:
```c
// main.c
#include "todos/todos.c"

config mach(){
  return (config) {
    .includes = {todos_config, ...}
  };
}
```

### Error Handling
When something goes wrong, whether a query returns no rows, a database connection fails, or a handler calls `error()`, MACH adds an error record to the context and marks it as errored. Handlers can check for this with `has()` and respond accordingly. If the error is not handled, MACH matches it to an error route in the config and defers to that route's template or handler. If there are no error handlers registered it falls back to built in error handlers, MACH never fails to handle an error. This keeps error handling declarative and co-located with routing, with no try/catch blocks or exception hierarchies.

```c
// check for an error in a handler
if(has("error")){
  // handle it yourself
}

// or let it bubble up to the error route
{.status = http_not_found, .render = "404"},
{.status = http_error, .render = "5xx"}
```

### Observability
Telemetry, logging, spans, and error reporting are built into MACH. When a request is handled, MACH emits spans. When an error hits the context, MACH reports it. When a query runs slow, you see it immediately.

The dev environment includes a telemetry server on port 4000 that aggregates logs, traces, and errors in real time. Developers can see exactly where time is being spent and where things are going wrong with zero instrumentation code. In production, the telemetry destination is configurable so you can send to your own infrastructure. MACH uses opentelemetry, so any compatible backend works out of the box.

### AI
MACH includes an ai assistant in the tui. It knows MACH's patterns, the IDEAL philosophy, and can write handlers, queries, migrations, and templates.

The tui pairs the chat with an editor where generated code is visible immediately. Developers can accept it, edit it, or ignore the ai entirely and write everything by hand.

## API Reference

### Types

**Enums**

| Type | Values |
|---|---|
| `http_method` | `http_get`, `http_sse`, `http_put`, `http_post`, `http_head`, `http_patch`, `http_delete`, `http_options` |
| `http_status` | `http_ok` (200), `http_created` (201), `http_redirect` (301), `http_bad_request` (400), `http_not_authorized` (401), `http_not_found` (404), `http_error` (500) |
| `mime_type` | `mime_js`, `mime_txt`, `mime_sse`, `mime_html`, `mime_json` |

**Structs**

| Type | Fields | Description |
|---|---|---|
| `b` | `.resource`, `.engine`, `.txt` | Response body. Use `.resource` for named resources, `.engine` to specify the template engine, `.txt` for inline text. |
| `q` | `.db`, `.resource`, query string | Database query. `.db` is the database name. Use `.resource` for a named sql resource or the query string as the first positional field for inline sql. |

### MACH API
MACH core function reference.

#### Reading & Writing Context
| Function | Description |
|---|---|
| `get(key)` | Reads a value from the context. Pulls from all sources: query params, route params, form data, request body, and session. Returns nullptr if the key is not found. |
| `assign(key, value)` | Writes a value to the context, making it available to templates and subsequent handlers. |
| `has(key)` | Returns true if a key exists in the context. |

```c
auto const uid = get("user_id");
auto const name = get("name");
if(!has("name")) assign("name", "World");
```

#### Strings
| Function | Description |
|---|---|
| `format(format)` | Formats a string using mustache-style interpolation from the context. Memory is managed by the arena allocator, no manual free needed. |

```c
assign("name", name);
assign("uid", uid);
auto const msg = format("Hello {{name}}");
auto const path = format("file:{{uid}}.db?mode=rwc");
```

#### Responses
| Function | Description |
|---|---|
| `respond(http_status, mime_type, body)` | Sends a response. The `b` body struct supports `.resource` (render a named resource), `.engine` (specify the template engine), and `.txt` (inline text content). |
| `redirect(route_name)` | Redirects to a named route. |
| `transfer(handler)` | Transfers execution to another handler within the same request, no http round trip. |
| `await()` | Runs all queued async tasks concurrently and resumes handler when all are completed. |

#### SSE
| Function | Description |
|---|---|
| `respond_sse_data(client_name, body)` | Sends a data event to a named sse channel. |
| `respond_sse_event(client_name, body)` | Sends a named event to a named sse channel. |
| `respond_sse_comment(client_name, body)` | Sends a comment to a named sse channel. |

```c
auto const client = format("todos/{{user_id}}");
respond_sse_data(client, "some data");
respond_sse_event(client, "some event");
```

#### Events
| Function | Description |
|---|---|
| `publish(event_name)` | Publishes an event to all subscribers. |

#### Database
Queries use the `q` struct with `.db` for the database name. Sql can be inline as the first positional field, or referenced by name with `.resource`. Parameters are interpolated from the context using mustache syntax.

| Function | Description |
|---|---|
| `assign_db_query(key, query)` | Queues a multi-row query without waiting. Result is set as a table on the context. |
| `await_assign_db_query(key, query)` | Queues and immediately executes a multi-row query, setting the result as a table on the context. |
| `assign_db_find(key, query)` | Queues a single-row query without waiting. Result is set as a record on the context. |
| `await_assign_db_find(key, query)` | Queues and immediately executes a single-row query, setting the result as a record on the context. Adds a not found error to the context if no record is found. |
| `db_query(query)` | Queues a multi-row query without waiting. Result is not assigned to the context. |
| `db_find(query)` | Queues a single-row query without waiting. Result is not assigned to the context. |

```c
await_assign_db_query("todos", (q){.db = "app_db", "select * from todos where user_id = {{uid}};"});
await_assign_db_find("todo", (q){.db = "app_db", "select * from todos where id = {{tid}};"});
```

#### Tables & Records
Query results are returned as tables (multiple rows) or records (single row) on the context. They can also be constructed manually.

| Function | Description |
|---|---|
| `table_get(table, index)` | Gets a record from a table by index. |
| `table_count(table)` | Returns the number of records in a table. |
| `table_add(table, record)` | Adds a record to a table. |
| `table_remove(table, record)` | Removes a record from a table. |
| `table_remove_at(table, index)` | Removes a record by index. |
| `table_nest(base, nest, new_base_field, base_field, nest_field)` | Joins two tables by key, nesting the second table's records into the first. |
| `table_new()` | Creates an empty table. |
| `record_get(record, key)` | Gets a field value from a record. |
| `record_assign(record, key, value)` | Sets a field value on a record. |
| `record_remove(record, key)` | Removes a field from a record. |
| `record_new()` | Creates an empty record. |

```c
auto const t = get("challengers");
auto const p0 = table_get(t, 0);
auto const p1 = table_get(t, 1);
record_assign(p0, "opponent_id", record_get(p1, "id"));
```

#### Headers & Cookies
| Function | Description |
|---|---|
| `headers_assign(key, value)` | Sets a response header. |
| `cookies_assign(key, value)` | Sets a response cookie. |

#### Errors
| Function | Description |
|---|---|
| `error(message)` | Adds an error to the context. Unhandled errors bubble up to the `http_error` route handler. |

#### Memory
| Function | Description |
|---|---|
| `allocate(bytes)` | Allocates memory from the arena. Freed automatically at the end of the request. |
| `defer_free(ptr)` | Registers a pointer for deferred cleanup at the end of the request, for memory allocated outside the arena such as from a C library. |

### HTMX API
| Function | Description |
|---|---|
| `htmx()` | Returns true if the current request is an HTMX request. |

### Datastar API
| Function | Description |
|---|---|
| `datastar()` | Returns true if the current request is a Datastar request. |
| `respond_ds_elements(options)` | Sends a dom update. The `d` struct supports `.client` (sse channel, omit for direct response), `.mode` (merge mode), `.target` (dom element), and `.elements` (a `b` body with template content). |
| `respond_ds_signals(options)` | Sends a signal update. The `d` struct supports `.client` (sse channel, omit for direct response), `.mode` (merge mode), and `.json` (signal data). |

## Built Using
MACH is built with:

[libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) · [libuv](https://libuv.org/) · [libcurl](https://curl.se/) · [minicoro](https://github.com/edubart/minicoro) · [mustach](https://gitlab.com/jobol/mustach) · [md4c](https://github.com/mity/md4c) · [opentelemetry-c](https://github.com/dorsal-lab/opentelemetry-c)

Core modules:

[Datastar](https://data-star.dev) · [HTMX](https://htmx.org) · [sqlite](https://sqlite.org) · [Litestream](https://litestream.io)

Tools:

[Fresh](https://getfresh.dev/) · [OpenCode](https://opencode.ai) · [Docker](https://docker.com)

