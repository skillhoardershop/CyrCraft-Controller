//#include <sd_diskio.h>
//#include <sd_defines.h>
//#include <dummy.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "FastLED.h"
#include "math.h"

//LED things
#define NUM_LEDS 80
#define DATA_PIN 2
#define FPS 80
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
	//Vars that only change when a new image is loaded
	bool contPlayingFile = true;
	bool firstCycle = true;
	int8_t extension = 0;
	uint8_t paddingSize = 0;
	uint32_t sizeOfFile = 0;
	uint32_t startOfPixels = 0;
	uint32_t sizeOfDIB = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t colorDepth = 0;
	uint32_t rawImageSize = 0;
	uint32_t * ledData;
	uint64_t timeDelayStartOne;
	double frequencyMillis = (pow(10, -6) * (1 / FPS));

	free(ledData); //Ensure the memory used by the previous section is avaiable for new writes

	//Prompt serial monitor for file dir and name
	String fileName = Serial.readString();

	//Open file
	File file = SD.open(fileName);
	if (!file) {
		Serial.println("Failed to open file for reading");
		return;
	}

	while (contPlayingFile) {
		
		//Only runs memory allocation and array saving once, will use a different function for every time after
		if (firstCycle) {
			//The next chunk of lines are grabbing initial data right from the file instead of saving it at all twice
			//File size
			file.seek(0x2);
			sizeOfFile = endianSwap(file, 4);
			//Starting pixel location
			file.seek(0xA);
			startOfPixels = endianSwap(file, 4);
			//Size of DIB header
			sizeOfDIB = endianSwap(file, 4); //Will breakout here to account for all sizes of BMP DIBs

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

			//Allocate enough memory to store ONLY the bits we need to save time
			ledData = (uint32_t *)malloc(((sizeOfFile - startOfPixels - (height * paddingSize)) * 4) / 3);

			//Variable to ensure FPS is acheived
			timeDelayStartOne = micros();
			//For loop for rows
			for (int y = 0; y < height; y++) {
				//For loop for colums
				for (int x = 0; x < width; x++) {
					//For loop for filling in LEDs if file width is not long enough
					for (int ext = 0; ext < extension; ext++) {
						//Dont keep writing if all leds have been activated
						if ((ext * width + x) < NUM_LEDS) break;

						//Set the color of LEDs to show
						leds[(width * ext) + x].r = file.read();
						leds[(width * ext) + x].g = file.read();
						leds[(width * ext) + x].b = file.read();
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

			//Fill time to ensure FPS is acheived
			if ((micros() - timeDelayStartOne) < frequencyMillis) {
				delayMicroseconds(frequencyMillis - (micros() - timeDelayStartOne));
				Serial.printf("Time worked out!");
			}

			!firstCycle;
		}

		else if (!firstCycle) {
			timeDelayStartOne = micros();
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
			//Fill time to ensure FPS is acheived
			if ((micros() - timeDelayStartOne) < frequencyMillis) {
				delay(frequencyMillis - (micros() - timeDelayStartOne));
				Serial.printf("Time worked out!");
			}
		}
	}
}
