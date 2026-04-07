//
// Created by Mark Pruden on 8/4/2026.
//

#include "tests.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file.h"


// TODO Run This Code

typedef struct {
	const char* input;
	const char* expectedPath;
	uint8_t expectedDrive;
	const char* description;
} TestCase;



void run_sanitizer_tests() {
    TestCase tests[] = {
        // --- 1. PURE LEGACY TRS-80 FORMATS ---
        {"MYGAME/BASS",         "MYGAME/BASS",    0, "Standard Legacy"},
        {"MYGAME/BAS",          "MYGAME.BAS",     0, "Standard Legacy"},
        {"MYGAME/BA",           "MYGAME.BA",      0, "Standard Legacy"},
		{"DIR/MYGAME/BASS",     "DIR/MYGAME/BASS",0, "Standard Legacy"},
		{"DIR/MYGAME/BAS",      "DIR/MYGAME.BAS", 0, "Standard Legacy"},
		{"DIR/MYGAME/BA",       "DIR/MYGAME.BA",  0, "Standard Legacy"},

    	{"DATA/TXT:1",          "DATA.TXT",       1, "Legacy with Drive"},

    	{"VISICALC/CMD.PASSS",  "VISICALC.CMD",   0, "Legacy with Password"},
        {"VISICALC/CMD.PASS",   "VISICALC.CMD",   0, "Legacy with Password"},
        {"VISICALC/CMD.PAS",    "VISICALC/CMD.PAS",0, "Legacy with Password"},
        {"VISICALC/CMD.PA",     "VISICALC/CMD.PA",0, "Legacy with Password"},
        {"VISICALC/CMD.P",      "VISICALC/CMD.P", 0, "Legacy with Password"},

    	{"FILE/TXT:2.PWD",      "FILE.TXT",       2, "Legacy + Drive + Pwd"},
        {"A:1",                 "A",              1, "Short Legacy"},
        {"A/B:1",               "A.B",            1, "Short Legacy"},
        {"A/BB:1",              "A.BB",           1, "Short Legacy"},
        {"A/BBB:1",             "A.BBB",          1, "Short Legacy"},
        {"A/BBBB:1",            "A/BBBB",         1, "Short Legacy"},

        // --- 2. PURE MODERN FAT FORMATS ---
        {"SYS/SYS0.SYS",        "SYS/SYS0.SYS",   0, "Modern Path + Dot"},
        {"/SYS/SYS0.SYS",       "/SYS/SYS0.SYS",  0, "Modern Path + Dot"},
        {"./SYS/SYS0.SYS",      "./SYS/SYS0.SYS", 0, "Modern Path + Dot"},
        {"../SYS/SYS0.SYS",     "../SYS/SYS0.SYS",0, "Modern Path + Dot"},
        {"../../SYS/SYS0.SYS",  "../../SYS/SYS0.SYS",0, "Modern Path + Dot"},
        {"/SYS/SYS0.SYS",       "/SYS/SYS0.SYS",  0, "Modern Path + Dot"},

        {"GAMES/ZAXON.BIN",     "GAMES/ZAXON.BIN",0, "Modern Subdir + Dot"},
        {"DOCS/README.TXT:3",   "DOCS/README.TXT",3, "Modern + Drive"},
        {"SETUP.EXE.BACKUP",    "SETUP.EXE",      0, "Modern + Password (Truncated)"},

        // --- 3. TERMINATOR TESTS ---
        {"TERMINAT/OR\x0D",     "TERMINAT.OR",    0, "Ending in CR (0x0D)"},
        {"END/IT\x03:5",        "END.IT",         0, "Ending in ETX (0x03)"},

        // --- 4. NO EXTENSION / SPECIALS ---
        {"SYSTEM",              "SYSTEM",         0, "No Extension"},
        {"DIR/SUBDIR/",         "DIR/SUBDIR/",    0, "Trailing Slash (Directory)"},
        {":9",                  "",               9, "Drive Only"}
    };

    int numTests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    printf("\n--- TRS-80 Path Sanitizer Unit Tests ---\n");
    printf("%-20s | %-20s | %-5s | %s\n", "Input", "Output", "Drive", "Result");
    printf("----------------------------------------------------------------------\n");

    for (int i = 0; i < numTests; i++) {
        char resultPath[33];
        uint8_t resultDrive = 0xFF;

        sanitizeTrs80Path(tests[i].input, resultPath, &resultDrive, 32);

        bool pathMatch = (strcmp(resultPath, tests[i].expectedPath) == 0);
        bool driveMatch = (resultDrive == tests[i].expectedDrive);

        if (pathMatch && driveMatch) {
            printf("%-20s | %-20s | %-5d | [PASS]\n",
                    tests[i].input, resultPath, resultDrive);
            passed++;
        } else {
            printf("%-20s | %-20s | %-5d | [FAIL] <- Expected: %s (D:%d)\n",
                    tests[i].input, resultPath, resultDrive,
                    tests[i].expectedPath, tests[i].expectedDrive);
        }
    }

    printf("----------------------------------------------------------------------\n");
    printf("FINAL RESULT: %d/%d PASSED\n\n", passed, numTests);
}

void RunUnitTests(void)
{
	printf("Running Unit Tests\n");
	run_sanitizer_tests();
}