// p-system file system emulator to allow direct access to a Windows folder--TEXT conversions
// Copyright (C) 2013 Frank Seide

#pragma once

#include <string>
#include <vector>

bool converttoTEXT(const char * from, const char * to, bool make = false);
bool convertfromTEXT(const char * from, const char * to, bool make = false);

void converttoTEXT(const wchar_t * from, std::vector<char> & appendto);
void convertfromTEXT(const std::vector<char> & from, std::vector<wchar_t> & to);
