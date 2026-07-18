#ifndef RTC_H
#define RTC_H

#include <file_system/vfs.h>

#include <stdint.h>

// A single point-in-time reading from CMOS RTC
typedef struct
{
    uint32_t year;                  // Full year (century assumed - see rtc.c)
    uint8_t month;                  // 1 - 12
    uint8_t day;                    // 1 - 31
    uint8_t hour;                   // 0 - 23
    uint8_t minute;                 // 0 - 59
    uint8_t second;                 // 0 - 59
} rtc_time_t;

// Read once at boot, log to serial
void rtc_init();

// Read the current date/time from CMOS
void rtc_read(rtc_time_t* out);

// Current time as seconds since 1970-01-01
uint64_t rtc_now_epoch();

// Current time as seconds since 1970-01-01
int dev_rtc_read(file_t* file, void* buffer, uint32_t size);

#endif