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
// VENTANA HORARIA:
//   Solo se registra entre las 07:00 y las 19:00, todos los dias (incluidos
//   sabados y domingos, para permitir estudios de ruido exterior en fin de
//   semana). Fuera de ese horario el sistema queda en espera sin escribir.
//   Autonomia a 30 s de intervalo: >11 dias incluso en el peor caso.
//
// FORMATO CSV:
//   timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_medio,dB_max,alertas
//   El campo 'alertas' lleva las etiquetas activas separadas por ';' u "OK".
//   Etiquetas de condicion:
//     T_BAJA T_ALTA T_RIESGO / HR_BAJA HR_ALTA /
//     CO2_DEFIC CO2_MALA CO2_RIESGO / DB_ELEV DB_RIESGO
//   Etiquetas de fiabilidad del dato (imprescindibles en estudio desatendido):
//     ERR_TH ERR_CO2 ERR_DB   sensor sin respuesta valida en esa lectura
//     ERR_RTC                 marca de tiempo no fiable
//     FLASH_BAJA              queda poco espacio de almacenamiento
//   Umbrales segun RD 486/1997 Anexo III, RITE (IDA2) y CTE DB-HS 3.
//   dB_medio y dB_max se calculan muestreando el sonometro cada segundo.
//
// NO INTERFERENCIA CON EL ESTUDIO:
//   Durante el registro el LED queda a brillo minimo (3/255) como simple
//   testigo de funcionamiento: verde = todo correcto, rojo = algun fallo.
//   NO codifica condiciones ambientales ni la ventana horaria. Si lo hiciera,
//   los ocupantes sabrian cuando se mide o que el aire esta cargado, y
//   podrian ventilar o bajar la voz: el dato dejaria de reflejar el aula
//   real (reactividad). Se mantiene encendido porque el modulo DS3231 ya
//   lleva un LED de alimentacion permanente e inevitable; el equipo se ve
//   encendido igualmente. El comando L da un destello puntual mas visible
//   para que el operador compruebe el estado cuando lo necesite.
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

// -- Umbrales de alerta (tabla de interpretacion, RD 486/1997 - RITE - CTE) --
// Temperatura (RD 486/1997 Anexo III, trabajo sedentario 17-27 C)
#define T_MIN_LEGAL   17.0    // por debajo: incumplimiento
#define T_MAX_LEGAL   27.0    // por encima: incumplimiento
#define T_RIESGO      30.0    // evaluar estres termico (mod. RDL 4/2023)
// Humedad relativa (RD 486/1997 Anexo III, 30-70%)
#define HR_MIN_LEGAL  30.0
#define HR_MAX_LEGAL  70.0
// CO2 (RITE IDA2 aulas / CTE DB-HS 3)
#define CO2_DEFIC      900    // CTE: media anual < 900 ppm
#define CO2_MALA      1200    // NTP 742: mala calidad
#define CO2_RIESGO    1600    // CTE: nunca superar
// Nivel sonoro
#define DB_ELEVADO      55    // esfuerzo vocal del docente
#define DB_RIESGO       65    // comunicacion comprometida

// -- Muestreo de picos de ruido --
#define DB_SAMPLE_MS  1000UL  // muestrear el sonometro cada 1 s

// -- Logging --
#define LOG_INTERVAL_MS  30000UL
#define CSV_FILENAME     "/datalog.csv"

// -- Ventana horaria de registro --
// Solo se registra entre HORA_INICIO y HORA_FIN (todos los dias, incluidos
// fines de semana: permite estudios de ruido exterior en sabado y domingo).
// Fuera de la ventana el sistema permanece en espera sin escribir en flash.
#define HORA_INICIO   7    // 07:00 h
#define HORA_FIN     19    // 19:00 h (se registra hasta las 18:59:59)
// Nota: la ventana no admite cruce de medianoche (HORA_FIN debe ser mayor
// que HORA_INICIO). Para registro continuo 24 h, poner 0 y 24.

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

// -- Seguimiento de picos de ruido dentro del intervalo --
unsigned long lastDbSample = 0;
uint8_t  dbMaxIntervalo = 0;    // pico observado en el intervalo actual
uint32_t dbSumaIntervalo = 0;   // suma para la media
uint16_t dbNumMuestras = 0;     // numero de muestras acumuladas

// -- Control de espacio en flash --
// Si el espacio libre baja de este umbral, el sistema avisa (LED rojo tenue)
// y marca los registros. Evita que un estudio desatendido pierda datos en
// silencio al llenarse la memoria.
#define FLASH_MIN_KB   60      // umbral de aviso
uint16_t fallosEscritura = 0;  // registros que no se pudieron guardar
bool flashCasiLlena = false;

// -- Ventana horaria --
bool enVentanaAnterior = true;  // para avisar solo en los cambios de estado

// Devuelve true si la hora actual esta dentro de la ventana de registro.
// Si el RTC no esta disponible, registra siempre (para no perder datos).
bool enVentanaHoraria() {
  if (!ds3231_ok) return true;
  int hora = rtc.now().hour();
  return (hora >= HORA_INICIO && hora < HORA_FIN);
}


// ============================================================
// LED WS2812B (GPIO48, 5V, neopixelWrite)
// ============================================================
void setLED(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(PIN_LED, r, g, b); }
void ledOff()   { setLED(0,0,0); }
void ledGreen() { setLED(0,80,0); }
void ledRed()   { setLED(80,0,0); }
void ledBlue()  { setLED(0,0,80); }
void ledError(int t){ for(int i=0;i<t;i++){ledRed();delay(200);ledOff();delay(200);} }

// -- LED de estado a brillo minimo --
// Durante el registro el LED queda encendido al minimo perceptible, solo como
// testigo de que el sistema funciona. NO codifica condiciones ambientales ni
// la ventana horaria: si lo hiciera, delataria el estudio a los ocupantes.
// Se mantiene encendido porque el modulo DS3231 ya lleva un LED de
// alimentacion permanente que no puede apagarse; el equipo se ve encendido
// igualmente, de modo que un testigo tenue no anade informacion nueva.
#define BRILLO_MIN  3          // 3/255: visible de cerca, apenas perceptible
void ledEstadoOK()    { setLED(0, BRILLO_MIN, 0); }   // verde tenue: todo correcto
void ledEstadoFallo() { setLED(BRILLO_MIN, 0, 0); }   // rojo tenue: algun componente KO



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
  f.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_medio,dB_max,alertas");
  f.println("2025-04-15T09:00:00,22.50,55.0,750,48,52,OK");
  f.println("2025-04-15T09:00:03,28.10,45.0,1350,58,71,CO2_MALA;DB_RIESGO");
  f.close();
  Serial.printf("  [OK] Fichero de prueba escrito: %s\n", tf);

  f=LittleFS.open(tf,"r");
  if(!f){ Serial.println(F("  [!!] No se puede leer fichero.")); littlefs_ok=false; return; }
  Serial.println(F("  Contenido leido:"));
  while(f.available()){ Serial.print("    "); Serial.println(f.readStringUntil('\n')); }
  f.close();

  if(!LittleFS.exists(CSV_FILENAME)){
    File lg=LittleFS.open(CSV_FILENAME,"w");
    lg.println("timestamp_iso8601,temp_C,hum_pct,co2_ppm,dB_medio,dB_max,alertas");
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

// Construye la cadena de alertas activas separadas por ';'
// Devuelve "OK" si ninguna variable esta fuera de rango.
void construirAlertas(char* buf, size_t n, float t, float h, int co2,
                      uint8_t dbMed, uint8_t dbMax,
                      bool errTH, bool errCO2, bool errDB, bool errRTC) {
  buf[0] = '\0';
  bool primera = true;
  auto add = [&](const char* etiqueta) {
    if (!primera) strncat(buf, ";", n - strlen(buf) - 1);
    strncat(buf, etiqueta, n - strlen(buf) - 1);
    primera = false;
  };

  // Errores de lectura: marcan la fila como no fiable para esa variable.
  // Imprescindible en estudios desatendidos: sin esto, un sensor que falla
  // a mitad de estudio registraria ceros indistinguibles de lecturas validas.
  if (errRTC) add("ERR_RTC");
  if (errTH)  add("ERR_TH");
  if (errCO2) add("ERR_CO2");
  if (errDB)  add("ERR_DB");
  if (flashCasiLlena) add("FLASH_BAJA");

  // Temperatura (solo si el sensor responde y la lectura es valida)
  if (sht41_ok && !errTH) {
    if      (t >= T_RIESGO)     add("T_RIESGO");
    else if (t >  T_MAX_LEGAL)  add("T_ALTA");
    else if (t <  T_MIN_LEGAL)  add("T_BAJA");
    // Humedad
    if      (h >  HR_MAX_LEGAL) add("HR_ALTA");
    else if (h <  HR_MIN_LEGAL) add("HR_BAJA");
  }
  // CO2 (0 = lectura no valida, se omite)
  if (s8_ok && !errCO2 && co2 > 0) {
    if      (co2 >= CO2_RIESGO) add("CO2_RIESGO");
    else if (co2 >= CO2_MALA)   add("CO2_MALA");
    else if (co2 >= CO2_DEFIC)  add("CO2_DEFIC");
  }
  // Ruido: se evalua sobre el pico del intervalo
  if (dbmeter_ok && !errDB && dbMax > 0) {
    if      (dbMax >= DB_RIESGO)  add("DB_RIESGO");
    else if (dbMax >= DB_ELEVADO) add("DB_ELEV");
  }

  if (primera) strncpy(buf, "OK", n);
}

void logRow() {
  float t=0,h=0; int co2=0;
  bool errTH=false, errCO2=false, errDB=false, errRTC=false;

  // Lectura T/HR con deteccion de error
  if (sht41_ok) {
    uint16_t e = sht4x.measureHighPrecision(t,h);
    if (e != 0) { errTH = true; t = 0; h = 0; }
  }

  // Lectura CO2 con deteccion de error (get_co2 devuelve <0 si falla)
  if (s8_ok) {
    int16_t r = sensor_S8->get_co2();
    if (r > 0) co2 = r;
    else if (r < 0) errCO2 = true;
    // r == 0 durante el calentamiento inicial: no se marca como error
  }

  // Ruido: media y pico acumulados durante el intervalo
  uint8_t dbMed = 0, dbMax = dbMaxIntervalo;
  if (dbNumMuestras > 0) dbMed = (uint8_t)(dbSumaIntervalo / dbNumMuestras);
  else if (dbmeter_ok)   { dbMed = readDBMeter(); dbMax = dbMed;
                           if (dbMed == 0) errDB = true; }

  // Marca de tiempo
  char ts[25]="0000-00-00T00:00:00";
  if(ds3231_ok){
    DateTime n=rtc.now();
    if (n.year() < 2020) errRTC = true;   // reloj sin ajustar o sin pila
    snprintf(ts,sizeof(ts),"%04d-%02d-%02dT%02d:%02d:%02d",
      n.year(),n.month(),n.day(),n.hour(),n.minute(),n.second());
  } else errRTC = true;

  // Control de espacio libre en flash
  if (littlefs_ok) {
    size_t libre = LittleFS.totalBytes() - LittleFS.usedBytes();
    bool bajaAhora = (libre / 1024) < FLASH_MIN_KB;
    if (bajaAhora && !flashCasiLlena) {
      Serial.printf("\n[!!] AVISO: quedan %u KB de flash. Vuelca y borra los datos.\n\n",
        libre/1024);
      ledEstadoFallo();   // testigo rojo tenue: algo requiere atencion
    }
    flashCasiLlena = bajaAhora;
  }

  char alertas[64];
  construirAlertas(alertas, sizeof(alertas), t, h, co2, dbMed, dbMax,
                   errTH, errCO2, errDB, errRTC);

  char row[128];
  snprintf(row,sizeof(row),"%s,%.2f,%.1f,%d,%d,%d,%s",
    ts, t, h, co2, dbMed, dbMax, alertas);

  // Escritura con verificacion: si falla, se contabiliza y se avisa
  bool guardado = false;
  if(littlefs_ok){
    File f=LittleFS.open(CSV_FILENAME,"a");
    if(f){
      size_t escritos = f.println(row);
      f.close();
      if (escritos > 0) { rowCount++; guardado = true; }
    }
  }
  if (!guardado) {
    fallosEscritura++;
    Serial.printf("[!!] FALLO DE ESCRITURA #%u — dato perdido: %s\n", fallosEscritura, row);
    ledEstadoFallo();
  } else {
    Serial.printf("[LOG #%lu] %s\n", rowCount, row);
  }

  if(littlefs_ok && rowCount%10==0 && guardado){
    size_t fb=LittleFS.totalBytes()-LittleFS.usedBytes();
    Serial.printf("  [Flash libre: %u KB]\n", fb/1024);
  }

  // Reiniciar acumuladores del intervalo
  dbMaxIntervalo = 0; dbSumaIntervalo = 0; dbNumMuestras = 0;
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
  Serial.println(F("  L   destello de comprobacion (el LED sigue apagado despues)"));
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
  size_t tot=LittleFS.totalBytes(), us=LittleFS.usedBytes(), libre=tot-us;
  Serial.printf("Flash: %u KB total | %u KB usado | %u KB libre\n", tot/1024, us/1024, libre/1024);
  Serial.printf("Ventana de registro: %02d:00-%02d:00 (todos los dias)\n", HORA_INICIO, HORA_FIN);
  Serial.printf("Intervalo: %lu s\n", LOG_INTERVAL_MS/1000);
  if(littlefs_ok && LittleFS.exists(CSV_FILENAME)){
    File f=LittleFS.open(CSV_FILENAME,"r");
    size_t by=f.size(); int ln=0;
    while(f.available()) if(f.read()=='\n') ln++;
    f.close();
    int regs = ln>0?ln-1:0;
    Serial.printf("CSV: %u bytes, %d registros\n", by, regs);
    // Autonomia estimada en el peor caso (86 B/fila, todas las alertas)
    uint32_t regDia = ((HORA_FIN-HORA_INICIO)*3600UL)/(LOG_INTERVAL_MS/1000);
    uint32_t capacidad = libre / 86;
    Serial.printf("Autonomia restante (peor caso): %lu registros = %.1f dias\n",
      capacidad, (float)capacidad/regDia);
  }
  Serial.printf("Estado: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  if (fallosEscritura > 0)
    Serial.printf("[!!] Fallos de escritura acumulados: %u (datos perdidos)\n", fallosEscritura);
  if (flashCasiLlena)
    Serial.println(F("[!!] Flash por debajo del umbral: vuelca y borra los datos."));
}

// Comprobacion de funcionamiento bajo demanda: un unico destello verde.
// Se usa solo cuando el operador esta presente; el resto del tiempo el LED
// permanece apagado para no senalizar la medicion a los ocupantes.
void comprobarVida() {
  Serial.println(F("Destello de comprobacion (el LED vuelve al brillo minimo)."));
  bool todoOk = sht41_ok && dbmeter_ok && ds3231_ok && s8_ok && littlefs_ok;
  ledGreen(); delay(400);
  if (todoOk) ledEstadoOK(); else ledEstadoFallo();
  if (ds3231_ok) {
    DateTime n = rtc.now();
    Serial.printf("  Hora: %02d:%02d:%02d | Estado: %s | Registros: %lu\n",
      n.hour(), n.minute(), n.second(),
      enVentanaHoraria() ? "REGISTRANDO" : "en espera", rowCount);
    if (fallosEscritura > 0)
      Serial.printf("  [!!] %u fallos de escritura\n", fallosEscritura);
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
    case 'L': case 'l': comprobarVida();        break;
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

  // Al iniciar el registro el LED pasa a brillo minimo como simple testigo de
  // funcionamiento. No cambia con las condiciones ambientales ni con la
  // ventana horaria, para no senalizar la medicion a los ocupantes.
  delay(1500);   // margen para que el operador vea el resultado del diagnostico
  if (ok==5) ledEstadoOK(); else ledEstadoFallo();

  lastLog = millis() - LOG_INTERVAL_MS;
  enVentanaAnterior = enVentanaHoraria();

  Serial.printf("\n>> Ventana de registro: %02d:00-%02d:00 (todos los dias, incl. fines de semana)\n",
    HORA_INICIO, HORA_FIN);
  Serial.printf(">> Estado actual: %s\n", enVentanaHoraria() ? "REGISTRANDO" : "en espera (fuera de horario)");
  Serial.println(F(">> LED a brillo minimo: solo testigo de funcionamiento."));
  Serial.println(F(">> No indica condiciones ambientales, para no contaminar el estudio."));
  Serial.println(F(">> Comandos: T=reloj  H=hora  D=volcar  I=info  L=comprobar  ?=ayuda"));
  Serial.println(F(">> Ejemplo para ajustar hora: T2026-08-06 06:50:00\n"));
}


// ============================================================
// loop()
// ============================================================
void loop() {
  if (Serial.available()) procesarComando();

  unsigned long now = millis();
  bool activo = enVentanaHoraria();

  // Avisar por serie al entrar o salir de la ventana horaria
  // (solo por consola: el LED permanece apagado para no señalizar a los
  //  ocupantes cuando el equipo esta registrando)
  if (activo != enVentanaAnterior) {
    if (ds3231_ok) {
      DateTime n = rtc.now();
      Serial.printf("\n>> %s ventana de registro (%02d:%02d) — activa %02d:00-%02d:00\n\n",
        activo ? "ENTRANDO en" : "SALIENDO de", n.hour(), n.minute(),
        HORA_INICIO, HORA_FIN);
    }
    enVentanaAnterior = activo;
    // Descartar acumuladores al cambiar de estado
    dbMaxIntervalo = 0; dbSumaIntervalo = 0; dbNumMuestras = 0;
    lastLog = now - LOG_INTERVAL_MS;   // registrar de inmediato al entrar
  }

  if (!activo) { delay(50); return; }    // fuera de horario: en espera

  // Muestreo del sonometro para media y pico del intervalo
  if (dbmeter_ok && (now - lastDbSample >= DB_SAMPLE_MS)) {
    lastDbSample = now;
    uint8_t db = readDBMeter();
    if (db > 0) {
      if (db > dbMaxIntervalo) dbMaxIntervalo = db;
      dbSumaIntervalo += db;
      dbNumMuestras++;
    }
  }

  // Registro periodico
  if (now - lastLog >= LOG_INTERVAL_MS) {
    lastLog = now;
    logRow();
  }
}
