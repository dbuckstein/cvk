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

	cvkConsole.c
	Console utility source.
*/

#include "cvk/cvk-util/cvkConsole.h"
#ifdef _WIN32

#include <io.h>
#include <stdio.h>
#include <Windows.h>


//-----------------------------------------------------------------------------

// redirect with settings
inline void cvkConsoleInternalRedirectToggle(cvkConsole* const console, int const redirectInput, int const redirectOutput)
{
	FILE* str = 0;
	int i = -1, j = -1;

	// redirect input
	i = 0;
	if (redirectInput)
	{
		if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE && !console->handle[i])
		{
			// flush buffer, duplicate handle and reopen stream to console
			//j = fprintf(stdin, "\n STDIN =/= DEFAULT \n");
			j = fflush(stdin);
			j = _dup(i);
			str = freopen("CONIN$", "r+", stdin);
			if (str)
			{
				// store values and configure
				console->handle[i] = str;
				console->io[i] = j;
				j = setvbuf(stdin, NULL, _IONBF, 0);
				//j = fprintf(stdin, "\n STDIN == CONSOLE \n");
			}
		}
	}
	else
	{
		if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE && console->handle[i])
		{
			// flush and reopen
			//j = fprintf(stdin, "\n STDIN =/= CONSOLE \n");
			j = fflush(stdin);
			str = freopen("NUL:", "r+", stdin);
			if (str)
			{
				// duplicate handle and reconfigure stream, reset variables
				j = _dup2(console->io[i], i);
				j = setvbuf(stdin, NULL, _IONBF, 0);
				//j = fprintf(stdin, "\n STDIN == DEFAULT \n");
				console->handle[i] = 0;
				console->io[i] = -1;
			}
		}
	}

	// redirect output
	i = 1;
	if (redirectOutput)
	{
		if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE && !console->handle[i])
		{
			// flush buffer, duplicate handle and reopen stream to console
			//j = fprintf(stdout, "\n STDOUT =/= DEFAULT \n");
			j = fflush(stdout);
			j = _dup(i);
			str = freopen("CONOUT$", "a+", stdout);
			if (str)
			{
				// store values and configure
				console->handle[i] = str;
				console->io[i] = j;
				j = setvbuf(stdout, NULL, _IONBF, 0);
				//j = fprintf(stdout, "\n STDOUT == CONSOLE \n");
			}
		}
	}
	else
	{
		if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE && console->handle[i])
		{
			// flush and reopen
			//j = fprintf(stdout, "\n STDOUT =/= CONSOLE \n");
			j = fflush(stdout);
			str = freopen("NUL:", "a+", stdout);
			if (str)
			{
				// duplicate handle and reconfigure stream, reset variables
				j = _dup2(console->io[i], i);
				j = setvbuf(stdout, NULL, _IONBF, 0);
				//j = fprintf(stdout, "\n STDOUT == DEFAULT \n");
				console->handle[i] = 0;
				console->io[i] = -1;
			}
		}
	}
}


//-----------------------------------------------------------------------------

int cvkConsoleCreateMain(cvkConsole* const console)
{
	if (console)
	{
		// if console not already open
		void* handle = GetConsoleWindow();
		if (!handle && !console->handle[3])
		{
			// allocate and show console
			if (AllocConsole())
			{
				// reset flags
				console->handle[0] = console->handle[1] = console->handle[2] = 0;
				console->io[0] = console->io[1] = console->io[2] = -1;

				// init flag
				console->handle[3] = handle = GetConsoleWindow();

				// disable closing console manually because doing this kills 
				//	the whole application; could also start a new process, 
				//	but then there's also that to manage
				DeleteMenu(GetSystemMenu(handle, FALSE), SC_CLOSE, MF_BYCOMMAND);

				// redirect to new console (in/out, not err)
				cvkConsoleInternalRedirectToggle(console, 1, 1);

				// done
				return 0;
			}
			return -2;
		}
		return +1;
	}
	return -1;
}


int cvkConsoleReleaseMain(cvkConsole* const console)
{
	if (console)
	{
		// if console exists
		void* const handle = GetConsoleWindow();
		if ((console->handle[3] == handle) && handle)
		{
			// reset to original standard i/o
			cvkConsoleInternalRedirectToggle(console, 0, 0);

			// delete console instance
			// console will hide when all standard handles are closed
			if (FreeConsole())
			{
				// reset
				console->handle[3] = 0;

				// done
				return 0;
			}
			return -2;
		}
		return +1;
	}
	return -1;
}


int cvkConsolePrintDebug(char const* const format, ...)
{
	if (format)
	{
		char str[256] = { 0 };
		va_list args = 0;
		int result = 0;

		// fill buffer with formatted arguments
		va_start(args, format);
		result = _vsnprintf(str, sizeof(str), format, args);
		va_end(args);

		// internal print
		OutputDebugStringA(str);

		// return length
		return result;
	}
	return -1;
}


//-----------------------------------------------------------------------------


#endif	// _WIN32