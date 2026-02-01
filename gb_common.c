//=============================================================================
//
//
// Gertboard Common code
//
// This file is part of the gertboard test suite
//
//
// Copyright (C) Gert Jan van Loo & Myra VanInwegen 2012
// No rights reserved
// You may treat this program as if it was in the public domain
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
// Notes:
// 1/ In some Linux systems (e.g. Debian) the UART is used by Linux.
//    So for now do not demo the UART.
// 2/ At the moment (16-March-2012) there is no Linux driver for
//    the audio yet so the PWM is free.
//    This is likely to change and in that case the Linux
//    audio/PWM driver must be disabled.
//
// This file contains code use by all the test programs for individual 
// capabilities.

#include "gb_common.h"

/* 
 * Peripheral base addresses:
 * - Pi 1 (BCM2835): 0x20000000
 * - Pi 2/3 (BCM2836/BCM2837): 0x3F000000
 * - Pi 4 (BCM2711): 0xFE000000
 * 
 * We use runtime detection to determine the correct base.
 */
#define BCM2708_PERI_BASE_PI1    0x20000000
#define BCM2708_PERI_BASE_PI2_3  0x3F000000
#define BCM2708_PERI_BASE_PI4    0xFE000000

/* Offsets from peripheral base */
#define CLOCK_OFFSET             0x101000
#define GPIO_OFFSET              0x200000
#define PWM_OFFSET               0x20C000
#define SPI0_OFFSET              0x204000
#define UART0_OFFSET             0x201000
#define UART1_OFFSET             0x215000

/* Runtime-determined base addresses */
static unsigned int peri_base = BCM2708_PERI_BASE_PI2_3;  /* Default to Pi 2/3 */
static int is_pi4 = 0;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
char *clk_mem_orig, *clk_mem, *clk_map;
char *gpio_mem_orig, *gpio_mem, *gpio_map;
char *pwm_mem_orig, *pwm_mem, *pwm_map;
char *spi0_mem_orig, *spi0_mem, *spi0_map;
char *uart_mem_orig, *uart_mem, *uart_map;

// I/O access
volatile unsigned *gpio;
volatile unsigned *pwm;
volatile unsigned *clk;
volatile unsigned *spi0;
volatile unsigned *uart;


//
//  GPIO
//

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET0   *(gpio+7)  // Set GPIO high bits 0-31
#define GPIO_SET1   *(gpio+8)  // Set GPIO high bits 32-53

#define GPIO_CLR0   *(gpio+10) // Set GPIO low bits 0-31
#define GPIO_CLR1   *(gpio+11) // Set GPIO low bits 32-53
#define GPIO_PULL   *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock


//
//  UART 0
//

#define UART0_BAUD_HI *(uart+9)
#define UART0_BAUD_LO *(uart+10)


void setup_io();
void restore_io();

//
// This is a software loop to wait
// a short while.
//
void short_wait()
{ int w;
  for (w=0; w<100; w++)
  { w++;
    w--;
  }
} // short_wait


//
// Simple SW wait loop
//
void long_wait(int v)
{ int w;
  while (v--)
    for (w=-800000; w<800000; w++)
    { w++;
      w--;
    }
} // long_wait


//
// Detect Raspberry Pi model and set peripheral base address
// Returns: 1=Pi1, 2=Pi2/3, 4=Pi4, 0=unknown
//
static int detect_pi_model(void)
{
   FILE *fp;
   char text[256];
   unsigned int revision = 0;
   int model = 0;
   
   fp = fopen("/proc/cpuinfo", "r");
   if (!fp) return 0;
   
   while (fgets(text, sizeof(text), fp))
   {
      if (!strncmp(text, "Revision", 8))
      {
         char *colon = strchr(text, ':');
         if (colon)
         {
            sscanf(colon + 1, "%x", &revision);
         }
         break;
      }
   }
   fclose(fp);
   
   if (revision == 0) return 0;
   
   /* Check for new-style revision code (bit 23 set) */
   if (revision & (1 << 23))
   {
      int type = (revision >> 4) & 0xFF;
      /* Type 0x11 = Pi 4B, 0x13 = Pi 400, 0x14 = CM4 */
      if (type == 0x11 || type == 0x13 || type == 0x14)
      {
         model = 4;
         peri_base = BCM2708_PERI_BASE_PI4;
         is_pi4 = 1;
      }
      else
      {
         /* Other new-style codes are Pi 2/3 */
         model = 2;
         peri_base = BCM2708_PERI_BASE_PI2_3;
         is_pi4 = 0;
      }
   }
   else
   {
      /* Old-style revision code */
      if (revision < 4)
      {
         model = 1;
         peri_base = BCM2708_PERI_BASE_PI1;
      }
      else
      {
         model = 2;
         peri_base = BCM2708_PERI_BASE_PI2_3;
      }
      is_pi4 = 0;
   }
   
   return model;
}


//
// Set up memory regions to access the peripherals.
// This is a bit of 'magic' which you should not touch.
// It it also the part of the code which makes that
// you have to use 'sudo' to run this program.
//
void setup_io()
{  unsigned long extra;
   unsigned int clock_base, gpio_base, pwm_base, spi0_base, uart0_base;
   
   /* Detect Pi model and set peripheral base */
   detect_pi_model();
   
   /* Calculate base addresses from peripheral base + offsets */
   clock_base = peri_base + CLOCK_OFFSET;
   gpio_base = peri_base + GPIO_OFFSET;
   pwm_base = peri_base + PWM_OFFSET;
   spi0_base = peri_base + SPI0_OFFSET;
   uart0_base = peri_base + UART0_OFFSET;

   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("Can't open /dev/mem\n");
      printf("Did you forgot to use 'sudo .. ?'\n");
      exit (-1);
   }

   /*
    * mmap clock
    */
   if ((clk_mem_orig = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }
   extra = (unsigned long)clk_mem_orig % PAGE_SIZE;
   if (extra)
     clk_mem = clk_mem_orig + PAGE_SIZE - extra;
   else
     clk_mem = clk_mem_orig;

   clk_map = (unsigned char *)mmap(
      (caddr_t)clk_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      clock_base
   );

   if ((long)clk_map < 0) {
      printf("clk mmap error %d\n", (int)clk_map);
      exit (-1);
   }
   clk = (volatile unsigned *)clk_map;


   /*
    * mmap GPIO
    */
   if ((gpio_mem_orig = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }
   extra = (unsigned long)gpio_mem_orig % PAGE_SIZE;
   if (extra)
     gpio_mem = gpio_mem_orig + PAGE_SIZE - extra;
   else
     gpio_mem = gpio_mem_orig;

   gpio_map = (unsigned char *)mmap(
      (caddr_t)gpio_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      gpio_base
   );

   if ((long)gpio_map < 0) {
      printf("gpio mmap error %d\n", (int)gpio_map);
      exit (-1);
   }
   gpio = (volatile unsigned *)gpio_map;

   /*
    * mmap PWM
    */
   if ((pwm_mem_orig = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }
   extra = (unsigned long)pwm_mem_orig % PAGE_SIZE;
   if (extra)
     pwm_mem = pwm_mem_orig + PAGE_SIZE - extra;
   else
     pwm_mem = pwm_mem_orig;

   pwm_map = (unsigned char *)mmap(
      (caddr_t)pwm_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      pwm_base
   );

   if ((long)pwm_map < 0) {
      printf("pwm mmap error %d\n", (int)pwm_map);
      exit (-1);
   }
   pwm = (volatile unsigned *)pwm_map;

   /*
    * mmap SPI0
    */
   if ((spi0_mem_orig = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }
   extra = (unsigned long)spi0_mem_orig % PAGE_SIZE;
   if (extra)
     spi0_mem = spi0_mem_orig + PAGE_SIZE - extra;
   else
     spi0_mem = spi0_mem_orig;

   spi0_map = (unsigned char *)mmap(
      (caddr_t)spi0_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      spi0_base
   );

   if ((long)spi0_map < 0) {
      printf("spi0 mmap error %d\n", (int)spi0_map);
      exit (-1);
   }
   spi0 = (volatile unsigned *)spi0_map;

   /*
    * mmap UART
    */
   if ((uart_mem_orig = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }
   extra = (unsigned long)uart_mem_orig % PAGE_SIZE;
   if (extra)
     uart_mem = uart_mem_orig + PAGE_SIZE - extra;
   else
     uart_mem = uart_mem_orig;

   uart_map = (unsigned char *)mmap(
      (caddr_t)uart_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      uart0_base
   );

   if ((long)uart_map < 0) {
      printf("uart mmap error %d\n", (int)uart_map);
      exit (-1);
   }
   uart = (volatile unsigned *)uart_map;

} // setup_io

//
// Undo what we did above
//
void restore_io()
{
  munmap(uart_map,BLOCK_SIZE);
  munmap(spi0_map,BLOCK_SIZE);
  munmap(pwm_map,BLOCK_SIZE);
  munmap(gpio_map,BLOCK_SIZE);
  munmap(clk_map,BLOCK_SIZE);
  // free memory
  free(uart_mem_orig);
  free(spi0_mem_orig);
  free(pwm_mem_orig);
  free(gpio_mem_orig);
  free(clk_mem_orig);
  close(mem_fd);
} // restore_io

// simple routine to convert the last several bits of an integer to a string 
// showing its binary value
// nbits is the number of bits in i to look at
// i is integer we want to show as a binary number
// we only look at the nbits least significant bits of i and we assume that 
// s is at least nbits+1 characters long
void make_binary_string(int nbits, int i, char *s)
{ char *p;
  int bit;

  p = s;
  for (bit = 1 << (nbits-1); bit > 0; bit = bit >> 1, p++)
    *p = (i & bit) ? '1' : '0';
  *p = '\0';
}

//
// Find out what revision board the Raspberry Pi is
// Using the file '/proc/cpuinfo' for that.
// returns 1 or 2 for boards rev 1 or 2, and 0 if it wasn't able to tell
//
int pi_revision()
{
   FILE *fp;
   int  revision, match, number;
   char text[128]; // holds one line of file

   revision = 0;
   // Open the file with the CPU info
   fp = fopen("/proc/cpuinfo","r");
   if (fp)
   { // Find the line which starts 'Revision'
     while (fgets(text, 128, fp)) // get one line of text from file
     {
       if (!strncmp(text,"Revision",8)) // strncmp returns 0 if string matches
       {  // Get the revision number from the text
	 match = sscanf(text,"%*s : %0X",&number); // rev num is after the :
	 if (match == 1)
	   { // Yes, we have a revision number
	     if (number >= 4)
	       revision = 2;
	     else
	       revision = 1;
	   } // have number
          break; // no use in reading more lines, so break out of the while
       } // have revision text
     } // get line from file
     fclose(fp);
   } // Have open file

   return revision;
} // pi_revision
