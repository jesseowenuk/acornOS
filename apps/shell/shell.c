#include <drivers/timer.h>
#include <drivers/vga.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/string.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/processes/process.h>

#include "shell.h"

// --- Helpers -----------------------------------------------
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
static int skip_prompt = 0;  

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
    kprintf("    ls [path]    -- list directory contents\n");
    kprintf("    cat <path>   -- print file contents\n");
    kprintf("    mkdir <path> -- create a directory\n");
    kprintf("    rm <path>    -- delete a file\n");
}

static void cmd_clear()
{
    // Clear the VGA buffer entirely
    vga_clear(); 
    
    // Print prompt
    print_prompt();
    
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
    int fd = vfs_open("/process/meminfo", O_RDONLY);
    if(fd < 0)
    {
        kprintf("mem: cannot open /process/meminfo\n");
        return;
    }

    char buffer[256];
    int bytes = vfs_read(fd, buffer, 255);
    buffer[bytes] = 0;
    vfs_close(fd);

    kprintf("\n%s", buffer);
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
    // List all processes via procFS
    int fd = vfs_open("/process/", O_RDONLY);
    if(fd < 0)
    {
        kprintf("ps: cannot open /process\n");
        return;
    }

    dentry_t dentry;

    while(vfs_readdir(fd, &dentry) > 0)
    {
        // Skip non-PID entries
        if(dentry.type != VFS_TYPE_DIR)
        {
            continue;
        }

        // Open status file for this PID
        char path[64];
        ksnprintf(path, sizeof(path), "/process/%s/status", dentry.name);

        int sfd = vfs_open(path, O_RDONLY);
        if(sfd < 0)
        {
            continue;
        }

        char buffer[256];
        int bytes = vfs_read(sfd, buffer, 255);
        buffer[bytes] = 0;
        vfs_close(sfd);

        // Print the status
        vga_set_colour(CYAN, BLACK);
        kprintf("------------------\n");
        vga_set_colour(WHITE, BLACK);
        kprintf("%s\n", buffer);
    }

    vfs_close(fd);
}

static void cmd_ls(const char* path)
{
    // Default to /temp if no path given
    if(*path == ' ')
    {
        path++;
    }

    if(*path == 0)
    {
        path = "/temp";
    }

    int file_descriptor = vfs_open(path, O_RDONLY);
    if(file_descriptor < 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("ls: cannot open %s\n", path);
        vga_set_colour(WHITE, BLACK);
        return;
    }

    dentry_t dentry;
    int result;
    vga_set_colour(LIGHT_CYAN, BLACK);
    kprintf("\n");

    while((result = vfs_readdir(file_descriptor, &dentry)) > 0)
    {
        if(dentry.type == VFS_TYPE_DIR)
        {
            vga_set_colour(LIGHT_BLUE, BLACK);
            kprintf("  %s/\n", dentry.name);
        }
        else
        {
            vga_set_colour(WHITE, BLACK);
            kprintf("  %s\n", dentry.name);
        }
    }

    vga_set_colour(WHITE, BLACK);
    vfs_close(file_descriptor);
}

static void cmd_cat(const char* path)
{
    if(*path == ' ')
    {
        path++;
    }

    if(*path == 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("Usgae: cat <path>\n");
        vga_set_colour(WHITE, BLACK);
        return;
    }

    int file_descriptor = vfs_open(path, O_RDONLY);
    if(file_descriptor < 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("cat: cannot open %s\n", path);
        vga_set_colour(WHITE, BLACK);
        return;
    }

    char buffer[256];
    int bytes;
    kprintf("\n");

    while((bytes = vfs_read(file_descriptor, buffer, 255)) > 0)
    {
        buffer[bytes] = 0;
        kprintf("%s", buffer);
    }

    kprintf("\n");
    vfs_close(file_descriptor);
}

static void cmd_mkdir(const char* path)
{
    if(*path == ' ')
    {
        path++;
    }

    if(*path == 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("Usage: mkdir <path>\n");
        vga_set_colour(WHITE, BLACK);
        return;
    }

    int result = vfs_mkdir(path);
    if(result < 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("mkdir: failed to create %s\n", path);
        vga_set_colour(WHITE, BLACK);
    }
    else
    {
        kprintf("Created directory %s\n", path);
    }
}

static void cmd_rm(const char* path)
{
    if(*path == ' ')
    {
        path++;
    }

    if(*path == 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("Usage: rm <path>\n");
        vga_set_colour(WHITE, BLACK);
        return;
    }

    int result = vfs_delete(path);
    if(result < 0)
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("rm: failed to delete %s\n", path);
        vga_set_colour(WHITE, BLACK);
    }
    else
    {
        kprintf("Deleted %s\n", path);
    }
}

// -- Command dispatch ----------------------------------------------------

static command_t commands[] =
{
    {"help", cmd_help},
    {"clear", cmd_clear},
    {"about", cmd_about},
    {"uptime", cmd_uptime},
    {"mem", cmd_mem},
    {"ps", cmd_ps},
    {"echo", cmd_echo},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"mkdir", cmd_mkdir},
    {"rm", cmd_rm},
    {0, 0}                   // Sentinel
};

// Called when the user presses enter
// Looks at the buffer and decides which command to run
static void process_command()
{
    // Move to a new line first
    kprintf("\n");

    // Empty line- just reprint the prompt
    if(buffer[0] == 0)
    {
        return;
    }

    // Walk the command table looking for a match
    for(int i = 0; commands[i].name != 0; i++)
    {
        const char* args = starts_with(buffer, commands[i].name);
        if(args)
        {
            commands[i].handler(args);
            return;
        }
    }

    // Check for "echo " prefix - echo takes an argument
    const char* echo_text = starts_with(buffer, "echo");

    if(echo_text)
    {
        // Pass everything after "echo" to handler
        cmd_echo(echo_text);
    }
    else
    {
        vga_set_colour(LIGHT_RED, BLACK);
        kprintf("Unknown command: ");
        vga_set_colour(WHITE, BLACK);
        kprintf("%s\n", buffer);              // Echo back what they typed
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
    print_prompt();                     // Print prompt
}