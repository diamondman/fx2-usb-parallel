#include <stdio.h>
#include <stdint.h>
#include <fx2regs.h>
#include <fx2macros.h>
#include <serial.h>
#include <lights.h>
#include <delay.h>
#include <fx2ints.h>
#include <autovector.h>
#include <setupdat.h>
#include <eputils.h>

#include "wave_6800.h"

#define GLEAR_GPIF() CLEAR_GPIF(); //Fix till pull request for fx2lib is taken
#define SET_TRANSFER_COUNT(bak0, bak1, bak2, bak3)    \
  SYNCDELAY4;                        \
  GPIFTCB0 = bak0;                    \
  SYNCDELAY4;                        \
  GPIFTCB1 = bak1;                    \
  SYNCDELAY4;                        \
  GPIFTCB2 = bak2;                    \
  SYNCDELAY4;                        \
  GPIFTCB3 = bak3;                    \
  SYNCDELAY4;
#define WAIT_EP2FIFO_NOT_EMPTY() \
  SYNCDELAY4;                 \
  while (EP2FIFOFLGS & 2){}      \
  SYNCDELAY4;
#define WAIT_GPIF_DONE()      \
  while(!(GPIFTRIG & 0x80)){} \
  SYNCDELAY;

#define SYNCDELAY SYNCDELAY4;
#define PARALLEL_COMMAND 0xB5

#define SET_LIGHT 0
#define LIGHT_OFF 0
#define LIGHT_ON 1

#define SET_ADDRESS_MODE 1
#define ADDRESS_AUTO 2
#define ADDRESS_ONLY_DATA 1
#define ADDRESS_ONLY_CMD 0

#define WRITE_GPIO 2
#define SET 0
#define AND 1
#define OR 2
volatile __bit dosuspend;
volatile __bit got_sud;
volatile __bit autodata_mode;

BOOL handle_parallelcommand(){
  switch (SETUPDAT[2]) {
  case SET_LIGHT:
    {
      switch (SETUPDAT[4]) {
      case LIGHT_OFF:
        {
          d2off();
          return TRUE;
	}
      case LIGHT_ON:
	{
	  d2on();
	  return TRUE;
	}
      default:
	return FALSE;
      }
    }
  case SET_ADDRESS_MODE:
    {
      printf("ADDRESS MODE: ");
      switch (SETUPDAT[4]) {
      case ADDRESS_AUTO:
	{
	  printf("AUTO\n");
	  PORTCCFG = 0xFF;    // [7:0] as alt. func. GPIFADR[7:0]
	  OEC = 0xFF;         // and as outputs
	  IOC = 0;
	  autodata_mode = TRUE;
	  return TRUE;
	}
      case ADDRESS_ONLY_CMD:
	{
	  printf("CMD ONLY\n");
	  PORTCCFG = 0x00;  // [7:0] as port I/O
	  OEC = 0xFF;       // and as inputs
	  IOC = 0;
	  autodata_mode = FALSE;
	  return TRUE;
	}
      case ADDRESS_ONLY_DATA:
	{
	  printf("DATA ONLY\n");
	  PORTCCFG = 0x00;  // [7:0] as port I/O
	  OEC = 0xFF;       // and as inputs
	  IOC = 0xFF;
	  autodata_mode = FALSE;
	  return TRUE;
	}
      default:
	printf("UNKNOWN\n");
	return FALSE;
      }
    }
  case WRITE_GPIO:
    {
      uint8_t val = SETUPDAT[4];
      switch (SETUPDAT[5]) {
      case SET:
	{
	  IOA = val;
	  printf("SET IOA = %d, %d", IOA, val);
	  return TRUE;
	}
      case AND:
	{
	  IOA &= val;
	  printf("SET IOA &= %d, %d", IOA, val);
	  return TRUE;
	}
      case OR:
	{
	  IOA |= val;
	  printf("SET IOA |= %d, %d", IOA, val);
	  return TRUE;
	}
      default:
	return FALSE;
      }
    }
  default:
    return FALSE;
  }
}

BOOL handle_vendorcommand(uint8_t cmd) {

  switch ( cmd ) {
  case PARALLEL_COMMAND:
    {
      return handle_parallelcommand();
    }
  }
  return FALSE;
}

void suspend_isr() __interrupt SUSPEND_ISR {
  dosuspend=TRUE;
  CLEAR_SUSPEND();
}

void resume_isr() __interrupt RESUME_ISR {
  CLEAR_RESUME();
}

// copied routines from setupdat.h
BOOL handle_get_descriptor() {
  return FALSE;
}

// copied usb jt routines from usbjt.h
// this firmware only supports 0,0
BOOL handle_get_interface(uint8_t ifc, uint8_t* alt_ifc) {
  //printf ( "Get Interface\n" );
  if (ifc==0) {*alt_ifc=0; return TRUE;} else { return FALSE;}
}
BOOL handle_set_interface(uint8_t ifc, uint8_t alt_ifc) {
  //printf ( "Set interface %d to alt: %d\n" , ifc, alt_ifc );

  if (ifc==0&&alt_ifc==0) {
    // SEE TRM 2.3.7
    // reset toggles
    RESETTOGGLE(0x02);
    RESETTOGGLE(0x86);
    // restore endpoints to default condition
    RESETFIFO(0x02);
    EP2BCL=0x80;
    SYNCDELAY;
    EP2BCL=0X80;
    SYNCDELAY;
    RESETFIFO(0x86);
    return TRUE;
  } else
    return FALSE;
}

void sudav_isr() __interrupt SUDAV_ISR {
  got_sud=TRUE;
  CLEAR_SUDAV();
}

void sof_isr () __interrupt SOF_ISR __using 1 {
  CLEAR_SOF();
}

// get/set configuration
uint8_t handle_get_configuration() {
  return 1;
}
BOOL handle_set_configuration(uint8_t cfg) {
  return cfg == 1 ? TRUE : FALSE; // we only handle cfg 1
}

//Add something for suspend
//revisit these two functions to check for compliance
void usbreset_isr() __interrupt USBRESET_ISR {
  handle_hispeed(FALSE);
  CLEAR_USBRESET();
}
void hispeed_isr() __interrupt HISPEED_ISR {
  handle_hispeed(TRUE);
  CLEAR_HISPEED();
}

void init(){
  REVCTL = 0x00;//3 breaks everything. I followed the steps but no luck.
  SYNCDELAY;

  RENUMERATE_UNCOND();

  SETCPUFREQ(CLK_48M);
  SETIF48MHZ();
  sio0_init(57600);
  CKCON &= 0xF8; //Make memory access 2 cycles TRM page 272

  d2off();

  ENABLE_RESUME();
  USE_USB_INTS();
  ENABLE_SUDAV();
  ENABLE_SUSPEND();
  ENABLE_HISPEED();
  ENABLE_USBRESET();
  //ENABLE_SOF();

  GpifInit();
  SYNCDELAY;

  //Can't seem to get the GPIF editor to set these correctly. Overriding.
  PORTACFG = 0;
  OEA = 0xff;
  IOA = 0;
  //Not important
  //PORTCCFG = 0; //Wrong default. Overriding. C is the address.
  //OEC = 0x00;
  //PORTECFG = 0;
  //OEE = 0xD8; //For T*OUT

  //BIT 6 0 = OUT, 1 = IN. This is same direction as USB. IN means to PC
  EP2CFG &= 0xA2; //EP2 is READ FROM USB. 512byte OUT BULK set DOUBLE BUFF
  SYNCDELAY;
  EP6CFG = 0xE2;
  SYNCDELAY;


  EP1INCFG &= ~bmVALID;
  SYNCDELAY;
  EP1OUTCFG &= ~bmVALID;
  SYNCDELAY;
  EP4CFG &= ~bmVALID;
  SYNCDELAY;
  EP8CFG &= ~bmVALID;
  SYNCDELAY;

  //reset_FIFO_setup_ep
  SYNCDELAY;
  FIFORESET = 0x80;
  SYNCDELAY;
  FIFORESET = 0x02;
  SYNCDELAY;
  FIFORESET = 0x06;
  SYNCDELAY;
  FIFORESET = 0x00;
  SYNCDELAY;


  EP2FIFOCFG = 0x10;//0x15; //AUTO OUT, WORDWIDE=0
  EP6FIFOCFG = 0x08;//AUTO IN, WORDWIDE=0
  SYNCDELAY;


  // arm ep2
  EP2BCL = 0x80; // write once
  SYNCDELAY;
  EP2BCL = 0x80; // do it again
  OUTPKTEND = 0x82; //?? Needed??
  OUTPKTEND = 0x82;


  //Auto-commit 512-byte packets
  EP6AUTOINLENH = 0x02;
  SYNCDELAY;
  EP6AUTOINLENL = 0x00;
  SYNCDELAY;


  EA=1; // global interrupt enable

  d3off();


}

volatile WORD bytes;
void main(){
  //int a = 0x1234;
  //printf("VAL %d", a);
  bytes = 0;
  got_sud=FALSE;
  dosuspend = FALSE;
  autodata_mode = TRUE;

  init();
  printf("\n\n");

  SYNCDELAY4;
  printf("%d %d\n", EP2BCH, EP2BCL);
  while(TRUE) {
    if (dosuspend){
      printf("SUSPENDING\n");
      dosuspend = FALSE;

      /*//TURN EVERYTHING OFF
      OEA = 0;
      OEE = 0;

      //SUSPEND
      WAKEUPCS |= bmWU|bmWU2; // make sure ext wakeups are cleared
      SUSPEND=1;
      PCON |= 1;
      SYNCDELAY;
      SYNCDELAY;

      //TURN EVERYTHING BACK ON
      OEA = 0xFF;
      OEE = 0xD8; //For T*OUT*/
    }
    if ( got_sud ) {
      printf ( "CTRLPACKET\n" );
      got_sud = FALSE;
      handle_setupdata();
      }

    SYNCDELAY4;
    if (!(EP2FIFOFLGS & 2)){
      //SYNCDELAY4;
      //printf("EP2FIFOBC %d %d\n", EP2FIFOBCH, EP2FIFOBCL);
      SYNCDELAY4;
      SET_TRANSFER_COUNT(EP2FIFOBCL,EP2FIFOBCH,0,0);
      SYNCDELAY4;
      IOC = 0; //Make sure address starts at 0
      GPIFTRIG = 0;
      //SYNCDELAY4;
      //printf("FIFO GOT DATA %d %d %d %d...", 
      //       GPIFTCB0, GPIFTCB1, GPIFTCB2, GPIFTCB3);
      SYNCDELAY4;
      while(!(GPIFTRIG & 0x80)){
	//printf("FIFO GOT DATA %d %d %d %d\n", 
	//       GPIFTCB0, GPIFTCB1, GPIFTCB2, GPIFTCB3);
      }
      //SYNCDELAY4;
      //printf("FIFO GOT DATA %d %d %d %d\n", 
      //       GPIFTCB0, GPIFTCB1, GPIFTCB2, GPIFTCB3);
      //SYNCDELAY4;
      //printf("DONE\n");
      SYNCDELAY4;
    }
    
    
    /*if(!(EP2468STAT & 0x01) && !(EP2468STAT & 0x20)){
      WORD i;
      d4off();

      printf ( "Sending data to ep6 in\n");
      bytes = MAKEWORD(EP2BCH, EP2BCL);

      for (i=0; i<bytes; i++)
	EP6FIFOBUF[i] = EP2FIFOBUF[i];
      EP6BCH = MSB(bytes);
      SYNCDELAY;
      EP6BCL = LSB(bytes);
      EP2BCL = 0x80;
      }*/

  }
}
