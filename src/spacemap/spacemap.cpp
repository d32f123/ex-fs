#include "spacemap.h"
#include <string.h>

#include <iomanip>

struct hex_char_struct
{
  unsigned char c;
  hex_char_struct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const hex_char_struct& hs)
{
  return (o << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(hs.c));
}

inline hex_char_struct hex(unsigned char _c)
{
  return {_c};
}

space_map::space_map(const space_map& that)
{
	bytes_count_ = that.bytes_count_;
	bits_count_ = that.bits_count_;
	bits_arr = new uint8_t[bytes_count_];
	memcpy(this->bits_arr, that.bits_arr, sizeof(uint8_t) * bytes_count_);
}

space_map& space_map::operator=(const space_map& that)
{
	if (this != &that)
	{
		delete[] bits_arr;
		bytes_count_ = that.bytes_count_;
		bits_count_ = that.bits_count_;
		bits_arr = new uint8_t[bytes_count_];
		memcpy(this->bits_arr, that.bits_arr, sizeof(uint8_t) * bytes_count_);
	}
	return *this;
}

space_map::space_map(uint32_t bits_n)
{
	bytes_count_ = bits_n / 8 + ((bits_n % 8) != 0);
	this->bits_arr = new uint8_t[bytes_count_]();
	bits_count_ = bits_n;
}

space_map::space_map(uint8_t * const bits_arr, const uint32_t bits_n)
{
	bytes_count_ = bits_n / 8 + ((bits_n % 8) != 0);
	this->bits_arr = new uint8_t[bytes_count_];
	bits_count_ = bits_n;
	memcpy(this->bits_arr, bits_arr, bytes_count_);
}

bool space_map::get(uint32_t index) const
{
	return this->operator[](index);
}

bool space_map::operator[](uint32_t index) const
{
	if (index >= bits_count_)
		return false;
	return bits_arr[index / 8] & (0b10000000 >> (index % 8));
}

void space_map::set(bool value, uint32_t index)
{
	if (index >= bits_count_)
		return;
	if (value)
		bits_arr[index / 8] |= (0b10000000 >> (index % 8));
	else
		bits_arr[index / 8] &= (~(0b10000000 >> (index % 8)));
}

std::ostream & operator<<(std::ostream & os, const space_map & sm)
{
	for (uint32_t i = 0; i < sm.bytes_count_; ++i)
	{
		os << hex(sm.bits_arr[i]) << (((i + 1) % 8) == 0 ? "\n" : "|");
	}
	os << std::endl << std::dec;
	return os;
}