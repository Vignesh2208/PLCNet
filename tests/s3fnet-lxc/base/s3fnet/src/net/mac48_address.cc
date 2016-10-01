/**
 * \file mac48_address.cc
 *
 * \brief Source file for the class Mac48Address
 *
 */

#include "mac48_address.h"
#include <iomanip>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <sstream>

namespace s3f {
namespace s3fnet {

#define ASCII_a (0x41)
#define ASCII_z (0x5a)
#define ASCII_A (0x61)
#define ASCII_Z (0x7a)
#define ASCII_COLON (0x3a)
#define ASCII_ZERO (0x30)

static char AsciiToLowCase (char c)
{
	if (c >= ASCII_a && c <= ASCII_z) {
		return c;
	} else if (c >= ASCII_A && c <= ASCII_Z) {
		return c + (ASCII_a - ASCII_A);
	} else {
		return c;
	}
}

Mac48Address::Mac48Address ()
{
	std::memset (m_address, 0, 6);
}

Mac48Address::Mac48Address(const Mac48Address& addr)
{
	std::memset (m_address, 0, 6);
	addr.CopyTo(m_address);
}

Mac48Address::Mac48Address (const char *str)
{
	int i = 0;
	while (*str != 0 && i < 6)
	{
		uint8_t byte = 0;
		while (*str != ASCII_COLON && *str != 0)
		{
			byte <<= 4;
			char low = AsciiToLowCase (*str);
			if (low >= ASCII_a)
			{
				byte |= low - ASCII_a + 10;
			}
			else
			{
				byte |= low - ASCII_ZERO;
			}
			str++;
		}
		m_address[i] = byte;
		i++;
		if (*str == 0)
		{
			break;
		}
		str++;
	}
	assert (i == 6);
}

void Mac48Address::CopyFrom (const uint8_t buffer[6])
{
	std::memcpy (m_address, buffer, 6);
}

void Mac48Address::CopyFrom (const Mac48Address* addr)
{
	memcpy(m_address, addr->m_address, 6);
}

void Mac48Address::CopyTo (uint8_t buffer[6]) const
{
	std::memcpy (buffer, m_address, 6);
}

Mac48Address* Mac48Address::Allocate (void)
{
	static uint64_t id = 0;
	id++;
	Mac48Address* address = new Mac48Address();
	address->m_address[0] = (id >> 40) & 0xff;
	address->m_address[1] = (id >> 32) & 0xff;
	address->m_address[2] = (id >> 24) & 0xff;
	address->m_address[3] = (id >> 16) & 0xff;
	address->m_address[4] = (id >> 8) & 0xff;
	address->m_address[5] = (id >> 0) & 0xff;
	return address;
}

bool Mac48Address::IsBroadcast (void) const
{
	return *this == GetBroadcast ();
}
bool Mac48Address::IsGroup (void) const
{
	return (m_address[0] & 0x01) == 0x01;
}

Mac48Address Mac48Address::GetBroadcast (void)
{
	static Mac48Address broadcast = Mac48Address ("ff:ff:ff:ff:ff:ff");
	return broadcast;
}

Mac48Address Mac48Address::GetMulticastPrefix (void)
{
	static Mac48Address multicast = Mac48Address ("01:00:5e:00:00:00");
	return multicast;
}

Mac48Address Mac48Address::GetMulticast6Prefix (void)
{
	static Mac48Address multicast = Mac48Address ("33:33:00:00:00:00");
	return multicast;
}

std::ostream& operator<< (std::ostream& os, const Mac48Address & address)
{
	uint8_t ad[6];
	address.CopyTo (ad);

	os.setf (std::ios::hex, std::ios::basefield);
	os.fill ('0');
	for (uint8_t i=0; i < 5; i++)
	{
		os << std::setw (2) << (uint32_t)ad[i] << ":";
	}
	// Final byte not suffixed by ":"
	os << std::setw (2) << (uint32_t)ad[5];
	os.setf (std::ios::dec, std::ios::basefield);
	os.fill (' ');
	return os;
}

std::ostream& operator<< (std::ostream& os, const Mac48Address* address)
{
	uint8_t ad[6];
	address->CopyTo (ad);

	os.setf (std::ios::hex, std::ios::basefield);
	os.fill ('0');
	for (uint8_t i=0; i < 5; i++)
	{
		os << std::setw (2) << (uint32_t)ad[i] << ":";
	}
	// Final byte not suffixed by ":"
	os << std::setw (2) << (uint32_t)ad[5];
	os.setf (std::ios::dec, std::ios::basefield);
	os.fill (' ');
	return os;
}

static uint8_t AsInt (std::string v)
{
	std::istringstream iss;
	iss.str (v);
	uint32_t retval;
	iss >> std::hex >> retval >> std::dec;
	return retval;
}

std::istream& operator>> (std::istream& is, Mac48Address & address)
{
	std::string v;
	is >> v;

	std::string::size_type col = 0;
	for (uint8_t i = 0; i < 6; ++i)
	{
		std::string tmp;
		std::string::size_type next;
		next = v.find (":", col);
		if (next == std::string::npos)
		{
			tmp = v.substr (col, v.size ()-col);
			address.m_address[i] = AsInt (tmp);
			break;
		}
		else
		{
			tmp = v.substr (col, next-col);
			address.m_address[i] = AsInt (tmp);
			col = next + 1;
		}
	}
	return is;
}

std::istream& operator>> (std::istream& is, Mac48Address* address)
{
	std::string v;
	is >> v;

	std::string::size_type col = 0;
	for (uint8_t i = 0; i < 6; ++i)
	{
		std::string tmp;
		std::string::size_type next;
		next = v.find (":", col);
		if (next == std::string::npos)
		{
			tmp = v.substr (col, v.size ()-col);
			address->m_address[i] = AsInt (tmp);
			break;
		}
		else
		{
			tmp = v.substr (col, next-col);
			address->m_address[i] = AsInt (tmp);
			col = next + 1;
		}
	}
	return is;
}

int Mac48Address::IsEqual(const Mac48Address* addr)
{
	for(int i = 0; i < 6; i++)
	{
		if(m_address[i] != addr->m_address[i])
		{
			return 0;
		}
	}
	return 1;
}

}; // namespace s3fnet
}; // namespace s3f
