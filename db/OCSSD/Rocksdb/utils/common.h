#ifndef YWJ_OCSSD_COMMON_H
#define YWJ_OCSSD_COMMON_H

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


inline int bitmap2str(uint32_t bitmap, char *buf)
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




} // namespace ocssd
} // namespace rocksdb

#endif
