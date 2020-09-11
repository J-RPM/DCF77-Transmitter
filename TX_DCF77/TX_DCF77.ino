/*                       *** Transmisor DCF77 ***
____________________________________________________________________________________
            Oscilador de 77,5 KHz, controlado ON/OFF (Impulsos TX DCF77)
                             >>> ATmega328P <<<
                             Escrito por: J_RPM
                  http://j-rpm.com/2019/04/en-hora-con-dcf77/
                           Rev. Septiembre de 2020
____________________________________________________________________________________
*/
#include <PWM.h> 
const int inDato = 2;                     // Entrada de datos DCF77 (0 = OFF - Desconecta la portadora)               
const int outLed = 4;                     // LED portadora 
const int iniLed = 5;                     // LED inicio del minuto 
const int ceroLed = 6;                    // LED Bit = 0 
const int unoLed = 7;                     // LED Bit = 1 
const int outDCF = 9;                     // Salida de portadora 77,5 KHz
int32_t DCF_freq = 77500;                 // Real = 77609 Hz (Saltos: 76864 ... 77609)
unsigned long t1 = 0;                     // Referencia de tiempo Inicio del impulso
unsigned long t2 = 0;                     // Medida de tiempo del impulso
unsigned long t3 = 0;                     // Referencia de tiempo Fin del impulso
unsigned long t4 = 0;                     // Medida de tiempo sin impulso (segundo 59)
boolean flanco = false;                   // Detección de cambio de estado
boolean datos = false;                    // Detección de entrada de datos
int i=0;                                  // TimeOut LED encendido
int n=0;                                  // Contador de BIT 
             
void setup() {
  pinMode(inDato, INPUT);                 // Entrada de datos DCF77
  digitalWrite(inDato, HIGH);             // Pull-Up
  pinMode(outDCF, OUTPUT);      
  pinMode(outLed, OUTPUT);      
  pinMode(iniLed, OUTPUT);      
  pinMode(ceroLed, OUTPUT);      
  pinMode(unoLed, OUTPUT);      
  Serial.begin(9600); 
  Serial.println();
  Serial.println();
  Serial.println(F("******************************"));
  Serial.println(F("________ DCF77 decoder ____"));
  Serial.println(F("N#1/0 > |_____Low_____|High|__"));
  Serial.println(F("******************************"));
  InitTimersSafe();                       // Inicializa todos los timer menos el 0

  SetPinFrequencySafe(outDCF, DCF_freq);  // Permite frecuencias entre 1 Hz y 2 MHz (16 bit)
  pwmWriteHR(outDCF, 32768);              // Ciclo 50% (16-bit 1/2 de 65536 = 32768)
  digitalWrite(outLed, LOW);              // DCF77 ON
  t1=millis();                            // Reinicia la referencia del INICIO del impulso
  t3=millis();                            // Reinicia la referencia del FIN del impulso
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(){
  // Dato #1 = No portadora 
  if(digitalRead (inDato)==LOW){
    pwmWriteHR(outDCF, 0);                // Salida = 0 (Sin portadara DCF77)     
    digitalWrite(outLed, HIGH);           // Apaga LED indicador de portadora DCF77                                                                            
    // Detecta el instante del cambio
    if (flanco == false) {
      t1=millis();                        // Toma referencia del INICIO del impulso
      flanco = true;
      datos = true;                       // Detecta datos en la entrada
    }

  // Dato #0 = PORTADORA 
  }else {
    pwmWriteHR(outDCF, 32768);            // Ciclo 50% (16-bit 1/2 de 65536 = 32768)
    digitalWrite(outLed, LOW);            // DCF77 ON
    if (flanco == true) {
      t2 = millis() - t1;                 // Toma la referencia de FIN del impulso
      muestraLed();                       // Reinicia INICIO/FIN del impulso y flanco
    }else {
      t4 = millis() - t3;
      if (t4 > 1000 && datos == true) {   // Detección del segmento 59 DCF77 (NO Bit)
        datos = false;
        muestraLed();                     // Reinicia INICIO/FIN del impulso y flanco   
      }
    }
    
    // Tiempo que permanecen encendidos los LED
    i=i+1;
    if (i>7000) {                         // TimeOut de los LED (Apagado)
      apagaLed();                                            
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Subrutinas
////////////////////////////////////////////////////////////////////////////////////////////////////
void muestraLed() {
  // Número del segundo (0...59), se sincroniza al final del primer minuto transmitido
  Serial.print(n);
  
  if(t4 > 1000) {                         // >>> Inicio del minuto 
    n=59;                                 // Sincroniza el número de bit con el 59 
    digitalWrite(iniLed, LOW);            // Led inicio ON
    digitalWrite(ceroLed, HIGH);          // Led cero OFF
    digitalWrite(unoLed, HIGH);           // Led uno OFF
    Serial.print(F("##"));
  }else if(t2 > 150) {                    // >>> Bit = 1
    digitalWrite(unoLed, LOW);            // Led uno ON
    digitalWrite(iniLed, HIGH);           // Led inicio OFF
    digitalWrite(ceroLed, HIGH);          // Led cero OFF
    Serial.print(F("#1"));
  }else {                                 // >>> Bit = 0
    digitalWrite(ceroLed, LOW);           // Led cero ON                                                                             
    digitalWrite(iniLed, HIGH);           // Led inicio OFF
    digitalWrite(unoLed, HIGH);           // Led uno OFF
    Serial.print(F("#0"));
  }
  // Indicación de tiempos por el puerto serie
  if (n != 59) {
    Serial.print(F(" > "));
    Serial.print(t4);                     // Tiempo con nivel '0', previo al impulso
    Serial.print(F("ms >>> "));
    Serial.print(t2);                     // Duración del impulso
    Serial.println(F("ms"));
  }else {
    Serial.println(F(" -<<< SYNCHRONIZED >>>-"));
  }

  // Reinicio de parámetros
  flanco = false;
  t1=millis();                            // Reinicia la referencia del INICIO del impulso
  t3=millis();                            // Reinicia la referencia del FIN del impulso
  i=0;                                    // Reinicia TimeOut del apagado de los LED
  n++;                                    // Incrementa el contador de bit
  if (n>59) n=0;                          // Reinicia el contador cuando se desborda  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void apagaLed() {
  digitalWrite(iniLed, HIGH);             // Apaga LED del Bit inicio de minuto                                                                             
  digitalWrite(ceroLed, HIGH);            // Apaga LED Bit = 0                                                                             
  digitalWrite(unoLed, HIGH);             // Apaga LED Bit = 1                                                                             
  i=0;                                    // Reinicia TimeOut del apagado de los LED
}
//////////////////////// FIN ////////////////////////////////////////////////////////////////////////
