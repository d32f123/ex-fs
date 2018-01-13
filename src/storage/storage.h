#ifndef STORAGE_H_GUARD
#define STORAGE_H_GUARD

#include <cstdlib>
#include <exception>
#include <iostream>

#include "../spacemap/spacemap.h"

template <typename T>
class storage
{
public:
	explicit storage(std::size_t size);

	std::size_t insert(const T & elem);
	void remove(std::size_t index) const;

	T& get(std::size_t index);
	void set(std::size_t index, const T & elem);

	T& operator[](const std::size_t index) { return get(index); }

	void resize(std::size_t new_size);

	storage(const storage& that);
	storage(storage&& that) noexcept;

	storage& operator=(const storage& that);
	storage& operator=(storage&& that) noexcept;

	~storage();
private:
	T * arr_;
	std::size_t arr_size_;
	space_map * arr_map_;
	void clean_arr() const;
};

template <typename T>
storage<T>::storage(const std::size_t size) 
	: arr_(new T[size]), arr_size_(size), arr_map_(new space_map(size))
{
}

template <typename T>
std::size_t storage<T>::insert(const T& elem)
{
	const auto index = arr_map_->find_first_of(false);
	arr_[index] = T(elem);
	arr_map_->set(true, index);
	return index;
}

template <typename T>
void storage<T>::remove(const std::size_t index) const
{
	if (index >= arr_size_ || !(*arr_map_)[index])
		throw std::exception();
	arr_map_->set(false, index);
}

template <typename T>
T& storage<T>::get(const std::size_t index)
{
	if (index >= arr_size_ || !(*arr_map_)[index])
		throw std::exception();
	return arr_[index];
}

template <typename T>
void storage<T>::set(const std::size_t index, const T& elem)
{
	if (index >= arr_size_ || !(*arr_map_)[index])
		throw std::exception();
	arr_[index] = elem;
}

template <typename T>
void storage<T>::resize(const std::size_t new_size)
{
	T * temp = new T[new_size];

	for (std::size_t i = 0; i < new_size; ++i)
	{
		if ((*arr_map_)[i])
		{
			temp[i] = arr_[i];
		}
	}
	delete[] arr_;
	arr_ = temp;

	delete arr_map_;
	arr_map_ = new space_map(new_size);
}

template <typename T>
storage<T>::storage(const storage& that)
{
	arr_ = new T[that.arr_size_];
	arr_map_ = new space_map(that.arr_size_);
	arr_size_ = that.arr_size_;

	memcpy(arr_map_->bits_arr, that.arr_map_->bits_arr, that.arr_map_->get_bytes_count());

	for (std::size_t i = 0; i < arr_size_; ++i)
	{
		if (arr_map_->get(i))
		{
			arr_[i] = that.arr_[i];
		}
	}
}

template <typename T>
storage<T>::storage(storage&& that) noexcept
{
	arr_ = that.arr_;
	that.arr_ = nullptr;

	arr_map_ = that.arr_map_;
	that.arr_map_ = nullptr;

	arr_size_ = that.arr_size_;
	that.arr_size_ = 0;
}

template <typename T>
storage<T>& storage<T>::operator=(const storage& that)
{
	clean_arr();
	delete arr_map_;

	arr_ = new T[that.arr_size_];
	arr_map_ = new space_map(that.arr_size_);
	arr_size_ = that.arr_size_;

	memcpy(arr_map_->bits_arr, that.arr_map_->bits_arr, that.arr_map_->get_bytes_count());

	for (std::size_t i = 0; i < arr_size_; ++i)
	{
		if (arr_map_->get(i))
		{
			arr_[i] = that.arr_[i];
		}
	}

	return *this;
}

template <typename T>
storage<T>& storage<T>::operator=(storage&& that) noexcept
{
	clean_arr();
	delete arr_map_;

	arr_ = that.arr_;
	that.arr_ = nullptr;

	arr_map_ = that.arr_map_;
	that.arr_map_ = nullptr;

	arr_size_ = that.arr_size_;
	that.arr_size_ = 0;

	return *this;
}

template <typename T>
storage<T>::~storage()
{
	clean_arr();
	delete arr_map_;
}

template <typename T>
void storage<T>::clean_arr() const
{
	delete[] arr_;
}


#endif
