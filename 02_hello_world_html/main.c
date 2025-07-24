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
