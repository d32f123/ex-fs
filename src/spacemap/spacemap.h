#ifndef SPACEMAP_H_GUARD
#define SPACEMAP_H_GUARD

#include <inttypes.h>
#include <iostream>


class space_map
{
public:
	explicit space_map(uint32_t bits_n);
	space_map(uint8_t * bits_arr, uint32_t bits_n);

	~space_map() { delete[] bits_arr; }

	bool operator[](uint32_t index) const;

	bool get(uint32_t index) const;
	void set(bool value, uint32_t index);

	uint8_t * bits_arr;

	friend std::ostream & operator<<(std::ostream & os, const space_map & sm);
private:
	uint32_t bits_count_;
	uint32_t bytes_count_;
};


#endif
