/*
@file json.c
@brief handles very basic JSON with a minimal footprint on the system

This code is a lightly modified version of cJSON 1.4.7. cJSON is licensed under the MIT license:
Copyright (c) 2009 Dave Gamble

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

@see https://github.com/DaveGamble/cJSON
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"


bool json_print_string(const unsigned char *input, unsigned char *output_buffer)
{
	const unsigned char *input_pointer = NULL;
	unsigned char *output = NULL;
	unsigned char *output_pointer = NULL;
	size_t output_length = 0;
	/* numbers of additional characters needed for escaping */
	size_t escape_characters = 0;

	if (output_buffer == NULL)
	{
		return false;
	}

	/* empty string */
	if (input == NULL)
	{
		//output = ensure(output_buffer, sizeof("\"\""), hooks);
		if (output == NULL)
		{
			return false;
		}
		strcpy((char*)output, "\"\"");

		return true;
	}

	/* set "flag" to 1 if something needs to be escaped */
	for (input_pointer = input; *input_pointer; input_pointer++)
	{
		if (strchr("\"\\\b\f\n\r\t", *input_pointer))
		{
			/* one character escape sequence */
			escape_characters++;
		}
		else if (*input_pointer < 32)
		{
			/* UTF-16 escape sequence uXXXX */
			escape_characters += 5;
		}
	}
	output_length = (size_t)(input_pointer - input) + escape_characters;

	/* in the original cJSON it is possible to realloc here in case output buffer is too small.
	 * This is overkill for an embedded system. */
	output = output_buffer;

	/* no characters have to be escaped */
	if (escape_characters == 0)
	{
		output[0] = '\"';
		memcpy(output + 1, input, output_length);
		output[output_length + 1] = '\"';
		output[output_length + 2] = '\0';

		return true;
	}

	output[0] = '\"';
	output_pointer = output + 1;
	/* copy the string */
	for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
	{
		if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
		{
			/* normal character, copy */
			*output_pointer = *input_pointer;
		}
		else
		{
			/* character needs to be escaped */
			*output_pointer++ = '\\';
			switch (*input_pointer)
			{
			case '\\':
				*output_pointer = '\\';
				break;
			case '\"':
				*output_pointer = '\"';
				break;
			case '\b':
				*output_pointer = 'b';
				break;
			case '\f':
				*output_pointer = 'f';
				break;
			case '\n':
				*output_pointer = 'n';
				break;
			case '\r':
				*output_pointer = 'r';
				break;
			case '\t':
				*output_pointer = 't';
				break;
			default:
				/* escape and print as unicode codepoint */
				sprintf((char*)output_pointer, "u%04x", *input_pointer);
				output_pointer += 4;
				break;
			}
		}
	}
	output[output_length + 1] = '\"';
	output[output_length + 2] = '\0';

	return true;
}

