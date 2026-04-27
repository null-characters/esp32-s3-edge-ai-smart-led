/*
 * PIR Mock for Unit Testing
 * Provides stub implementations for pir functions
 */

#include <stdbool.h>
#include "pir.h"

/* Mock state */
static bool mock_presence = false;
static bool mock_initialized = false;

/* ================================================================
 * Mock implementations
 * ================================================================ */
int pir_init(void)
{
	mock_initialized = true;
	return 0;
}

bool pir_get_status(void)
{
	return mock_presence;
}

bool pir_get_raw(void)
{
	return mock_presence;
}

/* ================================================================
 * Mock control functions (for test setup)
 * ================================================================ */
void mock_pir_set_presence(bool presence)
{
	mock_presence = presence;
}

void mock_pir_reset(void)
{
	mock_presence = false;
	mock_initialized = false;
}
