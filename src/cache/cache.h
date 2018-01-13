#ifndef CACHE_H_GUARD
#define CACHE_H_GUARD

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <utility>

#define INVALID_NODE	(static_cast<std::size_t>(-1))

template <typename TKey, typename TVal>
struct node;

template <typename TKey, typename TVal>
class cache
{
public:
	explicit cache(const std::size_t size)
		: size_{size}, nodes_{new node<TKey, TVal>[size]} {}

	void insert(const TKey& key, const TVal& elem);
	bool contains(const TKey& key);
	TVal& get(const TKey& key);
	void clear() { count_ = 0; }

	cache(const cache& that);
	cache(cache&& that) noexcept;

	cache& operator=(const cache& that);
	cache& operator=(cache&& that) noexcept;

	~cache();
private:
	std::size_t count_{0};
	std::size_t size_;
	std::size_t head_{INVALID_NODE};
	std::size_t tail_{INVALID_NODE};
	node<TKey, TVal>* nodes_;

	std::size_t get_index(const TKey& key);
	void set_head(std::size_t index);
};

template <typename TKey, typename TVal>
struct node
{
	node() = default;
	node(TKey key, TVal val) : node(key, val, INVALID_NODE, INVALID_NODE) {}

	node(TKey key, TVal val, const std::size_t ll, const std::size_t rl)
		: key{key}, val{std::move(val)}, left_link{ll}, right_link{rl} {}

	TKey key{};
	TVal val{};
	std::size_t left_link{INVALID_NODE};
	std::size_t right_link{INVALID_NODE};
};

template <typename TKey, typename TVal>
void cache<TKey, TVal>::insert(const TKey& key, const TVal& elem)
{
	using std::cerr;
	if (size_ == 0)
		return;
	if (contains(key))
	{
		//cerr << "cache ins " << key << ": hit\n";
		auto index = get_index(key);
		set_head(index);
		nodes_[index].val = elem;
		return;
	}
	// just insert if space remaining
	if (count_ < size_)
	{
		//cerr << "cache ins " << key << ": miss, free space, adding node " << count_ << "\n";
		nodes_[count_] = node<TKey, TVal>(key, elem, INVALID_NODE, head_);
		if (head_ != INVALID_NODE)
			nodes_[head_].left_link = count_;
		head_ = count_;
		if (count_ == 0)
			tail_ = count_;
		++count_;
		return;
	}

	//cerr << "cache ins " << key << ": miss, no space, node " << tail_ << "removed\n";
	const auto tail_temp = tail_;
	set_head(tail_);

	nodes_[tail_temp] = node<TKey, TVal>(key, elem);
}

template <typename TKey, typename TVal>
bool cache<TKey, TVal>::contains(const TKey& key)
{
	using std::cerr;
	for (std::size_t i = 0; i < count_; ++i)
	{
		if (nodes_[i].key == key)
		{
			//cerr << "cache con " << key << ": hit\n";
			return true;
		}
	}
	//cerr << "cache con " << key << ": miss\n";
	return false;
}

template <typename TKey, typename TVal>
TVal& cache<TKey, TVal>::get(const TKey& key)
{
	using std::cerr;
	for (std::size_t i = 0; i < count_; ++i)
	{
		if (nodes_[i].key == key)
		{
			//cerr << "cache get " << key << ": hit\n";
			set_head(i);
			return nodes_[i].val;
		}
	}
	//cerr << "cache get " << key << ": miss\n";
	throw std::exception();
}

template <typename TKey, typename TVal>
cache<TKey, TVal>::cache(const cache& that)
{
	this->head_ = that.head_;
	this->tail_ = that.tail_;

	this->size_ = that.size_;
	this->count_ = that.count_;

	nodes_ = new node<TKey, TVal>[size_];

	memcpy(nodes_, that.nodes_, sizeof(node<TKey, TVal>) * size_);
}

template <typename TKey, typename TVal>
cache<TKey, TVal>::cache(cache&& that) noexcept
{
	this->head_ = that.head_;
	that.head_ = INVALID_NODE;
	this->tail_ = that.tail_;
	that.tail_ = INVALID_NODE;

	this->size_ = that.size_;
	that.size_ = 0;

	this->count_ = that.count_;
	that.count_ = 0;

	this->nodes_ = that.nodes_;
	that.nodes_ = nullptr;
}

template <typename TKey, typename TVal>
cache<TKey, TVal>& cache<TKey, TVal>::operator=(const cache& that)
{
	if (this == &that) return *this;

	this->head_ = that.head_;
	this->tail_ = that.tail_;

	this->size_ = that.size_;
	this->count_ = that.count_;

	nodes_ = new node<TKey, TVal>[size_];

	for (std::size_t i = 0; i < size_; ++i)
		nodes_[i] = that.nodes_[i];

	return *this;
}

template <typename TKey, typename TVal>
cache<TKey, TVal>& cache<TKey, TVal>::operator=(cache&& that) noexcept
{
	if (this == &that) return *this;

	this->head_ = that.head_;
	that.head_ = INVALID_NODE;
	this->tail_ = that.tail_;
	that.tail_ = INVALID_NODE;

	this->size_ = that.size_;
	that.size_ = 0;

	this->count_ = that.count_;
	that.count_ = 0;

	this->nodes_ = that.nodes_;
	that.nodes_ = nullptr;

	return *this;
}

template <typename TKey, typename TVal>
cache<TKey, TVal>::~cache()
{
	delete[] nodes_;
}

template <typename TKey, typename TVal>
std::size_t cache<TKey, TVal>::get_index(const TKey& key)
{
	for (std::size_t i = 0; i < count_; ++i)
	{
		if (nodes_[i].key == key)
			return i;
	}
	return INVALID_NODE;
}

template <typename TKey, typename TVal>
void cache<TKey, TVal>::set_head(std::size_t index)
{
	auto node = nodes_[index];

	if (index == head_)
	{
		return;
	}
	if (index == tail_)
	{
		nodes_[node.left_link].right_link = INVALID_NODE;
		tail_ = node.left_link;

		nodes_[head_].left_link = index;

		node.left_link = INVALID_NODE;
		node.right_link = head_;

		head_ = index;
		return;
	}

	nodes_[node.left_link].right_link = node.right_link;
	nodes_[node.right_link].left_link = node.left_link;

	node.left_link = INVALID_NODE;
	node.right_link = head_;
	nodes_[index] = node;
	nodes_[head_].left_link = index;

	head_ = index;
}

#endif
