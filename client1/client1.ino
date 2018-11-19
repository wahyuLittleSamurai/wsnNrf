#include <SPI.h>    //tambahkan librayr untuk komuniaksi spi
#include <nRF24L01.h> //tambahkan library NRF
#include <RF24.h>
#include <dht.h>  //tambahkan library untuk DHT

#define DHT11_PIN A1  //pin dht 11 di A1

dht DHT;  //object deklarasi untuk dht

const int pinCE = 9;  //pin ce untuk NRF
const int pinCSN = 10; //pin CSN untuk NRF
RF24 radio(pinCE, pinCSN);  //deklarasi object yg digunakan untuk NRF
#define WHICH_NODE 2      //id node
const uint64_t wAddress[] = {0x7878787878LL, 0xB3B4B5B6F1LL, 0xB3B4B5B6CDLL, 0xB3B4B5B6A3LL, 0xB3B4B5B60FLL, 0xB3B4B5B605LL}; //alamat node yg akan digunakan
const uint64_t PTXpipe = wAddress[ WHICH_NODE - 1 ];    //inisialisasi alamat transfer untuk NRF
byte counter = 1; //

long int timeArrival = 0;
long int dataArrival = 0;
long int currentMillis = 0;
int kurangMillis = 0;

int RawValue= 0;
int voltMeter = 0;
int validSync = 0;

char data[32];
String valSplit[7];

boolean timeSync = false;
boolean firstSending = false;

int paketKe = 0;

void setup()  
{ 
  Serial.begin(115200);   //inisialisasi baudrate
  radio.begin();                  //aktifasi untuk NRF
  radio.setPayloadSize(32);       //banyaknya jml data yg di pakai sebanyak 32byte
  radio.setPALevel(RF24_PA_LOW);  //
  radio.setChannel(108);          //setting channel yg digunakan
  radio.openReadingPipe(0,PTXpipe);   //open reading untuk alamat yg digunakan
  
  radio.stopListening();              //stop listening dan start untuk pengiriman data

}

void loop()  
{
  while(!timeSync)  //jika belum sycrone waktu lakukan sync time
  {
    syncTime(); //panggil function sync time
  }
  while(timeSync) //jika sudah sync waktu maka...
  {
    DHT.read11(DHT11_PIN);              //baca dht
    int DhtTemp = int(DHT.temperature); //ambil nilai temperature
    int DhtHumi = int(DHT.humidity);    //ambil nilai humidity
    RawValue = analogRead(A2);          //ambil data RAW dari analog read 2 untuk data ACS
    voltMeter = analogRead(A0);         //ambil data RAW dari sensor tegangan
  
    if((millis() - currentMillis) >= kurangMillis && !firstSending) //jika waktu = 10 detik pertama
    {
      Serial.println("first sending data");

      sprintf(data,"%2d,%2d,%4d,%4d,%2d",DhtTemp, DhtHumi, RawValue, voltMeter, paketKe); //kirim data temperature, humi, raw value ampere, raw value voltmeter dan paketke X
      radio.openWritingPipe(PTXpipe); //open writing untuk alamat NRF
      if (!radio.write( &data, 32 ))  //jika tidak terkirim data maka..
      {  
         Serial.println("Guess delivery failed");   //tampilkan berikut
      } 
      else  //jika berhasil mengirim data maka... 
      { 
        timeArrival = millis();
        Serial.print("Success sending guess: ");
        Serial.println(data);
        radio.startListening();
        unsigned long startTimer = millis(); 
        bool timeout = false; 
        while ( !radio.available() && !timeout )  //jika NRF tidak mendapat balasan 
        { 
          
          if (millis() - startTimer > 200 )   //selama lebih dari 200milisecond maka
          {
            timeout = true; //time out = true
          }
        }
        if (timeout) 
        {
          Serial.println(); 
        }
        else    //tampilkan jika RTO untuk mendapatkan balasan
        { 
          Serial.println("RTO");
        }
        radio.stopListening(); 
      }
      currentMillis = millis();
      firstSending = true;

    }
    if((millis() - currentMillis) >= 10000 && firstSending) //jika 10 detik kedua dan seterusnya
    {
      Serial.println("continous data");

      sprintf(data,"%2d,%2d,%4d,%4d,%2d",DhtTemp, DhtHumi, RawValue, voltMeter, paketKe); //kirimkan data
      radio.openWritingPipe(PTXpipe); 
      if (!radio.write( &data, 32 ))
      {  
         Serial.println("Guess delivery failed");   
      }
      else 
      { 
        timeArrival = millis();
        Serial.print("Success sending guess: ");
        Serial.println(data);
        radio.startListening();
        unsigned long startTimer = millis(); 
        bool timeout = false; 
        while ( !radio.available() && !timeout ) 
        { 
          
          if (millis() - startTimer > 200 ) 
          {
            timeout = true;
          }
        }
        if (timeout) 
        {
          Serial.println(); 
        }
        else  
        { 
          Serial.println("RTO");
        }
        radio.stopListening(); 
      }
      paketKe++;
      currentMillis = millis();
    }
    
   
  }
    
}

void syncTime()     //syncronisasi waktu
{
   sprintf(data,"%3d",dataArrival); //kirimkan data waktu balasan
   radio.openWritingPipe(PTXpipe);  //open writing
   if (!radio.write( &data, 32 )) //ada balasan???
   {  
       Serial.println("Guess delivery failed");   
   }
   else 
   { 
      timeArrival = millis();
      Serial.print("Success sending guess: ");
      Serial.println(data);
      radio.startListening();
      unsigned long startTimer = millis(); 
      bool timeout = false; 
      while ( !radio.available() && !timeout ) 
      { 
        
        if (millis() - startTimer > 200 ) 
        {
          timeout = true;
        }
      }
      if (timeout) 
      {
        Serial.println("Last guess was wrong, try again"); 
      }
      else                            //jika ada balasan
      { 
        Serial.print("timeArrival ");
        dataArrival = millis() - timeArrival; //cari nilai data waktu antara pengiriman dan balasan
        Serial.println(dataArrival);
        radio.read( &data,32);    //baca data yg masuk
        String myData = String(data); //data yg mausk di jadikan string
        parseData(myData, ","); //split data yg masuk

        int serverMillis = valSplit[1].toInt(); //jadikan integer
        int modTenSecond = 9-(valSplit[0].toInt()%10);  //cari nilai 10 detik terdekat
        kurangMillis = (modTenSecond*1000) + (1000-serverMillis); //cari waktu milisecond ke 10 detik terdekat
        
        if(kurangMillis < 10000) 
        {
          if(validSync >= 1)  //lakukan sync waktu selama 2x
          {
            timeSync = true; 
          }
          currentMillis = millis(); 
          radio.stopListening();
          Serial.println();
          Serial.println("Waiting First Sending Data\n");
          validSync++;
        }
        
        Serial.print("kurang berapa: ");
        Serial.println(kurangMillis);
        Serial.println();
      }
      radio.stopListening(); 
   }
   delay(1000);
}

void parseData(String text, String key) //proses split data sama seperti pada server
{
  int countSplitSecond=0;
  int lastIndexSecond=0;
  text += key;

  //Serial.println(text);
  for(int j = 0; j < text.length(); j++)
  {
    if(text.substring(j, j+1) == key)
    {
      valSplit[countSplitSecond] = text.substring(lastIndexSecond,j);
      lastIndexSecond = j + 1;
      //Serial.print(countSplitSecond);
      //Serial.print(":");
      //Serial.println(valSplit[countSplitSecond]);
      countSplitSecond++;
    }
  }
  
}  

