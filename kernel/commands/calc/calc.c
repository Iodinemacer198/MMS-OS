#include <stdint.h>
#include "calc.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern int atoi(const char* str);
extern char get_key();
extern bool isdigit(char c);
extern void putchar(char c);

extern int cursorX;

#define CALC_BUFFER 128
char calc_buffer[CALC_BUFFER];
int calc_index = 0;

#define CALC_BUFFER2 128
char calc_buffer2[CALC_BUFFER2];
int calc_index2 = 0;

#define CALC_BUFFER3 128
char calc_buffer3[CALC_BUFFER3];
int calc_index3 = 0;

void run_calc() {
    println("=== Calculator ===");
    print("First number: ");
    bool running = true;
    int first_number = 0;
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isdigit(key)) {
            putchar(key);
            calc_buffer[calc_index] = key;
            calc_index++;
        }
        else if (key == 8) {
            if (calc_index > 0) {
                calc_index--;
                calc_buffer[calc_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            first_number = atoi(calc_buffer);
            running = false;
        }
        else {
            continue;
        }
    }
    putchar('\n');
    print("Second number: "); 
    bool running2 = true;
    int second_number = 0;
    while (running2) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isdigit(key)) {
            putchar(key);
            calc_buffer2[calc_index2] = key;
            calc_index2++;
        }
        else if (key == '\n') {
            second_number = atoi(calc_buffer2);
            running2 = false;
        }
        else if (key == 8) {
            if (calc_index2 > 0) {
                calc_index2--;
                calc_buffer2[calc_index2] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else {
            continue;
        }
    }
    putchar('\n');
    println("Operation (type number):");
    println("1. +");
    println("2. -");
    println("3. *");
    println("4. /");
    print("Operation > ");
    bool running3 = true;
    int output = 0;
    while (running3) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isdigit(key)) {
            if (calc_index3 == 0)
            {
                putchar(key);
                calc_buffer3[calc_index3] = key;
                calc_index3++;
            }
            else
            {
                continue;
            }
        }
        else if (key == 8) {
            if (calc_index3 > 0) {
                calc_index3--;
                calc_buffer3[calc_index3] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            if (calc_buffer3[0] == '1') {
                output = first_number + second_number;
                running3 = false;
            }
            else if (calc_buffer3[0] == '2') {
                output = first_number - second_number;
                running3 = false;
            }
            else if (calc_buffer3[0] == '3') {
                output = first_number * second_number;
                running3 = false;
            }
            else if (calc_buffer3[0] == '4') {
                if (second_number == 0) {
                    putchar('\n');
                    println("Divide by zero error!");
                    running3 = false;
                }
                else {
                    output = first_number / second_number;
                    running3 = false;
                }
            }
            else {
                putchar('\n');
                println("Invalid operation!");
                running3 = false;
            }
            running3 = false;
        }
        else {
            continue;
        }
    }
    putchar('\n');
    print("Answer: ");
    printint(output); 
    calc_index = 0;
    calc_index2 = 0;
    calc_index3 = 0;
    output = 0;
    putchar('\n');
}
