#include "spacemap.h"
#include <string.h>

space_map::space_map(uint32_t bits_n)
{
	bytes_count_ = bits_n / 8 + ((bits_n % 8) == 0);
	this->bits_arr = new uint8_t[bytes_count_]();
	bits_count_ = bits_n;
}

space_map::space_map(uint8_t * const bits_arr, const uint32_t bits_n)
{
	bytes_count_ = bits_n / 8 + ((bits_n % 8) == 0);
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
	return bits_arr[index / 8] & (1 << (index % 8));
}

void space_map::set(bool value, uint32_t index)
{
	if (index >= bits_count_)
		return;
	if (value)
		bits_arr[index / 8] |= (1 << (index % 8));
	else
		bits_arr[index / 8] &= (~(1 << (index % 8)));
}