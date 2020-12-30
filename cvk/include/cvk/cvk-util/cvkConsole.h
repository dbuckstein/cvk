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

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus


//-----------------------------------------------------------------------------

// eprintf
//	Shorthand macro for outputting formatted string to standard error.
#define eprintf(fmt, ...)		fprintf(stderr, fmt, __VA_ARGS__)

// dprintf
//	Shorthand macro for outputting formatted string to debugging interface.
#define dprintf(fmt, ...)		cvkConsolePrintDebug(fmt, __VA_ARGS__)


//-----------------------------------------------------------------------------

// cvkConsole
//	Console organizational structure.
typedef struct cvkTag_cvkConsole
{
	void* handle[4];
	int io[3];
} cvkConsole;


//-----------------------------------------------------------------------------

// cvkConsoleCreateMain
//	Create and initialize console instance for the main process; redirects 
//	standard input and output to new console (excludes standard error).
//		return SUCCESS: 0 if console successfully initialized
//		return FAILURE: -1 if console not initialized
int cvkConsoleCreateMain(cvkConsole* const console);

// cvkConsoleReleaseMain
//	Terminate and release console instance for the main process.
//		param console: pointer to descriptor that stores console info
//			valid: non-null
//		return SUCCESS: 0 if console successfully terminated
//		return FAILURE: -2 if console not terminated
int cvkConsoleReleaseMain(cvkConsole* const console);

// cvkConsolePrintDebug
//	Print formatted string to debugging interface.
//		param format: format string, as used with standard 'printf'
//			valid: non-null c-string
//		params ...: parameter list matching specifications in 'format'
//		return SUCCESS: result of internal print operation if succeeded
//		return FAILURE: -1 if invalid parameters
int cvkConsolePrintDebug(char const* const format, ...);


//-----------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif	// __cplusplus


#endif	// !_CVK_CONSOLE_H_