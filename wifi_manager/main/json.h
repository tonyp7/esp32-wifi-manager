/*
 * @file json.h
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
#ifndef MAIN_JSON_H_
#define MAIN_JSON_H_


char* json_escape_string(char *str);
char* json_unescape_string(const char *str);

#endif /* MAIN_JSON_H_ */
