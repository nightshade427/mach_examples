#include <mach.h>

config mach() {
  return (config) {
    .resources = {{
      {"home", "/",
        .get = {{
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<div class='card'>"
                  "<h1>Weather Lookup</h1>"
                  "<p class='sub'>Enter a US zip code to get the current forecast</p>"
                  "<form action='{{url:weather}}' method='get'>"
                    "<div class='form-row'>"
                      "<input type='text' name='zipcode' placeholder='e.g. 90210' "
                        "maxlength='5' pattern='[0-9]{5}' required />"
                      "<button type='submit'>Search</button>"
                    "</div>"
                  "</form>"
                "</div>"
              "{{/body}}"
            "{{/layout}}"
          })
        }}
      },

      {"weather", "/weather",
        .get = {{
          validate((v){
            .name = "zipcode",
            .validation = "^[0-9]{5}$",
            .message = "Zip code must be exactly 5 digits"
          }),
          fetch((f){
            .set = "weather",
            .url = "https://wttr.in/{{zipcode}}?format=j1",
          }),
          render((r){
            "{{< layout}}"
              "{{$body}}"
                "<div class='card'>"
                  "<h1>Current Weather</h1>"
                  "{{#nearest_area}}"
                    "<p class='location'>{{areaName}} &mdash; {{region}}, {{country}}</p>"
                  "{{/nearest_area}}"
                  "{{#current_condition}}"
                    "<div class='temp-main'>{{temp_F}}&deg;F</div>"
                    "<div class='desc'>{{weatherDesc}}</div>"
                    "<div class='details'>"
                      "<div class='detail'>"
                        "<div class='label'>Feels Like</div>"
                        "<div class='value'>{{FeelsLikeF}}&deg;F</div>"
                      "</div>"
                      "<div class='detail'>"
                        "<div class='label'>Humidity</div>"
                        "<div class='value'>{{humidity}}%</div>"
                      "</div>"
                      "<div class='detail'>"
                        "<div class='label'>Wind</div>"
                        "<div class='value'>{{windspeedMiles}} mph</div>"
                      "</div>"
                      "<div class='detail'>"
                        "<div class='label'>UV Index</div>"
                        "<div class='value'>{{uvIndex}}</div>"
                      "</div>"
                    "</div>"
                  "{{/current_condition}}"
                  "<a class='back' href='{{url:home}}'>&larr; Search another zip code</a>"
                "</div>"
              "{{/body}}"
            "{{/layout}}"
          })
        }},

        .errors = {
          {http_bad_request, {
            render((r){
              "{{< layout}}"
                "{{$body}}"
                  "<div class='card center'>"
                    "<h1 class='error-icon'>Oops</h1>"
                    "<p class='error-msg'>Invalid zip code. Please enter exactly 5 digits.</p>"
                    "<a class='back' href='{{url:home}}'>&larr; Try again</a>"
                  "</div>"
                "{{/body}}"
              "{{/layout}}"
            })
          }}
        }
      }
    }},

    .context = {
      {"layout",
        "<html>"
        "<head>"
          "<title>Weather App</title>"
          "<style>"
            "* { margin: 0; padding: 0; box-sizing: border-box; }"
            "body {"
              "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;"
              "background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);"
              "min-height: 100vh;"
              "display: flex;"
              "justify-content: center;"
              "align-items: center;"
              "color: #e0e0e0;"
            "}"
            ".card {"
              "background: rgba(255,255,255,0.08);"
              "backdrop-filter: blur(12px);"
              "border-radius: 16px;"
              "padding: 48px;"
              "width: 500px;"
              "box-shadow: 0 8px 32px rgba(0,0,0,0.3);"
              "border: 1px solid rgba(255,255,255,0.1);"
            "}"
            ".card.center { text-align: center; }"
            "h1 { font-size: 24px; margin-bottom: 4px; }"
            ".sub { color: #9ab; margin-bottom: 32px; font-size: 14px; }"
            ".location { color: #9ab; font-size: 14px; margin-bottom: 32px; }"
            ".form-row { display: flex; gap: 12px; }"
            "input {"
              "flex: 1;"
              "padding: 14px 18px;"
              "border-radius: 10px;"
              "border: 1px solid rgba(255,255,255,0.15);"
              "background: rgba(255,255,255,0.06);"
              "color: #fff;"
              "font-size: 16px;"
              "outline: none;"
            "}"
            "input::placeholder { color: #789; }"
            "input:focus { border-color: #5ba3d9; }"
            "button {"
              "padding: 14px 28px;"
              "border-radius: 10px;"
              "border: none;"
              "background: #5ba3d9;"
              "color: #fff;"
              "font-size: 16px;"
              "font-weight: 600;"
              "cursor: pointer;"
            "}"
            "button:hover { background: #4a8ec4; }"
            ".temp-main {"
              "font-size: 72px;"
              "font-weight: 700;"
              "line-height: 1;"
              "margin-bottom: 4px;"
            "}"
            ".desc {"
              "font-size: 18px;"
              "color: #acd;"
              "margin-bottom: 32px;"
            "}"
            ".details {"
              "display: grid;"
              "grid-template-columns: 1fr 1fr;"
              "gap: 16px;"
            "}"
            ".detail {"
              "background: rgba(255,255,255,0.05);"
              "border-radius: 10px;"
              "padding: 16px;"
            "}"
            ".detail .label { font-size: 12px; color: #789; text-transform: uppercase; }"
            ".detail .value { font-size: 20px; font-weight: 600; margin-top: 4px; }"
            ".back {"
              "display: inline-block;"
              "margin-top: 32px;"
              "color: #5ba3d9;"
              "text-decoration: none;"
              "font-size: 14px;"
            "}"
            ".back:hover { text-decoration: underline; }"
            ".error-icon { font-size: 48px; margin-bottom: 16px; }"
            ".error-msg { color: #e88; font-size: 16px; margin-bottom: 24px; }"
          "</style>"
        "</head>"
        "<body>"
          "{{$body}}{{/body}}"
        "</body>"
        "</html>"
      }
    }
  };
}
