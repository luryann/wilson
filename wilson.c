#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define WORD_LENGTH 5
#define MAX_WORDS 12972
#define ALPHABET 26
#define MAX_PATTERNS 243  // 3^5
#define SAMPLE_SIZE 100   // For entropy sampling

typedef struct {
    unsigned short position[WORD_LENGTH][ALPHABET];
} FrequencyMap;

typedef struct {
    char word[WORD_LENGTH + 1];
    float entropy;
} EntropyCache;

// Precomputed optimal first guesses with their entropy values
const char* optimal_guesses[] = { "salet", "crate", "raise", "roate" };
EntropyCache first_guess_cache[4];
bool cache_initialized = false;

// Custom strdup implementation
char* my_strdup(const char* s) {
    size_t size = strlen(s) + 1;
    char* p = malloc(size);
    if (p) memcpy(p, s, size);
    return p;
}

// Compute feedback pattern
void compute_feedback(const char* guess, const char* target, int* feedback) {
    int count[ALPHABET] = { 0 };
    memset(feedback, 0, WORD_LENGTH * sizeof(int));

    for (int i = 0; i < WORD_LENGTH; i++) {
        count[target[i] - 'a']++;
    }

    for (int i = 0; i < WORD_LENGTH; i++) {
        if (guess[i] == target[i]) {
            feedback[i] = 2;
            count[guess[i] - 'a']--;
        }
    }

    for (int i = 0; i < WORD_LENGTH; i++) {
        if (feedback[i] != 2 && count[guess[i] - 'a'] > 0) {
            feedback[i] = 1;
            count[guess[i] - 'a']--;
        }
    }
}

// Calculate entropy for a candidate word
float calculate_entropy(char** words, int count, const char* candidate) {
    int pattern_counts[MAX_PATTERNS] = { 0 };

    for (int i = 0; i < count; i++) {
        int feedback[WORD_LENGTH];
        compute_feedback(candidate, words[i], feedback);

        int pattern = 0;
        for (int j = 0; j < WORD_LENGTH; j++) {
            pattern = pattern * 3 + feedback[j];
        }
        pattern_counts[pattern]++;
    }

    float entropy = 0.0;
    for (int i = 0; i < MAX_PATTERNS; i++) {
        if (pattern_counts[i] > 0) {
            float prob = (float)pattern_counts[i] / count;
            // Fix for line 80: Explicit cast from double to float
            entropy -= prob * (float)log2(prob);
        }
    }
    return entropy;
}

// Precompute entropy for optimal first guesses
void precompute_first_guesses(char** words, int total_words) {
    for (int i = 0; i < 4; i++) {
        // Fix for line 89: Using strncpy_s instead of strncpy
        strncpy_s(first_guess_cache[i].word, sizeof(first_guess_cache[i].word), optimal_guesses[i], WORD_LENGTH);
        first_guess_cache[i].entropy = calculate_entropy(words, total_words, optimal_guesses[i]);
    }
    cache_initialized = true;
}

// Select the best first guess
const char* get_optimal_first_guess() {
    float max_entropy = -1.0;
    int best_index = 0;

    for (int i = 0; i < 4; i++) {
        if (first_guess_cache[i].entropy > max_entropy) {
            max_entropy = first_guess_cache[i].entropy;
            best_index = i;
        }
    }
    return first_guess_cache[best_index].word;
}

// Select guess using entropy sampling
const char* select_entropy_guess(char** words, int count) {
    float max_entropy = -1.0;
    const char* best_word = words[0];

    for (int i = 0; i < SAMPLE_SIZE; i++) {
        int index = rand() % count;
        float entropy = calculate_entropy(words, count, words[index]);
        if (entropy > max_entropy) {
            max_entropy = entropy;
            best_word = words[index];
        }
    }
    return best_word;
}

// Calculate letter frequencies
void calculate_frequencies(char** words, int count, FrequencyMap* freq) {
    memset(freq, 0, sizeof(FrequencyMap));
    for (int i = 0; i < count; i++) {
        const char* word = words[i];
        for (int pos = 0; pos < WORD_LENGTH; pos++) {
            freq->position[pos][word[pos] - 'a']++;
        }
    }
}

// Score word based on letter frequencies
int word_score(const char* word, const FrequencyMap* freq) {
    int score = 0;
    for (int pos = 0; pos < WORD_LENGTH; pos++) {
        score += freq->position[pos][word[pos] - 'a'];
    }
    return score;
}

// Main guess selection logic
const char* select_guess(char** words, int count) {
    static bool first_guess = true;

    if (first_guess && cache_initialized) {
        first_guess = false;
        return get_optimal_first_guess();
    }

    if (count > 100) {
        return select_entropy_guess(words, count);
    }

    FrequencyMap freq;
    calculate_frequencies(words, count, &freq);

    int max_score = -1;
    const char* best_word = words[0];
    for (int i = 0; i < count; i++) {
        int score = word_score(words[i], &freq);
        if (score > max_score) {
            max_score = score;
            best_word = words[i];
        }
    }
    return best_word;
}

// Main function
int main() {
    // Fix for line 175: Explicit cast from time_t to unsigned int
    srand((unsigned int)time(NULL));
    FILE* file = fopen("words.txt", "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open words.txt\n");
        return 1;
    }

    char** words = malloc(MAX_WORDS * sizeof(char*));
    if (!words) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return 1;
    }

    int word_count = 0;
    char buffer[WORD_LENGTH + 2];

    while (fgets(buffer, sizeof(buffer), file) && word_count < MAX_WORDS) {
        buffer[WORD_LENGTH] = '\0';
        for (int i = 0; i < WORD_LENGTH; i++) {
            if (!islower(buffer[i])) {
                buffer[i] = tolower(buffer[i]);
            }
        }
        words[word_count] = my_strdup(buffer);
        if (!words[word_count]) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }
        word_count++;
    }
    fclose(file);

    if (word_count == 0) {
        fprintf(stderr, "No valid words loaded\n");
        free(words);
        return 1;
    }

    precompute_first_guesses(words, word_count);

    int remaining = word_count;
    int attempts = 0;
    char feedback_str[6];
    int feedback[WORD_LENGTH];

    while (attempts < 6 && remaining > 0) {
        const char* guess = select_guess(words, remaining);
        printf("Suggested guess: %s\n", guess);
        printf("Enter feedback (5 digits, 0=gray, 1=yellow, 2=green): ");

        if (scanf_s("%5s", feedback_str, (unsigned)_countof(feedback_str)) != 1) {
            fprintf(stderr, "Error reading input\n");
            break;
        }

        bool valid = true;
        bool solved = true;
        for (int i = 0; i < WORD_LENGTH; i++) {
            if (feedback_str[i] < '0' || feedback_str[i] > '2') {
                valid = false;
            }
            feedback[i] = feedback_str[i] - '0';
            if (feedback[i] != 2) solved = false;
        }

        if (!valid) {
            printf("Invalid feedback. Use digits 0-2.\n");
            continue;
        }

        if (solved) {
            printf("Solved in %d attempts!\n", attempts + 1);
            break;
        }

        // Filter words based on feedback
        int new_count = 0;
        for (int i = 0; i < remaining; i++) {
            int temp_feedback[WORD_LENGTH];
            compute_feedback(guess, words[i], temp_feedback);
            if (memcmp(feedback, temp_feedback, sizeof(feedback)) == 0) {
                words[new_count++] = words[i];
            }
        }
        remaining = new_count;
        printf("Remaining possible words: %d\n", remaining);
        attempts++;
    }

    if (remaining > 1 && attempts == 6) {
        printf("Possible solutions:\n");
        for (int i = 0; i < remaining && i < 10; i++) {
            printf("  %s\n", words[i]);
        }
        if (remaining > 10) printf("... and %d more\n", remaining - 10);
    }

    for (int i = 0; i < word_count; i++) {
        free(words[i]);
    }
    free(words);

    return 0;
}
