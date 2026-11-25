#include <string.h>
#include <vector>

#include <BackgroundAudio.h>
#include <ESP32I2SAudio.h>
#include <MFRC522.h>
#include <SD.h>
#include <SPI.h>

#define AMPLIFIER_I2S_BCLK          26
#define AMPLIFIER_I2S_LRCLK         25
#define AMPLIFIER_I2S_DOUT          22

#define MFRC522_SPI_RESET           16
#define MFRC522_SPI_SS              21
#define MFRC522_SPI_SCK             18
#define MFRC522_SPI_MISO            19
#define MFRC522_SPI_MOSI            23

#define SDCARD_SPI_MISO             27
#define SDCARD_SPI_MOSI             13
#define SDCARD_SPI_SS               17
#define SDCARD_SPI_SCK              14

#ifdef BOX_DEBUG
    #define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
    #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
    #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...)   ((void)0)
    #define DEBUG_PRINTLN(...) ((void)0)
    #define DEBUG_PRINTF(...)  ((void)0)
#endif

SPIClass spiSD(HSPI);
MFRC522 mfrc522(MFRC522_SPI_SS, MFRC522_SPI_RESET);
ESP32I2SAudio audio(AMPLIFIER_I2S_BCLK, AMPLIFIER_I2S_LRCLK, AMPLIFIER_I2S_DOUT);
BackgroundAudioMP3Class<RawDataBuffer<16 * 1024>> BMP(audio);

// List of all MP3 files in the root directory
static std::vector<String> mp3list;

// The file we're currently playing
static File f;

// Read buffer that's better off not in the stack due to its size
static uint8_t filebuff[512];

// Recursively scan the card and make a list of all MP3 files in all dirs
static void scanDirectory(const char *dirname) {
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
      DEBUG_PRINTF("ADD: %s\n", path.c_str());
    } else {
      DEBUG_PRINTF("SKP: %s\n", path.c_str());
    }
    f.close();
  }
  root.close();
}

static void mfrc522_setup() {
    SPI.begin(MFRC522_SPI_SCK, MFRC522_SPI_MISO, MFRC522_SPI_MOSI, MFRC522_SPI_SS);
	mfrc522.PCD_Init();		// Init MFRC522
	delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
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

void setup() {
#ifdef BOX_DEBUG
    Serial.begin(115200);
    delay(100);
    DEBUG_PRINTLN("Box starting...");
#endif

    srand(ESP.getCycleCount());

    // Initialize the SD card
    bool sdInitialized = false;
    spiSD.begin(SDCARD_SPI_SCK, SDCARD_SPI_MISO, SDCARD_SPI_MOSI, SDCARD_SPI_SS);
    sdInitialized = SD.begin(SDCARD_SPI_SS, spiSD);
    if (!sdInitialized) {
        DEBUG_PRINTLN("initialization failed!");
        while (1) {
            delay(1000);
            ESP.restart();
        }
    }

    scanDirectory("/");

    // Start the background player
    BMP.begin();
    BMP.setGain(0.1f);

    mfrc522_setup();

    pinMode(32, INPUT_PULLUP);
}

void loop() {
    if (Serial.available()) {
        f.close();  // Choose another
        while (Serial.available()) {
            Serial.read();
        }
    }

    // Stuff the buffer with as much as it will take, only doing full sector reads for performance
    while (f && BMP.availableForWrite() > 512) {
        int len = f.read(filebuff, 512);
        BMP.write(filebuff, len);
        if (len != 512) {
            f.close(); // Short reads == EOF
        }
    }

    if (digitalRead(32) == LOW) {
        DEBUG_PRINTLN("BUTTON!!!");
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
        DEBUG_PRINTLN(fname_animal[a]);
        fname = String(fname_animal[a]);
    } else {
        DEBUG_PRINTLN("Animal did not MATCHED!");
    }
    if (!f && fname.length() > 0) {
        f = SD.open(fname);
        if (!f) {
            DEBUG_PRINTF("\n Could not open %s", fname);
            return;
        }
        DEBUG_PRINTF("\r\n\r\nNow playing: %s\r\n", fname);
        auto p = f.position();
        p = p & ~511; // Ensure on a sector boundary, MP3 will resync appropriately
        f.seek(p);
    }
}
