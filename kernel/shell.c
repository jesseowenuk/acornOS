#include "shell.h"
#include "vga.h"

// -- String helpers ----------------------------------------------------
// We have no standard library so we write our own minimal helpers

// Compare two strings - returns 1 if equal, 0 if not
static int streq(const char* a, const char* b)
{
    // Walk through both strings simultaneously
    while(*a && *b)
    {
        if(*a != *b)
        {
            // Mismatch found - not equal
            return 0;
        }

        a++;
        b++;
    }

    // Both must be at null terminator to be equal
    return *a == *b;
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
    vga_print("\nacorn> ");             // The shell prompt
    vga_set_colour(WHITE, BLACK);       // Input typed in white
}

static void print_prompt_first()
{
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("acorn> ");               // The shell prompt
    vga_set_colour(WHITE, BLACK);       // Input typed in white
}

// -- Commands ----------------------------------------------------

static void cmd_help()
{
    vga_set_colour(YELLOW, BLACK);
    vga_print("\nAvailable commands\n");
    vga_set_colour(WHITE, BLACK);
    vga_print("    help    -- show this message\n");
    vga_print("    clear   -- clear the screen\n");
    vga_print("    about   -- about acornOS\n");
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
    vga_print("\nacornOS v0.1\n");
    vga_set_colour(WHITE, BLACK);
    vga_print("A tiny OS built from scratch.\n");
    vga_print("Guided by AI, built by hand.\n");
}

// -- Command dispatch ----------------------------------------------------

// Called when the user presses enter
// Looks at the buffer and decides which command to run
static void process_command()
{
    // Move to a new line first
    vga_print("\n");

    if(streq(buffer, "help"))
    {
        cmd_help();
    }
    else if(streq(buffer, "clear"))
    {
        cmd_clear();
    }
    else if(streq(buffer, "about"))
    {
        cmd_about();
    }
    else if(buffer[0] == 0)
    {
        // User just pressed enter on empty line
        // So do nothing - just reprint the prompt
    }
    else
    {
        vga_set_colour(LIGHT_RED, BLACK);
        vga_print("Unknown command: ");
        vga_set_colour(WHITE, BLACK);
        vga_print(buffer);              // Echo back what they typed
        vga_print("\n");
        vga_set_colour(WHITE, BLACK);
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