#include "fs-core.h"

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

struct blk_addr blk_addr_handle::get_right_edge_addr(struct blk_addr *blk)
{
	struct blk_addr ret;
	ret.__buf = blk->__buf;
	
	int bval = (ret.__buf & mask_[format_.blk]) >> lmov_[format_.blk];
	SetFieldBlkAddr(bval + 1, format_.blk, &ret, true);
	return ret;
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


///global vars
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

void addr_info()
{
	printf("%zu bahs:\n", nchs);
	for (size_t i = 0; i < nchs; i++) {
		printf("bah_%zu \n", i );
		bah[i]->PrInfo();
	}
	printf("\n");
}

void addr_next_addr_info()
{
	printf("next_addr:\n"); 
	for (size_t i = 0; i < nchs; i++) {
		printf("%4zu ", i);
	}
	printf("\n");
	for (size_t i = 0; i < nchs; i++) {
		printf("%4zu ", next_start[i]);
	}
	printf("\n"); 
}

void addr_nvm_addr_print(nvm_addr *naddr,bool pr_title, const char* prefix)
{
	int i;
	size_t vals[5];
	const char *titles[5] = {
		"ch", "blk", "pl", "lun", "pg"
	};
	vals[0] = naddr->g.ch;
	vals[1] = naddr->g.blk;
	vals[2] = naddr->g.pl;
	vals[3] = naddr->g.lun;
	vals[4] = naddr->g.pg;

	
	if (pr_title) {
		printf("%5s", prefix); 
		for (i = 0; i < 5; i++) {
			printf("%5s", titles[i]);
		}
		printf("\n");
	}

	printf("%5s", prefix); 
	for (i = 0; i < 5; i++) {
		printf("%5zu", vals[i]);
	}
	printf("\n"); 
}

} // namespace addr

/*FS's logic*/
///global vars
static mba_mnmg_t mba_region;
static oc_page_pool *pgp;

///
meta_block_area_t *file_meta_mba;


void _mba_list_list_destroy();
void _mba_init_addr_tbl(meta_block_area_t *mbaptr);
void _mba_init(meta_block_area_t *mbaptr);
void _mba_release(meta_block_area_t *mbaptr);
void _mba_init_bitmaps(meta_block_area_t *mbaptr);
void _mba_get_acblks(meta_block_area_t *mbaptr, int idx);

/* 
 *  
 *  
 */
void mba_mngm_init()
{
	mba_region.mba_list = new list_wrapper();
}

void mba_mngm_init_pgp(oc_page_pool *pgp_)
{
	mba_region.mba_list = new list_wrapper();
	pgp = pgp_;
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

#define mba_mngm_info_print_blk_addr_tbl
#define mba_mngm_bitmaps_info
void mba_mngm_info()
{
	list_wrapper *ptr;
	printf("----mba mngm----\n");
	for (ptr = mba_region.mba_list->next_; ptr != mba_region.mba_list; ptr = ptr->next_) {
		mba_info(list_entry_CTPtr<meta_block_area_t>(ptr));
#ifdef mba_mngm_info_print_blk_addr_tbl
		mba_pr_blk_addr_tbl(list_entry_CTPtr<meta_block_area_t>(ptr));
#endif
#ifdef mba_mngm_bitmaps_info
		mba_bitmaps_info(list_entry_CTPtr<meta_block_area_t>(ptr));
#endif
	}
}
#define __init_debug__


void _mba_init_addr_tbl(meta_block_area_t *mbaptr)
{
	size_t nluns = addr::bah[0]->geo_->nluns;
	addr::blk_addr *blkas, blkline;
	int dis_ch = mbaptr->ed_ch - mbaptr->st_ch + 1; // mbaptr->blk_count;
	int i, tblidx = 0;
	size_t *idx;
	int mbaidx;
	blkas = new addr::blk_addr [dis_ch];
	idx = new size_t[dis_ch];
#ifdef __init_debug__
	printf("----mba init blks----\n");
#endif

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch && tblidx < mbaptr->blk_count; i++) 
	{
		mbaidx = i - mbaptr->st_ch;
		idx[mbaidx] = mbaptr->occ_ext[mbaidx].stblk; 
	}

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch && tblidx < mbaptr->blk_count; i++) 
	{
		mbaidx = i - mbaptr->st_ch;
		if (idx[mbaidx] == mbaptr->occ_ext[mbaidx].stblk) {
			blkas[mbaidx] = addr::bah[i]->get_lowest();
			addr::bah[i]->BlkAddrAdd(idx[mbaidx], &blkas[mbaidx]);
		}
		blkline = addr::bah[i]->get_right_edge_addr(&blkas[mbaidx]);

		while (addr::bah[i]->BlkAddrCmp(&blkas[mbaidx], &blkline) < 0 
			   && idx[mbaidx] <= mbaptr->occ_ext[mbaidx].edblk) {
#ifdef __init_debug__
			if (tblidx == 0) {
				addr::bah[i]->PrBlkAddr(&blkas[mbaidx], true, "");
			} else {
				addr::bah[i]->PrBlkAddr(&blkas[mbaidx], false, ""); 
			}
#endif
			addr::bah[i]->convert_2_nvm_addr(&blkas[mbaidx], mbaptr->blk_addr_tbl + tblidx);

			addr::bah[i]->BlkAddrAdd(1, &blkas[mbaidx]);
			idx[mbaidx]++;
			tblidx++;
		}// while
	}// for

	delete[] idx;
	delete[] blkas;
}
void _mba_init_bitmaps(meta_block_area_t *mbaptr)
{
	int i;
	size_t pg_per_blk = addr::bah[0]->geo_->npages;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
	size_t objs_per_pg = addr::bah[0]->geo_->page_nbytes / mbaptr->obj_size;
	size_t objs_per_blk = objs_per_pg * pg_per_blk;

	//calloc things...
	mbaptr->bitmaps[0] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), 1);	                    //lvl 0 - 1 bit/blk  for all blocks.
	mbaptr->bitmaps[1] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), mbaptr->acblk_counts);	//lvl 1 - 1 bit/page for active blocks.
	mbaptr->bitmaps[2] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), ac_pg_sum);  			//lvl 2 - 1 bit/obj  for active pages.
																							
	mbaptr->bitmaps[3] = (oc_bitmap **)calloc(sizeof(oc_bitmap *), mbaptr->blk_count);      //lvl 4 - 1


	mbaptr->bitmaps[0][0] = new oc_bitmap(mbaptr->blk_count);//lvl 0 - 1 bit/blk  for all blocks.

	for (i = 0; i < mbaptr->acblk_counts; i++) {
		mbaptr->bitmaps[1][i] = new oc_bitmap(pg_per_blk);  //lvl 1 - 1 bit/page for active blocks.
	}
	for (i = 0; i < ac_pg_sum; i++) {
		mbaptr->bitmaps[2][i] = new oc_bitmap(objs_per_pg); //lvl 2 - 1 bit/obj  for active pages.
	}
	for (i = 0; i < mbaptr->blk_count; i++) {
		mbaptr->bitmaps[3][i] = new oc_bitmap(objs_per_blk);	//lvl 4 - 1 bit/obj  for junk bits for all objects.
	}
}

void _mba_release_bitmaps(meta_block_area_t *mbaptr)
{
	int i;
	size_t pg_per_blk = addr::bah[0]->geo_->npages;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
	size_t objs_per_pg = addr::bah[0]->geo_->page_nbytes / mbaptr->obj_size;
	size_t objs_per_blk = objs_per_pg * pg_per_blk;

	delete mbaptr->bitmaps[0][0];
	for (i = 0; i < mbaptr->acblk_counts; i++) {
		delete mbaptr->bitmaps[1][i];  //lvl 1 - 1 bit/page for active blocks.
	}
	for (i = 0; i < ac_pg_sum; i++) {
		delete mbaptr->bitmaps[2][i]; //lvl 2 - 1 bit/obj  for active pages.
	}
	for (i = 0; i < mbaptr->blk_count; i++) {
		delete mbaptr->bitmaps[3][i];	//lvl 4 - 1 bit/obj  for junk bits for all objects.
	}
	for (int i = 0 ; i < MBA_BM_LVLs; i++) {
		free(mbaptr->bitmaps[i]);
	}
}

void _mba_init_buffer_pgp(meta_block_area_t *mbaptr)
{
	mbaptr->buffer = (oc_page **)calloc(sizeof(oc_page*), mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk);
	for (int i = 0; i < mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk; i++) {
		pgp->page_alloc(&mbaptr->buffer[i]);
	}
}
void _mba_release_buffer_pgp(meta_block_area_t *mbaptr)
{
	for (int i = 0; i < mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk; i++) {
		pgp->page_dealloc(mbaptr->buffer[i]); 
	}
	free(mbaptr->buffer);
}


/*
 * this will setup the blocks layout & other stuff.
 */
void _mba_init(meta_block_area_t *mbaptr)
{
	int dis_ch = mbaptr->ed_ch - mbaptr->st_ch + 1; //range: [mbaptr->ed_ch, mbaptr->st_ch]
	int blks = mbaptr->blk_count;
	int each_ch_blk = blks / dis_ch;
	int i, iblk;

	/* "extent" for mba-occupied-blocks per channel. */
	mbaptr->occ_ext = (mba_extent_t *)calloc(sizeof(mba_extent_t), dis_ch);

	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		iblk = i == mbaptr->ed_ch ? blks : each_ch_blk;

		mbaptr->occ_ext[i - mbaptr->st_ch].stblk = addr::next_start[i];
		mbaptr->occ_ext[i - mbaptr->st_ch].edblk = addr::next_start[i] + iblk - 1;

		addr::next_start[i] = addr::next_start[i] + iblk; //need lock ?
		blks -= iblk;
	}

	/* nvm_addr blk_addr_tbl for blocks's quick referencing */
	mbaptr->blk_addr_tbl = (nvm_addr *)calloc(sizeof(nvm_addr), mbaptr->blk_count);
	_mba_init_addr_tbl(mbaptr);


	/* active id array for blks & pgs */
	mbaptr->acblk_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts); 
	mbaptr->pg_id = (int *)calloc(sizeof(int), mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk);// [0, 1 * mbaptr->pg_counts_p_acblk),[,),...,[,)

	/* buffer */
	_mba_init_buffer_pgp(mbaptr);


	/* bitmaps */
	_mba_init_bitmaps(mbaptr);

	//obj addr tbl or oat
	mbaptr->obj_addr_tbl = NULL; //init by user.
}

void _mba_release(meta_block_area_t* mbaptr)
{
	int i,j;
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
	_mba_release_buffer_pgp(mbaptr);
	_mba_release_bitmaps(mbaptr);
}	

meta_block_area_t*  mba_alloc(const char *name,
	int blkcts, int obj_size, int st_ch, int ed_ch,int obj_num_max)
{
	meta_block_area_t* mba = (meta_block_area_t*)calloc(sizeof(meta_block_area_t), 1);
	mba->mba_name = name;
	mba->blk_count = blkcts;
	mba->obj_size = obj_size;
	mba->st_ch = st_ch;
	mba->ed_ch = ed_ch;
	mba->acblk_counts = MBA_ACTIVE_BLK;
	mba->pg_counts_p_acblk = MBA_ACTIVE_PAGE;
	mba->obj_num_max = obj_num_max;

	list_wrapper *mba_list_node = new list_wrapper(mba);
	list_push_back(mba_region.mba_list, mba_list_node);
	_mba_init(mba);
	return mba;
}

/* 
 *  
 *  
 */
void mba_info(meta_block_area_t* mbaptr)
{
	int i, mbaidx;
	printf("mba: %s\n", mbaptr->mba_name);
	printf("blocks: %d\n", mbaptr->blk_count);
	printf("blks layout:\n");

	printf("       "); 
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		printf("%4d ", i);
	}
	printf("\n");

	printf("start  ");
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		mbaidx = i - mbaptr->st_ch;
		printf("%4zu ", mbaptr->occ_ext[mbaidx].stblk); 
	}
	printf("\n");

	printf("ed     "); 
	for (i = mbaptr->st_ch; i <= mbaptr->ed_ch; i++) {
		mbaidx = i - mbaptr->st_ch;
		printf("%4zu ", mbaptr->occ_ext[mbaidx].edblk);
	}
	printf("\n");
}

void mba_pr_blk_addr_tbl(meta_block_area_t *mbaptr)
{
	printf("----blk_addr_tbl----\n");
	for (int i = 0; i < mbaptr->blk_count; i++) {
		if (i == 0) {
			addr::addr_nvm_addr_print(mbaptr->blk_addr_tbl + i, true, ""); 
		} else {
			addr::addr_nvm_addr_print(mbaptr->blk_addr_tbl + i, false, ""); 
		}
	}
}

void mba_bitmaps_info(meta_block_area_t *mbaptr)
{
	int bm_counts[4] = {
		1, 
		mbaptr->acblk_counts, 
		mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk,
		mbaptr->blk_count
	};

	for (int i = 0; i < 4; i++) {
		printf("LVL:%d (%d)\n", i, bm_counts[i]);
		for (int j = 0; j < bm_counts[i]; j++) {
			printf("--%d--\n", j);
			mbaptr->bitmaps[i][j]->info();
		}
	}
}

void mba_pr_bitmaps(meta_block_area_t *mbaptr)
{
}

typedef struct obj_idx{
	int acblk_id_idx;
	int pg_id_idx;
	int obj_idx;
} obj_idx_t;
///blk - pg - obj
void _mba_get_acblks_init(meta_block_area_t *mbaptr)
{
	int blkid;
	mbaptr->acblk_id_lock.Lock();
	for (int i = 0; i < mbaptr->acblk_counts; i++) {
		blkid = mbaptr->bitmaps[0][0]->get_slot();		
		mbaptr->acblk_id[i] = blkid;
		mbaptr->bitmaps[0][0]->set_slot(blkid);	//LVL 0

		//mbaptr->bitmaps[1][i]->unset_all();		//LVL 1
		_mba_get_pg_id_from_blk_id_init(mbaptr, i);
	}
	mbaptr->acblk_id_lock.Unlock();
}

void _mba_get_acblks_idx(meta_block_area_t *mbaptr, int idx)
{
	int i, id;
	mbaptr->acblk_id_lock.Lock();
	id = mbaptr->bitmaps[0][0]->get_slot();	//this will always ok.
	mbaptr->acblk_id[idx] = id;
	mbaptr->bitmaps[0][0]->set_slot(id);    //LVL 0
											
	mbaptr->bitmaps[1][idx]->unset_all();   //LVL 1
	_mba_get_pg_id_from_blk_id_init(mbaptr, idx);

	mbaptr->acblk_id_lock.Unlock();
}

void _mba_get_pg_id_from_blk_id_init(meta_block_area_t *mbaptr, int acblkidx)
{
	int i, pg_id;
	mbaptr->pg_id_lock.Lock();
	for (i = acblkidx * mbaptr->pg_counts_p_acblk; 
		  i < (acblkidx + 1) * mbaptr->pg_counts_p_acblk; 
		  i++) {
		pg_id = mbaptr->bitmaps[1][acblkidx]->get_slot();
		mbaptr->pg_id[i] = pg_id;
		mbaptr->bitmaps[1][acblkidx]->set_slot(pg_id);
		mbaptr->bitmaps[2][i]->unset_all(); //LVL 2
	}
	mbaptr->pg_id_lock.Unlock();
}

void _mba_get_pg_id_from_blk_id_idx(meta_block_area_t *mbaptr, int pgidx)
{
	int pg_id;
	int blk_idx = pgidx / mbaptr->pg_counts_p_acblk;
	mbaptr->pg_id_lock.Lock();
	pg_id = mbaptr->bitmaps[1][blk_idx]->get_slot();
	if (pg_id < 0) {
		mbaptr->pg_id_lock.Unlock();
		_mba_get_acblks_idx(mbaptr, blk_idx);
		return ;
	}
	mbaptr->pg_id[pgidx] = pg_id;
	mbaptr->bitmaps[1][blk_idx]->set_slot(pg_id);
	mbaptr->bitmaps[2][pgidx]->unset_all();
	mbaptr->pg_id_lock.Unlock();
}


void _mba_get_obj_id(meta_block_area_t *mbaptr, obj_idx_t *obj_id)
{
	int pg_idx, obj_idx;
	int ac_pg_sum = mbaptr->acblk_counts * mbaptr->pg_counts_p_acblk;
ALLOC_OBJ:
	for (pg_idx = 0 ; pg_idx < ac_pg_sum; pg_idx++) {
		obj_idx = mbaptr->bitmaps[2][pg_idx]->get_slot();
		if (obj_idx >= 0) {
			obj_id->acblk_id_idx = pg_idx / mbaptr->pg_counts_p_acblk; 
			obj_id->pg_id_idx = pg_idx;
			obj_id->obj_idx = obj_idx;
			return;
		}
	}
	//issue an I/O to dump all the active pages

	//get new active pages
	for (pg_idx = 0 ; pg_idx < ac_pg_sum; pg_idx++) {
		_mba_get_pg_id_from_blk_id_idx(mbaptr, pg_idx);
	}
	goto ALLOC_OBJ;
}




//file_meta implementation

void* _file_meta_mba_alloc_oat() throw(oc_excpetion)
{
	file_meta_number_t *oat = (file_meta_number_t *)calloc(sizeof(file_meta_number_t), FILE_META_OBJ_CTS); //<8MB
	if (!(*oat)) {
		throw (oc_excpetion("not enough memory", false));
	}
	return reinterpret_cast<void *>(oat);
}

void file_meta_init()
{	
	file_meta_mba = mba_alloc(FILE_META_NAME, FILE_META_BLK_CTS, FILE_META_OBJ_SIZE,
		0, addr::bah[0]->geo_->nchannels - 1, FILE_META_OBJ_CTS);
	mba_set_oat(file_meta_mba, _file_meta_mba_alloc_oat());
}

file_meta_number_t file_meta_alloc_obj_id()
{
}

void file_meta_write_by_obj_id()
{
}

void file_meta_read_by_obj_id()
{
}

void file_meta_info()
{
}


} // namespace ocssd
} // namespace rocksdb
