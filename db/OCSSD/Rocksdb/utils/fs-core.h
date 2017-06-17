#ifndef YWJ_OCSSD_OC_TREE_H
#define YWJ_OCSSD_OC_TREE_H

//liblightnvm headers
#include "../oc_lnvm.h"

#include "common.h"
#include "oc_exception.hpp"
#include "../oc_page_cache.h"

#include "port/port.h"
#include <stdint.h>

namespace rocksdb {
namespace ocssd {


class oc_ssd;

namespace addr{

//1 channel 1 ext_tree
//for qemu:
// @ch is fixed to 0
// @stlun & @nluns is changable
// a channel contains multi ext_tree
//
//for real-device:
// @ch is changable
// @stlun & @nluns is fixed
// this ext_tree's lun range: [stlun, stlun + nluns - 1]
struct addr_meta {
    //fields for address-format 
    size_t ch;
    size_t stlun;
    size_t nluns;

    //other fields
};


/*
 * Block Address Format: >>>>>>>>default
 *
 *        |   ch   |  blk   |  pl   |   lun   |
 * length   _l[0]    _l[1]    _l[2]    _l[3]
 * idx      0        1        2        3
 */

/* 
 * Block Address Layout:(format 3)
 * 				 
 *  			 ext_tree_0						ext_tree_1					....
 * 			  |-----ch_0---------|			|-----ch_1---------|			....
 *  		  L0    L2    L3    L4			L0    L2    L3    L4	  
 * B0  p0     =     =     =     =		    =     =     =     =				Low ----> High
 *     p1     =     =     =     =           =     =     =     =				|   ---->
 * B1  p0     =     =     =     =		    =     =     =     =				|
 *     p1     =     =     =     =           =     =     =     =				|
 * B2  p0     =     =     =     =		    =     =     =     =				High
 *     p1     =     =     =     =           =     =     =     = 
 * B3  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * Bn  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * 			  BBT0  BBT1  BBT2  BBT3 		BBT0  BBT1  BBT2  BBT3
 */

struct blk_addr{
	uint64_t __buf;
	struct blk_addr& operator = (const struct blk_addr& rhs)
	{
		this->__buf = rhs.__buf;
		return *this;
	}
};

class blk_addr_handle{ // a handle should attach to a tree.
public:
	blk_addr_handle(struct nvm_geo const *g, struct addr_meta const *tm);
	~blk_addr_handle();

	enum{
		RetOK	 = 0,
		FieldCh  = 1,
		FieldLun = 2,
		FieldPl  = 3,
		FieldBlk = 4,
		FieldOOR = 5,		//Out of range

		AddrInvalid = 6,
		CalcOF = 7			//calculation: overflow/underflow occur
	};
	
	struct blk_addr get_lowest();
	struct blk_addr get_right_edge_addr(struct blk_addr *blk);
	void convert_2_nvm_addr(struct blk_addr *blk_a, struct nvm_addr *nvm_a);

	int MakeBlkAddr(size_t ch, size_t lun, size_t pl, size_t blk, struct blk_addr* addr);
	ssize_t GetFieldFromBlkAddr(struct blk_addr const * addr, int field, bool isidx);
	ssize_t SetFieldBlkAddr(size_t val, int field, struct blk_addr * addr, bool isidx);

	/*
	 * @blk_addr + @x 
	 * warning - @blk_addr will be change 
	 */
	void BlkAddrAdd(size_t x, struct blk_addr* addr);
	
	/*
	 * @blk_addr - @x 
	 * warning - @blk_addr will be change  
	 */
	void BlkAddrSub(size_t x, struct blk_addr* addr);
	
	/*
	 * return -1 if @lhs < @rhs; 
	 * 		  0  if @lhs == @rhs;
	 * 		  1  if @lhs > @rhs; 
	 */
	int BlkAddrCmp(const struct blk_addr* lhs, const struct blk_addr* rhs);

	bool CalcOK();
	
    /*
     * return @addr - @_lowest, if mode == 0; 
     * return @_highest - @addr, if mode == 1;
     */
    size_t BlkAddrDiff(const struct blk_addr* addr, int mode);

	/*
	 * valid value range: [0, field_limit_val).
	 * @ret - 0 - OK. 
	 *      - otherwise return value means corresponding field not valid.
	 */
	int BlkAddrValid(size_t ch, size_t lun, size_t pl, size_t blk);




	void PrInfo();
	void PrBlkAddr(struct blk_addr* addr, bool pr_title, const char *prefix);
	void PrTitleMask(struct blk_addr* addr);

private:

	struct AddrFormat {
		int ch;
		int lun;
		int pl;
		int blk;
		AddrFormat()
			: ch(0), lun(3), pl(2), blk(1)
		{
		}
	};

	void init();
	void do_sub_or_add(size_t x, struct blk_addr* addr, int mode);
	void do_sub(size_t* aos, size_t* v, struct blk_addr *addr);
	void do_add(size_t* aos, size_t* v, struct blk_addr *addr);

public:
	struct nvm_geo const * geo_;
private:
	struct addr_meta const *tm_; 
	int status_;						//status of last operation. 
	AddrFormat format_;

	struct blk_addr lowest;
	struct blk_addr highest;

	int lmov_[4];
	uint64_t mask_[4];
	size_t usize_[4];
};

inline struct blk_addr blk_addr_handle::get_lowest()
{
	return lowest;
}


extern addr_meta *am;
extern blk_addr_handle **bah;
extern size_t *next_start;
void addr_init(const nvm_geo *g) throw(ocssd::oc_excpetion);
void addr_release();
void addr_info();
void addr_next_addr_info();
void addr_nvm_addr_print(nvm_addr *naddr, bool pr_title, const char *prefix = "");
} // namespace addr




/* fs-logic */
//mba mngm
typedef struct mba_extent {
	size_t stblk;
	size_t edblk;
}mba_extent_t;

#define MBA_BM_LVLs 4
#define MBA_ACTIVE_BLK 4
#define MBA_ACTIVE_PAGE 1
typedef struct meta_block_area {
	const char *mba_name;
	int blk_count;
	int obj_size;
	int st_ch;                  //
	int ed_ch;

	mba_extent_t *occ_ext;      //each channel's blocks, length = [st_ch, ed_ch]
	nvm_addr *blk_addr_tbl;     //for real-time I/O's quick reference
	int *st_pg_id;				//for gc use

	int acblk_counts;           //active block
	int *acblk_id;
	rocksdb::port::Mutex acblk_id_lock;

	int pg_counts_p_acblk;      //active page per active block
	int *pg_id;

	oc_page **buffer; 			//

	rocksdb::port::Mutex pg_id_lock;

	oc_bitmap **bitmaps[MBA_BM_LVLs];   //one of <@counts>-bits-bitmap
										//<@acblk_counts> of <@geo->npages>-bits-bitmap
										//<acpg_counts_per_acblk * acblk_counts> of <pg_size/obj_size>-bitmap

										//<counts> of <blk_size/obj_size>-bits-bitmap

	
	void *obj_addr_tbl;                 //object address table or "oat"	(NAT)
	int obj_num_max;
}meta_block_area_t;

typedef struct mba_mnmg {
	list_wrapper *mba_list;
}mba_mnmg_t; 

void mba_mngm_init();
void mba_mngm_init_pgp(oc_page_pool *pgp_);
void mba_mngm_dump(meta_block_area_t *mbaptr);
void mba_mngm_release();
void mba_mngm_info();
meta_block_area_t*  mba_alloc(const char *name, int blkcts, int obj_size, int st_ch, int ed_ch, int obj_num_max);
void mba_info(meta_block_area_t *mbaptr); 

void mba_pr_blk_addr_tbl(meta_block_area_t *mbaptr);

void mba_bitmaps_info(meta_block_area_t *mbaptr);
void mba_pr_bitmaps(meta_block_area_t *mbaptr);

inline void mba_set_oat(meta_block_area_t *mbaptr, void *oat)
{
	mbaptr->obj_addr_tbl = oat;
}

//file_meta implementation
typedef uint32_t file_meta_number_t;
#define FILE_META_NAME "FileMeta"
#define FILE_META_BLK_CTS 300
#define FILE_META_OBJ_SIZE 1024
#define FILE_META_OBJ_CTS 2000000
extern meta_block_area_t *file_meta_mba;

inline file_meta_number_t* _file_meta_mba_get_oat()
{
	return reinterpret_cast<file_meta_number_t *>(file_meta_mba->obj_addr_tbl);
}




///
struct extent{
	uint64_t addr_st_buf;
	uint64_t addr_ed_buf;
	uint32_t free_bitmap;
	uint32_t junk_bitmap;
}__attribute__((aligned(8)));		//24B




struct ext_meta_obj{
	uint32_t free_vblk_num;
	uint16_t obj_id;
	uint16_t reserve;
}__attribute__((aligned(8)));		//8B

#define Node_ID_Type uint32_t
#define Ext_Node_ID_Type Node_ID_Type
#define Ext_Leaf_Node_Degree 127
#define Ext_Non_Leaf_Node_Degree 145
#define Ext_Non_Leaf_Node_Degree_Meta 435 //MUST be {3 * Ext_Non_Leaf_Node_Degree}

struct ext_leaf_node{
	uint16_t el_2_mobj_num; 	//ext_len_2_meta_obj_num
	uint16_t el_8_mobj_num; 
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;

	uint32_t free_vblk_sum_2;	//free_vblock_number of extent_len = 2
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;

	struct ext_meta_obj mobjs[Ext_Leaf_Node_Degree];
	struct extent exts[Ext_Leaf_Node_Degree];
	
	Ext_Node_ID_Type father_id;
	uint32_t node_type;			//0
}__attribute__((aligned(8)));	//4096B

struct ext_non_leaf_node{
	uint16_t el_2_mobj_num; 	//ext_len_2_meta_obj_num
	uint16_t el_8_mobj_num; 
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;

	uint32_t free_vblk_sum_2;	//free_vblock_number of extent_len = 2
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;

	struct ext_meta_obj mobjs[Ext_Non_Leaf_Node_Degree_Meta];
	Ext_Node_ID_Type node_id_array[Ext_Non_Leaf_Node_Degree]; //a node id is 4Bytes
	
	uint32_t reserve3[1];
	uint32_t father_id;
	uint32_t node_type;			//1
}__attribute__((aligned(8)));	//4096B

#define Ext_Des_ID_Type uint64_t //8bits for offset of ext_des_array, 56bits for dir_ext_node_id
#define Nat_Node_ID_Type uint64_t
#define Dir_Node_ID_Type Node_ID_Type


struct ext_descriptor{
	uint64_t addr_st_buf;
	uint64_t addr_ed_buf;
	uint32_t use_bitmap;
	uint32_t reserve;
	Ext_Des_ID_Type next;
}__attribute__((aligned(8)));


#define Dir_Ext_Node_Degree 127
#define Dir_Leaf_Node_Degree 30

struct dir_ext_node{
	struct ext_descriptor ext_des_array[Dir_Ext_Node_Degree];
	Ext_Des_ID_Type next_dir_ext_node_id;
	uint32_t reserve[6];
}__attribute__((aligned(8)));




struct nat_entry{
	Node_ID_Type id;
	uint8_t dead;
	uint8_t reserve[3];
}__attribute__((aligned(8)));

#define Nat_Node_Degree 511
struct nat_node{
	struct nat_entry entries[Nat_Node_Degree];
	Nat_Node_ID_Type next_nat_node_id;
}__attribute__((aligned(8)));;

#define Nat_Entry_ID_Type uint32_t


#define File_Name_Length 128
struct file_descriptor{
	uint8_t file_name[File_Name_Length];
}__attribute__((aligned(8)));

struct dir_leaf_node_entry{
	struct file_descriptor fdes;
	Ext_Des_ID_Type f_first_ext_id;
}__attribute__((aligned(8)));

struct dir_leaf_node{
	struct dir_leaf_node_entry entries[Dir_Leaf_Node_Degree];
	uint32_t reserve[3];
	Nat_Entry_ID_Type father_id;
}__attribute__((aligned(8)));

struct dir_non_leaf_node{
	struct file_descriptor fdes_array[Dir_Leaf_Node_Degree];
	Nat_Entry_ID_Type nat_entries[Dir_Leaf_Node_Degree];
	uint32_t reserve[3];
	Nat_Entry_ID_Type father_id;
}__attribute__((aligned(8)));


} // namespace ocssd
} // namespace rocksdb
#endif
