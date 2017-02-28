#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char to_hex (char code)
{
  static const char hex[] = "0123456789abcdef";
  return hex[code & 15];
}


/* This is HTML form encoding, not URL encoding! -- Netch */

/* Returns a url-encoded version of str *//* IMPORTANT: be sure to free() the returned string after use */
char *
url_encode (char *str){
  char *pstr = str, *buf = malloc (strlen (str) * 3 + 1), *pbuf = buf;
  while (*pstr)
    {
      int c = 255 & *pstr;
      if (c < 32 || c == 127 || *pstr == '%' || *pstr == '=') {
	*pbuf++ = '%', *pbuf++ = to_hex (*pstr >> 4), *pbuf++ =
	  to_hex (*pstr & 15);
      }
      else if (*pstr == ' ')
        *pbuf++ = '+';
      else
        *pbuf++ = *pstr;
      pstr++;
    }
  *pbuf = '\0';
  return buf;
}


