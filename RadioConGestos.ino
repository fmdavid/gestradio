#include <Wire.h>
#include <SI4703.h> // modificado SI4703.cpp para usar pin 3 como INT
#include <RDSParser.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_APDS9960.h"

// canales predefinidos
RADIO_FREQ memorias[] = { 8920, 9160, 9430, 9590, 9650, 9710, 9960, 10030, 10320, 10510 }; // Sevilla, España
bool esperandoDigitos = false;
int mem_actual=0;        // Presintonía actual

// Instancias
LiquidCrystal_I2C lcd(0x3F,20,4);  // Instancia del LCD en la dirección I2C 0x3F (ajustar según convenga)
SI4703 radio;    // Instancia del SI4703 (radio).
RDSParser rds; // Para la gestión de los datos recibidos por RDS (nombre de la emisora)
Adafruit_APDS9960 apds; // Instancia del APDS9960 (sensor de gestos)

void setup() {
  // inicializamos el puerto serie
  Serial.begin(57600);

  // Inicializamos y configuramos LCD
  lcd.begin(16, 2);
  lcd.init();
  lcd.setBacklight(1);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("[[[  RadioGest!  ]]]");

  // Inicializamos y configuramos radio
  radio.init();
  radio.debugEnable();
  radio.setBandFrequency(RADIO_BAND_FM, memorias[mem_actual]);
  delay(100);
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(4);
  esperandoDigitos = false;
  radio.attachReceiveRDS(ProcesarRDS);
  rds.attachServicenNameCallback(MostrarDatosRDS); 

  // Inicializamos y configuramos sensor de gestos
  if(!apds.begin()){
    Serial.println("Falló el inicio del sensor de gestos. Revise el cableado.");
  }
  apds.enableProximity(true);
  apds.enableGesture(true);
}

void loop() {
  unsigned long ahora = millis();
  static unsigned long proximoRefresco = 0;
  static unsigned long proximoRefrescoRadioInfo = 0;
  static char comando;
  static int16_t valor;
  static RADIO_FREQ ultimaFrecuencia = 0;
  RADIO_FREQ frecuenciaActual = 0;
  char c;

  // lectura del puerto serie
  if (Serial.available() > 0) {
    // se lee el siguiente caracter del puerto serie
    c = Serial.peek();
    if ((!esperandoDigitos) && (c < 0x20)) { // evitamos caracteres extraños
      Serial.read();
    } else if (!esperandoDigitos) { // lo que leemos es un comando
      comando = Serial.read();
      esperandoDigitos = true;
    } else if (esperandoDigitos) { // lo que leemos son parámetros del comando (digitos)
      if ((c >= '0') && (c <= '9')) {
        c = Serial.read();
        valor = (valor * 10) + (c - '0');
    } else {
        ejecutarComando(comando, valor);
        comando = ' ';
        esperandoDigitos = false;
        valor = 0;
      }
    }
  }

   // lectura del sensor de gestos
   GestionarGesto(apds.readGesture());

  // comprobación de datos RDS
  radio.checkRDS();

  // actualización de los datos en pantalla
  if (ahora > proximoRefresco) {
    MostrarEnPantallaDatos();
    frecuenciaActual = radio.getFrequency();
    if (frecuenciaActual != ultimaFrecuencia) {
      MostrarEnPantallaFrecuencia();
      ultimaFrecuencia = frecuenciaActual;
    }
    proximoRefresco = ahora + 400;
  }

  // actualización de la información de radio
  if (ahora > proximoRefrescoRadioInfo) {
    MostrarEnPantallaInfoRadio();
    proximoRefrescoRadioInfo = ahora + 1000;
  }
}

void ProcesarRDS(uint16_t bloque1, uint16_t bloque2, uint16_t bloque3, uint16_t bloque4) {
  rds.processData(bloque1, bloque2, bloque3, bloque4);
}

// tratamiento de los diferentes comandos que se pueden realizar desde la consola serie
void ejecutarComando(char cmd, int16_t valor)
{
  if (cmd == '?') {
    MostrarAyuda();
  } else if (cmd == '+') {
    int v = radio.getVolume();
    if (v < 15) radio.setVolume(++v);
  } else if (cmd == '-') {
    int v = radio.getVolume();
    if (v > 0) radio.setVolume(--v);
  } else if (cmd == 'm') { radio.setMute(!radio.getMute()); } 
    else if (cmd == 's') { radio.setMono(! radio.getMono()); } 
    else if (cmd == '>') {
    if (mem_actual < (sizeof(memorias) / sizeof(RADIO_FREQ))-1) {
      mem_actual++; radio.setFrequency(memorias[mem_actual]);
    } 
  } else if (cmd == '<') {
    if (mem_actual > 0) {
      mem_actual--;
      radio.setFrequency(memorias[mem_actual]);
      } 
  } 
  else if (cmd == 'f') { radio.setFrequency(valor); }
  else if (cmd == '.') { radio.seekUp(false); }
  else if (cmd == ':') { radio.seekUp(true); }
  else if (cmd == ',') { radio.seekDown(false); }
  else if (cmd == ';') { radio.seekDown(true); }
  else if (cmd == 'x') { radio.debugStatus(); }
  else if (cmd == '!') {
    if (valor == 0) radio.term();
    if (valor == 1) radio.init();
  }
  else if (cmd == 'i') { MostrarEstado(); }
}

void GestionarGesto(uint8_t gesto){
  if(gesto == APDS9960_DOWN){
      Serial.println("v");
      int v = radio.getVolume();
      if (v > 0) { radio.setVolume(--v); }
    }
    if(gesto == APDS9960_UP){
      Serial.println("^");
      int v = radio.getVolume();
      if (v < 15) { radio.setVolume(++v); }
    }
    if(gesto == APDS9960_LEFT){
      Serial.println("<");
      if (mem_actual > 0) {
        mem_actual--;
        radio.setFrequency(memorias[mem_actual]);
      }
    }
    if(gesto == APDS9960_RIGHT){
      Serial.println(">");
        if (mem_actual < (sizeof(memorias) / sizeof(RADIO_FREQ))-1) {
          mem_actual++; radio.setFrequency(memorias[mem_actual]);
      }
    }
}

void MostrarAyuda()
{
    Serial.println();
    Serial.println("? Ayuda");
    Serial.println("+ subir volumen");
    Serial.println("- bajar volumen");
    Serial.println("> siguiente memoria");
    Serial.println("< anterior memoria");
    Serial.println(". buscar hacia adelante   : ... la siguiente estación");
    Serial.println(", buscar hacia atrás ; ... la anterior estación");
    Serial.println("fnnnnn: establecer esa frecuencia");
    Serial.println("i información de estado");
    Serial.println("x información de depuración");
    Serial.println("s mono/stereo");
    Serial.println("m silenciar/no silenciar");
    Serial.println("! reiniciar receptor");
}

void MostrarEstado(){
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  Serial.print("Emisora: "); Serial.println(s);
  Serial.print("Radio: "); radio.debugRadioInfo();
  Serial.print("Audio: "); radio.debugAudioInfo();
}

void MostrarEnPantallaDatos(){
  int v = radio.getVolume();
  lcd.setCursor(13, 1);
  lcd.print("VOL: ");
  if(v < 10) lcd.print("0");
  lcd.print(v);
  lcd.setCursor(13, 2);
  lcd.print("MEM: ");
  if(mem_actual < 10) lcd.print("0");
  lcd.print(mem_actual + 1);
}

void MostrarEnPantallaInfoRadio(){
  RADIO_INFO info;
  radio.getRadioInfo(&info);
  lcd.setCursor(13, 3);
  lcd.print("POT: ");
  lcd.print(info.rssi);
}

void MostrarEnPantallaFrecuencia(){
  // no mostramos el nombre de la emisora cuando aún no es estable...
  MostrarDatosRDS("        ");
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  Serial.print("FREQ:");
  Serial.println(s);
  lcd.setCursor(0, 3);
  lcd.print(s);
}

void MostrarDatosRDS(char *datos)
{
  Serial.print("RDS: ");
  Serial.println(datos);
  lcd.setCursor(0, 1);
  lcd.print(datos);
}
