//#include <sd_diskio.h>
//#include <sd_defines.h>
//#include <dummy.h>
#include <dummy.h>
#include "FreeRTOS/FreeRTOS.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "FastLED.h"
#include "math.h"

//LED things
#define NUM_LEDS 80
#define DATA_PIN 2
#define FPS 80
double FRQUENCY_MICROS = (pow(10, 6) * (1 / FPS));
CRGB leds[NUM_LEDS];

/*Function to search through directories, not used just yet
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    unsigned int biggestFile = 0;

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        if (file.size() > biggestFile) biggestFile = file.size();
        file = root.openNextFile();
    }
    Serial.printf("The largest file was of size: %d", biggestFile);
    Serial.println(" bytes");
}*/

//Switches the endian of the hex
uint32_t endianSwap(File file, uint8_t numBytes) {
	uint32_t out = 0;

	for (int i = 0; i < numBytes; i++) {
		out += (file.read() * pow(2, (8 * i)));
	}

	return out;
}

//Class for bitmap files
class GenericBitmap {

private:
	//Vars for each bmp file
	File file;
	int8_t extension;
	uint8_t paddingSize;
	uint32_t sizeOfFile, startOfPixels, sizeOfDIB, width;
	uint32_t height, colorDepth, rawImageSize;
	uint32_t* ledData;


public:
	String fileName;

	GenericBitmap() {
		//Variables init (We dont want garbage data)
		int8_t extension = 0;
		uint8_t paddingSize = 0;
		uint32_t sizeOfFile = 0;
		uint32_t startOfPixels = 0;
		uint32_t sizeOfDIB = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t colorDepth = 0;
		uint32_t rawImageSize = 0;
		free(ledData);
	}

	~GenericBitmap() {
		file.close();
		free(ledData);
	}

	//Method to open the file
	void openFile() {
		file = SD.open(fileName);
		if (!file) {
			Serial.printf("File could not be opened\n");
			return;
		}
		return;
	}

	//Grabs all info from headers
	void init() {

		//4 byte temp data buffer
		uint8_t* tempData[4];
		//Ensure we're at the top of the file
		file.seek(0x0000);
		//Check that this file is of an allowed type (currently only bmp)
		if ((file.read() == 0x42) && (file.read() == 0x4D)) {	//File is bmp with file type 0x4D42
			//File size
			sizeOfFile = endianSwap(file, 4);
			//Starting pixel location
			file.seek(0xA);
			startOfPixels = endianSwap(file, 4);
			//Size of DIB header
			sizeOfDIB = endianSwap(file, 4); //Will breakout here to account for all sizes of BMP DIBs (Currently only 24 bpp supported)

			//Dimensions
			width = endianSwap(file, 4);
			height = endianSwap(file, 4);
			//Color Depth
			file.seek(file.position() + 0x2);
			colorDepth = endianSwap(file, 2);
			//Raw image size with padding
			file.seek(file.position() + 0x2);
			rawImageSize = endianSwap(file, 4);

			//Skip to pixel data
			file.seek(startOfPixels);
			paddingSize = ((width * 3) % 4);
			//Check how many LEDS 
			extension = (0 - (0 - (NUM_LEDS / width)));
		}
		//Allocate the memory that will be used in the array of pixels
		ledData = (uint32_t*)malloc(((sizeOfFile - startOfPixels - (height * paddingSize)) /3 ) * 4);
		//Prepare file to have pixels read
		file.seek(startOfPixels); 
	}	

	void playAndStore() {
		for (int y = 0; y < height; y++) {
			//For loop for colums
			for (int x = 0; x < width; x++) {
				//For loop for filling in LEDs if file width is not long enough
				for (int ext = 0; ext < extension; ext++) {
					//Dont keep writing if all leds have been activated
					if ((ext * width + x) < NUM_LEDS) break;

					//Set the color of LEDs to show
					leds[(width * ext) + x].r = file.read();
					leds[(width * ext) + x].b = file.read();
					leds[(width * ext) + x].g = file.read();
				}

				//Add each of the variables to the heap for later (much more  efficient this way)
				ledData[(y * width) + (x * 3)] = leds[(y * width) + x];

			}

			//Show leds
			FastLED.show();
			//Skip padding
			file.seek(file.position() + paddingSize);
			//Break if end of file
			if (!file.available()) break;
		}
		file.close();
	}

	void play() {
		//For loop for rows
		for (int y = 0; y < height; y++) {
			//For loop for columns
			for (int x = 0; x < width; x++) {
				//For loop for filling in LEDs if file width is not long enough
				for (int ext = 0; ext < extension; ext++) {
					//Dont keep writing if all leds have been activated
					if ((ext * width + x) < NUM_LEDS) break;

					//Set the color of LEDs to show
					leds[(width * ext) + x] = ledData[(y * width) + x];
				}
			}
			//Show leds
			FastLED.show();
		}
	}

	void store() {
		//For loo[s for rows
		for (int y = 0; y < height; y++) {
			//For loop for colums
			for (int x = 0; x < width; x++) {
				//Store data
				ledData[(y * width) + (x * 3)] = leds[(y * width) + x];
			}
			//Break if end of file
			if (!file.available()) break;
		}
	}
};



void setup(){
    //LED setup
    FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);

    //SD setup
    Serial.begin(115200);
	while (!SD.begin()) {
		Serial.printf("Card Mount Failed!! Attemping again in 2 seconds .");
		delay(500);
		Serial.printf(".");
		delay(500);
		Serial.printf(".");
		delay(500);
		Serial.println(".");
		delay(500);
	}

}

void loop() {
	//Vars that need initializing each cycle
	GenericBitmap curBitmap;

	bool contPlayingFile = true;
	bool firstCycle = true;
	uint64_t timeDelayStartOne;

	//Prompt serial monitor for file dir and name
	Serial.printf("Please enter the directory and file name:\n");
	if (Serial.available()) {
		curBitmap.fileName = Serial.readString();
	}

	while (contPlayingFile) {
		
		//Only runs memory allocation and array saving once, will use a different function for every time after
		if (firstCycle) {

			curBitmap.openFile();
			curBitmap.init();
			
			//Variable to ensure FPS is acheived
			timeDelayStartOne = micros();
			
			//Call the save to array and play method in bitmap class
			curBitmap.playAndStore();
			

			//Fill time to ensure FPS is acheived
			if ((micros() - timeDelayStartOne) < FRQUENCY_MICROS) {
				delayMicroseconds(FRQUENCY_MICROS - (micros() - timeDelayStartOne));
				Serial.printf("Time worked out!");
			}

			!firstCycle;
		}

		else if (!firstCycle) {
			timeDelayStartOne = micros();

			//Play leds from memory
			curBitmap.play();

			//Fill time to ensure FPS is acheived
			if ((micros() - timeDelayStartOne) < FRQUENCY_MICROS) {
				delay(FRQUENCY_MICROS - (micros() - timeDelayStartOne));
				Serial.printf("Time worked out!");
			}
		}
	}
}
