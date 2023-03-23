#pragma once

class CSaltEncoder  
{
public:
	CSaltEncoder(int nSalt=8);
	virtual ~CSaltEncoder();

public:
	std::string	EncodeString(std::string csdata);
	std::string	DecodeString(std::string csdata);

	std::string	EncodeWithBase64(std::string csdata);
	std::string	DecodeWithBase64(std::string csdata);

	static std::string	EncodeBase64(std::string csdata);
	static std::string	DecodeBase64(std::string csdata);

	static std::string GenRandomString(int length);

protected:
	char	m_encode_table[64];	// size is must be 64
	char	m_encode_pad;

	void	genEncodeTable(int nSalt);

	void	encode(const unsigned char *string, int length, unsigned char **ret_string, int *ret_length);
	void	decode(const unsigned char *string, int length, unsigned char **ret_string, int *ret_length);
};