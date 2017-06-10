#include "fs-core.h"
#include "common.h"

#include <malloc.h>

namespace rocksdb {
namespace ocssd {

namespace addr{

blk_addr_handle::blk_addr_handle(struct nvm_geo const * g, struct addr_meta const * tm)
	: geo_(g), tm_(tm), status_(0), format_()
{
	init();
}

blk_addr_handle::~blk_addr_handle()
{
}

void blk_addr_handle::init()
{
	int _l[4];
	_l[format_.ch] = GetBinMaskBits<size_t>(geo_->nchannels);
	_l[format_.lun] = GetBinMaskBits<size_t>(geo_->nluns);
	_l[format_.pl] = GetBinMaskBits<size_t>(geo_->nplanes);
	_l[format_.blk] = GetBinMaskBits<size_t>(geo_->nblocks);

	usize_[format_.ch] = geo_->nchannels;
    usize_[format_.lun] = tm_->nluns;
    usize_[format_.pl] = geo_->nplanes;
	usize_[format_.blk] = geo_->nblocks;

	lmov_[0] = _l[1] + _l[2] + _l[3];
	lmov_[1] = _l[2] + _l[3];
	lmov_[2] = _l[3];
	lmov_[3] = 0;

	for (int i = 0; i < 4; i++) {
		mask_[i] = 1;
		mask_[i] = (mask_[i] << _l[i]) - 1;
		mask_[i] = mask_[i] << lmov_[i];
	}


	MakeBlkAddr(tm_->ch, tm_->stlun, 0, 0, &lowest);
	MakeBlkAddr(tm_->ch, tm_->stlun + tm_->nluns - 1, geo_->nplanes - 1, geo_->nblocks - 1, &highest); 
}

void blk_addr_handle::convert_2_nvm_addr(struct blk_addr *blk_a, struct nvm_addr *nvm_a)
{
	uint64_t tmp[4];
	for (int idx = 0; idx < 4; idx++) {
		tmp[idx] = (blk_a->__buf & mask_[idx]) >> lmov_[idx];
	}
	nvm_a->g.ch = tmp[format_.ch];
	nvm_a->g.lun = tmp[format_.lun]; 
	nvm_a->g.pl = tmp[format_.pl];
	nvm_a->g.blk = tmp[format_.blk]; 
}
int blk_addr_handle::MakeBlkAddr(size_t ch, 
	size_t lun, 
	size_t pl, 
	size_t blk, 
	struct blk_addr* addr)
{
	uint64_t tmp[4];
	int ret = BlkAddrValid(ch, lun, pl, blk); ///Warning - @ch must be same as ext_meta.ch 
	if(ret){
		return ret;
	}
	
	tmp[format_.ch] = ch; 
	tmp[format_.lun] = lun; 
	tmp[format_.pl] = pl; 
	tmp[format_.blk] = blk; 

	addr->__buf = 0;

	for(int i = 0; i < 4; i++){
		tmp[i] = tmp[i] << lmov_[i];
		addr->__buf |= tmp[i];
	}

    return RetOK;
}

ssize_t blk_addr_handle::GetFieldFromBlkAddr(struct blk_addr const *addr, int field, bool isidx)
{
	int idx;
	size_t value;
	if (isidx) {
		idx = field;
	} else {
		switch (field) {
		case blk_addr_handle::FieldCh:
			idx = format_.ch; break;
		case blk_addr_handle::FieldLun:
			idx = format_.lun; break;
		case blk_addr_handle::FieldPl:
			idx = format_.pl; break;
		case blk_addr_handle::FieldBlk:
			idx = format_.blk; break;
		default:
			return -FieldOOR;
		}
	}

	value = (addr->__buf & mask_[idx]) >> lmov_[idx];
	return value;
}
ssize_t blk_addr_handle::SetFieldBlkAddr(size_t val, int field, struct blk_addr *addr, bool isidx)
{
	int idx;
	size_t org_value;

	if (isidx) {
		idx = field;
	} else {
		switch (field) {
		case blk_addr_handle::FieldCh:
			idx = format_.ch; break;
		case blk_addr_handle::FieldLun:
			idx = format_.lun; break;
		case blk_addr_handle::FieldPl:
			idx = format_.pl; break;
		case blk_addr_handle::FieldBlk:
			idx = format_.blk; break;
		default:
			return -FieldOOR;
		}
	}

	org_value = GetFieldFromBlkAddr(addr, idx, true);

	if (val > usize_[idx]) {
		return -AddrInvalid;
	}

	addr->__buf = addr->__buf & (~mask_[idx]);                              // clean
	addr->__buf = addr->__buf | (static_cast<uint64_t>(val) << lmov_[idx]); // set

	return org_value;                                               // return original value
}
/*
 * @blk_addr + @x 
 * warning - @blk_addr will be change 
 */
void blk_addr_handle::BlkAddrAdd(size_t x, struct blk_addr *addr)
{
	do_sub_or_add(x, addr, 1);
}

/*
 * @blk_addr - @x 
 * warning - @blk_addr will be change  
 */
void blk_addr_handle::BlkAddrSub(size_t x, struct blk_addr *addr)
{
	do_sub_or_add(x, addr, 0);
}

/*
 * return -1 if @lhs < @rhs; 
 * 		  0  if @lhs == @rhs;
 * 		  1  if @lhs > @rhs; 
 */
int blk_addr_handle::BlkAddrCmp(const struct blk_addr *lhs, const struct blk_addr *rhs)
{
	if (lhs->__buf == rhs->__buf) {
		return 0;
	}else if(lhs->__buf < rhs->__buf){
		return -1;
	}else{
		return 1;
	}
}

bool blk_addr_handle::CalcOK()
{
	return status_ == 0; 
}

/*
 * return @addr - @lowest, if mode == 0; 
 * return @highest - @addr, if mode == 1;
 */
size_t blk_addr_handle::BlkAddrDiff(const struct blk_addr *addr, int mode)
{
	const struct blk_addr *lhs = mode == 0 ? addr : &highest;
	const struct blk_addr *rhs = mode == 0 ? &lowest : addr;
	size_t diff = 0;
	size_t unit = 1;
	size_t v1[4], v2[4];

	// 0 -- 1 -- 2 -- 3
	// high --------- low
	for (int i = 3; i >= 0; i--) {
		v1[i] = static_cast<size_t>((lhs->__buf & mask_[i]) >> lmov_[i]);
		v2[i] = static_cast<size_t>((rhs->__buf & mask_[i]) >> lmov_[i]);
		diff += (v1[i] - v2[i]) * unit;
		unit = unit * usize_[i];
	}
	return diff;
}

/*
 * valid value range: [0, field_limit_val).
 * @ret - 0 - OK. 
 *      - otherwise return value means corresponding field not valid.
 */
int blk_addr_handle::BlkAddrValid(size_t ch, size_t lun, size_t pl, size_t blk)
{
	if (ch != tm_->ch)
		return -FieldCh;
	if (!ocssd::InRange<size_t>(lun, tm_->stlun, tm_->stlun + tm_->nluns - 1))
		return -FieldLun;
	if (!ocssd::InRange<size_t>(pl, 0, geo_->nplanes - 1))
		return -FieldPl;
	if (!ocssd::InRange<size_t>(blk, 0, geo_->nblocks - 1))
		return -FieldBlk;

	return RetOK;
}
/*
 *  do sub, if @mode == 0
 *  do add, if @mode == 1
 */
void blk_addr_handle::do_sub_or_add(size_t x, struct blk_addr *addr, int mode)
{
	size_t aos[4]; //add or sub
	size_t v[4];
	int i;
	if (x > BlkAddrDiff(addr, mode)) { //operation not good
		status_ = -CalcOF;	
		return ;
	}

	for (i = 3; i >= 0; i--) {
		aos[i] = x % usize_[i];
		x = x / usize_[i];
		v[i] = static_cast<size_t>((addr->__buf & mask_[i]) >> lmov_[i]);
	}
#if 0
    printf("[D1] ");
    for (i = 0; i < 4; i++) {
        printf("%zu ", v[i]);
    }
    printf("\n");
#endif
	if (mode == 0) {
		do_sub(aos, v, addr);
	} else {
		do_add(aos, v, addr);
	}
#if 0
    printf("[D2] ");
    for (i = 0; i < 4; i++) {
        printf("%zu ", v[i]);
    }
    printf("\n");
#endif
	addr->__buf = 0;
	for(i = 0; i < 4; i++){
		addr->__buf |= static_cast<uint64_t>(v[i]) << lmov_[i];
	}
}
void blk_addr_handle::do_sub(size_t *aos, size_t *v, struct blk_addr *addr)
{
	for (int i = 3; i >= 0; i--) {
		if (v[i] < aos[i]) {
			v[i - 1]--;
			v[i] = usize_[i] + v[i] - aos[i];
		} else {
			v[i] -= aos[i];
		}
	}
	v[format_.lun] += tm_->stlun;
}
void blk_addr_handle::do_add(size_t *aos, size_t *v, struct blk_addr *addr)
{
	for (int i = 3; i >= 0; i--) {
		v[i] += aos[i];
		if (v[i] >= usize_[i]) {
			v[i - 1]++;
			v[i] -= usize_[i];
		}
	}
	v[format_.lun] += tm_->stlun;
}


void blk_addr_handle::PrInfo()
{
	int i;
	const char *optitles[4];
	int _l[4];
	 _l[format_.ch] = GetBinMaskBits<size_t>(geo_->nchannels);
	 _l[format_.lun] = GetBinMaskBits<size_t>(geo_->nluns);
	 _l[format_.pl] = GetBinMaskBits<size_t>(geo_->nplanes);
	 _l[format_.blk] = GetBinMaskBits<size_t>(geo_->nblocks);

	optitles[format_.ch] = "ch";
	optitles[format_.lun] = "lun";
	optitles[format_.pl] = "pl";
	optitles[format_.blk] = "blk";

	printf("|----BlkAddr Handle Info----\n");
	printf("|masks's length:\n");
	printf("|  ");
	for(i = 0; i < 4; i++){
		printf("%3s ", optitles[i]);
	}
	printf("\n");
	printf("|  ");
	for(i = 0; i < 4; i++){
		printf("%3d ", _l[i]);
	}
	printf("\n");

	printf("|\n");
	printf("|tree's info:\n");
	PrBlkAddr(&lowest, true,   "|  Lowest: ");
	PrBlkAddr(&highest, false, "|  Highest:");
	printf("|----------------------------|\n");
}
void blk_addr_handle::PrBlkAddr(struct blk_addr *addr, bool pr_title, const char *prefix)
{
	size_t vals;
	const char *optitles[4];
	int i;

	optitles[format_.ch] = "ch";
	optitles[format_.lun] = "lun";
	optitles[format_.pl] = "pl";
	optitles[format_.blk] = "blk";

	if (pr_title) {
		printf("%s", prefix);
		for(i = 0; i < 4; i++){
			printf("%8s ", optitles[i]);
		}
		printf("\n");
	}
	printf("%s", prefix);
	for(i = 0; i < 4; i++){
		vals = GetFieldFromBlkAddr(addr, i, true);
		printf("%8zu ", vals);
	}
	printf("\n");
}
void blk_addr_handle::PrTitleMask(struct blk_addr *addr)
{
	const char *optitles[4];
	int i;
	std::string str1;

	optitles[format_.ch] = "ch";
	optitles[format_.lun] = "lun";
	optitles[format_.pl] = "pl";
	optitles[format_.blk] = "blk";
	for(i = 0; i < 4; i++){
		printf("%s|", optitles[i]);
	}
	printf("\n");
	for (i = 0; i < 4; i++){
		if (addr) {
			printf("%s %zu\n", BinStr<uint64_t>(mask_[i], str1), GetFieldFromBlkAddr(addr, i, true)); 
		} else {
			printf("%s\n", BinStr<uint64_t>(mask_[i], str1));
		}
	}
}


//
addr_meta *am;
blk_addr_handle **bah;
size_t *next_start;						//for channel(i): [0, next_start[i] - 1] is occupied by meta blocks.
rocksdb::port::Mutex next_start_lock;	//need lock?
static size_t nchs;

void addr_init(const nvm_geo *g) throw(ocssd::oc_excpetion)
{
	size_t i;
	nchs = g->nchannels;
	am = (addr_meta *)calloc(sizeof(addr_meta), nchs);
	if (!am) {
		throw (oc_excpetion("not enough memory", false));
	}

	next_start = (size_t *)calloc(sizeof(size_t), nchs);
	if (!next_start) {
		addr_release();
		throw (oc_excpetion("not enough memory", false)); 
	}

	bah = (blk_addr_handle **)calloc(sizeof(blk_addr_handle *), nchs);
	if (!bah) {
		addr_release();
		throw (oc_excpetion("not enough memory", false));
	}

	for (i = 0; i < nchs; i++) {
		am[i].ch = i;
		am[i].nluns = g->nluns;
		am[i].stlun = 0;

		next_start[i] = 0;

		try {
			bah[i] = new blk_addr_handle(g, am + i);
		} catch (std::bad_alloc&) {
			addr_release();
			throw (oc_excpetion("not enough memory", false));
		}
	}
}
void addr_release()
{
	if (am) {
		free(am);
	}
	for (size_t i = 0; i < nchs; i++) {
		if (bah[i]) {
			delete bah[i];
		}
	}
	if (bah) {
		free(bah);
	}
	if (next_start) {
		free(next_start);
	}
}


} // namespace addr

/*FS's logic*/
//mba mngm
typedef struct mba_extent{
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
	int st_ch;					//
	int ed_ch;

	mba_extent_t *occ_ext;			//each channel's blocks, length = [st_ch, ed_ch]							
	nvm_addr *blk_addr_tbl;         //for real-time I/O's quick reference


	int acblk_counts;           //active block
	int *acblk_id;
	int pg_counts_p_acblk;      //active page per active block
	int *pg_id;

	oc_bitmap *bitmaps[MBA_BM_LVLs];   //one of <@counts>-bits-bitmap
	//oc_bitmap *l1_bitmap_4_ac_blk;      //<@acblk_counts> of <@geo->npages>-bits-bitmap
	//oc_bitmap *l2_bitmap_4_ac_pg;       //<acpg_counts_per_acblk * acblk_counts> of <pg_size/obj_size>-bitmap

	//oc_bitmap *bitmap_4_obj_p_blk;      //<counts> of <blk_size/obj_size>-bits-bitmap

	void *obj_addr_tbl;						//object address table or "oat"	(NAT)
}meta_block_area_t; 

typedef struct mba_mnmg {
	list_wrapper *mba_list;
}mba_mnmg_t;

mba_mnmg_t mba_region;

/* 
 *  
 *  
 */
void mba_mngm_init()	//info
{
	mba_region.mba_list = new list_wrapper();
}

struct list_wrapper_MBA_deleter {
	void operator ()(list_wrapper *lentry)
	{
		meta_block_area_t *mbaptr = list_entry_CTPtr<meta_block_area_t>(lentry);
		if (mbaptr) {
			_mba_release(mbaptr);
			free(mbaptr);
		}
		delete lentry;
	}
};
void _mba_list_list_destroy()
{
	list_wrapper_MBA_deleter del;
	list_traverse<list_wrapper_MBA_deleter>(mba_region.mba_list, &del);
}

void mba_mngm_dump(meta_block_area_t* mbaptr)
{
	//this will dump all the meta_data into SUPERBLOCK.
}

/* 
 *  WARNING: Before release, one should need to dump(mba_mngm_dump) it first.
 *  
 */
void mba_mngm_release()	
{
	_mba_list_list_destroy();
	if (mba_region.mba_list) {
		delete mba_region.mba_list;
	}
}
/* 
 *  
 *  
 */
void mba_mngm_info()
{
	list_wrapper *ptr;
	for (ptr = mba_region.mba_list->next_; ptr != mba_region.mba_list; ptr = ptr->next_) {
		mba_info(list_entry_CTPtr<meta_block_area_t>(ptr));
	}
}


void _mba_init_addr_tbl(meta_block_area_t *mbaptr)
{

}


void _mba_init(meta_block_area_t *mbaptr) //this will setup the blocks layout & other stuff.
{
	int dis_ch = mbaptr->ed_ch - mbaptr->st_ch + 1; //range: [mbaptr->ed_ch, mbaptr->st_ch]
	int blks = mbaptr->blk_count;
	int each_ch_blk = blks / dis_ch;
	int i, iblk;

	mbaptr->occ_ext = (mba_extent_t *)calloc(sizeof(mba_extent_t), dis_ch);

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		iblk = i == mbaptr->ed_ch ? blks : each_ch_blk;

		mbaptr->occ_ext[i].stblk = next_start[i];
		mbaptr->occ_ext[i].edblk = next_start[i] + iblk - 1;

		next_start[i] = next_start[i] + iblk; //need lock ?
		blks -= iblk;
	}

	//nvm_addr blk_addr_tbl for blocks's quick referencing
	mbaptr->blk_addr_tbl = (nvm_addr *)calloc(sizeof(nvm_addr), mbaptr->blk_count);

	//active blks & pgs
	mbaptr->acblk_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts); 
	mbaptr->pg_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk); 


	//bitmaps
	mbaptr->bitmaps[0] =
	mbaptr->bitmaps[1]
	mbaptr->bitmaps[2]
	mbaptr->bitmaps[3]

	//obj addr tbl or oat
	

}

void _mba_release(meta_block_area_t* mbaptr)
{
	if (mbaptr->occ_ext) {
		free(mbaptr->occ_ext);
	}
	if (mbaptr->blk_addr_tbl) {
		free(mbaptr->blk_addr_tbl);
	}
	if (mbaptr->acblk_id) {
		free(mbaptr->acblk_id);
	}
	if (mbaptr->pg_id) {
		free(mbaptr->pg_id);
	}
	for (int i = 0 ; i < MBA_BM_LVLs; i++) {
		delete mbaptr->bitmaps[i];
	}
}

meta_block_area_t*  mba_alloc(const char *name,
	int blkcts, int obj_size, int st_ch, int ed_ch)
{
	meta_block_area_t* mba = (meta_block_area_t*)calloc(sizeof(meta_block_area_t), 1);
	mba->mba_name = name;
	mba->blk_count = blkcts;
	mba->obj_size = obj_size;
	mba->st_ch = st_ch;
	mba->ed_ch = ed_ch;
	mba->acblk_counts = MBA_ACTIVE_BLK;
	mba->pg_counts_p_acblk = MBA_ACTIVE_PAGE;

	list_wrapper *mba_list_node = new list_wrapper(mba);
	list_push_back(mba_region.mba_list, mba_list_node);
	_mba_init(mba);
}



/* 
 *  
 *  
 */
void mba_info(meta_block_area_t* mbaptr)
{
	int i;
	printf("mba: %s\n", mbaptr->mba_name);
	printf("blocks: %d\n", mbaptr->blk_count);
	printf("blks layout:\n");

	for (int i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		printf("%d ", i);
	}
	printf("\n");
}





void mba_set_acblk_counts(int acblk, int pgs_per_blk) // inline
{
}



//file_meta implementation

typedef uint32_t file_meta_number_t;
#define file_meta_block_counts 300
#define file_meta_size_bytes 1024


static meta_block_area_t oc_fs_file_meta;

void file_meta_init_oat(void** tblptr, uint32_t entry_counts) throw(oc_excpetion)
{
	*tblptr = calloc(sizeof(file_meta_number_t), entry_counts);
	if (!(*tblptr)) {
		throw (oc_excpetion("not enough memory", false));
	}
}

void file_meta_init_mba(meta_block_area_t* mbaptr, const nvm_geo *g, int st_ch, int ed_ch)
{

}

void file_meta_info()
{
}

void file_meta_usage()
{
}



} // namespace ocssd
} // namespace rocksdb
