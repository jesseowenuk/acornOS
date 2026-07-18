#include <drivers/rtc.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/string.h>

// --- CMOS ports -------------------------------------------------------
// Index port selects a register, data port reads/writes its value
#define CMOS_ADDRESS    0x70
#define CMOS_DATA       0x71

// --- CMOS RTC registers -----------------------------------------------
#define CMOS_REG_SECONDS    0x00
#define CMOS_REG_MINUTES    0x02
#define CMOS_REG_HOURS      0x04
#define CMOS_REG_DAY        0x07
#define CMOS_REG_MONTH      0x08
#define CMOS_REG_YEAR       0x09
#define CMOS_REG_STATUS_A   0x0A            // Bit 7 = update in progress
#define CMOS_REG_STATUS_B   0x0B            // Bit 2 = binary (vs BCD), bit 1 = 24h (vs 12)

// --- I/O helpers -------------------------------------------------------

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile(
        "outb %0, %1"
        :
        : "a"(value),                       // value in AL
          "Nd"(port)                        // value in DX
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(value)                       // result into value via AL
        : "Nd"(port)                        // port in DX
    );

    return value;
}

// --- CMOS access ---------------------------------------

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int cmos_update_in_progress()
{
    return cmos_read(CMOS_REG_STATUS_A) & 0x80;
}

static void cmos_read_raw(uint8_t* second, uint8_t* minute, uint8_t* hour, uint8_t* day, uint8_t* month, uint8_t* year)
{
    *second = cmos_read(CMOS_REG_SECONDS);
    *minute = cmos_read(CMOS_REG_MINUTES);
    *hour = cmos_read(CMOS_REG_HOURS);
    *day = cmos_read(CMOS_REG_DAY);
    *month = cmos_read(CMOS_REG_MONTH);
    *year = cmos_read(CMOS_REG_YEAR);
}

// --- rtc_read ------------------------------------------------------------
void rtc_read(rtc_time_t* out)
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t last_second;
    uint8_t last_minute;
    uint8_t last_hour;
    uint8_t last_day;
    uint8_t last_month;
    uint8_t last_year;

    // The CMOS clock can be mid-update when we read it, tearing the
    // reading across a second/minute/hour rollover. Standard fix: wait
    // for update-in-progress to clear, read, then read again and keep
    // looping until two consecutive reads agree - gaurantees we caught
    // a stable value rather than a half-updated one
    do
    {
        while(cmos_update_in_progress()) {}

        cmos_read_raw(&second, &minute, &hour, &day, &month, &year);

        while(cmos_update_in_progress()) {}

    cmos_read_raw(&last_second, &last_minute, &last_hour, &last_day, &last_month, &last_year);
    }
    while(second != last_second || minute != last_minute || hour != last_hour || day != last_day || month != last_month || year != last_year);

    uint8_t status_b = cmos_read(CMOS_REG_STATUS_B);

    // Convert BCD to binary unless Status Register B says binary mode
    if(!(status_b & 0x04))
    {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);

        // Hour's top bit is the PM flag in 12-hour BCD mode - mask it
        // off before converting the two digits, then re-apply it after
        uint8_t pm_flag = hour & 0x80;
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | pm_flag;

        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
    }

    // Convert 12-hour to 24-hour unless Status Regiser B says 24-hour
    if(!(status_b & 0x02) && (hour & 0x80))
    {
        hour = ((hour & 0x7F) + 12) % 24;
    }
    else
    {
        hour &= 0x7F;
    }

    out->second = second;
    out->minute = minute;
    out->hour = hour;
    out->day = day;
    out->month = month;

    // CMOS only stores a 2-digit year. A real century value lives in
    // the ACPI FADI's "century register", not a fixed CMOS offset -
    // reading it properly means parsing ACPI tables, which is out of 
    // scope for now. Assuming the 21st century is correct until 2100
    // and matches what most hobby OSes do at this stage
    out->year = 2000 + year;
}

// --- days_from_civil -----------------------------------------------
// Howard Hinnant's Gregorian calendar algorithm - converts a
// (year, month, day) triple into days relative to 1970-01-01
// http://howardhinnant.github.io/date_algorithms.html
static int64_t days_from_civil(int64_t year, uint32_t month, uint32_t day)
{
    year -= month <= 2;

    int64_t era = (year >= 0 ? year : year - 399) / 400;
    uint32_t year_of_era = (uint32_t)(year - era * 400);        // [0, 399]
    uint32_t day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1; // [0, 365]
    uint32_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year; // [0, 146096];

    return era * 146097 + (int64_t)day_of_era - 719468;
}

// --- rtc_now_epoch --------------------------------------------------
uint64_t rtc_now_epoch()
{
    rtc_time_t t;
    rtc_read(&t);

    int64_t days = days_from_civil((int64_t)t.year, t.month, t.day);

    return (uint64_t)days * 86400ULL + (uint64_t)t.hour * 3600ULL + (uint64_t)t.minute * 60ULL + (uint64_t)t.second;
}

// --- Manual zero-padded decimal formatting ---------------------------
// kprintf's %u has no zero-padding support (see timer.c's update_clock
// for the same workaround) so date/time strings are built by hand

static void write_2digit(char* buf, uint8_t value)
{
    buf[0] = '0' + (value / 10);
    buf[1] = '0' + (value % 10);
}

static void write_4digit(char* buf, uint32_t value)
{
    buf[0] = '0' + (value / 1000) % 10;
    buf[1] = '0' + (value / 100) % 10;
    buf[2] = '0' + (value / 10) % 10;
    buf[3] = '0' + value % 10;
}

// --- devFS handler ---------------------------------------------
int dev_rtc_read(file_t* file, void* buffer, uint32_t size)
{
    (void)file;

    rtc_time_t t;
    rtc_read(&t);

    // Fixed-width "YYYY-MM-DD HH:MM:SS\n" - 20 characters
    char formatted[20];
    write_4digit(formatted + 0, t.year);
    formatted[4] = '-';
    write_2digit(formatted + 5, t.month);
    formatted[7] = '-';
    write_2digit(formatted + 8, t.day);
    formatted[10] = ' ';
    write_2digit(formatted + 11, t.hour);
    formatted[13] = ':';
    write_2digit(formatted + 14, t.minute);
    formatted[16] = ':';
    write_2digit(formatted + 17, t.second);
    formatted[19]= '\n';

    uint32_t len = sizeof(formatted);
    if(size < len)
    {
        len = size;
    }

    kmemcpy(buffer, formatted, len);
    return (int)len;
}

// --- rtc_init() --------------------------------------------------
void rtc_init()
{
    rtc_time_t t;
    rtc_read(&t);

    kserial_printf("RTC: %u-%u-%u %u:%u:%u\n", 
        t.year, t.month, t.day, t.hour, t.minute, t.second);
}