#include <SPI.h> //penambahaan library untuk SPI
#include <nRF24L01.h> //penambahan library NRF
#include <RF24.h>
#include <Wire.h> //penambahan untuk dht
#include "RTClib.h" //penambahan library untuk RTC

RTC_DS3231 rtc; //init object DS3231
int jam, menit, detik;  
long int miliSecond = 0;
int lastSecond = 0;
long int lastMillis = 0;
int timeSending = 0;
int timeEveryNode[6];

String valSplit[7]; 

const int pinCE = 9;    //pin CE untuk pin NRF
const int pinCSN = 10;  //pin CSN untuk pin NRF
RF24 radio(pinCE, pinCSN);  //konfigurasi pin 

const uint64_t rAddress[] = {0x7878787878LL, 0xB3B4B5B6F1LL, 0xB3B4B5B6CDLL, 0xB3B4B5B6A3LL, 0xB3B4B5B60FLL, 0xB3B4B5B605LL }; //alamat untuk 6 buah NRF,0=master,2=client1 dst...

char data[32]; //variable untuk menyimpan data yg dikirim/diterima oleh NRF

void setup()   
{
  Serial.begin(115200);  
  Serial.print("The number they are trying to guess is: "); 
  Serial.println(data); 
  Serial.println();

  if (! rtc.begin()) {    //cek komunikasi Arduino dgn RTC
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {  //cek power untuk RTC
    Serial.println("RTC lost power, lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  radio.begin();  //configurasi NRF

  radio.setPayloadSize(32); //panjang data yg dapat di tampung NRF
  radio.setPALevel(RF24_PA_LOW); 
  radio.setChannel(108);          //channel untuk NRF

  radio.openReadingPipe(0,rAddress[0]); //inisialisasi untuk reading alamat client 1
  radio.openReadingPipe(1,rAddress[1]); // inisialisasi untuk reading alamat client 2
  radio.openReadingPipe(2,rAddress[2]);
  radio.openReadingPipe(3,rAddress[3]);
  radio.openReadingPipe(4,rAddress[4]);
  radio.openReadingPipe(5,rAddress[5]);
  
  radio.startListening(); //inisialisasi awalan untuk penerimaan data

  millis();
  while(millis() <= 15000)  //waktu syncronisasi
  {
    syncTime(); //panggil fungsi sinkronisasi waktu
  }

  radio.startListening(); 
}

void loop()  
{   
  byte pipeNum = 0;

  DateTime now = rtc.now(); //baca RTC
  detik = now.second(); //baca detik sekarang
  jam = now.hour(); //baca jam sekarang
  menit = now.minute(); //baca menit sekarang
  if(detik != lastSecond) //apabila detik sekarang tidak sama dengan detik terakhir
  {
      lastSecond = detik; //simpan detik sebelumnya
      lastMillis = millis();  //simpan nilai milisecond detik
  }
  miliSecond = millis() - lastMillis; //mencari nilai milisecond dari setiap detik
  
  while(radio.available(&pipeNum))  //jika ada data dari NRF maka...
  {
    radio.read( &data, 32 );  //simpan data yg didapat dari NRF ke variable data dengan panjang maksimal 32 byte
    String myData = String(data); //ubah kedalam string
    parseData(myData, ","); //parsing/pisah data yg diterima
    
    float ACSoffset = 2.5;  //nilai offset untuk acs
    float mVperAmp = 0.066; // use 100 for 20A Module and 66 for 30A Module
    float RawValue = valSplit[2].toFloat();
    float Voltage = (RawValue/1024)*5; // Gets you mV
    float Amps = ((Voltage - ACSoffset) / mVperAmp);

    float rawValueVolt = valSplit[3].toFloat();
    float valueVolt = (rawValueVolt/1024) * 5;

    float daya = Amps * valueVolt;  
    
    Serial.println("#################################################");
    Serial.print("Time Arrival:\t");
    Serial.print(jam); Serial.print(":");
    Serial.print(menit); Serial.print(":");
    Serial.print(detik); Serial.print(":");
    Serial.print(miliSecond); Serial.println();
    Serial.print("Received guess from transmitter: "); 
    Serial.println(pipeNum + 1); 
    Serial.println("Data Continous: ");
    Serial.print("Temperat:\t"); Serial.println(valSplit[0].toInt());
    Serial.print("Humidity:\t"); Serial.println(valSplit[1].toInt());
    Serial.print("Ampere:\t\t"); Serial.println(Amps);
    Serial.print("Voltage:\t"); Serial.println(valueVolt);
    Serial.print("Daya:   \t"); Serial.println(daya);
    Serial.print("Paket Ke:\t"); Serial.println(valSplit[4].toInt());
    
  }
  
}

bool sendCorrectNumber(byte xMitter)  //pengecekan dan kirim balik data dari NRF
{
  bool worked;
  
  radio.stopListening();  //berhenti untuk mode baca
  radio.openWritingPipe(rAddress[xMitter]);   //open alamat ke Node yg telah memberikan request

  switch(xMitter) //pilih nilai node yg telah mengirimkan data
  {
    case 3: //jika node yg mengirimkan data merupakan node yg ke 3, maka...
            sprintf(data,"%2d,%3d",detik,miliSecond+timeEveryNode[3]);  //kirimkan nilai detik, milisecond+data waktu pemrosesan
            if(!radio.write(&data, 32))  //kirim data balasan
            {
              worked = false; //jika tidak berhasil
            }
            else              //jika berhasil
            {
              worked = true;  //work = true
            }
            break;
    case 2: 
            if(timeEveryNode[3] > 0)  //jika waktu singkron node ke 3 lebih dari 0
            {
              sprintf(data,"%2d,%3d",detik,miliSecond+timeEveryNode[2]+timeEveryNode[3]); //gabungkan data
              if(!radio.write(&data, 32))   //jika gagal kirim balasan
              { 
                worked = false; 
              }
              else  //jika berhasil kirim balasan 
              {
                worked = true;
              }
            }
            break;
    case 1: 
            if(timeEveryNode[3] > 0 && timeEveryNode[2] > 0)  //jika waktu untuk node 3 dan 2 lebih besar dari 0
            {
              sprintf(data,"%2d,%3d",detik,miliSecond+timeEveryNode[3]+timeEveryNode[1]+timeEveryNode[2]);  //gabungkan data dari detik milidetik+waktu node 2 dan 3
              if(!radio.write(&data, 32))  //jika gagal pengiriman
              {
                worked = false; 
              }
              else                        //jika berhasil pengiriman
              {
                worked = true;
              }
            }
            break;
  }
      
  radio.startListening();   //NRF start untuk penerimaan data
  return worked;  
}
void syncTime()     //function untuk sycn waktu
{
  byte pipeNum = 0; 
  
  DateTime now = rtc.now(); //ambil data RTC
  detik = now.second(); //ambil detik
  jam = now.hour(); //ambil jam
  menit = now.minute(); //ambil data menit
  
  if(detik != lastSecond) //fungsi untuk memulai perhitungan milisecond
  {
      lastSecond = detik; //last second = second
      lastMillis = millis();  //ambil data lastmilisecond
  }
  miliSecond = millis() - lastMillis; //cari nilai milisecond untuk waktu RTC
  
  while(radio.available(&pipeNum))  //jika ada data masuk maka...
  {
    //tampilkan data berikut:
    Serial.println("#################################################");
    Serial.print("Time Arrival:\t");
    Serial.print(jam); Serial.print(":");
    Serial.print(menit); Serial.print(":");
    Serial.print(detik); Serial.print(":");
    Serial.print(miliSecond); Serial.println();

    radio.read( &data, 32 );        //baca data yg masuk
    Serial.print("Received guess from transmitter: "); 
    Serial.println(pipeNum + 1);                //tampilkan node manakah yg mengirimkan data
    Serial.print("They guess number: ");        
    String dataToString = String(data);         //print data yg masuk (dijadikan kedalam string)
    Serial.println(data); 

    String stringArrival = String(data);  
    timeSending = stringArrival.toInt();  //waktu yg didapat dijadikan integer
    timeEveryNode[pipeNum] = timeSending; //simpan data waktu sesuai dengan node
   
    if(sendCorrectNumber(pipeNum))  //validasi data yg masuk
    {
      Serial.println("Correct! You're done."); 
    }
    else 
    {
      Serial.println("Write failed"); 
    }
    Serial.println();
  }
  //delay(200);    
}
void parseData(String text, String key)   //function untuk split data yg masuk
{
  int countSplitSecond=0;
  int lastIndexSecond=0;
  text += key;

  //Serial.println(text);
  for(int j = 0; j < text.length(); j++)  //ulang data awal  masuk  sampai akhir masuk
  {
    if(text.substring(j, j+1) == key) //jika data perulangan = kunci yg di inginkan (,) maka...
    {
      valSplit[countSplitSecond] = text.substring(lastIndexSecond,j); //simpan nilai awal data masuk sampai kunci 
      lastIndexSecond = j + 1;  
      //Serial.print(countSplitSecond);
      //Serial.print(":");
      //Serial.println(valSplit[countSplitSecond]);
      countSplitSecond++;
    }
  }
  
}  

