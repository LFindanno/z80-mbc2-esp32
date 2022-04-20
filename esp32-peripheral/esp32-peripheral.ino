#define CS 12
#define LED 22

// ------------------------------------------------------------------------------
//
// File names and starting addresses
//
// ------------------------------------------------------------------------------

#define   BASICFN       "/BASIC47.BIN"
#define   FORTHFN       "/FORTH13.BIN"
#define   CPMFN         "/CPM22.BIN"
#define   QPMFN         "/QPMLDR.BIN"
#define   CPM3FN        "/CPMLDR.COM"      // CP/M 3 CPMLDR.COM loader
#define   AUTOFN        "/AUTOBOOT.BIN"
#define   Z80DISK       "/DSxNyy.DSK"      // Generic Z80 disk name (from DS0N00.DSK to DS9N99.DSK)
#define   DS_OSNAME     "/DSxNAM.DAT"      // File with the OS name for Disk Set "x" (from DS0NAM.DAT to DS9NAM.DAT)
#define   BASSTRADDR    0x0000            // Starting address for the stand-alone Basic interptreter
#define   FORSTRADDR    0x0100            // Starting address for the stand-alone Forth interptreter
#define   CPM22CBASE    0xD200            // CBASE value for CP/M 2.2
#define   CPMSTRADDR    (CPM22CBASE - 32) // Starting address for CP/M 2.2
#define   QPMSTRADDR    0x80              // Starting address for the QP/M 2.71 loader
#define   CPM3STRADDR   0x100             // Starting address for the CP/M 3 loader
#define   AUTSTRADDR    0x0000            // Starting address for the AUTOBOOT.BIN file


#define SYNC_BOOTMODE 1
#define SYNC_DISKSET  2
#define SYNC_AUTOEXECFLAG 3
// ------------------------------------------------------------------------------
//
//  Libraries
//
// ------------------------------------------------------------------------------

#include <ESP32SPISlave.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// ------------------------------------------------------------------------------
//
//  Constants
//
// ------------------------------------------------------------------------------

const byte    diskSetAddr  = 14;          // Internal EEPROM address for the current Disk Set [0..9]
const byte    maxDiskNum   = 99;          // Max number of virtual disks
const byte    maxDiskSet   = 3;           // Number of configured Disk Sets

// General purpose variables
byte          ioData;                     // Data byte used for the I/O operation
byte          ioOpcode       = 0xFF;      // I/O operation code or Opcode (0xFF means "No Operation")
word          ioByteCnt;                  // Exchanged bytes counter during an I/O operation
byte          tempByte;                   // Temporary variable (buffer)
byte          bootMode       = 0;         // Set the program to boot (from flash or SD)
byte *        BootImage;                  // Pointer to selected flash payload array (image) to boot
word          BootImageSize  = 0;         // Size of the selected flash payload array (image) to boot
word          BootStrAddr;                // Starting address of the selected program to boot (from flash or SD)
byte          iCount;                     // Temporary variable (counter)

const char *  fileNameSD;                 // Pointer to the string with the currently used file name
byte          autobootFlag;               // Set to 1 if "autoboot.bin" must be executed at boot, 0 otherwise
byte          autoexecFlag;               // Set to 1 if AUTOEXEC must be executed at CP/M cold boot, 0 otherwise
byte          errCodeSD;                  // Temporary variable to store error codes from the PetitFS
byte          numReadBytes;               // Number of read bytes after a readSD() call

// Disk emulation on SD
char          diskName[12]    = Z80DISK;  // String used for virtual disk file name
char          OsName[12]      = DS_OSNAME;// String used for file holding the OS name
word          trackSel;                   // Store the current track number [0..511]
byte          sectSel;                    // Store the current sector number [0..31]
byte          diskErr         = 19;       // SELDISK, SELSECT, SELTRACK, WRITESECT, READSECT or SDMOUNT resulting 
                                          //  error code
byte          numWriBytes;                // Number of written bytes after a writeSD() call
byte          diskSet;                    // Current "Disk Set"

ESP32SPISlave slave;

static constexpr uint32_t BUFFER_SIZE {40};
byte spi_slave_tx_buf[BUFFER_SIZE];
byte spi_slave_rx_buf[BUFFER_SIZE];

File file;

void setup() {
  Serial.begin(115200);
  //pinMode(CS, INPUT);
  pinMode(LED, OUTPUT);
  //delay(2000);
  memset(spi_slave_tx_buf, 0, BUFFER_SIZE);
  memset(spi_slave_rx_buf, 0, BUFFER_SIZE);
  
  slave.setDataMode(SPI_MODE0);
  slave.begin();



}

void loop() {
  byte e, s;
  word p;
  // put your main code here, to run repeatedly:
  slave.wait(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);

  while (slave.available()){
    ioOpcode = spi_slave_rx_buf[0];
    switch (ioOpcode){
      // Commands IOS compatible
      case (0x09):
        // DISK EMULATION
        // SELDISK
        ioData = spi_slave_rx_buf[1];
        if (ioData <= maxDiskNum)               // Valid disk number
        // Set the name of the file to open as virtual disk, and open it
        {
          diskName[3] = diskSet + 48;           // Set the current Disk Set
          diskName[5] = (ioData / 10) + 48;     // Set the disk number
          diskName[6] = ioData - ((ioData / 10) * 10) + 48;
          fileNameSD = diskName; // update the file name to open
        }
        else diskErr = 16;                      // Illegal disk number
        
      break;


      case (0x87):
        // DISK EMULATION
        // SDMOUNT
      break;
      
      // ----------  FROM HERE NEW OPCODES  ------------

      case (0x10):
        // LED ON PIN 22
        ioData = spi_slave_rx_buf[1];
        if (ioData == 0){
          digitalWrite(LED, LOW);
          spi_slave_tx_buf[0] = 0;
        }else{
          digitalWrite(LED, HIGH);
          spi_slave_tx_buf[0] = 1;
        }
      break;

      case (0x11):
        // SYNC VARIABLE VALUES
        syncVariable(spi_slave_rx_buf[1], spi_slave_rx_buf[2]);
      break;

      case (0x12):
        // SYNC BOOT MODE
        setBootMode(spi_slave_rx_buf[1]);
      break;

      // Remotized side of disk function:
      // mountSD, openSD, readSD, writeSD, seekSD

      case (0x13):
        // remote mountSD
        if (SD.begin()){
          if (SD.cardType() == CARD_NONE){
            spi_slave_tx_buf[0] = 2;  // no card
          }else{
            spi_slave_tx_buf[0] = 0;  // ok
          }
        }else{
          spi_slave_tx_buf[0] = 1; // problem while mounting
        }
        //Serial.print("Mount result: ");Serial.println(spi_slave_tx_buf[0]);
        
      break;

      case (0x14):
        // remote openSD
        file = SD.open(fileNameSD, "r+");
        //Serial.print("File open: ");Serial.println(fileNameSD);
        if (file){
          spi_slave_tx_buf[0] = 0; // ok
        }else{
          spi_slave_tx_buf[0] = 3; // problem while opening file
        }
        //Serial.print("Open result: ");Serial.println(spi_slave_tx_buf[0]);
        
      break;

      case (0x15):
        // remote readSD
        
        if (file){
          s = 32;
          e = file.read(spi_slave_tx_buf + 2, s);
          spi_slave_tx_buf[0] = e;
          spi_slave_tx_buf[1] = 0; // no errors
          //for (int t=0;t<e;t++){
            //Serial.print(spi_slave_tx_buf[t+2]); Serial.print(" ");
          //}
          //Serial.println("");

        }else{
          spi_slave_tx_buf[0] = 0;
          spi_slave_tx_buf[1] = 4; // error while reading
        }
        

       break;

       case (0x16):
         // remote seekSD
         p = (spi_slave_rx_buf[1] << 8) + spi_slave_rx_buf[2];
         if (file){
           file.seek(((unsigned long) p) << 9);
           spi_slave_tx_buf[0] = 0;
         }else{
           spi_slave_tx_buf[0] = 6; // error code
         }
       break;

       case (0x17):
         // remote writeSD
         if (file){
           s = 32;
           e = file.write(spi_slave_rx_buf + 1, s);
           spi_slave_tx_buf[0] = e;
           spi_slave_tx_buf[1] = 0; // no errors
           
         }else{
           spi_slave_tx_buf[0] = 0;
           spi_slave_tx_buf[1] = 5; // error code
         }
       break;

       case (0x18):
         // remote closefile
         file.close();
       break;

       case (0xFF):
          // tx code do nothing
       break;
    }
    slave.pop();  
  }
  
}

void syncVariable(byte var, byte value){
  switch (var){
    case (SYNC_BOOTMODE):     // bootMode
      bootMode = value;
      Serial.print("bootMode: ");Serial.println(bootMode);
    break;
    case (SYNC_DISKSET):      // diskSet
      diskSet = value;
      Serial.print("diskSet: ");Serial.println(diskSet);
    break;
    case (SYNC_AUTOEXECFLAG):  // autoexecFlag
      autoexecFlag = value;
      Serial.print("autoexecFlag: ");Serial.println(autoexecFlag);
    break; 
  }
}

void setBootMode(byte bootMode){
  Serial.print("Sync boot mode: ");Serial.println(bootMode);
  switch (bootMode)
  {
    case 0:                                       // Load Basic from SD
      fileNameSD = BASICFN;
      BootStrAddr = BASSTRADDR;
    break;
    
    case 1:                                       // Load Forth from SD
      fileNameSD = FORTHFN;
      BootStrAddr = FORSTRADDR;
    break;
    case 2:                                       // Load an OS from current Disk Set on SD
      switch (diskSet)
      {
      case 0:                                     // CP/M 2.2
        fileNameSD = CPMFN;
        BootStrAddr = CPMSTRADDR;
      break;

      case 1:                                     // QP/M 2.71
        fileNameSD = QPMFN;
        BootStrAddr = QPMSTRADDR;
      break;

      case 2:                                     // CP/M 3.0
        fileNameSD = CPM3FN;
        BootStrAddr = CPM3STRADDR;
      break;
      }
    break;
    
    case 3:                                       // Load AUTOBOOT.BIN from SD (load an user executable binary file)
      fileNameSD = AUTOFN;
      BootStrAddr = AUTSTRADDR;
    break;
    
  }
}
