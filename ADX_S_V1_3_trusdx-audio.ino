//*********************************************************************************************************
//********************* ADX-S - ARDUINO DIGITAL MODES 4 BAND HF TRANSCEIVER ***************************
//********************************* Write up start: 02/01/2022 ********************************************
// FW VERSION: ADX_S_V1.3_trusdr-audio by JE1RAV
// Based on ADX-S V1.3 Release:
//  -  Modified to use the audio streaming over CAT, which is now used in (tr)uSDX developed by DL2MAN and PE1NNZ.
// The PC-side code originally written by SQ3SWF are developed and packaged by PE1NNZ. https://github.com/threeme3/trusdx-audio
// To adapt the PC-side code, the firmware of ADX-S has been modified based on the audioStreamArduino by Idan Regev. https://github.com/idanre1/audioStreamArduino/
// The CAT emulation is changed from TS-2000 to TS-480 which is emuelated in (tr)uSDX, because FT8CN supports audio streaming over CAT on (tr)uSDX.
//
// FW VERSION: ADX_S_V1.3 - Version release date: 08/08/2023
// Barb(Barbaros ASUROGLU) - WB2CBA - 2022
//  Version History
//  Based on ADX-Quad V1.2 Release:
//  -  Modified Calibration code to protect R/W cycle of EEPROM.
//  - 10m/28Mhz band support added.
//  ADX-S V1.3 - 20230808: Add 12m frequency table and 7 band (40,30,20,17,15,12,10) support
//  ADX-S V1.2RX - 20230503: Support RX modification
//  ADX-S V1.2 - 20230419: Ported CAT code based on ADX firmware. Cross Band Check is added before TX to protect the final.
//  ADX-S V1.1 - 20230319: Special measures are taken to avoid WWV 15MHz interference to 14MHz FT8.
//  ADX-S V1.0 - 20230304: modified by Adam Rong, BD6CR E-mail: rongxh@gmail.com
//    1. FSK code bug fix by JE1RAV https://github.com/je1rav/QP-7C, now lower and higher audio frequency will be better supported
//    2. RX modified from direct conversion to superheterodyne based on JA9TTT's blog https://ja9ttt.blogspot.com/2018/07/short-wave-radio-design-2.html . 
//    The modification will significantly reduce BCI and improve RX performance.
//    Hardware modifications required or contact BD6CR for a new PCB design or a whole kit: 
//      a. Cut two PCB traces between CD2003 pin 4 (AM MIX) and 0.47uF capacitor or 1uF in ADX UNO for audio output, and between CD2003 pin 7 (AM IF) and 5V trace.
//      b. Add a 5.1pF ceramic capacitor (C25) between SI5351 CLK2 and CD2003 pin 4 (AM MIX), and use an extension wire if required.
//      c. Add a 455kHz ceramic filter MURATA PFB455JR or equivalent (FL1) to CD2003 pin 4 (AM MIX), 6 (VCC) and 7 (AM IF), same as CD2003 reference circuit.    
//      d. Add a wire between CD2003 pin 11 (DET OUT) and 0.47uF capacitor or 1uF in ADX UNO for audio output.
//    3. Add 1kHz tone to manual TX (freq + 1000) for monitoring at the carrier frequency.
//    4. Changed the way to switch band by pressing and holding SW1 and power on.
//*********************************************************************************************************
// Required Libraries
// ----------------------------------------------------------------------------------------------------------------------
// Etherkit Si5351 (Needs to be installed via Library Manager to arduino ide) - SI5351 Library by Jason Mildrum (NT7S) - https://github.com/etherkit/Si5351Arduino
//*****************************************************************************************************
//* IMPORTANT NOTE: Use V2.1.3 of NT7S SI5351 Library. This is the only version compatible with ADX!!!*
//*****************************************************************************************************
// Arduino "Wire.h" I2C library(built-into arduino ide)
// Arduino "EEPROM.h" EEPROM Library(built-into arduino ide)
//*************************************[ LICENCE and CREDITS ]*********************************************
//  FSK TX Signal Generation code by: Burkhard Kainka(DK7JD) - http://elektronik-labor.de/HF/SDRtxFSK2.html
//  SI5351 Library by Jason Mildrum (NT7S) - https://github.com/etherkit/Si5351Arduino

// License
// -------
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//*****************[ SI5351 VFO CALIBRATION PROCEDURE ]****************************************
// For SI5351 VFO Calibration Procedure follow these steps:
// 1 - Connect CAL test point and GND test point on ADX PCB to a Frequency meter or Scope that can measure 1 Mhz up to 1Hz accurately.
// 2 - Press SW2 / --->(CAL) pushbutton and hold.
// 4-  Power up with 12V or with 5V by using arduino USB socket while still pressing SW2 / --->(CAL) pushbutton. 
// 5 - FT8 and WSPR LEDs will flash 3 times and stay lit. Now Release SW2 / --->(CAL). Now Calibration mode is active.
// 6 - Using SW1(<---) and SW2(--->) pushbuttons change the Calibration frequency.
// 7 - Try to read 1 Mhz = 1000000 Hz exact on  Frequency counter or Oscilloscope. 
//     The waveform is Square wave so freqency calculation can be performed esaily.
// 8 - If you read as accurate as possible 1000000 Hz then calibration is done. 
// 9 - Now we must save this calibration value to EEPROM location. 
//     In order to save calibration value, press TX button briefly. TX LED will flash 3 times which indicates that Calibration value is saved.
// 10- Power off ADX.
 
//*******************************[ LIBRARIES ]*************************************************
#include <si5351.h>
#include "Wire.h"
#include <EEPROM.h>

//int  test_counter= 0;
//*******************************[ VARIABLE DECLERATIONS ]*************************************
uint32_t val;
int temp;
uint32_t val_EE; 
int addr = 0;
int mode;
unsigned long freq; 
unsigned long ifreq;    //IF frquency, default to 464570Hz or 447430Hz for the MURATA PFB455JR ceramic filter
int32_t cal_factor;
int TX_State = 0;
int bfo = 1;

//------------ CAT Variables
int cat_stat = 0;
int CAT_mode = 2;   //USB

// int TxStatus = 0; //0 =  RX 1=TX 
int attnow = 0; //0 = no ATT, 1 = ATT

//------------ Audio Stream Variables  //  added by JE1RAV
long ADC_offset;
int Audio_stream = 0;
int RX_streaming = 0;
int TX_streaming = 0;

unsigned long old_freq;
int16_t mono_prev_p=0; 
float delta_prev_p=0;
uint32_t sampling_p=0;
uint8_t cycle_p = 0;
int16_t mono_prev_n=0;
float delta_prev_n=0;
uint32_t sampling_n=0;
uint8_t cycle_n = 0;
uint32_t mean_audio_freq_p = 0;
uint32_t mean_audio_freq_n = 0;
uint32_t mean_audio_freq;
unsigned long time_prev = 0;

unsigned long Cal_freq = 1000000UL; // Calibration Frequency: 1 Mhz = 1000000 Hz
unsigned long F_FT8;
unsigned long F_FT4; 
unsigned long F_JS8;
unsigned long F_WSPR;
//**********************************[ BAND SELECT ]************************************************
// ADX can support up to 4 bands on board. Those 4 bands needs to be assigned to Band1 ... Band4 from supported 8 bands.
// To change bands press and hold SW1 and power on. Band LED will flash 3 times briefly and stay lit for the stored band. also TX LED will be lit to indicate
// that Band select mode is active. Now change band bank by pressing SW1(<---) or SW2(--->). When desired band bank is selected press TX button briefly to exit band select mode. 
// Now the new selected band bank will flash 3 times and then stored mode LED will be lit. 
// TX won't activate when changing bands so don't worry on pressing TX button when changing bands in band mode.
// Assign your prefered bands to B1,B2,B3 and B4
// Supported Bands are: 80m, 40m, 30m, 20m,17m, 15m, 12, 10m
unsigned int Band_slot = 0;  //0,1,2,3,4,5,6
unsigned int Band[7] = {40,30,20,17,15,12,10};      //B1, 2, 3, 4, 5, 6, 7 = 40,30,20,17,15,12,10

// **********************************[ DEFINE's ]***********************************************
#define UP 2    // UP Switch
#define DOWN 3  // DOWN Switch
#define TXSW 4  // TX Switch
#define ATT 5 // RX Attenuator Switch

#define TX 13 //TX LED
#define WSPR  9 //WSPR LED 
#define JS8  10 //JS8 LED
#define FT4  11 //FT4 LED
#define FT8 12 //FT8 LED

#define FSK_INPUT 7 // FSK input
#define RX  8 // RX SWITCH
#define SI5351_REF 		25000000UL  //change this to the frequency of the crystal on your si5351’s PCB, usually 25 or 27 MHz

#define ADC_INPUT A2 // ADC input pin
#define TX_SAMPLING_RATE 11520
#define RX_SAMPLING_RATE 7812

Si5351 si5351;

//*************************************[ SETUP FUNCTION ]************************************** 
void setup()
{
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(TXSW, INPUT);
  pinMode(ATT, OUTPUT);  
  pinMode(TX, OUTPUT);
  pinMode(WSPR, OUTPUT);
  pinMode(JS8, OUTPUT);
  pinMode(FT4, OUTPUT);
  pinMode(FT8, OUTPUT);
  pinMode(RX, OUTPUT);

  digitalWrite(RX,HIGH);    //enable RX and disable TX first
  digitalWrite(ATT,attnow);    //default no ATT.

  //SET UP SERIAL FOR CAT CONTROL
  Serial.begin(115200); 
  Serial.setTimeout(3); 

  Wire.begin();   
  Wire.setClock(400000);

  addr = 30;
  EEPROM.get(addr,temp);
  
  if (temp != 100) {
    addr = 10; 
    cal_factor = 100000;
    EEPROM.put(addr, cal_factor); 
    addr = 40;
    temp = 4;
    EEPROM.put(addr,temp);
    addr = 30;
    temp = 100;
    EEPROM.put(addr,temp);
    addr = 50;
    temp = 1;
    EEPROM.put(addr,temp);
  }
  else  {
    //--------------- EEPROM INIT VALUES
    addr = 30;
    EEPROM.get(addr,temp);
    addr = 10;
    EEPROM.get(addr,cal_factor);
    addr = 40;
    EEPROM.get(addr,mode);
    mode = mode % 4;
    addr = 50;
    EEPROM.get(addr,Band_slot);
    Band_slot = Band_slot % 7;
  }  

  Band_assign();
  si5351.set_clock_pwr(SI5351_CLK2, 0); // Turn off Calibration Clock

  //------------------------------- SET SI5351 VFO -----------------------------------  
  // The crystal load value needs to match in order to have an accurate calibration
  si5351.init(SI5351_CRYSTAL_LOAD_10PF, 0, 0);
  si5351.set_correction(cal_factor, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);// SET For Max Power
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA); // Set for reduced power for RX 
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA); // Set for max power for RX IF, added by BD6CR

    
  if ( digitalRead(DOWN) == LOW ) {  //You must reset the system to exit calibration
    Calibration();
  }

  if (digitalRead(UP) == LOW ) {     //You can go on after band select, if things are no problem.
    Band_Select();
  }
/*
  if (digitalRead(TXSW) == LOW ) {     //You can go on after band select, if things are no problem.
    bfo = 0;
    ifreq = 455000UL;
  }
*/

/*  moved to the function "void start_analog_comparator()"
  // ----------------Timer1 and analog comparator----------------------
  TCCR1A = 0x00;
  TCCR1B = 0x01; // Timer1 Timer 16 MHz
  TCCR1B = 0x81; // Timer1 Input Capture Noise Canceller
  ACSR |= (1<<ACIC);  // Analog Comparator Capture Input
*/
  start_analog_comparator();

  pinMode(FSK_INPUT, INPUT); //PD7 = AN1 = HiZ, PD6 = AN0 = 0
  digitalWrite(RX,LOW);

  //set into RX
  RX_receive();
  Set_frequency();
}
//**************************[ END OF SETUP FUNCTION ]************************

//***************************[ Main LOOP Function ]**************************
void loop()
{  
 // CAT CHECK SERIAL FOR NEW DATA FROM WSJT-X AND RESPOND TO QUERIES

  if (Serial.available() > 0){
    if (cat_stat == 0){
      digitalWrite(WSPR, LOW); 
      digitalWrite(JS8, HIGH); 
      digitalWrite(FT4, HIGH); 
      digitalWrite(FT8, LOW); 
      cat_stat = 1; 
    }
    CAT_control(); 
  }

  if (keypressed(UP) && keypressed(DOWN)) {
    attnow = !attnow;
    digitalWrite(ATT,attnow);
  }

 if (keypressed(UP)&&(TX_State == 0)) {
    if (cat_stat == 0) {
    mode = mode+3;
    mode = mode % 4;
    addr = 40;
    EEPROM.put(addr, mode); 
    }
    else freq = freq - 5000;  //manual tuning for BC
  }

  if (keypressed(DOWN)&&(TX_State == 0)) {
    if (cat_stat == 0) {
    mode++;
    mode = mode % 4;
    addr = 40;
    EEPROM.put(addr, mode); 
    }
    else freq = freq + 5000;  //manual tuning for BC
  }

  if (cat_stat == 0)   Mode_assign();

  if (freq != old_freq)
  {
    Set_frequency();
  }

  if (keypressed(TXSW)) {
    if (cat_stat == 0) {
      ManualTX();
    } 
    else {
      if (bfo == 1) {
        bfo = 0;
        Freq_assign();
        si5351.output_enable(SI5351_CLK2, 0);
      }
      else {
        bfo = 1;
        Freq_assign();
      }
    }
  }
 
  if (Audio_stream == 0)    //analog audio comparator
  {
  // The following code is from JE1RAV https://github.com/je1rav/QP-7C
  //(Using 3 cycles for timer sampling to improve the precision of frequency measurements)
  //(Against overflow in low frequency measurements)
    
    int FSK = 1;
    int FSKtx = 0;

    while (FSK>0){
      int Nsignal = 10;
      int Ncycle01 = 0;
      int Ncycle12 = 0;
      int Ncycle23 = 0;
      int Ncycle34 = 0;
      unsigned int d1=1,d2=2,d3=3,d4=4;
  
      TCNT1 = 0;  
      while (ACSR &(1<<ACO)){
        if (TIFR1&(1<<TOV1)) {
          Nsignal--;
          TIFR1 = _BV(TOV1);
          if (Nsignal <= 0) {break;}
        }
      }
      while ((ACSR &(1<<ACO))==0){
        if (TIFR1&(1<<TOV1)) {
          Nsignal--;
          TIFR1 = _BV(TOV1);
          if (Nsignal <= 0) {break;}
        }
      }
      if (Nsignal <= 0) {break;}
      TCNT1 = 0;
      while (ACSR &(1<<ACO)){
          if (TIFR1&(1<<TOV1)) {
          Ncycle01++;
          TIFR1 = _BV(TOV1);
          if (Ncycle01 >= 2) {break;}
        }
      }
      d1 = ICR1;  
      while ((ACSR &(1<<ACO))==0){
        if (TIFR1&(1<<TOV1)) {
          Ncycle12++;
          TIFR1 = _BV(TOV1);
          if (Ncycle12 >= 3) {break;}      
        }
      } 
      while (ACSR &(1<<ACO)){
        if (TIFR1&(1<<TOV1)) {
          Ncycle12++;
          TIFR1 = _BV(TOV1);
          if (Ncycle12 >= 6) {break;}
        }
      }
      d2 = ICR1;
      while ((ACSR &(1<<ACO))==0){
        if (TIFR1&(1<<TOV1)) {
          Ncycle23++;
          TIFR1 = _BV(TOV1);
          if (Ncycle23 >= 3) {break;}
        }
      } 
      while (ACSR &(1<<ACO)){
        if (TIFR1&(1<<TOV1)) {
        Ncycle23++;
        TIFR1 = _BV(TOV1);
        if (Ncycle23 >= 6) {break;}
        }
      } 
      d3 = ICR1;
      while ((ACSR &(1<<ACO))==0){
        if (TIFR1&(1<<TOV1)) {
          Ncycle34++;
          TIFR1 = _BV(TOV1);
          if (Ncycle34 >= 3) {break;}
        }
      } 
      while (ACSR &(1<<ACO)){
        if (TIFR1&(1<<TOV1)) {
          Ncycle34++;
          TIFR1 = _BV(TOV1);
          if (Ncycle34 >= 6) {break;}
        }
      } 
      d4 = ICR1;
      unsigned long codefreq1 = 1600000000/(65536*Ncycle12+d2-d1);
      unsigned long codefreq2 = 1600000000/(65536*Ncycle23+d3-d2);
      unsigned long codefreq3 = 1600000000/(65536*Ncycle34+d4-d3);
      unsigned long codefreq = (codefreq1 + codefreq2 + codefreq3)/3;
      if (d3==d4) codefreq = 5000;     
      if ((codefreq < 310000) and  (codefreq >= 10000)) {
        if (FSKtx == 0 && crossbandcheck()){
          TX_State = 1;
          digitalWrite(RX,LOW);
          digitalWrite(TX,HIGH);
          delay(10);
          si5351.output_enable(SI5351_CLK1, 0);   //RX off
          si5351.output_enable(SI5351_CLK2, 0);   //RX IF off as well, added by BD6CR
          si5351.output_enable(SI5351_CLK0, 1);   // TX on
        }
        si5351.set_freq((freq * 100 + codefreq), SI5351_CLK0);  

        if(Serial.available() > 0) CAT_control(); 

        FSKtx = 1;
        }
        else{
        FSK--;
      }
    }
    RX_receive();
    FSKtx = 0; 
  }
}
//*********************[ END OF MAIN LOOP FUNCTION ]*************************

//********************************************************************************
//******************************** [ FUNCTIONS ] *********************************
//********************************************************************************
//--The function "void CAT_control(void)" is modified for audio stream over CAT, which is used in (tr)uSDX.
void CAT_control(void)
{ 
//------------ CAT Variables
  String received = "";
  String receivedPart0;
  String receivedPart1;
  String receivedPart2;    
  String command;
  String command2;  
  String parameter;
  String parameter2; 
  String sent = "";
  String sent2;

  if (TX_streaming == 1){
    while ((Serial.available() > 0)) {
      uint8_t received_data = (uint8_t) Serial.read();
      if (received_data == 59) {
        TX_streaming = 2;
        break;
      }
      else {
        trusdr_audio(received_data - 128);
      }
    }
  }

  if (TX_streaming == 1){
    return;
  }

  if (RX_streaming == 1){
    stop_RX_streaming();
    Serial.flush();
    Serial.print(";"); 
  }

  if ((Serial.available() > 0)) {
    received = Serial.readString();  
  }
  
  received.toUpperCase();  
  received.replace("\n","");  

  String data = "";
  int bufferIndex = 0;
  for (int i = 0; i < received.length(); ++i)
  {
    char c = received[i];
    if (c != ';')
    {
      data += c;
    }
    else if (data != "")
    {
      if (bufferIndex == 0)
      {  
        data += '\0';
        receivedPart0 = data;
        bufferIndex++;
        data = "";
      }
      else if (bufferIndex == 1)
      {  
        data += '\0';
        receivedPart1 = data;
        bufferIndex++;
        data = "";
      }
      else
      {  
        data += '\0';
        receivedPart2 = data;
        bufferIndex++;
        data = "";
      }
    }
  }

  if (receivedPart0.length() <2 || receivedPart0.length() > 14) {
    return;
  }

  command = receivedPart0.substring(0,2);
  parameter = receivedPart0.substring(2,receivedPart0.length());
  command2 = receivedPart1.substring(0,2);    
  parameter2 = receivedPart1.substring(2,receivedPart1.length());


  if (command == "TX")
  {   
    if (crossbandcheck()){      
      TX_transmit();
      RX_streaming = 0;
      if (Audio_stream == 2){
        TX_streaming = 1;
      }
      sent = "TX0;";
    }
  }

  else if (command == "RX")  
  {  
    RX_receive();
    TX_streaming = 0;
    if (Audio_stream == 2){
      RX_streaming = 1;
    }
    sent = "RX0;";
  }

  else if (command == "FA")  
  {  
    if (parameter != "")  
    {  
      freq = parameter.toInt();
      Set_frequency();
      RX_receive();
      //VfoRx = VfoTx;   
    }
    sent = "FA" // Return 11 digit frequency in Hz.  
          + String("00000000000").substring(0,11-(String(freq).length()))   
          + String(freq) + ";";     
  }

  if (command == "FB")  
  {  
    sent = "FB" // Return 11 digit frequency in Hz.  
          + String("00000000000").substring(0,11-(String(freq).length()))   
          + String(freq) + ";";     
  }

  else if (command == "PS")  
  {  
    sent = "PS1;";
  }


  else  if (command == "ID")  
  {  
    //sent = "ID019;";   //TS-2000
    sent = "ID020;";   //TS-480
  }

  else if (command == "AI")  
  {
    sent = "AI0;"; 
  }

  else if (command == "IF")  
  {
    if (TX_State == 1)
    {  
      sent = "IF" // Return 11 digit frequency in Hz.  
          + String("00000000000").substring(0,11-(String(freq).length()))   
          //+ String(freq) + "0000" + "+" + "00000" + "0" + "0" + "0" + "00" + "1" + String(CAT_mode) + "0" + "0" + "0" + "0" + "00" + "0" + ";";   //TS-2000
          + String(freq) + "00000+000000000020000000;";   //TS-480 
    } 
    else
    {  
      sent = "IF" // Return 11 digit frequency in Hz.  
          + String("00000000000").substring(0,11-(String(freq).length()))   
          //+ String(freq) + "0000" + "+" + "00000" + "0" + "0" + "0" + "00" + "0" + String(CAT_mode) + "0" + "0" + "0" + "0" + "00" + "0" + ";";   //TS-2000
          + String(freq) + "00000+000000000020000000;";   //TS-480
    } 
  }
  
  else if (command == "MD")  
    {  
      sent = "MD2;";
    }

  else if (command == "FW")  
    {  
      sent = "FW0000;";
    }

  else if (command == "UA")
  {
    //if (TX_State == 0)
    {
      if (parameter == "0")  
      {  
        Audio_stream = 0;
        RX_streaming = 0;
        TX_streaming = 0;
        start_analog_comparator();
        //sent = "UA0;";
      }
      else if ((parameter == "1") || (parameter == "2"))
      {  
        Audio_stream = 2;
        RX_streaming = 1;
        //sent = "UA2;";
      }
    }
  }

//------------------------------------------------------------------------------      
  if (command2 == "ID")  
  {  
    //sent2 = "ID019;";   //TS-2000
    sent2 = "ID020;";   //TS-480
  }
  else if (command2 == "TX")
  {   
    if (crossbandcheck()){      
      TX_transmit();
      RX_streaming = 0;
      if (Audio_stream == 2){
        TX_streaming = 1;
      }
      sent = "TX0;";
    }
  }
  else if (command2 == "RX")  
  {  
    RX_receive();
    TX_streaming = 0;
    if (Audio_stream == 2){
      RX_streaming = 1;
    }
    sent2 = "RX0;";
  }
  else if (command2 == "UA")
  {
    //if (TX_State == 0)
    {
      if (parameter2 == "0")  
      {  
        Audio_stream = 0;
        RX_streaming = 0;
        TX_streaming = 0;
        start_analog_comparator();
        //sent2 = "UA0;";
      }
      else if ((parameter2 == "1") || (parameter2 == "2"))
      {  
        Audio_stream = 2;
        RX_streaming = 1;
        //sent2 = "UA2;";
      }
    }
  }
if  (bufferIndex > 0) 
  if (bufferIndex == 2) Serial.print(sent2);
  Serial.flush();
  Serial.print(sent);
  Serial.flush();
  if (RX_streaming == 1){
    RX_receive();
    Serial.write("US");
    Serial.flush();
    start_RX_streaming();
  }
  if (TX_streaming == 2){
    TX_streaming = 1;
  }
  \
}

//************************************[ Cross Band Check ]**********************************
unsigned int crossbandcheck()
{
  unsigned int bandnow = 0;
  switch (freq / 1000000) {
    case 3:
      bandnow = 80;
      break;
    case 7:
      bandnow = 40;
      break;
    case 10:
      bandnow = 30;
      break;
    case 14:
      bandnow = 20;
      break;
    case 18:
      bandnow = 17;
      break;
    case 21:
      bandnow = 15;
      break;
    case 24:
      bandnow = 12;
      break;
    case 28:
      bandnow = 10;
      break;
    default:
      bandnow = 0;
      break;
  }
  if ((Band[Band_slot] == bandnow) && freqcheck(freq))
    return (1);
  else
    return (0);
} 

//checking for the prevention of out-of-band transmission
int freqcheck(unsigned long frequency)  // retern 0=out-of-band, 1=in-band
{
  if (frequency < 7000000) {
    return 0;
  }
  else if (frequency > 7200000 && frequency < 10100000) {
    return 0;
  }
  else if (frequency > 10150000 && frequency < 14000000) {
    return 0;
  }
  else if (frequency > 14350000 && frequency < 18068000) {
    return 0;
  }
  else if (frequency > 18168000 && frequency < 21000000) {
    return 0;
  }
  else if (frequency > 21450000 && frequency < 24890000) {
    return 0;
  }
  else if (frequency > 24990000 && frequency < 28000000) {
    return 0;
  }
  else if (frequency > 29700000 && frequency < 50000000) {
    return 0;
  }
  else if (frequency > 54000000) {
    return 0;
  }
  else return 1;
}

//******************************[ Band Slot to LED mapping ]*****************************
void band2led(int band_slot_number) 
{
  switch (band_slot_number) {
    case 0:
    digitalWrite(WSPR, HIGH);
    break;
    case 1:
    digitalWrite(WSPR, HIGH);
    digitalWrite(JS8, HIGH);
    break;    
    case 2:
    digitalWrite(JS8, HIGH);
    break;
    case 3:
    digitalWrite(JS8, HIGH); 
    digitalWrite(FT4, HIGH);
    break;  
    case 4:
    digitalWrite(FT4, HIGH);
    break;
    case 5:
    digitalWrite(FT4, HIGH);
    digitalWrite(FT8, HIGH);
    break;  
    case 6:
    digitalWrite(FT8, HIGH);
    break;
  }
}

//************************************[ Blink 3 Times ]**********************************
void blink3times(int band_slot_num) 
{
  for (int i=0;i<3;i++) {
    band2led(band_slot_num);
    delay(250);
    ledalloff();
    delay(250);
  }
}

//************************************[ 4 LEDs all off ]**********************************
void ledalloff() 
{
  digitalWrite(WSPR, LOW);
  digitalWrite(JS8, LOW);
  digitalWrite(FT4, LOW);
  digitalWrite(FT8, LOW);
}

//*************************[ Check if specific key is pressed ]**************************
int keypressed(int key_number)
{
   if (!digitalRead(key_number)) {
    delay(70); 
    if (!digitalRead(key_number)) {
      return (1);
    }
  return (0);
}
return (0);
}


//************************************[ MODE Assign ]**********************************
void Mode_assign(void) 
{
  addr = 40;
  EEPROM.get(addr,mode);
  mode = mode % 4;

  if ( mode == 0){
    freq = F_WSPR;
  }
  else if ( mode == 1){
    freq = F_JS8;
  }
  else if ( mode == 2){
    freq = F_FT4;
  }
  else if ( mode == 3){
    freq = F_FT8;
  }
  if (cat_stat == 0) {
    ledalloff();
    digitalWrite(WSPR+mode,HIGH);
  }
}

//*********************[ Band dependent Frequency Assign Function ]********************
void Freq_assign() 
{
  Band_slot = Band_slot % 7;

//---------- 80m/3.5Mhz 
         if (Band[Band_slot] == 80){

            F_FT8 = 3573000;
            F_FT4 = 3575000;
            F_JS8 = 3578000;
            F_WSPR = 3568600;
            ifreq = 460570UL;
          }

//---------- 40m/7 Mhz 
          if (Band[Band_slot] == 40){

            F_FT8 = 7074000;
            F_FT4 = 7047500;
            //F_JS8 = 7078000;
            F_JS8 = 7041000;
            F_WSPR = 7038600;
            ifreq = 460570UL;
          }


//---------- 30m/10 Mhz 
          if (Band[Band_slot] == 30){

            F_FT8 = 10136000;
            F_FT4 = 10140000;
            F_JS8 = 10130000;
            F_WSPR = 10138700;
            ifreq = 460570UL;
          }


//---------- 20m/14 Mhz 
          if (Band[Band_slot] == 20){

            F_FT8 = 14074000;
            F_FT4 = 14080000;
            F_JS8 = 14078000;
            F_WSPR = 14095600;
            ifreq = 449430UL; //special cure for WWV 15MHz interference, thanks VK3YE for feedback
          }


 //---------- 17m/18 Mhz 
          if (Band[Band_slot] == 17){

            F_FT8 = 18100000;
            F_FT4 = 18104000;
            F_JS8 = 18104000;
            F_WSPR = 18104600;
            ifreq = 460570UL;
          } 

//---------- 15m/ 21Mhz 
          if (Band[Band_slot] == 15){

            F_FT8 = 21074000;
            F_FT4 = 21140000;
            F_JS8 = 21078000;
            F_WSPR = 21094600;
            ifreq = 460570UL;
          } 

//---------- 12m/ 24Mhz 
          if (Band[Band_slot] == 12){

            F_FT8 = 24915000;
            F_FT4 = 24919000;
            F_JS8 = 24922000;
            F_WSPR = 24924600;
            ifreq = 460570UL;
          } 
          
//---------- 10m/ 28Mhz 
          if (Band[Band_slot] == 10){

            F_FT8 = 28074000;
            F_FT4 = 28180000;
            F_JS8 = 28078000;
            F_WSPR = 28124600;
            ifreq = 460570UL;
          }
  if (bfo == 0) {
    ifreq = 455000UL;
  }                  
}

//******************************[ Band  Assign Function ]******************************
void Band_assign() 
{
  addr = 50;
  EEPROM.get(addr,Band_slot);
  Band_slot = Band_slot % 7;
  ledalloff();
  blink3times(Band_slot);
  delay(500);
  Freq_assign();
  Mode_assign();
}

//*******************************[ Manual TX FUNCTION ]********************************
void ManualTX() 
{
 
 digitalWrite(RX,LOW);
 si5351.output_enable(SI5351_CLK1, 0);   //RX off
 si5351.output_enable(SI5351_CLK2, 0);   //RX IF off as well, added by BD6CR
 delay(10);

TXON:  

  digitalWrite(TX,1);
  si5351.set_freq((freq+1000)*100ULL, SI5351_CLK0); //1000Hz audio for USB mode
  si5351.output_enable(SI5351_CLK0, 1);   //TX on
  TX_State = 1;
  if (digitalRead(TXSW)) {
    goto EXIT_TX;
  }
goto TXON;

EXIT_TX:
  RX_receive();
}

//******************************[ BAND SELECT Function]********************************
void Band_Select() 
{
  digitalWrite(TX,1);
  addr = 50; 
  EEPROM.get(addr,Band_slot);
  ledalloff();
  blink3times(Band_slot);

  Band_cont:
  
    ledalloff();
    band2led(Band_slot);

    if (keypressed(UP)) {
      Band_slot=Band_slot+6;
    }
    if (keypressed(DOWN)) {
      Band_slot++;
    } 
    Band_slot = Band_slot % 7;

    if (keypressed(TXSW)) {
      digitalWrite(TX,0);
      goto Band_exit;
    }
    delay(30);

  goto Band_cont;

  Band_exit:

    addr = 50;
    EEPROM.put(addr, Band_slot); 
    Band_assign();
}

//************************** [SI5351 VFO Calibration Function] ************************
void Calibration() 
{
  ledalloff();
  digitalWrite(WSPR, HIGH); 
  digitalWrite(FT8, HIGH);

  addr = 10;
  EEPROM.get(addr, cal_factor); 

  while (1) //loop forever until reset
  {
    if (keypressed(UP)) {
      cal_factor = cal_factor - 100;
    }

    if (keypressed(DOWN)) {
      cal_factor = cal_factor + 100;
    }
      
    si5351.set_correction(cal_factor, SI5351_PLL_INPUT_XO);
    // Set CLK2 output
    si5351.set_freq(Cal_freq * 100, SI5351_CLK2);
    si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA); // Set for lower power for Calibration
    si5351.set_clock_pwr(SI5351_CLK2, 1); // Enable clock2 

    if (keypressed(TXSW)) {
      addr = 10;
      EEPROM.put(addr, cal_factor); 
      digitalWrite(TX,HIGH);
      delay(250);
      digitalWrite(TX,LOW);
      delay(250);
      digitalWrite(TX,HIGH);
      delay(250);
      digitalWrite(TX,LOW);
      delay(250);
      digitalWrite(TX,HIGH);
      delay(250);
      digitalWrite(TX,LOW);
      delay(250);
    } 
  }
}

//********** [ ADDED FUNCTIONS for audio stream over CAT] ************
//********************************************************************
void RX_receive()
{
  si5351.output_enable(SI5351_CLK0, 0);   //TX off
  delay(1);
  digitalWrite(TX,LOW);
  digitalWrite(RX,HIGH);
  delay(3);
  si5351.output_enable(SI5351_CLK1, 1);   //RX on
  delay(1);
  if (bfo == 1) {
  si5351.output_enable(SI5351_CLK2, 1);   //RX IF on as well, added by BD6CR
  }
  TX_State = 0;
  delay(1);
}

void TX_transmit()
{
    digitalWrite(RX,LOW);
    digitalWrite(TX,HIGH);
    si5351.output_enable(SI5351_CLK1, 0);   //RX off
    if (bfo == 1) {
      si5351.output_enable(SI5351_CLK2, 0);   //RX IF off as well, added by BD6CR
    }
    si5351.output_enable(SI5351_CLK0, 1);   //TX on
    delay(10);
    TX_State = 1;
}

void Set_frequency()
{
    if (Band[Band_slot] == 20) {
      si5351.set_freq((freq-ifreq)*100ULL, SI5351_CLK1);    //special cure to avoid WWV 15MHz interference, thanks VK3YE for feedback
    }
    else {
      si5351.set_freq((freq+ifreq)*100ULL, SI5351_CLK1);    //Set RX LO frequency, added by BD6CR
    }
    if (bfo == 1) {
      si5351.set_freq(ifreq*100ULL, SI5351_CLK2);   //RX IF frequency = ifreq, added by BD6CR
    }
    si5351.set_freq(freq*100ULL, SI5351_CLK0); // set frequency back
    old_freq = freq;

    //ADC
    init_ADC();
}

void start_analog_comparator()
{
  // ----------------Timer1 and analog comparator----------------------
  TCCR1A = 0x00;
  TCCR1B = 0x01; // Timer1 Timer 16 MHz
  TCCR1B = 0x81; // Timer1 Input Capture Noise Canceller
  ACSR |= (1<<ACIC);  // Analog Comparator Capture Input
}

void start_RX_streaming()
{
  //--------TIMER 1----------------------------------
  // Set up Timer 1 to send a sample every interrupt.
  // This will interrupt at the sample rate (7812 hz)
  //
  cli();
  // Set CTC mode (Clear Timer on Compare Match)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));
  // No prescaler
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = F_CPU / RX_SAMPLING_RATE;  //added  16e6 / 7812 = 2048
  //Timer/Counter Interrupt Mask Register
  // Enable interrupt when TCNT1 == OCR1A
  TIMSK1 |= _BV(OCIE1A);
  //Enable Interrupts
  sei();  
}

void stop_RX_streaming(void) {
  cli();
  // Disable playback per-sample interrupt.
  TIMSK1 &= ~_BV(OCIE1A);
  // Disable the per-sample timer completely.
  TCCR1B &= ~_BV(CS10);
  sei();
}//End StopPlayback

//Interrupt Service Routine (ISR)
// This is called at 7812 Hz to send the received data.
ISR(TIMER1_COMPA_vect) {
  if (TX_State == 0){
    if (RX_streaming == 0)
    {
      RX_streaming = 1;
      Serial.print("US");
    }
    int RXsignal = analogRead(ADC_INPUT);
    uint8_t RXsent = (RXsignal-ADC_offset)/4 + 128;  
    if (RXsent == 59) RXsent = 58;
    Serial.write(RXsent);
  }
  else if (RX_streaming == 1){
    Serial.print(";");
    RX_streaming = 0;
  }
}//End Interrupt

void init_ADC(void)
{
  // for Audio streaming over serial interface
  // ----------------ADC for receiver----------------------
  ADCSRA = ADCSRA & 0xf8;
  ADCSRA = ADCSRA | 0x04;    //prescaler = 16
  //ADCSRA = ADCSRA | 0x05;    //prescaler = 32

  long long int RXsignal = 0;
  for (long int i =0; i < 1000; i++){
    RXsignal += analogRead(ADC_INPUT);
    delay(1);
  }
  ADC_offset = RXsignal /1000;
  //Serial.println(ADC_offset);
}

void trusdr_audio(int mono){
   	
  if ((mono_prev_p < 0) && (mono >= 0)) {
    //if ((mono == 0) && (((float)mono_prev_p * 1.8 - (float)mono_preprev_p < 0.0) || ((float)mono_prev_p * 2.02 - (float)mono_preprev_p > 0.0))) {    //Detect the sudden drop to zero due to the end of transmission
    //		break;
    //}

    int16_t difference = mono - mono_prev_p;
    float delta_p = (float)mono_prev_p / (float)difference;
    float period = (1.0 + delta_prev_p) + (float)sampling_p - delta_p;
    int32_t audio_freq_p = TX_SAMPLING_RATE/period; // in Hz    
    if ((audio_freq_p>200) && (audio_freq_p<2850) && (difference > 3)) {
      //audio_frequency_p[cycle_p] = audio_freq_p;   
      mean_audio_freq_p += audio_freq_p;
      cycle_p++;
    }
    delta_prev_p = delta_p;
    sampling_p=0;
    mono_prev_p = mono;     
    }
  else {
		sampling_p++;
		mono_prev_p = mono;
  }

  if ((mono_prev_n > 0) && (mono <= 0)) {
    //if ((mono == 0) && (((float)mono_prev_n * 1.8 - (float)mono_preprev_n > 0.0) || ((float)mono_prev_n * 2.02 - (float)mono_preprev_n < 0.0))) {    //Detect the sudden drop to zero due to the end of transmission
    //	break;
    //}
    int16_t difference = mono - mono_prev_n;
    float delta_n = (float)mono_prev_n / (float)difference;
    float period = (1.0 + delta_prev_n) + (float)sampling_n - delta_n;
    int32_t audio_freq_n = TX_SAMPLING_RATE/period; // in Hz    

    if ((audio_freq_n>200) && (audio_freq_n<2850) && (difference < -3)){
      //audio_frequency_n[cycle_n] = audio_freq_n;   
      mean_audio_freq_n += audio_freq_n;
      cycle_n++;
    }
    delta_prev_n = delta_n;
    sampling_n=0;
    mono_prev_n = mono;     
  }
  else {
    sampling_n++;
    mono_prev_n = mono;
  }
  
  unsigned long time_now = millis();
  if (((time_now - time_prev) > 10) && ((cycle_p + cycle_n) > 4)) {
    mean_audio_freq = (mean_audio_freq_p + mean_audio_freq_n)/(cycle_p + cycle_n);
    si5351.set_freq((freq * 100ULL + mean_audio_freq * 100ULL), SI5351_CLK0);
    time_prev = time_now;
    mean_audio_freq_p = 0;
    mean_audio_freq_n = 0;
    cycle_p = 0;
    cycle_n = 0;
  }
}