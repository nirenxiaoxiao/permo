#ifndef UTILS_H_
#define UTILS_H_
typedef unsigned int	UC;					//Unicode编码
class Utils
{
public:

//Ansi字符串转换到UTF16
	static void AnsiToUtf16(const char *name, wchar_t *wname, int nSize);
	
	static void Utf16ToAnsi(const wchar_t *wchars, char *chars, int nSize);
	
	static void UCS32ToUCS16(const UC UC32Char, TCHAR *buffer);
	
	//判断一个4字节TChar数组，是由几个汉字组成的。返回值：0，1，2
	static int UCS16Len(TCHAR *buffer);
};

#endif