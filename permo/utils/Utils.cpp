#include "StdAfx.h"
#include "Utils.h"

/*	Ansi字符串转换到UTF16
 */
void Utils::AnsiToUtf16(const char *name, wchar_t *wname, int nSize)
{
	MultiByteToWideChar(936, 0, name, (int)strlen(name) + 1, wname, nSize);
}

void Utils::Utf16ToAnsi(const wchar_t *wchars, char *chars, int nSize)
{
	WideCharToMultiByte(936, 0, wchars, -1, chars, nSize, NULL, FALSE);
}

void Utils::UCS32ToUCS16(const UC UC32Char, TCHAR *buffer)
{
	buffer[1] = 0;

	if (UC32Char > 0x10FFFF || (UC32Char >= 0xD800 && UC32Char <= 0xDFFF))
	{
		buffer[0] = '?';
		return;
	}

	if (UC32Char < 0x10000)
		buffer[0] = (TCHAR)UC32Char;
	else
	{
		buffer[0] = (UC32Char - 0x10000) / 0x400 + 0xD800;
		buffer[1] = (UC32Char - 0x10000) % 0x400 + 0xDC00;
		buffer[2] = 0;
	}
}

//判断一个4字节TChar数组，是由几个汉字组成的。返回值：0，1，2
int Utils::UCS16Len(TCHAR *buffer)
{
	size_t L = _tcslen( buffer );
	if ( L == 0 )
		return 0;
	if ( L > 2 )
	  return 2;
	if ( L == 2 )
	{
	  if (( buffer[0] >= 0xD800 ) && ( buffer[0] <= 0xDFFF ))
		return 1;
	  else
		return 2;
	}
	return 1;
}