/**
 * SVN FILE: $Id: SaltEncoder.cpp 15669 2016-04-06 02:17:39Z sanmaul $
 *
 * (C) Copyright AhnLab, Inc.
 *
 * Any part of this source code can not be copied with
 * any method without prior written permission from
 * the author or authorized person.
 *
 * @author			sanmaul@ahnlab.com
 *
 * @version			$Revision: 15669 $
 * @modifiedby		$LastChangedBy: sanmaul $
 * 
 */

#include <time.h>
#include <string>
#include "SaltEncoder.h"

#include <atlbase.h>
#include <atlenc.h>
#include <strsafe.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSaltEncoder::CSaltEncoder(int nSalt/*=8*/)
{
	m_encode_pad = '*';
	genEncodeTable(nSalt);
}

CSaltEncoder::~CSaltEncoder()
{

}

void CSaltEncoder::genEncodeTable(int nSalt)
{
	// sanmaul: create a base64 table on run-time.

	const char table[] = {
		'-','f','3','_','v','8','s','p','V','G','2','9','k',
		'z','l','5','X','u','D','N','L','c','i','6','r','d',
		'o','Q','y','0','Z','F','q','m','S','A','C','M','g',
		'Y','b','j','R','e','7','n','P','T','O','E','K','t',
		'a','w','U','h','J','4','H','W','I','x','B','1','\0'
	};

	const int length = 64; // length is must be 64
	const int rnd = ((nSalt > 0 ? nSalt : (length-1)) % length) + 1;

	for(int l=0, n=0; ; l++)
	{
		for(int i=(rnd-1); i>=0; i--)
		{
			int p = i+(rnd*l);
			if(p >= length) continue;

			m_encode_table[n++] = table[p];
			if(n >= length) break;
		} // end of for
		if(n >= length) break;
	} // end of for
}

void CSaltEncoder::encode(const unsigned char *string, int length, unsigned char **ret_string, int *ret_length)
{
	unsigned char *presult = (unsigned char *)malloc( ((length + 3 - length % 3) * 4 / 3 + 1) );

	const unsigned char *current = string;
	int i = 0;

	while(length > 2) 
	{	
		/* keep going until we have less than 24 bits */
		presult[i++] = m_encode_table[current[0] >> 2];
		presult[i++] = m_encode_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
		presult[i++] = m_encode_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
		presult[i++] = m_encode_table[current[2] & 0x3f];

		current += 3;
		length  -= 3;	/* we just handle 3 octets of data */
	} // end of while

	/* now deal with the tail end of things */
	if(length != 0)
	{
		presult[i++] = m_encode_table[current[0] >> 2];
		if(length > 1) {
			presult[i++] = m_encode_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
			presult[i++] = m_encode_table[(current[1] & 0x0f) << 2];
			presult[i++] = m_encode_pad;
		}else {
			presult[i++] = m_encode_table[(current[0] & 0x03) << 4];
			presult[i++] = m_encode_pad;
			presult[i++] = m_encode_pad;
		}
	}

	presult[i] = 0x00;
	
	*ret_length = i;
	*ret_string = presult;
}

void CSaltEncoder::decode(const unsigned char *string, int length, unsigned char **ret_string, int *ret_length)
{
	unsigned char *presult = (unsigned char *)malloc( length + 1 );

	const unsigned char *current = string;
	int i = 0, j = 0, k, ch;
	char *chp = NULL;

	/* run through the whole string, converting as we go */
	while((ch = *current++) != '\0') 
	{
		if(ch == m_encode_pad) 
			break;

		if(ch == ' ') ch = '+'; 

		chp = strchr(m_encode_table, ch);
		if(chp == NULL) continue;

		ch = (char)(chp - m_encode_table);

		switch(i % 4) {
			case 0:
				presult[j] = (unsigned char)(ch << 2);
			break;
			case 1:
				presult[j++] |= ch >> 4;
				presult[j] = (unsigned char)(ch & 0x0f) << 4;
			break;
			case 2:
				presult[j++] |= ch >> 2;
				presult[j] = (unsigned char)(ch & 0x03) << 6;
			break;
			case 3:
				presult[j++] |= (unsigned char)ch;
			break;
		}
		i++;
	} // end of while

	k = j;

	/* mop things up if we ended on a boundary */
	if(ch == m_encode_pad)
	{
		switch(i % 4) {
			case 0 :
			case 1 :
				{
					//2018-06-15, ÃÖÁ¾µÎ, SATN-1524, Resource leaks ¼öÁ¤
					if(presult)
					{
						free(presult);
					}
				}
			return;

			case 2 :
				k++;
			case 3 :
				presult[k++] = 0x00;
			break;
		}
	}
	
	presult[k] = 0x00;

	*ret_length = j;
	*ret_string = presult;
}

std::string CSaltEncoder::EncodeString(std::string csdata)
{
	int nLength = csdata.length();

	unsigned char* pResult = NULL;
	encode((const unsigned char *)csdata.c_str(), (int)nLength, &pResult, &nLength);

	std::string csresult = "";
	if(pResult)	{
          char* tmp = new char[nLength];
          memset(tmp, 0, nLength);
          memcpy(tmp, pResult, nLength);
          csresult = std::string(tmp);
          csresult[nLength - 1] = 0;
          free(pResult);
	}

	return csresult;
}

std::string CSaltEncoder::DecodeString(std::string csdata)
{
	int nLength = csdata.length();

	unsigned char* pResult = NULL;
	decode((const unsigned char *)csdata.c_str(), (int)nLength, &pResult, &nLength);

	std::string csresult = "";
    if (pResult) {
        char* tmp = new char[nLength];
      memset(tmp, 0, nLength);
		memcpy(tmp, pResult, nLength);
        csresult = std::string(tmp);
		csresult[nLength - 1] = 0;
        free(pResult);
    }

	return csresult;
}

std::string CSaltEncoder::EncodeBase64(std::string csdata)
{
	int lenData = csdata.length();
	if(lenData <=0 ) return "";

	int lenBase64 = ::Base64EncodeGetRequiredLength(lenData);

	std::string encode;

	encode.replace(0, encode.find("\r"), "");
    encode.replace(0, encode.find("\n"), "");

	return encode;
}

std::string CSaltEncoder::DecodeBase64(std::string csdata)
{
	int lenData = csdata.length();
	if(lenData <=0 ) return "";

	int lenBase64 = ::Base64DecodeGetRequiredLength(lenData);

	std::string decode;


	return decode;
}

std::string CSaltEncoder::EncodeWithBase64(std::string csdata)
{
	std::string encode = EncodeBase64(csdata);
	return EncodeString(encode);
}

std::string CSaltEncoder::DecodeWithBase64(std::string csdata)
{
	std::string decode = DecodeString(csdata);
	return DecodeBase64(decode);
}

std::string CSaltEncoder::GenRandomString(int length) 
{
	std::string string;

	static const char alphanum[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_!@#$";

	srand((unsigned int)time(NULL));

	for(int i = 0; i < length; ++i) {
		string[i] = alphanum[rand() % (strlen(alphanum) - 1)];
	}

	return string;
}