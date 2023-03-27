// SRGBmods LED Controller v1 Firmware
// generated at https://srgbmods.net

#if !defined ARDUINO_RASPBERRY_PI_PICO && !defined ARDUINO_ADAFRUIT_FEATHER_RP2040_SCORPIO
  #error ONLY Raspberry Pi Pico and Adafruit Feather RP2040 SCORPIO are supported!
#endif

#if !defined USE_TINYUSB
	#error Please select "Adafruit TinyUSB" as "USB Stack" in "Tools"!
#endif
#include <EEPROM.h>
#include <Adafruit_NeoPXL8.h>

const byte firmwareVersion[64] = { 1, 2, 5 };

#define MAX_PINS 8

uint8_t const desc_hid_report[] =
{
	TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
};

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 1, true);

const int numPins = 8;
int8_t pinList[MAX_PINS] = { 0,1,2,3,4,5,6,7 };
const int ledsPerStrip = 30;
const int ledsPerPin[numPins] = { 97,30,30,30,30,30,30,30 };
const int offsetPerPin[numPins] = { 97,127,157,187,217,247,277,307 };
const int totalLedCount = 240;

int packetCount = 0;
int deviceCount = 0;
int ledCounter = 0;
bool updateChannel = false;
Adafruit_NeoPXL8 leds(ledsPerStrip, pinList, NEO_GRB);

byte usbPacket[64];

const uint8_t BrightFull = 255;
const int ledsPerPacket = 20;
bool newUSBpacketArrived = false;
const byte rainbowColors[7][3] = { { 255, 0, 0 }, { 255, 37, 0 }, { 255, 255, 0 }, { 0, 128, 0 }, { 128, 128, 0 }, { 0, 0, 200 }, { 75, 0, 130 } };
unsigned long lastPacketRcvd;
bool DataLedOn = false;
bool hardwareLighting = false;
unsigned long lastHWLUpdate;
const int eeprom_HWL_enable = 0;
const int eeprom_HWL_return = 1;
const int eeprom_HWL_returnafter = 2;
const int eeprom_HWL_effectMode = 3;
const int eeprom_HWL_effectSpeed = 4;
const int eeprom_HWL_brightness = 5;
const int eeprom_HWL_color_r = 6;
const int eeprom_HWL_color_g = 7;
const int eeprom_HWL_color_b = 8;
const int eeprom_StatusLED_enable = 9;
const int eeprom_ColorCompression_enable = 10;
byte HWL_enable;
byte HWL_return;
byte HWL_returnafter;
byte HWL_effectMode;
byte HWL_effectSpeed;
byte HWL_brightness;
byte HWL_singleColor[3];
byte StatusLED_enable;
byte ColorCompression_enable;
bool Core0ready = false;
bool picoSuspended = false;

void setup()
{
	Setup_SRGBmodsLCv1();
	usb_hid.setReportCallback(NULL, receiveUSBpacket);
	usb_hid.begin();
	while(!TinyUSBDevice.mounted() && !Core0ready) delay(1);
	Core0ready = true;
}

void setup1()
{
	while(!Core0ready) delay(1);
	pinMode(LED_BUILTIN, OUTPUT);
	EEPROM.begin(256);
	HWL_readEEPROM();
	leds.begin();
}

void Setup_SRGBmodsLCv1()
{
	usb_hid.setStringDescriptor("LED Controller v1");
	USBDevice.setID(0x16D0,0x1205);
	USBDevice.setManufacturerDescriptor("SRGBmods.net");
	USBDevice.setProductDescriptor("LED Controller v1");
}

void loop()
{
}

void loop1()
{
	switch(TinyUSBDevice.suspended())
	{
		case true:
		{
			if(!picoSuspended)
			{
				picoSuspended = true;
				toggleOnboardLED(false);
				resetLighting();
			}
			return;
		}	
		case false:
		{
			if(picoSuspended)
			{
				picoSuspended = false;
			}
		}
	}
	if(newUSBpacketArrived)
	{
		processUSBpacket();	
	}
	if(updateChannel)
	{
		updateLighting();
	}
	if(millis() - lastPacketRcvd >= 500 && DataLedOn)
	{
		DataLedOn = false;
		toggleOnboardLED(false);
	}
	handleHWLighting();
}

void HWL_readEEPROM()
{
	HWL_enable = EEPROM.read(eeprom_HWL_enable) == 0xFF ? 0x01 : EEPROM.read(eeprom_HWL_enable);
	HWL_return = EEPROM.read(eeprom_HWL_return) == 0xFF ? 0x01 : EEPROM.read(eeprom_HWL_return);
	HWL_returnafter = EEPROM.read(eeprom_HWL_returnafter) == 0xFF ? 0x0A : EEPROM.read(eeprom_HWL_returnafter);
	HWL_effectMode = EEPROM.read(eeprom_HWL_effectMode) == 0xFF ? 0x01 : EEPROM.read(eeprom_HWL_effectMode);
	HWL_effectSpeed = EEPROM.read(eeprom_HWL_effectSpeed) == 0xFF ? 0x06 : EEPROM.read(eeprom_HWL_effectSpeed);
	HWL_brightness = EEPROM.read(eeprom_HWL_brightness) == 0xFF ? 0x7F : EEPROM.read(eeprom_HWL_brightness);
	HWL_singleColor[0] = EEPROM.read(eeprom_HWL_color_r) == 0xFF ? 0x80 : EEPROM.read(eeprom_HWL_color_r);
	HWL_singleColor[1] = EEPROM.read(eeprom_HWL_color_g) == 0xFF ? 0x00 : EEPROM.read(eeprom_HWL_color_g);
	HWL_singleColor[2] = EEPROM.read(eeprom_HWL_color_b) == 0xFF ? 0x80 : EEPROM.read(eeprom_HWL_color_b);
	StatusLED_enable = EEPROM.read(eeprom_StatusLED_enable) == 0xFF ? 0x00 : EEPROM.read(eeprom_StatusLED_enable);
	ColorCompression_enable = EEPROM.read(eeprom_ColorCompression_enable) == 0xFF ? 0x00 : EEPROM.read(eeprom_ColorCompression_enable);
	if(!HWL_enable)
	{
		hardwareLighting = false;
	}
	else
	{
		hardwareLighting = true;
	}
}

void toggleOnboardLED(bool state)
{
	if(StatusLED_enable || state == false)
	{
		digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
	}
}

void updateLighting()
{
	if(!DataLedOn)
	{
		DataLedOn = true;
		toggleOnboardLED(true);
	}
	packetCount = 0;
	deviceCount = 0;
	ledCounter = 0;
	updateChannel = false;
	if(leds.canShow())
	{
		leds.show();
	}
}

void HWLfillRainbow(uint16_t first_hue = 0, int8_t reps = 1, uint8_t saturation = 255, uint8_t brightness = 255, bool gammify = true)
{
	for(int ledIdx = 0; ledIdx < ledsPerStrip * numPins; ledIdx++)
	{
		uint16_t hue = first_hue + (ledIdx * reps * 65536) / ledsPerStrip;
		uint32_t color = leds.ColorHSV(hue, saturation, brightness);
		if (gammify) color = leds.gamma32(color);
		leds.setPixelColor(ledIdx, color);
	}
	if(leds.canShow())
	{
		leds.show();
	}
}

void HWLfillSolid(byte r, byte g, byte b, byte brightness = BrightFull)
{
	for(int ledIdx = 0; ledIdx < ledsPerStrip * numPins; ledIdx++)
	{
		leds.setPixelColor(ledIdx, setRGBbrightness(r, g, b, brightness));
	}
	if(leds.canShow())
	{
		leds.show();
	}
}

void handleHWLighting()
{
	unsigned long currentMillis = millis();
	if(HWL_enable == 1 && hardwareLighting == true)
	{
		int hwleffectspeed = ceil(300 / HWL_effectSpeed);
		if(currentMillis - lastHWLUpdate >= (hwleffectspeed < 10 ? 10 : hwleffectspeed))
		{
			lastHWLUpdate = currentMillis;
			switch(HWL_effectMode)
			{
				case 1:
				{
					static uint16_t firsthue = 0;
					HWLfillRainbow(firsthue -= 256, 10, 255, HWL_brightness > 75 ? HWL_brightness : 75);
					return;
				}
				case 2:
				{
					static int currentColor = 0;
					static uint8_t breath_bright = HWL_brightness;
					static bool isDimming = true;
					if(isDimming && (breath_bright-1 <= 1))
					{
						if((currentColor + 1) > 6)
						{
							currentColor = 0;
						}
						else
						{
							currentColor++;
						}
						isDimming = false;
					}
					else if(!isDimming && (breath_bright+1 >= HWL_brightness))
					{
						isDimming = true;
					}
					breath_bright = isDimming ? breath_bright-1 : breath_bright+1;
					HWLfillSolid(rainbowColors[currentColor][0], rainbowColors[currentColor][1], rainbowColors[currentColor][2], breath_bright);
					return;
				}
				case 3:
				{
					if(!HWL_singleColor[0] && !HWL_singleColor[1] && !HWL_singleColor[2])
					{
						HWLfillSolid(0x00, 0x00, 0x00);
						return;
					}
					HWLfillSolid(HWL_singleColor[0], HWL_singleColor[1], HWL_singleColor[2], HWL_brightness);
					return;
				}
				case 4:
				{
					static uint8_t breath_bright = HWL_brightness;
					static bool isDimming = true;
					isDimming = (isDimming && (breath_bright-1 <= 1)) ? false : (!isDimming && (breath_bright+1 >= HWL_brightness)) ? true : isDimming;
					breath_bright = isDimming ? breath_bright-1 : breath_bright+1;
					HWLfillSolid(HWL_singleColor[0], HWL_singleColor[1], HWL_singleColor[2], breath_bright);
					return;
				}
			}
			return;
		}
	}
	if(HWL_enable == 1 && hardwareLighting == false && HWL_return == 1)
	{
		if(currentMillis - lastPacketRcvd >= (HWL_returnafter * 1000))
		{
			resetLighting();
			hardwareLighting = true;
			return;
		}
	}
}

unsigned int setRGBbrightness(byte r, byte g, byte b, byte brightness)
{
	r = (r * brightness) >> 8;
	g = (g * brightness) >> 8;
	b = (b * brightness) >> 8;
	return(((unsigned int)r & 0xFF )<<16 | ((unsigned int)g & 0xFF)<<8 | ((unsigned int)b & 0xFF));
}

void resetLighting()
{
	newUSBpacketArrived = false;
	packetCount = 0;
	deviceCount = 0;
	ledCounter = 0;
	updateChannel = false;
	HWLfillSolid(0x00, 0x00, 0x00);
}

void receiveUSBpacket(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
	if(!newUSBpacketArrived)
	{
		memcpy(usbPacket, buffer, 64);
		newUSBpacketArrived = true;
	}
}

void processUSBpacket()
{
	newUSBpacketArrived = false;
	if(usbPacket[0] && usbPacket[1] && !usbPacket[2] && usbPacket[3] == 0xAA)
	{
		lastPacketRcvd = millis();
		if(HWL_enable == 1 && hardwareLighting == true)
		{
			resetLighting();
			hardwareLighting = false;
		}
		colorLeds(usbPacket);
		return;
	}
	else if(!usbPacket[0] && !usbPacket[1] && !usbPacket[2] && usbPacket[3] == 0xBB)
	{
		HWL_enable = usbPacket[4];
		HWL_return = usbPacket[5];
		HWL_returnafter = usbPacket[6];
		HWL_effectMode = usbPacket[7];
		HWL_effectSpeed = usbPacket[8];
		HWL_brightness = usbPacket[9];
		HWL_singleColor[0] = usbPacket[10];
		HWL_singleColor[1] = usbPacket[11];
		HWL_singleColor[2] = usbPacket[12];
		StatusLED_enable = usbPacket[13];
		ColorCompression_enable = usbPacket[14];
		
		if(HWL_enable)
		{
			hardwareLighting = true;
		}
		else
		{
			hardwareLighting = false;
		}
		
		DataLedOn = false;
		toggleOnboardLED(false);
		
		EEPROM.write(eeprom_HWL_enable, HWL_enable);
		EEPROM.write(eeprom_HWL_return, HWL_return);
		EEPROM.write(eeprom_HWL_returnafter, HWL_returnafter);
		EEPROM.write(eeprom_HWL_effectMode, HWL_effectMode);
		EEPROM.write(eeprom_HWL_effectSpeed, HWL_effectSpeed);
		EEPROM.write(eeprom_HWL_brightness, HWL_brightness);
		EEPROM.write(eeprom_HWL_color_r, HWL_singleColor[0]);
		EEPROM.write(eeprom_HWL_color_g, HWL_singleColor[1]);
		EEPROM.write(eeprom_StatusLED_enable, StatusLED_enable);
		EEPROM.write(eeprom_ColorCompression_enable, ColorCompression_enable);
		
		EEPROM.commit();
		delay(50);
		resetLighting();
		return;
	}
	else if(!usbPacket[0] && !usbPacket[1] && !usbPacket[2] && usbPacket[3] == 0xCC)
	{
		usb_hid.sendReport(0, firmwareVersion, sizeof(firmwareVersion));
		return;
	}
}

void colorLeds(uint8_t const* usbPacket)
{
	int multiplier = ColorCompression_enable ? 2 : 1;
	uint8_t uncompressedRGB[ledsPerPacket*3*multiplier];
	if(usbPacket[0] == packetCount+1)
	{
		int ledsSent = packetCount * ledsPerPacket * multiplier;
		if(ledsSent < totalLedCount)
		{
			if(ColorCompression_enable)
			{
				for(int runCount = 0; runCount < ledsPerPacket; runCount++)
				{
					uncompressedRGB[(runCount*6)] = (usbPacket[4+(runCount*3)] & 0x0F) << 4;
					uncompressedRGB[(runCount*6)+1] = (usbPacket[4+(runCount*3)] & 0xF0);
					uncompressedRGB[(runCount*6)+2] = (usbPacket[4+(runCount*3)+1] & 0x0F) << 4;
					uncompressedRGB[(runCount*6)+3] = (usbPacket[4+(runCount*3)+1] & 0xF0);
					uncompressedRGB[(runCount*6)+4] = (usbPacket[4+(runCount*3)+2] & 0x0F) << 4;
					uncompressedRGB[(runCount*6)+5] = (usbPacket[4+(runCount*3)+2] & 0xF0);
				}
			}
			
			for(int ledIdx = 0; ledIdx < ledsPerPacket * multiplier; ledIdx++)
			{
				if(ledsSent + 1 <= totalLedCount)
				{
					if((packetCount * ledsPerPacket * multiplier) + ledIdx >= offsetPerPin[deviceCount])
					{
						deviceCount++;
						ledCounter = 0;
					}
					if(ColorCompression_enable)
					{
						leds.setPixelColor((deviceCount * ledsPerStrip) + ledCounter, uncompressedRGB[(ledIdx*3)], uncompressedRGB[(ledIdx*3)+1], uncompressedRGB[(ledIdx*3)+2]);
					}
					else
					{
						leds.setPixelColor((deviceCount * ledsPerStrip) + ledCounter, usbPacket[4+(ledIdx*3)], usbPacket[4+(ledIdx*3)+1], usbPacket[4+(ledIdx*3)+2]);
					}
					ledsSent++;
					ledCounter++;
				}
			}
		}
		packetCount++;
	}
	else
	{
		packetCount = 0;
		deviceCount = 0;
		ledCounter = 0;
	}
	if(packetCount == usbPacket[1])
	{
		updateChannel = true;
	}
	return;
}
