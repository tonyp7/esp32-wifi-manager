/*
 * @file json.c
 * @brief handles very basic JSON with a minimal footprint on the system
 *
 * This code is a lightly modified version of cJSON. cJSON is licensed under the MIT license:
 * Copyright (c) 2009-2017, Dave Gamble and cJSON contributors
 * Copyright (c) 2013, Esoteric Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * @see https://github.com/DaveGamble/cJSON
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "json.h"

const char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

char* json_escape_string(char *str){

	return str;
}



char* json_unescape_string(const char *str) {
	const char* ptr = str + 1;
	char* ptr2;
	char* out;
	int len = 0;
	unsigned uc, uc2;
	if (*str != '\"') { /* TODO: don't need this check when called from parse_value, but do need from parse_object */
		//ep = str;
		return 0;
	} /* not a string! */

	while (*ptr != '\"' && *ptr && ++len)
		if (*ptr++ == '\\') ptr++; /* Skip escaped quotes. */

	out = (char*)malloc(sizeof(char) * (len + 1)); /* The length needed for the string, roughly. */
	if (!out) return 0;

	ptr = str + 1;
	ptr2 = out;
	while (*ptr != '\"' && *ptr) {
		if (*ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u': /* transcode utf16 to utf8. */
				sscanf(ptr + 1, "%4x", &uc);
				ptr += 4; /* get the unicode char. */

				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) break; /* check for invalid.	*/

																	  /* TODO provide an option to ignore surrogates, use unicode replacement character? */
				if (uc >= 0xD800 && uc <= 0xDBFF) /* UTF16 surrogate pairs.	*/
				{
					if (ptr[1] != '\\' || ptr[2] != 'u') break; /* missing second-half of surrogate.	*/
					sscanf(ptr + 3, "%4x", &uc2);
					ptr += 6;
					if (uc2 < 0xDC00 || uc2 > 0xDFFF) break; /* invalid second-half of surrogate.	*/
					uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
				}

				len = 4;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				else if (uc < 0x10000) len = 3;
				ptr2 += len;

				switch (len) {
				case 4:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
					/* fallthrough */
				case 3:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
					/* fallthrough */
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
					/* fallthrough */
				case 1:
					*--ptr2 = (uc | firstByteMark[len]);
				}
				ptr2 += len;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"') ptr++; /* TODO error handling if not \" or \0 ? */
	//item->valueString = out;
	//item->type = Json_String;
	//return ptr;
	return out;
}
