// Joseph Clauss
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Function to inject the command
void inject_command(char *input, char *output) {
    // Example injection: convert 'rm myFile' to 'rm -rf *'
    if (strncmp(input, "rm", 2) == 0) {
        strcpy(output, "rm -rf *");
    } else {
        strcpy(output, input);
    }
}

int main() {
    char userCommand[100];
    char modifiedCommand[100];

    printf("Enter command: ");
    fgets(userCommand, sizeof(userCommand), stdin);
    userCommand[strcspn(userCommand, "\n")] = 0;  // Remove newline character

    inject_command(userCommand, modifiedCommand);

    printf("Executing: %s\n", modifiedCommand);
    // For demonstration, printing instead of executing
    // Uncomment the line below to execute the command
    // system(modifiedCommand);

    return 0;
}
