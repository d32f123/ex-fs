#ifndef SPACEMAP_H_GUARD
#define SPACEMAP_H_GUARD

#include <inttypes.h>

class space_map
{
public:
	explicit space_map(uint32_t bits_n);
	space_map(uint8_t * bits_arr, uint32_t bits_n);

	~space_map() { delete[] bits_arr; }

	bool operator[](int index) const;

	bool get(int index) const;
	void set(bool value, int index);

	uint8_t * bits_arr;
private:
	
	uint32_t bits_count_;
	uint32_t bytes_count_;
};


#endif
