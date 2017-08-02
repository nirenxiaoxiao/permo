#ifndef UTILS_H_
#define UTILS_H_
typedef unsigned int	UC;					//Unicode����
class Utils
{
public:

//Ansi�ַ���ת����UTF16
	static void AnsiToUtf16(const char *name, wchar_t *wname, int nSize);
	
	static void Utf16ToAnsi(const wchar_t *wchars, char *chars, int nSize);
	
	static void UCS32ToUCS16(const UC UC32Char, TCHAR *buffer);
	
	//�ж�һ��4�ֽ�TChar���飬���ɼ���������ɵġ�����ֵ��0��1��2
	static int UCS16Len(TCHAR *buffer);
};

#endif