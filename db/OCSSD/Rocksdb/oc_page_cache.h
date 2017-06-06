#ifndef YWJ_OCSSD_OC_PAGE_CACHE_H
#define YWJ_OCSSD_OC_PAGE_CACHE_H

//liblightnvm headers
#include "oc_lnvm.h"

#include "rocksdb/status.h"
#include "rocksdb/env.h"

//mutex & condition varieble headers
#include "port/port.h"



#include <queue>
#include <vector>

namespace rocksdb {
namespace ocssd {


class oc_ssd;
class oc_page;
class oc_file;


class oc_page_pool {
public:
	oc_page_pool(const struct nvm_geo *g);
	~oc_page_pool();

	//Page Operations collection
	void page_alloc(oc_page **pptr) throw(page_pool_exception);
	void page_dealloc(oc_page *p) throw();
	size_t page_append(oc_page *p, const char *str, size_t len);
	inline size_t page_leftbytes(oc_page *p);
	inline size_t page_content_len(oc_page *p);
	inline const char* page_content(oc_page *p);
	inline void page_clear(oc_page *p);

	//TESTS
	void TEST_Pr_Usage(const char *title);
	void TEST_Queue();
	void TEST_Basic();


private:

	struct p_entry {
		int id_;
		int usage_;
		uint32_t bitmap_;
		rocksdb::port::Mutex bm_lock_;
		 
		oc_page *reps[1];
	};

	struct p_entry_cmp {
		bool operator ()(p_entry *a, p_entry *b)
		{
			//printf("CALLED! %d %d, Ret:%d\n",a->usage_ , b->usage_, a->usage_ < b->usage_);
			return a->usage_ > b->usage_;
		}
	};

	friend class oc_page;
	
	p_entry* 	alloc_p_entry();
	void 		dealloc_p_entry(p_entry *pe);

	std::priority_queue<p_entry *, std::vector<p_entry *>, p_entry_cmp> pool_;
	rocksdb::port::Mutex pool_lock_;

	rocksdb::port::Mutex 		meta_lock_;
	int 						ID_;	
	const struct nvm_geo *const geo_;
	const size_t 				page_size_;
	const int 					degree_;
	rocksdb::Status 			s_;
};


class oc_page {
private:
	friend class oc_page_pool;
	friend class oc_buffer;
	

	oc_page_pool::p_entry *held_;
	void*		ptr_;
	void*		ofs_;
	int 		idx_;

	oc_page(void *mem, int i, oc_page_pool::p_entry *p);
	~oc_page();

	//no copy
	oc_page(oc_page const&);
	const oc_page operator=(oc_page const&);
};


inline size_t oc_page_pool::page_leftbytes(oc_page *p)
{
	return reinterpret_cast<char *>(p->ptr_) + page_size_ - reinterpret_cast<char *>(p->ofs_);
}

inline size_t oc_page_pool::page_content_len(oc_page *p)
{
	return reinterpret_cast<char *>(p->ofs_) - reinterpret_cast<char *>(p->ptr_);
}

inline const char* oc_page_pool::page_content(oc_page *p)
{
	return reinterpret_cast<const char *>(p->ptr_);
}

inline void oc_page_pool::page_clear(oc_page *p)
{
	p->ofs_ = p->ptr_;
}


/* 
 * a wrapper to oc_page. 
 * Support: I/O to oc_file, I/O to writablefile 
 * Not Support: compress
 */
class oc_buffer {
public:
	oc_buffer(oc_page_pool *p) : active_(NULL), page_pool_(p), size_(0) { }
	~oc_buffer();
	void append(const char *data, size_t len);
	void clear();
	
	void pr_pagesinfo();

	inline bool empty()
	{
		return size_ == 0;
	}
	inline size_t size()
	{
		return size_;
	}

	inline std::string toString(){	//return a string copy from the active page's content
		std::string str(page_pool_->page_content(active_)); 
		return str;
	}

	rocksdb::Status dump2file();
	rocksdb::Status dump2file(oc_file *f);
	rocksdb::Status dump2file(rocksdb::WritableFile *f);

	uint32_t CRCValue();

	//TESTS
	void TEST_Basic();
	void TEST_Large();
	void TEST_Large2();
	void TEST_WritableFile();
	void TEST_WritableFile_Clear_Again();
private:
	typedef std::vector<oc_page *>::iterator dump_iterator;
	void push_active();
	void cleanup();
	void update_pagesinfo();

	oc_page*				active_;
	oc_page_pool*			page_pool_;
	std::vector<oc_page *> 	todump_;
	size_t 					size_;
	std::string				info_;
};

} //namespace ocssd
} //namespace rocksdb


#endif
