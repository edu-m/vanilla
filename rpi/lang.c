#include "lang.h"

static const char *lang_str[__VPI_LANG_T_COUNT] = {
    "Sync",
    "Connect",
    "Edit",
    "Settings",
    "OK",
    "Cancel",
    "Touch the symbols in the order they are displayed on the TV screen from left to right.",
    "If the symbols are not displayed on the TV screen, press the SYNC Button on the Wii U console.",
    "Connecting to the Wii U console...",
    "Sync Failed",
    "Vanilla needs root permission to configure the wireless interface for connection.",
    "Please enter your \"sudo\" password here.",
    "Error",
    "No consoles synced",
    "Connecting to \"%s\"...",
    "Are you sure you want to delete \"%s\"?",
    "Successfully deleted \"%s\"",
    "Rename",
    "Delete",
    "Back",
    "More",
    "Gamepad",
    "Audio",
    "Connection",
    "Webcam",
    "Region",
    "Local",
    "Via Server",
    "Set up how Vanilla connects to the Wii U",
    "Choose 'Local' to connect directly with the hardware on your system. Otherwise, you may need a compatible 'server' to relay the necessary packets.",
    "Select the wireless interface to use for the connection.",
    "Enter the IP address of the server you wish to connect through.",
    "Screenshot saved to \"%s\"",
    "Recording started to \"%s\"",
    "Recording finished",
    "Connection lost, attempting to re-connect...",
    "Your platform is not capable of \"Local\" connection, and must connect via a server.",
    "Failed to save screenshot (%i)",
    "Select gamepad region",
    "Gamepad region must match the console region or else the console will throw an error. This has no other effect on usage.",
    "Japan",
    "America",
    "Europe",
    "Quit",
};

const char *lang(vpi_lang_t id)
{
    return lang_str[id];
}