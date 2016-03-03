#ifndef __MAC48_ADDRESS_H__
#define __MAC48_ADDRESS_H__

#include <stdint.h>
#include <ostream>
#include <string.h>

namespace s3f {
namespace s3fnet {

/**
 * \file mac48_address.h
 * 
 * \brief an EUI-48 address
 *
 * This class can contain 48 bit IEEE addresses. This class is ported from the ns-3 project.
 * http://www.nsnam.org/docs/release/3.17/doxygen/mac48-address_8h_source.html
 */

/**
 * MAC address (48 bit IEEE addresses)
 */
class Mac48Address
{
public:
	Mac48Address ();
	/**
	 * \param str a string representing the new Mac48Address
	 *
	 * The format of the string is "xx:xx:xx:xx:xx:xx"
	 */
	Mac48Address (const char *str);

	Mac48Address (const Mac48Address& addr);

	/**
	 * \param buffer address in network order
	 *
	 * Copy the input address to our internal buffer.
	 */
	void CopyFrom (const uint8_t buffer[6]);
	/**
	 * \param buffer address in network order
	 *
	 * Copy the internal address to the input buffer.
	 */
	void CopyTo (uint8_t buffer[6]) const;

	void CopyFrom (const Mac48Address* addr);

	/**
	 * Allocate a new Mac48Address.
	 */
	static Mac48Address* Allocate (void);

	/**
	 * \returns true if this is a broadcast address, false otherwise.
	 */
	bool IsBroadcast (void) const;

	/**
	 * \returns true if the group bit is set, false otherwise.
	 */
	bool IsGroup (void) const;

	/**
	 * \returns the broadcast address
	 */
	static Mac48Address GetBroadcast (void);

	/**
	 * \returns the multicast prefix (01:00:5e:00:00:00).
	 */
	static Mac48Address GetMulticastPrefix (void);

	/**
	 * \brief Get the multicast prefix for IPv6 (33:33:00:00:00:00).
	 * \returns a multicast address.
	 */
	static Mac48Address GetMulticast6Prefix (void);

	/**
	 * \brief Check if two MAC addresses are the same *
	 * \return 1 if equal, 0 otherwise
	 */
	int IsEqual(const Mac48Address* addr);

private:

	friend bool operator < (const Mac48Address &a, const Mac48Address &b);
	friend bool operator == (const Mac48Address &a, const Mac48Address &b);
	friend bool operator != (const Mac48Address &a, const Mac48Address &b);
	friend std::istream& operator>> (std::istream& is, Mac48Address & address);
	friend std::istream& operator>> (std::istream& is, Mac48Address* address);

	uint8_t m_address[6];
};

inline bool operator == (const Mac48Address &a, const Mac48Address &b)
		{
	return memcmp (a.m_address, b.m_address, 6) == 0;
		}
inline bool operator != (const Mac48Address &a, const Mac48Address &b)
		{
	return memcmp (a.m_address, b.m_address, 6) != 0;
		}
inline bool operator < (const Mac48Address &a, const Mac48Address &b)
{
	return memcmp (a.m_address, b.m_address, 6) < 0;
}

std::ostream& operator<< (std::ostream& os, const Mac48Address* address);
std::ostream& operator<< (std::ostream& os, const Mac48Address & address);
std::istream& operator>> (std::istream& is, Mac48Address & address);
std::istream& operator>> (std::istream& is, Mac48Address* address);

}; // namespace s3fnet
}; // namespace s3f

#endif /* __MAC48_ADDRESS_H__ */
