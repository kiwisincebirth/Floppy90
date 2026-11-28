//
// Created by Mark Pruden on 8/4/2026.
//

#include "tests.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "file.h"
#include "file_cmnds.h"


typedef struct {
	const char* input;
	const char* expectedPath;
	uint8_t expectedDrive;
	const char* description;
} TestCase;


typedef struct {
	const char *srcSpec;
	const char *dstSpec;
	RenameMovePolicy policy;
	BYTE expectedErr;
	uint32_t expectedCount;
	const char *label;
} RenameMoveTestCase;

static void run_endpoint_tests(void) {
	struct {
		const char *input;
		BYTE expectedErr;
		bool expectedHasDrive;
		uint8_t expectedDriveId;
		bool expectedTrailingSep;
		bool expectedExplicitPathOrSep;
		const char *label;
	} cases[] = {
		{ "HELLO.BAS:1",      TRS_OK,                 true,  1, false, false, "Drive :1" },
		{ "DOCS/README.TXT:3", TRS_ILLEGAL_FILENAME,  true,  3, false, true,  "Drive :3 with slash rejected" },
		{ "/SYS/SYS0.SYS:2",   TRS_ILLEGAL_FILENAME,  true,  2, false, true,  "Leading slash + :2 rejected" },
		{ "./FILE.BAS:1",      TRS_ILLEGAL_FILENAME,  true,  1, false, true,  "Dot prefix + :1 rejected" },
		{ "FILE.BAS:0",        TRS_ILLEGAL_FILENAME,  false, 0, false, false, "Drive :0 rejected" },
		{ "*.BAS:1",           TRS_OK,                 true,  1, false, false, "Wildcard with :1" },
		{ "DIR/SUBDIR/",       TRS_OK,                 false, 0, true,  true,  "Trailing slash" },
		{ "END/IT\x03",        TRS_OK,                 false, 0, false, true,  "ETX terminator" },
	};

	int numCases = (int)(sizeof(cases) / sizeof(cases[0]));
	int passed = 0;

	printf("\n--- Endpoint Resolver Unit Tests ---\n");
	printf("%-20s | %-6s | %-5s | %-4s | %s\n", "Input", "Status", "Drive", "Sep", "Result");
	printf("----------------------------------------------------------------------\n");

	for (int i = 0; i < numCases; i++) {
		ResolvedEndpoint ep;
		BYTE err = resolve_spec_endpoint(cases[i].input, &ep);

		bool ok = (err == cases[i].expectedErr) &&
		          (ep.hasDrive == cases[i].expectedHasDrive) &&
		          (ep.driveId == cases[i].expectedDriveId) &&
		          (ep.trailingSep == cases[i].expectedTrailingSep) &&
		          (ep.hasExplicitPathOrSep == cases[i].expectedExplicitPathOrSep);

		if (ok) {
			printf("%-20s | %-6u | %-5u | %-4d | [PASS] %s\n",
			       cases[i].input, (unsigned)err, (unsigned)ep.driveId,
			       ep.trailingSep ? 1 : 0, cases[i].label);
			passed++;
		} else {
			printf("%-20s | %-6u | %-5u | %-4d | [FAIL] %s (exp err=%u d=%u)\n",
			       cases[i].input, (unsigned)err, (unsigned)ep.driveId,
			       ep.trailingSep ? 1 : 0, cases[i].label,
			       (unsigned)cases[i].expectedErr, (unsigned)cases[i].expectedDriveId);
		}
	}

	printf("----------------------------------------------------------------------\n");
	printf("ENDPOINT RESULT: %d/%d PASSED\n\n", passed, numCases);
}

void RunUnitTests(void)
{
	printf("Running Unit Tests\n");
	run_endpoint_tests();
}
