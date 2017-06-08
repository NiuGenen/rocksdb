#ifndef YWJ_OCSSD_OC_EXCEPTION_HPP
#define YWJ_OCSSD_OC_EXCEPTION_HPP

#include "oc_exception.hpp"

#include <string>
#include <cerrno>
#include <cstring>

namespace rocksdb {
namespace ocssd {

class oc_excpetion : public std::exception {
public:
	explicit oc_excpetion(const char *msg, bool errno_is_set = false) throw()
		: err_hint(msg)
	{
		if (errno_is_set) {
			err_hint += ":"; 
			err_hint += strerror(errno);
		}
	}

	virtual const char* what() const throw()
	{
		return err_hint.c_str(); 
	}
protected:
	std::string err_hint;
};

class dev_init_exception : public oc_excpetion {
public:
	explicit dev_init_exception(const char *msg) throw()
		: oc_excpetion(msg, true)
	{
	}
};

class bbt_operation_not_good : public oc_excpetion {
public:
	explicit bbt_operation_not_good(const char* msg) throw()
		: oc_excpetion(msg, true)
	{
	}
};

class erase_block_not_good : public oc_excpetion {
public:
	explicit erase_block_not_good(/*address*/) throw()
		: oc_excpetion(/*address_to_string*/"", true)
	{
	}
};

//IO not good

class page_pool_exception : public oc_excpetion {
public:
	explicit page_pool_exception(const char* msg) throw()
		: oc_excpetion(msg, true)
	{
	}
};

} // namespace ocssd
} // namespace rocksdb

#endif
