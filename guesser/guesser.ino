#include <BackgroundAudio.h>
#include <SD.h>
#include <vector>
#include <string.h>
#include "ID3.h"
#include "ID3Callback.h"
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN     16          // Configurable, see typical pin layout above
#define SS_PIN      21         // Configurable, see typical pin layout above
#define SCK_PIN     18
#define MISO_PIN        19
#define MOSI_PIN        23

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

// This are GP pins for SPI0 on the Raspberry Pi Pico board, and connect
// to different *board* level pinouts.  Check the PCB while wiring.
// Only certain pins can be used by the SPI hardware, so if you change
// these be sure they are legal or the program will crash.
// See: https://datasheets.raspberrypi.com/picow/PicoW-A4-Pinout.pdf
const int _MISO = 27;//19;  // AKA SPI RX 
const int _MOSI = 13;//23;  // AKA SPI TX
const int _CS = 17;//5;
const int _SCK = 14;//18;

#ifdef ESP32
#include <ESP32I2SAudio.h>
SPIClass spiSD(HSPI);
ESP32I2SAudio audio(26, 25, 22); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <PWMAudio.h>
#include <I2S.h>
// I2S audio(OUTPUT, 0, 2);
PWMAudio audio(0);
#endif
// We will make a larger buffer because SD cards can sometime take a long time to read
BackgroundAudioMP3Class<RawDataBuffer<16 * 1024>> BMP(audio);

// List of all MP3 files in the root directory
std::vector<String> mp3list;

// The file we're currently playing
File f;

// Read buffer that's better off not in the stack due to its size
uint8_t filebuff[512];

// Recursively scan the card and make a list of all MP3 files in all dirs
void scanDirectory(const char *dirname) {
  File root = SD.open(dirname);
  while (true) {
    f = root.openNextFile();
    if (!f) {
      break;
    }
    String n = f.name();
    n.toLowerCase();
    String path = dirname;
    path += f.name();
    if (f.isDirectory()) {
      if (f.name()[0] == '.') {
        // Ignore . and ..
        continue;
      }
      String sub = dirname;
      sub += f.name();
      sub += "/";
      scanDirectory(sub.c_str());
    } else if (strstr(n.c_str(), ".mp3")) {
      mp3list.push_back(path);
      Serial.printf("ADD: %s\n", path.c_str());
    } else {
      Serial.printf("SKP: %s\n", path.c_str());
    }
    f.close();
  }
  root.close();
}

static void mfrc522_setup() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
	mfrc522.PCD_Init();		// Init MFRC522
	delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
}

void setup() {
  Serial.begin(115200);

  // Ensure a different random sequence every time we start up
#ifdef ESP32
  srand(ESP.getCycleCount());
#else
  srand(rp2040.hwrand32());
#endif

  // Initialize the SD card
  bool sdInitialized = false;
#ifdef ESP32
  spiSD.begin(_SCK, _MISO, _MOSI, _CS);
  sdInitialized = SD.begin(_CS, spiSD);
#else
  if (_MISO == 0 || _MISO == 4 || _MISO == 16) {
    SPI.setRX(_MISO);
    SPI.setTX(_MOSI);
    SPI.setSCK(_SCK);
    sdInitialized = SD.begin(_CS);
  } else if (_MISO == 8 || _MISO == 12) {
    SPI1.setRX(_MISO);
    SPI1.setTX(_MOSI);
    SPI1.setSCK(_SCK);
    sdInitialized = SD.begin(_CS, SPI1);
  } else {
    Serial.println(F("ERROR: Unknown SPI Configuration"));
    while (1) {
      delay(1);
    }
  }
#endif

  if (!sdInitialized) {
    Serial.println("initialization failed!");
    while (1) {
      delay(1000);
#ifdef ESP32
      ESP.restart();
#else
      rp2040.reboot();
#endif
    }
  }

  scanDirectory("/");

  // Start the background player
  BMP.begin();
  BMP.setGain(0.1f);

  mfrc522_setup();
}

	typedef struct {
		byte		size;			// Number of bytes in the UID. 4, 7 or 10.
		byte		uidByte[10];
		byte		sak;			// The SAK (Select acknowledge) byte returned from the PICC after successful selection.
	} Uid;

enum animal {
    CAT,
    DOG,
    ANIMAL_MAX
};

const String fname_animal[ANIMAL_MAX] = {
    [CAT] = "/cat_sfx.mp3",
    [DOG] = "/dog_sfx.mp3",
};

const Uid card_animal[ANIMAL_MAX] = {
    [CAT] = { .size = 4, .uidByte = { 0xEC, 0x75, 0x6E, 0x05 } },
    [DOG] = { .size = 4, .uidByte = { 0x9C, 0x03, 0x6B, 0x05 } },
};

void loop() {
  // When BOOTSEL held, skip to another song
#ifdef ESP32
  if (Serial.available()) {
    f.close();  // Choose another
    while (Serial.available()) {
      Serial.read();
    }
  }
#else
  if (BOOTSEL) {
    f.close();  // Choose another
    // wait for release
    while (BOOTSEL) {
      delay(1);
    }
  }
#endif

  // Stuff the buffer with as much as it will take, only doing full sector reads for performance
  while (f && BMP.availableForWrite() > 512) {
    int len = f.read(filebuff, 512);
    BMP.write(filebuff, len);
    if (len != 512) {
      f.close(); // Short reads == EOF
    }
  }
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    String fname = "";
    int a;
    bool animal_match;
    for (a = CAT; a < ANIMAL_MAX; a++) {
        animal_match = true;
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            if (mfrc522.uid.uidByte[i] != card_animal[a].uidByte[i]) {
                animal_match = false;
                break;
            }
        }
        if (animal_match) {
            break;
        }
    }
    if (animal_match) {
        Serial.print("Animal MATCH: ");
        Serial.println(fname_animal[a]);
        fname = String(fname_animal[a]);
    } else {
        Serial.println("Animal did not MATCHED!");
    }
  if (!f && fname.length() > 0) {
    f = SD.open(fname);
    if (!f) {
        Serial.printf("\n Could not open %s", fname);
        return;
    }
    Serial.printf("\r\n\r\nNow playing: %s\r\n", fname);
    processID3(f, id3Callback);
    auto p = f.position();
    p = p & ~511; // Ensure on a sector boundary, MP3 will resync appropriately
    f.seek(p);
  }

  

  // Could do things like draw a UI on a LCD, check buttons, etc. here on each loop
  // Just be sure to feed the MP3 raw data at a BPS matching the recording mode
  // (i.e. a 128Kb MP3 only needs 128Kb (16K byte) of reads every second).  Don't disable
  // IRQs for long, either, as the audio DMA and calculations re driven off of them.
  //
  // If you don't send data fast enough (underflow) or you stop sending data completely,
  // the audio will silence and continue when more data is available.

}
