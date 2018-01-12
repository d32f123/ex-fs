#ifndef SPACEMAP_H_GUARD
#define SPACEMAP_H_GUARD

#include <cstdint>
#include <iostream>

class space_map
{
public:
	space_map(const space_map & that);
	explicit space_map(std::size_t bits_n);
	space_map(uint8_t * bits_arr, std::size_t bits_n);

	~space_map() { delete[] bits_arr; }

	bool operator[](std::size_t index) const;

	bool get(std::size_t index) const;
	void set(bool value, std::size_t index) const;

	std::size_t find_first_of(bool val) const;

	uint8_t * bits_arr;
	uint32_t get_bytes_count() const { return bytes_count_; }
	uint32_t get_bits_count() const { return bits_count_; }

	friend std::ostream & operator<<(std::ostream & os, const space_map & sm);
	space_map& operator=(const space_map & that);
private:
	std::size_t bits_count_;
	std::size_t bytes_count_;
};


#endif
