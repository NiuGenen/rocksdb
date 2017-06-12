#include "common.h"

#include <ctime>
#include <cstdio>
#include <string>
#include <cstdlib> //calloc()

namespace rocksdb {
namespace ocssd {

void StrAppendTime(std::string& str)
{
	char buf[32];
	time_t timep;
	struct tm *p;
	int ret;
	time(&timep);
	p = gmtime(&timep);
	str.clear();
	ret = snprintf(buf, 32, "%d/%d/%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday);
	str.append(buf);
	ret = snprintf(buf, 32, " %d:%d:%d", p->tm_hour, p->tm_min, p->tm_sec);
	str.append(buf);
}

std::string& StrAppendInt(std::string& str, int val)
{
	char buf[32];
	int ret;
	ret = snprintf(buf, 32, "%d", val);
	if (ret < 32) {
		str.append(buf);
	}
	return str;
}

//oc_bitmap implementation

oc_bitmap::oc_bitmap(int counts) throw(oc_excpetion)
	: bits(counts), used_bits(0)
{
	buf_len = ((bits >> 5) + 1); 
	buf = (uint32_t*)calloc(sizeof(uint32_t), buf_len);
	if (!buf) {
		throw (oc_excpetion("not enough memory", false));
	}
}


int oc_bitmap::get_slot()
{
	int i = 0, slot_id;
	while (i < buf_len && buf[i] == 0xffffffff) {
		i++;
	}
	if (i >= buf_len) {
		return -1;
	}

	slot_id = i << 5;
	slot_id += bitmap_get_slot(buf[i]);
	return slot_id;
}

int oc_bitmap::set_slot(int slot_id)
{
	int idx, inner_u32;
	if (slot_id < 0 || slot_id >= bits) { //valid range: [0, bits)
		return -1;
	}
	idx = slot_id >> 5;
	inner_u32 = slot_id - (idx << 5);
	bitmap_set(buf + idx, inner_u32);
	used_bits++;
	return 0;
}

int oc_bitmap::unset_slot(int slot_id)
{
	int idx, inner_u32;
	if (slot_id < 0 || slot_id >= bits) { //valid range: [0, bits)
		return -1;
	}
	idx = slot_id >> 5;
	inner_u32 = slot_id - (idx << 5);
	bitmap_unset(buf + idx, inner_u32);
	used_bits--;
	return 0;
}

bool oc_bitmap::slot_empty(int slot_id)
{
	int idx, inner_u32;
	if (slot_id < 0 || slot_id >= bits) { //valid range: [0, bits)
		return false;
	}
	idx = slot_id >> 5;
	inner_u32 = slot_id - (idx << 5);

	return (buf[idx] & (1 << inner_u32)) ? false : true ;

}
void oc_bitmap::printbuf()
{
	char tmp[33];
	for (int i = 0; i < buf_len; i++) {
		bitmap2str(buf[i], tmp);
		printf("%s", tmp);
	}
	printf("\n");
}

list_wrapper* list_push_back(list_wrapper *listhead, list_wrapper *ptr)
{
	list_wrapper *prev = listhead->prev_; //the end of list
	return list_insert(prev, ptr);
}
list_wrapper* list_pop_back(list_wrapper *listhead)
{
	list_wrapper *end = listhead->prev_; //the end of list
	return end == listhead ? NULL : list_remove_entry(end);
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

void list_destroy(list_wrapper *listhead)
{
	list_wrapper_deleter del;
	list_traverse<list_wrapper_deleter>(listhead, &del);
}

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
