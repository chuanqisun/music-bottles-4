/*
   minimal_gpio.c
   2016-04-30
   Public Domain
*/

/*
   gcc -o minimal_gpio minimal_gpio.c
   sudo ./minimal_gpio
*/

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static volatile uint32_t piModel = 1;
static volatile uint32_t piPeriphBase = 0x20000000;
static volatile uint32_t piBusAddr = 0x40000000;
static volatile int isPi4 = 0;  /* Flag for Pi 4 specific handling */

#define SYST_BASE  (piPeriphBase + 0x003000)
#define DMA_BASE   (piPeriphBase + 0x007000)
#define CLK_BASE   (piPeriphBase + 0x101000)
#define GPIO_BASE  (piPeriphBase + 0x200000)
#define UART0_BASE (piPeriphBase + 0x201000)
#define PCM_BASE   (piPeriphBase + 0x203000)
#define SPI0_BASE  (piPeriphBase + 0x204000)
#define I2C0_BASE  (piPeriphBase + 0x205000)
#define PWM_BASE   (piPeriphBase + 0x20C000)
#define BSCS_BASE  (piPeriphBase + 0x214000)
#define UART1_BASE (piPeriphBase + 0x215000)
#define I2C1_BASE  (piPeriphBase + 0x804000)
#define I2C2_BASE  (piPeriphBase + 0x805000)
#define DMA15_BASE (piPeriphBase + 0xE05000)

#define DMA_LEN   0x1000 /* allow access to all channels */
#define CLK_LEN   0xA8
#define GPIO_LEN  0xB4
#define SYST_LEN  0x1C
#define PCM_LEN   0x24
#define PWM_LEN   0x28
#define I2C_LEN   0x1C
#define BSCS_LEN  0x40

#define GPSET0 7
#define GPSET1 8

#define GPCLR0 10
#define GPCLR1 11

#define GPLEV0 13
#define GPLEV1 14

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39

/* Pi 4 uses different pull-up/down registers (GPIO_PUP_PDN_CNTRL) */
#define GPPUPPDN0 57  /* GPIO Pull-up / Pull-down Register 0 */
#define GPPUPPDN1 58  /* GPIO Pull-up / Pull-down Register 1 */
#define GPPUPPDN2 59  /* GPIO Pull-up / Pull-down Register 2 */
#define GPPUPPDN3 60  /* GPIO Pull-up / Pull-down Register 3 */

#define SYST_CS  0
#define SYST_CLO 1
#define SYST_CHI 2

static volatile uint32_t  *gpioReg = MAP_FAILED;
static volatile uint32_t  *systReg = MAP_FAILED;
static volatile uint32_t  *bscsReg = MAP_FAILED;

#define PI_BANK (gpio>>5)
#define PI_BIT  (1<<(gpio&0x1F))

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

void gpioSetMode(unsigned gpio, unsigned mode)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
}

int gpioGetMode(unsigned gpio)
{
   int reg, shift;

   reg   =  gpio/10;
   shift = (gpio%10) * 3;

   return (*(gpioReg + reg) >> shift) & 7;
}

/* Values for pull-ups/downs off, pull-down and pull-up. */

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

void gpioSetPullUpDown(unsigned gpio, unsigned pud)
{
   if (isPi4)
   {
      /* Pi 4 uses GPIO_PUP_PDN_CNTRL registers instead of GPPUD/GPPUDCLK */
      /* Each GPIO uses 2 bits: 00=no resistor, 01=pull-up, 10=pull-down */
      int regOffset = GPPUPPDN0 + (gpio / 16);
      int shift = (gpio % 16) * 2;
      uint32_t pull;
      
      /* Map PI_PUD values to Pi 4 values */
      switch(pud)
      {
         case PI_PUD_OFF:  pull = 0; break;  /* No resistor */
         case PI_PUD_UP:   pull = 1; break;  /* Pull up */
         case PI_PUD_DOWN: pull = 2; break;  /* Pull down */
         default:          pull = 0; break;
      }
      
      uint32_t reg = *(gpioReg + regOffset);
      reg &= ~(3 << shift);  /* Clear the 2 bits */
      reg |= (pull << shift);  /* Set the new value */
      *(gpioReg + regOffset) = reg;
   }
   else
   {
      /* Pi 1/2/3 use GPPUD and GPPUDCLK registers */
      *(gpioReg + GPPUD) = pud;

      usleep(20);

      *(gpioReg + GPPUDCLK0 + PI_BANK) = PI_BIT;

      usleep(20);
     
      *(gpioReg + GPPUD) = 0;

      *(gpioReg + GPPUDCLK0 + PI_BANK) = 0;
   }
}

int gpioRead(unsigned gpio)
{
   if ((*(gpioReg + GPLEV0 + PI_BANK) & PI_BIT) != 0) return 1;
   else                                         return 0;
}

void gpioWrite(unsigned gpio, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}

void gpioTrigger(unsigned gpio, unsigned pulseLen, unsigned level)
{
   if (level == 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;

   usleep(pulseLen);

   if (level != 0) *(gpioReg + GPCLR0 + PI_BANK) = PI_BIT;
   else            *(gpioReg + GPSET0 + PI_BANK) = PI_BIT;
}

/* Bit (1<<x) will be set if gpio x is high. */

uint32_t gpioReadBank1(void) { return (*(gpioReg + GPLEV0)); }
uint32_t gpioReadBank2(void) { return (*(gpioReg + GPLEV1)); }

/* To clear gpio x bit or in (1<<x). */

void gpioClearBank1(uint32_t bits) { *(gpioReg + GPCLR0) = bits; }
void gpioClearBank2(uint32_t bits) { *(gpioReg + GPCLR1) = bits; }

/* To set gpio x bit or in (1<<x). */

void gpioSetBank1(uint32_t bits) { *(gpioReg + GPSET0) = bits; }
void gpioSetBank2(uint32_t bits) { *(gpioReg + GPSET1) = bits; }

unsigned gpioHardwareRevision(void)
{
   static unsigned rev = 0;

   FILE * filp;
   char buf[512];
   char term;
   int chars=4; /* number of chars in revision string */
   unsigned tempRev = 0;

   if (rev) return rev;

   piModel = 0;
   isPi4 = 0;

   filp = fopen ("/proc/cpuinfo", "r");

   if (filp != NULL)
   {
      while (fgets(buf, sizeof(buf), filp) != NULL)
      {
         if (piModel == 0)
         {
            if (!strncasecmp("model name", buf, 10))
            {
               if (strstr (buf, "ARMv6") != NULL)
               {
                  piModel = 1;
                  chars = 4;
                  piPeriphBase = 0x20000000;
                  piBusAddr = 0x40000000;
               }
               else if (strstr (buf, "ARMv7") != NULL)
               {
                  piModel = 2;
                  chars = 6;
                  piPeriphBase = 0x3F000000;
                  piBusAddr = 0xC0000000;
               }
               else if (strstr (buf, "ARMv8") != NULL)
               {
                  /* Could be Pi 3 (64-bit) or Pi 4, need to check revision */
                  piModel = 2;
                  chars = 6;
                  piPeriphBase = 0x3F000000;
                  piBusAddr = 0xC0000000;
               }
            }
         }

         if (!strncasecmp("revision", buf, 8))
         {
            if (sscanf(buf+strlen(buf)-(chars+1),
               "%x%c", &tempRev, &term) == 2)
            {
               if (term == '\n') 
               {
                  rev = tempRev;
                  
                  /* Check for new-style revision code (bit 23 set) */
                  /* Pi 4 type codes: 0x11 = Pi 4B, 0x13 = Pi 400, 0x14 = CM4 */
                  if (rev & (1 << 23))
                  {
                     int type = (rev >> 4) & 0xFF;
                     if (type == 0x11 || type == 0x13 || type == 0x14)
                     {
                        /* This is a Pi 4 */
                        piModel = 4;
                        isPi4 = 1;
                        piPeriphBase = 0xFE000000;
                        piBusAddr = 0xC0000000;
                     }
                  }
               }
            }
         }
      }

      fclose(filp);
   }
   return rev;
}

/* Returns the number of microseconds after system boot. Wraps around
   after 1 hour 11 minutes 35 seconds.
*/

uint32_t gpioTick(void) { return systReg[SYST_CLO]; }


/* Map in registers. */

static uint32_t * initMapMem(int fd, uint32_t addr, uint32_t len)
{
    return (uint32_t *) mmap(0, len,
       PROT_READ|PROT_WRITE|PROT_EXEC,
       MAP_SHARED|MAP_LOCKED,
       fd, addr);
}

int gpioInitialise(void)
{
   int fd;

   gpioHardwareRevision(); /* sets piModel, needed for peripherals address */

   fd = open("/dev/mem", O_RDWR | O_SYNC) ;

   if (fd < 0)
   {
      fprintf(stderr,
         "This program needs root privileges.  Try using sudo\n");
      return -1;
   }

   gpioReg  = initMapMem(fd, GPIO_BASE,  GPIO_LEN);
   systReg  = initMapMem(fd, SYST_BASE,  SYST_LEN);
   bscsReg  = initMapMem(fd, BSCS_BASE,  BSCS_LEN);

   close(fd);

   if ((gpioReg == MAP_FAILED) ||
       (systReg == MAP_FAILED) ||
       (bscsReg == MAP_FAILED))
   {
      fprintf(stderr,
         "Bad, mmap failed\n");
      return -1;
   }
   return 0;
}

/*
main()
{
   int i;

   if (gpioInitialise() < 0) return 1;

   gpioSetMode(13,0);
   gpioSetPullUpDown(13,2);

   for (i=0; i<54; i++)
   {
      printf("gpio=%d tick=%u mode=%d level=%d\n",
         i, gpioTick(), gpioGetMode(i), gpioRead(i));
   }

   for (i=0; i<16; i++)
   {
      printf("reg=%d val=%8X\n",
         i, bscsReg[i]);
   }
}

*/