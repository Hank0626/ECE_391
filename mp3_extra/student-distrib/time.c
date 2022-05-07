#include "time.h"
#include "rtc.h"
#include "lib.h"
#include "terminal.h"
#include "gui.h"
// Reference: https://wiki.osdev.org/CMOS#Getting_Current_Date_and_Time_from_RTC

/* int32_t get_update_in_progress_flag();
 * Inputs: None
 * Return Value: The waiting time
 * Function: Wait for cmos to generate the next time */
/* Side effect: none.*/
int get_update_in_progress_flag() {
    outb(0x0A, CMOS_ADDR);
    int input = inb(CMOS_DATA);
    return input & 0x80;
}

/* int32_t get_RTC_register(int reg);
 * Inputs: reg -- the register port number
 * Return Value: the data from the corresponding resgister
 * Function: Read the data from the corresponding register */
/* Side effect: none.*/
unsigned get_RTC_register(int reg) {
    outb(reg, CMOS_ADDR);
    return inb(CMOS_DATA);
}

/* int32_t get_system_time();
 * Inputs: None
 * Return Value: None
 * Function: Get the shystem time and draw it to the screen */
void get_system_time() {
    int counter = 0;
    while (get_update_in_progress_flag() || counter >= 10000) {
        counter++;
    }
    // read the time from the register
    sec = get_RTC_register(0x00);
    mins = get_RTC_register(0x02);
    hour = get_RTC_register(0x04);
    day = get_RTC_register(0x07);
    month = get_RTC_register(0x08);
    year = get_RTC_register(0x09);

    // Get the mode of the RTC register
    int Mode = get_RTC_register(0x0B);
    if (!(Mode & 0x04)) {
        hour = (hour >> 4) * 10 + (hour & 0x0f);
        mins = (mins >> 4) * 10 + (mins & 0x0f);
        sec = (sec >> 4) * 10 + (sec & 0x0f);
        year = (year >> 4) * 10 + (year & 0x0f);
        month = (month >> 4) * 10 + (month & 0x0f);
        day = (day >> 4) * 10 + (day & 0x0f);
    }

    if (!(Mode & 0x02) && (hour & 0x80)) hour = ((hour & 0x7F) + 12) % 24;

    // Draw the time to the screen
    draw_time_bar();
}

