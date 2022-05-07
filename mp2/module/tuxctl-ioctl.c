/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

// Add for mp2
#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

static int reset;				// reset used to determine the ack of MTCP reset
static int finish;				// finish used to determine other task's ack

// store the argument of tuxctl_handle_packet
static unsigned char bit[2];
static char prev_led[6];

#define BIT1  0x1		// Used to compare the last bit
#define BYTE1 0xF		// Used to mask last byte

int tuxctl_ioctl_tux_init(struct  tty_struct* tty);
int tuxctl_ioctl_tux_buttons(struct tty_struct* tty, unsigned long arg);
void tux_reset_helper(struct tty_struct* tty);
int tuxctl_ioctl_tux_set_led(struct tty_struct* tty, unsigned long arg);


#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet(struct tty_struct* tty, unsigned char* packet)
{
    unsigned  a, b, c;
	char buf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

	/*printk("packet : %x %x %x\n", a, b, c); */
    switch(a)
    {		
    	case MTCP_ACK:
    		//printk("check initialized");
    		if (reset == 1) {				// Determine what kind of ack
				finish = 0;
				tuxctl_ldisc_put(tty, prev_led, 6);
				reset = 0;
			}
			else
				finish = 1;		
    		break;
    	case MTCP_BIOC_EVENT:
			bit[0] = b;				// Store the packet value and used by push button
			bit[1] = c;
    		break;
    	case MTCP_RESET: 					

			finish = 0;
			tuxctl_ldisc_put(tty, buf, 2);	// reset = 1 to prevent other instruction
			reset = 1;
    		break;
    	default:
    		return;
    }

/*
    printk("packet : %x %x %x\n", a, b, c); 
    printk("b which is array[0] is : %x \n", array[0]);
    printk("c which is array[1] is : %x \n", array[1]);
*/
}
/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/

/* 
 * tuxctl_ioctl
 *   DESCRIPTION: Act as a dispatcher based on the value of cmd
 *   INPUTS: tty -- the structure write to the device
 * 			file -- the file related to the ioctl
 *          cmd -- determine which operation
 *          arg -- the argument pass to the selected operation
 * 	 OUTPUTS: None
 *   RETURN VALUE: 0 on success, -EINVAL on failure
 *   SIDE EFFECTS: Write information to the tux device
 * 
 */
int tuxctl_ioctl (struct tty_struct* tty, struct file* file, unsigned cmd, unsigned long arg) {
	// Use cmd to determine which instruction
    switch (cmd) {
		case TUX_INIT:
			if (tuxctl_ioctl_tux_init(tty) > 0)
				return -EINVAL;
			return 0;
		case TUX_BUTTONS:
			if (((int*)arg == NULL) || (tuxctl_ioctl_tux_buttons(tty, arg) > 0))
				return -EINVAL;
			return 0;
		case TUX_SET_LED:
			if (finish == 0)			// Check whether the previous task is ack
				return 0;
			finish = 0;
			if (tuxctl_ioctl_tux_set_led(tty, arg) > 0)
				return -EINVAL;
			return 0;
		case TUX_LED_ACK: return -EINVAL;
		case TUX_LED_REQUEST: return -EINVAL;
		case TUX_READ_LED: return -EINVAL;
		default:
			return -EINVAL;
    }
}

/* 
 * tuxctl_ioctl_tux_init
 *   DESCRIPTION: Initialize the tux
 *   INPUTS: tty -- the structure write to the device
 * 	 OUTPUTS: None
 *   RETURN VALUE: 0 on success, -EINVAL on failure
 *   SIDE EFFECTS: Write information to the tux device
 * 
 */
int tuxctl_ioctl_tux_init(struct  tty_struct* tty) {
	char buf[2] = {MTCP_BIOC_ON, MTCP_LED_USR};
	reset = 0;
	finish = 0;
	return tuxctl_ldisc_put(tty, buf, 2);;
}


/* 
 * tuxctl_ioctl_tux_buttons
 *   DESCRIPTION: Write the touched button information to user
 *   INPUTS: tty -- the structure write to the device
 *           arg -- user-level pointer
 * 	 OUTPUTS: None
 *   RETURN VALUE: 0 on success, -EINVAL on failure
 *   SIDE EFFECTS: Write information to the user space
 * 
 */
int tuxctl_ioctl_tux_buttons(struct tty_struct* tty, unsigned long arg) {
	int copy_value;		// Hold the bits write to user space
	int bit5, bit6;     // Used to exchange fifth and sixth bit of copy_value
	int both_mask;		// Clear the fifth and sixth bit of copy_value

	// bit[0]: XXXXCBAS  bit[1]: XXXXRDLU
	copy_value = (bit[0] & 0xF) | ((bit[1] & 0xF) << 4);

	// Transform RDLUCBAS to RLDUCBAS
	// Exchange bit 5 and 6
	bit5 = 0x20;				// 00100000
	bit6 = 0x40;				// 01000000
	both_mask = 0x9F;			// 10010000
	bit5 = (bit5 & copy_value) << 1;
	bit6 = (bit6 & copy_value) >> 1;
	copy_value &= both_mask;
	copy_value |= (bit5 | bit6);
	// Copy to user
	return copy_to_user((int*)arg, &copy_value, sizeof(copy_value));;
}


/* 
 * tuxctl_ioctl_tux_set_led
 *   DESCRIPTION: Write the time of the game to tux board
 *   INPUTS: tty -- the structure write to the device
 *           arg -- information of the time
 * 	 OUTPUTS: None
 *   RETURN VALUE: 0 on success, -EINVAL on failure
 *   SIDE EFFECTS: Display the time on tux board
 * 
 */
int
tuxctl_ioctl_tux_set_led(struct tty_struct* tty, unsigned long arg)
{
	/* Reference
		  _A
		F| |B               
		  -G
		E| |C
		  -D .dp

		__7___6___5___4____3___2___1___0__
		| A | E | F | dp | G | C | B | D | 
		+---+---+---+----+---+---+---+---+
	*/
	int digit[16] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAE, 0xEE, 0x6D, 0xE1, 0x4F,0xE9, 0xE8};
	int i;							// Used for the for loop
	unsigned char buf[6];			// data sent to the device
	int decimals_on = (0xF & (arg >> 24));				// Determine which decimal is on
	int led_on = (0xF & (arg >> 16)); 
	
	buf[0] = MTCP_LED_SET;
	buf[1] = 0x0F;					// All led needs to prepare working
	for (i = 2; i < 6 ; i++) {
		buf[i] = 0x00;
		if((led_on >> (i - 2)) & BIT1)
		{
			if((decimals_on >> (i - 2)) & BIT1)
				buf[i] = digit[(arg >> (4 * (i - 2))) & BYTE1] + 0x10;		// 0x10 stands for the dp bit
			else
				buf[i] = digit[(arg >> (4 * (i - 2))) & BYTE1];
		}
	}

	if (tuxctl_ldisc_put(tty, buf, i) > 0)		// Determine whethe the put is successful
		return -EINVAL;

	for (i = 0; i < 6; i++)						// store the buf information
		prev_led[i] = buf[i];

	return 0;
}
