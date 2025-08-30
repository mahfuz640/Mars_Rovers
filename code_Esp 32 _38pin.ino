#include <WiFi.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <math.h>

// ======= Wi-Fi AP =======
const char* ssid = "ESP32_AP";
const char* password = "12345678";
WiFiServer server(80);

// ======= Servos =======
#define SERVO1_PIN 14
#define SERVO2_PIN 12
#define SERVO3_PIN 13
Servo servo1, servo2, servo3;
int servo1Angle = 0; // limit 50-90
int servo2Angle = 90;
int servo3Angle = 100;

// ======= Moisture Sensor =======
#define MOISTURE_SENSOR 34

// ======= DHT11 =======
#define DHT_PIN 25
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ======= L298N Motor Pins =======
#define IN1 26
#define IN2 27
#define IN3 32
#define IN4 33

// ======= Ultrasonic Sensor =======
#define TRIG_PIN 5
#define ECHO_PIN 18

long duration;
int distance;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Attach servos
  servo1.attach(SERVO1_PIN); servo1.write(servo1Angle);
  servo2.attach(SERVO2_PIN); servo2.write(servo2Angle);
  servo3.attach(SERVO3_PIN); servo3.write(servo3Angle);

  // Moisture sensor
  pinMode(MOISTURE_SENSOR, INPUT);

  // Motor pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopCar();

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Start Wi-Fi AP
  WiFi.softAP(ssid, password);
  Serial.println("ESP32 AP started");
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  server.begin();
}

void loop() {
  // Ultrasonic read
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout ~30ms
  distance = duration * 0.034 / 2; // cm

  // Wi-Fi client handling
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r'); client.flush();
    Serial.println(request);

    // ======= Servo Control =======
    if (request.indexOf("/servo1?open") >= 0) { servo1Angle = max(0, servo1Angle - 5); servo1.write(servo1Angle); }
    if (request.indexOf("/servo1?close") >= 0) { servo1Angle = min(40, servo1Angle + 5); servo1.write(servo1Angle); }
    if (request.indexOf("/servo2?up") >= 0) { servo2Angle = max(0, servo2Angle - 5); servo2.write(servo2Angle); }
    if (request.indexOf("/servo2?down") >= 0) { servo2Angle = min(180, servo2Angle + 5); servo2.write(servo2Angle); }
    if (request.indexOf("/servo3?up") >= 0) { servo3Angle = max(70, servo3Angle - 5); servo3.write(servo3Angle); }
    if (request.indexOf("/servo3?down") >= 0) { servo3Angle = min(180, servo3Angle + 5); servo3.write(servo3Angle); }

    // ======= Car Control =======
    if (request.indexOf("/car?forward") >= 0) moveForward();
    else if (request.indexOf("/car?back") >= 0) moveBackward();
    else if (request.indexOf("/car?left") >= 0) turnLeft();
    else if (request.indexOf("/car?right") >= 0) turnRight();
    else if (request.indexOf("/car?stop") >= 0) stopCar();

    // ======= JSON Data Endpoint =======
    if (request.indexOf("/data") >= 0) {
      int moistureVal = analogRead(MOISTURE_SENSOR);
      String soilStatus = (moistureVal < 2000) ? "Wet" : "Dry"; 
      float temp = dht.readTemperature();
      float hum  = dht.readHumidity();

      // Ultrasonic: separate distance and alert
      String distText = "Distance: " + String(distance) + " cm";
      String alertText = "";
      if (distance > 0 && distance <= 20) {
        alertText = "🚨 ALERT: Object " + String(distance) + " cm";
      }

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      client.print("{\"soil\":\""); client.print(soilStatus); client.print("\"");
      client.print(",\"temp\":"); client.print(temp,1);
      client.print(",\"hum\":"); client.print(hum,1);
      client.print(",\"servo1\":"); client.print(servo1Angle);
      client.print(",\"servo2\":"); client.print(servo2Angle);
      client.print(",\"servo3\":"); client.print(servo3Angle);
      client.print(",\"distance\":\""); client.print(distText); client.print("\"");
      client.print(",\"alert\":\""); client.print(alertText); client.print("\"");
      client.print("}");
      client.stop();
      return;
    }

    // ======= Default HTML =======
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    client.println(getHTML());
    client.stop();
  }
}

// ======= Motor Functions =======
void moveForward() { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); }
void moveBackward(){ digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); }
void turnLeft(){ digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); }
void turnRight(){ digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); }
void stopCar(){ digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,LOW); }

// ======= HTML + JS =======
String getHTML() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 Control + Moisture + Car</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body{background:#0d0d0d;color:#0ff;text-align:center;font-family:sans-serif;";
  html += "-webkit-user-select:none;-moz-user-select:none;-ms-user-select:none;user-select:none;";
  html += "margin:0;padding:0;}";
  html += "button{padding:12px 20px;margin:5px;font-size:16px;border:none;border-radius:8px;";
  html += "color:#fff;cursor:pointer;background: linear-gradient(45deg,#ff6b6b,#f94d6a);transition:0.3s;}";
  html += "button:hover{background: linear-gradient(45deg,#6bc1ff,#4d94f9);} ";
  html += "button:active{box-shadow:0 0 20px #fff inset;} ";
  html += ".status{padding:10px;margin:5px;font-size:16px;border-radius:8px;display:inline-block;}"; 
  html += ".wet{background:#00ff80;color:#000;} .dry{background:#ff0080;color:#fff;}"; 
  html += ".alert{color:#ff4040;font-weight:bold;}"; // ALERT style
  html += "</style></head><body>";

  html += "<h2> Car Control</h2>";

  // Servo controls
  html += "<p>Servo1: <span id='servo1Val'>0</span></p>";
  html += "<button onclick=\"fetch('/servo1?open')\">open</button>";
  html += "<button onclick=\"fetch('/servo1?close')\">close</button><br>";
  html += "<p>Servo2: <span id='servo2Val'>0</span></p>";
  html += "<button onclick=\"fetch('/servo2?up')\">Up</button>";
  html += "<button onclick=\"fetch('/servo2?down')\">Down</button><br>";
  html += "<p>Servo3: <span id='servo3Val'>0</span></p>";
  html += "<button onclick=\"fetch('/servo3?up')\">Up</button>";
  html += "<button onclick=\"fetch('/servo3?down')\">Down</button><hr>";

  // Moisture & DHT
  html += "<div>Soil Status: <span id='soilStatus' class='status dry'>--</span></div>";
  html += "<div>Temp: <span id='temp'>--</span>°C, Hum: <span id='hum'>--</span>%</div>";

  // Ultrasonic status
  html += "<div>Ultrasonic: <span id='dist'>--</span></div>";
  html += "<div><span id='alert'></span></div>";   // separate alert line

  // Car buttons
  html += "<h3>Car Control (Hold Button)</h3>";
  html += "<button onmousedown=\"fetch('/car?forward')\" onmouseup=\"fetch('/car?stop')\" ontouchstart=\"fetch('/car?forward')\" ontouchend=\"fetch('/car?stop')\">Forward</button><br>";
  html += "<button onmousedown=\"fetch('/car?back')\" onmouseup=\"fetch('/car?stop')\" ontouchstart=\"fetch('/car?back')\" ontouchend=\"fetch('/car?stop')\">Backward</button><br>";
  html += "<button onmousedown=\"fetch('/car?left')\" onmouseup=\"fetch('/car?stop')\" ontouchstart=\"fetch('/car?left')\" ontouchend=\"fetch('/car?stop')\">Left</button><br>";
  html += "<button onmousedown=\"fetch('/car?right')\" onmouseup=\"fetch('/car?stop')\" ontouchstart=\"fetch('/car?right')\" ontouchend=\"fetch('/car?stop')\">Right</button><br>";

  // JS auto-update
  html += "<script>";
  html += "function update(){fetch('/data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('servo1Val').innerText=d.servo1;";
  html += "document.getElementById('servo2Val').innerText=d.servo2;";
  html += "document.getElementById('servo3Val').innerText=d.servo3;";
  html += "document.getElementById('soilStatus').innerText=d.soil;";
  html += "document.getElementById('soilStatus').className='status '+(d.soil=='Wet'?'wet':'dry');";
  html += "document.getElementById('temp').innerText=d.temp.toFixed(1);";
  html += "document.getElementById('hum').innerText=d.hum.toFixed(1);";
  html += "document.getElementById('dist').innerText=d.distance;";      // always update distance
  html += "document.getElementById('alert').innerText=d.alert;";        // update alert if exists
  html += "if(d.alert && d.alert.includes('ALERT')){document.getElementById('alert').className='alert';}else{document.getElementById('alert').className='';}";
  html += "});}";
  html += "setInterval(update,500);update();";  // update every 0.5 sec
  html += "</script></body></html>";
  return html;
}
