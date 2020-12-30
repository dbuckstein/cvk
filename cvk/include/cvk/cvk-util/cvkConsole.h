/*
   Copyright 2020 Daniel S. Buckstein

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	cvk: Vulkan renderer from scratch in C.
	By Daniel S. Buckstein

	cvkConsole.h
	Console utility header.
*/

#ifndef _CVK_CONSOLE_H_
#define _CVK_CONSOLE_H_

#include "cvk/cvk/cvk-types.h"

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus


//-----------------------------------------------------------------------------

// eprintf
//	Shorthand macro for outputting formatted string to standard error.
#define eprintf(format, ...)	fprintf(stderr, format, __VA_ARGS__)

// dprintf
//	Shorthand macro for outputting formatted string to debugging interface.
#define dprintf(format, ...)	cvkConsolePrintDebug(format, __VA_ARGS__)


//-----------------------------------------------------------------------------

// cvkConsole
//	Console organizational structure.
//		member handle: array of internal stream handles
//		member io: array of internal stream data
typedef struct cvkTag_cvkConsole
{
	ptr handle[4];		// Internal stream handles.
	int io[3];			// Internal stream data.
} cvkConsole;


//-----------------------------------------------------------------------------

// cvkConsoleCreateMain
//	Create and initialize console instance for the main process; redirects 
//	standard input and output to new console (excludes standard error).
//		return SUCCESS: 0 if console successfully initialized
//		return FAILURE: -1 if invalid parameters
//		return FAILURE: -2 if console not initialized
//		return WARNING: +1 if console already exists
int cvkConsoleCreateMain(cvkConsole* const console);

// cvkConsoleReleaseMain
//	Terminate and release console instance for the main process.
//		param console: pointer to descriptor that stores console info
//			valid: non-null
//		return SUCCESS: 0 if console successfully terminated
//		return FAILURE: -1 if invalid parameters
//		return FAILURE: -2 if console not terminated
//		return WARNING: +1 if console does not exist
int cvkConsoleReleaseMain(cvkConsole* const console);

// cvkConsolePrintDebug
//	Print formatted string to debugging interface.
//		param format: format string, as used with standard 'printf'
//			valid: non-null c-string
//		params ...: parameter list matching specifications in 'format'
//		return SUCCESS: result of internal print operation if succeeded
//		return FAILURE: -1 if invalid parameters
int cvkConsolePrintDebug(kstr const format, ...);


//-----------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif	// __cplusplus


#endif	// !_CVK_CONSOLE_H_