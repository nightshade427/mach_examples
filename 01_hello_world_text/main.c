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
