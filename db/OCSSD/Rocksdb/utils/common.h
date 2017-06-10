#ifndef YWJ_OCSSD_COMMON_H
#define YWJ_OCSSD_COMMON_H

#include "oc_exception.hpp"

#include <string>

namespace rocksdb {
namespace ocssd {

#define BOOL2STR(b) ((b) ? "true" : "false")
void StrAppendTime(std::string& str);
std::string& StrAppendInt(std::string& str, int val);

#define NVM_MIN(x, y) ({                \
        __typeof__(x) _min1 = (x);      \
        __typeof__(y) _min2 = (y);      \
        (void) (&_min1 == &_min2);      \
        _min1 < _min2 ? _min1 : _min2; })

#define NVM_MAX(x, y) ({                \
        __typeof__(x) _min1 = (x);      \
        __typeof__(y) _min2 = (y);      \
        (void) (&_min1 == &_min2);      \
        _min1 > _min2 ? _min1 : _min2; })



// get binary mask of @x
template<typename XType>
int GetBinMaskBits(XType x)
{
	XType i = 1;
	int bits = 0;
	while(i < x){
		i = i << 1;
		bits++;
	}
	return bits;
}

// if x is in range of [a, b]
template<typename XType>
inline bool InRange(XType x, XType a, XType b)
{
	return x >= a && x <= b ? true : false;
}

template<typename XType>
const char* BinStr(XType num, std::string& str)
{
	XType mask = 1;
	int binlen = sizeof(num) * 8;
	binlen--;
	str.clear();
	while(binlen >= 0){
		str.append(1, num & (mask << binlen) ? '1' : '0');
		binlen--;
	}
	return str.c_str();
}


inline int bitmap2str(uint32_t bitmap, char *buf) //this is reversed string.
{
	for (int i = 0; i < 32; i++) {
		buf[32 - i - 1] = bitmap & (1 << i) ? '#' : '0';
	}
	buf[32] = '\0';
	return 0;
}


inline bool bitmap_is_all_set(uint32_t bitmap)
{
	return ~bitmap == 0; //if true , this bitmap is FULL.
}

/*
 * gaudent algorithm
 * require 	- only 1 bit of @x is 1. 
 * return 	- the index of this bit.
 * e.g.
 * 0x00000400 --> 10
 */
inline int bitmap_ntz32(uint32_t x)
{
	int b4, b3, b2, b1, b0;
	b4 = (x & 0x0000ffff) ? 0 : 16;
	b3 = (x & 0x00ff00ff) ? 0 : 8;
	b2 = (x & 0x0f0f0f0f) ? 0 : 4;
	b1 = (x & 0x33333333) ? 0 : 2;
	b0 = (x & 0x55555555) ? 0 : 1;
	return b0 + b1 + b2 + b3 + b4;
}

/*
 * get an empty slot(bit == 0) to use 
 *  
 * numerical value 	|2^31                 2^0| 
 * digit bit        |high                 low|  math_digit
 * varieble         |-- -- -- ...........  --|  32bits uint32_t
 * slot_id          |31					    0|
 */
inline int bitmap_get_slot(uint32_t bitmap)
{
	int b4, b3, b2, b1, b0;
	uint32_t x;

	x = bitmap | (bitmap + 1); //first bit 0 ==> 1
	x = x & (~bitmap);

	b4 = (x & 0x0000ffff) ? 0 : 16;
	b3 = (x & 0x00ff00ff) ? 0 : 8;
	b2 = (x & 0x0f0f0f0f) ? 0 : 4;
	b1 = (x & 0x33333333) ? 0 : 2;
	b0 = (x & 0x55555555) ? 0 : 1;

	return b0 + b1 + b2 + b3 + b4;
}

inline void bitmap_set(uint32_t *bitmap, int idx)
{
	*bitmap = *bitmap | (1 << idx);
}

inline void bitmap_unset(uint32_t *bitmap, int idx)
{
	*bitmap = *bitmap & (~(1 << idx));
}


/*
 * slot bit == 0: empty(unset), usable 
 *          == 1: set, not usable
 * buf index: 0                           1
 * 			 |-- -- -- ...........  --|  |-- -- -- ...........  --|
 * 			 |31				     0|  |63                    32|
 */
class oc_bitmap {
public:
	oc_bitmap(int counts) throw(oc_excpetion);
	int get_slot();									//find an empty slot_id to use.
	int set_slot(int slot_id);						//
	int unset_slot(int slot_id);
	bool slot_empty(int slot_id);

	void info();									//print info
	void printbuf();								//
private:
	int bits;
	int used_bits;
	int buf_len;
	uint32_t *buf;
};

inline void oc_bitmap::info()
{
	printf("usage: %d/%d\n", used_bits, bits);
}


//list
struct list_wrapper {
	void *ct_;
	list_wrapper *prev_;
	list_wrapper *next_;
	list_wrapper() : ct_(NULL), prev_(this), next_(this) { }
	list_wrapper(void *ct) : ct_(ct), prev_(this), next_(this) { }
};

//an entry pool to prevent memory leak.
class list_entry_pool {
public:
	list_entry_pool();
	list_entry_pool(int initnum);
	~list_entry_pool();

	list_wrapper* alloc();
	void dealloc(list_wrapper *);

private:
	list_wrapper freelist_;
	int num_;
	int used_;
};

inline list_wrapper* list_insert(list_wrapper *previous, list_wrapper *ptr)
{
	ptr->next_ = previous->next_;
	previous->next_ = ptr;

	ptr->next_->prev_ = ptr;
	ptr->prev_ = previous;
	return ptr;
}


list_wrapper* list_push_back(list_wrapper *listhead, list_wrapper *ptr)
{
	list_wrapper *prev = listhead->prev_; //the end of list
	return list_insert(prev, ptr);
}

inline list_wrapper* list_remove_entry(list_wrapper *ptr)
{
	ptr->prev_->next_ = ptr->next_;
	ptr->next_->prev_ = ptr->prev_;
	return ptr;
}

//Wrapper Get Content
template<class ContentType>
inline ContentType* list_entry_CTPtr(list_wrapper *ptr)
{
	return reinterpret_cast<ContentType *>(ptr->ct_);
}

list_wrapper* list_pop_back(list_wrapper *listhead)
{
	list_wrapper *end = listhead->prev_; //the end of list
	return end == listhead ? NULL : list_remove_entry(end);
}


template<class ContentType>
inline void list_entry_set_ct(list_wrapper *ptr, ContentType *ct)
{
	ptr->ct_ = reinterpret_cast<void *>(ct);
}

template<class ContentType>
inline list_wrapper* alloc_list_entry(ContentType *ct)
{
	return new list_wrapper(reinterpret_cast<void *>(ct));
}


/*
 * alloc list_entry from a "list_entry pool"
 */
template<class ContentType>
list_wrapper* alloc_list_entry_from_pool(ContentType *ct, list_entry_pool *pool)
{
	list_wrapper *newone = pool->alloc();
	list_entry_set_ct<ContentType>(newone, ct);
	return newone;
}



/*
 * INFO - remove from the original list, and give back to pool
 * Require - @ptr is generate by @alloc_list_entry_from_pool<ContentType>
 *
 */
list_wrapper* list_remove_entry_to_pool(list_wrapper *ptr, list_entry_pool *pool)
{
	ptr->prev_->next_ = ptr->next_;
	ptr->next_->prev_ = ptr->prev_;

	pool->dealloc(ptr); //this will let the removed node insert to the "@freelist_ of @pool"

	return ptr;
}


template<class LentryDoer>
void list_traverse(list_wrapper *listhead, LentryDoer *handlerptr)
{
	list_wrapper *lentry = listhead->next_, *tmp;
	while (lentry != listhead) {
		tmp = lentry->next_;
		(*handlerptr)(lentry);
		lentry = tmp;
	}
}

template<class LentryDoer>
void list_traverse(list_wrapper *listhead, LentryDoer *handlerptr, int i, int *num)
{
	list_wrapper *lentry = listhead->next_, *tmp;
	while (lentry != listhead) {
		tmp = lentry->next_;
		(*handlerptr)(lentry, i, num);
		lentry = tmp;
	}
}

struct list_wrapper_deleter {
	void operator ()(list_wrapper *lentry)
	{
		delete lentry;
	}
};


void list_destroy(list_wrapper *listhead)
{
	list_wrapper_deleter del;
	list_traverse<list_wrapper_deleter>(listhead, &del);
}


struct list_wrapper_deleter_to_pool {
	list_entry_pool *p;
	list_wrapper_deleter_to_pool(list_entry_pool *_p) : p(_p) { }
	void operator ()(list_wrapper *lentry)
	{
		list_remove_entry_to_pool(lentry, p);
	}
};

void list_destroy_to_pool(list_wrapper *listhead, list_entry_pool *pool)
{
	list_wrapper_deleter_to_pool del(pool);
	list_traverse<list_wrapper_deleter_to_pool>(listhead, &del);
}

list_entry_pool::list_entry_pool() : used_(0)
{
	num_ = 10;
	for (int i = 0; i < num_; ++i) {
		list_push_back(&freelist_, new list_wrapper());
	}
}

list_entry_pool::list_entry_pool(int initnum) 
: num_(initnum), used_(0)
{
	for (int i = 0; i < num_; ++i) {
		list_push_back(&freelist_, new list_wrapper());
	}
}

list_entry_pool::~list_entry_pool()
{
	list_destroy(&freelist_);
}
list_wrapper* list_entry_pool::alloc()
{
	if (num_ - used_ < 5) {
		int newnum = num_ * 2;
		int need = newnum - used_;

		for (int i = 0; i < need; ++i) {
			list_push_back(&freelist_, new list_wrapper());
		}

		num_ = newnum;
	}

	used_++;

	return list_pop_back(&freelist_);
}
void list_entry_pool::dealloc(list_wrapper *ptr)
{
	list_push_back(&freelist_, ptr);
	used_--;
}






} // namespace ocssd
} // namespace rocksdb

#endif
