#include "shell.h"
#include "vga.h"
#include "timer.h"      // For uptime command
#include "mem.h"        // For mem command
#include "pmm.h"
#include "process.h"
#include "kprintf.h"
#include "string.h"

// Check if a string starts with a given prefix
// Returns pointer to the character after the prefix, or 0 if no match
static const char* starts_with(const char* str, const char* prefix)
{
    // Walk the prefix
    while(*prefix)
    {
        if(*str != *prefix)
        {
            // Mismatch
            return 0;
        }

        str++;
        prefix++;
    }

    // Return pointer to the rest of string after prefix
    return str;
}

// -- Buffer ----------------------------------------------------

static char buffer[SHELL_BUFFER_SIZE];          // Stores the current line being typed
static int buf_pos = 0;                         // Current position in the buffer   

// Clear the input buffer and reset position
static void buffer_clear()
{
    for(int i = 0; i < SHELL_BUFFER_SIZE; i++)
    {
        // Zero out every byte
        buffer[i] = 0;
    }

    // Reset position to start
    buf_pos = 0;
}

// -- Prompt ----------------------------------------------------

static void print_prompt()
{
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("acorn> ");                 // The shell prompt
    vga_set_colour(WHITE, BLACK);       // Input typed in white
}

static void print_prompt_first()
{
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("acorn> ");               // The shell prompt
    vga_set_colour(WHITE, BLACK);       // Input typed in white
}

// -- Commands ----------------------------------------------------

static void cmd_help()
{
    vga_set_colour(YELLOW, BLACK);
    kprintf("\nAvailable commands\n");
    vga_set_colour(WHITE, BLACK);
    kprintf("    help         -- show this message\n");
    kprintf("    clear        -- clear the screen\n");
    kprintf("    about        -- about acornOS\n");
    kprintf("    uptime       -- show time since boot\n");
    kprintf("    mem          -- show memory usage\n");
    kprintf("    ps           -- list all processes\n");
    kprintf("    echo <text>  -- print text screen\n");
}

static int skip_prompt = 0;         // Flag to suppress prompt after command

static void cmd_clear()
{
    // Clear the VGA buffer entirely
    vga_clear(); 
    
    // Print prompt without leading newline
    print_prompt_first();
    
    // Clear the input buffer too
    buffer_clear();

    // Tell process_command not to reprint it
    skip_prompt = 1;
}

static void cmd_about()
{
    vga_set_colour(LIGHT_CYAN, BLACK);
    kprintf("\nacornOS v0.1\n");
    vga_set_colour(WHITE, BLACK);
    kprintf("A tiny OS built from scratch.\n");
    kprintf("Guided by AI, built by hand.\n");
    kprintf("github.com/jesseowenuk/acornOS\n");
}

static void cmd_uptime()
{
    uint32_t seconds = timer_get_seconds();         // Get seconds since boot from timer
    uint32_t minutes = seconds / 60;                // Convert to minutes
    uint32_t hours = minutes / 60;                  // Convert to hours

    seconds %= 60;                                  // Remainder in seconds
    minutes %= 60;                                  // Remainder in minutes

    vga_set_colour(WHITE, BLACK);
    kprintf("\nUptime: %uh %um %us\n", hours, minutes, seconds);
}

static void cmd_mem()
{
    // Heap stats from mem.c
    mem_print_stats();

    // Physical memory stats from pmm.c
    pmm_print_stats();
}

static void cmd_echo(const char* text)
{
    if(*text == ' ')
    {
        // Skip the space after "echo"
        text++;
    }

    // Nothing after echo
    if(*text == 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("Usage: echo <text>\n");
    }
    else
    {
        // Print everything after "echo "
        vga_set_colour(WHITE, BLACK);

        // Print the text
        kprintf("%s\n", text);
    }
}

static void cmd_ps()
{
    // Delegate to process subsystem
    process_print_all();
}

// -- Command dispatch ----------------------------------------------------

// Called when the user presses enter
// Looks at the buffer and decides which command to run
static void process_command()
{
    // Move to a new line first
    kprintf("\n");

    if(kstreq(buffer, "help"))
    {
        cmd_help();
    }
    else if(kstreq(buffer, "clear"))
    {
        cmd_clear();
    }
    else if(kstreq(buffer, "about"))
    {
        cmd_about();
    }
    else if(kstreq(buffer, "uptime"))
    {
        // Show uptime since boot
        cmd_uptime();
    }
    else if(kstreq(buffer, "mem"))
    {
        // Show memory stats
        cmd_mem();
    }
    else if(kstreq(buffer, "ps"))
    {
        cmd_ps();
    }
    else
    {
        // Check for "echo " prefix - echo takes an argument
        const char* echo_text = starts_with(buffer, "echo");

        if(echo_text)
        {
            // Pass everything after "echo" to handler
            cmd_echo(echo_text);
        }
        else if(buffer[0] == 0)
        {
            // Empty line - just re-print the prompt
        }
        else
        {
            vga_set_colour(LIGHT_RED, BLACK);
            kprintf("Unknown command: ");
            vga_set_colour(WHITE, BLACK);
            kprintf("%s\n", buffer);              // Echo back what they typed
        }
    }
}

// -- Key handler ----------------------------------------------------

// Called by keyboard.c for every printable keypress
void shell_handle_key(char c)
{
    // Reset flag before processing
    skip_prompt = 0;

    // Enter key - process the command
    if(c == '\n')
    {
        process_command();
        buffer_clear();                 // Clear buffer for next command

        if(!skip_prompt)
        {
            print_prompt();            // Print a fresh prompt if not already done so
        }
    }
    else if(c == '\b')
    {
        // Backspace key
        if(buf_pos > 0)
        {
            // Only if there's something to delete
            buf_pos--;                  // Move buffer position back one
            buffer[buf_pos] = 0;        // Erase the character from buffer
            vga_putchar('\b');          // Move VGA cursor back one
            vga_putchar(' ');           // Overwrite character with a space
            vga_putchar('\b');          // Move cursor back again
        }
    }
    else
    {
        // Don't overflow the buffer
        if(buf_pos < SHELL_BUFFER_SIZE - 1)
        {
            buffer[buf_pos] = c;        // Store character in buffer
            buf_pos++;                  // Advance buffer position
            vga_putchar(c);             // Echo character to screen
        }
    }
}

// -- Init ----------------------------------------------------

void shell_init()
{
    buffer_clear();                     // Make sure buffer starts empty
    print_prompt_first();               // Print the first prompt
}