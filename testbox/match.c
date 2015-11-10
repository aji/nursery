#include <ctype.h>
#include <stdio.h>

/* this wildcard matcher taken from ircd-micro */
int match(char *mask, char *string)
{
	char *m = mask, *s = string;
	char *m_bt = m, *s_bt = s;
 
	for (;;) {
		switch (*m) {
		case '\0':
			if (*s == '\0')
				return 1;
		backtrack:
			if (m_bt == mask)
				return 0;
			m = m_bt;
			s = ++s_bt;
			break;
 
		case '*':
			while (*m == '*')
				m++;
			m_bt = m;
			s_bt = s;
			break;
 
		default:
			if (!*s)
				return 0;
			if (toupper(*m) != toupper(*s)
			    && *m != '?')
				goto backtrack;
			m++;
			s++;
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3)
		return 1;
	return match(argv[1], argv[2]);
}
