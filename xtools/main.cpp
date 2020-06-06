/*
 * XTOOLS is based on xenium-tools, which is free software:
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This software has been developed using NXDK. The cross-platform, open-source SDK to develop for Original Xbox
 * See https://github.com/XboxDev/nxdk
 */

#include <hal/debug.h>
#include <pbkit/pbkit.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <hal/input.h>
#include <hal/io.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <SDL.h>

#include "am29lv160mt.hpp"
#include "xenium.hpp"

#define MAXRAM 0x03FFAFFF
#define SCREEN_HEIGHT 480
#define SCREEN_WIDTH 640

//From /hal/debug.c to modify position of cursor
static int nextRow;
static int nextCol;

//Get framebuffer pointer
static uint8_t* _fb;

SDL_GameController * gamepad;
static unsigned char * flashData;
static unsigned char * readBackBuffer;

//forward declarations
void drawMainMenu(void);
unsigned char getButton(SDL_GameController * , SDL_GameControllerButton);
void waitForButton(SDL_GameController * , SDL_GameControllerButton);
unsigned char getButtonActivated(SDL_GameController * , SDL_GameControllerButton);
unsigned char checkForXenium(void);
char dumpXenium(unsigned char * );
void writeXeniumRaw(unsigned char * buffer);
void debugPrintXY(char * str, int x, int y);

static bool xenium_ok_ = false;

static char flash_path_[32];
static const char* flash_path_gold = "D:\\xenium_os\\xenium_gold.bin";
static const char* flash_path_blue = "D:\\xenium_os\\xenium_blue.bin";
static const char* flash_path_ice = "D:\\xenium_os\\xenium_ice.bin";

enum FLASH_TYPE
{
    FLASH_GOLD,
    FLASH_BLUE,
    FLASH_ICE
};
static FLASH_TYPE flash_type_ = FLASH_BLUE;
static char flash_string_[10];

enum LED_TYPE
{
    LED_OFF,
    LED_RED,
    LRED_GREEN,
    LED_AMBER,
    LED_BLUE,
    LED_PURPLE,
    LED_TEAL,
    LED_WHITE

};
static char LED_type_ = LED_OFF;
static char LED_string_[10];

int main() 
{

    size_t fb_size = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
    _fb = (uint8_t*)MmAllocateContiguousMemoryEx(fb_size,
                                                 0,
                                                 0xFFFFFFFF,
                                                 0x1000,
                                                 PAGE_READWRITE | PAGE_WRITECOMBINE);
    memset(_fb, 0x00, fb_size);

    XVideoSetMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, REFRESH_DEFAULT);
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    gamepad = SDL_GameControllerOpen(0);

    flashData = (unsigned char*) MmAllocateContiguousMemoryEx(XENIUM_FLASH_SIZE, 0, MAXRAM,
                                             0, PAGE_READWRITE | PAGE_NOCACHE);
    readBackBuffer =(unsigned char*) MmAllocateContiguousMemoryEx(XENIUM_FLASH_SIZE, 0, MAXRAM,
                                                  0, PAGE_READWRITE | PAGE_NOCACHE);

    drawMainMenu();

    //Record the bank used to get into this program. Useful when rebooting.
    //If not a valid bank, probably booted with a non Xenium device. In this case
    //just set it to Bank 1 - Cromwell bootloader for Xenium.
    unsigned char initialBiosBank = IoInputByte(0xEF) & 0x0F;
    if (initialBiosBank > 10 || initialBiosBank == 0) {
        initialBiosBank = XENIUM_BANK_CROMWELL;
    }

    while (1) {
        SDL_GameControllerUpdate();

        // Set Flash Type 
        if  (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_X))
        {
            flash_type_ = FLASH_BLUE;
            drawMainMenu();
        }
        if  (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_Y))
        {
            flash_type_ = FLASH_GOLD;
            drawMainMenu();
        }
        if  (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_B))
        {
            flash_type_ = FLASH_ICE;
            drawMainMenu();
        }

        /* WRITE A RAW XENIUM FLASH DUMP TO FLASH MEMORY */
        else if (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_A)) 
        {

            debugPrint("\n");
            debugPrint("** Write a FULL 2MB flash to Xenium device **\n");
            debugPrint("**       DO NOT POWER OFF THE XBOX         **\n\n");
            FILE * f0 = fopen(flash_path_, "rb");

            if (f0 == NULL) 
            {
                debugPrint("NOT FOUND: ");
                debugPrint(flash_path_);
                debugPrint("!!! \n");
            } 
            else if (checkForXenium() == 0) 
            {
                debugPrint("Xenium device NOT detected! Something is wrong\n");
            } 
            else 
            {
                memset(flashData, 0x00, XENIUM_FLASH_SIZE);
                memset(readBackBuffer, 0x00, XENIUM_FLASH_SIZE);

                debugPrint("Reading ");
                debugPrint(flash_path_);
                debugPrint("\n");

                fread(flashData, 1, XENIUM_FLASH_SIZE, f0);
                fclose(f0);

                debugPrint("Press BLACK to start FLASH\n\n");
                waitForButton(gamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

                debugPrint("Erasing:\n");
                debugPrint("   Full 2MB flash erase...\n");
                flashFullErase();

                debugPrint("\nWriting:\n");
                writeXeniumRaw(flashData);

                debugPrint("\nVerifying:\n");
                dumpXenium(readBackBuffer);

                //Blank the write protected sector which is on a Genuine Xenium.
                //This will prevent verification errors on a Genuine xenium.
                memset(&readBackBuffer[0x1C0000], 0x00, 0x20000);
                memset(&flashData[0x1C0000], 0x00, 0x20000);
                
                //verify written data is same as rea
                if (memcmp(readBackBuffer, flashData, XENIUM_FLASH_SIZE) != 0) 
                {
                    debugPrint("\n**** ERROR VERIFYING FLASH ****\n\n");
                } else {
                    debugPrint("\n*****  SUCCESS ****\n\n");
                }
            }

            if (f0 != NULL) {
                fclose(f0);
            }

            debugPrint("Press START to return to the main menu\n");
            waitForButton(gamepad, SDL_CONTROLLER_BUTTON_START);

            drawMainMenu();

        }

        /* TOGGLE RGB LED ON EVERY PRESS OF WHITE */
        else if (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) 
        {
            //Cycle LED
            if (++LED_type_ > 7) 
                LED_type_ = LED_TYPE::LED_OFF;

            drawMainMenu();
        }

        //Recheck for Xenium device. (Basically redraw the main menu)
        else if (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_START)) 
        {
            drawMainMenu();
        }

        //Exit and reboot..
        else if (getButtonActivated(gamepad, SDL_CONTROLLER_BUTTON_BACK)) 
        {
            XReboot();
        }

    }

    SDL_GameControllerClose(gamepad);
    pb_kill();
    return 0;
}

unsigned char getButton(SDL_GameController * pad, SDL_GameControllerButton button) {
    return SDL_GameControllerGetButton(pad, button);
}

void waitForButton(SDL_GameController * pad, SDL_GameControllerButton button) {
    do {
        SDL_GameControllerUpdate();
        Sleep(50);
    } while (getButton(pad, button) == 0);
}

unsigned char getButtonActivated(SDL_GameController * pad, SDL_GameControllerButton button) {
    static unsigned short buttonState = 0x0000;
    unsigned char ret = 0;

    //Exit on invalid inputs
    if (button == -1 || button >= 16) {
        return 0;
    }

    //Return 1 if button is pressed, and the buttonState was previously 0.
    if (SDL_GameControllerGetButton(pad, button)) {
        if (!(buttonState & (1 << (unsigned short) button))) {
            ret = 1;
        }
        buttonState |= 1 << (unsigned short) button; //Set buttonState
    } else {
        buttonState &= ~(1 << (unsigned short) button); //Clear buttonState
    }

    return ret;
}

void debugPrintXY(char * str, int x, int y) {
    int row = nextRow;
    int col = nextCol;
    nextCol = x;
    nextRow = y;
    debugPrint(str);
    nextRow = row;
    nextCol = col;
}

void drawMainMenu(void) 
{


    //Check flash type
    switch (flash_type_)
    {
        case (FLASH_GOLD):
            strncpy(flash_string_, "GOLD", sizeof(flash_string_));
            strncpy(flash_path_, flash_path_gold, sizeof(flash_path_));            
            break;
        case (FLASH_ICE):
            strncpy(flash_string_, "ICE", sizeof(flash_string_));
            strncpy(flash_path_, flash_path_ice, sizeof(flash_path_));
            break;
        case (FLASH_BLUE):
        default:
            strncpy(flash_string_, "BLUE", sizeof(flash_string_));
            strncpy(flash_path_, flash_path_blue, sizeof(flash_path_));
            break;

    }

    //set and check LED
    IoOutputByte(XENIUM_REGISTER_LED, LED_type_);
    switch (LED_type_) 
    {
        default:
        case (LED_OFF):
            strncpy(LED_string_, "OFF", sizeof(LED_string_));
            break;
        case (LED_RED):
            strncpy(LED_string_, "RED", sizeof(LED_string_));
            break;
        case (LRED_GREEN):
            strncpy(LED_string_, "GREEN", sizeof(LED_string_));
            break;
        case (LED_AMBER):
            strncpy(LED_string_, "AMBER", sizeof(LED_string_));
            break;
        case (LED_BLUE):
            strncpy(LED_string_, "BLUE", sizeof(LED_string_));
            break;
        case (LED_PURPLE):
            strncpy(LED_string_, "PURPLE", sizeof(LED_string_));
            break;
        case (LED_TEAL):
            strncpy(LED_string_, "TEAL", sizeof(LED_string_));
            break;
        case (LED_WHITE):
            strncpy(LED_string_, "WHITE", sizeof(LED_string_));
            break;
    }

    debugClearScreen();
    debugPrint("XTOOLS (based on Xenium Tools by Ryzee119\n");

    debugPrint("--------------------------------------------------------------\n");

    //Check for and detect Xenium hardware
    if (checkForXenium() == 1) 
    {
        xenium_ok_ = true;
        debugPrint("Xenium: OK\n");
        debugPrint("Flash: ");
        debugPrint(flash_string_);
        debugPrint("\n");

        debugPrint("LED: ");
        debugPrint(LED_string_);
        debugPrint("\n");
    } 
    else 
    {
        debugPrint("Xenium: ** NOT Detected**\n");
        xenium_ok_ = false;
    }

    debugPrint("--------------------------------------------------------------\n");

    if (xenium_ok_)
    {
        //Main menu..
        debugPrint("A:     START FLASH!!\n");
        debugPrint("X:     Set BLUE Flash\n");
        debugPrint("Y:     Set GOLD Flash\n");
        debugPrint("B:     Set ICE Flash\n");
        debugPrint("WHITE: Cycle LED\n");
    }

    debugPrint("--------------------------------------------------------------\n\n");
    debugPrint("START: Re-Check Xenium\n");
    debugPrint("BACK : Reboot\n");
    debugPrint("--------------------------------------------------------------\n\n");

}

unsigned char checkForXenium() 
{
    unsigned char temp[8];
    unsigned char manuf, devid;

    //A genuine xenium needs something similar to a real Xos boot process to start working
    //Must be something quirky in the CPLD. This seems to work:
    LPCmemoryRead(temp, 0x70, 8); //Couple reads just to get Xenium going
    IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_XENIUMOS);
    LPCmemoryRead(temp, 0x70, 8); //Couple reads just to get Xenium going
    IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_RECOVERY);
    LPCmemoryRead(temp, 0x70, 8); //Couple reads just to get Xenium going
    IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_CROMWELL);

    manuf = getManufID();
    devid = getDevID();

    //debugPrint("Man ID %02x, Dev ID %02x: ", manuf, devid);
    if (manuf == XENIUM_MANUF_ID && devid == XENIUM_DEVICE_ID){
        return 1;
    } else {
        return 0;
    }
}

char dumpXenium(unsigned char * buffer) 
{
    unsigned int address = 0;
    const unsigned short chunk_size = 128;

    for (unsigned int rawAddress = 0; rawAddress < XENIUM_FLASH_SIZE; rawAddress += chunk_size) {
        if (rawAddress == 0x000000) {
            debugPrint("   BIOS Banks...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_1_1024);
            address = 0;
        } else if (rawAddress == 0x100000) {
            debugPrint("   XeniumOS...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_XENIUMOS);
            address = 0;
        } else if (rawAddress == 0x180000) {
            debugPrint("   Boot Loader...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_CROMWELL);
            address = 0;
        } else if (rawAddress == 0x1C0000) {
            debugPrint("   Recovery...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_RECOVERY);
            address = 0;
        }
        LPCmemoryRead( & buffer[rawAddress], address, chunk_size);
        address += chunk_size;
    }
    return 0;
}

void writeXeniumRaw(unsigned char * buffer) 
{
    unsigned int bankSize = 0;

    for (unsigned int rawAddress = 0; rawAddress < XENIUM_FLASH_SIZE; rawAddress += bankSize) {
        if (rawAddress == 0x000000) {
            debugPrint("   BIOS Banks...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_1_1024);
            bankSize = 0x100000;
            
        } else if (rawAddress == 0x100000) {
            debugPrint("   XeniumOS...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_XENIUMOS);
            bankSize = 0x80000;
            
        } else if (rawAddress == 0x180000) {
            debugPrint("   Boot Loader...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_CROMWELL);
            bankSize = 0x40000;
            
        } else if (rawAddress == 0x1C0000) {
            debugPrint("   Recovery...\n");
            IoOutputByte(XENIUM_REGISTER_BANKING, XENIUM_BANK_RECOVERY);
            bankSize = 0x40000;
        }

        const unsigned int chunk = 1024;
        unsigned int bankAddress = 0;
        while (bankAddress < bankSize) {

            for (unsigned int i = 0; i < chunk; i++) {
                flashProgramByte(bankAddress, buffer[rawAddress + bankAddress]);
                bankAddress++;
            }

        }
    }
}