# MaCH Web Framework (closed beta)

MaCH (ie. Mustache and C Hypermedia, mach speed) is a web framework that is batteries included, fast, simple, no boilerplate, no ceremony, using [data oriented design](https://en.m.wikipedia.org/wiki/Data-oriented_design), [self-contained system architecture](https://scs-architecture.org/), and [locality of behavior](https://htmx.org/essays/locality-of-behaviour/), with a great developer experience. It aims to keep the performance of C with the ease of development of a modern web framework. No need to worry about dependencies, packages, compilations, or builds, just open text editor and write code and MaCH handles the rest.

## Table of Contents

- [MaCH Core Philosophy](#mach-core-philosophy)
  - [D.E.C.A.L Principles](#decal-durable-software-that-sticks-more-aposd-and-cupid-less-solid)
- [Architectural Primitives and Flow](#architectural-primitives-and-flow)
- [Why MaCH?](#why-mach)
  - [Performance](#performance)
  - [Standards Based](#standards-based)
  - [Modern Development Experience](#modern-development-experience)
  - [Universal Ecosystem Access](#universal-ecosystem-access)
- [Modules Included](#modules-included)
- [Quick Start](#quick-start)
- [Hello World Examples](#hello-world-examples)
  - [Hello (text)](#hello-text)
  - [Hello World (text)](#hello-world-text)
  - [Hello World (html)](#hello-world-html)
  - [Hello World (html w/defines)](#hello-world-html-wdefines)
- [Progressive Examples](#progressive-examples)
  - [0. Hello (Text)](#0-hello-text)
  - [1. Hello World (Text)](#1-hello-world-text)
  - [2. Hello World (HTML)](#2-hello-world-html)
  - [3. Single File Todo App](#3-single-file-todo-app)
  - [4. External Templates Todo](#4-external-templates-todo)
  - [5. Per-User Database Todo](#5-per-user-database-todo)
  - [6. Todo Using Session Auth Module](#6-todo-using-session-auth-module)
  - [7. Modular SCS Todo](#7-modular-scs-todo)
  - [8. Roundest Pokémon](#8-roundest-pok%C3%A9mon)
  - [9. Todo No Branching](#9-todo-no-branching)
- [Licensing](#licensing)

## MaCH Core Philosophy

### D.E.C.A.L: durable software that sticks (more [A.P.O.S.D](https://newsletter.pragmaticengineer.com/p/the-philosophy-of-software-design) and [C.U.P.I.D](https://dannorth.net/blog/cupid-for-joyful-coding/), less [S.O.L.I.D](https://en.m.wikipedia.org/wiki/SOLID))
- **Deep Interfaces** - Modules have deep, tall, and narrow interfaces, eg, session module is complex and does everything related to auth but exposes only a single auth middleware handler function
- **Encapsulation** - Modules internal workings are fully encapsulated, eg, all modules expose nothing to rest of system, if they want to communicate with other modules they have to do so at hypermedia layer and events like they were any other user
- **Cohesive** - Modules have a single, well-defined purpose or domain, eg session, todos, etc
- **Autonomous** - Modules are autonomous, operating as a fully self-contained, independent units, eg session should include everything about sessions and todos should include everything about todos, including routes, db, schema, logic, ui, etc
- **Locality** - Modules adhere to locality of behavior, keeping all related code close together, modules code should be co-located in same file/folder, eg all todos logic is in todos.c and all logic, db, templates, etc are in a folder called todos

## Architectural Primitives and Flow (DoD tranformation flow)
req => ctx => alter ctx => render ctx => resp
- requests establish a context (ctx) that includes things like cookies, fields, request url, etc
- functions make changes to the ctx, add/edit cookies, add data to ctx from db, etc
- current state of ctx gets rendered as html, text, json, etc, and sent as response

## Why MaCH?

### Performance
- **Native C performance** - maximum throughput with minimal resource usage
- **Fully async** - fully async and multi-threaded architecture that uses continuation passing style state machine to allow for a more synchronous programming style
- **Request-scoped memory management** - automatic cleanup without gc pauses
- **Zero garbage collection overhead** - predictable, consistent performance
- **Live events** - supports websockets and server side events
- **Hypermedia native** - modules for htmx, datastar, alpine ajax, etc
- **Static asset pipeline/serving** - fast serving of static assets like images, video, html, js, css, etc

### Standards Based
- Generate HTML server-side, enhance with hypermedia (HTMX, Alpine, Datastar)
- Write SQL directly - SQLite, Postgres, MySQL, DuckDB, MongoDB, Redis
- Use any JavaScript library - many included, or add your own
- Standard HTTP fetch, SMTP email, API integrations
- No proprietary patterns - just HTML, CSS, JS, and SQL

### Modern Development Experience
- **Docker-first** - consistent development and production environments with no worry about dependencies or building/compiling
- **No boilerplate** - low ceremony code
- **Simple modern C** - no pointers, no malloc/free, no macros, no buffer overflows
- **Fast automatic compilation + hmr** - fast interactive feedback loop
- **Automatic memory management** - no manual malloc/free in application code
- **Self-contained System architecture & Locality of Behavior** - features are fully self contained (config, urls, routing, middleware, handlers, db, queries, templates, etc) into a single file, no scattered file explosion, scale from single file hello world, to large scale applications using single file self contained features/modules, mach grows with you without rewrites
- **Middleware** - create middleware like login, sessions, authorization, etc
- **Automatic error handling** - errors captured and handled automatically
- **Application performance monitoring and error reporting** -  get apm and errors using open telemetry with automatic spanning and reporting for req/resp, db, fetch, render, tasks, emails, etc
- **Production debugging** - debug production issues locally using reproduced production state with automatic core dumps attached to error reports
- **Multiple database support** - sqlite, duckdb, postgres, mysql, mongodb, redis
- **Use multiple databases at same time** - mix and match db providers, have as many dbs as your app needs, per user db supported
- **Automatic database migrations** - create migrations and specify database version and migrations happen automatically at application startup
- **Orm** - not your typical orm, c isnt oo, write safe sql, injections/protections are included, queries create linked tables in memory for use in template autobinding, app logic, structs, etc
- **Object storage** - s3, r2, and other blob storage providers
- **Authentication/Authorization** - modules for most auth scenarios, primitives to create any auth system to suite your needs
- **Events** - Subscribe to events for cross module communication
- **Fetch** - http fetch, make api calls, etc
- **Email** - sending emails with smtp or api
- **Background tasks** - enqueue background tasks to be executed in thread pool or in separate worker processes
- **Scheduled tasks** - schedule background tasks to run
- **Comprehensive testing** - ctx is fully mockable/fuzzable turning all mach application functions into pure functions, unit (Criterion), E2E (Playwright), and fuzzing (libfuzzer)
- **Professional tooling** - full IDE support via LSP and integrated debugging
- **Includes common data structures** - tables, records, hashes, arraylist, strings, string views, slices, etc

### Universal Ecosystem Access
- **Direct C library integration** - access decades of mature, battle-tested libraries
- **Automatic memory cleanup for 3rd party libraries** - use any C library with request-scoped resource management via `ma_alloc` & `ma_defer_free`

## Modules included
- Htmx
- Datastar (full sse support)
- AlpineJs
- Alpine Ajax
- Instant Page
- ForesightJS
- TailwindCss
- DaisyUi
- Basecoat
- Wired Elements
- Web Awesome
- Font Awesome
- Google Fonts
- Sqlite
- DuckDb
- Postgres
- Mysql
- Mongodb
- Redis/Valkey
- S3/R2
- Session Auth
- 3rd Party Auth (google, meta, github, etc)
- Passkey Auth
- Saml Auth

## Quick Start

```bash
# Create development environment container
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml
docker compose up -d

# Initialize project
# initialization creates:
#   'public' folder for static assets
#   main.c hello world app
#   .clangd for lsp
docker compose exec app mach init

# Start development server
# open editor/ide
# write code
# save file
# see changes in browser instantly
# mach dev server does:
#   file watching
#   auto compilation
#   hot code reloading
#   hmr
docker compose exec app mach dev

# application info:
#   overview
#     routes
#     databases
#     middleware
#     events
#     subscriptions
#   modules composing the system
#     routes
#     exposed middleware
#     used middleware
#     used modules
#     exposed events
#     subscribed events
#     databases
docker compose exec app mach info
docker compose exec app mach info routes
docker compose exec app mach info middleware
docker compose exec app mach info events
docker compose exec app mach info databases
docker compose exec app mach info module {module_name}

# run debugger attached to running dev server process
docker compose exec app mach debug

# Run criterion unit tests
docker compose exec app mach unit  

# Run playwright e2e tests
docker compose exec app mach e2e

# Build optimize production docker image
#   push to any docker repository for easy deployment
docker compose exec app mach build
```

## Hello World Examples

### Hello (text)

```c
#include <mach.h>

ma_config mach(){
  return (ma_config) {.routes = {{ma_get, "/", .text = "hello"}}};
}
```

### Hello World (text)

```c
#include <mach.h>

ma_state hello(const ma_ctx ctx){
  auto name = ma_params_get(ctx, "name");
  if(!name) name = "World";
  auto const message = ma_str_printf(ctx, "Hello %s", name);
  ma_send(ctx, ma_text, ma_ok, message);
  return ma_complete;
}

ma_config mach(){
  return (ma_config) {.routes = {{ma_get, "/", .call = hello}}};
}
```

### Hello World (html)

```c
#include <mach.h>

const ma_str ts_hello =
  "<html>"
    "<head></head>"
    "<body>"
      "<p>Hello {{name}}</p>"
    "</body>"
  "</html>";

ma_state hello(const ma_ctx ctx){
  auto name = ma_params_get(ctx, "name");
  if(!name) name = "World";
  ma_ctx_set(ctx, "name", name);
  ma_render(ctx, ma_ok, "hello");
  return ma_complete;
}

ma_config mach(){
  return (ma_config) {
    .templates = {{"hello", ts_hello}},
    .routes = {{ma_get, "/", .call = hello}}
  };
}
```

### Hello World (html w/defines)

```c
#include <mach.h>
#define m_name "name"
#define m_hello "hello"

const ma_str ts_hello =
  "<html>"
    "<head></head>"
    "<body>"
      "<p>Hello {{" m_name "}}</p>"
    "</body>"
  "</html>";

ma_state hello(const ma_ctx ctx){
  auto name = ma_params_get(ctx, m_name);
  if(!name) name = "World";
  ma_ctx_set(ctx, m_name, name);
  ma_render(ctx, ma_ok, m_hello);
  return ma_complete;
}

ma_config mach(){
  return (ma_config) {
    .templates = {{m_hello, ts_hello}},
    .routes = {{ma_get, "/", .call = hello}}
  };
}
```

## Progressive Examples

This repository contains eight progressive examples demonstrating MaCH's capabilities, from simple hello world to complex modular applications.

### Example Progression

Each example builds upon the previous one, introducing new concepts and patterns:

#### [0. Hello Text](./00_hello_text/)
**Concepts**: Static routes, minimal configuration, zero-logic responses
- Absolute simplest MaCH application possible
- Static text responses without handler functions
- Route-level content definition
- Instant web server in 5 lines of code

#### [1. Hello World (Text)](./01_hello_world_text/)
**Concepts**: Basic handlers, context, field access, string operations
- Simplest possible MaCH application
- Plain text responses
- Query parameter handling
- Request-scoped memory management

#### [2. Hello World (HTML)](./02_hello_world_html/)
**Concepts**: Template system, HTML rendering, Mustache syntax
- Builds on: Text example fundamentals
- Adds: HTML templates with variable substitution
- Introduces template registration and rendering

#### [3. Single File Todo App](./03_todo_single_file/)
**Concepts**: Database operations, authentication, middleware, HTMX
- Builds on: Template system from HTML example
- Adds: SQLite integration, async database queries, session management, CRUD operations
- Complete application in single file demonstrating locality of behavior

#### [4. External Templates Todo](./04_todo/)
**Concepts**: External template files, hot reloading, developer experience
- Builds on: All functionality from single file todo
- Adds: Template files for better IDE support and team collaboration
- Same logic, improved developer workflow

#### [5. Per-User Database Todo](./05_todo_db_per_user/)
**Concepts**: Dynamic databases, data isolation, horizontal scaling
- Builds on: External template structure
- Adds: Individual databases per user, enhanced privacy, simplified queries
- Demonstrates advanced data architecture patterns

#### [6. Todo Using Session Auth Module](./06_todo_using_session_auth_module/)
**Concepts**: Built in MaCH modules, built in session auth module
- Builds on: All previous patterns
- Adds: Session auth module
- Shows MaCH has built in modules, eg. htmx, alpine_ajax, datastar, session_auth, etc

#### [7. Modular SCS Todo](./07_todo_scs/)
**Concepts**: Module architecture, configuration composition, team development
- Builds on: All previous patterns
- Adds: Self-contained modules, clean dependencies
- Demonstrates enterprise-scale application organization

#### [8. Roundest Pokémon](./08_roundest_pokemon/)
**Concepts**: Modern frontend, real-time voting, production UI, analytics
- Builds on: Modular architecture principles
- Adds: Tailwind CSS integration, advanced SQL analytics, handler transfers
- Professional, production-ready application

#### [9. Todo No Branching](./09_todo_no_branching/)
**Concepts**: No branching to check for ctx state instead jump to next state
- Builds on: All previous patterns
- Adds: Jump to next state manually instead of reusing main hander and checking ctx state

## Licensing

MaCH is dual-licensed under:
- **AGPL-3.0** - free for open source projects and internal company use
- **Commercial License** - available for proprietary applications and SaaS products
