# MaCH Web Framework (closed beta)

MaCH (ie. Mustache and C Hypermedia, mach speed) is a web framework that is batteries included, fast, simple, no boilerplate, no ceremony, using [data oriented design](https://en.m.wikipedia.org/wiki/Data-oriented_design), [self-contained system architecture](https://scs-architecture.org/), and [locality of behavior](https://htmx.org/essays/locality-of-behaviour/), with a great developer experience. It aims to keep the performance of C with the ease of development of a modern web framework. No need to worry about dependencies, packages, compilations, or builds, just open text editor and write code and MaCH handles the rest.

## Table of Contents

- [MaCH Core Philosophy](#mach-core-philosophy)
  - [D.E.C.A.L Principles](#decal-durable-software-that-sticks-more-aposd-and-cupid-less-solid)
- [Architectural Primitives and Flow](#architectural-primitives-and-flow)
- [Why MaCH?](#why-mach)
  - [Performance](#performance)
  - [Modern Development Experience](#modern-development-experience)
  - [Universal Ecosystem Access](#universal-ecosystem-access)
- [Modules Included](#modules-included)
- [Quick Start](#quick-start)

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
- Native C performance
- Fully async
- Zero garbage collection overhead
- Live events
- Hypermedia native
- Static asset pipeline/serving

### Modern Development Experience
- **Docker-first** - consistent development and production environments with no worry about dependencies or building/compiling
- **AI** - use the included ai to help write, sql queries, handlers, etc
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
- **Pdf Generation** - use Typst templates to generate pdfs
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
- Typst
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
# Create development environment
mkdir myapp && cd myapp
wget https://docker.nightshadecoder.dev/mach/compose.yml
docker compose up -d

# Initialize project
# Starts development server (default pprt 3000)
# Starts log/telemetry/apm server (default port 4000)
# Launches mach terminal ui
# Terminal ui includes: editor, ai, console
#   write code in editor or ask ai to write code for you
#   you can use your own editor as well to edit the files locally
#   see changes in browser instantly
# mach dev server does:
#   file watching
#   auto compilation
#   hot code reloading
#   hmr
docker compose exec mach launch

# use mach console to run commands
# or from outside mach ui use:
#   docker compose exec mach {command}

# mach_info:
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
app_info
app_info routes
app_info middleware
app_info events
app_info databases
app_info module {module_name}

# run debugger attached to running dev server process
app_debug

# Build optimize production docker image
app_build

# Run criterion unit tests
unit_tests

# Run playwright e2e tests
e2e_tests
```
