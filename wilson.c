#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define WORD_LENGTH 5
#define MAX_ATTEMPTS 6
#define SAMPLE_SIZE 100
#define MAX_WORDS 20000
#define OPTIMAL_FIRST_COUNT 4

// ANSI Escape Codes for colors
#define COLOR_RESET   "\x1b[0m"
#define COLOR_GRAY    "\x1b[90m"
#define COLOR_YELLOW  "\x1b[93m"
#define COLOR_GREEN   "\x1b[92m"

typedef struct {
    char** words;
    int count;
} WordList;

const char* optimal_first[] = { "salet", "crate", "raise", "roate" };

// Windows terminal control functions
#ifdef _WIN32
HANDLE hStdin;
DWORD fdwSaveOldMode;

void enable_raw_mode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwSaveOldMode);
    DWORD mode = fdwSaveOldMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(hStdin, mode);
}

void disable_raw_mode() {
    SetConsoleMode(hStdin, fdwSaveOldMode);
}

#else
// Unix terminal control functions
void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}
#endif

// Simplified display functions
void print_colored_letter(char c, int feedback) {
    const char* color = COLOR_RESET;
    switch (feedback) {
    case 0: color = COLOR_GRAY; break;
    case 1: color = COLOR_YELLOW; break;
    case 2: color = COLOR_GREEN; break;
    }
    printf("%s%c%s ", color, toupper(c), COLOR_RESET);
}

void print_guess(const char* guess, const int* feedback) {
    for (int i = 0; i < WORD_LENGTH; i++) {
        if (feedback[i] >= 0) {
            print_colored_letter(guess[i], feedback[i]);
        }
        else {
            printf("%c ", toupper(guess[i]));
        }
    }
    printf("\n");
}

void print_input_boxes(const int* feedback) {
    printf("Enter feedback (0-2): ");
    for (int i = 0; i < WORD_LENGTH; i++) {
        if (feedback[i] >= 0) {
            printf("[%d]", feedback[i]);
        }
        else {
            printf("[ ]");
        }
    }
    printf("\r");
    fflush(stdout);
}

// Enhanced input handling with Windows support
int get_feedback(int feedback[], const char* guess) {
    memset(feedback, -1, WORD_LENGTH * sizeof(int));
    int position = 0;

    printf("\nCurrent guess: ");
    print_guess(guess, feedback);
    printf("(0=gray, 1=yellow, 2=green)\n");
    print_input_boxes(feedback);

    enable_raw_mode();

    while (1) {
#ifdef _WIN32
        INPUT_RECORD irInBuf[128];
        DWORD cNumRead;

        if (!ReadConsoleInput(hStdin, irInBuf, 128, &cNumRead)) continue;

        for (DWORD i = 0; i < cNumRead; i++) {
            if (irInBuf[i].EventType == KEY_EVENT && irInBuf[i].Event.KeyEvent.bKeyDown) {
                KEY_EVENT_RECORD ker = irInBuf[i].Event.KeyEvent;

                if (ker.wVirtualKeyCode == VK_LEFT) {
                    position = position > 0 ? position - 1 : 0;
                }
                else if (ker.wVirtualKeyCode == VK_RIGHT) {
                    position = position < WORD_LENGTH - 1 ? position + 1 : WORD_LENGTH - 1;
                }
                else if (ker.uChar.AsciiChar >= '0' && ker.uChar.AsciiChar <= '2') {
                    feedback[position] = ker.uChar.AsciiChar - '0';
                    position = position < WORD_LENGTH - 1 ? position + 1 : WORD_LENGTH - 1;
                }
                else if (ker.wVirtualKeyCode == VK_BACK) {
                    feedback[position] = -1;
                    position = position > 0 ? position - 1 : 0;
                }
                else if (ker.wVirtualKeyCode == VK_RETURN) {
#else
        int c = getchar();

        if (c == 27) { // Handle arrow keys
            getchar(); // Skip [
            switch (getchar()) {
            case 'D': position = position > 0 ? position - 1 : 0; break;
            case 'C': position = position < WORD_LENGTH - 1 ? position + 1 : WORD_LENGTH - 1; break;
            }
        }
        else if (c >= '0' && c <= '2') {
            feedback[position] = c - '0';
            position = position < WORD_LENGTH - 1 ? position + 1 : WORD_LENGTH - 1;
        }
        else if (c == 127 || c == 8) { // Backspace/Delete
            feedback[position] = -1;
            position = position > 0 ? position - 1 : 0;
        }
        else if (c == '\n') {
#endif
            int complete = 1;
            for (int i = 0; i < WORD_LENGTH; i++) {
                if (feedback[i] < 0 || feedback[i] > 2) {
                    complete = 0;
                    break;
                }
            }
            if (complete) {
                disable_raw_mode();
                return 1;
            }
        }
#ifdef _WIN32
                }
            }
#endif

// Update display
printf("\x1b[2K"); // Clear line
print_input_boxes(feedback);
        }
    }

    WordList read_words(const char* filename) {
        FILE* file = fopen(filename, "r");
        WordList list = { malloc(MAX_WORDS * sizeof(char*)), 0 };

        if (!file) {
            fprintf(stderr, "Error opening file: %s\n", filename);
            return list;
        }

        char line[32];
        while (fgets(line, sizeof(line), file) != NULL) {
            line[strcspn(line, "\n")] = '\0';
            for (int i = 0; line[i]; i++) {
                line[i] = tolower(line[i]);
            }

            if (strlen(line) != WORD_LENGTH) continue;

            int valid = 1;
            for (int i = 0; i < WORD_LENGTH; i++) {
                if (!isalpha(line[i])) {
                    valid = 0;
                    break;
                }
            }

            if (valid) {
                list.words[list.count] = _strdup(line);
                list.count++;
                if (list.count >= MAX_WORDS) break;
            }
        }

        fclose(file);
        return list;
    }

    void free_words(WordList * list) {
        for (int i = 0; i < list->count; i++) {
            free(list->words[i]);
        }
        free(list->words);
        list->count = 0;
    }

    void compute_feedback(const char* guess, const char* target, int* feedback) {
        int count[26] = { 0 };
        int temp_fb[WORD_LENGTH] = { 0 };

        for (int i = 0; i < WORD_LENGTH; i++) {
            count[target[i] - 'a']++;
        }

        for (int i = 0; i < WORD_LENGTH; i++) {
            if (guess[i] == target[i]) {
                temp_fb[i] = 2;
                count[guess[i] - 'a']--;
            }
        }

        for (int i = 0; i < WORD_LENGTH; i++) {
            if (temp_fb[i] != 2 && count[guess[i] - 'a'] > 0) {
                temp_fb[i] = 1;
                count[guess[i] - 'a']--;
            }
        }

        memcpy(feedback, temp_fb, WORD_LENGTH * sizeof(int));
    }

    WordList filter_words(const WordList * list, const char* guess, const int* feedback) {
        WordList filtered = { malloc(list->count * sizeof(char*)), 0 };
        int expected_fb[WORD_LENGTH];

        for (int i = 0; i < list->count; i++) {
            compute_feedback(guess, list->words[i], expected_fb);
            if (memcmp(feedback, expected_fb, WORD_LENGTH * sizeof(int)) == 0) {
                filtered.words[filtered.count++] = list->words[i];
            }
        }

        return filtered;
    }

    char* select_guess(const WordList * list, int attempt, int* first_used) {
        if (list->count == 0) return NULL;

        if (!*first_used) {
            for (int i = 0; i < OPTIMAL_FIRST_COUNT; i++) {
                for (int j = 0; j < list->count; j++) {
                    if (strcmp(optimal_first[i], list->words[j]) == 0) {
                        *first_used = 1;
                        return list->words[j];
                    }
                }
            }
        }

        if (list->count > 100) {
            double max_entropy = -1;
            char* best = NULL;

            for (int s = 0; s < SAMPLE_SIZE; s++) {
                int idx = rand() % list->count;
                const char* candidate = list->words[idx];
                int pattern_counts[243] = { 0 };

                for (int i = 0; i < list->count; i++) {
                    int fb[WORD_LENGTH];
                    compute_feedback(candidate, list->words[i], fb);
                    int pattern = 0;
                    for (int j = 0; j < WORD_LENGTH; j++) {
                        pattern = pattern * 3 + fb[j];
                    }
                    pattern_counts[pattern]++;
                }

                double entropy = 0;
                for (int i = 0; i < 243; i++) {
                    if (pattern_counts[i] > 0) {
                        double p = (double)pattern_counts[i] / list->count;
                        entropy -= p * log2(p);
                    }
                }

                if (entropy > max_entropy) {
                    max_entropy = entropy;
                    best = list->words[idx];
                }
            }
            return best ? best : list->words[0];
        }

        int freq[WORD_LENGTH][26] = { 0 };
        for (int i = 0; i < list->count; i++) {
            for (int j = 0; j < WORD_LENGTH; j++) {
                freq[j][list->words[i][j] - 'a']++;
            }
        }

        int max_score = -1;
        char* best = NULL;
        for (int i = 0; i < list->count; i++) {
            int score = 0;
            for (int j = 0; j < WORD_LENGTH; j++) {
                score += freq[j][list->words[i][j] - 'a'];
            }
            if (score > max_score) {
                max_score = score;
                best = list->words[i];
            }
        }
        return best;
    }

    int main() {
#ifdef _WIN32
        // Enable ANSI escape codes on Windows
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
#endif

        srand(time(NULL));
        WordList all_words = read_words("words.txt");
        if (all_words.count == 0) {
            fprintf(stderr, "Error: No valid words found in words.txt\n");
            return 1;
        }

        WordList remaining = { malloc(MAX_WORDS * sizeof(char*)), all_words.count };
        memcpy(remaining.words, all_words.words, all_words.count * sizeof(char*));

        int attempt = 0;
        int first_used = 0;

        printf("\nWordle Solver\n");
        printf("-------------\n");

        while (attempt < MAX_ATTEMPTS) {
            printf("\nAttempt %d/%d\n", attempt + 1, MAX_ATTEMPTS);
            char* guess = select_guess(&remaining, attempt, &first_used);
            if (!guess) {
                printf("No possible words remaining!\n");
                break;
            }

            printf("Suggested guess: ");
            print_guess(guess, (int[]) { -1, -1, -1, -1, -1 });

            int feedback[WORD_LENGTH];
            while (!get_feedback(feedback, guess));

            printf("\nFeedback: ");
            print_guess(guess, feedback);
            printf("Codes:    %d %d %d %d %d\n\n",
                feedback[0], feedback[1], feedback[2],
                feedback[3], feedback[4]);

            WordList filtered = filter_words(&remaining, guess, feedback);
            free(remaining.words);
            remaining = filtered;

            if (remaining.count == 0) {
                printf("No more possible words. Game over.\n");
                break;
            }

            int solved = 1;
            for (int i = 0; i < WORD_LENGTH; i++) {
                if (feedback[i] != 2) {
                    solved = 0;
                    break;
                }
            }

            if (solved) {
                printf("\x1b[1;32mSolved in %d attempts!\x1b[0m\n", attempt + 1);
                break;
            }

            printf("Remaining possible words: %d\n", remaining.count);
            if (remaining.count <= 20) {
                printf("Possible solutions:");
                for (int i = 0; i < remaining.count; i++) {
                    if (i % 8 == 0) printf("\n");
                    printf("%s ", remaining.words[i]);
                }
                printf("\n");
            }

            attempt++;
        }

        free_words(&all_words);
        free(remaining.words);
        return 0;
    }
