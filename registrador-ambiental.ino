// ============================================================
// datalogger_test_v1.ino
// Monitor Ambiental - firmware de prueba, verificacion y logging
//
// Placa: YD-ESP32-S3 - WROOM-1-N16R8 - PCB 2022-V1.3
// Board Arduino IDE: "ESP32S3 Dev Module"
// Flash Size: 16MB (128Mb) | PSRAM: OPI PSRAM | Core 3.2.0
// USB CDC On Boot: ENABLED (el monitor serie sale por USB nativo)
//
// IMPORTANTE (hallazgos de la puesta en marcha):
//  - Cerrar el puente IN-OUT de la placa para que el pin 5V de VBUS
//    alimente el S8. Con el puente abierto el S8 no mide (CO2 = 0).
//  - USB CDC On Boot debe estar en Enabled o no se ve el monitor.
//
// Librerias (Library Manager):
//   - Sensirion I2C SHT4x  (Sensirion)
//   - S8_UART              (jcomas)  -> el fichero es s8_uart.h (minusculas)
//   - RTClib               (Adafruit)
//   - LittleFS             (incluida en el core ESP32)
//
// Pines verificados:
//   SDA  GPIO8  col53B  |  SCL  GPIO9  col56B
//   S8 TX GPIO1 col45J -> S8 UART_RxD col2J
//   S8 RX GPIO2 col46J <- S8 UART_TxD col3J
//   S8 5V ESP32 col62B -> S8 G+ col1A  |  S8 GND -> S8 G0 col2A
//   LED GPIO48 col57J
//
// COMANDOS por el monitor serie (escribir y pulsar Enter):
//   T2026-08-06 06:50:00  ajustar reloj (formato TAAAA-MM-DD hh:mm:ss)
//   H   mostrar hora del RTC
//   D   volcar el CSV guardado
//   I   info del fichero y flash
//   ?   ayuda
// El registro de datos sigue activo mientras se aceptan comandos.
// ============================================================
// Autoria: www.acusticaescolar.com - Licencia MIT
// ============================================================

#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include <s8_uart.h>
#include <RTClib.h>
#include <LittleFS.h>

// -- Pines --
#define PIN_SDA       8
#define PIN_SCL       9
#define PIN_S8_TX     1
#define PIN_S8_RX     2
#define PIN_LED       48

// -- Direcciones I2C --
#define ADDR_SHT41    0x44
#define ADDR_DBMETER  0x48
#define ADDR_DS3231   0x68

// -- Registros DBMETER --
#define DBM_REG_VERSION  0x00
#define DBM_REG_DECIBEL  0x0A

// -- Umbrales LED --
#define CO2_WARN    800
#define CO2_HIGH   1000
#define CO2_CRIT   1500
#define DB_HIGH      65

// -- Logging --
#define LOG_INTERVAL_MS  30000UL
#define CSV_FILENAME     "/datalog.csv"

// -- Objetos globales --
SensirionI2cSht4x sht4x;
S8_UART           *sensor_S8;
HardwareSerial    S8_serial(1);
RTC_DS3231        rtc;

// -- Estado --
bool sht41_ok = false, dbmeter_ok = false, ds3231_ok = false;
bool s8_ok = false, littlefs_ok = false;
unsigned long lastLog = 0;
uint32_t rowCount = 0;


// ============================================================
// LED WS2812B (GPIO48, 5V, neopixelWrite)
// ============================================================
void setLED(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(PIN_LED, r, g, b); }
void ledOff()   { setLED(0,0,0); }
void ledGreen() { setLED(0,80,0); }
void ledYellow(){ setLED(80,60,0); }
void ledOrange(){ setLED(80,25,0); }
void ledRed()   { setLED(80,0,0); }
void ledBlue()  { setLED(0,0,80); }
void ledError(int t){ for(int i=0;i<t;i++){ledRed();delay(200);ledOff();delay(200);} }

void updateStatusLED(int co2, float db) {
  if (co2 >= CO2_CRIT) ledRed();
  else if (co2 >= CO2_HIGH) ledOrange();
  else if (co2 >= CO2_WARN || db >= DB_HIGH) ledYellow();
  else ledGreen();
}


// ============================================================
// DBMETER - lectura por registro I2C
// ============================================================
uint8_t readDBMeter() {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_DECIBEL);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom((uint8_t)ADDR_DBMETER, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}
uint8_t readDBMeterVersion() {
  Wire.beginTransmission(ADDR_DBMETER);
  Wire.write(DBM_REG_VERSION);
  if (Wire.endTransmission(false) != 0) return 0;
  Wire.requestFrom((uint8_t)ADDR_DBMETER, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}


// ============================================================
// FASE 1 - Escaneo I2C
// ============================================================
void fase1_scanI2C() {
  Serial.println(F("\n== FASE 1 - Escaneo del bus I2C =="));
  int found = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  [OK] Dispositivo en 0x%02X", a);
      if (a == ADDR_SHT41)   Serial.print(" -> SHT41 T/H");
      if (a == ADDR_DBMETER) Serial.print(" -> DBMETER");
      if (a == ADDR_DS3231)  Serial.print(" -> DS3231 RTC");
      Serial.println();
      found++; delay(10);
    }
  }
  if (found == 0) {
    Serial.println(F("  [!!] NO se encontro ningun dispositivo I2C."));
    Serial.println(F("       Verifica SDA(53B), SCL(56B), 3V3, GND y pull-ups."));
  }
  auto chk = [](byte a)->bool{ Wire.beginTransmission(a); return Wire.endTransmission()==0; };
  sht41_ok = chk(ADDR_SHT41); dbmeter_ok = chk(ADDR_DBMETER); ds3231_ok = chk(ADDR_DS3231);
  Serial.println();
  Serial.printf("  SHT41   (0x44): %s\n", sht41_ok   ? "OK":"NO DETECTADO");
  Serial.printf("  DBMETER (0x48): %s\n", dbmeter_ok ? "OK":"NO DETECTADO");
  Serial.printf("  DS3231  (0x68): %s\n", ds3231_ok  ? "OK":"NO DETECTADO");
  if(!sht41_ok)   Serial.println(F("  -> SHT41: cols 14B(SCL) 15B(SDA) 13B(3V3) 12B(GND). Girado 180!"));
  if(!dbmeter_ok) Serial.println(F("  -> DBMETER: cols 22B(SCL) 23B(SDA) 20B(3V3) 24B(GND)."));
  if(!ds3231_ok)  Serial.println(F("  -> DS3231: cols 32B(SCL) 31B(SDA) 30B(3V3) 29B(GND). Pila CR2032?"));
}


// ============================================================
// FASE 2 - Lectura de sensores
// ============================================================
void fase2_leerSensores() {
  Serial.println(F("\n== FASE 2 - Lectura de sensores =="));
  if (sht41_ok) {
    float t=0,h=0; uint16_t e=sht4x.measureHighPrecision(t,h);
    if(e==0) Serial.printf("  SHT41   -> Temp: %.2f C  |  HR: %.1f %%\n",t,h);
    else     Serial.printf("  SHT41   -> Error lectura: %d\n",e);
  } else Serial.println(F("  SHT41   -> [OMITIDO]"));

  if (ds3231_ok) {
    DateTime n=rtc.now();
    Serial.printf("  DS3231  -> %04d-%02d-%02d %02d:%02d:%02d\n",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
    Serial.printf("            Temp interna RTC: %.1f C\n", rtc.getTemperature());
  } else Serial.println(F("  DS3231  -> [OMITIDO]"));

  if (dbmeter_ok) {
    uint8_t v=readDBMeterVersion();
    Serial.printf("  DBMETER -> Version fw: 0x%02X\n", v);
    delay(1100);
    uint8_t db=readDBMeter();
    if(db>0) Serial.printf("  DBMETER -> Nivel sonoro: %d dB SPL\n", db);
    else     Serial.println(F("  DBMETER -> Lectura = 0. Espera >1s."));
  } else Serial.println(F("  DBMETER -> [OMITIDO]"));

  if (s8_ok) {
    int16_t c=sensor_S8->get_co2();
    if(c>0) Serial.printf("  S8 LP   -> CO2: %d ppm\n", c);
    else    Serial.println(F("  S8 LP   -> Lectura = 0. Tarda ~30s en responder."));
  } else Serial.println(F("  S8 LP   -> [OMITIDO]"));
}


// ============================================================
// FASE 3 - LED
// ============================================================
void fase3_pruebaLED() {
  Serial.println(F("\n== FASE 3 - Prueba del LED WS2812B =="));
  struct{uint8_t r,g,b; const char* n;} seq[]={
    {0,80,0,"Verde -> OK"},{80,60,0,"Amarillo -> CO2 800-1000"},
    {80,25,0,"Naranja -> CO2 1000-1500"},{80,0,0,"Rojo -> CO2 >1500"},
    {0,0,80,"Azul -> USB/init"},{0,0,0,"Apagado"},
  };
  for(auto& s:seq){ setLED(s.r,s.g,s.b); Serial.printf("  -> %s\n",s.n); delay(700); }
  Serial.println(F("  LED OK."));
}


// ============================================================
// FASE 4 - LittleFS
// ============================================================
void fase4_pruebaLittleFS() {
  Serial.println(F("\n== FASE 4 - Prueba LittleFS =="));
  if (!LittleFS.begin(true)) {
    Serial.println(F("  [!!] Error montando LittleFS. Verifica Flash Size = 16MB."));
    littlefs_ok=false; return;
  }
  littlefs_ok=true;
  Serial.println(F("  [OK] LittleFS montado."));
  size_t tot=LittleFS.totalBytes(), us=LittleFS.usedBytes();
  Serial.printf("  Total: %u KB | Usado: %u KB | Libre: %u KB\n", tot/1024, us/1024, (tot-us)/1024);

  const char* tf="/test_escritura.csv";
  File f=LittleFS.open(tf,"w");
  if(!f){ Serial.println(F("  [!!] No se puede crear fichero.")); littlefs_ok=false; return; }
  f.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_medio,tipo");
  f.println("2025-04-15T09:00:00,22.50,55.0,750,48,INTERVAL");
  f.println("2025-04-15T09:00:03,,,,70,ALERT_DB");
  f.close();
  Serial.printf("  [OK] Fichero de prueba escrito: %s\n", tf);

  f=LittleFS.open(tf,"r");
  if(!f){ Serial.println(F("  [!!] No se puede leer fichero.")); littlefs_ok=false; return; }
  Serial.println(F("  Contenido leido:"));
  while(f.available()){ Serial.print("    "); Serial.println(f.readStringUntil('\n')); }
  f.close();

  if(!LittleFS.exists(CSV_FILENAME)){
    File lg=LittleFS.open(CSV_FILENAME,"w");
    lg.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_medio,tipo");
    lg.close();
    Serial.printf("  [OK] Fichero de log creado: %s\n", CSV_FILENAME);
  } else {
    Serial.printf("  [OK] Fichero de log existente: %s\n", CSV_FILENAME);
  }
  LittleFS.remove(tf);
  Serial.println(F("  [OK] LittleFS: escritura/lectura/verificacion superadas."));
}


// ============================================================
// FASE 5 - Registro de una fila
// ============================================================
void logRow() {
  float t=0,h=0; int co2=0; uint8_t db=0;
  if(sht41_ok)   sht4x.measureHighPrecision(t,h);
  if(dbmeter_ok) db=readDBMeter();
  if(s8_ok){ int16_t r=sensor_S8->get_co2(); if(r>0) co2=r; }

  char ts[25]="0000-00-00T00:00:00";
  if(ds3231_ok){
    DateTime n=rtc.now();
    snprintf(ts,sizeof(ts),"%04d-%02d-%02dT%02d:%02d:%02d",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  }
  char row[80];
  snprintf(row,sizeof(row),"%s,%.2f,%.1f,%d,%d,INTERVAL",ts,t,h,co2,db);

  if(littlefs_ok){
    File f=LittleFS.open(CSV_FILENAME,"a");
    if(f){ f.println(row); f.close(); rowCount++; }
  }
  Serial.printf("[LOG #%lu] %s\n", rowCount, row);
  if(littlefs_ok && rowCount%10==0){
    size_t fb=LittleFS.totalBytes()-LittleFS.usedBytes();
    Serial.printf("  [Flash libre: %u KB]\n", fb/1024);
  }
  updateStatusLED(co2, db);
}


// ============================================================
// COMANDOS por monitor serie
// ============================================================
void mostrarAyuda() {
  Serial.println(F("\n-- COMANDOS --"));
  Serial.println(F("  T2026-08-06 06:50:00  ajustar reloj (TAAAA-MM-DD hh:mm:ss)"));
  Serial.println(F("  H   mostrar hora del RTC"));
  Serial.println(F("  D   volcar el CSV guardado"));
  Serial.println(F("  I   info del fichero y flash"));
  Serial.println(F("  ?   esta ayuda"));
  Serial.println(F("El registro sigue activo mientras tanto.\n"));
}

void sincronizarReloj(const String& cmd) {
  if(!ds3231_ok){ Serial.println(F("[!] DS3231 no disponible.")); return; }
  if(cmd.length()<20){ Serial.println(F("[!] Formato: T2026-08-06 06:50:00")); return; }
  int yr=cmd.substring(1,5).toInt(), mon=cmd.substring(6,8).toInt(), day=cmd.substring(9,11).toInt();
  int hr=cmd.substring(12,14).toInt(), mi=cmd.substring(15,17).toInt(), se=cmd.substring(18,20).toInt();
  if(yr<2020||yr>2099||mon<1||mon>12||day<1||day>31||hr>23||mi>59||se>59){
    Serial.println(F("[!] Valores fuera de rango. Usa: T2026-08-06 06:50:00")); return;
  }
  rtc.adjust(DateTime(yr,mon,day,hr,mi,se));
  DateTime n=rtc.now();
  Serial.printf("[OK] Reloj ajustado a: %04d-%02d-%02d %02d:%02d:%02d\n",
    n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
}

void mostrarHora() {
  if(!ds3231_ok){ Serial.println(F("[!] DS3231 no disponible.")); return; }
  DateTime n=rtc.now();
  Serial.printf("Hora del RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
    n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
}

void volcarCSV() {
  if(!littlefs_ok || !LittleFS.exists(CSV_FILENAME)){ Serial.println(F("[!] No hay CSV.")); return; }
  File f=LittleFS.open(CSV_FILENAME,"r");
  if(!f){ Serial.println(F("[!] No se puede abrir el CSV.")); return; }
  Serial.println(F("\n----- INICIO CSV -----"));
  int n=0; while(f.available()){ Serial.println(f.readStringUntil('\n')); n++; }
  f.close();
  Serial.printf("----- FIN CSV (%d lineas) -----\n\n", n);
}

void infoCSV() {
  size_t tot=LittleFS.totalBytes(), us=LittleFS.usedBytes();
  Serial.printf("Flash: %u KB total | %u KB usado | %u KB libre\n", tot/1024, us/1024, (tot-us)/1024);
  if(littlefs_ok && LittleFS.exists(CSV_FILENAME)){
    File f=LittleFS.open(CSV_FILENAME,"r");
    size_t by=f.size(); int ln=0;
    while(f.available()) if(f.read()=='\n') ln++;
    f.close();
    Serial.printf("CSV: %u bytes, %d registros\n", by, ln>0?ln-1:0);
  }
}

void procesarComando() {
  String cmd=Serial.readStringUntil('\n');
  cmd.trim();
  if(cmd.length()==0) return;
  switch(cmd.charAt(0)){
    case 'T': case 't': sincronizarReloj(cmd); break;
    case 'H': case 'h': mostrarHora();          break;
    case 'D': case 'd': volcarCSV();            break;
    case 'I': case 'i': infoCSV();              break;
    case '?':           mostrarAyuda();         break;
    default: Serial.printf("Comando '%c' no reconocido. ? para ayuda.\n", cmd.charAt(0));
  }
}


// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("\n== DATALOGGER AMBIENTAL - Test firmware v1.0 =="));
  Serial.println(F("   YD-ESP32-S3 N16R8 | core 3.2.0"));

  ledBlue();
  Wire.begin(PIN_SDA, PIN_SCL, 100000UL);
  delay(50);

  // UART S8 - workaround bug core 3.x (begin cuelga si RX tiene trafico)
  Serial.println(F("\n[UART] Iniciando Senseair S8 LP..."));
  S8_serial.begin(9600, SERIAL_8N1, -1, -1);
  S8_serial.setPins(PIN_S8_RX, PIN_S8_TX);
  delay(100);
  sensor_S8 = new S8_UART(S8_serial);

  S8_sensor s8;
  sensor_S8->get_firmware_version(s8.firm_version);
  s8.sensor_type_id = sensor_S8->get_sensor_type_ID();
  if (strlen(s8.firm_version)>0 || s8.sensor_type_id>0) {
    s8_ok=true;
    Serial.printf("  [OK] S8 LP detectado. Tipo ID: 0x%08X  FW: %s\n", s8.sensor_type_id, s8.firm_version);
  } else {
    delay(2000);
    int c=sensor_S8->get_co2();
    s8_ok=(c>=0);
    if(s8_ok) Serial.println(F("  [OK] S8 LP respondiendo."));
    else {
      Serial.println(F("  [!!] S8 LP no responde. Verifica:"));
      Serial.println(F("       - UART cruzado: GPIO1(45J)TX->S8 RxD(2J) y GPIO2(46J)RX<-S8 TxD(3J)"));
      Serial.println(F("       - 5V: ESP32 col62B -> S8 col1A(G+). Puente IN-OUT cerrado?"));
      Serial.println(F("       - GND: S8 col2A(G0) -> carril GND"));
    }
  }

  // SHT41
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);
  sht4x.softReset();
  delay(10);
  uint32_t sn;
  if(sht4x.serialNumber(sn)==0){ sht41_ok=true; Serial.printf("  [OK] SHT41 init. N serie: 0x%08X\n", sn); }

  // DS3231
  if(rtc.begin(&Wire)){
    ds3231_ok=true;
    if(rtc.lostPower()){
      Serial.println(F("  [!] DS3231 perdio alimentacion. Ajusta la hora con el comando T."));
    }
    DateTime n=rtc.now();
    Serial.printf("  [OK] DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  }

  fase1_scanI2C();
  fase2_leerSensores();
  fase3_pruebaLED();
  fase4_pruebaLittleFS();

  Serial.println(F("\n== RESUMEN DE DIAGNOSTICO =="));
  Serial.printf("  SHT41 (T/H)       : %s\n", sht41_ok    ? "OK":"FALLO");
  Serial.printf("  DBMETER (dB)      : %s\n", dbmeter_ok  ? "OK":"FALLO");
  Serial.printf("  DS3231 (RTC)      : %s\n", ds3231_ok   ? "OK":"FALLO");
  Serial.printf("  Senseair S8 (CO2) : %s\n", s8_ok       ? "OK":"FALLO");
  Serial.printf("  LittleFS (flash)  : %s\n", littlefs_ok ? "OK":"FALLO");
  int ok=sht41_ok+dbmeter_ok+ds3231_ok+s8_ok+littlefs_ok;
  Serial.printf("\n  %d/5 componentes OK.\n", ok);
  if(ok==5){ Serial.println(F("  Sistema completo. Iniciando logging...")); ledGreen(); }
  else     { Serial.println(F("  [!] Hay fallos. Revisa el diagnostico.")); ledError(3); }

  lastLog = millis() - LOG_INTERVAL_MS;

  Serial.println(F("\n>> Comandos: T=ajustar reloj  H=hora  D=volcar CSV  I=info  ?=ayuda"));
  Serial.println(F(">> Ejemplo para ajustar hora: T2026-08-06 06:50:00\n"));
}


// ============================================================
// loop()
// ============================================================
void loop() {
  if (Serial.available()) procesarComando();

  unsigned long now = millis();
  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    logRow();
  }
}
