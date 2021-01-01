/*
   Copyright 2020-2021 Daniel S. Buckstein

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

	main.c
	Entry point source for application.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvk/cvk.h"


//-----------------------------------------------------------------------------

#ifdef _WIN32
// Windows entry point.
int __stdcall WinMain(
	kptr const hInstance,
	kptr const hPrevInstance,
	kstr const lpCmdLine,
	int const nCmdShow)
#else	// !_WIN32
// Other.
int main()
#endif	// _WIN32
{
	int status = 0, result = 0;
	cvkConsole console[1] = { 0 };

	status = cvkConsoleCreateMain(console);

	status = cvkConsoleReleaseMain(console);

	return result;
}


//-----------------------------------------------------------------------------
