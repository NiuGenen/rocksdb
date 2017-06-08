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


} // namespace ocssd
} // namespace rocksdb
