/*******************************************************************
    Un bot de Telegram que te envía un mensaje cuando el ESP
    se inicia y controla una bomba de riego basada en la humedad del suelo.

    Partes:
    - D1 Mini ESP8266 (o cualquier placa ESP8266)
    - Sensor de humedad
    - Bomba de riego
    - Pantalla LCD I2C

    Este código usa WiFiManager para la configuración WiFi y 
    UniversalTelegramBot para la interacción con Telegram.

    Comandos de Telegram:
    - /humedad: Muestra el nivel actual de humedad del suelo.
    - /plantas: Muestra una lista de plantas disponibles.
    - /setplanta <nombre>: Establece la planta a monitorear.
    - /plantainfo: Muestra la planta actual y sus parámetros.
    - /info: Muestra las instrucciones para usar el bot.

    Versión: 1.2

    Autor: Hernzum
    GitHub: https://github.com/hernzum/esp8266-watering-bot
 *******************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <LiquidCrystal_I2C.h>
#include <FS.h>

// Configuración de la pantalla LCD
const int lcdColumnas = 16;
const int lcdFilas = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumnas, lcdFilas);

// Define las constantes para el bot de Telegram y el chat ID
const char* BOT_TOKEN = "TU_BOT_TOKEN";
const char* CHAT_ID = "TU_CHAT_ID";

// Certificado para conexión segura con Telegram
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure clienteSeguro;
UniversalTelegramBot bot(BOT_TOKEN, clienteSeguro);

// Pines del sensor de humedad y la bomba
const int pinHumedad = A0;
const int pinBomba = D6;

// Variables para el sensor de humedad
int ultimaLecturaHumedad = -1;
unsigned long ultimaLectura = 0;
const unsigned long intervaloLectura = 60000; // Leer la humedad del suelo cada 1 minuto

// Variables para la bomba
const unsigned long duracionBombaEncendida = 7000; // Duración de la bomba encendida
const unsigned long duracionBombaApagada = 3000; // Duración de la bomba apagada
unsigned long ultimaActivacionBomba = 0;
int intentosRiego = 0;
const int maxIntentosRiego = 10; // Máximo de intentos de riego

// Variables para la pantalla LCD
const unsigned long tiempoApagadoLCD = 1800000; // Tiempo de apagado de la LCD (30 minutos)
unsigned long ultimaActualizacionLCD = 0;
bool lcdEncendida = true;

// Variables para el reporte diario
bool reporteDiarioEnviado = false;

// Tiempo de deep sleep
const unsigned long tiempoSleepS = 600; // Tiempo en segundos (10 minutos)
const unsigned long tiempoSleepUS = tiempoSleepS * 1000000; // Convertir a microsegundos

// Estructura y lista de plantas
struct Planta {
  String nombre;
  int humedadMinima;
  int humedadMaxima;
};

Planta plantas[] = {
  {"Tomate", 30, 50},
  {"Rosa", 40, 60},
  {"Cactus", 10, 30},
  {"Albahaca", 50, 70},
  {"Limonero", 20, 40}
};

String plantaActual = "Tomate";
const char* rutaArchivoPlanta = "/plantaActual.txt";
int indicePlantaActual = 0;

// Estado de la bomba
enum EstadoBomba { BOMBA_APAGADA, BOMBA_ENCENDIDA, BOMBA_ESPERA };
EstadoBomba estadoBomba = BOMBA_APAGADA;

void entrarEnSleepProfundo() {
  Serial.println("Entrando en modo de sueño profundo...");
  ESP.deepSleep(tiempoSleepUS);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Comprobar la razón del reinicio
  if (ESP.getResetReason() == "Deep-Sleep Wake") {
    Serial.println("Despertar del sueño profundo");
  } else {
    // Inicializar solo si no es un reinicio por "deep sleep"
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Inicializando...");

    if (!SPIFFS.begin()) {
      Serial.println("Error al montar el sistema de archivos");
      return;
    }

    if (SPIFFS.exists(rutaArchivoPlanta)) {
      File archivoPlanta = SPIFFS.open(rutaArchivoPlanta, "r");
      if (archivoPlanta) {
        plantaActual = archivoPlanta.readStringUntil('\n');
        archivoPlanta.close();
      }
    }

    establecerParametrosPlanta(plantaActual);
    mostrarNombrePlanta(plantaActual);

    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");

    Serial.println("Conectado a WiFi");
    clienteSeguro.setTrustAnchors(&cert);
    Serial.print("\nWiFi conectado. Dirección IP: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi conectado");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    configTime(3600 * 2, 3600, "pool.ntp.org", "time.nist.gov");
    time_t ahora = time(nullptr);
    while (ahora < 24 * 3600) {
      Serial.print(".");
      delay(100);
      ahora = time(nullptr);
    }
    Serial.println(ahora);

    struct tm *infoTiempo = localtime(&ahora);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("%02d/%02d/%04d", infoTiempo->tm_mday, infoTiempo->tm_mon + 1, infoTiempo->tm_year + 1900);
    lcd.setCursor(0, 1);
    lcd.printf("%02d:%02d:%02d", infoTiempo->tm_hour, infoTiempo->tm_min, infoTiempo->tm_sec);

    pinMode(pinBomba, OUTPUT);
    digitalWrite(pinBomba, HIGH);

    bot.sendMessage(CHAT_ID, "Bot iniciado", "");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Bot iniciado");
    delay(2000);

    ultimaActualizacionLCD = millis();
  }
}

void loop() {
  int numMensajesNuevos = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < numMensajesNuevos; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String texto = bot.messages[i].text;

    if (texto.equals("/humedad")) {
      enviarLecturaHumedad();
    } else if (texto.equals("/plantas")) {
      enviarListaPlantas(chat_id);
    } else if (texto.startsWith("/setplanta")) {
      String nombrePlanta = texto.substring(10);
      nombrePlanta.trim();
      if (establecerParametrosPlanta(nombrePlanta)) {
        plantaActual = nombrePlanta;
        mostrarNombrePlanta(plantaActual);
        bot.sendMessage(chat_id, "Planta establecida a " + nombrePlanta, "");
        guardarPlantaActual(nombrePlanta);
      } else {
        bot.sendMessage(chat_id, "Planta no encontrada. Usa /plantas para ver la lista de plantas disponibles.", "");
      }
    } else if (texto.equals("/plantainfo")) {
      enviarInfoPlanta(chat_id);
    } else if (texto.equals("/info")) {
      enviarInfoBot(chat_id);
    } else {
      bot.sendMessage(chat_id, "Comando inválido. Usa /humedad, /plantas, /setplanta <nombre>, /plantainfo, o /info", "");
    }
  }

  if (millis() - ultimaLectura >= intervaloLectura) {
    ultimaLectura = millis();
    int sensorHumedad = obtenerHumedadPromedio(20);
    int valorBruto = analogRead(pinHumedad);

    Serial.print(sensorHumedad);
    Serial.println("%% Humedad");

    if (abs(sensorHumedad - ultimaLecturaHumedad) >= 5) {
      if (sensorHumedad < plantas[indicePlantaActual].humedadMinima) {
        bot.sendMessage(CHAT_ID, "¡Se necesita riego!", "");
      } else if (sensorHumedad > plantas[indicePlantaActual].humedadMaxima) {
        bot.sendMessage(CHAT_ID, "¡Demasiada agua!", "");
      } else {
        bot.sendMessage(CHAT_ID, "Normal", "");
      }
      ultimaLecturaHumedad = sensorHumedad;
      actualizarLCD(sensorHumedad, valorBruto);
      ultimaActualizacionLCD = millis();
      lcdEncendida = true;
    }

    if (intentosRiego >= maxIntentosRiego && sensorHumedad < plantas[indicePlantaActual].humedadMinima) {
      bot.sendMessage(CHAT_ID, "¡NO AGUA!", "");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("¡NO AGUA!");
      delay(2000);
      ESP.deepSleep(0);
    }
  }

  int sensorHumedad = obtenerHumedadPromedio(20);
  verificarTiempoRiego(sensorHumedad);
  gestionarEstadoBomba();
  enviarReporteDiario();

  if (lcdEncendida && millis() - ultimaActualizacionLCD >= tiempoApagadoLCD) {
    lcd.noBacklight();
    lcdEncendida = false;
  }

  // Entra en modo deep sleep después de completar todas las tareas
  entrarEnSleepProfundo();
}

void verificarTiempoRiego(int sensorHumedad) {
  time_t ahora = time(nullptr);
  struct tm *infoTiempo = localtime(&ahora);

  if ((infoTiempo->tm_hour >= 6) && (infoTiempo->tm_hour < 23)) {
    if (sensorHumedad < plantas[indicePlantaActual].humedadMinima) {
      iniciarRiego();
      bot.sendMessage(CHAT_ID, "Bomba de agua activada automáticamente", "");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Riego");
      lcd.setCursor(0, 1);
      lcd.print("Intentos: ");
      lcd.print(intentosRiego);
      ultimaActualizacionLCD = millis();
      lcdEncendida = true;

      intentosRiego++;
      Serial.print("Intentos de riego: ");
      Serial.println(intentosRiego);
    }
  }
}

void iniciarRiego() {
  if (estadoBomba == BOMBA_APAGADA) {
    estadoBomba = BOMBA_ENCENDIDA;
    digitalWrite(pinBomba, LOW);
    ultimaActivacionBomba = millis();
  }
}

void gestionarEstadoBomba() {
  unsigned long tiempoActual = millis();

  switch (estadoBomba) {
    case BOMBA_ENCENDIDA:
      if (tiempoActual - ultimaActivacionBomba >= duracionBombaEncendida) {
        digitalWrite(pinBomba, HIGH);
        ultimaActivacionBomba = tiempoActual;
        estadoBomba = BOMBA_ESPERA;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Pausa");
        lcd.setCursor(0, 1);
        lcd.print("Intentos: ");
        lcd.print(intentosRiego);
        ultimaActualizacionLCD = millis();
        lcdEncendida = true;
      }
      break;

    case BOMBA_ESPERA:
      if (tiempoActual - ultimaActivacionBomba >= duracionBombaApagada) {
        int sensorHumedad = obtenerHumedadPromedio(20);
        if (sensorHumedad < plantas[indicePlantaActual].humedadMinima) {
          digitalWrite(pinBomba, LOW);
          ultimaActivacionBomba = tiempoActual;
          estadoBomba = BOMBA_ENCENDIDA;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Riego");
          lcd.setCursor(0, 1);
          lcd.print("Intentos: ");
          lcd.print(intentosRiego);
          ultimaActualizacionLCD = millis();
          lcdEncendida = true;
        } else {
          estadoBomba = BOMBA_APAGADA;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Normal");
          lcd.setCursor(0, 1);
          lcd.print("Intentos: ");
          lcd.print(intentosRiego);
          ultimaActualizacionLCD = millis();
          lcdEncendida = true;
          intentosRiego = 0;
        }
      }
      break;

    case BOMBA_APAGADA:
      digitalWrite(pinBomba, HIGH);
      break;
  }
}

void enviarLecturaHumedad() {
  int sensorHumedad = obtenerHumedadPromedio(20);
  String respuesta = "La humedad del suelo es " + String(sensorHumedad) + "%%";
  bot.sendMessage(CHAT_ID, respuesta, "");

  int valorBruto = analogRead(pinHumedad);
  actualizarLCD(sensorHumedad, valorBruto);
  ultimaActualizacionLCD = millis();
  lcdEncendida = true;
}

void enviarListaPlantas(String chat_id) {
  String listaPlantas = "Plantas disponibles:\n";
  for (int i = 0; i < sizeof(plantas) / sizeof(plantas[0]); i++) {
    listaPlantas += "- " + plantas[i].nombre + "\n";
  }
  bot.sendMessage(chat_id, listaPlantas, "");
}

void enviarInfoPlanta(String chat_id) {
  String info = "Planta actual: " + plantaActual + "\n";
  info += "Humedad mínima: " + String(plantas[indicePlantaActual].humedadMinima) + "%\n";
  info += "Humedad máxima: " + String(plantas[indicePlantaActual].humedadMaxima) + "%\n";
  bot.sendMessage(chat_id, info, "");
}

void enviarInfoBot(String chat_id) {
  String info = "Instrucciones de uso del bot:\n";
  info += "/humedad - Muestra el nivel actual de humedad del suelo.\n";
  info += "/plantas - Muestra una lista de plantas disponibles.\n";
  info += "/setplanta <nombre> - Establece la planta a monitorear.\n";
  info += "/plantainfo - Muestra la planta actual y sus parámetros.\n";
  info += "/info - Muestra este mensaje.\n";
  bot.sendMessage(chat_id, info, "");
}

bool establecerParametrosPlanta(String nombrePlanta) {
  for (int i = 0; i < sizeof(plantas) / sizeof(plantas[0]); i++) {
    if (plantas[i].nombre.equalsIgnoreCase(nombrePlanta)) {
      indicePlantaActual = i;
      Serial.println("Parámetros de la planta establecidos:");
      Serial.print("Planta: ");
      Serial.println(plantas[i].nombre);
      Serial.print("Humedad mínima: ");
      Serial.println(plantas[i].humedadMinima);
      Serial.print("Humedad máxima: ");
      Serial.println(plantas[i].humedadMaxima);
      return true;
    }
  }
  return false;
}

void mostrarNombrePlanta(String nombrePlanta) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Planta: ");
  lcd.print(nombrePlanta);
}

bool guardarPlantaActual(String nombrePlanta) {
  File archivoPlanta = SPIFFS.open(rutaArchivoPlanta, "w");
  if (!archivoPlanta) {
    Serial.println("Error al abrir el archivo de la planta para escribir");
    return false;
  }
  archivoPlanta.println(nombrePlanta);
  archivoPlanta.close();
  return true;
}

int obtenerHumedadPromedio(int muestras) {
  long total = 0;
  for (int i = 0; i < muestras; i++) {
    total += analogRead(pinHumedad);
    delay(50);
  }
  int promedio = total / muestras;
  return map(promedio, 705, 317, 0, 100);
}

void actualizarLCD(int sensorHumedad, int valorBruto) {
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Humedad:");
  lcd.print(sensorHumedad);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("Bruto:");
  lcd.print(valorBruto);
}

void enviarReporteDiario() {
  time_t ahora = time(nullptr);
  struct tm *infoTiempo = localtime(&ahora);

  if (infoTiempo->tm_hour == 0 && !reporteDiarioEnviado) {
    int sensorHumedad = obtenerHumedadPromedio(20);
    String reporte = "Reporte diario - Humedad del suelo: " + String(sensorHumedad) + "%%";
    reporte += "\nFecha: ";
    reporte += String(infoTiempo->tm_mday) + "/" + String(infoTiempo->tm_mon + 1) + "/" + String(infoTiempo->tm_year + 1900);
    reporte += "\nHora: ";
    reporte += String(infoTiempo->tm_hour) + ":" + String(infoTiempo->tm_min) + ":" + String(infoTiempo->tm_sec);
    bot.sendMessage(CHAT_ID, reporte, "");
    reporteDiarioEnviado = true;
  }

  if (infoTiempo->tm_hour == 1) {
    reporteDiarioEnviado = false;
  }
}
