#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int check_input(char *input);
bool check_palindrom(char *input);

int main() {

    char palindrom[] = "8?48";

    bool isPal = check_palindrom(palindrom);

    if (isPal) {
        printf("palindrom!");
    } else {
        printf("nein");
    }

    return 0;
}

bool check_palindrom(char *input) {
    int left = 0;
    int right = 0;
    while (input[right] != '\0') {
        right++;
    }
    right = right - 1;

    while (left < right) {
        // Skip spaces
        if (input[left] == ' ') {
            left++;
            continue;
        }
        if (input[right] == ' ') {
            right--;
            continue;
        }

        // Convert characters to lower case for case-insensitive comparison
        char leftChar = tolower(input[left]);
        char rightChar = tolower(input[right]);

        // Handle special character '?'
        if (leftChar != '?' && rightChar != '?') {
            if (leftChar != rightChar) {
                return false;
            }
        }

        left++;
        right--;
    }

    return true;
}