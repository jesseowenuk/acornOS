#include <drivers/random.h>
#include <drivers/timer.h>

// Check if RDRAND is supported via CPUID
// CPUID leaf 1, ECX bit 30 = RDRAND support
static int rdrand_supported()
{
    uint32_t ecx;
    __asm__ volatile(
        "cpuid\n\t"
        : "=c"(ecx)
        : "a"(1)
        : "ebx", "edx"
    );

    return (ecx >> 30) & 1;
}

// --- Simple LGC (Linear Congrugential Generator) -------------------------
// Not cryptographically secure - good enough for games and general use
// Seeded from timer ticks for different values each boot

static uint64_t seed = 0;

static uint64_t rand_next()
{
    seed ^= (uint64_t)timer_get_ticks();
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return seed;
}

// --- devFS handler -----------------------------------------------------------
int dev_random_read(file_t* file, void* buffer, uint32_t size)
{
    (void)file;

    uint8_t* dst = (uint8_t*)buffer;

    if(rdrand_supported())
    {
        for(uint32_t i = 0;  i < size; i += 8)
        {
            uint64_t val;
            __asm__ volatile(
                "rdrand %0\n\t"
                : "=r"(val)
                :
                :
            );

            // Copy up to 8 bytes from val
            uint32_t to_copy = (size - i < 8) ? size - i : 8;

            for(uint32_t j = 0; j < to_copy; j++)
            {
                dst[(i + j)] = (uint8_t)(val >> (j * 8));
            }
        }
    }
    else
    {
        // RDRAND not supported - return zeros
        // TODO: fallback enthropy source (RTC, keyboard timing)
        kserial_printf("random: RDRAND not supported!\n");
        for(uint32_t i = 0; i < size; i++)
        {
            dst[i] = 0;
        }
    }

    return (int)size;
}