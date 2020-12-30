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

	cvk-types.h
	Utility to define common non-standard types.
*/

#ifndef _CVK_TYPES_H_
#define _CVK_TYPES_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


//-----------------------------------------------------------------------------

typedef uint8_t			byte, ui8;		// Convenient alias for 8-bit unsigned byte.
typedef uint16_t		word, ui16;		// Convenient alias for 16-bit unsigned short integer.
typedef uint32_t		dword, ui32;	// Convenient alias for 32-bit unsigned long integer or double-word.
typedef uint64_t		qword, ui64;	// Convenient alias for 64-bit unsigned long-long integer or quad-word.
typedef int8_t			i8;				// Convenient alias for 8-bit signed byte.
typedef int16_t			i16;			// Convenient alias for 16-bit signed short integer.
typedef int32_t			i32;			// Convenient alias for 32-bit signed long integer.
typedef int64_t			i64;			// Convenient alias for 64-bit signed long-long integer.
typedef size_t			size, psize;	// Convenient alias for architecture-dependent pointer-sized integer.
typedef ptrdiff_t		diff, pdiff;	// Convenient alias for architecture-dependent pointer difference.
typedef unsigned int	uint;			// Convenient alias for standard unsigned integer.
typedef float			flt, f32;		// Convenient alias for 32-bit single-precision floating point number.
typedef double			dbl, f64;		// Convenient alias for 64-bit double-precision floating point number.
typedef void*			ptr;			// Convenient alias for generic pointer.
typedef void const*		kptr;			// Convenient alias for generic pointer to constant.
typedef char*			str;			// Convenient alias for generic character string (c-string).
typedef char const*		kstr;			// Convenient alias for generic constant character string (c-string).


//-----------------------------------------------------------------------------


#endif	// !_CVK_TYPES_H_