#define CS 12
#define LED 22

// ------------------------------------------------------------------------------
//
// File names and starting addresses
//
// ------------------------------------------------------------------------------

#define   BASICFN       "BASIC47.BIN"
#define   FORTHFN       "FORTH13.BIN"
#define   CPMFN         "CPM22.BIN"
#define   QPMFN         "QPMLDR.BIN"
#define   CPM3FN        "CPMLDR.COM"      // CP/M 3 CPMLDR.COM loader
#define   AUTOFN        "AUTOBOOT.BIN"
#define   Z80DISK       "DSxNyy.DSK"      // Generic Z80 disk name (from DS0N00.DSK to DS9N99.DSK)
#define   DS_OSNAME     "DSxNAM.DAT"      // File with the OS name for Disk Set "x" (from DS0NAM.DAT to DS9NAM.DAT)
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

const byte    debug        = 0;           // Debug off = 0, on = 1, on = 2 with interrupt trace
const byte    bootModeAddr = 10;          // Internal EEPROM address for boot mode storage
const byte    autoexecFlagAddr = 12;      // Internal EEPROM address for AUTOEXEC flag storage
const byte    clockModeAddr = 13;         // Internal EEPROM address for the Z80 clock high/low speed switch
                                          //  (1 = low speed, 0 = high speed)
const byte    diskSetAddr  = 14;          // Internal EEPROM address for the current Disk Set [0..9]
const byte    maxDiskNum   = 99;          // Max number of virtual disks
const byte    maxDiskSet   = 3;           // Number of configured Disk Sets

// General purpose variables
byte          ioData;                     // Data byte used for the I/O operation
byte          ioOpcode       = 0xFF;      // I/O operation code or Opcode (0xFF means "No Operation")
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
char          diskName[11]    = Z80DISK;  // String used for virtual disk file name
char          OsName[11]      = DS_OSNAME;// String used for file holding the OS name
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

byte *rxData;

void setup() {
 
  rxData = spi_slave_rx_buf + 1;
  pinMode(CS, OUTPUT);
  pinMode(LED, OUTPUT);
  memset(spi_slave_tx_buf, 0, BUFFER_SIZE);
  memset(spi_slave_rx_buf, 0, BUFFER_SIZE);
  slave.setDataMode(SPI_MODE0);
  slave.begin();



}

void loop() {
  // put your main code here, to run repeatedly:
  slave.wait(spi_slave_rx_buf, spi_slave_tx_buf, BUFFER_SIZE);
  while (slave.available()){
    ioOpcode = spi_slave_rx_buf[0];
    switch (ioOpcode){
      // Commands IOS compatible
      case 0x09:
        // DISK EMULATION
        // SELDISK
        iodata = spi_slave_rx_buf[1];
        if (ioData <= maxDiskNum)               // Valid disk number
        // Set the name of the file to open as virtual disk, and open it
        {
          diskName[2] = diskSet + 48;           // Set the current Disk Set
          diskName[4] = (ioData / 10) + 48;     // Set the disk number
          diskName[5] = ioData - ((ioData / 10) * 10) + 48;

          //-------------- modify here -------------------
          diskErr = openSD(diskName);           // Open the "disk file" corresponding to the given disk number
        }
        else diskErr = 16;                      // Illegal disk number
        
      break;

      case 0x0A:
        // DISK EMULATION
        // SELTRACK
      break;

      case 0x0B:
        // DISK EMULATION
        // SELSEC
      break;

      case 0x0C:
        // DISK EMULATION
        // WRITESECT
      break;

      case 0x85:
        // DISK EMULATION
        // ERRDISK
      break;

      case 0x86:
        // DISK EMULATION
        // READSECT
      break;

      case 0x87:
        // DISK EMULATION
        // SDMOUNT
      break;
      
      // ----------  FROM HERE NEW OPCODES  ------------

      case 0x10:
        // LED ON PIN 22
        ioData = spi_slave_rx_buf[1];
        if (ioData == 0){
          digitalWrite(LED, LOW);
        }else{
          digitalWrite(LED, HIGH);
        }
      break;

      case 0x11:
        // SYNC VARIABLE VALUES
        syncVariable(spi_slave_rx_buf[1],spi_slave_rx_buf[2]);
      break;

      case 0x12:
        // SYNC BOOT MODE
        setBootMode(spi_slave_rx_buf[1]);
      break;

      // Remotized side of disk function:
      // mountSD, openSD, readSD, writeSD, seekSD

      case 0x13:
        // remote mountSD
        spi_slave_tx_buf[0] = SD.begin();
      break;

      case 0x14:
        // remote openSD
        errCodeSD = openSD(fileNameSD);
        spi_slave_tx_buf[0] = errCodeSD;
        
      break;

    }
    
  }
  slave.pop();
}

void syncVariable(byte var, byte value){
  switch (var){
    case (SYNC_BOOTMODE):     // bootMode
      bootMode = value;
    break;
    case (SYNC_DISKSET):      // diskSet
      diskSet = value;
    break;
    case (SYNC_AUTOEXECFLAG):  // autoexecFlag
      autoexecFlag = value;
    break; 
  }
}

void setBootMode(byte bootMode){

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

byte openSD(const char* fileName)
{
//  return pf_open(fileName);
  File file = fs.open(fileNameSD);
  if(!file) return 1; else return 0;

}
