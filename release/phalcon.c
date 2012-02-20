
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2012 Phalcon Team (http://www.phalconphp.com)       |
  +------------------------------------------------------------------------+
  | This source file is subject to the New BSD License that is bundled     |
  | with this package in the file docs/LICENSE.txt.                        |
  |                                                                        |
  | If you did not receive a copy of the license and are unable to         |
  | obtain it through the world-wide-web, please send an email             |
  | to license@phalconphp.com so we can send you a copy immediately.       |
  +------------------------------------------------------------------------+
  | Authors: Andres Gutierrez <andres@phalconphp.com>                      |
  |          Eduar Carvajal <eduar@phalconphp.com>                         |
  +------------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_phalcon.h"
#include "phalcon.h"

#include "zend_operators.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"

/** Main macros */
#define PHALCON_DEBUG 0

#define PHALCON_NOISY_FETCH 0
#define PHALCON_SILENT_FETCH 1
#define PHALCON_WRITE_FETCH 2

#define PHALCON_CALL_CHECK 1
#define PHALCON_CALL_DEFAULT 0

#define PHALCON_METHOD_CTOR 1
#define PHALCON_METHOD_NORMAL 0

#define PHALCON_INVOKE_THIS 1
#define PHALCON_INVOKE_OTHER 0

#define INIT_PARAM(x) ALLOC_ZVAL(x); INIT_PZVAL(x);

#if defined(HAVE_SPL) && ((PHP_MAJOR_VERSION > 5) || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1))
extern ZEND_API zend_class_entry *zend_ce_iterator;
extern ZEND_API zend_class_entry *zend_ce_arrayaccess;
extern PHPAPI zend_class_entry *spl_ce_RuntimeException;
extern PHPAPI zend_class_entry *spl_ce_Countable;
extern PHPAPI zend_class_entry *spl_ce_SeekableIterator;
#endif

/** Globals functions */
int phalcon_init_global(char *global TSRMLS_DC);
int phalcon_get_global(zval *arr, char *global, int global_type TSRMLS_DC);
int phalcon_get_global_by_index(char *global, char *index, zval *result TSRMLS_DC);

int phalcon_get_class_constant(zval *return_value, zend_class_entry *ce, char *constant_name, int constant_length TSRMLS_DC);

/** Logical functions */
int phalcon_and_function(zval *result, zval *left, zval *right);

/** Concat functions */
int phalcon_concat_right(zval *result, zval *op1, const char *op2 TSRMLS_DC);
int phalcon_concat_left(zval *result, const char *op1, zval *op2 TSRMLS_DC);
int phalcon_concat_both(zval *result, const char *op1, zval *op2, const char *op3 TSRMLS_DC);
int phalcon_concat_vboth(zval *result, zval *op1, const char *op2, zval *op3 TSRMLS_DC);

int phalcon_compare_strict_string(zval *op1, char *op2);

int phalcon_file_exists(zval *filename TSRMLS_DC);
extern long phalcon_count(const zval *value);

/** Low level filters */
int phalcon_filter_alphanum(zval **result, zval *param);

/** Memory macros */
#define PHALCON_ALLOC_ZVAL(z) \
	ALLOC_ZVAL(z); INIT_PZVAL(z);

/** Renew memory allocated on certain pointer doing copy-write if needed */
#define PHALCON_REALLOC_ZVAL(z) \
	if(Z_REFCOUNT_P(z)<=1){\
		FREE_ZVAL(z);\
		PHALCON_ALLOC_ZVAL(z);\
	} else {\
		SEPARATE_ZVAL(&z);\
	}\

/** Copy value on destiny making separation if needed  */
#define PHALCON_INIT_SEPARATE(destiny, value) \
	PHALCON_VAR_INIT(destiny);\
	ZVAL_ZVAL(destiny, value, 1, 0);\
	Z_DELREF_P(value);\
	Z_SET_REFCOUNT_P(destiny, 1);

/** Do/Renew memory allocation for zval used as a result */
#define PHALCON_RESULT_INIT(var) \
	if(!var){\
		PHALCON_ALLOC_ZVAL(var);\
	} else {\
		PHALCON_REALLOC_ZVAL(var);\
	}

/** Do/Renew memory allocation for zval used as a var */
#define PHALCON_VAR_INIT(var) \
	if(!var){\
		PHALCON_ALLOC_ZVAL(var);\
	} else {\
		PHALCON_REALLOC_ZVAL(var);\
	}

/** Do/Renew memory allocation for zval used only if data-type has changed */
#define PHALCON_TYPEVAR_INIT(var, type) \
	if(!var){\
		PHALCON_ALLOC_ZVAL(var);\
	} else {\
		if(Z_REFCOUNT_P(var)>1){\
			SEPARATE_ZVAL(&var);\
		} else {\
			if(Z_TYPE_P(var)!=type){\
				FREE_ZVAL(var);\
				PHALCON_ALLOC_ZVAL(var);\
			}\
		}\
	}

/** Do/Renew memory allocation for zval used only if data-type isn't long */
#define PHALCON_LVAR_INIT(var) PHALCON_TYPEVAR_INIT(var, IS_LONG);

/** Do/Renew memory allocation for zval used only on binary operations (expect data-type will not change) */
#define PHALCON_BRESULT_INIT(var) \
	if(!var){\
		PHALCON_ALLOC_ZVAL(var);\
	} else {\
		if(Z_REFCOUNT_P(var)>1){\
			SEPARATE_ZVAL(&var);\
		}\
	}

/** Do zval assignment doing separation on destiny or value if needed */
#define PHALCON_CPY_WRT(destiny, value) \
	if(!destiny||Z_REFCOUNT_P(value)==1){\
		Z_ADDREF_P(value);\
		destiny = value;\
	} else {\
		PHALCON_INIT_SEPARATE(destiny, value);\
	}

/** Frees memory if is posible */
#define PHALCON_FREE(v) if(v){\
	if(Z_REFCOUNT_P(v)<=1){\
		FREE_ZVAL(v);\
		v = NULL;\
	} else {\
		Z_DELREF_P(v);\
	}\
}

/** Frees and destroys asociated memory */
#define PHALCON_DFREE(v) if(v){\
	if(Z_REFCOUNT_P(v)<=1){\
		zval_ptr_dtor(&v);\
		FREE_ZVAL(v);\
		v = NULL;\
	} else {\
		Z_DELREF_P(v);\
	}\
}

#define PHALCON_DELREF(v) if(v){\
	Z_DELREF_P(v);\
}

/** Control flow macros */
#define FOREACH_KV(var, varcopy, slabel, elabel, arrval, pointer, key, value) \
			if(Z_TYPE_P(var)!=IS_ARRAY){\
               zend_error(E_WARNING, "Invalid argument supplied for foreach()");\
			} else {\
				if(!value){\
					PHALCON_ALLOC_ZVAL(value);\
				} else {\
					PHALCON_REALLOC_ZVAL(value);\
				}\
				PHALCON_ALLOC_ZVAL(varcopy);\
				ZVAL_ZVAL(varcopy, var, 1, 0);\
				Z_SET_REFCOUNT_P(varcopy, 1);\
				arrval = Z_ARRVAL_P(varcopy);\
				zend_hash_internal_pointer_reset_ex(arrval, &pointer);\
				slabel:\
				if(zend_hash_get_current_data_ex(arrval, (void**) &hd, &pointer) != SUCCESS){\
					goto elabel;\
				} else {\
					if(!key){\
						PHALCON_ALLOC_ZVAL(key);\
					} else {\
						PHALCON_REALLOC_ZVAL(key);\
					}\
					htype = zend_hash_get_current_key_ex(arrval, &index, &index_len, &num, 0, &pointer);\
					if(htype==HASH_KEY_IS_STRING){\
						ZVAL_STRING(key, index, 0);\
					} else {\
						if(htype==HASH_KEY_IS_LONG){\
							ZVAL_LONG(key, num);\
						}\
					}\
				}\
				*value = **hd;

#define FOREACH_V(var, varcopy, slabel, elabel, arrval, pointer, value) \
		if(Z_TYPE_P(var)!=IS_ARRAY){\
			zend_error(E_WARNING, "Invalid argument supplied for foreach()");\
		} else {\
			if(!value) PHALCON_ALLOC_ZVAL(value);\
			PHALCON_ALLOC_ZVAL(varcopy);\
			ZVAL_ZVAL(varcopy, var, 1, 0);\
			arrval = Z_ARRVAL_P(varcopy);\
			zend_hash_internal_pointer_reset_ex(arrval, &pointer);\
			slabel:\
			if(zend_hash_get_current_data_ex(arrval, (void**) &hd, &pointer) != SUCCESS){\
				goto elabel;\
			}\
			*value = **hd;

#define END_FOREACH(varcopy, slabel, elabel, arrval, pointer) zend_hash_move_forward_ex(arrval, &pointer);\
		goto slabel;\
		elabel:\
		Z_DELREF_P(varcopy);\
		PHALCON_FREE(varcopy);\
		}


/** Variables */
#define PHALCON_SET_LONG(var, vlong) PHALCON_LVAR_INIT(var); ZVAL_LONG(var, vlong)
#define PHALCON_SET_STRING(var, str, alloc) PHALCON_VAR_INIT(var); ZVAL_STRING(var, str, alloc)
#define PHALCON_SET_DOUBLE(var, vdouble) PHALCON_VAR_INIT(var); ZVAL_DOUBLE(var, vdouble)
#define PHALCON_INIT_NULL(var) PHALCON_VAR_INIT(var); ZVAL_NULL(var)
#define PHALCON_INIT_BOOL(var, bvalue) PHALCON_VAR_INIT(var); ZVAL_BOOL(var, bvalue)
#define PHALCON_INIT_FALSE(var) PHALCON_VAR_INIT(var); ZVAL_BOOL(var, 0)
#define PHALCON_INIT_TRUE(var) PHALCON_VAR_INIT(var); ZVAL_BOOL(var, 1)
#define PHALCON_INIT_ZVAL(var, value) PHALCON_VAR_INIT(var, value);
#define PHALCON_INIT_ARRAY(var) PHALCON_VAR_INIT(var); array_init(var)

/** Function and method params */
#define PHALCON_PARAM_STRING(param, str, cpy) INIT_PARAM(param); ZVAL_STRING(param, str, cpy)
#define PHALCON_PARAM_LONG(param, vlong) INIT_PARAM(param); ZVAL_LONG(param, vlong)
#define PHALCON_PARAM_DOUBLE(param, vdouble) INIT_PARAM(param); ZVAL_DOUBLE(param, vdouble)
#define PHALCON_PARAM_BOOL(var, bvalue) INIT_PARAM(var); ZVAL_BOOL(var, bvalue)
#define PHALCON_PARAM_NULL(var) INIT_PARAM(var); ZVAL_NULL(var)
#define PHALCON_PARAM_ZVAL(param, value) INIT_PARAM(param); ZVAL_ZVAL(param, value, 1, 0)
#define PHALCON_PARAM_CPY(param, value) Z_ADDREF_P(value); param = value;

/** Operators */
#define PHALCON_EQUAL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_equal_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_NOT_EQUAL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_not_equal_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_SMALLER_EQUAL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_smaller_or_equal_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_SMALLER_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_smaller_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_IDENTICAL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_identical_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_NOT_IDENTICAL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); is_not_identical_function(result, op1, op2 TSRMLS_CC)

#define PHALCON_ADD_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); add_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_SUB_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); sub_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_MUL_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); mul_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_DIV_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); div_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_MOD_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); mod_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_CONCAT_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); concat_function(result, op1, op2 TSRMLS_CC)

#define PHALCON_BITWISE_OR_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); bitwise_or_function(result, op1, op2 TSRMLS_CC)
#define PHALCON_BITWISE_AND_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); bitwise_and_function(result, op1, op2 TSRMLS_CC)

#define PHALCON_AND_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); phalcon_and_function(result, op1, op2)
#define PHALCON_OR_FUNCTION(result, op1, op2) PHALCON_BRESULT_INIT(result); ZVAL_BOOL(result, zend_is_true(op1) || zend_is_true(op2))

#define PHALCON_BOOLEAN_NOT_FUNCTION(result, op1) PHALCON_BRESULT_INIT(result); boolean_not_function(result, op1 TSRMLS_CC)

/** Constants */
#define PHALCON_GET_CONSTANT(var, name) PHALCON_VAR_INIT(var); zend_get_constant(name, strlen(name), var TSRMLS_CC)
#define PHALCON_GET_CLASS_CONSTANT(var, class_entry, name) PHALCON_VAR_INIT(var); phalcon_get_class_constant(var, class_entry, name, strlen(name) TSRMLS_CC)

/** Symbols */
#define PHALCON_READ_SYMBOL(var, auxarr, name) if(EG(active_symbol_table)){\
			if(zend_hash_find(EG(active_symbol_table), name, sizeof(name), (void **)  &auxarr)==SUCCESS){\
				var = *auxarr;\
			} else {\
				ZVAL_NULL(var);\
			}\
		} else {\
			ZVAL_NULL(var);\
		}

/** Globals */
#define PHALCON_GET_GLOBAL(varr, nglobal, glovar)	phalcon_init_global(nglobal TSRMLS_CC);\
		if(&EG(symbol_table)){\
			if(zend_hash_find(&EG(symbol_table), nglobal, sizeof(nglobal), (void **) &glovar)==SUCCESS){\
				if(Z_TYPE_PP(glovar)==IS_ARRAY){\
					varr = *glovar;\
				} else {\
					PHALCON_INIT_ARRAY(varr);\
				}\
			}\
		} else {\
			PHALCON_INIT_ARRAY(varr);\
		}

int phalcon_find_scope(zend_class_entry *ce, char *method_name TSRMLS_DC);
int phalcon_find_parent_scope(zend_class_entry *ce, char *active_class, char *method_name TSRMLS_DC);

int phalcon_call_func(zval *return_value, char *func_name, int noreturn TSRMLS_DC);
int phalcon_call_func_params(zval *return_value, char *func_name, int param_count, zval *params[], int noreturn TSRMLS_DC);

int phalcon_call_method(zval *return_value, zval *object, char *method_name, int check, int noreturn TSRMLS_DC);
int phalcon_call_method_params(zval *return_value, zval *object, char *method_name, int param_count, zval *params[], int check, int noreturn TSRMLS_DC);

int phalcon_call_parent_func(zval *return_value, zval *object, char *active_class, char *method_name, int noreturn TSRMLS_DC);
int phalcon_call_parent_func_params(zval *return_value, zval *object, char *active_class, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC);

int phalcon_call_self_func(zval *return_value, zval *object, char *method_name, int noreturn TSRMLS_DC);
int phalcon_call_self_func_params(zval *return_value, zval *object, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC);

int phalcon_call_static_func(zval *return_value, char *class_name, char *method_name, int noreturn TSRMLS_DC);
int phalcon_call_static_func_params(zval *return_value, char *class_name, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC);

int phalcon_is_callable_check_class(const char *name, int name_len, zend_fcall_info_cache *fcc, int *strict_class, char **error TSRMLS_DC);
int phalcon_is_callable_check_func(int check_flags, zval *callable, zend_fcall_info_cache *fcc, int strict_class, char **error TSRMLS_DC);

extern zend_bool phalcon_is_callable_ex(zval *callable, zval *object_ptr, uint check_flags, char **callable_name, int *callable_name_len, zend_fcall_info_cache *fcc, char **error TSRMLS_DC);
int phalcon_call_user_function(HashTable *function_table, zval **object_pp, zval *function_name, zval *retval_ptr, zend_uint param_count, zval *params[] TSRMLS_DC);
int phalcon_call_user_function_ex(HashTable *function_table, zval **object_pp, zval *function_name, zval **retval_ptr_ptr, zend_uint param_count, zval **params[], int no_separation, HashTable *symbol_table TSRMLS_DC);

#ifndef PHP_WIN32
int phalcon_call_function(zend_fcall_info *fci, zend_fcall_info_cache *fci_cache TSRMLS_DC);
#endif

#define PHALCON_CALL_FUNC(return_value, func_name) phalcon_call_func(return_value, func_name, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_FUNC_NORETURN(func_name) phalcon_call_func(NULL, func_name, 0 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_FUNC_PARAMS(return_value, func_name, param_count, params) PHALCON_RESULT_INIT(return_value); phalcon_call_func_params(return_value, func_name, param_count, params, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_FUNC_PARAMS_NORETURN(func_name, param_count, params) phalcon_call_func_params(NULL, func_name, param_count, params, 0 TSRMLS_CC); if(EG(exception)) return

#define PHALCON_CALL_METHOD(return_value, object, method_name, check) phalcon_call_method(return_value, object, method_name, check, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_METHOD_NORETURN(object, method_name, check) phalcon_call_method(NULL, object, method_name, check, 0 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_METHOD_PARAMS(return_value, object, method_name, param_count, params, check) phalcon_call_method_params(return_value, object, method_name, param_count, params, check, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_METHOD_PARAMS_NORETURN(object, method_name, param_count, params, check) phalcon_call_method_params(NULL, object, method_name, param_count, params, check, 0 TSRMLS_CC); if(EG(exception)) return

#define PHALCON_CALL_PARENT_PARAMS(return_value, object, active_class, method_name, param_count, params)  phalcon_call_parent_func_params(return_value, object, active_class, method_name, param_count, params, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_PARENT_PARAMS_NORETURN(object, active_class, method_name, param_count, params) phalcon_call_parent_func_params(NULL, object, active_class, method_name, param_count, params, 0 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_PARENT(return_value, object, active_class, method_name) phalcon_call_parent_func(return_value, object, active_class, method_name, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_PARENT_NORETURN(object, active_class, method_name) phalcon_call_parent_func(NULL, object, active_class, method_name, 0 TSRMLS_CC); if(EG(exception)) return;

#define PHALCON_CALL_SELF_PARAMS(return_value, object, method_name, param_count, params) phalcon_call_self_func_params(return_value, object, method_name, param_count, params, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_SELF_PARAMS_NORETURN(object, method_name, param_count, params) phalcon_call_self_func_params(NULL, object, method_name, param_count, params, 0 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_SELF(return_value, object, method_name) phalcon_call_self_func(return_value, object, method_name, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_SELF_NORETURN(object, method_name) phalcon_call_self_func(NULL, object, method_name, 0 TSRMLS_CC); if(EG(exception)) return;

#define PHALCON_CALL_STATIC_PARAMS(return_value, class_name, method_name, param_count, params) phalcon_call_static_func_params(return_value, class_name, method_name, param_count, params, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_STATIC_PARAMS_NORETURN(class_name, method_name, param_count, params) phalcon_call_static_func_params(NULL, class_name, method_name, param_count, params, 0 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_STATIC(return_value, class_name, method_name) phalcon_call_static_func(return_value, class_name, method_name, 1 TSRMLS_CC); if(EG(exception)) return
#define PHALCON_CALL_STATIC_NORETURN(class_name, method_name) phalcon_call_static_func(NULL, class_name, method_name, 0 TSRMLS_CC); if(EG(exception)) return

int phalcon_require(zval *require_path TSRMLS_DC);int phalcon_spprintf(char **message, int max_len, char *format, ...);

#ifndef PHALCON_RELEASE

#define PHV(v) phalcon_vdump(v)
#define PHPR(v) phalcon_print_r(v)

typedef struct _phalcon_debug_entry {
	char *class_name;
	char *method_name;
	int lineno;
	struct _phalcon_debug_entry *prev;
	struct _phalcon_debug_entry *next;
} phalcon_debug_entry;

int phalcon_start_debug();

int phalcon_print_r(zval *userval TSRMLS_DC);
int phalcon_vdump(zval *uservar TSRMLS_DC);
int phalcon_debug_assign(char *name, zval *value TSRMLS_DC);
int phalcon_vpdump(const zval **uservar TSRMLS_DC);
int phalcon_dump_ce(zend_class_entry *ce TSRMLS_DC);
int phalcon_class_debug(zval *val TSRMLS_DC);

int phalcon_debug_backtrace_internal();
int phalcon_debug_str(char *what, char *message);
int phalcon_debug_long(char *what, uint vlong);
int phalcon_debug_screen(char *message);

int phalcon_step_over(char *message);
int phalcon_step_into(char *message);
int phalcon_step_out(char *message);

int phalcon_step_into_entry(char *class_name, char *method_name, int lineno);
int phalcon_step_out_entry();

int phalcon_debug_method_call(zval *obj, char *method_name TSRMLS_DC);
int phalcon_debug_vdump(char *preffix, zval *value TSRMLS_DC);
int phalcon_debug_param(zval *param TSRMLS_DC);

int phalcon_error_space();
int phalcon_debug_space();

extern FILE *phalcon_log;
int phalcon_debug_trace;
extern phalcon_debug_entry *start;
extern phalcon_debug_entry *active;

#endif
#ifndef PHALCON_RELEASE

int phalcon_assert_class(zval *object, char *class_name TSRMLS_DC);

#endif//Reads properties
#define PHALCON_READ_PROPERTY_NOISY(result, object, property_name, property_length) PHALCON_VAR_INIT(result); phalcon_read_property(result, object, property_name, property_length, PHALCON_NOISY_FETCH TSRMLS_CC);
#define PHALCON_READ_PROPERTY_SILENT(result, object, property_name, property_length) PHALCON_VAR_INIT(result); phalcon_read_property(result, object, property_name, property_length, PHALCON_SILENT_FETCH TSRMLS_CC);

//New
#define PHALCON_NEW_STD(object) PHALCON_ALLOC_ZVAL(object); object_init(object)
#define PHALCON_NEW(object, class) PHALCON_ALLOC_ZVAL(object); object_init_ex(object, class)

//Instance of
#define PHALCON_INSTANCE_OF(result, var, class) if(Z_TYPE_P(var)!=IS_OBJECT){\
      zend_error(E_ERROR, "instanceof expects an object instance, constant given");\
    } else {\
      PHALCON_RESULT_INIT(result);\
      ZVAL_BOOL(result, instanceof_function(Z_OBJCE_P(var), class TSRMLS_CC));\
    }


int phalcon_instance_of(zval *result, const zval *object, const zend_class_entry *ce TSRMLS_DC);
int phalcon_class_exists(zval *return_value, zval *class_name_zval, zval *autoload_zval TSRMLS_DC);
int phalcon_method_exists(zval *object, zval *method_name TSRMLS_DC);
int phalcon_clone(zval **dst, zval *obj TSRMLS_DC);

int phalcon_isset_property(zval *object, char *property_name, int property_length TSRMLS_DC);
int phalcon_isset_property_zval(zval *object, zval *property TSRMLS_DC);

int phalcon_read_property(zval *result, zval *object, char *property_name, int property_length, int silent TSRMLS_DC);
int phalcon_read_property_zval(zval *result, zval *object, zval *property, int silent TSRMLS_DC);

int phalcon_update_property_long(zval *obj, char *property_name, int property_length, long value TSRMLS_DC);
int phalcon_update_property_string(zval *obj, char *property_name, int property_length, char *value TSRMLS_DC);
int phalcon_update_property_zval(zval *obj, char *property_name, int property_length, zval *value TSRMLS_DC);

#define PHALCON_ARRFETCH_NOISY(result, arr, index) PHALCON_RESULT_INIT(result); phalcon_array_fetch(result, arr, index, PHALCON_NOISY_FETCH TSRMLS_CC);
#define PHALCON_ARRFETCH_SILENT(result, arr, index) PHALCON_RESULT_INIT(result); phalcon_array_fetch(result, arr, index, PHALCON_SILENT_FETCH TSRMLS_CC);
#define PHALCON_ARRFETCH_LONG_NOISY(result, arr, lindex) PHALCON_RESULT_INIT(result); phalcon_array_fetch_long(result, arr, lindex, PHALCON_NOISY_FETCH TSRMLS_CC);
#define PHALCON_ARRFETCH_LONG_SILENT(result, arr, lindex) PHALCON_RESULT_INIT(result); phalcon_array_fetch_long(result, arr, lindex, PHALCON_SILENT_FETCH TSRMLS_CC);
#define PHALCON_ARRFETCH_STRING_NOISY(result, arr, sindex) PHALCON_RESULT_INIT(result); phalcon_array_fetch_string(result, arr, sindex, PHALCON_NOISY_FETCH TSRMLS_CC);
#define PHALCON_ARRFETCH_STRING_SILENT(result, arr, sindex) PHALCON_RESULT_INIT(result); phalcon_array_fetch_string(result, arr, sindex, PHALCON_SILENT_FETCH TSRMLS_CC);

#define PHALCON_ARRAPPEND(arr, value) \
	{\
		zval *copy; \
		ALLOC_ZVAL(copy); \
		ZVAL_ZVAL(copy, value, 1, 0); \
		phalcon_array_append(arr, copy TSRMLS_CC); \
	}

#define PHALCON_ARRUPDATE(arr, index, value) \
	{\
		zval *copy; \
		ALLOC_ZVAL(copy); \
		ZVAL_ZVAL(copy, value, 1, 0); \
		phalcon_array_update(arr, index, copy TSRMLS_CC); \
	}

int phalcon_array_isset(const zval *arr, zval *index);
int phalcon_array_isset_long(const zval *arr, ulong index);
int phalcon_array_isset_string(const zval *arr, char *index, uint index_length);

int phalcon_array_unset(zval *arr, zval *index);
int phalcon_array_unset_long(zval *arr, ulong index);
int phalcon_array_unset_string(zval *arr, char *index, uint index_length);

int phalcon_array_append(zval *arr, zval *value TSRMLS_DC);

int phalcon_array_update(zval *arr, zval *index, zval *value TSRMLS_DC);
int phalcon_array_update_string(zval *arr, char *index, uint index_length, zval *value TSRMLS_DC);
int phalcon_array_update_long(zval *arr, ulong index, zval *value TSRMLS_DC);

int phalcon_array_fetch(zval *return_value, const zval *arr, zval *index, int silent TSRMLS_DC);
int phalcon_array_fetch_string(zval *return_value, const zval *arr, char *index, uint index_length, int silent TSRMLS_DC);
int phalcon_array_fetch_long(zval *return_value, const zval *arr, ulong index, int silent TSRMLS_DC);

#ifdef HAVE_CONFIG_H
#endif


/**
 * Initilializes super global variables if doesn't
 */
int phalcon_init_global(char *global TSRMLS_DC){
	zend_bool jit_initialization = (PG(auto_globals_jit) && !PG(register_globals) && !PG(register_long_arrays));
	if(jit_initialization){
		return zend_is_auto_global(global, sizeof(global)-1 TSRMLS_CC);
	}
	return SUCCESS;
}

/**
 * Gets the global zval into PG macro
 */
int phalcon_get_global(zval *arr, char *global, int global_type TSRMLS_DC){
	zend_bool jit_initialization = (PG(auto_globals_jit) && !PG(register_globals) && !PG(register_long_arrays));
	if(jit_initialization){
		zend_is_auto_global(global, sizeof(global)-1 TSRMLS_CC);
	}
	arr = PG(http_globals)[global_type];	
	return SUCCESS;
}

/**
 * Get a super global zval by its name
 */
int phalcon_get_global_by_index(char *global, char *index, zval *result TSRMLS_DC){
	zval **global_vars;
	if(zend_hash_find(&EG(symbol_table), global, sizeof(global), (void **) &global_vars)==SUCCESS){
		if(Z_TYPE_PP(global_vars)==IS_ARRAY){
			return zend_hash_find(Z_ARRVAL_PP(global_vars), index, sizeof(index), (void **) &result);
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Can't find superglobal: %s", global);
		ZVAL_NULL(result);
	}
	return FAILURE;
}

/**
 * Reads class constant from string name and returns its value
 */
int phalcon_get_class_constant(zval *return_value, zend_class_entry *ce, char *constant_name, int constant_length TSRMLS_DC){

	zval **result_ptr;

	if(zend_hash_find(&ce->constants_table, constant_name, constant_length+1, (void **) &result_ptr) != SUCCESS){
		zend_error_noreturn(E_ERROR, "Undefined class constant '%s::%s'", ce->name, constant_name);
		return FAILURE;
	} else {
		*return_value  = **result_ptr;
	}

	return SUCCESS;
}

/**
 * Makes fast count on implicit array types
 */
long phalcon_count(const zval *value){
	if(Z_TYPE_P(value)==IS_ARRAY){
		return zend_hash_num_elements(Z_ARRVAL_P(value));
	} else {
		if(Z_TYPE_P(value)==IS_NULL){
			return 0;
		}
	}
	return 1;
}

/**
 * Performs logical AND function operator
 */
int phalcon_and_function(zval *result, zval *left, zval *right){
	int istrue = 1;
	istrue = zend_is_true(left);
	if(istrue){
		istrue = zend_is_true(right);
	}
	ZVAL_BOOL(result, istrue);
	return SUCCESS;
}

/**
 * Performs fast concat when right operator is char constant
 */
int phalcon_concat_right(zval *result, zval *op1, const char *op2 TSRMLS_DC){

	zval op1_copy;
	int use_copy1 = 0;

	int op2_length = strlen(op2);

	if(Z_TYPE_P(op1) != IS_STRING){
		zend_make_printable_zval(op1, &op1_copy, &use_copy1);
	}

	if(use_copy1){
		op1 = &op1_copy;
	}

	Z_STRLEN_P(result) = Z_STRLEN_P(op1) + op2_length;
	Z_STRVAL_P(result) = (char *) emalloc(Z_STRLEN_P(result) + 1);
	memcpy(Z_STRVAL_P(result), Z_STRVAL_P(op1), Z_STRLEN_P(op1));
	memcpy(Z_STRVAL_P(result)+Z_STRLEN_P(op1), op2, op2_length);
	Z_STRVAL_P(result)[Z_STRLEN_P(result)] = 0;
	Z_TYPE_P(result) = IS_STRING;

	if(use_copy1){
		zval_dtor(op1);
	}

	return SUCCESS;
}

/**
 * Performs fast concat when left operator is char constant
 */
int phalcon_concat_left(zval *result, const char *op1, zval *op2 TSRMLS_DC){

	zval op2_copy;
	int use_copy2 = 0;

	int op1_length = strlen(op1);

	if(Z_TYPE_P(op2) != IS_STRING){
		zend_make_printable_zval(op2, &op2_copy, &use_copy2);
	}

	if(use_copy2){
		op2 = &op2_copy;
	}

	Z_STRLEN_P(result) = Z_STRLEN_P(op2) + op1_length;
	Z_STRVAL_P(result) = (char *) emalloc(Z_STRLEN_P(result) + 1);
	memcpy(Z_STRVAL_P(result), op1, op1_length);
	memcpy(Z_STRVAL_P(result)+op1_length, Z_STRVAL_P(op2), Z_STRLEN_P(op2));
	Z_STRVAL_P(result)[Z_STRLEN_P(result)] = 0;
	Z_TYPE_P(result) = IS_STRING;

	if(use_copy2){
		zval_dtor(op2);
	}

	return SUCCESS;
}

/**
 * Performs fast concat when left and right operator are char constants
 */
int phalcon_concat_both(zval *result, const char *op1, zval *op2, const char *op3 TSRMLS_DC){

	zval op2_copy;
	int use_copy2 = 0;

	int op1_length = strlen(op1);
	int op3_length = strlen(op3);

	if(Z_TYPE_P(op2) != IS_STRING){
		zend_make_printable_zval(op2, &op2_copy, &use_copy2);
	}

	if(use_copy2){
		op2 = &op2_copy;
	}

	Z_STRLEN_P(result) = op1_length + Z_STRLEN_P(op2) + op3_length;
	Z_STRVAL_P(result) = (char *) emalloc(Z_STRLEN_P(result) + 1);
	memcpy(Z_STRVAL_P(result), op1, op1_length);
	memcpy(Z_STRVAL_P(result)+op1_length, Z_STRVAL_P(op2), Z_STRLEN_P(op2));
	memcpy(Z_STRVAL_P(result)+op1_length+Z_STRLEN_P(op2), op3, op3_length);
	Z_STRVAL_P(result)[Z_STRLEN_P(result)] = 0;
	Z_TYPE_P(result) = IS_STRING;

	if(use_copy2){
		zval_dtor(op2);
	}

	return SUCCESS;

}

/**
 * Performs fast concat when left and right operator are zvals
 */
int phalcon_concat_vboth(zval *result, zval *op1, const char *op2, zval *op3 TSRMLS_DC){

	zval op1_copy, op3_copy;
	int use_copy1 = 0, use_copy3 = 0;

	int op2_length = strlen(op2);

	if(Z_TYPE_P(op1) != IS_STRING){
		zend_make_printable_zval(op1, &op1_copy, &use_copy1);
	}

	if(Z_TYPE_P(op3) != IS_STRING){
		zend_make_printable_zval(op3, &op3_copy, &use_copy3);
	}

	if(use_copy1){
		op1 = &op1_copy;
	}

	if(use_copy3){
		op3 = &op3_copy;
	}

	Z_STRLEN_P(result) = Z_STRLEN_P(op1) + op2_length + Z_STRLEN_P(op3);
	Z_STRVAL_P(result) = (char *) emalloc(Z_STRLEN_P(result) + 1);
	memcpy(Z_STRVAL_P(result), Z_STRVAL_P(op1), Z_STRLEN_P(op1));
	memcpy(Z_STRVAL_P(result)+Z_STRLEN_P(op1), op2, op2_length);
	memcpy(Z_STRVAL_P(result)+Z_STRLEN_P(op1)+op2_length, Z_STRVAL_P(op3), Z_STRLEN_P(op3));
	Z_STRVAL_P(result)[Z_STRLEN_P(result)] = 0;
	Z_TYPE_P(result) = IS_STRING;

	if(use_copy1){
		zval_dtor(op1);
	}

	if(use_copy3){
		zval_dtor(op3);
	}

	return SUCCESS;

}

/**
 * Natural compare with string operator on right
 */
int phalcon_compare_strict_string(zval *op1, char *op2){

	int op2_length = strlen(op2);

	switch(Z_TYPE_P(op1)){
		case IS_STRING:
			if(!Z_STRLEN_P(op1)&&!op2_length){
				return 1;
			} else {
				return zend_binary_strcmp(Z_STRVAL_P(op1), Z_STRLEN_P(op1), op2, op2_length)==0;
			}
		case IS_NULL:
			return zend_binary_strcmp("", 0, op2, op2_length)==0;
		case IS_BOOL:
			if(!Z_BVAL_P(op1)){
				return zend_binary_strcmp("0", strlen("0"), op2, op2_length)==0;
			} else {
				return zend_binary_strcmp("1", strlen("1"), op2, op2_length)==0;
			}
	}

	return 0;
}


/**
 * Inmediate function resolution for addslaches function
 */
int phalcon_addslashes(zval *return_value, zval *param TSRMLS_DC){

	if(Z_TYPE_P(param)!=IS_STRING){
		return FAILURE;
	}

	//ZVAL_STRING(return_value, php_addslashes(Z_STRVAL_P(param), Z_STRLEN_P(param), &Z_STRLEN_P(return_value), 0 TSRMLS_CC), 0);

	return SUCCESS;

}

/**
 * Checks if a file exists
 *
 */
int phalcon_file_exists(zval *filename TSRMLS_DC){

	if(Z_TYPE_P(filename)!=IS_STRING){
		return FAILURE;
	}

	if(VCWD_ACCESS(Z_STRVAL_P(filename), F_OK)==0){
		return SUCCESS;
	}

	return FAILURE;
}

/**
 * Filter alphanum string
 */
int phalcon_filter_alphanum(zval **result, zval *param){

	int i, ch, alloc = 0;
	zval *tmp;
	char temp[255];

	if(Z_TYPE_P(param)==IS_STRING){
		for(i=0;i<Z_STRLEN_P(param)&&i<255;i++){
			ch = Z_STRVAL_P(param)[i];
			if((ch>96&&ch<123)||(ch>64&&ch<91)||(ch>47&&ch<58)){
				temp[alloc] = ch;
				alloc++;
			}
		}
	}

	ALLOC_INIT_ZVAL(tmp);
	if(alloc>0){
		Z_TYPE_P(tmp) = IS_STRING;
		Z_STRLEN_P(tmp) = alloc;
		Z_STRVAL_P(tmp) = (char *) emalloc(alloc+1);
		memcpy(Z_STRVAL_P(tmp), temp, alloc);
		Z_STRVAL_P(tmp)[Z_STRLEN_P(tmp)] = 0;
	} else {
		ZVAL_STRING(tmp, "", 1);
	}

	*result = tmp;

	return SUCCESS;
}
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2012 Phalcon Team (http://www.phalconphp.com)       |
  +------------------------------------------------------------------------+
  | This source file is subject to the New BSD License that is bundled     |
  | with this package in the file docs/LICENSE.txt.                        |
  |                                                                        |
  | If you did not receive a copy of the license and are unable to         |
  | obtain it through the world-wide-web, please send an email             |
  | to license@phalconphp.com so we can send you a copy immediately.       |
  +------------------------------------------------------------------------+
  | Authors: Andres Gutierrez <andres@phalconphp.com>                      |
  |          Eduar Carvajal <eduar@phalconphp.com>                         |
  +------------------------------------------------------------------------+

  +------------------------------------------------------------------------+
  | Zend Engine                                                            |
  +------------------------------------------------------------------------+
  | Copyright (c) 1998-2011 Zend Technologies Ltd. (http://www.zend.com)   |
  +------------------------------------------------------------------------+
  | This source file is subject to version 2.00 of the Zend license,       |
  | that is bundled with this package in the file LICENSE, and is          |
  | available through the world-wide-web at the following url:             |
  | http://www.zend.com/license/2_00.txt.                                  |
  | If you did not receive a copy of the Zend license and are unable to    |
  | obtain it through the world-wide-web, please send a note to            |
  | license@zend.com so we can mail you a copy immediately.                |
  +------------------------------------------------------------------------+
  | Authors: Andi Gutmans <andi@zend.com>                                  |
  |          Zeev Suraski <zeev@zend.com>                                  |
  |          Andrei Zmievski <andrei@php.net>                              |
  +------------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#endif


/**
 * Finds the correct scope to execute the function
 */
int phalcon_find_scope(zend_class_entry *ce, char *method_name TSRMLS_DC){
	const zend_function_entry *functions;
	int method_len = strlen(method_name);
	while(ce){
		if(ce->builtin_functions){
			functions = ce->builtin_functions;
			while(functions->fname){
				if(!zend_binary_strcasecmp(functions->fname, strlen(functions->fname), method_name, method_len)){
					EG(scope) = ce;
					return SUCCESS;
				}
				functions++;
			}
		}
		ce = ce->parent;
	}
	return FAILURE;
}

/**
 * Find function scope on parent classes
 */
int phalcon_find_parent_scope(zend_class_entry *ce, char *active_class, char *method_name TSRMLS_DC){
	const zend_function_entry *functions;
	int method_len = strlen(method_name);
	int active_class_len = strlen(active_class);
	while(ce){
		//fprintf(stderr, "%s/%s\n", active_class, ce->name);
		if(!zend_binary_strcasecmp(ce->name, strlen(ce->name), active_class, active_class_len)){
			if(ce->builtin_functions){
				functions = ce->builtin_functions;
				while(functions->fname){
					//fprintf(stderr, " %s <> %s\n", functions->fname, method_name);
					if(!zend_binary_strcasecmp(functions->fname, strlen(functions->fname), method_name, method_len)){
						EG(scope) = ce;
						return SUCCESS;
					}
					functions++;
				}
			}
		}
		ce = ce->parent;
	}
	return FAILURE;
}

/**
 * Call single function which not requires parameters
 */
int phalcon_call_func(zval *return_value, char *func_name, int noreturn TSRMLS_DC){

	zval *fn = NULL;
	int status = FAILURE;

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	PHALCON_SET_STRING(fn, func_name, 0);
	status = phalcon_call_user_function(CG(function_table), NULL, fn, return_value, 0, NULL TSRMLS_CC);
	if(status==FAILURE){
		zend_error_noreturn(E_ERROR, "Call to undefined function %s()", func_name);
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

/**
 * Call single function which requires parameters
 */
int phalcon_call_func_params(zval *return_value, char *func_name, int param_count, zval *params[], int noreturn TSRMLS_DC){

	zval *fn = NULL;
	int status = FAILURE;

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	PHALCON_SET_STRING(fn, func_name, 0);
	status = phalcon_call_user_function(CG(function_table), NULL, fn, return_value, param_count, params TSRMLS_CC);
	if(status==FAILURE){
		zend_error_noreturn(E_ERROR, "Call to undefined function %s()", func_name);
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

/**
 * Call method on an object which not requires parameters
 *
 */
int phalcon_call_method(zval *return_value, zval *object, char *method_name, int check, int noreturn TSRMLS_DC){

	zval *fn = NULL;
	int status = FAILURE;
	zend_class_entry *active_scope = NULL;

	if(check){
		if(!zend_hash_exists(&Z_OBJCE_P(object)->function_table, method_name, strlen(method_name)+1)){
			return FAILURE;
		}
	}

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	PHALCON_SET_STRING(fn, method_name, 0);
	if(Z_TYPE_P(object)==IS_OBJECT){
		active_scope = EG(scope);
		phalcon_find_scope(Z_OBJCE_P(object), method_name TSRMLS_CC);
		status = call_user_function(&Z_OBJCE_P(object)->function_table, &object, fn, return_value, 0, NULL TSRMLS_CC);
		if(status==FAILURE){
			zend_error_noreturn(E_ERROR, "Call to undefined method %s()", Z_STRVAL_P(fn));
			return FAILURE;
		}
		EG(scope) = active_scope;
	} else {
		zend_error_noreturn(E_ERROR, "Call to method %s() on a non object", Z_STRVAL_P(fn));
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

/**
 * Call method on an object which requires parameters
 *
 */
int phalcon_call_method_params(zval *return_value, zval *object, char *method_name, int param_count, zval *params[], int check, int noreturn TSRMLS_DC){

	zval *fn = NULL;
	int status = FAILURE;
	zend_class_entry *active_scope = NULL;

	if(check){
		if(!zend_hash_exists(&Z_OBJCE_P(object)->function_table, method_name, strlen(method_name)+1)){
			return FAILURE;
		}
	}

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	PHALCON_SET_STRING(fn, method_name, 1);
	if(Z_TYPE_P(object)==IS_OBJECT){
		active_scope = EG(scope);
		phalcon_find_scope(Z_OBJCE_P(object), method_name TSRMLS_CC);
		status = phalcon_call_user_function(&Z_OBJCE_P(object)->function_table, &object, fn, return_value, param_count, params TSRMLS_CC);
		if(status==FAILURE){
			EG(scope) = active_scope;
			zend_error_noreturn(E_ERROR, "Call to undefined method %s() on class %s", Z_STRVAL_P(fn), Z_OBJCE_P(object)->name);
			return FAILURE;
		}
		EG(scope) = active_scope;
	} else {
		zend_error_noreturn(E_ERROR, "Call to method %s() on a non object", Z_STRVAL_P(fn));
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

/**
 * Call parent static function which not requires parameters
 */
int phalcon_call_parent_func(zval *return_value, zval *object, char *active_class, char *method_name, int noreturn TSRMLS_DC){
	int success;
	zend_class_entry *active_scope = NULL;
	if(object){
		active_scope = EG(scope);
		phalcon_find_parent_scope(Z_OBJCE_P(object), active_class, method_name TSRMLS_CC);
	}
	success = phalcon_call_static_func(return_value, "parent", method_name, noreturn TSRMLS_CC);
	if(object){
		EG(scope) = active_scope;
	}
	return success;
}

/**
 * Call parent static function which requires parameters
 */
int phalcon_call_parent_func_params(zval *return_value, zval *object, char *active_class, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC){
	int success;
	zend_class_entry *active_scope = NULL;
	if(object){
		active_scope = EG(scope);
		phalcon_find_parent_scope(Z_OBJCE_P(object), active_class, method_name TSRMLS_CC);
	}
	success = phalcon_call_static_func_params(return_value, "parent", method_name, param_count, params, noreturn TSRMLS_CC);
	if(object){
		EG(scope) = active_scope;
	}
	return success;
}

/**
 * Call self-class static function which not requires parameters
 */
int phalcon_call_self_func(zval *return_value, zval *object, char *method_name, int noreturn TSRMLS_DC){
	int success;
	zend_class_entry *active_scope = NULL;
	if(object){
		active_scope = EG(scope);
		phalcon_find_scope(Z_OBJCE_P(object), method_name TSRMLS_CC);
	}
	success = phalcon_call_static_func(return_value, "self", method_name, noreturn TSRMLS_CC);
	if(object){
		EG(scope) = active_scope;
	}
	return success;
}

/**
 * Call self-class static function which requires parameters
 */
int phalcon_call_self_func_params(zval *return_value, zval *object, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC){
	int success;
	zend_class_entry *active_scope = NULL;
	if(object){
		active_scope = EG(scope);
		phalcon_find_scope(Z_OBJCE_P(object), method_name TSRMLS_CC);
	}
	success = phalcon_call_static_func_params(return_value, "self", method_name, param_count, params, noreturn TSRMLS_CC);
	if(object){
		EG(scope) = active_scope;
	}
	return success;
}
/**
 * Call single static function which not requires parameters
 */
int phalcon_call_static_func(zval *return_value, char *class_name, char *method_name, int noreturn TSRMLS_DC){

	zval *fn;
	int status = FAILURE;

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	ALLOC_INIT_ZVAL(fn);
	array_init(fn);
	add_next_index_string(fn, class_name, 0);
	add_next_index_string(fn, method_name, 0);
	status = phalcon_call_user_function(CG(function_table), NULL, fn, return_value, 0, NULL TSRMLS_CC);
	if(status==FAILURE){
		zend_error_noreturn(E_ERROR, "Call to undefined function %s::%s()", class_name, method_name);
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

/**
 * Call single function which requires parameters
 */
int phalcon_call_static_func_params(zval *return_value, char *class_name, char *method_name, int param_count, zval *params[], int noreturn TSRMLS_DC){

	zval *fn;
	int status = FAILURE;

	if(!noreturn){
		ALLOC_INIT_ZVAL(return_value);
	}

	ALLOC_INIT_ZVAL(fn);
	array_init(fn);
	add_next_index_string(fn, class_name, 0);
	add_next_index_string(fn, method_name, 0);
	status = phalcon_call_user_function(CG(function_table), NULL, fn, return_value, param_count, params TSRMLS_CC);
	if(status==FAILURE){
		zend_error_noreturn(E_ERROR, "Call to undefined function %s::%s()", class_name, method_name);
		return FAILURE;
	}

	if(!noreturn){
		zval_ptr_dtor(&return_value);
	}

	return status;
}

static inline zend_function *phalcon_check_private_int(zend_function *fbc, zend_class_entry *ce, char *function_name_strval, int function_name_strlen TSRMLS_DC){

	int result;

	if(!ce){
		return 0;
	}

	/* We may call a private function if:
	 * 1.  The class of our object is the same as the scope, and the private
	 *     function (EX(fbc)) has the same scope.
	 * 2.  One of our parent classes are the same as the scope, and it contains
	 *     a private function with the same name that has the same scope.
	 */
	if(fbc->common.scope == ce && EG(scope) == ce){
		/* rule #1 checks out ok, allow the function call */
		return fbc;
	}

	/* Check rule #2 */
	ce = ce->parent;
	while(ce){
		if(PHALCON_DEBUG){
			fprintf(stderr, "Comparing %s with %s", ce->name, EG(scope)->name);
		}
		if(ce==EG(scope)){
			result = zend_hash_find(&ce->function_table, function_name_strval, function_name_strlen+1, (void **) &fbc)==SUCCESS
				&& fbc->op_array.fn_flags & ZEND_ACC_PRIVATE
				&& fbc->common.scope == EG(scope);
			if(result){
				return fbc;
			}
			break;
		}
		ce = ce->parent;
	}
	return NULL;
}

int phalcon_check_private(zend_function *fbc, zend_class_entry *ce, char *function_name_strval, int function_name_strlen TSRMLS_DC){
	return phalcon_check_private_int(fbc, ce, function_name_strval, function_name_strlen TSRMLS_CC) != NULL;
}

/**
 * Checks if a class method is callable
 */
int phalcon_is_callable_check_class(const char *name, int name_len, zend_fcall_info_cache *fcc, int *strict_class, char **error TSRMLS_DC){

	int ret = 0;
	zend_class_entry **pce;
	char *lcname = zend_str_tolower_dup(name, name_len);

	*strict_class = 0;
	if (name_len == sizeof("self") - 1 && !memcmp(lcname, "self", sizeof("self") - 1)) {
		if(!EG(scope)){
			if(error){
				*error = estrdup("cannot access self:: when no class scope is active");
			}
		} else {
			fcc->called_scope = EG(called_scope);
			fcc->calling_scope = EG(scope);
			if (!fcc->object_ptr) {
				fcc->object_ptr = EG(This);
			}
			ret = 1;
		}
	} else {
		if(name_len == sizeof("parent") - 1 && !memcmp(lcname, "parent", sizeof("parent") - 1)){
			if(!EG(scope)){
				if(error){
					*error = estrdup("cannot access parent:: when no class scope is active");
				}
			} else {
				if(!EG(scope)->parent){
					if(error){
						*error = estrdup("cannot access parent:: when current class scope has no parent");
					}
				} else {
					fcc->called_scope = EG(called_scope);
					fcc->calling_scope = EG(scope)->parent;
					if(!fcc->object_ptr){
						fcc->object_ptr = EG(This);
					}
					*strict_class = 1;
					ret = 1;
				}
			}
		} else {
			if (name_len == sizeof("static") - 1 && !memcmp(lcname, "static", sizeof("static") - 1)) {
				if(!EG(called_scope)){
					if (error) *error = estrdup("cannot access static:: when no class scope is active");
				} else {
					fcc->called_scope = EG(called_scope);
					fcc->calling_scope = EG(called_scope);
					if(!fcc->object_ptr){
						fcc->object_ptr = EG(This);
					}
					*strict_class = 1;
					ret = 1;
				}
			} else {
				if(zend_lookup_class_ex(name, name_len, 1, &pce TSRMLS_CC) == SUCCESS){
					zend_class_entry *scope = EG(active_op_array) ? EG(active_op_array)->scope : NULL;

					fcc->calling_scope = *pce;
					if(scope && !fcc->object_ptr && EG(This) &&
					    instanceof_function(Z_OBJCE_P(EG(This)), scope TSRMLS_CC) &&
					    instanceof_function(scope, fcc->calling_scope TSRMLS_CC)) {
						fcc->object_ptr = EG(This);
						fcc->called_scope = Z_OBJCE_P(fcc->object_ptr);
					} else {
						fcc->called_scope = fcc->object_ptr ? Z_OBJCE_P(fcc->object_ptr) : fcc->calling_scope;
					}
					*strict_class = 1;
					ret = 1;
				} else {
					if(error){
						phalcon_spprintf(error, 0, "class '%.*s' not found", name_len, name);
					}
				}
			}
		}
	}
	efree(lcname);
	return ret;
}

int phalcon_is_callable_check_func(int check_flags, zval *callable, zend_fcall_info_cache *fcc, int strict_class, char **error TSRMLS_DC){

	zend_class_entry *ce_org = fcc->calling_scope;
	int retval = 0;
	char *mname, *lmname, *colon;
	int clen, mlen, result;
	zend_class_entry *last_scope;
	HashTable *ftable;
	int call_via_handler = 0;
	zend_class_entry *scope;

	if(error){
		*error = NULL;
	}

	fcc->calling_scope = NULL;
	fcc->function_handler = NULL;

	if(!ce_org){
		/* Skip leading \ */
		if(Z_STRVAL_P(callable)[0] == '\\'){
			mlen = Z_STRLEN_P(callable) - 1;
			mname = Z_STRVAL_P(callable) + 1;
			lmname = zend_str_tolower_dup(Z_STRVAL_P(callable) + 1, mlen);
		} else {
			mlen = Z_STRLEN_P(callable);
			mname = Z_STRVAL_P(callable);
			lmname = zend_str_tolower_dup(Z_STRVAL_P(callable), mlen);
		}
		/* Check if function with given name exists.
		 * This may be a compound name that includes namespace name */
		if (zend_hash_find(EG(function_table), lmname, mlen+1, (void**)&fcc->function_handler) == SUCCESS) {
			efree(lmname);
			return 1;
		}
		efree(lmname);
	}

	/* Split name into class/namespace and method/function names */
	result = (colon = zend_memrchr(Z_STRVAL_P(callable), ':', Z_STRLEN_P(callable))) != NULL && colon > Z_STRVAL_P(callable) && *(colon-1) == ':';
	if(result){
		colon--;
		clen = colon - Z_STRVAL_P(callable);
		mlen = Z_STRLEN_P(callable) - clen - 2;

		if(colon == Z_STRVAL_P(callable)){
			if(error){
				phalcon_spprintf(error, 0, "invalid function name");
			}
			return 0;
		}

		/* This is a compound name.
		 * Try to fetch class and then find static method. */
		last_scope = EG(scope);
		if(ce_org){
			EG(scope) = ce_org;
			if(PHALCON_DEBUG){
				fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
			}
		}

		if(!phalcon_is_callable_check_class(Z_STRVAL_P(callable), clen, fcc, &strict_class, error TSRMLS_CC)){
			EG(scope) = last_scope;
			if(PHALCON_DEBUG){
				fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
			}
			return 0;
		}
		EG(scope) = last_scope;
		if(PHALCON_DEBUG){
			fprintf(stderr, "ScopeAddress=%p\n", last_scope);
		}

		ftable = &fcc->calling_scope->function_table;
		if(ce_org && !instanceof_function(ce_org, fcc->calling_scope TSRMLS_CC)){
			if (error) phalcon_spprintf(error, 0, "class '%s' is not a subclass of '%s'", ce_org->name, fcc->calling_scope->name);
			return 0;
		}
		mname = Z_STRVAL_P(callable) + clen + 2;
	} else {
		if (ce_org) {
			/* Try to fetch find static method of given class. */
			mlen = Z_STRLEN_P(callable);
			mname = Z_STRVAL_P(callable);
			ftable = &ce_org->function_table;
			fcc->calling_scope = ce_org;
		} else {
			/* We already checked for plain function before. */
			if(error && !(check_flags & IS_CALLABLE_CHECK_SILENT)){
				phalcon_spprintf(error, 0, "function '%s' not found or invalid function name", Z_STRVAL_P(callable));
			}
			return 0;
		}
	}

	lmname = zend_str_tolower_dup(mname, mlen);
	//fprintf(stderr, "%s\n", lmname);
	//fprintf(stderr, " => ");

	result = strict_class && fcc->calling_scope && mlen == sizeof(ZEND_CONSTRUCTOR_FUNC_NAME)-1 && !memcmp(lmname, ZEND_CONSTRUCTOR_FUNC_NAME, sizeof(ZEND_CONSTRUCTOR_FUNC_NAME));
	//fprintf(stderr, "%d => ", result);

	if(result){
		fcc->function_handler = fcc->calling_scope->constructor;
		if(fcc->function_handler){
			retval = 1;
		}
	} else {
		result = (zend_hash_find(ftable, lmname, mlen+1, (void**)&fcc->function_handler) == SUCCESS);
		if(result){

			//fprintf(stderr, "%d => ", result);

			retval = 1;
			result = (fcc->function_handler->op_array.fn_flags & ZEND_ACC_CHANGED) && EG(scope) && instanceof_function(fcc->function_handler->common.scope, EG(scope) TSRMLS_CC);
			if(result){
				zend_function *priv_fbc;
				if(zend_hash_find(&EG(scope)->function_table, lmname, mlen+1, (void **) &priv_fbc)==SUCCESS
					&& priv_fbc->common.fn_flags & ZEND_ACC_PRIVATE
					&& priv_fbc->common.scope == EG(scope)) {
					fcc->function_handler = priv_fbc;
				}
			}

			result = (check_flags & IS_CALLABLE_CHECK_NO_ACCESS) == 0 && (fcc->calling_scope && (fcc->calling_scope->__call || fcc->calling_scope->__callstatic));
			//fprintf(stderr, "%d => ", result);

			if(result){
				if(fcc->function_handler->op_array.fn_flags & ZEND_ACC_PRIVATE){
					if(!phalcon_check_private(fcc->function_handler, fcc->object_ptr ? Z_OBJCE_P(fcc->object_ptr) : EG(scope), lmname, mlen TSRMLS_CC)){
						retval = 0;
						fcc->function_handler = NULL;
						goto get_function_via_handler;
					}
				} else {
					if(fcc->function_handler->common.fn_flags & ZEND_ACC_PROTECTED){
						if(!zend_check_protected(fcc->function_handler->common.scope, EG(scope))){
							retval = 0;
							fcc->function_handler = NULL;
							goto get_function_via_handler;
						}
					}
				}
			}
		} else {

			get_function_via_handler:
			if (fcc->object_ptr && fcc->calling_scope == ce_org) {
				if (strict_class && ce_org->__call) {
					fcc->function_handler = emalloc(sizeof(zend_internal_function));
					fcc->function_handler->internal_function.type = ZEND_INTERNAL_FUNCTION;
					fcc->function_handler->internal_function.module = ce_org->module;
					fcc->function_handler->internal_function.handler = zend_std_call_user_call;
					fcc->function_handler->internal_function.arg_info = NULL;
					fcc->function_handler->internal_function.num_args = 0;
					fcc->function_handler->internal_function.scope = ce_org;
					fcc->function_handler->internal_function.fn_flags = ZEND_ACC_CALL_VIA_HANDLER;
					fcc->function_handler->internal_function.function_name = estrndup(mname, mlen);
					fcc->function_handler->internal_function.pass_rest_by_reference = 0;
					fcc->function_handler->internal_function.return_reference = ZEND_RETURN_VALUE;
					call_via_handler = 1;
					retval = 1;
				} else if (Z_OBJ_HT_P(fcc->object_ptr)->get_method) {
					fcc->function_handler = Z_OBJ_HT_P(fcc->object_ptr)->get_method(&fcc->object_ptr, mname, mlen TSRMLS_CC);
					if (fcc->function_handler) {
						if (strict_class &&
						    (!fcc->function_handler->common.scope ||
						     !instanceof_function(ce_org, fcc->function_handler->common.scope TSRMLS_CC))) {
							if ((fcc->function_handler->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0) {
								if (fcc->function_handler->type != ZEND_OVERLOADED_FUNCTION) {
									efree(fcc->function_handler->common.function_name);
								}
								efree(fcc->function_handler);
							}
						} else {
							retval = 1;
							call_via_handler = (fcc->function_handler->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0;
						}
					}
				}
			} else if (fcc->calling_scope) {
				if (fcc->calling_scope->get_static_method) {
					fcc->function_handler = fcc->calling_scope->get_static_method(fcc->calling_scope, mname, mlen TSRMLS_CC);
				} else {
					fcc->function_handler = zend_std_get_static_method(fcc->calling_scope, mname, mlen TSRMLS_CC);
				}
				if (fcc->function_handler) {
					retval = 1;
					call_via_handler = (fcc->function_handler->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0;
					if (call_via_handler && !fcc->object_ptr && EG(This) &&
					    Z_OBJ_HT_P(EG(This))->get_class_entry &&
					    instanceof_function(Z_OBJCE_P(EG(This)), fcc->calling_scope TSRMLS_CC)) {
						fcc->object_ptr = EG(This);
					}
				}
			}
		}
	}

	//fprintf(stderr, "%d\n", retval);

	if(retval){
		if(fcc->calling_scope && !call_via_handler){
			if(!fcc->object_ptr && !(fcc->function_handler->common.fn_flags & ZEND_ACC_STATIC)){
				int severity;
				char *verb;
				if(fcc->function_handler->common.fn_flags & ZEND_ACC_ALLOW_STATIC){
					severity = E_STRICT;
					verb = "should not";
				} else {
					/* An internal function assumes $this is present and won't check that. So PHP would crash by allowing the call. */
					severity = E_ERROR;
					verb = "cannot";
				}
				if((check_flags & IS_CALLABLE_CHECK_IS_STATIC) != 0){
					retval = 0;
				}
				if(EG(This) && instanceof_function(Z_OBJCE_P(EG(This)), fcc->calling_scope TSRMLS_CC)){
					fcc->object_ptr = EG(This);
					if (error) {
						phalcon_spprintf(error, 0, "non-static method %s::%s() %s be called statically, assuming $this from compatible context %s", fcc->calling_scope->name, fcc->function_handler->common.function_name, verb, Z_OBJCE_P(EG(This))->name);
						if (severity == E_ERROR) {
							retval = 0;
						}
					} else if (retval) {
						zend_error(severity, "Non-static method %s::%s() %s be called statically, assuming $this from compatible context %s", fcc->calling_scope->name, fcc->function_handler->common.function_name, verb, Z_OBJCE_P(EG(This))->name);
					}
				} else {
					if(error){
						phalcon_spprintf(error, 0, "non-static method %s::%s() %s be called statically", fcc->calling_scope->name, fcc->function_handler->common.function_name, verb);
						if (severity == E_ERROR) {
							retval = 0;
						}
					} else if (retval) {
						zend_error(severity, "Non-static method %s::%s() %s be called statically", fcc->calling_scope->name, fcc->function_handler->common.function_name, verb);
					}
				}
			}

			if(retval && (check_flags & IS_CALLABLE_CHECK_NO_ACCESS) == 0){
				if(fcc->function_handler->op_array.fn_flags & ZEND_ACC_PRIVATE){
					if(fcc->object_ptr){
						scope = Z_OBJCE_P(fcc->object_ptr);
					} else {
						scope = EG(scope);
					}
					if(!phalcon_check_private(fcc->function_handler, scope, lmname, mlen TSRMLS_CC)){
						if(error){
							if(*error){
								efree(*error);
							}
							phalcon_spprintf(error, 0, "cannot access private method %s::%s() (21)", fcc->calling_scope->name, fcc->function_handler->common.function_name);
						}
						retval = 0;
					}
				} else {
					if((fcc->function_handler->common.fn_flags & ZEND_ACC_PROTECTED)){
						if(!zend_check_protected(fcc->function_handler->common.scope, EG(scope))){
							if(error){
								if(*error){
									efree(*error);
								}
								phalcon_spprintf(error, 0, "cannot access protected method %s::%s() (22)", fcc->calling_scope->name, fcc->function_handler->common.function_name);
							}
							retval = 0;
						}
					}
				}
			}
		}
	} else {
		if(error && !(check_flags & IS_CALLABLE_CHECK_SILENT)){
			if(fcc->calling_scope){
				if (error) phalcon_spprintf(error, 0, "class '%s' does not have a method '%s'", fcc->calling_scope->name, mname);
			} else {
				if (error) phalcon_spprintf(error, 0, "function '%s' does not exist", mname);
			}
		}
	}
	efree(lmname);

	if(fcc->object_ptr){
		fcc->called_scope = Z_OBJCE_P(fcc->object_ptr);
	}
	if(retval){
		fcc->initialized = 1;
	}
	return retval;
}

zend_bool phalcon_is_callable_ex(zval *callable, zval *object_ptr, uint check_flags, char **callable_name, int *callable_name_len, zend_fcall_info_cache *fcc, char **error TSRMLS_DC){
	zend_bool ret;
	int callable_name_len_local;
	zend_fcall_info_cache fcc_local;

	if(callable_name){
		*callable_name = NULL;
	}
	if(callable_name_len == NULL){
		callable_name_len = &callable_name_len_local;
	}
	if(fcc == NULL){
		fcc = &fcc_local;
	}
	if(error){
		*error = NULL;
	}

	fcc->initialized = 0;
	fcc->calling_scope = NULL;
	fcc->called_scope = NULL;
	fcc->function_handler = NULL;
	fcc->calling_scope = NULL;
	fcc->object_ptr = NULL;

	if (object_ptr && Z_TYPE_P(object_ptr) != IS_OBJECT) {
		object_ptr = NULL;
	}
	if (object_ptr &&
	    (!EG(objects_store).object_buckets ||
	     !EG(objects_store).object_buckets[Z_OBJ_HANDLE_P(object_ptr)].valid)) {
		return 0;
	}

	switch (Z_TYPE_P(callable)) {
		case IS_STRING:

			if(object_ptr){
				fcc->object_ptr = object_ptr;
				fcc->calling_scope = Z_OBJCE_P(object_ptr);
				if(callable_name){
					char *ptr;

					*callable_name_len = fcc->calling_scope->name_length + Z_STRLEN_P(callable) + sizeof("::") - 1;
					ptr = *callable_name = emalloc(*callable_name_len + 1);
					memcpy(ptr, fcc->calling_scope->name, fcc->calling_scope->name_length);
					ptr += fcc->calling_scope->name_length;
					memcpy(ptr, "::", sizeof("::") - 1);
					ptr += sizeof("::") - 1;
					memcpy(ptr, Z_STRVAL_P(callable), Z_STRLEN_P(callable) + 1);
				}
			} else {
				if(callable_name){
					*callable_name = estrndup(Z_STRVAL_P(callable), Z_STRLEN_P(callable));
					*callable_name_len = Z_STRLEN_P(callable);
				}
			}

			if(check_flags & IS_CALLABLE_CHECK_SYNTAX_ONLY){
				fcc->called_scope = fcc->calling_scope;
				return 1;
			}

			ret = phalcon_is_callable_check_func(check_flags, callable, fcc, 0, error TSRMLS_CC);
			if (fcc == &fcc_local &&
			    fcc->function_handler &&
				((fcc->function_handler->type == ZEND_INTERNAL_FUNCTION &&
			      (fcc->function_handler->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER)) ||
			     fcc->function_handler->type == ZEND_OVERLOADED_FUNCTION_TEMPORARY ||
			     fcc->function_handler->type == ZEND_OVERLOADED_FUNCTION)) {
				if (fcc->function_handler->type != ZEND_OVERLOADED_FUNCTION) {
					efree(fcc->function_handler->common.function_name);
				}
				efree(fcc->function_handler);
			}
			return ret;

		case IS_ARRAY:
			{
				zval **method = NULL;
				zval **obj = NULL;
				int strict_class = 0;

				if (zend_hash_num_elements(Z_ARRVAL_P(callable)) == 2) {
					zend_hash_index_find(Z_ARRVAL_P(callable), 0, (void **) &obj);
					zend_hash_index_find(Z_ARRVAL_P(callable), 1, (void **) &method);
				}
				if (obj && method &&
					(Z_TYPE_PP(obj) == IS_OBJECT ||
					Z_TYPE_PP(obj) == IS_STRING) &&
					Z_TYPE_PP(method) == IS_STRING) {

					if (Z_TYPE_PP(obj) == IS_STRING) {
						if (callable_name) {
							char *ptr;

							*callable_name_len = Z_STRLEN_PP(obj) + Z_STRLEN_PP(method) + sizeof("::") - 1;
							ptr = *callable_name = emalloc(*callable_name_len + 1);
							memcpy(ptr, Z_STRVAL_PP(obj), Z_STRLEN_PP(obj));
							ptr += Z_STRLEN_PP(obj);
							memcpy(ptr, "::", sizeof("::") - 1);
							ptr += sizeof("::") - 1;
							memcpy(ptr, Z_STRVAL_PP(method), Z_STRLEN_PP(method) + 1);
						}

						if (check_flags & IS_CALLABLE_CHECK_SYNTAX_ONLY) {
							return 1;
						}

						if (!phalcon_is_callable_check_class(Z_STRVAL_PP(obj), Z_STRLEN_PP(obj), fcc, &strict_class, error TSRMLS_CC)) {
							return 0;
						}

					} else {
						if (!EG(objects_store).object_buckets ||
						    !EG(objects_store).object_buckets[Z_OBJ_HANDLE_PP(obj)].valid) {
							return 0;
						}

						fcc->calling_scope = Z_OBJCE_PP(obj); /* TBFixed: what if it's overloaded? */

						fcc->object_ptr = *obj;

						if (callable_name) {
							char *ptr;

							*callable_name_len = fcc->calling_scope->name_length + Z_STRLEN_PP(method) + sizeof("::") - 1;
							ptr = *callable_name = emalloc(*callable_name_len + 1);
							memcpy(ptr, fcc->calling_scope->name, fcc->calling_scope->name_length);
							ptr += fcc->calling_scope->name_length;
							memcpy(ptr, "::", sizeof("::") - 1);
							ptr += sizeof("::") - 1;
							memcpy(ptr, Z_STRVAL_PP(method), Z_STRLEN_PP(method) + 1);
						}

						if (check_flags & IS_CALLABLE_CHECK_SYNTAX_ONLY) {
							fcc->called_scope = fcc->calling_scope;
							return 1;
						}
					}

					ret = phalcon_is_callable_check_func(check_flags, *method, fcc, strict_class, error TSRMLS_CC);
					if (fcc == &fcc_local &&
					    fcc->function_handler &&
						((fcc->function_handler->type == ZEND_INTERNAL_FUNCTION &&
					      (fcc->function_handler->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER)) ||
					     fcc->function_handler->type == ZEND_OVERLOADED_FUNCTION_TEMPORARY ||
					     fcc->function_handler->type == ZEND_OVERLOADED_FUNCTION)) {
						if (fcc->function_handler->type != ZEND_OVERLOADED_FUNCTION) {
							efree(fcc->function_handler->common.function_name);
						}
						efree(fcc->function_handler);
					}
					return ret;

				} else {
					if (zend_hash_num_elements(Z_ARRVAL_P(callable)) == 2) {
						if (!obj || (Z_TYPE_PP(obj) != IS_STRING && Z_TYPE_PP(obj) != IS_OBJECT)) {
							if (error) phalcon_spprintf(error, 0, "first array member is not a valid class name or object");
						} else {
							if (error) phalcon_spprintf(error, 0, "second array member is not a valid method");
						}
					} else {
						if (error) phalcon_spprintf(error, 0, "array must have exactly two members");
					}
					if (callable_name) {
						*callable_name = estrndup("Array", sizeof("Array")-1);
						*callable_name_len = sizeof("Array") - 1;
					}
				}
			}
			return 0;

		case IS_OBJECT:
			if (Z_OBJ_HANDLER_P(callable, get_closure) && Z_OBJ_HANDLER_P(callable, get_closure)(callable, &fcc->calling_scope, &fcc->function_handler, &fcc->object_ptr TSRMLS_CC) == SUCCESS) {
				fcc->called_scope = fcc->calling_scope;
				if (callable_name) {
					zend_class_entry *ce = Z_OBJCE_P(callable); /* TBFixed: what if it's overloaded? */

					*callable_name_len = ce->name_length + sizeof("::__invoke") - 1;
					*callable_name = emalloc(*callable_name_len + 1);
					memcpy(*callable_name, ce->name, ce->name_length);
					memcpy((*callable_name) + ce->name_length, "::__invoke", sizeof("::__invoke"));
				}
				return 1;
			}
			/* break missing intentionally */

		default:
			if (callable_name) {
				zval expr_copy;
				int use_copy;

				zend_make_printable_zval(callable, &expr_copy, &use_copy);
				*callable_name = estrndup(Z_STRVAL(expr_copy), Z_STRLEN(expr_copy));
				*callable_name_len = Z_STRLEN(expr_copy);
				zval_dtor(&expr_copy);
			}
			if (error) phalcon_spprintf(error, 0, "no array or string given");
			return 0;
	}
}

/**
 * zend call_user_function for debugging
 *
 */
int phalcon_call_user_function(HashTable *function_table, zval **object_pp, zval *function_name, zval *retval_ptr, zend_uint param_count, zval *params[] TSRMLS_DC){

	zval ***params_array;
	zend_uint i;
	int ex_retval;
	zval *local_retval_ptr = NULL;

	if(param_count){
		params_array = (zval ***) emalloc(sizeof(zval **)*param_count);
		for(i=0; i<param_count; i++){
			params_array[i] = &params[i];
		}
	} else {
		params_array = NULL;
	}
	ex_retval = phalcon_call_user_function_ex(function_table, object_pp, function_name, &local_retval_ptr, param_count, params_array, 1, NULL TSRMLS_CC);
	if(local_retval_ptr){
		COPY_PZVAL_TO_ZVAL(*retval_ptr, local_retval_ptr);
	} else {
		INIT_ZVAL(*retval_ptr);
	}
	if(params_array){
		efree(params_array);
	}
	return ex_retval;

}

int phalcon_call_user_function_ex(HashTable *function_table, zval **object_pp, zval *function_name, zval **retval_ptr_ptr, zend_uint param_count, zval **params[], int no_separation, HashTable *symbol_table TSRMLS_DC){
	zend_fcall_info fci;

	fci.size = sizeof(fci);
	fci.function_table = function_table;
	fci.object_ptr = object_pp ? *object_pp : NULL;
	fci.function_name = function_name;
	fci.retval_ptr_ptr = retval_ptr_ptr;
	fci.param_count = param_count;
	fci.params = params;
	fci.no_separation = (zend_bool) no_separation;
	fci.symbol_table = symbol_table;

	return zend_call_function(&fci, NULL TSRMLS_CC);
}

#ifndef PHP_WIN32

int phalcon_call_function(zend_fcall_info *fci, zend_fcall_info_cache *fci_cache TSRMLS_DC){

	zend_uint i;
	zval **original_return_value;
	HashTable *calling_symbol_table;
	zend_op_array *original_op_array;
	zend_op **original_opline_ptr;
	zend_class_entry *current_scope;
	zend_class_entry *current_called_scope;
	zend_class_entry *calling_scope = NULL;
	zend_class_entry *called_scope = NULL;
	zval *current_this;
	zend_execute_data execute_data;

	*fci->retval_ptr_ptr = NULL;

	if(!EG(active)){
		fprintf(stderr, "Failure: executor is already inactive\n");
		return FAILURE; /* executor is already inactive */
	}

	if(EG(exception)){
		fprintf(stderr, "Failure: We would result in an instable executor otherwise\n");
		return FAILURE; /* we would result in an instable executor otherwise */
	}

	switch(fci->size){
		case sizeof(zend_fcall_info):
			break; /* nothing to do currently */
		default:
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Corrupted fcall_info provided to zend_call_function()");
			break;
	}

	/* Initialize execute_data */
	if (EG(current_execute_data)) {
		execute_data = *EG(current_execute_data);
		EX(op_array) = NULL;
		EX(opline) = NULL;
		EX(object) = NULL;
	} else {
		/* This only happens when we're called outside any execute()'s
		 * It shouldn't be strictly necessary to NULL execute_data out,
		 * but it may make bugs easier to spot
		 */
		memset(&execute_data, 0, sizeof(zend_execute_data));
	}

	if (!fci_cache || !fci_cache->initialized) {
		zend_fcall_info_cache fci_cache_local;
		char *callable_name;
		char *error = NULL;

		if (!fci_cache) {
			fci_cache = &fci_cache_local;
		}

		if(!phalcon_is_callable_ex(fci->function_name, fci->object_ptr, 0, &callable_name, NULL, fci_cache, &error TSRMLS_CC)){
			if(error){
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback %s, %s (1)", callable_name, error);
				efree(error);
			}
			if(callable_name){
				efree(callable_name);
			}
			return FAILURE;
		} else {
			if(error){
				/* Capitalize the first latter of the error message */
				if (error[0] >= 'a' && error[0] <= 'z') {
					error[0] += ('A' - 'a');
				}
				php_error_docref(NULL TSRMLS_CC, E_STRICT, "%s", error);
				efree(error);
			}
		}
		efree(callable_name);
	}

	EX(function_state).function = fci_cache->function_handler;
	calling_scope = fci_cache->calling_scope;
	called_scope = fci_cache->called_scope;
	fci->object_ptr = fci_cache->object_ptr;
	EX(object) = fci->object_ptr;
	if (fci->object_ptr && Z_TYPE_P(fci->object_ptr) == IS_OBJECT && (!EG(objects_store).object_buckets || !EG(objects_store).object_buckets[Z_OBJ_HANDLE_P(fci->object_ptr)].valid)) {
		fprintf(stderr, "Failure: Some crazyness about buckets ¿?\n");
		return FAILURE;
	}

	if (EX(function_state).function->common.fn_flags & (ZEND_ACC_ABSTRACT|ZEND_ACC_DEPRECATED)) {
		if (EX(function_state).function->common.fn_flags & ZEND_ACC_ABSTRACT) {
			zend_error_noreturn(E_ERROR, "Cannot call abstract method %s::%s()", EX(function_state).function->common.scope->name, EX(function_state).function->common.function_name);
		}
		if (EX(function_state).function->common.fn_flags & ZEND_ACC_DEPRECATED) {
 			php_error_docref(NULL TSRMLS_CC, E_DEPRECATED, "Function %s%s%s() is deprecated",
				EX(function_state).function->common.scope ? EX(function_state).function->common.scope->name : "",
				EX(function_state).function->common.scope ? "::" : "",
				EX(function_state).function->common.function_name);
		}
	}

	ZEND_VM_STACK_GROW_IF_NEEDED(fci->param_count + 1);

	for (i=0; i<fci->param_count; i++) {
		zval *param;

		if (EX(function_state).function->type == ZEND_INTERNAL_FUNCTION
			&& (EX(function_state).function->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) == 0
			&& !ARG_SHOULD_BE_SENT_BY_REF(EX(function_state).function, i + 1)
			&& PZVAL_IS_REF(*fci->params[i])) {
			SEPARATE_ZVAL(fci->params[i]);
		}

		if (ARG_SHOULD_BE_SENT_BY_REF(EX(function_state).function, i + 1)
			&& !PZVAL_IS_REF(*fci->params[i])) {

			if (Z_REFCOUNT_PP(fci->params[i]) > 1) {
				zval *new_zval;

				if (fci->no_separation &&
				    !ARG_MAY_BE_SENT_BY_REF(EX(function_state).function, i + 1)) {
					if(i) {
						/* hack to clean up the stack */
						zend_vm_stack_push_nocheck((void *) (zend_uintptr_t)i TSRMLS_CC);
						zend_vm_stack_clear_multiple(TSRMLS_C);
					}

					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Parameter %d to %s%s%s() expected to be a reference, value given",
						i+1,
						EX(function_state).function->common.scope ? EX(function_state).function->common.scope->name : "",
						EX(function_state).function->common.scope ? "::" : "",
						EX(function_state).function->common.function_name);
					return FAILURE;
				}

				ALLOC_ZVAL(new_zval);
				*new_zval = **fci->params[i];
				zval_copy_ctor(new_zval);
				Z_SET_REFCOUNT_P(new_zval, 1);
				Z_DELREF_PP(fci->params[i]);
				*fci->params[i] = new_zval;
			}
			Z_ADDREF_PP(fci->params[i]);
			Z_SET_ISREF_PP(fci->params[i]);
			param = *fci->params[i];
		} else if (*fci->params[i] != &EG(uninitialized_zval)) {
			Z_ADDREF_PP(fci->params[i]);
			param = *fci->params[i];
		} else {
			ALLOC_ZVAL(param);
			*param = **(fci->params[i]);
			INIT_PZVAL(param);
		}
		zend_vm_stack_push_nocheck(param TSRMLS_CC);
	}

	EX(function_state).arguments = zend_vm_stack_top(TSRMLS_C);
	zend_vm_stack_push_nocheck((void*)(zend_uintptr_t)fci->param_count TSRMLS_CC);

	current_scope = EG(scope);
	EG(scope) = calling_scope;
	if(PHALCON_DEBUG){
		fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
	}

	current_this = EG(This);

	current_called_scope = EG(called_scope);
	if (called_scope) {
		EG(called_scope) = called_scope;
	} else if (EX(function_state).function->type != ZEND_INTERNAL_FUNCTION) {
		EG(called_scope) = NULL;
	}

	if (fci->object_ptr) {
		if ((EX(function_state).function->common.fn_flags & ZEND_ACC_STATIC)) {
			EG(This) = NULL;
		} else {
			EG(This) = fci->object_ptr;

			if (!PZVAL_IS_REF(EG(This))) {
				Z_ADDREF_P(EG(This)); /* For $this pointer */
			} else {
				zval *this_ptr;

				ALLOC_ZVAL(this_ptr);
				*this_ptr = *EG(This);
				INIT_PZVAL(this_ptr);
				zval_copy_ctor(this_ptr);
				EG(This) = this_ptr;
			}
		}
	} else {
		EG(This) = NULL;
	}

	EX(prev_execute_data) = EG(current_execute_data);
	EG(current_execute_data) = &execute_data;

	if (EX(function_state).function->type == ZEND_USER_FUNCTION) {
		calling_symbol_table = EG(active_symbol_table);
		EG(scope) = EX(function_state).function->common.scope;
		if(PHALCON_DEBUG){
			fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
		}
		if(fci->symbol_table){
			EG(active_symbol_table) = fci->symbol_table;
		} else {
			EG(active_symbol_table) = NULL;
		}

		original_return_value = EG(return_value_ptr_ptr);
		original_op_array = EG(active_op_array);
		EG(return_value_ptr_ptr) = fci->retval_ptr_ptr;
		EG(active_op_array) = (zend_op_array *) EX(function_state).function;
		original_opline_ptr = EG(opline_ptr);
		zend_execute(EG(active_op_array) TSRMLS_CC);
		if (!fci->symbol_table && EG(active_symbol_table)) {
			if (EG(symtable_cache_ptr)>=EG(symtable_cache_limit)) {
				zend_hash_destroy(EG(active_symbol_table));
				FREE_HASHTABLE(EG(active_symbol_table));
			} else {
				/* clean before putting into the cache, since clean
				   could call dtors, which could use cached hash */
				zend_hash_clean(EG(active_symbol_table));
				*(++EG(symtable_cache_ptr)) = EG(active_symbol_table);
			}
		}
		EG(active_symbol_table) = calling_symbol_table;
		EG(active_op_array) = original_op_array;
		EG(return_value_ptr_ptr)=original_return_value;
		EG(opline_ptr) = original_opline_ptr;
	} else if (EX(function_state).function->type == ZEND_INTERNAL_FUNCTION) {
		int call_via_handler = (EX(function_state).function->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER) != 0;
		ALLOC_INIT_ZVAL(*fci->retval_ptr_ptr);
		if (EX(function_state).function->common.scope) {
			EG(scope) = EX(function_state).function->common.scope;
			if(PHALCON_DEBUG){
				fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
			}
		}
		((zend_internal_function *) EX(function_state).function)->handler(fci->param_count, *fci->retval_ptr_ptr, fci->retval_ptr_ptr, fci->object_ptr, 1 TSRMLS_CC);
		/*  We shouldn't fix bad extensions here,
			because it can break proper ones (Bug #34045)
		if (!EX(function_state).function->common.return_reference)
		{
			INIT_PZVAL(*fci->retval_ptr_ptr);
		}*/
		if (EG(exception) && fci->retval_ptr_ptr) {
			zval_ptr_dtor(fci->retval_ptr_ptr);
			*fci->retval_ptr_ptr = NULL;
		}

		if (call_via_handler) {
			/* We must re-initialize function again */
			fci_cache->initialized = 0;
		}
	} else { /* ZEND_OVERLOADED_FUNCTION */
		ALLOC_INIT_ZVAL(*fci->retval_ptr_ptr);

		/* Not sure what should be done here if it's a static method */
		if (fci->object_ptr) {
			Z_OBJ_HT_P(fci->object_ptr)->call_method(EX(function_state).function->common.function_name, fci->param_count, *fci->retval_ptr_ptr, fci->retval_ptr_ptr, fci->object_ptr, 1 TSRMLS_CC);
		} else {
			zend_error_noreturn(E_ERROR, "Cannot call overloaded function for non-object");
		}

		if (EX(function_state).function->type == ZEND_OVERLOADED_FUNCTION_TEMPORARY) {
			efree(EX(function_state).function->common.function_name);
		}
		efree(EX(function_state).function);

		if (EG(exception) && fci->retval_ptr_ptr) {
			zval_ptr_dtor(fci->retval_ptr_ptr);
			*fci->retval_ptr_ptr = NULL;
		}
	}
	zend_vm_stack_clear_multiple(TSRMLS_C);

	if (EG(This)) {
		zval_ptr_dtor(&EG(This));
	}
	EG(called_scope) = current_called_scope;
	EG(scope) = current_scope;
	if(PHALCON_DEBUG){
		fprintf(stderr, "ScopeAddress=%p\n", EG(scope));
	}
	EG(This) = current_this;
	EG(current_execute_data) = EX(prev_execute_data);

	if (EG(exception)) {
		zend_throw_exception_internal(NULL TSRMLS_CC);
	}
	return SUCCESS;
}
#endif#ifdef HAVE_CONFIG_H
#endif


/**
 * Do an internal require to a plain php file
 *
 */
int phalcon_require(zval *require_path TSRMLS_DC){

	int ret;
	char *file_path;
	int file_path_length;
	zend_file_handle file_handle;
	zval *result = NULL;
	zend_op_array *orig_op_array;
	zval **orig_retval_ptr_ptr;

	switch(Z_TYPE_P(require_path)){
		case IS_ARRAY:
		case IS_OBJECT:
		case IS_RESOURCE:
			zend_error_noreturn(E_ERROR, "Invalid require path value");
			return FAILURE;
	}

	if(Z_TYPE_P(require_path)!=IS_STRING){
		convert_to_string(require_path);
	}

	file_path = Z_STRVAL_P(require_path);
	file_path_length = Z_STRLEN_P(require_path);

	ret = php_stream_open_for_zend_ex(file_path, &file_handle, ENFORCE_SAFE_MODE|USE_PATH|STREAM_OPEN_FOR_INCLUDE TSRMLS_CC);
	if(ret==SUCCESS){

		if(!file_handle.opened_path){
			file_handle.opened_path = estrndup(file_path, file_path_length);
		}

		EG(exit_status) = 0;
		PG(during_request_startup) = 0;

		if(file_handle.filename && (file_handle.filename[0] != '-' || file_handle.filename[1] != 0) && file_handle.opened_path == NULL && file_handle.type != ZEND_HANDLE_FILENAME){
			char realfile[MAXPATHLEN];
			int realfile_len;
			int dummy = 1;
			if(expand_filepath(file_handle.filename, realfile TSRMLS_CC)){
				realfile_len =  strlen(realfile);
				zend_hash_add(&EG(included_files), realfile, realfile_len+1, (void *)&dummy, sizeof(int), NULL);
				file_handle.opened_path = estrndup(realfile, realfile_len);
			}
		}

		orig_op_array = EG(active_op_array);
		orig_retval_ptr_ptr = EG(return_value_ptr_ptr);

		EG(active_op_array) = zend_compile_file(&file_handle, ZEND_REQUIRE TSRMLS_CC);
		if(EG(active_op_array) && file_handle.handle.stream.handle){
			EG(return_value_ptr_ptr) = NULL;
			zend_execute(EG(active_op_array) TSRMLS_CC);
			destroy_op_array(EG(active_op_array) TSRMLS_CC);
			efree(EG(active_op_array));
		} else {
			EG(active_op_array) = orig_op_array;
			EG(return_value_ptr_ptr) = orig_retval_ptr_ptr;
			return FAILURE;
		}

		EG(active_op_array) = orig_op_array;
		EG(return_value_ptr_ptr) = orig_retval_ptr_ptr;

		return SUCCESS;
	}

	return FAILURE;

}
#ifdef HAVE_CONFIG_H
#endif


/**
 * Applies sprintf function to a variable list
 */
int phalcon_spprintf(char **message, int max_len, char *format, ...){
	va_list arg;
	int len;

	va_start(arg, format);
	len = vspprintf(message, max_len, format, arg);
	va_end(arg);
	return len;
}

#ifndef PHALCON_RELEASE

FILE *phalcon_log = NULL;
int phalcon_debug_trace = 0;
phalcon_debug_entry *start = NULL;
phalcon_debug_entry *active = NULL;

/**
 * Stars debug on file
 */
int phalcon_start_debug(){
	if(!phalcon_log){
		phalcon_log = fopen("/tmp/phalcon-debug.a", "w");
		//phalcon_log = stderr;
	}
	return SUCCESS;
}

/**
 * Executes a print_r on an interal zval
 */
int phalcon_print_r(zval *userval TSRMLS_DC){
	zend_print_zval_r(userval, 0 TSRMLS_CC);
	return SUCCESS;
}

/**
 * Internal fast zval dump
 */
int phalcon_vdump(zval *uservar TSRMLS_DC){
	phalcon_start_debug();
    if(!uservar){
		fprintf(phalcon_log, "Null pointer\n");
		return SUCCESS;
	}
    switch(Z_TYPE_P(uservar)){
        case IS_NULL:
            fprintf(phalcon_log, "NULL \n");
            break;
        case IS_BOOL:
            fprintf(phalcon_log, "Boolean: %s\n", Z_LVAL_P(uservar) ? "TRUE" : "FALSE");
            break;
        case IS_LONG:
            fprintf(phalcon_log, "Long: %ld at %p, refcount=%d\n", Z_LVAL_P(uservar), uservar, Z_REFCOUNT_P(uservar));
            break;
        case IS_DOUBLE:
            fprintf(phalcon_log, "Double: %f\n", Z_DVAL_P(uservar));
            break;
        case IS_STRING:
			fprintf(phalcon_log, "String: %s(%d) at %p, refcount=%d\n", Z_STRVAL_P(uservar), Z_STRLEN_P(uservar), uservar, Z_REFCOUNT_P(uservar));
            break;
        case IS_RESOURCE:
            fprintf(phalcon_log, "Resource\n");
            break;
        case IS_ARRAY:
            fprintf(phalcon_log, "Array at %p, refcount=%d\n", uservar, Z_REFCOUNT_P(uservar));
            break;
        case IS_OBJECT:
            fprintf(phalcon_log, "Object <%s> at %p\n", Z_OBJCE_P(uservar)->name, uservar);
            break;
        default:
            fprintf(phalcon_log, "Unknown\n");
    }
    return SUCCESS;
}

/**
 * Internal fast zval dump
 */
int phalcon_vpdump(const zval **uservar TSRMLS_DC){
     if(!uservar){
		fprintf(phalcon_log, "Null pointer");
		return SUCCESS;
     }
     switch(Z_TYPE_PP(uservar)){
        case IS_NULL:
            fprintf(phalcon_log, "NULL\n");
            break;
        case IS_BOOL:
            fprintf(phalcon_log, "Boolean: %s\n", Z_LVAL_PP(uservar) ? "TRUE" : "FALSE");
            break;
        case IS_LONG:
            fprintf(phalcon_log, "Long: %ld\n", Z_LVAL_PP(uservar));
            break;
        case IS_DOUBLE:
            fprintf(phalcon_log, "Double: %f\n", Z_DVAL_PP(uservar));
            break;
        case IS_STRING:
            fprintf(phalcon_log, "String: %s\n", Z_STRVAL_PP(uservar));
            break;
        case IS_RESOURCE:
            fprintf(phalcon_log, "Resource\n");
            break;
        case IS_ARRAY:
            fprintf(phalcon_log, "Array\n");
            break;
        case IS_OBJECT:
            fprintf(phalcon_log, "Object\n");
            break;
        default:
            fprintf(phalcon_log, "Unknown\n");
    }
    return SUCCESS;
}

int phalcon_dump_ce(zend_class_entry *ce TSRMLS_DC){
	char *message = emalloc(sizeof(char *)*120);
	if(ce){
		sprintf(message, "- ClassType => %d", ce->type);
		phalcon_step_over(message);
		if(ce->name){
			sprintf(message, "- ClassName => %s", ce->name);
			phalcon_step_over(message);
		} else {
			phalcon_step_over("- ClassName => NULL");
		}
	} else {
		phalcon_step_over("- NULL class entry :(");
	}
	return SUCCESS;
}

int phalcon_class_debug(zval *val TSRMLS_DC){
	char *message = emalloc(sizeof(char *)*120);
	zend_class_entry *ce;
	if(val){
		ce = Z_OBJCE_P(val);
		if(ce){
			sprintf(message, "- MemoryAddress => %p", val);
			phalcon_step_over(message);
			phalcon_dump_ce(ce TSRMLS_CC);
		} else {
			phalcon_step_over("- No class entry :(");
		}
	} else {
		phalcon_step_over("- this_ptr is null :(");
	}
	return SUCCESS;
}

/**
 * Append debug information to file
 */
int phalcon_debug_str(char *what, char *message){
	fprintf(phalcon_log, "%s", what);
	fprintf(phalcon_log, "%s", message);
	fprintf(phalcon_log, "\n");
	return SUCCESS;
}

int phalcon_debug_long(char *what, uint vlong){
	fprintf(phalcon_log, "%s", what);
	fprintf(phalcon_log, "%u", vlong);
	fprintf(phalcon_log, "\n");
	return SUCCESS;
}

int phalcon_debug_screen(char *message){
	phalcon_debug_space();
	fprintf(phalcon_log, "%s", message);
	fprintf(phalcon_log, "\n");
	return SUCCESS;
}

int phalcon_debug_method_call(zval *obj, char *method_name TSRMLS_DC){
	if(Z_TYPE_P(obj)==IS_OBJECT){
		phalcon_debug_space();
	} else {
		phalcon_error_space();
	}
	if(Z_TYPE_P(obj)==IS_OBJECT){
		fprintf(phalcon_log, "Calling method %s::%s on Object at %p\n", Z_OBJCE_P(obj)->name, method_name, obj);
	} else {
		fprintf(phalcon_log, "Calling method %s on non object :(\n", method_name);
	}
	return SUCCESS;
}

int phalcon_error_space(){
	int i;
	fprintf(phalcon_log, "[ERROR] ");
	for(i=0;i<phalcon_debug_trace;i++){
		fprintf(phalcon_log, " ");
	}
	return SUCCESS;
}

int phalcon_debug_space(){
	int i;
	fprintf(phalcon_log, "[DEBUG] ");
	for(i=0;i<phalcon_debug_trace;i++){
		fprintf(phalcon_log, " ");
	}
	return SUCCESS;
}

int phalcon_debug_param(zval *param TSRMLS_DC){
	phalcon_debug_space();
	fprintf(phalcon_log, "Push method Param > ");
	phalcon_vdump(param TSRMLS_CC);
	return SUCCESS;
}

int phalcon_debug_vdump(char *preffix, zval *value TSRMLS_DC){
	phalcon_debug_space();
	fprintf(phalcon_log, "%s", preffix);
	phalcon_vdump(value TSRMLS_CC);
	return SUCCESS;
}

int phalcon_debug_assign(char *name, zval *value TSRMLS_DC){
	phalcon_debug_space();
	fprintf(phalcon_log, "Assign on %s with ", name);
	phalcon_vdump(value TSRMLS_CC);
	return SUCCESS;
}

int phalcon_step_over(char *message){
	phalcon_debug_screen(message);
	return SUCCESS;
}

int phalcon_step_into(char *message){
	phalcon_debug_trace++;
	phalcon_debug_screen(message);
	return SUCCESS;
}

int phalcon_step_out(char *message){
	phalcon_debug_screen(message);
	phalcon_debug_trace--;
	return SUCCESS;
}

/**
 * Prints internal debug backtrace
 */
int phalcon_debug_backtrace_internal(){
	int step = 0;
	char *message;
	phalcon_debug_entry *ptr = active;
	while(ptr){
		message = (char *) emalloc(sizeof(char)*(strlen(ptr->class_name)+strlen(ptr->method_name)+13));
		phalcon_spprintf(&message, 0, "#%d %s::%s", step, ptr->class_name, ptr->method_name);
		phalcon_debug_screen(message);
		efree(message);
		ptr = ptr->prev;
		step++;
	}
}

/**
 * Appends a debug entry to internal execution scope
 */
int phalcon_step_into_entry(char *class_name, char *method_name, int lineno){
	char *message;
	phalcon_debug_entry *entry;
	if(!start){
		start = (phalcon_debug_entry *) emalloc(sizeof(phalcon_debug_entry));
		start->class_name = "__main__";
		start->method_name = "__init__";
		start->lineno = 0;
		start->prev = NULL;
		start->next = NULL;
		active = start;
	}

	message = (char *) emalloc(sizeof(char)*(strlen(class_name)+strlen(method_name)+14));
	phalcon_spprintf(&message, 0, "Step Into %s::%s", class_name, method_name);
	phalcon_debug_screen(message);
	efree(message);

	entry = (phalcon_debug_entry *) emalloc(sizeof(phalcon_debug_entry));
	entry->class_name = class_name;
	entry->method_name = method_name;
	entry->lineno = lineno;
	entry->prev = active;
	active->next = entry;
	active = entry;
	phalcon_debug_trace++;

	return SUCCESS;
}

/**
 * Steps out current stack
 */
int phalcon_step_out_entry(){
	char *message;
	phalcon_debug_entry *prev;
	if(active){

		phalcon_debug_trace--;
		message = (char *) emalloc(sizeof(char)*(strlen(active->class_name)+strlen(active->method_name)+13));
		phalcon_spprintf(&message, 0, "Step out %s::%s", active->class_name, active->method_name);
		phalcon_debug_screen(message);
		efree(message);

		prev = active->prev;
		efree(active);
		active = prev;

	} else {
		fprintf(phalcon_log, "Problem, stack?");
		return FAILURE;
	}
	return SUCCESS;
}

#endif
#ifdef HAVE_CONFIG_H
#endif


#ifndef PHALCON_RELEASE

int phalcon_assert_class(zval *object, char *class_name TSRMLS_DC){
	if(object){
		if(Z_TYPE_P(object)!=IS_OBJECT){
			phalcon_error_space();
			fprintf(phalcon_log, "AssertClass: [Failed] Value is not an object\n");
			return FAILURE;
		} else {
			if(strcmp(Z_OBJCE_P(object)->name, class_name)){
				phalcon_error_space();
				fprintf(phalcon_log, "AssertClass: [Failed] Object is not class %s, is %s\n", class_name, Z_OBJCE_P(object)->name);
				return FAILURE;
			}
		}
	}
	return SUCCESS;
}

#endif#ifdef HAVE_CONFIG_H
#endif

#ifdef PHP_WIN32
#endif

/**
 * Copy on write for not tmp variables to be used as parameters
 */
int phalcon_cpy_wrt_param(zval **param, zval *value){
	if(Z_TYPE_P(value)==IS_OBJECT){
		Z_ADDREF_P(value);
		*param = value;
	} else {
		INIT_PARAM(*param);
		ALLOC_INIT_ZVAL(*param);
		ZVAL_ZVAL(*param, value, 1, 0);
	}
	return SUCCESS;
}

/**
 * Check if class is instance of
 */
int phalcon_instance_of(zval *result, const zval *object, const zend_class_entry *ce TSRMLS_DC){
	if(Z_TYPE_P(object)!=IS_OBJECT){
		zend_error(E_ERROR, "instanceof expects an object instance, constant given");
		return FAILURE;
    } else {
		ZVAL_BOOL(result, instanceof_function(Z_OBJCE_P(object), ce TSRMLS_CC));
    }
    return SUCCESS;
}

/**
 * Check if a class exists
 *
 * @TODO Unfortunately doesn't works
 */
int phalcon_class_exists(zval *return_value, zval *class_name_zval, zval *autoload_zval TSRMLS_DC){

	/*char *class_name;
	int class_name_len;
	zend_class_entry ***ce;
	ulong hash;
	ALLOCA_FLAG(use_heap)

	switch(Z_TYPE_P(class_name_zval)){
		case IS_ARRAY:
		case IS_OBJECT:
		case IS_RESOURCE:
			zend_error_noreturn(E_ERROR, "Invalid parameter for class_exists");
			return FAILURE;
	}

	if(Z_TYPE_P(class_name_zval)!=IS_STRING){
		convert_to_string(class_name_zval);
	}

	class_name = estrndup(Z_STRVAL_P(class_name_zval), Z_STRLEN_P(class_name_zval));
	class_name_len = strlen(class_name);

	zend_str_tolower(class_name, class_name_len);

	hash = zend_inline_hash_func(class_name, class_name_len);
	if(zend_hash_quick_find(EG(class_table), class_name, class_name_len, hash, (void **) ce) == SUCCESS) {
		free_alloca(class_name, use_heap);
		return SUCCESS;
	}

	free_alloca(class_name, use_heap);*/
	return FAILURE;

}

/**
 * Clones an object from obj to dst
 */
int phalcon_clone(zval **dst, zval *obj TSRMLS_DC){

	zval *destiny;
	zend_class_entry *ce;
	//zend_function *clone;
	zend_object_clone_obj_t clone_call;

	if(Z_TYPE_P(obj)!=IS_OBJECT){
		zend_error_noreturn(E_ERROR, "__clone method called on non-object");
		return FAILURE;
	}

	ce = Z_OBJCE_P(obj);
	//clone = ce ? ce->clone : NULL;
	clone_call =  Z_OBJ_HT_P(obj)->clone_obj;
	if(!clone_call){
		if(ce){
			zend_error_noreturn(E_ERROR, "Trying to clone an uncloneable object of class %s", ce->name);
		} else {
			zend_error_noreturn(E_ERROR, "Trying to clone an uncloneable object");
		}
		return FAILURE;
	}

	if(!EG(exception)){
		ALLOC_ZVAL(destiny);
		Z_OBJVAL_P(destiny) = clone_call(obj TSRMLS_CC);
		Z_TYPE_P(destiny) = IS_OBJECT;
		Z_SET_REFCOUNT_P(destiny, 1);
		Z_SET_ISREF_P(destiny);
		if(EG(exception)){
			zval_ptr_dtor(&destiny);
			destiny = NULL;
		}
	}

	if(destiny){
		*dst = destiny;
	}

	return SUCCESS;
}

/**
 * Checks if property exists on object
 */
int phalcon_isset_property(zval *object, char *property_name, int property_length TSRMLS_DC){
	if(Z_TYPE_P(object)==IS_OBJECT){
		if(zend_hash_exists(&Z_OBJCE_P(object)->properties_info, property_name, property_length+1)){
			return 1;
		} else {
			return zend_hash_exists(Z_OBJ_HT_P(object)->get_properties(object TSRMLS_CC), property_name, property_length+1);
		}
	} else {
		return 0;
	}
}

/**
 * Checks if string property exists on object
 */
int phalcon_isset_property_zval(zval *object, zval *property TSRMLS_DC){
	if(Z_TYPE_P(object)==IS_OBJECT){
		if(Z_TYPE_P(property)==IS_STRING){
			if(zend_hash_exists(&Z_OBJCE_P(object)->properties_info, Z_STRVAL_P(property), Z_STRLEN_P(property)+1)){
				return 1;
			} else {
				return zend_hash_exists(Z_OBJ_HT_P(object)->get_properties(object TSRMLS_CC), Z_STRVAL_P(property), Z_STRLEN_P(property)+1);
			}
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

/**
 * Reads a property from an object
 */
int phalcon_read_property(zval *result, zval *object, char *property_name, int property_length, int silent TSRMLS_DC){
	zend_class_entry *ce;
	if(Z_TYPE_P(object)==IS_OBJECT){
		ce = Z_OBJCE_P(object);
		while(ce){
			if(zend_hash_exists(&ce->properties_info, property_name, property_length+1)){
				*result = *zend_read_property(ce, object, property_name, property_length, 0 TSRMLS_CC);
				return SUCCESS;
			}
			ce = ce->parent;
		}
		*result = *zend_read_property(Z_OBJCE_P(object), object, property_name, property_length, 1 TSRMLS_CC);
	} else {
		ZVAL_NULL(result);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Trying to get property of non-object");
			return FAILURE;
		}
	}
	return SUCCESS;
}

/**
 * Reads a property from an object
 */
int phalcon_read_property_zval(zval *result, zval *object, zval *property, int silent TSRMLS_DC){
	if(Z_TYPE_P(object)==IS_OBJECT){
		if(Z_TYPE_P(property)==IS_STRING){
			*result = *zend_read_property(Z_OBJCE_P(object), object, Z_STRVAL_P(property), Z_STRLEN_P(property), 0 TSRMLS_CC);
		} else {
			ZVAL_NULL(result);
		}
	} else {
		ZVAL_NULL(result);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Trying to get property of non-object");
			return FAILURE;
		}
	}
	return SUCCESS;
}

/**
 * Checks whether obj is an object and updates property with long value
 */
int phalcon_update_property_long(zval *obj, char *property_name, int property_length, long value TSRMLS_DC){
	if(Z_TYPE_P(obj)!=IS_OBJECT){
		zend_error_noreturn(E_ERROR, "Attempt to assign property of non-object");
		return FAILURE;
	} else {
		zend_update_property_long(Z_OBJCE_P(obj), obj, property_name, property_length, value TSRMLS_CC);
	}
	return SUCCESS;
}

/**
 * Checks whether obj is an object and updates property with string value
 */
int phalcon_update_property_string(zval *obj, char *property_name, int property_length, char *value TSRMLS_DC){
	if(Z_TYPE_P(obj)!=IS_OBJECT){
		zend_error_noreturn(E_ERROR, "Attempt to assign property of non-object");
		return FAILURE;
	} else {
		zend_update_property_string(Z_OBJCE_P(obj), obj, property_name, property_length, value TSRMLS_CC);
	}
	return SUCCESS;
}

/**
 * Checks wheter obj is an object and updates property with another zval
 */
int phalcon_update_property_zval(zval *obj, char *property_name, int property_length, zval *value TSRMLS_DC){
	if(Z_TYPE_P(obj)!=IS_OBJECT){
		zend_error_noreturn(E_ERROR, "Attempt to assign property of non-object");
		return FAILURE;
	} else {
		Z_ADDREF_P(value);
		zend_update_property(Z_OBJCE_P(obj), obj, property_name, property_length, value TSRMLS_CC);
	}
	return SUCCESS;
}

/**
 * Check if method exists on certain object
 */
int phalcon_method_exists(zval *object, zval *method_name TSRMLS_DC){

	char *lcname;

	if(Z_TYPE_P(object)!=IS_OBJECT){
		return FAILURE;
	}

	if(Z_TYPE_P(method_name)!=IS_STRING){
		return FAILURE;
	}

	lcname = zend_str_tolower_dup(Z_STRVAL_P(method_name), Z_STRLEN_P(method_name));
	if(zend_hash_exists(&Z_OBJCE_P(object)->function_table, lcname, Z_STRLEN_P(method_name)+1)) {
		efree(lcname);
		return SUCCESS;
	}

	return FAILURE;
}
#ifdef HAVE_CONFIG_H
#endif


/**
 * Check if index exists on an array zval
 */
int phalcon_array_isset(const zval *arr, zval *index){

	zval *copy;

	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}

	if(Z_TYPE_P(index)==IS_NULL){
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, index, 1, 0);
		convert_to_string(copy);
		index = copy;
	} else {
		if(Z_TYPE_P(index)==IS_BOOL||Z_TYPE_P(index)==IS_DOUBLE){
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, index, 1, 0);
			convert_to_long(copy);
			index = copy;
		}
	}

	if(Z_TYPE_P(index)==IS_STRING){
		return zend_hash_exists(Z_ARRVAL_P(arr), Z_STRVAL_P(index), Z_STRLEN_P(index)+1);
	} else {
		return zend_hash_index_exists(Z_ARRVAL_P(arr), Z_LVAL_P(index));
	}

	return 0;
}

/**
 * Check if char index exists on an array zval
 */
int phalcon_array_isset_string(const zval *arr, char *index, uint index_length){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}
	return zend_hash_exists(Z_ARRVAL_P(arr), index, index_length);
}

/**
 * Check if char index exists on an array zval
 */
int phalcon_array_isset_long(const zval *arr, ulong index){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}
	return zend_hash_index_exists(Z_ARRVAL_P(arr), index);
}

/**
 * Unsets zval index from array
 */
int phalcon_array_unset(zval *arr, zval *index){

	zval *copy;

	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}

	if(Z_TYPE_P(index)==IS_NULL){
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, index, 1, 0);
		convert_to_string(copy);
		index = copy;
	} else {
		if(Z_TYPE_P(index)==IS_BOOL||Z_TYPE_P(index)==IS_DOUBLE){
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, index, 1, 0);
			convert_to_long(copy);
			index = copy;
		}
	}

	if(Z_TYPE_P(index)==IS_STRING){
		return zend_hash_del(Z_ARRVAL_P(arr), Z_STRVAL_P(index), Z_STRLEN_P(index)+1);
	} else {
		return zend_hash_index_del(Z_ARRVAL_P(arr), Z_LVAL_P(index));
	}

	return 0;
}

/**
 * Unsets string index from array
 */
int phalcon_array_unset_string(zval *arr, char *index, uint index_length){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}
	return zend_hash_del(Z_ARRVAL_P(arr), index, index_length);
}

/**
 * Unsets long index from array
 */
int phalcon_array_unset_long(zval *arr, ulong index){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		return 0;
	}
	return zend_hash_index_del(Z_ARRVAL_P(arr), index);
}

/**
 * Push one or more elements onto the end of array
 */
int phalcon_array_append(zval *arr, zval *value TSRMLS_DC){
	if(Z_TYPE_P(arr)==IS_ARRAY){
		return add_next_index_zval(arr, value);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
	}
	return FAILURE;
}

/**
 * Updates values on arrays by string or long indexes
 */
int phalcon_array_update(zval *arr, zval *index, zval *value TSRMLS_DC){

	if(Z_TYPE_P(arr)!=IS_ARRAY){
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
		return FAILURE;
	}

	if(Z_TYPE_P(index)==IS_NULL){
		convert_to_string(index);
	} else {
		if(Z_TYPE_P(index)==IS_BOOL||Z_TYPE_P(index)==IS_DOUBLE){
			convert_to_long(index);
		}
	}

 	if(Z_TYPE_P(index)==IS_STRING){
		return zend_hash_update(Z_ARRVAL_P(arr), Z_STRVAL_P(index), Z_STRLEN_P(index)+1, &value, sizeof(zval *), NULL);
	} else {
		if(Z_TYPE_P(index)==IS_LONG){
			return zend_hash_index_update(Z_ARRVAL_P(arr), Z_LVAL_P(index), &value, sizeof(zval *), NULL);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Illegal offset type");
		}
		return FAILURE;
	}

	return FAILURE;
}

/**
 * Updates values on arrays by string indexes only
 */
int phalcon_array_update_string(zval *arr, char *index, uint index_length, zval *value TSRMLS_DC){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
		return FAILURE;
	}
	Z_ADDREF_P(value);
	return zend_hash_update(Z_ARRVAL_P(arr), index, index_length+1, &value, sizeof(zval *), NULL);
}

/**
 * Updates values on arrays by long indexes only
 */
int phalcon_array_update_long(zval *arr, ulong index, zval *value TSRMLS_DC){
	if(Z_TYPE_P(arr)!=IS_ARRAY){
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
		return FAILURE;
	}
	Z_ADDREF_P(value);
	return zend_hash_index_update(Z_ARRVAL_P(arr), index, &value, sizeof(zval *), NULL);
}

/**
 * Reads an item from an array using a zval as index
 */
int phalcon_array_fetch(zval *return_value, const zval *arr, zval *index, int silent TSRMLS_DC){

	zval **zv;
	int result = FAILURE, type;

 	if(Z_TYPE_P(index)==IS_ARRAY||Z_TYPE_P(index)==IS_OBJECT){
		ZVAL_NULL(return_value);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Illegal offset type");
		}
		return FAILURE;
	}

 	if(Z_TYPE_P(index)==IS_NULL){
		convert_to_string(index);
	}

	if(Z_TYPE_P(arr)==IS_NULL||Z_TYPE_P(arr)==IS_BOOL){
		ZVAL_NULL(return_value);
		return FAILURE;
	}

	if(Z_TYPE_P(index)!=IS_STRING && Z_TYPE_P(index)!=IS_LONG && Z_TYPE_P(index)!=IS_DOUBLE){
		ZVAL_NULL(return_value);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Illegal offset type");
		}
		return FAILURE;
	}

 	if(Z_TYPE_P(index)==IS_STRING){
       	if((type = is_numeric_string(Z_STRVAL_P(index), Z_STRLEN_P(index), NULL, NULL, 0))){
			if(type==IS_LONG){
				convert_to_long(index);
			}
		}
	} else {
		if(Z_TYPE_P(index)==IS_DOUBLE){
			convert_to_long(index);
		}
	}

 	if(Z_TYPE_P(index)==IS_STRING){
		if((result = zend_hash_find(Z_ARRVAL_P(arr), Z_STRVAL_P(index), Z_STRLEN_P(index)+1, (void**)&zv))==SUCCESS){
			*return_value = **zv;
			if(Z_TYPE_P(return_value)>IS_BOOL){
				zval_copy_ctor(return_value);
			}
			return SUCCESS;
		}
	}

 	if(Z_TYPE_P(index)==IS_LONG){
		if((result = zend_hash_index_find(Z_ARRVAL_P(arr), Z_LVAL_P(index), (void**)&zv))==SUCCESS){
			*return_value = **zv;
			if(Z_TYPE_P(return_value)>IS_BOOL){
				zval_copy_ctor(return_value);
			}
			return SUCCESS;
		}
	}

	if(silent==PHALCON_NOISY_FETCH){
		if(Z_TYPE_P(index)==IS_LONG){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Undefined index: %ld", Z_LVAL_P(index));
		} else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Undefined index: %s", Z_STRVAL_P(index));
		}
	}

	ZVAL_NULL(return_value);

	return FAILURE;
}

/**
 * Reads an item from an array using a string as index
 */
int phalcon_array_fetch_string(zval *return_value, const zval *arr, char *index, uint index_length, int silent TSRMLS_DC){

	zval **zv;
	int result = FAILURE;

	if(Z_TYPE_P(arr)!=IS_ARRAY){
		ZVAL_NULL(return_value);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
		}
		return FAILURE;
	}

	if((result = zend_hash_find(Z_ARRVAL_P(arr), index, index_length+1, (void**)&zv))==SUCCESS){
		*return_value = **zv;
		if(Z_TYPE_P(return_value)>IS_BOOL){
			zval_copy_ctor(return_value);
		}
		return SUCCESS;
	}

	if(silent==PHALCON_NOISY_FETCH){
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Undefined index: %s", index);
	}

	ZVAL_NULL(return_value);

	return FAILURE;

}

/**
 * Reads an item from an array using a long as index
 */
int phalcon_array_fetch_long(zval *return_value, const zval *arr, ulong index, int silent TSRMLS_DC){

	zval **zv;
	int result = FAILURE;

	if(Z_TYPE_P(arr)!=IS_ARRAY){
		ZVAL_NULL(return_value);
		if(silent==PHALCON_NOISY_FETCH){
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Cannot use a scalar value as an array");
		}
		return FAILURE;
	}

	if((result = zend_hash_index_find(Z_ARRVAL_P(arr), index, (void**)&zv))==SUCCESS){
		*return_value = **zv;
		if(Z_TYPE_P(return_value)>IS_BOOL){
			zval_copy_ctor(return_value);
		}
		return SUCCESS;
	}

	if(silent==PHALCON_NOISY_FETCH){
		php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Undefined index: %ld", index);
	}

	ZVAL_NULL(return_value);

	return FAILURE;

}
/**
 * Php_Dispatcher
 *
 * Dispatching is the process of taking the request object, extracting the module name,
 * controller name, action name, and optional parameters contained in it, and then
 * instantiating a controller and calling an action of that controller.
 *
 * 
 */

PHP_METHOD(Phalcon_Dispatcher, __construct){

	zval *a0 = NULL, *a1 = NULL;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_property(phalcon_dispatcher_class_entry, this_ptr, "_controllers", strlen("_controllers"), a0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(a1);
	array_init(a1);
	zend_update_property(phalcon_dispatcher_class_entry, this_ptr, "_params", strlen("_params"), a1 TSRMLS_CC);
}

/**
 * Sets default controllers directory
 *
 * @param string $controllersDir
 */
PHP_METHOD(Phalcon_Dispatcher, setControllersDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_controllersDir", strlen("_controllersDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets active controllers directory
 *
 * @return string
 */
PHP_METHOD(Phalcon_Dispatcher, getControllersDir){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_controllersDir", sizeof("_controllersDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets base path for controllers dir
 *
 * @param string $basePath
 */
PHP_METHOD(Phalcon_Dispatcher, setBasePath){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_basePath", strlen("_basePath"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets base path for controllers dir
 *
 * @return string
 */
PHP_METHOD(Phalcon_Dispatcher, getBasePath){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_basePath", sizeof("_basePath")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets the controller name to be dispatched
 */
PHP_METHOD(Phalcon_Dispatcher, setControllerName){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_controllerName", strlen("_controllerName"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets last dispacthed controller name
 *
 * @return string
 */
PHP_METHOD(Phalcon_Dispatcher, getControllerName){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_controllerName", sizeof("_controllerName")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets the action name to be dispatched
 *
 * @param string $actionName
 */
PHP_METHOD(Phalcon_Dispatcher, setActionName){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets last dispacthed action name
 *
 * @return string
 */
PHP_METHOD(Phalcon_Dispatcher, getActionName){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_actionName", sizeof("_actionName")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets action params to be dispatch
 *
 * @param array $params
 */
PHP_METHOD(Phalcon_Dispatcher, setParams){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_params", strlen("_params"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets action params
     *
 * @return array
 */
PHP_METHOD(Phalcon_Dispatcher, getParams){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Dispatchs a action controller taking care of routing
 *
 * @param Php_Request $request
 * @param Php_Response $response
 * @param Php_View $view
 * @param Php_Model_Manager $model
 * @return Php_Controller
 */
PHP_METHOD(Phalcon_Dispatcher, dispatch){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL, *v10 = NULL, *v11 = NULL, *v12 = NULL, *v13 = NULL;
	zval *v14 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL, *t11 = NULL, *t12 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL;
	zval *i0 = NULL, *i1 = NULL, *i2 = NULL, *i3 = NULL, *i4 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL }, *p4[] = { NULL, NULL, NULL, NULL, NULL }, *p6[] = { NULL, NULL }, *p7[] = { NULL, NULL }, *p8[] = { NULL }, *p9[] = { NULL };
	int eval_int;
	zend_class_entry *ce0;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|zz", &v0, &v1, &v2, &v3) == FAILURE) {
		RETURN_NULL();
	}

	if (!v2) {
		PHALCON_INIT_NULL(v2);
	}
	if (!v3) {
		PHALCON_INIT_NULL(v3);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_basePath", sizeof("_basePath")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_controllersDir", sizeof("_controllersDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	PHALCON_ALLOC_ZVAL(r0);
	concat_function(r0, t0, t1 TSRMLS_CC);
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r0);
	v4 = r0;
	PHALCON_INIT_NULL(v5);
	PHALCON_INIT_NULL(v6);
	PHALCON_ALLOC_ZVAL(v7);
	ZVAL_LONG(v7, 0);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_finished", strlen("_finished"), 0 TSRMLS_CC);
	ws0:
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_finished", sizeof("_finished")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	PHALCON_BOOLEAN_NOT_FUNCTION(r1, t2);
	if (!zend_is_true(r1)) {
		goto we0;
	}
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_finished", strlen("_finished"), 1 TSRMLS_CC);
	if (!t3) {
		PHALCON_ALLOC_ZVAL(t3);
	} else {
		if (Z_REFCOUNT_P(t3) > 1) {
			{
				zval *orig_ptr = t3;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t3);
					*t3 = *orig_ptr;
					zval_copy_ctor(t3);
					Z_SET_REFCOUNT_P(t3, 1);
					Z_UNSET_ISREF_P(t3);
				}
			}
		} else {
			FREE_ZVAL(t3);
			PHALCON_ALLOC_ZVAL(t3);
		}
	}
	phalcon_read_property(t3, this_ptr, "_controllerName", sizeof("_controllerName")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	if (v8) {
		Z_DELREF_P(v8);
		if (!Z_REFCOUNT_P(v8)) {
			FREE_ZVAL(v8);
		}
	}
	Z_ADDREF_P(t3);
	v8 = t3;
	if (!zend_is_true(v8)) {
		if (!t4) {
			PHALCON_ALLOC_ZVAL(t4);
		} else {
			if (Z_REFCOUNT_P(t4) > 1) {
				{
					zval *orig_ptr = t4;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t4);
						*t4 = *orig_ptr;
						zval_copy_ctor(t4);
						Z_SET_REFCOUNT_P(t4, 1);
						Z_UNSET_ISREF_P(t4);
					}
				}
			} else {
				FREE_ZVAL(t4);
				PHALCON_ALLOC_ZVAL(t4);
			}
		}
		phalcon_read_property(t4, this_ptr, "_defaultController", sizeof("_defaultController")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t4);
		if (v8) {
			Z_DELREF_P(v8);
			if (!Z_REFCOUNT_P(v8)) {
				FREE_ZVAL(v8);
			}
		}
		Z_ADDREF_P(t4);
		v8 = t4;
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v8, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_controllerName", strlen("_controllerName"), copy TSRMLS_CC);
		}
	}
	if (!t5) {
		PHALCON_ALLOC_ZVAL(t5);
	} else {
		if (Z_REFCOUNT_P(t5) > 1) {
			{
				zval *orig_ptr = t5;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t5);
					*t5 = *orig_ptr;
					zval_copy_ctor(t5);
					Z_SET_REFCOUNT_P(t5, 1);
					Z_UNSET_ISREF_P(t5);
				}
			}
		} else {
			FREE_ZVAL(t5);
			PHALCON_ALLOC_ZVAL(t5);
		}
	}
	phalcon_read_property(t5, this_ptr, "_controllers", sizeof("_controllers")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t5);
	if (v9) {
		Z_DELREF_P(v9);
		if (!Z_REFCOUNT_P(v9)) {
			FREE_ZVAL(v9);
		}
	}
	Z_ADDREF_P(t5);
	v9 = t5;
	if (!r2) {
		PHALCON_ALLOC_ZVAL(r2);
	} else {
		if (Z_REFCOUNT_P(r2) > 1) {
			{
				zval *orig_ptr = r2;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(r2);
					*r2 = *orig_ptr;
					zval_copy_ctor(r2);
					Z_SET_REFCOUNT_P(r2, 1);
					Z_UNSET_ISREF_P(r2);
				}
			}
		} else {
			FREE_ZVAL(r2);
			PHALCON_ALLOC_ZVAL(r2);
		}
	}
	Z_ADDREF_P(v8);
	p0[0] = v8;
	PHALCON_CALL_STATIC_PARAMS(r2, "phalcon_utils", "camelize", 1, p0);
	if (!r3) {
		PHALCON_ALLOC_ZVAL(r3);
	} else {
		if (Z_REFCOUNT_P(r3) > 1) {
			{
				zval *orig_ptr = r3;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(r3);
					*r3 = *orig_ptr;
					zval_copy_ctor(r3);
					Z_SET_REFCOUNT_P(r3, 1);
					Z_UNSET_ISREF_P(r3);
				}
			}
		} else {
			FREE_ZVAL(r3);
			PHALCON_ALLOC_ZVAL(r3);
		}
	}
	phalcon_concat_right(r3, r2, "Controller" TSRMLS_CC);
	if (v10) {
		Z_DELREF_P(v10);
		if (!Z_REFCOUNT_P(v10)) {
			FREE_ZVAL(v10);
		}
	}
	Z_ADDREF_P(r3);
	v10 = r3;
	eval_int = phalcon_array_isset(v9, v10);
	if (!eval_int) {
		if (!r4) {
			PHALCON_ALLOC_ZVAL(r4);
		} else {
			if (Z_REFCOUNT_P(r4) > 1) {
				{
					zval *orig_ptr = r4;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r4);
						*r4 = *orig_ptr;
						zval_copy_ctor(r4);
						Z_SET_REFCOUNT_P(r4, 1);
						Z_UNSET_ISREF_P(r4);
					}
				}
			} else {
				FREE_ZVAL(r4);
				PHALCON_ALLOC_ZVAL(r4);
			}
		}
		concat_function(r4, v4, v10 TSRMLS_CC);
		if (!r5) {
			PHALCON_ALLOC_ZVAL(r5);
		} else {
			if (Z_REFCOUNT_P(r5) > 1) {
				{
					zval *orig_ptr = r5;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r5);
						*r5 = *orig_ptr;
						zval_copy_ctor(r5);
						Z_SET_REFCOUNT_P(r5, 1);
						Z_UNSET_ISREF_P(r5);
					}
				}
			} else {
				FREE_ZVAL(r5);
				PHALCON_ALLOC_ZVAL(r5);
			}
		}
		phalcon_concat_right(r5, r4, ".php" TSRMLS_CC);
		if (v11) {
			Z_DELREF_P(v11);
			if (!Z_REFCOUNT_P(v11)) {
				FREE_ZVAL(v11);
			}
		}
		Z_ADDREF_P(r5);
		v11 = r5;
		if (phalcon_file_exists(v11 TSRMLS_CC) == SUCCESS) {
			phalcon_require(v11 TSRMLS_CC);
			if (EG(exception) || EG(exit_status) == 255) {
				return;
			}
		} else {
			if (!i0) {
				PHALCON_ALLOC_ZVAL(i0);
			} else {
				if (Z_REFCOUNT_P(i0) > 1) {
					{
						zval *orig_ptr = i0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i0);
							*i0 = *orig_ptr;
							zval_copy_ctor(i0);
							Z_SET_REFCOUNT_P(i0, 1);
							Z_UNSET_ISREF_P(i0);
						}
					}
				} else {
					FREE_ZVAL(i0);
					PHALCON_ALLOC_ZVAL(i0);
				}
			}
			object_init_ex(i0, phalcon_exception_class_entry);
			if (!r6) {
				PHALCON_ALLOC_ZVAL(r6);
			} else {
				if (Z_REFCOUNT_P(r6) > 1) {
					{
						zval *orig_ptr = r6;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r6);
							*r6 = *orig_ptr;
							zval_copy_ctor(r6);
							Z_SET_REFCOUNT_P(r6, 1);
							Z_UNSET_ISREF_P(r6);
						}
					}
				} else {
					FREE_ZVAL(r6);
					PHALCON_ALLOC_ZVAL(r6);
				}
			}
			phalcon_concat_both(r6,  "File for controller class ", v10, " doesn't exists" TSRMLS_CC);
			Z_ADDREF_P(r6);
			p1[0] = r6;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		}
		if (!r7) {
			PHALCON_ALLOC_ZVAL(r7);
		} else {
			if (Z_REFCOUNT_P(r7) > 1) {
				{
					zval *orig_ptr = r7;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r7);
						*r7 = *orig_ptr;
						zval_copy_ctor(r7);
						Z_SET_REFCOUNT_P(r7, 1);
						Z_UNSET_ISREF_P(r7);
					}
				}
			} else {
				FREE_ZVAL(r7);
				PHALCON_ALLOC_ZVAL(r7);
			}
		}
		Z_ADDREF_P(v10);
		p2[0] = v10;
		PHALCON_PARAM_BOOL(p2[1], 0);
		PHALCON_CALL_FUNC_PARAMS(r7, "class_exists", 2, p2);
		if (!zend_is_true(r7)) {
			if (!i1) {
				PHALCON_ALLOC_ZVAL(i1);
			} else {
				if (Z_REFCOUNT_P(i1) > 1) {
					{
						zval *orig_ptr = i1;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i1);
							*i1 = *orig_ptr;
							zval_copy_ctor(i1);
							Z_SET_REFCOUNT_P(i1, 1);
							Z_UNSET_ISREF_P(i1);
						}
					}
				} else {
					FREE_ZVAL(i1);
					PHALCON_ALLOC_ZVAL(i1);
				}
			}
			object_init_ex(i1, phalcon_exception_class_entry);
			if (!r8) {
				PHALCON_ALLOC_ZVAL(r8);
			} else {
				if (Z_REFCOUNT_P(r8) > 1) {
					{
						zval *orig_ptr = r8;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r8);
							*r8 = *orig_ptr;
							zval_copy_ctor(r8);
							Z_SET_REFCOUNT_P(r8, 1);
							Z_UNSET_ISREF_P(r8);
						}
					}
				} else {
					FREE_ZVAL(r8);
					PHALCON_ALLOC_ZVAL(r8);
				}
			}
			phalcon_concat_both(r8,  "Class ", v10, " was not found on controller file" TSRMLS_CC);
			Z_ADDREF_P(r8);
			p3[0] = r8;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p3, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i1 TSRMLS_CC);
			Z_ADDREF_P(i1);
			return;
		}
		ce0 = zend_fetch_class(Z_STRVAL_P(v10), Z_STRLEN_P(v10), ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
		if (!i2) {
			PHALCON_ALLOC_ZVAL(i2);
		} else {
			if (Z_REFCOUNT_P(i2) > 1) {
				{
					zval *orig_ptr = i2;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(i2);
						*i2 = *orig_ptr;
						zval_copy_ctor(i2);
						Z_SET_REFCOUNT_P(i2, 1);
						Z_UNSET_ISREF_P(i2);
					}
				}
			} else {
				FREE_ZVAL(i2);
				PHALCON_ALLOC_ZVAL(i2);
			}
		}
		object_init_ex(i2, ce0);
		Z_ADDREF_P(this_ptr);
		p4[0] = this_ptr;
		Z_ADDREF_P(v0);
		p4[1] = v0;
		Z_ADDREF_P(v1);
		p4[2] = v1;
		Z_ADDREF_P(v2);
		p4[3] = v2;
		Z_ADDREF_P(v3);
		p4[4] = v3;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i2, "__construct", 5, p4, PHALCON_CALL_CHECK);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(i2);
		v6 = i2;
		if (!t6) {
			PHALCON_ALLOC_ZVAL(t6);
		} else {
			if (Z_REFCOUNT_P(t6) > 1) {
				SEPARATE_ZVAL(&t6);
			} else {
				if (Z_TYPE_P(t6) != IS_STRING) {
					FREE_ZVAL(t6);
					PHALCON_ALLOC_ZVAL(t6);
				}
			}
		}
		ZVAL_STRING(t6, "initialize", 0);
		if (phalcon_method_exists(v6, t6 TSRMLS_CC) == SUCCESS) {
			PHALCON_CALL_METHOD_NORETURN(v6, "initialize", PHALCON_CALL_DEFAULT);
		}
		if (!t7) {
			PHALCON_ALLOC_ZVAL(t7);
		} else {
			if (Z_REFCOUNT_P(t7) > 1) {
				{
					zval *orig_ptr = t7;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t7);
						*t7 = *orig_ptr;
						zval_copy_ctor(t7);
						Z_SET_REFCOUNT_P(t7, 1);
						Z_UNSET_ISREF_P(t7);
					}
				}
			} else {
				FREE_ZVAL(t7);
				PHALCON_ALLOC_ZVAL(t7);
			}
		}
		phalcon_read_property(t7, this_ptr, "_controllers", sizeof("_controllers")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t7);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v6, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = t7;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t7);
					*t7 = *orig_ptr;
					zval_copy_ctor(t7);
					Z_SET_REFCOUNT_P(t7, 1);
					Z_UNSET_ISREF_P(t7);
				}
			}
			phalcon_array_update(t7, v10, copy TSRMLS_CC);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t7, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_controllers", strlen("_controllers"), copy TSRMLS_CC);
		}
	} else {
		if (!r9) {
			PHALCON_ALLOC_ZVAL(r9);
		} else {
			if (Z_REFCOUNT_P(r9) > 1) {
				{
					zval *orig_ptr = r9;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r9);
						*r9 = *orig_ptr;
						zval_copy_ctor(r9);
						Z_SET_REFCOUNT_P(r9, 1);
						Z_UNSET_ISREF_P(r9);
					}
				}
			} else {
				FREE_ZVAL(r9);
				PHALCON_ALLOC_ZVAL(r9);
			}
		}
		phalcon_array_fetch(r9, v9, v10, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(r9);
		v6 = r9;
	}
	if (!t8) {
		PHALCON_ALLOC_ZVAL(t8);
	} else {
		if (Z_REFCOUNT_P(t8) > 1) {
			{
				zval *orig_ptr = t8;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t8);
					*t8 = *orig_ptr;
					zval_copy_ctor(t8);
					Z_SET_REFCOUNT_P(t8, 1);
					Z_UNSET_ISREF_P(t8);
				}
			}
		} else {
			FREE_ZVAL(t8);
			PHALCON_ALLOC_ZVAL(t8);
		}
	}
	phalcon_read_property(t8, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t8);
	if (v12) {
		Z_DELREF_P(v12);
		if (!Z_REFCOUNT_P(v12)) {
			FREE_ZVAL(v12);
		}
	}
	Z_ADDREF_P(t8);
	v12 = t8;
	if (!t9) {
		PHALCON_ALLOC_ZVAL(t9);
	} else {
		if (Z_REFCOUNT_P(t9) > 1) {
			{
				zval *orig_ptr = t9;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t9);
					*t9 = *orig_ptr;
					zval_copy_ctor(t9);
					Z_SET_REFCOUNT_P(t9, 1);
					Z_UNSET_ISREF_P(t9);
				}
			}
		} else {
			FREE_ZVAL(t9);
			PHALCON_ALLOC_ZVAL(t9);
		}
	}
	phalcon_read_property(t9, this_ptr, "_actionName", sizeof("_actionName")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t9);
	if (v13) {
		Z_DELREF_P(v13);
		if (!Z_REFCOUNT_P(v13)) {
			FREE_ZVAL(v13);
		}
	}
	Z_ADDREF_P(t9);
	v13 = t9;
	if (!zend_is_true(v13)) {
		if (!t10) {
			PHALCON_ALLOC_ZVAL(t10);
		} else {
			if (Z_REFCOUNT_P(t10) > 1) {
				{
					zval *orig_ptr = t10;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t10);
						*t10 = *orig_ptr;
						zval_copy_ctor(t10);
						Z_SET_REFCOUNT_P(t10, 1);
						Z_UNSET_ISREF_P(t10);
					}
				}
			} else {
				FREE_ZVAL(t10);
				PHALCON_ALLOC_ZVAL(t10);
			}
		}
		phalcon_read_property(t10, this_ptr, "_defaultAction", sizeof("_defaultAction")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t10);
		if (v13) {
			Z_DELREF_P(v13);
			if (!Z_REFCOUNT_P(v13)) {
				FREE_ZVAL(v13);
			}
		}
		Z_ADDREF_P(t10);
		v13 = t10;
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v13, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
		}
	}
	if (!r10) {
		PHALCON_ALLOC_ZVAL(r10);
	} else {
		if (Z_REFCOUNT_P(r10) > 1) {
			{
				zval *orig_ptr = r10;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(r10);
					*r10 = *orig_ptr;
					zval_copy_ctor(r10);
					Z_SET_REFCOUNT_P(r10, 1);
					Z_UNSET_ISREF_P(r10);
				}
			}
		} else {
			FREE_ZVAL(r10);
			PHALCON_ALLOC_ZVAL(r10);
		}
	}
	phalcon_concat_right(r10, v13, "Action" TSRMLS_CC);
	if (v14) {
		Z_DELREF_P(v14);
		if (!Z_REFCOUNT_P(v14)) {
			FREE_ZVAL(v14);
		}
	}
	Z_ADDREF_P(r10);
	v14 = r10;
	if (phalcon_method_exists(v6, v14 TSRMLS_CC) == SUCCESS) {
		if (!r11) {
			PHALCON_ALLOC_ZVAL(r11);
		} else {
			if (Z_REFCOUNT_P(r11) > 1) {
				{
					zval *orig_ptr = r11;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r11);
						*r11 = *orig_ptr;
						zval_copy_ctor(r11);
						Z_SET_REFCOUNT_P(r11, 1);
						Z_UNSET_ISREF_P(r11);
					}
				}
			} else {
				FREE_ZVAL(r11);
				PHALCON_ALLOC_ZVAL(r11);
			}
		}
		if (!a0) {
			PHALCON_ALLOC_ZVAL(a0);
		} else {
			if (Z_REFCOUNT_P(a0) > 1) {
				SEPARATE_ZVAL(&a0);
			} else {
				if (Z_TYPE_P(a0) != IS_ARRAY) {
					FREE_ZVAL(a0);
					PHALCON_ALLOC_ZVAL(a0);
				}
			}
		}
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v6, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v14, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		Z_ADDREF_P(a0);
		p6[0] = a0;
		Z_ADDREF_P(v12);
		p6[1] = v12;
		PHALCON_CALL_FUNC_PARAMS(r11, "call_user_func_array", 2, p6);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r11);
		v5 = r11;
	} else {
		if (!t11) {
			PHALCON_ALLOC_ZVAL(t11);
		} else {
			if (Z_REFCOUNT_P(t11) > 1) {
				SEPARATE_ZVAL(&t11);
			} else {
				if (Z_TYPE_P(t11) != IS_STRING) {
					FREE_ZVAL(t11);
					PHALCON_ALLOC_ZVAL(t11);
				}
			}
		}
		ZVAL_STRING(t11, "notFoundAction", 0);
		if (phalcon_method_exists(v6, t11 TSRMLS_CC) == SUCCESS) {
			if (!r12) {
				PHALCON_ALLOC_ZVAL(r12);
			} else {
				if (Z_REFCOUNT_P(r12) > 1) {
					{
						zval *orig_ptr = r12;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r12);
							*r12 = *orig_ptr;
							zval_copy_ctor(r12);
							Z_SET_REFCOUNT_P(r12, 1);
							Z_UNSET_ISREF_P(r12);
						}
					}
				} else {
					FREE_ZVAL(r12);
					PHALCON_ALLOC_ZVAL(r12);
				}
			}
			if (!a1) {
				PHALCON_ALLOC_ZVAL(a1);
			} else {
				if (Z_REFCOUNT_P(a1) > 1) {
					SEPARATE_ZVAL(&a1);
				} else {
					if (Z_TYPE_P(a1) != IS_ARRAY) {
						FREE_ZVAL(a1);
						PHALCON_ALLOC_ZVAL(a1);
					}
				}
			}
			array_init(a1);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v6, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				{
					zval *orig_ptr = a1;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(a1);
						*a1 = *orig_ptr;
						zval_copy_ctor(a1);
						Z_SET_REFCOUNT_P(a1, 1);
						Z_UNSET_ISREF_P(a1);
					}
				}
				add_next_index_zval(a1, copy);
			}
			add_next_index_string(a1, "notFoundAction", 1);
			Z_ADDREF_P(a1);
			p7[0] = a1;
			Z_ADDREF_P(v12);
			p7[1] = v12;
			PHALCON_CALL_FUNC_PARAMS(r12, "call_user_func_array", 2, p7);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r12);
			v5 = r12;
		} else {
			if (!i3) {
				PHALCON_ALLOC_ZVAL(i3);
			} else {
				if (Z_REFCOUNT_P(i3) > 1) {
					{
						zval *orig_ptr = i3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i3);
							*i3 = *orig_ptr;
							zval_copy_ctor(i3);
							Z_SET_REFCOUNT_P(i3, 1);
							Z_UNSET_ISREF_P(i3);
						}
					}
				} else {
					FREE_ZVAL(i3);
					PHALCON_ALLOC_ZVAL(i3);
				}
			}
			object_init_ex(i3, phalcon_exception_class_entry);
			if (!r14) {
				PHALCON_ALLOC_ZVAL(r14);
			} else {
				if (Z_REFCOUNT_P(r14) > 1) {
					{
						zval *orig_ptr = r14;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r14);
							*r14 = *orig_ptr;
							zval_copy_ctor(r14);
							Z_SET_REFCOUNT_P(r14, 1);
							Z_UNSET_ISREF_P(r14);
						}
					}
				} else {
					FREE_ZVAL(r14);
					PHALCON_ALLOC_ZVAL(r14);
				}
			}
			phalcon_concat_left(r14, "Action '", v13 TSRMLS_CC);
			if (!r13) {
				PHALCON_ALLOC_ZVAL(r13);
			} else {
				if (Z_REFCOUNT_P(r13) > 1) {
					{
						zval *orig_ptr = r13;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r13);
							*r13 = *orig_ptr;
							zval_copy_ctor(r13);
							Z_SET_REFCOUNT_P(r13, 1);
							Z_UNSET_ISREF_P(r13);
						}
					}
				} else {
					FREE_ZVAL(r13);
					PHALCON_ALLOC_ZVAL(r13);
				}
			}
			phalcon_concat_vboth(r13, r14, "' was not found on controller '", v8 TSRMLS_CC);
			if (!r15) {
				PHALCON_ALLOC_ZVAL(r15);
			} else {
				if (Z_REFCOUNT_P(r15) > 1) {
					{
						zval *orig_ptr = r15;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r15);
							*r15 = *orig_ptr;
							zval_copy_ctor(r15);
							Z_SET_REFCOUNT_P(r15, 1);
							Z_UNSET_ISREF_P(r15);
						}
					}
				} else {
					FREE_ZVAL(r15);
					PHALCON_ALLOC_ZVAL(r15);
				}
			}
			phalcon_concat_right(r15, r13, "'" TSRMLS_CC);
			Z_ADDREF_P(r15);
			p8[0] = r15;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i3, "__construct", 1, p8, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i3 TSRMLS_CC);
			Z_ADDREF_P(i3);
			return;
		}
	}
	{
		zval *orig_ptr = v7;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			Z_DELREF_P(orig_ptr);
			ALLOC_ZVAL(v7);
			*v7 = *orig_ptr;
			zval_copy_ctor(v7);
			Z_SET_REFCOUNT_P(v7, 1);
			Z_UNSET_ISREF_P(v7);
		}
	}
	increment_function(v7);
	if (!t12) {
		PHALCON_ALLOC_ZVAL(t12);
	} else {
		if (Z_REFCOUNT_P(t12) > 1) {
			SEPARATE_ZVAL(&t12);
		} else {
			if (Z_TYPE_P(t12) != IS_LONG) {
				FREE_ZVAL(t12);
				PHALCON_ALLOC_ZVAL(t12);
			}
		}
	}
	ZVAL_LONG(t12, 256);
	PHALCON_SMALLER_FUNCTION(r16, t12, v7);
	if (zend_is_true(r16)) {
		if (!i4) {
			PHALCON_ALLOC_ZVAL(i4);
		} else {
			if (Z_REFCOUNT_P(i4) > 1) {
				{
					zval *orig_ptr = i4;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(i4);
						*i4 = *orig_ptr;
						zval_copy_ctor(i4);
						Z_SET_REFCOUNT_P(i4, 1);
						Z_UNSET_ISREF_P(i4);
					}
				}
			} else {
				FREE_ZVAL(i4);
				PHALCON_ALLOC_ZVAL(i4);
			}
		}
		object_init_ex(i4, phalcon_exception_class_entry);
		if (!p9[0]) {
			PHALCON_ALLOC_ZVAL(p9[0]);
		} else {
			if (Z_REFCOUNT_P(p9[0]) > 1) {
				SEPARATE_ZVAL(&p9[0]);
			} else {
				if (Z_TYPE_P(p9[0]) != IS_STRING) {
					FREE_ZVAL(p9[0]);
					PHALCON_ALLOC_ZVAL(p9[0]);
				}
			}
		}
		ZVAL_STRING(p9[0], "Dispatcher has detected a cyclic routing causing stability problems", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i4, "__construct", 1, p9, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i4 TSRMLS_CC);
		Z_ADDREF_P(i4);
		return;
	}
	goto ws0;
	we0:
	r1 = NULL;
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v5, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_returnedValue", strlen("_returnedValue"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v6, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_lastController", strlen("_lastController"), copy TSRMLS_CC);
	}
	if (Z_TYPE_P(v6) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v6);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v6);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Routes to a controller/action using a string or array uri
 *
 * @param string $uri
 */
PHP_METHOD(Phalcon_Dispatcher, forward){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p4[] = { NULL }, *p5[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(v0);
		v1 = v0;
	} else {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "/", 1);
		p0[1] = v0;
		PHALCON_CALL_FUNC_PARAMS(r0, "explode", 2, p0);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r0);
		v1 = r0;
	}
	eval_int = phalcon_array_isset_long(v1, 0);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		p1[0] = r1;
		phalcon_filter_alphanum(&r2, p1[0]);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_controllerName", strlen("_controllerName"), copy TSRMLS_CC);
		}
		{
			zval *orig_ptr = v1;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(v1);
				*v1 = *orig_ptr;
				zval_copy_ctor(v1);
				Z_SET_REFCOUNT_P(v1, 1);
				Z_UNSET_ISREF_P(v1);
			}
		}
		phalcon_array_unset_long(v1, 0);
	} else {
		eval_int = phalcon_array_isset_string(v1, "controller", strlen("controller")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch_string(r3, v1, "controller", strlen("controller"), PHALCON_NOISY_FETCH TSRMLS_CC);
			p2[0] = r3;
			phalcon_filter_alphanum(&r4, p2[0]);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r4, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_controllerName", strlen("_controllerName"), copy TSRMLS_CC);
			}
		} else {
			PHALCON_ALLOC_ZVAL(r5);
			PHALCON_CALL_METHOD(r5, this_ptr, "getcontrollername", PHALCON_CALL_DEFAULT);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r5, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_controllerName", strlen("_controllerName"), copy TSRMLS_CC);
			}
		}
	}
	eval_int = phalcon_array_isset_long(v1, 1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_array_fetch_long(r6, v1, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		p4[0] = r6;
		phalcon_filter_alphanum(&r7, p4[0]);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r7, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(r8);
		phalcon_array_fetch_long(r8, v1, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r8, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
		}
		{
			zval *orig_ptr = v1;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(v1);
				*v1 = *orig_ptr;
				zval_copy_ctor(v1);
				Z_SET_REFCOUNT_P(v1, 1);
				Z_UNSET_ISREF_P(v1);
			}
		}
		phalcon_array_unset_long(v1, 1);
	} else {
		eval_int = phalcon_array_isset_string(v1, "action", strlen("action")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r9);
			phalcon_array_fetch_string(r9, v1, "action", strlen("action"), PHALCON_NOISY_FETCH TSRMLS_CC);
			p5[0] = r9;
			phalcon_filter_alphanum(&r10, p5[0]);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r10, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
			}
		} else {
			PHALCON_ALLOC_ZVAL(r11);
			PHALCON_CALL_METHOD(r11, this_ptr, "getactionname", PHALCON_CALL_DEFAULT);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r11, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_actionName", strlen("_actionName"), copy TSRMLS_CC);
			}
		}
	}
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(a0);
	v2 = a0;
	FOREACH_KV(v1, ac0, fes1, fee1, ah0, hp0, v4, v3)
		if (Z_TYPE_P(v4) == IS_LONG) {
			if (!r12) {
				PHALCON_ALLOC_ZVAL(r12);
			} else {
				if (Z_REFCOUNT_P(r12) > 1) {
					{
						zval *orig_ptr = r12;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r12);
							*r12 = *orig_ptr;
							zval_copy_ctor(r12);
							Z_SET_REFCOUNT_P(r12, 1);
							Z_UNSET_ISREF_P(r12);
						}
					}
				} else {
					FREE_ZVAL(r12);
					PHALCON_ALLOC_ZVAL(r12);
				}
			}
			phalcon_array_fetch(r12, v1, v4, PHALCON_NOISY_FETCH TSRMLS_CC);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r12, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = v2;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v2);
						*v2 = *orig_ptr;
						zval_copy_ctor(v2);
						Z_SET_REFCOUNT_P(v2, 1);
						Z_UNSET_ISREF_P(v2);
					}
				}
				phalcon_array_append(v2, copy TSRMLS_CC);
			}
		}
	END_FOREACH(ac0, fes1, fee1, ah0, hp0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v2, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_params", strlen("_params"), copy TSRMLS_CC);
	}
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_finished", strlen("_finished"), 0 TSRMLS_CC);
	RETURN_NULL();
}

/**
 * Returns all instantiated controllers whitin the dispatcher
 *
 * @return array
 */
PHP_METHOD(Phalcon_Dispatcher, getControllers){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_controllers", sizeof("_controllers")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns last dispatched controller
 *
 * @return Php_Controller
 */
PHP_METHOD(Phalcon_Dispatcher, getLastController){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_lastController", sizeof("_lastController")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns value returned by last dispacthed action
 *
 * @return mixed
 */
PHP_METHOD(Phalcon_Dispatcher, getReturnedValue){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_returnedValue", sizeof("_returnedValue")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Response, __construct){


	RETURN_NULL();
}

PHP_METHOD(Phalcon_Response, getInstance){

	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;

	t0 = zend_read_static_property(phalcon_response_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_response_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		zend_update_static_property(phalcon_response_class_entry, "_instance", sizeof("_instance")-1, i0 TSRMLS_CC);
	}
	t1 = zend_read_static_property(phalcon_response_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Request
 *
 * <p>Encapsulates request information for easily and secure access from application controllers.</p>
 *
 * <p>The request object is a simple value object that is passed between the dispatcher and controller classes.
 * It packages the HTTP request environment.</p>
 *
 * 
 *
 */

PHP_METHOD(Phalcon_Request, __construct){


	RETURN_NULL();
}

/**
 * Gets the singleton instance of Php_Request
 *
 * @return Php_Request
 */
PHP_METHOD(Phalcon_Request, getInstance){

	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;

	t0 = zend_read_static_property(phalcon_request_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_request_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		zend_update_static_property(phalcon_request_class_entry, "_instance", sizeof("_instance")-1, i0 TSRMLS_CC);
	}
	t1 = zend_read_static_property(phalcon_request_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Overwrittes Php_Filter object used to sanitize input data
 *
 * @param Php_Filter $filter
 */
PHP_METHOD(Phalcon_Request, setFilter){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_filter", strlen("_filter"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns active filter object used to sanitize input data
 *
 * @return Php_Filter
 */
PHP_METHOD(Phalcon_Request, getFilter){

	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_filter", sizeof("_filter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_filter_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, i0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_filter", strlen("_filter"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_filter", sizeof("_filter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Gets variable from $_POST superglobal applying filters if needed
 *
 * @param string $name
 * @param string|array $filters
 * @return mixed
 */
PHP_METHOD(Phalcon_Request, getPost){

	zval *v0 = NULL, *v1 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval **gv0;
	zval *p0[] = { NULL, NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	phalcon_init_global("_POST" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_POST", sizeof("_POST"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset(a0, v0);
	if (eval_int) {
		if (zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(r0);
			PHALCON_ALLOC_ZVAL(r1);
			PHALCON_CALL_METHOD(r1, this_ptr, "getfilter", PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_array_fetch(r2, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r2);
			p0[0] = r2;
			Z_ADDREF_P(v1);
			p0[1] = v1;
			PHALCON_CALL_METHOD_PARAMS(r0, r1, "sanitize", 2, p0, PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r0, 1, 0);
		} else {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch(r3, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r3) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		}
	} else {
		RETURN_NULL();
	}
	RETURN_NULL();
}

/**
 * Gets variable from $_GET superglobal applying filters if needed
 *
 * @param string $name
 * @param string|array $filters
 * @return mixed
 */
PHP_METHOD(Phalcon_Request, getQuery){

	zval *v0 = NULL, *v1 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval **gv0;
	zval *p0[] = { NULL, NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	phalcon_init_global("_GET" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_GET", sizeof("_GET"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset(a0, v0);
	if (eval_int) {
		if (zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(r0);
			PHALCON_ALLOC_ZVAL(r1);
			PHALCON_CALL_METHOD(r1, this_ptr, "getfilter", PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_array_fetch(r2, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r2);
			p0[0] = r2;
			Z_ADDREF_P(v1);
			p0[1] = v1;
			PHALCON_CALL_METHOD_PARAMS(r0, r1, "sanizite", 2, p0, PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r0, 1, 0);
		} else {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch(r3, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r3) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		}
	} else {
		RETURN_NULL();
	}
	RETURN_NULL();
}

/**
 * Gets variable from $_SERVER superglobal
 *
 * @param string $name
 * @return mixed
 */
PHP_METHOD(Phalcon_Request, getServer){

	zval *v0 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL;
	zval **gv0;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset(a0, v0);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch(r0, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_NULL();
	}
	RETURN_NULL();
}

/**
 * Gets HTTP header from request data
 *
 * @param string $header
 * @return string
 */
PHP_METHOD(Phalcon_Request, getHeader){

	zval *v0 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval **gv0;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset(a0, v0);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch(r0, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_concat_left(r1, "HTTP_", v0 TSRMLS_CC);
		eval_int = phalcon_array_isset(a0, r1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_concat_left(r2, "HTTP_", v0 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch(r3, a0, r2, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r3) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r3);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		} else {
			RETURN_STRING("", 1);
		}
	}
	RETURN_NULL();
}

/**
 * Gets HTTP schema (http/https)
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getScheme){

	zval *r0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "HTTP_HTTPS", 1);
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "getserver", 1, p0, PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "on")) {
		RETURN_STRING("https", 1);
	} else {
		RETURN_STRING("http", 1);
	}
	RETURN_NULL();
}

/**
 * Checks whether request has been made using ajax
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isAjax){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "HTTP_X_REQUESTED_WITH", 1);
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "getheader", 1, p0, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, "XMLHttpRequest", 1);
	PHALCON_EQUAL_FUNCTION(r1, r0, t0);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r1);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Checks whether request has been made using SOAP
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isSoapRequested){

	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *t0 = NULL;
	zval **gv0;
	zval *p0[] = { NULL, NULL };
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "HTTP_SOAPACTION", strlen("HTTP_SOAPACTION")+1);
	if (eval_int) {
		RETURN_TRUE;
	} else {
		eval_int = phalcon_array_isset_string(a0, "CONTENT_TYPE", strlen("CONTENT_TYPE")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r0);
			PHALCON_ALLOC_ZVAL(r1);
			phalcon_array_fetch_string(r1, a0, "CONTENT_TYPE", strlen("CONTENT_TYPE"), PHALCON_NOISY_FETCH TSRMLS_CC);
			p0[0] = r1;
			PHALCON_ALLOC_ZVAL(p0[1]);
			ZVAL_STRING(p0[1], "application/soap+xml", 1);
			PHALCON_CALL_FUNC_PARAMS(r0, "strpos", 2, p0);
			PHALCON_INIT_FALSE(t0);
			PHALCON_NOT_IDENTICAL_FUNCTION(r2, r0, t0);
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
				Z_SET_REFCOUNT_P(return_value, refcount);
			}
			return;
		} else {
			RETURN_FALSE;
		}
	}
	RETURN_NULL();
}

/**
 * Checks whether request has been made using any secure layer
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isSecureRequest){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getscheme", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, "https", 1);
	PHALCON_IDENTICAL_FUNCTION(r1, r0, t0);
	if (zend_is_true(r1)) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Gets HTTP raws request body
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getRawBody){

	zval *r0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "php://input", 1);
	PHALCON_CALL_FUNC_PARAMS(r0, "file_get_contents", 1, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Gets active server address IP
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getServerAddress){

	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval **gv0;
	zval *p0[] = { NULL };
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "SERVER_ADDR", strlen("SERVER_ADDR")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "SERVER_ADDR", strlen("SERVER_ADDR"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "localhost", 1);
		PHALCON_CALL_FUNC_PARAMS(r1, "gethostbyname", 1, p0);
		RETURN_ZVAL(r1, 1, 0);
	}
	RETURN_NULL();
}

/**
 * Gets active server name
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getServerName){

	zval *a0 = NULL;
	zval *r0 = NULL;
	zval **gv0;
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "SERVER_NAME", strlen("SERVER_NAME")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "SERVER_NAME", strlen("SERVER_NAME"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("localhost", 1);
	}
	RETURN_NULL();
}

/**
 * Gets information about schema, host and port used by the request
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getHttpHost){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *p1[] = { NULL }, *p2[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getscheme", PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(p1[0]);
	ZVAL_STRING(p1[0], "HTTP_SERVER_NAME", 1);
	PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "getserver", 1, p1, PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r1);
	v1 = r1;
	PHALCON_ALLOC_ZVAL(r2);
	PHALCON_ALLOC_ZVAL(p2[0]);
	ZVAL_STRING(p2[0], "HTTP_SERVER_PORT", 1);
	PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "getserver", 1, p2, PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r2);
	v2 = r2;
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, "http", 1);
	PHALCON_EQUAL_FUNCTION(r3, v0, t0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 80);
	PHALCON_EQUAL_FUNCTION(r4, v2, t1);
	PHALCON_AND_FUNCTION(r5, r3, r4);
	PHALCON_ALLOC_ZVAL(t2);
	ZVAL_STRING(t2, "https", 1);
	PHALCON_EQUAL_FUNCTION(r6, v0, t2);
	PHALCON_ALLOC_ZVAL(t3);
	ZVAL_LONG(t3, 443);
	PHALCON_EQUAL_FUNCTION(r7, v2, t3);
	PHALCON_AND_FUNCTION(r8, r6, r7);
	PHALCON_OR_FUNCTION(r9, r5, r8);
	if (zend_is_true(r9)) {
		if (Z_TYPE_P(v1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(r10);
		phalcon_concat_vboth(r10, v1, ":", v2 TSRMLS_CC);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r10);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	}
	RETURN_NULL();
}

/**
 * Gets most possibly client IPv4 Address
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getClientAddress){

	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval **gv0;
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "HTTP_X_FORWARDED_FOR", strlen("HTTP_X_FORWARDED_FOR")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "HTTP_X_FORWARDED_FOR", strlen("HTTP_X_FORWARDED_FOR"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		eval_int = phalcon_array_isset_string(a0, "REMOTE_ADDR", strlen("REMOTE_ADDR")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r1);
			phalcon_array_fetch_string(r1, a0, "REMOTE_ADDR", strlen("REMOTE_ADDR"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r1) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r1);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(r1);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		} else {
			RETURN_STRING("", 1);
		}
	}
	RETURN_NULL();
}

/**
 * Gets HTTP method which request has been made
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getMethod){

	zval *a0 = NULL;
	zval *r0 = NULL;
	zval **gv0;
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "REQUEST_METHOD", strlen("REQUEST_METHOD")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "REQUEST_METHOD", strlen("REQUEST_METHOD"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Gets HTTP user agent used to made the request
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getUserAgent){

	zval *a0 = NULL;
	zval *r0 = NULL;
	zval **gv0;
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "HTTP_USER_AGENT", strlen("HTTP_USER_AGENT")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "HTTP_USER_AGENT", strlen("HTTP_USER_AGENT"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is POST
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isPost){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "POST")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is GET
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isGet){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "GET")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is PUT
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isPut){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "PUT")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is HEAD
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isHead){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "HEAD")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is DELETE
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isDelete){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "DELETE")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether HTTP method is OPTIONS
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, isOptions){

	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getmethod", PHALCON_CALL_DEFAULT);
	if (phalcon_compare_strict_string(r0, "OPTIONS")) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Checks whether request include attached files
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Request, hasFiles){

	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval **gv0;
	zval *p0[] = { NULL };

	phalcon_init_global("_FILES" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_FILES", sizeof("_FILES"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	if (zend_is_true(a0)) {
		PHALCON_ALLOC_ZVAL(r1);
		p0[0] = a0;
		PHALCON_CALL_FUNC_PARAMS(r1, "count", 1, p0);
		PHALCON_ALLOC_ZVAL(t0);
		ZVAL_LONG(t0, 0);
		PHALCON_SMALLER_FUNCTION(r2, t0, r1);
		if (zend_is_true(r2)) {
			PHALCON_INIT_TRUE(t1);
			r0 = t1;
		} else {
			PHALCON_INIT_FALSE(t2);
			r0 = t2;
		}
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Gets attached files as Php_UploadFile clases
 *
 * @return array
 */
PHP_METHOD(Phalcon_Request, getUploadedFiles){

	zval *a0 = NULL, *a1 = NULL, *a2 = NULL;
	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *ac0 = NULL;
	zval *i0 = NULL;
	zval **gv0;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	zend_class_entry *ce0;

	phalcon_init_global("_FILES" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_FILES", sizeof("_FILES"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	if (zend_is_true(a0)) {
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a1);
		v0 = a1;
		FOREACH_V(a0, ac0, fes2, fee2, ah0, hp0, v1)
			ce0 = zend_fetch_class("phalcon_uploadfile", strlen("phalcon_uploadfile"), ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
			if (!i0) {
				PHALCON_ALLOC_ZVAL(i0);
			} else {
				if (Z_REFCOUNT_P(i0) > 1) {
					{
						zval *orig_ptr = i0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i0);
							*i0 = *orig_ptr;
							zval_copy_ctor(i0);
							Z_SET_REFCOUNT_P(i0, 1);
							Z_UNSET_ISREF_P(i0);
						}
					}
				} else {
					FREE_ZVAL(i0);
					PHALCON_ALLOC_ZVAL(i0);
				}
			}
			object_init_ex(i0, ce0);
			Z_ADDREF_P(v1);
			p0[0] = v1;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p0, PHALCON_CALL_CHECK);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(i0);
			v2 = i0;
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = v0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						ALLOC_ZVAL(v0);
						*v0 = *orig_ptr;
						zval_copy_ctor(v0);
						Z_SET_REFCOUNT_P(v0, 1);
						Z_UNSET_ISREF_P(v0);
					}
				}
				phalcon_array_append(v0, copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes2, fee2, ah0, hp0);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(a2);
		array_init(a2);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(a2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	}
	RETURN_NULL();
}

/**
 * Gets web page which refers active request
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getHTTPReferer){

	zval *a0 = NULL;
	zval *r0 = NULL;
	zval **gv0;
	int eval_int;

	phalcon_init_global("_SERVER" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "HTTP_REFERER", strlen("HTTP_REFERER")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, a0, "HTTP_REFERER", strlen("HTTP_REFERER"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Gets array with mime/types and their quality accepted by the browser/client.
 *
 * @return array
 */
PHP_METHOD(Phalcon_Request, getAcceptableContent){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	int eval_int;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "HTTP_ACCEPT", 1);
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "getserver", 1, p0, PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(p1[0]);
	ZVAL_STRING(p1[0], "/,\\s*/", 1);
	p1[1] = v0;
	PHALCON_CALL_FUNC_PARAMS(r1, "preg_split", 2, p1);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r1);
	v1 = r1;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(a0);
	v2 = a0;
	FOREACH_V(v1, ac0, fes3, fee3, ah0, hp0, v3)
		if (!r2) {
			PHALCON_ALLOC_ZVAL(r2);
		} else {
			if (Z_REFCOUNT_P(r2) > 1) {
				{
					zval *orig_ptr = r2;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r2);
						*r2 = *orig_ptr;
						zval_copy_ctor(r2);
						Z_SET_REFCOUNT_P(r2, 1);
						Z_UNSET_ISREF_P(r2);
					}
				}
			} else {
				FREE_ZVAL(r2);
				PHALCON_ALLOC_ZVAL(r2);
			}
		}
		if (!p2[0]) {
			PHALCON_ALLOC_ZVAL(p2[0]);
		} else {
			if (Z_REFCOUNT_P(p2[0]) > 1) {
				SEPARATE_ZVAL(&p2[0]);
			} else {
				if (Z_TYPE_P(p2[0]) != IS_STRING) {
					FREE_ZVAL(p2[0]);
					PHALCON_ALLOC_ZVAL(p2[0]);
				}
			}
		}
		ZVAL_STRING(p2[0], ";", 1);
		p2[1] = v3;
		PHALCON_CALL_FUNC_PARAMS(r2, "explode", 2, p2);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r2);
		v4 = r2;
		eval_int = phalcon_array_isset_long(v4, 1);
		if (eval_int) {
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_array_fetch_long(r4, v4, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r4);
			p3[0] = r4;
			PHALCON_PARAM_LONG(p3[1], 2);
			PHALCON_CALL_FUNC_PARAMS(r3, "substr", 2, p3);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r3);
			v5 = r3;
		} else {
			if (!v5) {
				PHALCON_ALLOC_ZVAL(v5);
			} else {
				if (Z_REFCOUNT_P(v5) > 1) {
					SEPARATE_ZVAL(&v5);
				} else {
					if (Z_TYPE_P(v5) != IS_DOUBLE) {
						FREE_ZVAL(v5);
						PHALCON_ALLOC_ZVAL(v5);
					}
				}
			}
			ZVAL_DOUBLE(v5, 1);
		}
		if (!a1) {
			PHALCON_ALLOC_ZVAL(a1);
		} else {
			if (Z_REFCOUNT_P(a1) > 1) {
				SEPARATE_ZVAL(&a1);
			} else {
				if (Z_TYPE_P(a1) != IS_ARRAY) {
					FREE_ZVAL(a1);
					PHALCON_ALLOC_ZVAL(a1);
				}
			}
		}
		array_init(a1);
		if (!r5) {
			PHALCON_ALLOC_ZVAL(r5);
		} else {
			if (Z_REFCOUNT_P(r5) > 1) {
				{
					zval *orig_ptr = r5;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r5);
						*r5 = *orig_ptr;
						zval_copy_ctor(r5);
						Z_SET_REFCOUNT_P(r5, 1);
						Z_UNSET_ISREF_P(r5);
					}
				}
			} else {
				FREE_ZVAL(r5);
				PHALCON_ALLOC_ZVAL(r5);
			}
		}
		phalcon_array_fetch_long(r5, v4, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r5, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a1);
					*a1 = *orig_ptr;
					zval_copy_ctor(a1);
					Z_SET_REFCOUNT_P(a1, 1);
					Z_UNSET_ISREF_P(a1);
				}
			}
			add_assoc_zval(a1, "accept", copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v5, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a1);
					*a1 = *orig_ptr;
					zval_copy_ctor(a1);
					Z_SET_REFCOUNT_P(a1, 1);
					Z_UNSET_ISREF_P(a1);
				}
			}
			add_assoc_zval(a1, "quality", copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, a1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 1);
			{
				zval *orig_ptr = v2;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v2);
					*v2 = *orig_ptr;
					zval_copy_ctor(v2);
					Z_SET_REFCOUNT_P(v2, 1);
					Z_UNSET_ISREF_P(v2);
				}
			}
			phalcon_array_append(v2, copy TSRMLS_CC);
		}
	END_FOREACH(ac0, fes3, fee3, ah0, hp0);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Gets best mime/type accepted by the browser/client.
 *
 * @return array
 */
PHP_METHOD(Phalcon_Request, getBestQualityAccept){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	PHALCON_ALLOC_ZVAL(v0);
	ZVAL_LONG(v0, 0);
	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_LONG(v1, 0);
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "", 0);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getacceptablecontent", PHALCON_CALL_DEFAULT);
	FOREACH_V(r0, ac0, fes4, fee4, ah0, hp0, v3)
		if (!t0) {
			PHALCON_ALLOC_ZVAL(t0);
		} else {
			if (Z_REFCOUNT_P(t0) > 1) {
				SEPARATE_ZVAL(&t0);
			} else {
				if (Z_TYPE_P(t0) != IS_LONG) {
					FREE_ZVAL(t0);
					PHALCON_ALLOC_ZVAL(t0);
				}
			}
		}
		ZVAL_LONG(t0, 0);
		PHALCON_EQUAL_FUNCTION(r1, v0, t0);
		if (zend_is_true(r1)) {
			if (!r2) {
				PHALCON_ALLOC_ZVAL(r2);
			} else {
				if (Z_REFCOUNT_P(r2) > 1) {
					{
						zval *orig_ptr = r2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r2);
							*r2 = *orig_ptr;
							zval_copy_ctor(r2);
							Z_SET_REFCOUNT_P(r2, 1);
							Z_UNSET_ISREF_P(r2);
						}
					}
				} else {
					FREE_ZVAL(r2);
					PHALCON_ALLOC_ZVAL(r2);
				}
			}
			phalcon_array_fetch_string(r2, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r2);
			v1 = r2;
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			phalcon_array_fetch_string(r3, v3, "accept", strlen("accept"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r3);
			v2 = r3;
		} else {
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_array_fetch_string(r4, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_SMALLER_FUNCTION(r5, v1, r4);
			if (zend_is_true(r5)) {
				if (!r6) {
					PHALCON_ALLOC_ZVAL(r6);
				} else {
					if (Z_REFCOUNT_P(r6) > 1) {
						{
							zval *orig_ptr = r6;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r6);
								*r6 = *orig_ptr;
								zval_copy_ctor(r6);
								Z_SET_REFCOUNT_P(r6, 1);
								Z_UNSET_ISREF_P(r6);
							}
						}
					} else {
						FREE_ZVAL(r6);
						PHALCON_ALLOC_ZVAL(r6);
					}
				}
				phalcon_array_fetch_string(r6, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v1) {
					Z_DELREF_P(v1);
					if (!Z_REFCOUNT_P(v1)) {
						FREE_ZVAL(v1);
					}
				}
				Z_ADDREF_P(r6);
				v1 = r6;
				if (!r7) {
					PHALCON_ALLOC_ZVAL(r7);
				} else {
					if (Z_REFCOUNT_P(r7) > 1) {
						{
							zval *orig_ptr = r7;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r7);
								*r7 = *orig_ptr;
								zval_copy_ctor(r7);
								Z_SET_REFCOUNT_P(r7, 1);
								Z_UNSET_ISREF_P(r7);
							}
						}
					} else {
						FREE_ZVAL(r7);
						PHALCON_ALLOC_ZVAL(r7);
					}
				}
				phalcon_array_fetch_string(r7, v3, "accept", strlen("accept"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v2) {
					Z_DELREF_P(v2);
					if (!Z_REFCOUNT_P(v2)) {
						FREE_ZVAL(v2);
					}
				}
				Z_ADDREF_P(r7);
				v2 = r7;
			}
		}
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		increment_function(v0);
	END_FOREACH(ac0, fes4, fee4, ah0, hp0);
	if (Z_TYPE_P(v2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Gets charsets array and their quality accepted by the browser/client.
 *
 * @return array
 */
PHP_METHOD(Phalcon_Request, getClientCharsets){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	int eval_int;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "HTTP_ACCEPT_CHARSET", 1);
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "getserver", 1, p0, PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(p1[0]);
	ZVAL_STRING(p1[0], "/,\\s*/", 1);
	p1[1] = v0;
	PHALCON_CALL_FUNC_PARAMS(r1, "preg_split", 2, p1);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r1);
	v1 = r1;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(a0);
	v2 = a0;
	FOREACH_V(v1, ac0, fes5, fee5, ah0, hp0, v3)
		if (!r2) {
			PHALCON_ALLOC_ZVAL(r2);
		} else {
			if (Z_REFCOUNT_P(r2) > 1) {
				{
					zval *orig_ptr = r2;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r2);
						*r2 = *orig_ptr;
						zval_copy_ctor(r2);
						Z_SET_REFCOUNT_P(r2, 1);
						Z_UNSET_ISREF_P(r2);
					}
				}
			} else {
				FREE_ZVAL(r2);
				PHALCON_ALLOC_ZVAL(r2);
			}
		}
		if (!p2[0]) {
			PHALCON_ALLOC_ZVAL(p2[0]);
		} else {
			if (Z_REFCOUNT_P(p2[0]) > 1) {
				SEPARATE_ZVAL(&p2[0]);
			} else {
				if (Z_TYPE_P(p2[0]) != IS_STRING) {
					FREE_ZVAL(p2[0]);
					PHALCON_ALLOC_ZVAL(p2[0]);
				}
			}
		}
		ZVAL_STRING(p2[0], ";", 1);
		p2[1] = v3;
		PHALCON_CALL_FUNC_PARAMS(r2, "explode", 2, p2);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r2);
		v4 = r2;
		eval_int = phalcon_array_isset_long(v4, 1);
		if (eval_int) {
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_array_fetch_long(r4, v4, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r4);
			p3[0] = r4;
			PHALCON_PARAM_LONG(p3[1], 2);
			PHALCON_CALL_FUNC_PARAMS(r3, "substr", 2, p3);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r3);
			v5 = r3;
		} else {
			if (!v5) {
				PHALCON_ALLOC_ZVAL(v5);
			} else {
				if (Z_REFCOUNT_P(v5) > 1) {
					SEPARATE_ZVAL(&v5);
				} else {
					if (Z_TYPE_P(v5) != IS_DOUBLE) {
						FREE_ZVAL(v5);
						PHALCON_ALLOC_ZVAL(v5);
					}
				}
			}
			ZVAL_DOUBLE(v5, 1);
		}
		if (!a1) {
			PHALCON_ALLOC_ZVAL(a1);
		} else {
			if (Z_REFCOUNT_P(a1) > 1) {
				SEPARATE_ZVAL(&a1);
			} else {
				if (Z_TYPE_P(a1) != IS_ARRAY) {
					FREE_ZVAL(a1);
					PHALCON_ALLOC_ZVAL(a1);
				}
			}
		}
		array_init(a1);
		if (!r5) {
			PHALCON_ALLOC_ZVAL(r5);
		} else {
			if (Z_REFCOUNT_P(r5) > 1) {
				{
					zval *orig_ptr = r5;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r5);
						*r5 = *orig_ptr;
						zval_copy_ctor(r5);
						Z_SET_REFCOUNT_P(r5, 1);
						Z_UNSET_ISREF_P(r5);
					}
				}
			} else {
				FREE_ZVAL(r5);
				PHALCON_ALLOC_ZVAL(r5);
			}
		}
		phalcon_array_fetch_long(r5, v4, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r5, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a1);
					*a1 = *orig_ptr;
					zval_copy_ctor(a1);
					Z_SET_REFCOUNT_P(a1, 1);
					Z_UNSET_ISREF_P(a1);
				}
			}
			add_assoc_zval(a1, "accept", copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v5, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a1);
					*a1 = *orig_ptr;
					zval_copy_ctor(a1);
					Z_SET_REFCOUNT_P(a1, 1);
					Z_UNSET_ISREF_P(a1);
				}
			}
			add_assoc_zval(a1, "quality", copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, a1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 1);
			{
				zval *orig_ptr = v2;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v2);
					*v2 = *orig_ptr;
					zval_copy_ctor(v2);
					Z_SET_REFCOUNT_P(v2, 1);
					Z_UNSET_ISREF_P(v2);
				}
			}
			phalcon_array_append(v2, copy TSRMLS_CC);
		}
	END_FOREACH(ac0, fes5, fee5, ah0, hp0);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Gets best charset accepted by the browser/client.
 *
 * @return string
 */
PHP_METHOD(Phalcon_Request, getBestQualityCharset){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	PHALCON_ALLOC_ZVAL(v0);
	ZVAL_LONG(v0, 0);
	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_LONG(v1, 0);
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "", 0);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getclientcharsets", PHALCON_CALL_DEFAULT);
	FOREACH_V(r0, ac0, fes6, fee6, ah0, hp0, v3)
		if (!t0) {
			PHALCON_ALLOC_ZVAL(t0);
		} else {
			if (Z_REFCOUNT_P(t0) > 1) {
				SEPARATE_ZVAL(&t0);
			} else {
				if (Z_TYPE_P(t0) != IS_LONG) {
					FREE_ZVAL(t0);
					PHALCON_ALLOC_ZVAL(t0);
				}
			}
		}
		ZVAL_LONG(t0, 0);
		PHALCON_EQUAL_FUNCTION(r1, v0, t0);
		if (zend_is_true(r1)) {
			if (!r2) {
				PHALCON_ALLOC_ZVAL(r2);
			} else {
				if (Z_REFCOUNT_P(r2) > 1) {
					{
						zval *orig_ptr = r2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r2);
							*r2 = *orig_ptr;
							zval_copy_ctor(r2);
							Z_SET_REFCOUNT_P(r2, 1);
							Z_UNSET_ISREF_P(r2);
						}
					}
				} else {
					FREE_ZVAL(r2);
					PHALCON_ALLOC_ZVAL(r2);
				}
			}
			phalcon_array_fetch_string(r2, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r2);
			v1 = r2;
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			phalcon_array_fetch_string(r3, v3, "accept", strlen("accept"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r3);
			v2 = r3;
		} else {
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_array_fetch_string(r4, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_SMALLER_FUNCTION(r5, v1, r4);
			if (zend_is_true(r5)) {
				if (!r6) {
					PHALCON_ALLOC_ZVAL(r6);
				} else {
					if (Z_REFCOUNT_P(r6) > 1) {
						{
							zval *orig_ptr = r6;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r6);
								*r6 = *orig_ptr;
								zval_copy_ctor(r6);
								Z_SET_REFCOUNT_P(r6, 1);
								Z_UNSET_ISREF_P(r6);
							}
						}
					} else {
						FREE_ZVAL(r6);
						PHALCON_ALLOC_ZVAL(r6);
					}
				}
				phalcon_array_fetch_string(r6, v3, "quality", strlen("quality"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v1) {
					Z_DELREF_P(v1);
					if (!Z_REFCOUNT_P(v1)) {
						FREE_ZVAL(v1);
					}
				}
				Z_ADDREF_P(r6);
				v1 = r6;
				if (!r7) {
					PHALCON_ALLOC_ZVAL(r7);
				} else {
					if (Z_REFCOUNT_P(r7) > 1) {
						{
							zval *orig_ptr = r7;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r7);
								*r7 = *orig_ptr;
								zval_copy_ctor(r7);
								Z_SET_REFCOUNT_P(r7, 1);
								Z_UNSET_ISREF_P(r7);
							}
						}
					} else {
						FREE_ZVAL(r7);
						PHALCON_ALLOC_ZVAL(r7);
					}
				}
				phalcon_array_fetch_string(r7, v3, "accept", strlen("accept"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v2) {
					Z_DELREF_P(v2);
					if (!Z_REFCOUNT_P(v2)) {
						FREE_ZVAL(v2);
					}
				}
				Z_ADDREF_P(r7);
				v2 = r7;
			}
		}
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		increment_function(v0);
	END_FOREACH(ac0, fes6, fee6, ah0, hp0);
	if (Z_TYPE_P(v2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Controller_Front
 *
 * Php_Controller_Front implements a "Front Controller" pattern used in "Model-View-Controller" (MVC) applications.
 * Its purpose is to initialize the request environment, route the incoming request, and then dispatch
 * any discovered actions; it aggregates any responses and returns them when the process is complete
 *
 *
 */

/**
 * Private Php_Controller_Front constructor for singleton
 */
PHP_METHOD(Phalcon_Controller_Front, __construct){


	RETURN_NULL();
}

/**
 * Gets Php_Controller_Front singleton instance
 *
 * @return Php_Controller_Front
 */
PHP_METHOD(Phalcon_Controller_Front, getInstance){

	zval *r0 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	t0 = zend_read_static_property(phalcon_controller_front_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	Z_ADDREF_P(t0);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_null", 1, p0);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_controller_front_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		zend_update_static_property(phalcon_controller_front_class_entry, "_instance", sizeof("_instance")-1, i0 TSRMLS_CC);
	}
	t1 = zend_read_static_property(phalcon_controller_front_class_entry, "_instance", sizeof("_instance")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Modifies multipe general settings using a Php_Config object or a stdClass filled with parameters
 *
 * 
 *
 * @param stdClass $config
 */
PHP_METHOD(Phalcon_Controller_Front, setConfig){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) == IS_OBJECT) {
		eval_int = phalcon_isset_property(v0, "database", strlen("database") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t0);
			phalcon_read_property(t0, v0, "database", sizeof("database")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t0);
			Z_ADDREF_P(t0);
			p0[0] = t0;
			PHALCON_CALL_STATIC_PARAMS_NORETURN("phalcon_db_pool", "setdefaultdescriptor", 1, p0);
		}
		eval_int = phalcon_isset_property(v0, "phalcon", strlen("phalcon") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t1);
			phalcon_read_property(t1, v0, "phalcon", sizeof("phalcon")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t1);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(t1);
			v1 = t1;
			eval_int = phalcon_isset_property(v1, "controllersDir", strlen("controllersDir") TSRMLS_CC);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(t2);
				phalcon_read_property(t2, v1, "controllersDir", sizeof("controllersDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t2);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, t2, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_controllersDir", strlen("_controllersDir"), copy TSRMLS_CC);
				}
			}
			eval_int = phalcon_isset_property(v1, "modelsDir", strlen("modelsDir") TSRMLS_CC);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(t3);
				phalcon_read_property(t3, v1, "modelsDir", sizeof("modelsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t3);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, t3, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_modelsDir", strlen("_modelsDir"), copy TSRMLS_CC);
				}
			}
			eval_int = phalcon_isset_property(v1, "viewsDir", strlen("viewsDir") TSRMLS_CC);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(t4);
				phalcon_read_property(t4, v1, "viewsDir", sizeof("viewsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t4);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, t4, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_viewsDir", strlen("_viewsDir"), copy TSRMLS_CC);
				}
			}
			eval_int = phalcon_isset_property(v1, "baseUri", strlen("baseUri") TSRMLS_CC);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(t5);
				phalcon_read_property(t5, v1, "baseUri", sizeof("baseUri")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t5);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, t5, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_baseUri", strlen("_baseUri"), copy TSRMLS_CC);
				}
			}
			eval_int = phalcon_isset_property(v1, "basePath", strlen("basePath") TSRMLS_CC);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(t6);
				phalcon_read_property(t6, v1, "basePath", sizeof("basePath")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t6);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, t6, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_basePath", strlen("_basePath"), copy TSRMLS_CC);
				}
			}
		}
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "Config parameter should be an Object", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
 * Sets the database default settings
 *
 * @param stdClass $database
 */
PHP_METHOD(Phalcon_Controller_Front, setDatabaseConfig){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_STATIC_PARAMS_NORETURN("phalcon_db_pool", "setdefaultdescriptor", 1, p0);
	RETURN_NULL();
}

/**
 * Sets controllers directory
 *
 * 
 *
 * @param string $controllersDir
 */
PHP_METHOD(Phalcon_Controller_Front, setControllersDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_controllersDir", strlen("_controllersDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sets models directory
     *
 * 
 *
 * @param string $modelsDir
 */
PHP_METHOD(Phalcon_Controller_Front, setModelsDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_modelsDir", strlen("_modelsDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sets views directory
 *
 * 
 *
 * @param string $viewsDir
 */
PHP_METHOD(Phalcon_Controller_Front, setViewsDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_viewsDir", strlen("_viewsDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Replaces the default router with a predefined object
 *
 * 
 *
 * @param Php_Router $router
 */
PHP_METHOD(Phalcon_Controller_Front, setRouter){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_router", strlen("_router"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Return active router
 *
 * @return Php_Router
 */
PHP_METHOD(Phalcon_Controller_Front, getRouter){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_router", sizeof("_router")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Replaces the default dispatcher with a predefined object
 *
 * @param Php_Dispatcher $dispatcher
 */
PHP_METHOD(Phalcon_Controller_Front, setDispatcher){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_dispatcher", strlen("_dispatcher"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Return active Dispatcher
 *
 * @return Php_Dispatcher
 */
PHP_METHOD(Phalcon_Controller_Front, getDispatcher){

	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dispatcher", sizeof("_dispatcher")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_dispatcher", sizeof("_dispatcher")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		if (Z_TYPE_P(t1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "Dispatch process has not started yet", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p0, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
 * Sets external uri which app is executed
 *
 * @param string $baseUri
 */
PHP_METHOD(Phalcon_Controller_Front, setBaseUri){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_baseUri", strlen("_baseUri"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets external uri where app is executed
 *
 * @return string
 */
PHP_METHOD(Phalcon_Controller_Front, getBaseUri){

	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *a0 = NULL;
	zval *v0 = NULL;
	zval **gv0;
	zval *p0[] = { NULL }, *p4[] = { NULL }, *p3[] = { NULL, NULL }, *p2[] = { NULL, NULL, NULL }, *p1[] = { NULL, NULL };
	int eval_int;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_baseUri", sizeof("_baseUri")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	Z_ADDREF_P(t0);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_null", 1, p0);
	if (zend_is_true(r0)) {
		phalcon_init_global("_SERVER" TSRMLS_CC);
		if (&EG(symbol_table)) {
			if( zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &gv0) == SUCCESS) {
				if(Z_TYPE_PP(gv0)==IS_ARRAY){
					a0 = *gv0;
				} else {
					PHALCON_INIT_ARRAY(a0);
				}
			}
		} else {
			PHALCON_INIT_ARRAY(a0);
		}
		eval_int = phalcon_array_isset_string(a0, "PHP_SELF", strlen("PHP_SELF")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r1);
			PHALCON_ALLOC_ZVAL(r2);
			PHALCON_ALLOC_ZVAL(r3);
			PHALCON_ALLOC_ZVAL(p3[0]);
			ZVAL_STRING(p3[0], "/", 1);
			PHALCON_ALLOC_ZVAL(r4);
			PHALCON_ALLOC_ZVAL(r5);
			phalcon_array_fetch_string(r5, a0, "PHP_SELF", strlen("PHP_SELF"), PHALCON_NOISY_FETCH TSRMLS_CC);
			p4[0] = r5;
			PHALCON_CALL_FUNC_PARAMS(r4, "dirname", 1, p4);
			p3[1] = r4;
			PHALCON_CALL_FUNC_PARAMS(r3, "explode", 2, p3);
			p2[0] = r3;
			PHALCON_PARAM_LONG(p2[1], 1);
			PHALCON_ALLOC_ZVAL(t2);
			ZVAL_LONG(t2, 1);
			PHALCON_ALLOC_ZVAL(t1);
			ZVAL_LONG(t1, -1);
			PHALCON_MUL_FUNCTION(r6, t1, t2);
			p2[2] = r6;
			PHALCON_CALL_FUNC_PARAMS(r2, "array_slice", 3, p2);
			p1[0] = r2;
			PHALCON_ALLOC_ZVAL(p1[1]);
			ZVAL_STRING(p1[1], "/", 1);
			PHALCON_CALL_FUNC_PARAMS(r1, "join", 2, p1);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(r1);
			v0 = r1;
		} else {
			PHALCON_ALLOC_ZVAL(v0);
			ZVAL_STRING(v0, "", 0);
		}
		if (phalcon_compare_strict_string(v0, "")) {
			phalcon_update_property_string(this_ptr, "_baseUri", strlen("_baseUri"), "/" TSRMLS_CC);
		} else {
			PHALCON_ALLOC_ZVAL(r7);
			phalcon_concat_both(r7,  "/", v0, "/" TSRMLS_CC);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r7, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_baseUri", strlen("_baseUri"), copy TSRMLS_CC);
			}
		}
	}
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_read_property(t3, this_ptr, "_baseUri", sizeof("_baseUri")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	if (Z_TYPE_P(t3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets local path where app/ directory is located
 *
  * @param string $basePath
 */
PHP_METHOD(Phalcon_Controller_Front, setBasePath){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_basePath", strlen("_basePath"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets local path where app/ directory is located
 *
 * @return string
 */
PHP_METHOD(Phalcon_Controller_Front, getBasePath){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_basePath", sizeof("_basePath")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Overwrittes request object default object
 *
 * @param Php_Request $response
 */
PHP_METHOD(Phalcon_Controller_Front, setRequest){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_request", strlen("_request"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Overwrittes response object default object
  *
 * @param Php_Response $response
 */
PHP_METHOD(Phalcon_Controller_Front, setResponse){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_response", strlen("_response"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Overwrittes models manager default object
 *
 * @param Php_Model_Manager $model
 */
PHP_METHOD(Phalcon_Controller_Front, setModelComponent){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_model", strlen("_model"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets the models manager
 *
 * @return Php_Model_Manager
 */
PHP_METHOD(Phalcon_Controller_Front, getModelComponent){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *v0 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_model_manager_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(i0);
		v0 = i0;
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_modelsDir", sizeof("_modelsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		Z_ADDREF_P(t1);
		p1[0] = t1;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setmodelsdir", 1, p1, PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_model", strlen("_model"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	if (Z_TYPE_P(t2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets view component
 *
 * @param Php_View $view
 */
PHP_METHOD(Phalcon_Controller_Front, setViewComponent){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_view", strlen("_view"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets the views part manager
 *
 * @return Php_View
 */
PHP_METHOD(Phalcon_Controller_Front, getViewComponent){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_view", sizeof("_view")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_view_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, i0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_view", strlen("_view"), copy TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_view", sizeof("_view")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_viewsDir", sizeof("_viewsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		Z_ADDREF_P(t2);
		p1[0] = t2;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(t1, "setviewsdir", 1, p1, PHALCON_CALL_DEFAULT);
	}
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_read_property(t3, this_ptr, "_view", sizeof("_view")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	if (Z_TYPE_P(t3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Executes the dispatch loop
 *
 * @return Php_View
 */
PHP_METHOD(Phalcon_Controller_Front, dispatchLoop){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL;
	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL;
	zval *p1[] = { NULL }, *p2[] = { NULL }, *p10[] = { NULL }, *p12[] = { NULL }, *p14[] = { NULL }, *p16[] = { NULL, NULL, NULL, NULL }, *p17[] = { NULL, NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dispatcher", sizeof("_dispatcher")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_dispatcher_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(i0);
		v0 = i0;
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_basePath", sizeof("_basePath")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		Z_ADDREF_P(t1);
		p1[0] = t1;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setbasepath", 1, p1, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_controllersDir", sizeof("_controllersDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		Z_ADDREF_P(t2);
		p2[0] = t2;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setcontrollersdir", 1, p2, PHALCON_CALL_DEFAULT);
	} else {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_dispatcher", sizeof("_dispatcher")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t3);
		v0 = t3;
	}
	PHALCON_ALLOC_ZVAL(t4);
	phalcon_read_property(t4, this_ptr, "_request", sizeof("_request")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t4);
	if (!zend_is_true(t4)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_STATIC(r0, "phalcon_request", "getinstance");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_request", strlen("_request"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t5);
	phalcon_read_property(t5, this_ptr, "_response", sizeof("_response")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t5);
	if (!zend_is_true(t5)) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_STATIC(r1, "phalcon_response", "getinstance");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_response", strlen("_response"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t6);
	phalcon_read_property(t6, this_ptr, "_router", sizeof("_router")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t6);
	if (!zend_is_true(t6)) {
		PHALCON_ALLOC_ZVAL(i1);
		object_init_ex(i1, phalcon_router_rewrite_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i1, "__construct", PHALCON_CALL_CHECK);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(i1);
		v1 = i1;
		PHALCON_CALL_METHOD_NORETURN(v1, "handle", PHALCON_CALL_DEFAULT);
	} else {
		PHALCON_ALLOC_ZVAL(t7);
		phalcon_read_property(t7, this_ptr, "_router", sizeof("_router")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t7);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(t7);
		v1 = t7;
	}
	PHALCON_ALLOC_ZVAL(r2);
	PHALCON_CALL_METHOD(r2, this_ptr, "getviewcomponent", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r2);
	v2 = r2;
	PHALCON_CALL_METHOD_NORETURN(v2, "start", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r3);
	PHALCON_CALL_METHOD(r3, this_ptr, "getmodelcomponent", PHALCON_CALL_DEFAULT);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r3);
	v3 = r3;
	PHALCON_ALLOC_ZVAL(r4);
	PHALCON_CALL_METHOD(r4, v1, "getcontrollername", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(r4);
	p10[0] = r4;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setcontrollername", 1, p10, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r5);
	PHALCON_CALL_METHOD(r5, v1, "getactionname", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(r5);
	p12[0] = r5;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setactionname", 1, p12, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r6);
	PHALCON_CALL_METHOD(r6, v1, "getparams", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(r6);
	p14[0] = r6;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setparams", 1, p14, PHALCON_CALL_DEFAULT);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_dispatcher", strlen("_dispatcher"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_router", strlen("_router"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(t8);
	phalcon_read_property(t8, this_ptr, "_request", sizeof("_request")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t8);
	Z_ADDREF_P(t8);
	p16[0] = t8;
	PHALCON_ALLOC_ZVAL(t9);
	phalcon_read_property(t9, this_ptr, "_response", sizeof("_response")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t9);
	Z_ADDREF_P(t9);
	p16[1] = t9;
	Z_ADDREF_P(v2);
	p16[2] = v2;
	Z_ADDREF_P(v3);
	p16[3] = v3;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "dispatch", 4, p16, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r7);
	PHALCON_CALL_METHOD(r7, v0, "getcontrollername", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(r7);
	p17[0] = r7;
	PHALCON_ALLOC_ZVAL(r8);
	PHALCON_CALL_METHOD(r8, v0, "getactionname", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(r8);
	p17[1] = r8;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "render", 2, p17, PHALCON_CALL_DEFAULT);
	PHALCON_CALL_METHOD_NORETURN(v2, "finish", PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(v2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_View
 *
 * Php_View is a class for working with the "view" portion of the model-view-controller pattern.
 * That is, it exists to help keep the view script separate from the model and controller scripts.
 * It provides a system of helpers, output filters, and variable escaping.
 *
 * 
 */

PHP_METHOD(Phalcon_View, __construct){

	zval *a0 = NULL, *a1 = NULL, *a2 = NULL;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_property(phalcon_view_class_entry, this_ptr, "_params", strlen("_params"), a0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(a1);
	array_init(a1);
	zend_update_property(phalcon_view_class_entry, this_ptr, "_templatesBefore", strlen("_templatesBefore"), a1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(a2);
	array_init(a2);
	zend_update_property(phalcon_view_class_entry, this_ptr, "_templatesAfter", strlen("_templatesAfter"), a2 TSRMLS_CC);
}

/**
 * Sets views directory
 *
 * @param string $viewsDir
 */
PHP_METHOD(Phalcon_View, setViewsDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_viewsDir", strlen("_viewsDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets views directory
 *
 * @return string
 */
PHP_METHOD(Phalcon_View, getViewsDir){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_viewsDir", sizeof("_viewsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Appends template before controller layout
 *
 * @param string $templateAfter
 */
PHP_METHOD(Phalcon_View, setTemplateBefore){

	zval *v0 = NULL;
	zval *a0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_next_index_zval(a0, copy);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, a0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_templatesBefore", strlen("_templatesBefore"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Appends template after controller layout
 *
 * @param string $templateAfter
 */
PHP_METHOD(Phalcon_View, setTemplateAfter){

	zval *v0 = NULL;
	zval *a0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_next_index_zval(a0, copy);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, a0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_templatesAfter", strlen("_templatesAfter"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Adds parameter to views
 *
 * @param string $key
 * @param mixed $value
 */
PHP_METHOD(Phalcon_View, setParamToView){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	{
		zval *orig_ptr = t0;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			Z_DELREF_P(orig_ptr);
			ALLOC_ZVAL(t0);
			*t0 = *orig_ptr;
			zval_copy_ctor(t0);
			Z_SET_REFCOUNT_P(t0, 1);
			Z_UNSET_ISREF_P(t0);
		}
	}
	phalcon_array_update(t0, v0, v1 TSRMLS_CC);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_params", strlen("_params"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns parameters to views
 *
 * @return array
 */
PHP_METHOD(Phalcon_View, getParamsToView){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Starts rendering process
 */
PHP_METHOD(Phalcon_View, start){


	PHALCON_CALL_FUNC_NORETURN("ob_start");
	RETURN_NULL();
}

/**
 * Executes render process from controller data
 *
 * @param string $controllerName
 * @param string $actionName
 */
PHP_METHOD(Phalcon_View, render){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL, *v10 = NULL, *v11 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *ac0 = NULL, *ac1 = NULL, *ac2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *p5[] = { NULL }, *p10[] = { NULL };
	HashTable *ah0, *ah1, *ah2;
	HashPosition hp0, hp1, hp2;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_viewsDir", sizeof("_viewsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(t0);
	v2 = t0;
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_layoutsDir", sizeof("_layoutsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(t1);
	v3 = t1;
	if (phalcon_compare_strict_string(v3, "")) {
		PHALCON_ALLOC_ZVAL(v3);
		ZVAL_STRING(v3, "layouts/", 0);
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	FOREACH_KV(t2, ac0, fes7, fee7, ah0, hp0, v5, v4)
		ZEND_SET_SYMBOL(EG(active_symbol_table), Z_STRVAL_P(v5), v4);
	END_FOREACH(ac0, fes7, fee7, ah0, hp0);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_FUNC(r0, "ob_get_contents");
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, r0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r2);
	concat_function(r2, v2, v0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r1);
	phalcon_concat_vboth(r1, r2, "/", v1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r3);
	phalcon_concat_right(r3, r1, ".phtml" TSRMLS_CC);
	if (v6) {
		Z_DELREF_P(v6);
		if (!Z_REFCOUNT_P(v6)) {
			FREE_ZVAL(v6);
		}
	}
	Z_ADDREF_P(r3);
	v6 = r3;
	if (phalcon_file_exists(v6 TSRMLS_CC) == SUCCESS) {
		PHALCON_CALL_FUNC_NORETURN("ob_clean");
		phalcon_require(v6 TSRMLS_CC);
		if (EG(exception) || EG(exit_status) == 255) {
			return;
		}
		PHALCON_ALLOC_ZVAL(r4);
		PHALCON_CALL_FUNC(r4, "ob_get_contents");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r4, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_read_property(t3, this_ptr, "_templatesBefore", sizeof("_templatesBefore")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	if (v7) {
		Z_DELREF_P(v7);
		if (!Z_REFCOUNT_P(v7)) {
			FREE_ZVAL(v7);
		}
	}
	Z_ADDREF_P(t3);
	v7 = t3;
	if (Z_TYPE_P(v7) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(r5);
		concat_function(r5, v2, v3 TSRMLS_CC);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(r5);
		v6 = r5;
		FOREACH_V(v7, ac1, fes8, fee8, ah1, hp1, v8)
			if (!r6) {
				PHALCON_ALLOC_ZVAL(r6);
			} else {
				if (Z_REFCOUNT_P(r6) > 1) {
					{
						zval *orig_ptr = r6;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r6);
							*r6 = *orig_ptr;
							zval_copy_ctor(r6);
							Z_SET_REFCOUNT_P(r6, 1);
							Z_UNSET_ISREF_P(r6);
						}
					}
				} else {
					FREE_ZVAL(r6);
					PHALCON_ALLOC_ZVAL(r6);
				}
			}
			concat_function(r6, v6, v8 TSRMLS_CC);
			if (!r7) {
				PHALCON_ALLOC_ZVAL(r7);
			} else {
				if (Z_REFCOUNT_P(r7) > 1) {
					{
						zval *orig_ptr = r7;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r7);
							*r7 = *orig_ptr;
							zval_copy_ctor(r7);
							Z_SET_REFCOUNT_P(r7, 1);
							Z_UNSET_ISREF_P(r7);
						}
					}
				} else {
					FREE_ZVAL(r7);
					PHALCON_ALLOC_ZVAL(r7);
				}
			}
			phalcon_concat_right(r7, r6, ".phtml" TSRMLS_CC);
			if (v9) {
				Z_DELREF_P(v9);
				if (!Z_REFCOUNT_P(v9)) {
					FREE_ZVAL(v9);
				}
			}
			Z_ADDREF_P(r7);
			v9 = r7;
			if (phalcon_file_exists(v9 TSRMLS_CC) == SUCCESS) {
				PHALCON_CALL_FUNC_NORETURN("ob_clean");
				phalcon_require(v9 TSRMLS_CC);
				if (EG(exception) || EG(exit_status) == 255) {
					return;
				}
				if (!r8) {
					PHALCON_ALLOC_ZVAL(r8);
				} else {
					if (Z_REFCOUNT_P(r8) > 1) {
						{
							zval *orig_ptr = r8;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r8);
								*r8 = *orig_ptr;
								zval_copy_ctor(r8);
								Z_SET_REFCOUNT_P(r8, 1);
								Z_UNSET_ISREF_P(r8);
							}
						}
					} else {
						FREE_ZVAL(r8);
						PHALCON_ALLOC_ZVAL(r8);
					}
				}
				PHALCON_CALL_FUNC(r8, "ob_get_contents");
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, r8, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
				}
			} else {
				if (!i0) {
					PHALCON_ALLOC_ZVAL(i0);
				} else {
					if (Z_REFCOUNT_P(i0) > 1) {
						{
							zval *orig_ptr = i0;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(i0);
								*i0 = *orig_ptr;
								zval_copy_ctor(i0);
								Z_SET_REFCOUNT_P(i0, 1);
								Z_UNSET_ISREF_P(i0);
							}
						}
					} else {
						FREE_ZVAL(i0);
						PHALCON_ALLOC_ZVAL(i0);
					}
				}
				object_init_ex(i0, phalcon_view_exception_class_entry);
				if (!r9) {
					PHALCON_ALLOC_ZVAL(r9);
				} else {
					if (Z_REFCOUNT_P(r9) > 1) {
						{
							zval *orig_ptr = r9;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r9);
								*r9 = *orig_ptr;
								zval_copy_ctor(r9);
								Z_SET_REFCOUNT_P(r9, 1);
								Z_UNSET_ISREF_P(r9);
							}
						}
					} else {
						FREE_ZVAL(r9);
						PHALCON_ALLOC_ZVAL(r9);
					}
				}
				phalcon_concat_both(r9,  "Template '", v8, "' was not found on templates directory" TSRMLS_CC);
				Z_ADDREF_P(r9);
				p5[0] = r9;
				PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p5, PHALCON_CALL_CHECK);
				zend_throw_exception_object(i0 TSRMLS_CC);
				Z_ADDREF_P(i0);
				return;
			}
		END_FOREACH(ac1, fes8, fee8, ah1, hp1);
	}
	PHALCON_ALLOC_ZVAL(r10);
	concat_function(r10, v2, v3 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r11);
	concat_function(r11, r10, v0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r12);
	phalcon_concat_right(r12, r11, ".phtml" TSRMLS_CC);
	if (v6) {
		Z_DELREF_P(v6);
		if (!Z_REFCOUNT_P(v6)) {
			FREE_ZVAL(v6);
		}
	}
	Z_ADDREF_P(r12);
	v6 = r12;
	if (phalcon_file_exists(v6 TSRMLS_CC) == SUCCESS) {
		PHALCON_CALL_FUNC_NORETURN("ob_clean");
		phalcon_require(v6 TSRMLS_CC);
		if (EG(exception) || EG(exit_status) == 255) {
			return;
		}
		PHALCON_ALLOC_ZVAL(r13);
		PHALCON_CALL_FUNC(r13, "ob_get_contents");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r13, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t4);
	phalcon_read_property(t4, this_ptr, "_templatesAfter", sizeof("_templatesAfter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t4);
	if (v10) {
		Z_DELREF_P(v10);
		if (!Z_REFCOUNT_P(v10)) {
			FREE_ZVAL(v10);
		}
	}
	Z_ADDREF_P(t4);
	v10 = t4;
	if (Z_TYPE_P(v10) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(r14);
		concat_function(r14, v2, v3 TSRMLS_CC);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(r14);
		v6 = r14;
		FOREACH_V(v10, ac2, fes9, fee9, ah2, hp2, v11)
			if (!r15) {
				PHALCON_ALLOC_ZVAL(r15);
			} else {
				if (Z_REFCOUNT_P(r15) > 1) {
					{
						zval *orig_ptr = r15;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r15);
							*r15 = *orig_ptr;
							zval_copy_ctor(r15);
							Z_SET_REFCOUNT_P(r15, 1);
							Z_UNSET_ISREF_P(r15);
						}
					}
				} else {
					FREE_ZVAL(r15);
					PHALCON_ALLOC_ZVAL(r15);
				}
			}
			concat_function(r15, v6, v11 TSRMLS_CC);
			if (!r16) {
				PHALCON_ALLOC_ZVAL(r16);
			} else {
				if (Z_REFCOUNT_P(r16) > 1) {
					{
						zval *orig_ptr = r16;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r16);
							*r16 = *orig_ptr;
							zval_copy_ctor(r16);
							Z_SET_REFCOUNT_P(r16, 1);
							Z_UNSET_ISREF_P(r16);
						}
					}
				} else {
					FREE_ZVAL(r16);
					PHALCON_ALLOC_ZVAL(r16);
				}
			}
			phalcon_concat_right(r16, r15, ".phtml" TSRMLS_CC);
			if (v9) {
				Z_DELREF_P(v9);
				if (!Z_REFCOUNT_P(v9)) {
					FREE_ZVAL(v9);
				}
			}
			Z_ADDREF_P(r16);
			v9 = r16;
			if (phalcon_file_exists(v9 TSRMLS_CC) == SUCCESS) {
				PHALCON_CALL_FUNC_NORETURN("ob_clean");
				phalcon_require(v9 TSRMLS_CC);
				if (EG(exception) || EG(exit_status) == 255) {
					return;
				}
				if (!r17) {
					PHALCON_ALLOC_ZVAL(r17);
				} else {
					if (Z_REFCOUNT_P(r17) > 1) {
						{
							zval *orig_ptr = r17;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r17);
								*r17 = *orig_ptr;
								zval_copy_ctor(r17);
								Z_SET_REFCOUNT_P(r17, 1);
								Z_UNSET_ISREF_P(r17);
							}
						}
					} else {
						FREE_ZVAL(r17);
						PHALCON_ALLOC_ZVAL(r17);
					}
				}
				PHALCON_CALL_FUNC(r17, "ob_get_contents");
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, r17, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
				}
			} else {
				if (!i1) {
					PHALCON_ALLOC_ZVAL(i1);
				} else {
					if (Z_REFCOUNT_P(i1) > 1) {
						{
							zval *orig_ptr = i1;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(i1);
								*i1 = *orig_ptr;
								zval_copy_ctor(i1);
								Z_SET_REFCOUNT_P(i1, 1);
								Z_UNSET_ISREF_P(i1);
							}
						}
					} else {
						FREE_ZVAL(i1);
						PHALCON_ALLOC_ZVAL(i1);
					}
				}
				object_init_ex(i1, phalcon_view_exception_class_entry);
				if (!r18) {
					PHALCON_ALLOC_ZVAL(r18);
				} else {
					if (Z_REFCOUNT_P(r18) > 1) {
						{
							zval *orig_ptr = r18;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r18);
								*r18 = *orig_ptr;
								zval_copy_ctor(r18);
								Z_SET_REFCOUNT_P(r18, 1);
								Z_UNSET_ISREF_P(r18);
							}
						}
					} else {
						FREE_ZVAL(r18);
						PHALCON_ALLOC_ZVAL(r18);
					}
				}
				phalcon_concat_both(r18,  "Template '", v8, "' was not found on templates directory" TSRMLS_CC);
				Z_ADDREF_P(r18);
				p10[0] = r18;
				PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p10, PHALCON_CALL_CHECK);
				zend_throw_exception_object(i1 TSRMLS_CC);
				Z_ADDREF_P(i1);
				return;
			}
		END_FOREACH(ac2, fes9, fee9, ah2, hp2);
	}
	PHALCON_ALLOC_ZVAL(r19);
	phalcon_concat_right(r19, v2, "index.phtml" TSRMLS_CC);
	if (v6) {
		Z_DELREF_P(v6);
		if (!Z_REFCOUNT_P(v6)) {
			FREE_ZVAL(v6);
		}
	}
	Z_ADDREF_P(r19);
	v6 = r19;
	if (phalcon_file_exists(v6 TSRMLS_CC) == SUCCESS) {
		PHALCON_CALL_FUNC_NORETURN("ob_clean");
		phalcon_require(v6 TSRMLS_CC);
		if (EG(exception) || EG(exit_status) == 255) {
			return;
		}
		PHALCON_ALLOC_ZVAL(r20);
		PHALCON_CALL_FUNC(r20, "ob_get_contents");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r20, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_content", strlen("_content"), copy TSRMLS_CC);
		}
	}
	RETURN_NULL();
}

/**
 * Finish render process
 */
PHP_METHOD(Phalcon_View, finish){


	PHALCON_CALL_FUNC_NORETURN("ob_end_clean");
	RETURN_NULL();
}

/**
 * Returns cached ouput on another view stage
 *
 * @return string
 */
PHP_METHOD(Phalcon_View, getContent){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_content", sizeof("_content")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Generates a external absolute path to an application uri
 *
 * @return string
 */
PHP_METHOD(Phalcon_View, url){

	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		p0[0] = v0;
		PHALCON_CALL_FUNC_PARAMS_NORETURN("print_r", 1, p0);
	} else {
		PHALCON_ALLOC_ZVAL(r0);
		Z_ADDREF_P(v0);
		p1[0] = v0;
		PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_utils", "geturl", 1, p1);
		RETURN_ZVAL(r0, 1, 0);
	}
	RETURN_NULL();
}

/**
 * Returns local path
 *
 * @return string
 */
PHP_METHOD(Phalcon_View, path){

	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_utils", "getlocalpath", 1, p0);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Test, nice){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	add_assoc_long(a0, "hello1", 1);
	add_assoc_long(a0, "hello2", 2);
	add_assoc_long(a0, "hello3", 3);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(a0);
	v1 = a0;
	FOREACH_KV(v1, ac0, fes10, fee10, ah0, hp0, v3, v2)
		if (!r0) {
			PHALCON_ALLOC_ZVAL(r0);
		} else {
			if (Z_REFCOUNT_P(r0) > 1) {
				{
					zval *orig_ptr = r0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r0);
						*r0 = *orig_ptr;
						zval_copy_ctor(r0);
						Z_SET_REFCOUNT_P(r0, 1);
						Z_UNSET_ISREF_P(r0);
					}
				}
			} else {
				FREE_ZVAL(r0);
				PHALCON_ALLOC_ZVAL(r0);
			}
		}
		phalcon_array_fetch(r0, v1, v3, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (!t0) {
			PHALCON_ALLOC_ZVAL(t0);
		} else {
			if (Z_REFCOUNT_P(t0) > 1) {
				SEPARATE_ZVAL(&t0);
			} else {
				if (Z_TYPE_P(t0) != IS_LONG) {
					FREE_ZVAL(t0);
					PHALCON_ALLOC_ZVAL(t0);
				}
			}
		}
		ZVAL_LONG(t0, 100);
		if (!r1) {
			PHALCON_ALLOC_ZVAL(r1);
		} else {
			if (Z_REFCOUNT_P(r1) > 1){ 
				{
					zval *orig_ptr = r1;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r1);
						*r1 = *orig_ptr;
						zval_copy_ctor(r1);
						Z_SET_REFCOUNT_P(r1, 1);
						Z_UNSET_ISREF_P(r1);
					}
				}
			}
		}
		add_function(r1, r0, t0 TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = v1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v1);
					*v1 = *orig_ptr;
					zval_copy_ctor(v1);
					Z_SET_REFCOUNT_P(v1, 1);
					Z_UNSET_ISREF_P(v1);
				}
			}
			phalcon_array_update(v1, v3, copy TSRMLS_CC);
		}
	END_FOREACH(ac0, fes10, fee10, ah0, hp0);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS_NORETURN("print_r", 1, p0);
	RETURN_NULL();
}

/**
 * Php_Utils
 *
 * Implements functionality used widely by the framework
 */

/**
 * Converts strings to camelize style
 *
 * 
 *
 * @param string $str
 * @return string
 */
PHP_METHOD(Phalcon_Utils, camelize){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *p3[] = { NULL }, *p2[] = { NULL, NULL, NULL }, *p1[] = { NULL }, *p0[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], " ", 1);
	PHALCON_ALLOC_ZVAL(p0[1]);
	ZVAL_STRING(p0[1], "", 1);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(r2);
	PHALCON_ALLOC_ZVAL(p2[0]);
	ZVAL_STRING(p2[0], "_", 1);
	PHALCON_ALLOC_ZVAL(p2[1]);
	ZVAL_STRING(p2[1], " ", 1);
	PHALCON_ALLOC_ZVAL(r3);
	p3[0] = v0;
	PHALCON_CALL_FUNC_PARAMS(r3, "strtolower", 1, p3);
	p2[2] = r3;
	PHALCON_CALL_FUNC_PARAMS(r2, "str_replace", 3, p2);
	p1[0] = r2;
	PHALCON_CALL_FUNC_PARAMS(r1, "ucwords", 1, p1);
	p0[2] = r1;
	PHALCON_CALL_FUNC_PARAMS(r0, "str_replace", 3, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Uncamelize strings which are camelized
 *
 * 
 *
 * @param string $str
 * @return string
 */
PHP_METHOD(Phalcon_Utils, uncamelize){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *p0[] = { NULL, NULL }, *p2[] = { NULL, NULL, NULL }, *p1[] = { NULL }, *p3[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	add_assoc_string(a0, "/(?<=(?:[A-Z]))([A-Z]+)([A-Z][A-z])/", "\\1_\\2", 1);
	add_assoc_string(a0, "/(?<=(?:[a-z]))([A-Z])/", "_\\1", 1);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(a0);
	v1 = a0;
	FOREACH_KV(v1, ac0, fes11, fee11, ah0, hp0, v3, v2)
		if (!r0) {
			PHALCON_ALLOC_ZVAL(r0);
		} else {
			if (Z_REFCOUNT_P(r0) > 1) {
				{
					zval *orig_ptr = r0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r0);
						*r0 = *orig_ptr;
						zval_copy_ctor(r0);
						Z_SET_REFCOUNT_P(r0, 1);
						Z_UNSET_ISREF_P(r0);
					}
				}
			} else {
				FREE_ZVAL(r0);
				PHALCON_ALLOC_ZVAL(r0);
			}
		}
		Z_ADDREF_P(v3);
		p0[0] = v3;
		Z_ADDREF_P(v0);
		p0[1] = v0;
		PHALCON_CALL_FUNC_PARAMS(r0, "preg_match", 2, p0);
		if (zend_is_true(r0)) {
			if (!r1) {
				PHALCON_ALLOC_ZVAL(r1);
			} else {
				if (Z_REFCOUNT_P(r1) > 1) {
					{
						zval *orig_ptr = r1;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r1);
							*r1 = *orig_ptr;
							zval_copy_ctor(r1);
							Z_SET_REFCOUNT_P(r1, 1);
							Z_UNSET_ISREF_P(r1);
						}
					}
				} else {
					FREE_ZVAL(r1);
					PHALCON_ALLOC_ZVAL(r1);
				}
			}
			if (!r2) {
				PHALCON_ALLOC_ZVAL(r2);
			} else {
				if (Z_REFCOUNT_P(r2) > 1) {
					{
						zval *orig_ptr = r2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r2);
							*r2 = *orig_ptr;
							zval_copy_ctor(r2);
							Z_SET_REFCOUNT_P(r2, 1);
							Z_UNSET_ISREF_P(r2);
						}
					}
				} else {
					FREE_ZVAL(r2);
					PHALCON_ALLOC_ZVAL(r2);
				}
			}
			Z_ADDREF_P(v3);
			p2[0] = v3;
			Z_ADDREF_P(v2);
			p2[1] = v2;
			Z_ADDREF_P(v0);
			p2[2] = v0;
			PHALCON_CALL_FUNC_PARAMS(r2, "preg_replace", 3, p2);
			p1[0] = r2;
			PHALCON_CALL_FUNC_PARAMS(r1, "strtolower", 1, p1);
			RETURN_ZVAL(r1, 1, 0);
		}
	END_FOREACH(ac0, fes11, fee11, ah0, hp0);
	PHALCON_ALLOC_ZVAL(r3);
	p3[0] = v0;
	PHALCON_CALL_FUNC_PARAMS(r3, "strtolower", 1, p3);
	RETURN_ZVAL(r3, 1, 0);
}

/**
 * Gets public URL to phalcon instance
 *
 * @param string $params
 * @return string
 */
PHP_METHOD(Phalcon_Utils, getUrl){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_STATIC(r1, "phalcon_controller_front", "getinstance");
	PHALCON_CALL_METHOD(r0, r1, "getbaseuri", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r2);
	concat_function(r2, r0, v0 TSRMLS_CC);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Gets path to local file
 *
 * @param string $params
 * @return string
 */
PHP_METHOD(Phalcon_Utils, getLocalPath){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_STATIC(r1, "phalcon_controller_front", "getinstance");
	PHALCON_CALL_METHOD(r0, r1, "getbasepath", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r2);
	concat_function(r2, r0, v0 TSRMLS_CC);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Php_Db
 *
 * Php_Db and its related classes provide a simple SQL database interface for Phalcon Framework.
 * The Php_Db is the basic class you use to connect your PHP application to a RDBMS.
 * There is a different adapter class for each brand of RDBMS.
 *
 * This component is intended to low level database operations. If you want to interact with databases using
 * high level abstraction use Php_Model.
 *
 * Php_Db is an abstract class. You only can use it with a database adapter like Php_Db_Mysql
 *
 * 
 */

/**
 * Php_Db contructor, This method does not should to be called directly. Use Php_Db::factory instead
 *
 * @param stdClass $descriptor
 */
PHP_METHOD(Phalcon_Db, __construct){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_descriptor", strlen("_descriptor"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sets a logger class to log all SQL operations sended to database server
 *
 * @param Php_Logger $logger
 */
PHP_METHOD(Phalcon_Db, setLogger){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_logger", strlen("_logger"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sends a SQL operation made to the logger
 *
 * @param string $sqlStatement
 * @param int $type
 */
PHP_METHOD(Phalcon_Db, log){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_logger", sizeof("_logger")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_logger", sizeof("_logger")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		Z_ADDREF_P(v0);
		p0[0] = v0;
		Z_ADDREF_P(v1);
		p0[1] = v1;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(t1, "log", 2, p0, PHALCON_CALL_DEFAULT);
	}
	RETURN_NULL();
}

/**
 * Returns the first row in a SQL query result
 *
 * 
 *
 * @param string $sqlQuery
 * @return array
 */
PHP_METHOD(Phalcon_Db, fetchOne){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *a0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "query", 1, p0, PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	if (zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r1);
		Z_ADDREF_P(v1);
		p1[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "fetcharray", 1, p1, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r1, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(a0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	}
	RETURN_NULL();
}

/**
 * Dumps the complete result of a query into an array
 *
 * 
 *
 * @param string $sqlQuery
 * @return array
 */
PHP_METHOD(Phalcon_Db, fetchAll){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(a0);
	v1 = a0;
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "query", 1, p0, PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	PHALCON_INIT_FALSE(t0);
	PHALCON_NOT_EQUAL_FUNCTION(r1, v2, t0);
	if (zend_is_true(r1)) {
		ws12:
		PHALCON_ALLOC_ZVAL(r2);
		Z_ADDREF_P(v2);
		p1[0] = v2;
		PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "fetcharray", 1, p1, PHALCON_CALL_DEFAULT);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r2);
		v3 = r2;
		if (!zend_is_true(v3)) {
			goto we12;
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v3, 1, 0);
			Z_SET_REFCOUNT_P(copy, 1);
			{
				zval *orig_ptr = v1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v1);
					*v1 = *orig_ptr;
					zval_copy_ctor(v1);
					Z_SET_REFCOUNT_P(v1, 1);
					Z_UNSET_ISREF_P(v1);
				}
			}
			phalcon_array_append(v1, copy TSRMLS_CC);
		}
		goto ws12;
		we12:
		v3 = NULL;
	}
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v1);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Inserts data into a database table using custom RBDM SQL syntax
 *
 * 
 *
 * @param string $tables
 * @param array $values
 * @param array $fields
 * @param boolean $automaticQuotes
 * @return boolean
 */
PHP_METHOD(Phalcon_Db, insert){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL;
	zval *t0 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL, NULL, NULL }, *p2[] = { NULL }, *p4[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL, NULL }, *p7[] = { NULL, NULL }, *p8[] = { NULL, NULL }, *p9[] = { NULL }, *p10[] = { NULL, NULL, NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|zz", &v0, &v1, &v2, &v3) == FAILURE) {
		RETURN_NULL();
	}

	if (!v2) {
		PHALCON_INIT_NULL(v2);
	}
	if (!v3) {
		PHALCON_INIT_BOOL(v3, 0);
	}
	
	PHALCON_ALLOC_ZVAL(v4);
	ZVAL_STRING(v4, "", 0);
	if (Z_TYPE_P(v1) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(r0);
		p0[0] = v1;
		PHALCON_CALL_FUNC_PARAMS(r0, "count", 1, p0);
		PHALCON_ALLOC_ZVAL(t0);
		ZVAL_LONG(t0, 0);
		PHALCON_EQUAL_FUNCTION(r1, r0, t0);
		if (zend_is_true(r1)) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_db_exception_class_entry);
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_concat_both(r2,  "Unable to insert into ", v0, " without data" TSRMLS_CC);
			Z_ADDREF_P(r2);
			p1[0] = r2;
			PHALCON_PARAM_LONG(p1[1], 0);
			PHALCON_PARAM_BOOL(p1[2], 1);
			Z_ADDREF_P(this_ptr);
			p1[3] = this_ptr;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p1, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		} else {
			if (zend_is_true(v3)) {
				FOREACH_KV(v1, ac0, fes13, fee13, ah0, hp0, v6, v5)
					if (!r3) {
						PHALCON_ALLOC_ZVAL(r3);
					} else {
						if (Z_REFCOUNT_P(r3) > 1) {
							{
								zval *orig_ptr = r3;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r3);
									*r3 = *orig_ptr;
									zval_copy_ctor(r3);
									Z_SET_REFCOUNT_P(r3, 1);
									Z_UNSET_ISREF_P(r3);
								}
							}
						} else {
							FREE_ZVAL(r3);
							PHALCON_ALLOC_ZVAL(r3);
						}
					}
					phalcon_array_fetch(r3, v1, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
					if (Z_TYPE_P(r3) == IS_OBJECT) {
						if (!r4) {
							PHALCON_ALLOC_ZVAL(r4);
						} else {
							if (Z_REFCOUNT_P(r4) > 1) {
								{
									zval *orig_ptr = r4;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r4);
										*r4 = *orig_ptr;
										zval_copy_ctor(r4);
										Z_SET_REFCOUNT_P(r4, 1);
										Z_UNSET_ISREF_P(r4);
									}
								}
							} else {
								FREE_ZVAL(r4);
								PHALCON_ALLOC_ZVAL(r4);
							}
						}
						phalcon_array_fetch(r4, v1, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
						if (!r5) {
							PHALCON_ALLOC_ZVAL(r5);
						} else {
							if (Z_REFCOUNT_P(r5) > 1){ 
								{
									zval *orig_ptr = r5;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r5);
										*r5 = *orig_ptr;
										zval_copy_ctor(r5);
										Z_SET_REFCOUNT_P(r5, 1);
										Z_UNSET_ISREF_P(r5);
									}
								}
							}
						}
						phalcon_instance_of(r5, r4, phalcon_db_rawvalue_class_entry TSRMLS_CC);
						if (zend_is_true(r5)) {
							if (!r6) {
								PHALCON_ALLOC_ZVAL(r6);
							} else {
								if (Z_REFCOUNT_P(r6) > 1) {
									{
										zval *orig_ptr = r6;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r6);
											*r6 = *orig_ptr;
											zval_copy_ctor(r6);
											Z_SET_REFCOUNT_P(r6, 1);
											Z_UNSET_ISREF_P(r6);
										}
									}
								} else {
									FREE_ZVAL(r6);
									PHALCON_ALLOC_ZVAL(r6);
								}
							}
							if (!r7) {
								PHALCON_ALLOC_ZVAL(r7);
							} else {
								if (Z_REFCOUNT_P(r7) > 1) {
									{
										zval *orig_ptr = r7;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r7);
											*r7 = *orig_ptr;
											zval_copy_ctor(r7);
											Z_SET_REFCOUNT_P(r7, 1);
											Z_UNSET_ISREF_P(r7);
										}
									}
								} else {
									FREE_ZVAL(r7);
									PHALCON_ALLOC_ZVAL(r7);
								}
							}
							if (!r8) {
								PHALCON_ALLOC_ZVAL(r8);
							} else {
								if (Z_REFCOUNT_P(r8) > 1) {
									{
										zval *orig_ptr = r8;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r8);
											*r8 = *orig_ptr;
											zval_copy_ctor(r8);
											Z_SET_REFCOUNT_P(r8, 1);
											Z_UNSET_ISREF_P(r8);
										}
									}
								} else {
									FREE_ZVAL(r8);
									PHALCON_ALLOC_ZVAL(r8);
								}
							}
							phalcon_array_fetch(r8, v1, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
							PHALCON_CALL_METHOD(r7, r8, "getvalue", PHALCON_CALL_DEFAULT);
							Z_ADDREF_P(r7);
							p2[0] = r7;
							PHALCON_CALL_FUNC_PARAMS(r6, "addslashes", 1, p2);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r6, 1, 0);
								Z_SET_REFCOUNT_P(copy, 0);
								{
									zval *orig_ptr = v1;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										ALLOC_ZVAL(v1);
										*v1 = *orig_ptr;
										zval_copy_ctor(v1);
										Z_SET_REFCOUNT_P(v1, 1);
										Z_UNSET_ISREF_P(v1);
									}
								}
								phalcon_array_update(v1, v6, copy TSRMLS_CC);
							}
							goto fes13;
						} else {
							if (!r9) {
								PHALCON_ALLOC_ZVAL(r9);
							} else {
								if (Z_REFCOUNT_P(r9) > 1) {
									{
										zval *orig_ptr = r9;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r9);
											*r9 = *orig_ptr;
											zval_copy_ctor(r9);
											Z_SET_REFCOUNT_P(r9, 1);
											Z_UNSET_ISREF_P(r9);
										}
									}
								} else {
									FREE_ZVAL(r9);
									PHALCON_ALLOC_ZVAL(r9);
								}
							}
							if (!r10) {
								PHALCON_ALLOC_ZVAL(r10);
							} else {
								if (Z_REFCOUNT_P(r10) > 1) {
									{
										zval *orig_ptr = r10;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r10);
											*r10 = *orig_ptr;
											zval_copy_ctor(r10);
											Z_SET_REFCOUNT_P(r10, 1);
											Z_UNSET_ISREF_P(r10);
										}
									}
								} else {
									FREE_ZVAL(r10);
									PHALCON_ALLOC_ZVAL(r10);
								}
							}
							phalcon_array_fetch(r10, v1, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
							Z_ADDREF_P(r10);
							p4[0] = r10;
							PHALCON_CALL_FUNC_PARAMS(r9, "addslashes", 1, p4);
							if (!r11) {
								PHALCON_ALLOC_ZVAL(r11);
							} else {
								if (Z_REFCOUNT_P(r11) > 1) {
									{
										zval *orig_ptr = r11;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r11);
											*r11 = *orig_ptr;
											zval_copy_ctor(r11);
											Z_SET_REFCOUNT_P(r11, 1);
											Z_UNSET_ISREF_P(r11);
										}
									}
								} else {
									FREE_ZVAL(r11);
									PHALCON_ALLOC_ZVAL(r11);
								}
							}
							phalcon_concat_both(r11,  "'", r9, "'" TSRMLS_CC);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r11, 1, 0);
								Z_SET_REFCOUNT_P(copy, 0);
								{
									zval *orig_ptr = v1;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										ALLOC_ZVAL(v1);
										*v1 = *orig_ptr;
										zval_copy_ctor(v1);
										Z_SET_REFCOUNT_P(v1, 1);
										Z_UNSET_ISREF_P(v1);
									}
								}
								phalcon_array_update(v1, v6, copy TSRMLS_CC);
							}
						}
					} else {
						if (!r12) {
							PHALCON_ALLOC_ZVAL(r12);
						} else {
							if (Z_REFCOUNT_P(r12) > 1) {
								{
									zval *orig_ptr = r12;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r12);
										*r12 = *orig_ptr;
										zval_copy_ctor(r12);
										Z_SET_REFCOUNT_P(r12, 1);
										Z_UNSET_ISREF_P(r12);
									}
								}
							} else {
								FREE_ZVAL(r12);
								PHALCON_ALLOC_ZVAL(r12);
							}
						}
						if (!r13) {
							PHALCON_ALLOC_ZVAL(r13);
						} else {
							if (Z_REFCOUNT_P(r13) > 1) {
								{
									zval *orig_ptr = r13;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r13);
										*r13 = *orig_ptr;
										zval_copy_ctor(r13);
										Z_SET_REFCOUNT_P(r13, 1);
										Z_UNSET_ISREF_P(r13);
									}
								}
							} else {
								FREE_ZVAL(r13);
								PHALCON_ALLOC_ZVAL(r13);
							}
						}
						phalcon_array_fetch(r13, v1, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
						Z_ADDREF_P(r13);
						p5[0] = r13;
						PHALCON_CALL_FUNC_PARAMS(r12, "addslashes", 1, p5);
						if (!r14) {
							PHALCON_ALLOC_ZVAL(r14);
						} else {
							if (Z_REFCOUNT_P(r14) > 1) {
								{
									zval *orig_ptr = r14;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r14);
										*r14 = *orig_ptr;
										zval_copy_ctor(r14);
										Z_SET_REFCOUNT_P(r14, 1);
										Z_UNSET_ISREF_P(r14);
									}
								}
							} else {
								FREE_ZVAL(r14);
								PHALCON_ALLOC_ZVAL(r14);
							}
						}
						phalcon_concat_both(r14,  "'", r12, "'" TSRMLS_CC);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, r14, 1, 0);
							Z_SET_REFCOUNT_P(copy, 0);
							{
								zval *orig_ptr = v1;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									ALLOC_ZVAL(v1);
									*v1 = *orig_ptr;
									zval_copy_ctor(v1);
									Z_SET_REFCOUNT_P(v1, 1);
									Z_UNSET_ISREF_P(v1);
								}
							}
							phalcon_array_update(v1, v6, copy TSRMLS_CC);
						}
					}
				END_FOREACH(ac0, fes13, fee13, ah0, hp0);
			}
		}
		if (Z_TYPE_P(v2) == IS_ARRAY) { 
			PHALCON_ALLOC_ZVAL(r17);
			phalcon_concat_left(r17, "INSERT INTO ", v0 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r18);
			PHALCON_ALLOC_ZVAL(p6[0]);
			ZVAL_STRING(p6[0], ", ", 1);
			p6[1] = v2;
			PHALCON_CALL_FUNC_PARAMS(r18, "join", 2, p6);
			PHALCON_ALLOC_ZVAL(r16);
			phalcon_concat_vboth(r16, r17, " (", r18 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r19);
			PHALCON_ALLOC_ZVAL(p7[0]);
			ZVAL_STRING(p7[0], ", ", 1);
			p7[1] = v1;
			PHALCON_CALL_FUNC_PARAMS(r19, "join", 2, p7);
			PHALCON_ALLOC_ZVAL(r15);
			phalcon_concat_vboth(r15, r16, ") VALUES (", r19 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r20);
			phalcon_concat_right(r20, r15, ")" TSRMLS_CC);
			if (v4) {
				Z_DELREF_P(v4);
				if (!Z_REFCOUNT_P(v4)) {
					FREE_ZVAL(v4);
				}
			}
			Z_ADDREF_P(r20);
			v4 = r20;
		} else {
			PHALCON_ALLOC_ZVAL(r22);
			phalcon_concat_left(r22, "INSERT INTO ", v0 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r23);
			PHALCON_ALLOC_ZVAL(p8[0]);
			ZVAL_STRING(p8[0], ", ", 1);
			p8[1] = v1;
			PHALCON_CALL_FUNC_PARAMS(r23, "join", 2, p8);
			PHALCON_ALLOC_ZVAL(r21);
			phalcon_concat_vboth(r21, r22, " VALUES (", r23 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r24);
			phalcon_concat_right(r24, r21, ")" TSRMLS_CC);
			if (v4) {
				Z_DELREF_P(v4);
				if (!Z_REFCOUNT_P(v4)) {
					FREE_ZVAL(v4);
				}
			}
			Z_ADDREF_P(r24);
			v4 = r24;
		}
		PHALCON_ALLOC_ZVAL(r25);
		Z_ADDREF_P(v4);
		p9[0] = v4;
		PHALCON_CALL_METHOD_PARAMS(r25, this_ptr, "query", 1, p9, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r25, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(i1);
		object_init_ex(i1, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p10[0]);
		ZVAL_STRING(p10[0], "The second parameter for insert isn't an Array", 1);
		PHALCON_PARAM_LONG(p10[1], 0);
		PHALCON_PARAM_BOOL(p10[2], 1);
		Z_ADDREF_P(this_ptr);
		p10[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 4, p10, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i1 TSRMLS_CC);
		Z_ADDREF_P(i1);
		return;
	}
	RETURN_NULL();
}

/**
 * Updates data on a table using custom RBDM SQL syntax
 *
 * 
 *
 * @param string $tables
 * @param array $fields
 * @param array $values
 * @param string $whereCondition
 * @param boolean $automaticQuotes
 * @return boolean
 */
PHP_METHOD(Phalcon_Db, update){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL;
	zval *i0 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL, NULL, NULL, NULL }, *p3[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL }, *p7[] = { NULL, NULL }, *p8[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz|zz", &v0, &v1, &v2, &v3, &v4) == FAILURE) {
		RETURN_NULL();
	}

	if (!v3) {
		PHALCON_INIT_NULL(v3);
	}
	if (!v4) {
		PHALCON_INIT_BOOL(v4, 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_concat_both(r0,  "UPDATE ", v0, " SET " TSRMLS_CC);
	if (v5) {
		Z_DELREF_P(v5);
		if (!Z_REFCOUNT_P(v5)) {
			FREE_ZVAL(v5);
		}
	}
	Z_ADDREF_P(r0);
	v5 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r1, "count", 1, p0);
	PHALCON_ALLOC_ZVAL(r2);
	p1[0] = v2;
	PHALCON_CALL_FUNC_PARAMS(r2, "count", 1, p1);
	PHALCON_NOT_EQUAL_FUNCTION(r3, r1, r2);
	if (zend_is_true(r3)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p2[0]);
		ZVAL_STRING(p2[0], "The number of values to update is not the same as fields", 1);
		PHALCON_PARAM_LONG(p2[1], 0);
		PHALCON_PARAM_BOOL(p2[2], 1);
		Z_ADDREF_P(this_ptr);
		p2[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p2, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	PHALCON_ALLOC_ZVAL(v6);
	ZVAL_LONG(v6, 0);
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v7) {
		Z_DELREF_P(v7);
		if (!Z_REFCOUNT_P(v7)) {
			FREE_ZVAL(v7);
		}
	}
	Z_ADDREF_P(a0);
	v7 = a0;
	FOREACH_V(v1, ac0, fes14, fee14, ah0, hp0, v8)
		if (zend_is_true(v4)) {
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_array_fetch(r4, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r4) == IS_OBJECT) {
				if (!r5) {
					PHALCON_ALLOC_ZVAL(r5);
				} else {
					if (Z_REFCOUNT_P(r5) > 1) {
						{
							zval *orig_ptr = r5;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r5);
								*r5 = *orig_ptr;
								zval_copy_ctor(r5);
								Z_SET_REFCOUNT_P(r5, 1);
								Z_UNSET_ISREF_P(r5);
							}
						}
					} else {
						FREE_ZVAL(r5);
						PHALCON_ALLOC_ZVAL(r5);
					}
				}
				phalcon_array_fetch(r5, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
				if (!r6) {
					PHALCON_ALLOC_ZVAL(r6);
				} else {
					if (Z_REFCOUNT_P(r6) > 1){ 
						{
							zval *orig_ptr = r6;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r6);
								*r6 = *orig_ptr;
								zval_copy_ctor(r6);
								Z_SET_REFCOUNT_P(r6, 1);
								Z_UNSET_ISREF_P(r6);
							}
						}
					}
				}
				phalcon_instance_of(r6, r5, phalcon_db_rawvalue_class_entry TSRMLS_CC);
				if (zend_is_true(r6)) {
					if (!r7) {
						PHALCON_ALLOC_ZVAL(r7);
					} else {
						if (Z_REFCOUNT_P(r7) > 1) {
							{
								zval *orig_ptr = r7;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r7);
									*r7 = *orig_ptr;
									zval_copy_ctor(r7);
									Z_SET_REFCOUNT_P(r7, 1);
									Z_UNSET_ISREF_P(r7);
								}
							}
						} else {
							FREE_ZVAL(r7);
							PHALCON_ALLOC_ZVAL(r7);
						}
					}
					if (!r8) {
						PHALCON_ALLOC_ZVAL(r8);
					} else {
						if (Z_REFCOUNT_P(r8) > 1) {
							{
								zval *orig_ptr = r8;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r8);
									*r8 = *orig_ptr;
									zval_copy_ctor(r8);
									Z_SET_REFCOUNT_P(r8, 1);
									Z_UNSET_ISREF_P(r8);
								}
							}
						} else {
							FREE_ZVAL(r8);
							PHALCON_ALLOC_ZVAL(r8);
						}
					}
					if (!r9) {
						PHALCON_ALLOC_ZVAL(r9);
					} else {
						if (Z_REFCOUNT_P(r9) > 1) {
							{
								zval *orig_ptr = r9;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r9);
									*r9 = *orig_ptr;
									zval_copy_ctor(r9);
									Z_SET_REFCOUNT_P(r9, 1);
									Z_UNSET_ISREF_P(r9);
								}
							}
						} else {
							FREE_ZVAL(r9);
							PHALCON_ALLOC_ZVAL(r9);
						}
					}
					phalcon_array_fetch(r9, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
					PHALCON_CALL_METHOD(r8, r9, "getvalue", PHALCON_CALL_DEFAULT);
					Z_ADDREF_P(r8);
					p3[0] = r8;
					PHALCON_CALL_FUNC_PARAMS(r7, "addslashes", 1, p3);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, r7, 1, 0);
						Z_SET_REFCOUNT_P(copy, 0);
						{
							zval *orig_ptr = v2;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								ALLOC_ZVAL(v2);
								*v2 = *orig_ptr;
								zval_copy_ctor(v2);
								Z_SET_REFCOUNT_P(v2, 1);
								Z_UNSET_ISREF_P(v2);
							}
						}
						phalcon_array_update(v2, v6, copy TSRMLS_CC);
					}
				} else {
					if (!r10) {
						PHALCON_ALLOC_ZVAL(r10);
					} else {
						if (Z_REFCOUNT_P(r10) > 1) {
							{
								zval *orig_ptr = r10;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r10);
									*r10 = *orig_ptr;
									zval_copy_ctor(r10);
									Z_SET_REFCOUNT_P(r10, 1);
									Z_UNSET_ISREF_P(r10);
								}
							}
						} else {
							FREE_ZVAL(r10);
							PHALCON_ALLOC_ZVAL(r10);
						}
					}
					if (!r11) {
						PHALCON_ALLOC_ZVAL(r11);
					} else {
						if (Z_REFCOUNT_P(r11) > 1) {
							{
								zval *orig_ptr = r11;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r11);
									*r11 = *orig_ptr;
									zval_copy_ctor(r11);
									Z_SET_REFCOUNT_P(r11, 1);
									Z_UNSET_ISREF_P(r11);
								}
							}
						} else {
							FREE_ZVAL(r11);
							PHALCON_ALLOC_ZVAL(r11);
						}
					}
					phalcon_array_fetch(r11, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r11);
					p5[0] = r11;
					PHALCON_CALL_FUNC_PARAMS(r10, "addslashes", 1, p5);
					if (!r12) {
						PHALCON_ALLOC_ZVAL(r12);
					} else {
						if (Z_REFCOUNT_P(r12) > 1) {
							{
								zval *orig_ptr = r12;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r12);
									*r12 = *orig_ptr;
									zval_copy_ctor(r12);
									Z_SET_REFCOUNT_P(r12, 1);
									Z_UNSET_ISREF_P(r12);
								}
							}
						} else {
							FREE_ZVAL(r12);
							PHALCON_ALLOC_ZVAL(r12);
						}
					}
					phalcon_concat_both(r12,  "'", r10, "'" TSRMLS_CC);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, r12, 1, 0);
						Z_SET_REFCOUNT_P(copy, 0);
						{
							zval *orig_ptr = v2;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								ALLOC_ZVAL(v2);
								*v2 = *orig_ptr;
								zval_copy_ctor(v2);
								Z_SET_REFCOUNT_P(v2, 1);
								Z_UNSET_ISREF_P(v2);
							}
						}
						phalcon_array_update(v2, v6, copy TSRMLS_CC);
					}
				}
			} else {
				if (!r13) {
					PHALCON_ALLOC_ZVAL(r13);
				} else {
					if (Z_REFCOUNT_P(r13) > 1) {
						{
							zval *orig_ptr = r13;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r13);
								*r13 = *orig_ptr;
								zval_copy_ctor(r13);
								Z_SET_REFCOUNT_P(r13, 1);
								Z_UNSET_ISREF_P(r13);
							}
						}
					} else {
						FREE_ZVAL(r13);
						PHALCON_ALLOC_ZVAL(r13);
					}
				}
				if (!r14) {
					PHALCON_ALLOC_ZVAL(r14);
				} else {
					if (Z_REFCOUNT_P(r14) > 1) {
						{
							zval *orig_ptr = r14;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r14);
								*r14 = *orig_ptr;
								zval_copy_ctor(r14);
								Z_SET_REFCOUNT_P(r14, 1);
								Z_UNSET_ISREF_P(r14);
							}
						}
					} else {
						FREE_ZVAL(r14);
						PHALCON_ALLOC_ZVAL(r14);
					}
				}
				phalcon_array_fetch(r14, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
				Z_ADDREF_P(r14);
				p6[0] = r14;
				PHALCON_CALL_FUNC_PARAMS(r13, "addslashes", 1, p6);
				if (!r15) {
					PHALCON_ALLOC_ZVAL(r15);
				} else {
					if (Z_REFCOUNT_P(r15) > 1) {
						{
							zval *orig_ptr = r15;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r15);
								*r15 = *orig_ptr;
								zval_copy_ctor(r15);
								Z_SET_REFCOUNT_P(r15, 1);
								Z_UNSET_ISREF_P(r15);
							}
						}
					} else {
						FREE_ZVAL(r15);
						PHALCON_ALLOC_ZVAL(r15);
					}
				}
				phalcon_concat_both(r15,  "'", r13, "'" TSRMLS_CC);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, r15, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					{
						zval *orig_ptr = v2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							ALLOC_ZVAL(v2);
							*v2 = *orig_ptr;
							zval_copy_ctor(v2);
							Z_SET_REFCOUNT_P(v2, 1);
							Z_UNSET_ISREF_P(v2);
						}
					}
					phalcon_array_update(v2, v6, copy TSRMLS_CC);
				}
			}
		}
		if (!r17) {
			PHALCON_ALLOC_ZVAL(r17);
		} else {
			if (Z_REFCOUNT_P(r17) > 1) {
				{
					zval *orig_ptr = r17;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r17);
						*r17 = *orig_ptr;
						zval_copy_ctor(r17);
						Z_SET_REFCOUNT_P(r17, 1);
						Z_UNSET_ISREF_P(r17);
					}
				}
			} else {
				FREE_ZVAL(r17);
				PHALCON_ALLOC_ZVAL(r17);
			}
		}
		phalcon_array_fetch(r17, v2, v6, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (!r16) {
			PHALCON_ALLOC_ZVAL(r16);
		} else {
			if (Z_REFCOUNT_P(r16) > 1) {
				{
					zval *orig_ptr = r16;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r16);
						*r16 = *orig_ptr;
						zval_copy_ctor(r16);
						Z_SET_REFCOUNT_P(r16, 1);
						Z_UNSET_ISREF_P(r16);
					}
				}
			} else {
				FREE_ZVAL(r16);
				PHALCON_ALLOC_ZVAL(r16);
			}
		}
		phalcon_concat_vboth(r16, v8, " = ", r17 TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r16, 1, 0);
			Z_SET_REFCOUNT_P(copy, 1);
			{
				zval *orig_ptr = v7;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v7);
					*v7 = *orig_ptr;
					zval_copy_ctor(v7);
					Z_SET_REFCOUNT_P(v7, 1);
					Z_UNSET_ISREF_P(v7);
				}
			}
			phalcon_array_append(v7, copy TSRMLS_CC);
		}
		{
			zval *orig_ptr = v6;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(v6);
				*v6 = *orig_ptr;
				zval_copy_ctor(v6);
				Z_SET_REFCOUNT_P(v6, 1);
				Z_UNSET_ISREF_P(v6);
			}
		}
		increment_function(v6);
	END_FOREACH(ac0, fes14, fee14, ah0, hp0);
	PHALCON_ALLOC_ZVAL(r18);
	PHALCON_ALLOC_ZVAL(p7[0]);
	ZVAL_STRING(p7[0], ", ", 1);
	p7[1] = v7;
	PHALCON_CALL_FUNC_PARAMS(r18, "join", 2, p7);
	PHALCON_CONCAT_FUNCTION(r19, v5, r18);
	if (v5) {
		Z_DELREF_P(v5);
		if (!Z_REFCOUNT_P(v5)) {
			FREE_ZVAL(v5);
		}
	}
	Z_ADDREF_P(r19);
	v5 = r19;
	PHALCON_INIT_NULL(t0);
	PHALCON_NOT_EQUAL_FUNCTION(r20, v3, t0);
	if (zend_is_true(r20)) {
		PHALCON_ALLOC_ZVAL(r21);
		phalcon_concat_left(r21, " WHERE ", v3 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r22, v5, r21);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r22);
		v5 = r22;
	}
	PHALCON_ALLOC_ZVAL(r23);
	Z_ADDREF_P(v5);
	p8[0] = v5;
	PHALCON_CALL_METHOD_PARAMS(r23, this_ptr, "query", 1, p8, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r23, 1, 0);
}

/**
 * Deletes data from a table using custom RBDM SQL syntax
 *
 * 
 *
 * @param string $tables
 * @param string $whereCondition
 * @return boolean
 */
PHP_METHOD(Phalcon_Db, delete){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r0, "trim", 1, p0);
	if (!phalcon_compare_strict_string(r0, "")) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_left(r3, "DELETE FROM ", v0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_vboth(r2, r3, " WHERE ", v1 TSRMLS_CC);
		Z_ADDREF_P(r2);
		p1[0] = r2;
		PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "query", 1, p1, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r1, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(r4);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_left(r5, "DELETE FROM ", v0 TSRMLS_CC);
		Z_ADDREF_P(r5);
		p2[0] = r5;
		PHALCON_CALL_METHOD_PARAMS(r4, this_ptr, "query", 1, p2, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r4, 1, 0);
	}
	RETURN_NULL();
}

/**
     * Starts a transaction in the connection
     *
     * @return boolean
     */
PHP_METHOD(Phalcon_Db, begin){

	zval *r0 = NULL;
	zval *p0[] = { NULL };

	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_autoCommit", strlen("_autoCommit"), 0 TSRMLS_CC);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_underTransaction", strlen("_underTransaction"), 1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "BEGIN", 1);
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "query", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
     * Rollbacks the active transaction in the connection
     *
     * @return boolean
     */
PHP_METHOD(Phalcon_Db, rollback){

	zval *t0 = NULL;
	zval *r0 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL, NULL, NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_underTransaction", sizeof("_underTransaction")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_underTransaction", strlen("_underTransaction"), 0 TSRMLS_CC);
		zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_autoCommit", strlen("_autoCommit"), 1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "ROLLBACK", 1);
		PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "query", 1, p0, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r0, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "There is not an active transaction on relational manager", 1);
		PHALCON_PARAM_LONG(p1[1], 0);
		PHALCON_PARAM_BOOL(p1[2], 1);
		Z_ADDREF_P(this_ptr);
		p1[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
     * Commits the active transaction in the connection
     *
     * @return boolean
     */
PHP_METHOD(Phalcon_Db, commit){

	zval *t0 = NULL;
	zval *r0 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL, NULL, NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_underTransaction", sizeof("_underTransaction")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_underTransaction", strlen("_underTransaction"), 0 TSRMLS_CC);
		zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_autoCommit", strlen("_autoCommit"), 1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "COMMIT", 1);
		PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "query", 1, p0, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r0, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "There is not an active transaction on relational manager", 1);
		PHALCON_PARAM_LONG(p1[1], 0);
		PHALCON_PARAM_BOOL(p1[2], 1);
		Z_ADDREF_P(this_ptr);
		p1[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
 * Manually sets a "under transaction" state for the connection
 *
 * @param boolean $underTransaction
 */
PHP_METHOD(Phalcon_Db, setUnderTransaction){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_underTransaction", strlen("_underTransaction"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Checks whether connection is under database transaction
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Db, isUnderTransaction){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_underTransaction", sizeof("_underTransaction")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Checks whether connection have auto commit
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Db, getHaveAutoCommit){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_autoCommit", sizeof("_autoCommit")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns database name in the internal connection
 *
 * @return string
 */
PHP_METHOD(Phalcon_Db, getDatabaseName){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	int eval_int;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	eval_int = phalcon_isset_property(t0, "name", strlen("name") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, t1, "name", sizeof("name")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		if (Z_TYPE_P(t2) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Returns active schema name in the internal connection
 *
 * @return string
 */
PHP_METHOD(Phalcon_Db, getDefaultSchema){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL;
	int eval_int;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	eval_int = phalcon_isset_property(t0, "schema", strlen("schema") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, t1, "schema", sizeof("schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		if (Z_TYPE_P(t2) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		eval_int = phalcon_isset_property(t3, "name", strlen("name") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t4);
			phalcon_read_property(t4, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t4);
			PHALCON_ALLOC_ZVAL(t5);
			phalcon_read_property(t5, t4, "name", sizeof("name")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t5);
			if (Z_TYPE_P(t5) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(t5);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(t5);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		} else {
			RETURN_STRING("", 1);
		}
	}
	RETURN_NULL();
}

/**
 * Returns the username which has connected to the database
 *
 * @return string
 */
PHP_METHOD(Phalcon_Db, getUsername){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	int eval_int;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	eval_int = phalcon_isset_property(t0, "username", strlen("username") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, t1, "username", sizeof("username")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		if (Z_TYPE_P(t2) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Returns the username which has connected to the database
     *
 * @return string
 */
PHP_METHOD(Phalcon_Db, getHostName){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	int eval_int;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	eval_int = phalcon_isset_property(t0, "host", strlen("host") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, t1, "host", sizeof("host")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		if (Z_TYPE_P(t2) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_STRING("", 1);
	}
	RETURN_NULL();
}

/**
 * Gets a active connection unique identifier
 *
 * @return string
 */
PHP_METHOD(Phalcon_Db, getConnectionId){

	zval *v0 = NULL;
	zval *t0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_BOOL(v0, 0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Instantiates Php_Db adapter using given parameters
 *
 * @param string $adapterName
 * @param stdClass $options
 * @param boolean $persistent
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Db, factory){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL;
	zval *i0 = NULL, *i1 = NULL, *i2 = NULL, *i3 = NULL, *i4 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *p0[] = { NULL, NULL, NULL, NULL }, *p1[] = { NULL, NULL, NULL, NULL }, *p2[] = { NULL }, *p3[] = { NULL, NULL }, *p4[] = { NULL, NULL, NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;
	zend_class_entry *ce0;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &v0, &v1, &v2) == FAILURE) {
		RETURN_NULL();
	}

	if (!v2) {
		PHALCON_INIT_BOOL(v2, 0);
	}
	
	if (phalcon_compare_strict_string(v0, "")) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "A valid adapter name is required", 1);
		PHALCON_PARAM_LONG(p0[1], 0);
		PHALCON_PARAM_BOOL(p0[2], 1);
		PHALCON_PARAM_NULL(p0[3]);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p0, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	if (Z_TYPE_P(v1) != IS_ARRAY) { 
		if (Z_TYPE_P(v1) != IS_OBJECT) {
			PHALCON_ALLOC_ZVAL(i1);
			object_init_ex(i1, phalcon_db_exception_class_entry);
			PHALCON_ALLOC_ZVAL(p1[0]);
			ZVAL_STRING(p1[0], "The parameter 'options' must be an Array or Object", 1);
			PHALCON_PARAM_LONG(p1[1], 0);
			PHALCON_PARAM_BOOL(p1[2], 1);
			PHALCON_PARAM_NULL(p1[3]);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 4, p1, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i1 TSRMLS_CC);
			Z_ADDREF_P(i1);
			return;
		}
	}
	if (Z_TYPE_P(v1) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(i2);
		object_init(i2);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(i2);
		v3 = i2;
		FOREACH_KV(v1, ac0, fes15, fee15, ah0, hp0, v5, v4)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v4, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(v3, Z_STRVAL_P(v5), Z_STRLEN_P(v5), copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes15, fee15, ah0, hp0);
	} else {
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(v1);
		v3 = v1;
	}
	eval_int = phalcon_isset_property(v3, "layer", strlen("layer") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t0);
		phalcon_read_property(t0, v3, "layer", sizeof("layer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t0);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(t0);
		v6 = t0;
	} else {
		PHALCON_ALLOC_ZVAL(v6);
		ZVAL_STRING(v6, "native", 0);
	}
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_concat_left(r0, "Phalcon_Db_", v0 TSRMLS_CC);
	if (v7) {
		Z_DELREF_P(v7);
		if (!Z_REFCOUNT_P(v7)) {
			FREE_ZVAL(v7);
		}
	}
	Z_ADDREF_P(r0);
	v7 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	Z_ADDREF_P(v7);
	p2[0] = v7;
	PHALCON_CALL_FUNC_PARAMS(r1, "class_exists", 1, p2);
	if (zend_is_true(r1)) {
		ce0 = zend_fetch_class(Z_STRVAL_P(v7), Z_STRLEN_P(v7), ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(i3);
		object_init_ex(i3, ce0);
		Z_ADDREF_P(v3);
		p3[0] = v3;
		Z_ADDREF_P(v2);
		p3[1] = v2;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i3, "__construct", 2, p3, PHALCON_CALL_CHECK);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(i3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(i4);
		object_init_ex(i4, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_both(r2,  "Database adapter class ", v7, " was not found" TSRMLS_CC);
		Z_ADDREF_P(r2);
		p4[0] = r2;
		PHALCON_PARAM_LONG(p4[1], 0);
		PHALCON_PARAM_BOOL(p4[2], 1);
		PHALCON_PARAM_NULL(p4[3]);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i4, "__construct", 4, p4, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i4 TSRMLS_CC);
		Z_ADDREF_P(i4);
		return;
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Tag_Exception, __construct){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Tag_Exception", "__construct", 1, p0);
	RETURN_NULL();
}

/**
 * Php_Router_Rewrite
 *
 * <p>Php_Router_Rewrite is the standard framework router. Routing is the
 * process of taking a URI endpoint (that part of the URI which comes after the base URL) and
 * decomposing it into parameters to determine which module, controller, and
 * action of that controller should receive the request</p>
 *
 *Rewrite rules using a single document root:
 *
 *
 *Rewrite rules using a hidden directory and a public/ document root:
 *
 *
 * On public/.htaccess:
 *
 *
 *
 * The component can be used as follows:
 *
 *
 */

PHP_METHOD(Phalcon_Router_Rewrite, __construct){

	zval *a0 = NULL;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_property(phalcon_router_rewrite_class_entry, this_ptr, "_params", strlen("_params"), a0 TSRMLS_CC);
}

/**
 * Handles routing information received from the rewrite engine
 */
PHP_METHOD(Phalcon_Router_Rewrite, handle){

	zval *a0 = NULL, *a1 = NULL;
	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval **gv0;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL };
	int eval_int;

	phalcon_init_global("_GET" TSRMLS_CC);
	if (&EG(symbol_table)) {
		if( zend_hash_find(&EG(symbol_table), "_GET", sizeof("_GET"), (void **) &gv0) == SUCCESS) {
			if(Z_TYPE_PP(gv0)==IS_ARRAY){
				a0 = *gv0;
			} else {
				PHALCON_INIT_ARRAY(a0);
			}
		}
	} else {
		PHALCON_INIT_ARRAY(a0);
	}
	eval_int = phalcon_array_isset_string(a0, "_url", strlen("_url")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "/", 1);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_string(r1, a0, "_url", strlen("_url"), PHALCON_NOISY_FETCH TSRMLS_CC);
		p0[1] = r1;
		PHALCON_CALL_FUNC_PARAMS(r0, "explode", 2, p0);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(r0);
		v0 = r0;
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_array_fetch_long(r2, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			p1[0] = r2;
			phalcon_filter_alphanum(&r3, p1[0]);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r3, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_controller", strlen("_controller"), copy TSRMLS_CC);
			}
			PHALCON_ALLOC_ZVAL(t0);
			phalcon_read_property(t0, this_ptr, "_controller", sizeof("_controller")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t0);
			if (!zend_is_true(t0)) {
				zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_controller", strlen("_controller") TSRMLS_CC);
			}
		} else {
			zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_controller", strlen("_controller") TSRMLS_CC);
		}
		eval_int = phalcon_array_isset_long(v0, 1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r4);
			phalcon_array_fetch_long(r4, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
			p2[0] = r4;
			phalcon_filter_alphanum(&r5, p2[0]);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r5, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_action", strlen("_action"), copy TSRMLS_CC);
			}
			PHALCON_ALLOC_ZVAL(t1);
			phalcon_read_property(t1, this_ptr, "_action", sizeof("_action")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t1);
			if (!zend_is_true(t1)) {
				zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_action", strlen("_action") TSRMLS_CC);
			}
		} else {
			zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_action", strlen("_action") TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(a1);
		v1 = a1;
		PHALCON_ALLOC_ZVAL(r6);
		p3[0] = v0;
		PHALCON_CALL_FUNC_PARAMS(r6, "count", 1, p3);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r6);
		v2 = r6;
		PHALCON_ALLOC_ZVAL(v3);
		ZVAL_LONG(v3, 2);
		fs16:
		PHALCON_SMALLER_FUNCTION(r7, v3, v2);
		if (!zend_is_true(r7)) {
			goto fe16;
		}
		if (!r8) {
			PHALCON_ALLOC_ZVAL(r8);
		} else {
			if (Z_REFCOUNT_P(r8) > 1) {
				{
					zval *orig_ptr = r8;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r8);
						*r8 = *orig_ptr;
						zval_copy_ctor(r8);
						Z_SET_REFCOUNT_P(r8, 1);
						Z_UNSET_ISREF_P(r8);
					}
				}
			} else {
				FREE_ZVAL(r8);
				PHALCON_ALLOC_ZVAL(r8);
			}
		}
		phalcon_array_fetch(r8, v0, v3, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r8, 1, 0);
			Z_SET_REFCOUNT_P(copy, 1);
			{
				zval *orig_ptr = v1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v1);
					*v1 = *orig_ptr;
					zval_copy_ctor(v1);
					Z_SET_REFCOUNT_P(v1, 1);
					Z_UNSET_ISREF_P(v1);
				}
			}
			phalcon_array_append(v1, copy TSRMLS_CC);
		}
		{
			zval *orig_ptr = v3;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(v3);
				*v3 = *orig_ptr;
				zval_copy_ctor(v3);
				Z_SET_REFCOUNT_P(v3, 1);
				Z_UNSET_ISREF_P(v3);
			}
		}
		increment_function(v3);
		goto fs16;
		fe16:
		r7 = NULL;
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_params", strlen("_params"), copy TSRMLS_CC);
		}
	} else {
		zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_controller", strlen("_controller") TSRMLS_CC);
		zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_action", strlen("_action") TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns proccesed controller name
 *
 * @return string
 */
PHP_METHOD(Phalcon_Router_Rewrite, getControllerName){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_controller", sizeof("_controller")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns proccesed action name
 *
 * @return string
 */
PHP_METHOD(Phalcon_Router_Rewrite, getActionName){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_action", sizeof("_action")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns proccesed extra params
 *
 * @return array
 */
PHP_METHOD(Phalcon_Router_Rewrite, getParams){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_params", sizeof("_params")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Db_Exception, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *p0[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|zz", &v0, &v1, &v2, &v3) == FAILURE) {
		RETURN_NULL();
	}

	if (!v2) {
		PHALCON_INIT_BOOL(v2, 1);
	}
	if (!v3) {
		PHALCON_INIT_NULL(v3);
	}
	
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	Z_ADDREF_P(v2);
	p0[2] = v2;
	PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Db_Exception", "__construct", 3, p0);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, __construct){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_INIT_BOOL(v1, 0);
	}
	
	if (phalcon_compare_strict_string(v0, "")) {
		PHALCON_ALLOC_ZVAL(t0);
		phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t0);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t0);
		v0 = t0;
	}
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "connect", 2, p0, PHALCON_CALL_DEFAULT);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, connect){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL, *t11 = NULL, *t12 = NULL, *t13 = NULL;
	zval *t14 = NULL, *t15 = NULL, *t16 = NULL, *t17 = NULL, *t18 = NULL, *t19 = NULL, *t20 = NULL;
	zval *t21 = NULL, *t22 = NULL, *t23 = NULL, *t24 = NULL, *t25 = NULL, *t26 = NULL, *t27 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *p0[] = { NULL, NULL, NULL, NULL, NULL }, *p1[] = { NULL, NULL, NULL, NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL, NULL, NULL }, *p6[] = { NULL }, *p7[] = { NULL }, *p8[] = { NULL }, *p9[] = { NULL }, *p10[] = { NULL, NULL, NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_INIT_BOOL(v1, 0);
	}
	
	if (phalcon_compare_strict_string(v0, "")) {
		PHALCON_ALLOC_ZVAL(t0);
		phalcon_read_property(t0, this_ptr, "_descriptor", sizeof("_descriptor")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t0);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t0);
		v0 = t0;
	}
	eval_int = phalcon_isset_property(v0, "host", strlen("host") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, v0, "host", sizeof("host")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		r0 = t1;
	} else {
		PHALCON_ALLOC_ZVAL(t2);
		ZVAL_STRING(t2, "", 1);
		r0 = t2;
	}
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	eval_int = phalcon_isset_property(v0, "username", strlen("username") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, v0, "username", sizeof("username")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		r1 = t3;
	} else {
		PHALCON_ALLOC_ZVAL(t4);
		ZVAL_STRING(t4, "", 1);
		r1 = t4;
	}
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r1);
	v3 = r1;
	eval_int = phalcon_isset_property(v0, "password", strlen("password") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t5);
		phalcon_read_property(t5, v0, "password", sizeof("password")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t5);
		r2 = t5;
	} else {
		PHALCON_ALLOC_ZVAL(t6);
		ZVAL_STRING(t6, "", 1);
		r2 = t6;
	}
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r2);
	v4 = r2;
	eval_int = phalcon_isset_property(v0, "port", strlen("port") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t7);
		phalcon_read_property(t7, v0, "port", sizeof("port")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t7);
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_vboth(r3, v2, ":", t7 TSRMLS_CC);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r3);
		v5 = r3;
	} else {
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(v2);
		v5 = v2;
	}
	PHALCON_INIT_NULL(v6);
	eval_int = phalcon_isset_property(v0, "compression", strlen("compression") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t8);
		phalcon_read_property(t8, v0, "compression", sizeof("compression")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t8);
		if (zend_is_true(t8)) {
			if (Z_TYPE_P(v6) == IS_NULL) {
				//Wasted infer Constant MYSQL_CLIENT_COMPRESS
				PHALCON_ALLOC_ZVAL(t9);
				ZVAL_LONG(t9, 32);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(t9);
				v6 = t9;
			} else {
				PHALCON_ALLOC_ZVAL(t10);
				ZVAL_LONG(t10, 32);
				PHALCON_BITWISE_OR_FUNCTION(r4, v6, t10);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r4);
				v6 = r4;
			}
		}
	}
	eval_int = phalcon_isset_property(v0, "ssl", strlen("ssl") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t11);
		phalcon_read_property(t11, v0, "ssl", sizeof("ssl")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t11);
		if (zend_is_true(t11)) {
			if (Z_TYPE_P(v6) == IS_NULL) {
				//Wasted infer Constant MYSQL_CLIENT_SSL
				PHALCON_ALLOC_ZVAL(t12);
				ZVAL_LONG(t12, 2048);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(t12);
				v6 = t12;
			} else {
				PHALCON_ALLOC_ZVAL(t13);
				ZVAL_LONG(t13, 2048);
				PHALCON_BITWISE_OR_FUNCTION(r5, v6, t13);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r5);
				v6 = r5;
			}
		}
	}
	eval_int = phalcon_isset_property(v0, "interactive", strlen("interactive") TSRMLS_CC);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t14);
		phalcon_read_property(t14, v0, "interactive", sizeof("interactive")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t14);
		if (zend_is_true(t14)) {
			if (Z_TYPE_P(v6) == IS_NULL) {
				//Wasted infer Constant MYSQL_CLIENT_INTERACTIVE
				PHALCON_ALLOC_ZVAL(t15);
				ZVAL_LONG(t15, 1024);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(t15);
				v6 = t15;
			} else {
				PHALCON_ALLOC_ZVAL(t16);
				ZVAL_LONG(t16, 1024);
				PHALCON_BITWISE_OR_FUNCTION(r6, v6, t16);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r6);
				v6 = r6;
			}
		}
	}
	if (!zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r7);
		p0[0] = v5;
		p0[1] = v3;
		p0[2] = v4;
		PHALCON_PARAM_BOOL(p0[3], 1);
		p0[4] = v6;
		PHALCON_CALL_FUNC_PARAMS(r7, "mysql_connect", 5, p0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r7, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_idConnection", strlen("_idConnection"), copy TSRMLS_CC);
		}
	} else {
		PHALCON_ALLOC_ZVAL(r8);
		p1[0] = v5;
		p1[1] = v3;
		p1[2] = v4;
		p1[3] = v6;
		PHALCON_CALL_FUNC_PARAMS(r8, "mysql_pconnect", 4, p1);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r8, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_idConnection", strlen("_idConnection"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t17);
	phalcon_read_property(t17, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t17);
	if (zend_is_true(t17)) {
		eval_int = phalcon_isset_property(v0, "name", strlen("name") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t18);
			phalcon_read_property(t18, v0, "name", sizeof("name")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t18);
			r9 = t18;
		} else {
			PHALCON_ALLOC_ZVAL(t19);
			ZVAL_STRING(t19, "", 1);
			r9 = t19;
		}
		if (v7) {
			Z_DELREF_P(v7);
			if (!Z_REFCOUNT_P(v7)) {
				FREE_ZVAL(v7);
			}
		}
		Z_ADDREF_P(r9);
		v7 = r9;
		PHALCON_ALLOC_ZVAL(t20);
		ZVAL_STRING(t20, "", 1);
		PHALCON_NOT_IDENTICAL_FUNCTION(r10, v7, t20);
		if (zend_is_true(r10)) {
			PHALCON_ALLOC_ZVAL(r11);
			p2[0] = v7;
			PHALCON_ALLOC_ZVAL(t21);
			phalcon_read_property(t21, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t21);
			p2[1] = t21;
			PHALCON_CALL_FUNC_PARAMS(r11, "mysql_select_db", 2, p2);
			if (!zend_is_true(r11)) {
				PHALCON_ALLOC_ZVAL(i0);
				object_init_ex(i0, phalcon_db_exception_class_entry);
				PHALCON_ALLOC_ZVAL(r12);
				PHALCON_CALL_METHOD(r12, this_ptr, "error", PHALCON_CALL_DEFAULT);
				Z_ADDREF_P(r12);
				p3[0] = r12;
				PHALCON_ALLOC_ZVAL(r13);
				PHALCON_CALL_METHOD(r13, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
				Z_ADDREF_P(r13);
				p3[1] = r13;
				PHALCON_PARAM_BOOL(p3[2], 0);
				PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 3, p3, PHALCON_CALL_CHECK);
				zend_throw_exception_object(i0 TSRMLS_CC);
				Z_ADDREF_P(i0);
				return;
			}
		}
		eval_int = phalcon_isset_property(v0, "autocommit", strlen("autocommit") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t22);
			phalcon_read_property(t22, v0, "autocommit", sizeof("autocommit")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t22);
			r14 = t22;
		} else {
			PHALCON_INIT_FALSE(t23);
			r14 = t23;
		}
		if (v8) {
			Z_DELREF_P(v8);
			if (!Z_REFCOUNT_P(v8)) {
				FREE_ZVAL(v8);
			}
		}
		Z_ADDREF_P(r14);
		v8 = r14;
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v8, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_autoCommit", strlen("_autoCommit"), copy TSRMLS_CC);
		}
		//Wasted infer Constant MYSQL_BOTH
		PHALCON_ALLOC_ZVAL(t24);
		ZVAL_LONG(t24, 3);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t24, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
		}
		Z_ADDREF_P(v0);
		p6[0] = v0;
		PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Db_Mysql", "__construct", 1, p6);
		eval_int = phalcon_isset_property(v0, "charset", strlen("charset") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t25);
			phalcon_read_property(t25, v0, "charset", sizeof("charset")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t25);
			PHALCON_ALLOC_ZVAL(r15);
			phalcon_concat_left(r15, "SET NAMES ", t25 TSRMLS_CC);
			Z_ADDREF_P(r15);
			p7[0] = r15;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "query", 1, p7, PHALCON_CALL_DEFAULT);
		}
		eval_int = phalcon_isset_property(v0, "collation", strlen("collation") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t26);
			phalcon_read_property(t26, v0, "collation", sizeof("collation")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t26);
			PHALCON_ALLOC_ZVAL(r16);
			phalcon_concat_left(r16, "SET collation_connection=", t26 TSRMLS_CC);
			Z_ADDREF_P(r16);
			p8[0] = r16;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "query", 1, p8, PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(t27);
			phalcon_read_property(t27, v0, "collation", sizeof("collation")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t27);
			PHALCON_ALLOC_ZVAL(r17);
			phalcon_concat_left(r17, "SET collation_database=", t27 TSRMLS_CC);
			Z_ADDREF_P(r17);
			p9[0] = r17;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "query", 1, p9, PHALCON_CALL_DEFAULT);
		}
		RETURN_TRUE;
	} else {
		PHALCON_ALLOC_ZVAL(i1);
		object_init_ex(i1, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r18);
		PHALCON_CALL_METHOD(r18, this_ptr, "error", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r18);
		p10[0] = r18;
		PHALCON_ALLOC_ZVAL(r19);
		PHALCON_CALL_METHOD(r19, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r19);
		p10[1] = r19;
		PHALCON_PARAM_BOOL(p10[2], 0);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 3, p10, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i1 TSRMLS_CC);
		Z_ADDREF_P(i1);
		return;
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, query){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL, NULL }, *p3[] = { NULL }, *p2[] = { NULL }, *p5[] = { NULL, NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "connect", PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		if (!zend_is_true(t1)) {
			RETURN_FALSE;
		}
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_lastQuery", strlen("_lastQuery"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r0);
	p1[0] = v0;
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	p1[1] = t2;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_query", 2, p1);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	if (zend_is_true(v1)) {
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_lastResultQuery", strlen("_lastResultQuery"), copy TSRMLS_CC);
		}
		if (Z_TYPE_P(v1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_lastResultQuery", strlen("_lastResultQuery"), 0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_left(r3, " when executing \"", v0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r4);
		PHALCON_PARAM_BOOL(p3[0], 1);
		PHALCON_CALL_METHOD_PARAMS(r4, this_ptr, "getconnectionid", 1, p3, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_vboth(r2, r3, "\" on connection \"", r4 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_right(r5, r2, "\"" TSRMLS_CC);
		Z_ADDREF_P(r5);
		p2[0] = r5;
		PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "error", 1, p2, PHALCON_CALL_DEFAULT);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r1);
		v2 = r1;
		PHALCON_ALLOC_ZVAL(r6);
		PHALCON_CALL_METHOD(r6, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r6);
		v3 = r6;
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		Z_ADDREF_P(v2);
		p5[0] = v2;
		PHALCON_ALLOC_ZVAL(r7);
		PHALCON_CALL_METHOD(r7, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r7);
		p5[1] = r7;
		PHALCON_PARAM_BOOL(p5[2], 1);
		Z_ADDREF_P(this_ptr);
		p5[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p5, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, close){

	zval *t0 = NULL, *t1 = NULL;
	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *p1[] = { NULL };
	zend_bool silence;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_CALL_PARENT_NORETURN(this_ptr, "Phalcon_Db_Mysql", "close");
		silence = PG(display_errors);
		PG(display_errors) = 0;
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		Z_ADDREF_P(t1);
		p1[0] = t1;
		PHALCON_CALL_FUNC_PARAMS(r0, "mysql_close", 1, p1);
		PG(display_errors) = silence;
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(r0);
		v0 = r0;
		zend_update_property_null(Z_OBJCE_P(this_ptr), this_ptr, "_idConnection", strlen("_idConnection") TSRMLS_CC);
		if (Z_TYPE_P(v0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		RETURN_TRUE;
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, fetchArray){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		RETURN_FALSE;
	}
	if (!zend_is_true(v0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_lastResultQuery", sizeof("_lastResultQuery")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t1);
		v0 = t1;
		if (!zend_is_true(v0)) {
			RETURN_FALSE;
		}
	}
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_fetchMode", sizeof("_fetchMode")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	Z_ADDREF_P(t2);
	p0[1] = t2;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_fetch_array", 2, p0);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Db_Mysql, numRows){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *i0 = NULL;
	zval **pv0;
	zval *p0[] = { NULL }, *p2[] = { NULL }, *p1[] = { NULL, NULL, NULL, NULL };
	zend_bool silence;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		RETURN_FALSE;
	}
	if (!zend_is_true(v0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_lastResultQuery", sizeof("_lastResultQuery")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t1);
		v0 = t1;
		if (!zend_is_true(v0)) {
			RETURN_FALSE;
		}
	}
	silence = PG(display_errors);
	PG(display_errors) = 0;
	PHALCON_ALLOC_ZVAL(r0);
	p0[0] = v0;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_num_rows", 1, p0);
	PG(display_errors) = silence;
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_INIT_FALSE(t2);
	PHALCON_NOT_IDENTICAL_FUNCTION(r1, v1, t2);
	if (zend_is_true(r1)) {
		if (Z_TYPE_P(v1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_READ_SYMBOL(v2, pv0, "php_errormsg");
		Z_ADDREF_P(v2);
		p2[0] = v2;
		PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "error", 1, p2, PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r2);
		p1[0] = r2;
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_METHOD(r3, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r3);
		p1[1] = r3;
		PHALCON_PARAM_BOOL(p1[2], 1);
		Z_ADDREF_P(this_ptr);
		p1[3] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(Phalcon_Db_Mysql, dataSeek){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *i0 = NULL;
	zval **pv0;
	zval *p0[] = { NULL, NULL }, *p2[] = { NULL }, *p1[] = { NULL, NULL };
	zend_bool silence;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	if (!zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(t0);
		phalcon_read_property(t0, this_ptr, "_lastResultQuery", sizeof("_lastResultQuery")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t0);
		if (v1) {
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(t0);
		v1 = t0;
		if (!zend_is_true(v1)) {
			RETURN_FALSE;
		}
	}
	silence = PG(display_errors);
	PG(display_errors) = 0;
	PHALCON_ALLOC_ZVAL(r0);
	p0[0] = v1;
	p0[1] = v0;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_data_seek", 2, p0);
	PG(display_errors) = silence;
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	PHALCON_INIT_FALSE(t1);
	PHALCON_NOT_IDENTICAL_FUNCTION(r1, v2, t1);
	if (zend_is_true(r1)) {
		if (Z_TYPE_P(v2) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v2);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_READ_SYMBOL(v3, pv0, "php_errormsg");
		Z_ADDREF_P(v3);
		p2[0] = v3;
		PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "error", 1, p2, PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r2);
		p1[0] = r2;
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_METHOD(r3, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r3);
		p1[1] = r3;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 2, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(Phalcon_Db_Mysql, affectedRows){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;
	zval **pv0;
	zval *p0[] = { NULL }, *p2[] = { NULL }, *p1[] = { NULL, NULL };
	zend_bool silence;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	silence = PG(display_errors);
	PG(display_errors) = 0;
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_affected_rows", 1, p0);
	PG(display_errors) = silence;
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_INIT_FALSE(t1);
	PHALCON_NOT_IDENTICAL_FUNCTION(r1, v1, t1);
	if (zend_is_true(r1)) {
		if (Z_TYPE_P(v1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_READ_SYMBOL(v2, pv0, "php_errormsg");
		Z_ADDREF_P(v2);
		p2[0] = v2;
		PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "error", 1, p2, PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r2);
		p1[0] = r2;
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_METHOD(r3, this_ptr, "noerror", PHALCON_CALL_DEFAULT);
		Z_ADDREF_P(r3);
		p1[1] = r3;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 2, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(Phalcon_Db_Mysql, setFetchMode){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_GET_CLASS_CONSTANT(t0, phalcon_db_mysql_class_entry, "DB_ASSOC");
	PHALCON_EQUAL_FUNCTION(r0, v0, t0);
	if (zend_is_true(r0)) {
		//Wasted infer Constant MYSQL_ASSOC
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_LONG(t1, 1);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
		}
	}
	PHALCON_GET_CLASS_CONSTANT(t2, phalcon_db_mysql_class_entry, "DB_BOTH");
	PHALCON_EQUAL_FUNCTION(r1, v0, t2);
	if (zend_is_true(r1)) {
		//Wasted infer Constant MYSQL_BOTH
		PHALCON_ALLOC_ZVAL(t3);
		ZVAL_LONG(t3, 3);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t3, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
		}
	}
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_db_mysql_class_entry, "DB_NUM");
	PHALCON_EQUAL_FUNCTION(r2, v0, t4);
	if (zend_is_true(r2)) {
		//Wasted infer Constant MYSQL_NUM
		PHALCON_ALLOC_ZVAL(t5);
		ZVAL_LONG(t5, 2);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t5, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, error){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_FUNC(r0, "mysql_error");
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		p1[0] = t1;
		PHALCON_CALL_FUNC_PARAMS(r1, "mysql_error", 1, p1);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r1);
		v2 = r1;
	}
	if (zend_is_true(v2)) {
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_left(r3, "\"", v2 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_vboth(r2, r3, "\" ", v0 TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_lastError", strlen("_lastError"), copy TSRMLS_CC);
		}
	} else {
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_concat_both(r4,  "[Unknown MySQL error: ", v0, "]" TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r4, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_lastError", strlen("_lastError"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_lastError", sizeof("_lastError")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	if (Z_TYPE_P(t2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Db_Mysql, noError){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_NULL(v0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		RETURN_FALSE;
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	Z_ADDREF_P(t1);
	p0[0] = t1;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_errno", 1, p0);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Db_Mysql, lastInsertId){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzz", &v0, &v1, &v2) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	if (!v2) {
		PHALCON_ALLOC_ZVAL(v2);
		ZVAL_STRING(v2, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		RETURN_FALSE;
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_idConnection", sizeof("_idConnection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	p0[0] = t1;
	PHALCON_CALL_FUNC_PARAMS(r0, "mysql_insert_id", 1, p0);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Db_Mysql, limit){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_numeric", 1, p0);
	if (zend_is_true(r0)) {
		convert_to_long(v1);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_concat_vboth(r1, v0, " LIMIT ", v1 TSRMLS_CC);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	} else {
		if (Z_TYPE_P(v0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Db_Mysql, tableExists){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL }, *p2[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_FUNC_PARAMS(r0, "addslashes", 1, p0);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	Z_ADDREF_P(v0);
	p1[0] = v0;
	Z_ADDREF_P(v1);
	p1[1] = v1;
	PHALCON_CALL_STATIC_PARAMS(r1, "phalcon_db_dialect_mysql", "tableexists", 2, p1);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_fetchMode", sizeof("_fetchMode")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(t0);
	v3 = t0;
	//Wasted infer Constant MYSQL_NUM
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 2);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r2);
	Z_ADDREF_P(v2);
	p2[0] = v2;
	PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "fetchone", 1, p2, PHALCON_CALL_DEFAULT);
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r2);
	v4 = r2;
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v3, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r3);
	phalcon_array_fetch_long(r3, v4, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(r3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Db_Mysql, viewExists){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "tableexists", 2, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Db_Mysql, describeTable){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_db_dialect_mysql", "describetable", 2, p0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	//Wasted infer Constant MYSQL_ASSOC
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_LONG(t0, 1);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r1);
	Z_ADDREF_P(v2);
	p1[0] = v2;
	PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "fetchall", 1, p1, PHALCON_CALL_DEFAULT);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r1);
	v3 = r1;
	//Wasted infer Constant MYSQL_BOTH
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 3);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_fetchMode", strlen("_fetchMode"), copy TSRMLS_CC);
	}
	if (Z_TYPE_P(v3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Db_Mysql, getDateUsingFormat){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "YYYY-MM-DD", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_concat_both(r0,  "'", v0, "'" TSRMLS_CC);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r0);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Php_Db_Pool
 *
 * Manages caching of database connections. With the help of Php_Db_Pool, developers can be sure that no new database
 * connections will made when calling multiple of times Php_Db_Pool::getConnection.
 */

/**
 * Check if a default descriptor has already defined
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Db_Pool, hasDefaultDescriptor){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;

	t0 = zend_read_static_property(phalcon_db_pool_class_entry, "_defaultDescriptor", sizeof("_defaultDescriptor")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_INIT_NULL(t1);
	PHALCON_NOT_IDENTICAL_FUNCTION(r0, t0, t1);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r0);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Sets the default descriptor for database connections.
 *
 *
 *
 * @param array $options
 * @return boolean
 */
PHP_METHOD(Phalcon_Db_Pool, setDefaultDescriptor){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL, NULL, NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		if (Z_TYPE_P(v0) != IS_OBJECT) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_db_exception_class_entry);
			PHALCON_ALLOC_ZVAL(p0[0]);
			ZVAL_STRING(p0[0], "The parameter 'options' must be an Array or Object", 1);
			PHALCON_PARAM_LONG(p0[1], 0);
			PHALCON_PARAM_BOOL(p0[2], 1);
			PHALCON_PARAM_NULL(p0[3]);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p0, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		}
	}
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(i1);
		object_init(i1);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(i1);
		v1 = i1;
		FOREACH_KV(v0, ac0, fes17, fee17, ah0, hp0, v3, v2)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(v1, Z_STRVAL_P(v3), Z_STRLEN_P(v3), copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes17, fee17, ah0, hp0);
	} else {
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(v0);
		v1 = v0;
	}
	zend_update_static_property(phalcon_db_pool_class_entry, "_defaultDescriptor", sizeof("_defaultDescriptor")-1, v1 TSRMLS_CC);
	RETURN_NULL();
}

/**
 * Returns a connection builded with the default descriptor parameters
 *
 * 
 *
 * @param boolean $newConnection
     * @param boolean $renovate
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Db_Pool, getConnection){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *p0[] = { NULL, NULL, NULL, NULL }, *p1[] = { NULL, NULL, NULL, NULL }, *p2[] = { NULL, NULL, NULL }, *p3[] = { NULL, NULL, NULL }, *p4[] = { NULL }, *p5[] = { NULL, NULL, NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_BOOL(v0, 0);
	}
	if (!v1) {
		PHALCON_INIT_BOOL(v1, 0);
	}
	
	t0 = zend_read_static_property(phalcon_db_pool_class_entry, "_defaultDescriptor", sizeof("_defaultDescriptor")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(t0);
	v2 = t0;
	if (!zend_is_true(v2)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], "Default database connection parameters was not defined", 1);
		PHALCON_PARAM_LONG(p0[1], 0);
		PHALCON_PARAM_BOOL(p0[2], 1);
		PHALCON_PARAM_NULL(p0[3]);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 4, p0, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	eval_int = phalcon_isset_property(v2, "adapter", strlen("adapter") TSRMLS_CC);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(i1);
		object_init_ex(i1, phalcon_db_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "A valid adapter name is required", 1);
		PHALCON_PARAM_LONG(p1[1], 0);
		PHALCON_PARAM_BOOL(p1[2], 1);
		PHALCON_PARAM_NULL(p1[3]);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 4, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i1 TSRMLS_CC);
		Z_ADDREF_P(i1);
		return;
	}
	if (zend_is_true(v0)) {
		if (zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(r0);
			PHALCON_ALLOC_ZVAL(t1);
			phalcon_read_property(t1, v2, "adapter", sizeof("adapter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t1);
			Z_ADDREF_P(t1);
			p2[0] = t1;
			Z_ADDREF_P(v2);
			p2[1] = v2;
			PHALCON_PARAM_BOOL(p2[2], 1);
			PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_db", "factory", 3, p2);
			zend_update_static_property(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, r0 TSRMLS_CC);
			t2 = zend_read_static_property(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(t2);
			v3 = t2;
		} else {
			PHALCON_ALLOC_ZVAL(r1);
			PHALCON_ALLOC_ZVAL(t3);
			phalcon_read_property(t3, v2, "adapter", sizeof("adapter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t3);
			Z_ADDREF_P(t3);
			p3[0] = t3;
			Z_ADDREF_P(v2);
			p3[1] = v2;
			PHALCON_PARAM_BOOL(p3[2], 0);
			PHALCON_CALL_STATIC_PARAMS(r1, "phalcon_db", "factory", 3, p3);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(r1);
			v3 = r1;
		}
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		t4 = zend_read_static_property(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		Z_ADDREF_P(t4);
		p4[0] = t4;
		PHALCON_CALL_FUNC_PARAMS(r2, "is_null", 1, p4);
		if (zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(r3);
			PHALCON_ALLOC_ZVAL(t5);
			phalcon_read_property(t5, v2, "adapter", sizeof("adapter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t5);
			Z_ADDREF_P(t5);
			p5[0] = t5;
			Z_ADDREF_P(v2);
			p5[1] = v2;
			PHALCON_PARAM_BOOL(p5[2], 1);
			PHALCON_CALL_STATIC_PARAMS(r3, "phalcon_db", "factory", 3, p5);
			zend_update_static_property(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, r3 TSRMLS_CC);
		}
		t6 = zend_read_static_property(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(t6);
		v3 = t6;
	}
	if (Z_TYPE_P(v3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Db_Dialect_Mysql
 *
 * Generates database specific SQL for the MySQL RBDM
 */

/**
 * Generates SQL checking for the existence of a schema.table
 *
 * 
 *
 * @param string $tableName
 * @param string $schemaName
 * @return string
 */
PHP_METHOD(Phalcon_Db_Dialect_Mysql, tableExists){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	if (phalcon_compare_strict_string(v1, "")) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_concat_both(r0,  "SELECT COUNT(*) FROM `INFORMATION_SCHEMA`.`TABLES` WHERE `TABLE_NAME`='", v0, "'" TSRMLS_CC);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		Z_ADDREF_P(v1);
		p0[0] = v1;
		PHALCON_CALL_FUNC_PARAMS(r1, "addslashes", 1, p0);
		if (v1) {
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r1);
		v1 = r1;
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_left(r3, "SELECT COUNT(*) FROM `INFORMATION_SCHEMA`.`TABLES` WHERE `TABLE_NAME`= '", v0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_vboth(r2, r3, "' AND `TABLE_SCHEMA`='", v1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_concat_right(r4, r2, "'" TSRMLS_CC);
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r4);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
			Z_SET_REFCOUNT_P(return_value, refcount);
		}
		return;
	}
	RETURN_NULL();
}

/**
 * Generates SQL describing a table
 *
 * 
 *
 * @param string $tableName
 * @param string $schemaName
 * @return string
 */
PHP_METHOD(Phalcon_Db_Dialect_Mysql, describeTable){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	if (phalcon_compare_strict_string(v1, "")) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_concat_both(r0,  "DESCRIBE `", v0, "`" TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_left(r2, "DESCRIBE `", v1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_concat_vboth(r1, r2, "`.`", v0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_right(r3, r1, "`" TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r3);
		v2 = r3;
	}
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Php_Db_RawValue
 *
 * This class lets to insert/update raw data without quoting or formating.
 *
 * The next example shows how to use the MySQL now() function as a field value.
 * 
 */

/**
 * Php_Db_RawValue constructor
 *
 * @param string $value
 */
PHP_METHOD(Phalcon_Db_RawValue, __construct){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_value", strlen("_value"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns internal raw value without quoting or formating
 *
 * @return string
 */
PHP_METHOD(Phalcon_Db_RawValue, getValue){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_value", sizeof("_value")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Magic method __toString returns raw value without quoting or formating
 */
PHP_METHOD(Phalcon_Db_RawValue, __toString){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_value", sizeof("_value")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Flash
 *
 * Shows HTML notifications related to diferent circustances. Classes can be stylized using CSS
 */

PHP_METHOD(Phalcon_Flash, _showMessage){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p0[] = { NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v1) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(p0[0]);
		ZVAL_STRING(p0[0], " ", 1);
		p0[1] = v1;
		PHALCON_CALL_FUNC_PARAMS(r0, "join", 2, p0);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
	} else {
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(v1);
		v2 = v1;
	}
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		FOREACH_V(v0, ac0, fes18, fee18, ah0, hp0, v3)
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			phalcon_concat_left(r3, "<div class=\"", v2 TSRMLS_CC);
			if (!r2) {
				PHALCON_ALLOC_ZVAL(r2);
			} else {
				if (Z_REFCOUNT_P(r2) > 1) {
					{
						zval *orig_ptr = r2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r2);
							*r2 = *orig_ptr;
							zval_copy_ctor(r2);
							Z_SET_REFCOUNT_P(r2, 1);
							Z_UNSET_ISREF_P(r2);
						}
					}
				} else {
					FREE_ZVAL(r2);
					PHALCON_ALLOC_ZVAL(r2);
				}
			}
			phalcon_concat_vboth(r2, r3, "\">", v3 TSRMLS_CC);
			PHALCON_GET_CONSTANT(t0, "PHP_EOL");
			if (!r1) {
				PHALCON_ALLOC_ZVAL(r1);
			} else {
				if (Z_REFCOUNT_P(r1) > 1) {
					{
						zval *orig_ptr = r1;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r1);
							*r1 = *orig_ptr;
							zval_copy_ctor(r1);
							Z_SET_REFCOUNT_P(r1, 1);
							Z_UNSET_ISREF_P(r1);
						}
					}
				} else {
					FREE_ZVAL(r1);
					PHALCON_ALLOC_ZVAL(r1);
				}
			}
			phalcon_concat_vboth(r1, r2, "</div>", t0 TSRMLS_CC);
			zend_print_zval(r1, 0);
		END_FOREACH(ac0, fes18, fee18, ah0, hp0);
	} else {
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_concat_left(r6, "<div class=\"", v2 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_vboth(r5, r6, "\">", v0 TSRMLS_CC);
		PHALCON_GET_CONSTANT(t1, "PHP_EOL");
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_concat_vboth(r4, r5, "</div>", t1 TSRMLS_CC);
		zend_print_zval(r4, 0);
	}
	RETURN_NULL();
}

/**
 * Shows a HTML error message
 *
 * 
 *
 * @param string $message
 * @param string $classes
 * @return string
 */
PHP_METHOD(Phalcon_Flash, error){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "errorMessage", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_showmessage", 2, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Shows a HTML notice/information message
 *
     * 
 *
 * @param string $message
 * @param string $classes
 * @return string
 */
PHP_METHOD(Phalcon_Flash, notice){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "noticeMessage", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_showmessage", 2, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Shows a HTML sucess message
 *
 * 
 *
 * @param string $message
 * @param string $classes
 * @return string
 */
PHP_METHOD(Phalcon_Flash, success){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "successMessage", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_showmessage", 2, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Shows a HTML warning message
 *
 * 
 *
 * @param string $message
 * @param string $classes
 * @return string
 */
PHP_METHOD(Phalcon_Flash, warning){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "warningMessage", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	Z_ADDREF_P(v1);
	p0[1] = v1;
	PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_showmessage", 2, p0);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Php_Logger
 *
 * Php_Logger is a component whose purpose is to create logs using different backends via adapters,
 * generating options and formats and filters and implementation of transactions
 */

PHP_METHOD(Phalcon_Logger, __construct){

	zval *v0 = NULL, *v1 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "File", 0);
	}
	
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Logger, setFormat){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	RETURN_NULL();
}

/**
 * Php_Config
 *
 * Php_Config is designed to simplify the access to, and the use of, configuration data within applications.
 * It provides a nested object property based user interface for accessing this configuration data within
 * application code.
 *
 * 
 *
 */

/**
 * Php_Config constructor
 *
 * @paramarray $arrayConfig
 * @returnPhp_Config
 */
PHP_METHOD(Phalcon_Config, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *ac0 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	FOREACH_KV(v0, ac0, fes19, fee19, ah0, hp0, v2, v1)
		if (Z_TYPE_P(v1) == IS_ARRAY) { 
			if (!i0) {
				PHALCON_ALLOC_ZVAL(i0);
			} else {
				if (Z_REFCOUNT_P(i0) > 1) {
					{
						zval *orig_ptr = i0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i0);
							*i0 = *orig_ptr;
							zval_copy_ctor(i0);
							Z_SET_REFCOUNT_P(i0, 1);
							Z_UNSET_ISREF_P(i0);
						}
					}
				} else {
					FREE_ZVAL(i0);
					PHALCON_ALLOC_ZVAL(i0);
				}
			}
			object_init_ex(i0, phalcon_config_class_entry);
			Z_ADDREF_P(v1);
			p0[0] = v1;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p0, PHALCON_CALL_CHECK);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, i0, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, Z_STRVAL_P(v2), Z_STRLEN_P(v2), copy TSRMLS_CC);
			}
		} else {
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v1, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, Z_STRVAL_P(v2), Z_STRLEN_P(v2), copy TSRMLS_CC);
			}
		}
	END_FOREACH(ac0, fes19, fee19, ah0, hp0);
	RETURN_NULL();
}

/**
 * Php_Filter
 *
 * The Php_Filter component provides a set of commonly needed data filters. It provides
 * object oriented wrappers to the php filter extension
 *
 *
 *
 */

/**
 * Sanizites a value with a specified single or set of filters
 *
 * @param mixed $value
 * @param mixed $filters
 * @return mixed
 */
PHP_METHOD(Phalcon_Filter, sanitize){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *ac0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v1) == IS_ARRAY) { 
		if (zend_is_true(v0)) {
			FOREACH_V(v1, ac0, fes20, fee20, ah0, hp0, v2)
				if (!r0) {
					PHALCON_ALLOC_ZVAL(r0);
				} else {
					if (Z_REFCOUNT_P(r0) > 1) {
						{
							zval *orig_ptr = r0;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r0);
								*r0 = *orig_ptr;
								zval_copy_ctor(r0);
								Z_SET_REFCOUNT_P(r0, 1);
								Z_UNSET_ISREF_P(r0);
							}
						}
					} else {
						FREE_ZVAL(r0);
						PHALCON_ALLOC_ZVAL(r0);
					}
				}
				Z_ADDREF_P(v0);
				p0[0] = v0;
				Z_ADDREF_P(v2);
				p0[1] = v2;
				PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "_sanitize", 2, p0, PHALCON_CALL_DEFAULT);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r0);
				v0 = r0;
			END_FOREACH(ac0, fes20, fee20, ah0, hp0);
		}
		if (Z_TYPE_P(v0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		Z_ADDREF_P(v0);
		p1[0] = v0;
		Z_ADDREF_P(v1);
		p1[1] = v1;
		PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "_sanitize", 2, p1, PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r1, 1, 0);
	}
	RETURN_NULL();
}

/**
 * Internal sanizite wrapper to filter_var
 *
 * @param mixed $value
 * @param mixed $filters
 * @return mixed
 */
PHP_METHOD(Phalcon_Filter, _sanitize){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL;
	zval *a0 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL, NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL, NULL, NULL }, *p4[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, "email", 1);
	PHALCON_EQUAL_FUNCTION(r0, v1, t0);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(r1);
		p0[0] = v0;
		//UnresolvedConstantParam=FILTER_SANITIZE_EMAIL
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_LONG(t1, 517);
		p0[1] = t1;
		PHALCON_CALL_FUNC_PARAMS(r1, "filter_var", 2, p0);
		RETURN_ZVAL(r1, 1, 0);
	}
	PHALCON_ALLOC_ZVAL(t2);
	ZVAL_STRING(t2, "int", 1);
	PHALCON_EQUAL_FUNCTION(r2, v1, t2);
	if (zend_is_true(r2)) {
		PHALCON_ALLOC_ZVAL(r3);
		p1[0] = v0;
		//UnresolvedConstantParam=FILTER_SANITIZE_NUMBER_INT
		PHALCON_ALLOC_ZVAL(t3);
		ZVAL_LONG(t3, 519);
		p1[1] = t3;
		PHALCON_CALL_FUNC_PARAMS(r3, "filter_var", 2, p1);
		RETURN_ZVAL(r3, 1, 0);
	}
	PHALCON_ALLOC_ZVAL(t4);
	ZVAL_STRING(t4, "string", 1);
	PHALCON_EQUAL_FUNCTION(r4, v1, t4);
	if (zend_is_true(r4)) {
		PHALCON_ALLOC_ZVAL(r5);
		p2[0] = v0;
		//UnresolvedConstantParam=FILTER_SANITIZE_STRING
		PHALCON_ALLOC_ZVAL(t5);
		ZVAL_LONG(t5, 513);
		p2[1] = t5;
		PHALCON_CALL_FUNC_PARAMS(r5, "filter_var", 2, p2);
		RETURN_ZVAL(r5, 1, 0);
	}
	PHALCON_ALLOC_ZVAL(t6);
	ZVAL_STRING(t6, "float", 1);
	PHALCON_EQUAL_FUNCTION(r6, v1, t6);
	if (zend_is_true(r6)) {
		PHALCON_ALLOC_ZVAL(r7);
		p3[0] = v0;
		//UnresolvedConstantParam=FILTER_SANITIZE_NUMBER_FLOAT
		PHALCON_ALLOC_ZVAL(t7);
		ZVAL_LONG(t7, 520);
		p3[1] = t7;
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		PHALCON_ALLOC_ZVAL(t8);
		ZVAL_LONG(t8, 4096);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_assoc_zval(a0, "flags", t8);
		p3[2] = a0;
		PHALCON_CALL_FUNC_PARAMS(r7, "filter_var", 3, p3);
		RETURN_ZVAL(r7, 1, 0);
	}
	PHALCON_ALLOC_ZVAL(i0);
	object_init_ex(i0, phalcon_exception_class_entry);
	PHALCON_ALLOC_ZVAL(r8);
	phalcon_concat_both(r8,  "Filter ", v1, " is not supported" TSRMLS_CC);
	Z_ADDREF_P(r8);
	p4[0] = r8;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p4, PHALCON_CALL_CHECK);
	zend_throw_exception_object(i0 TSRMLS_CC);
	Z_ADDREF_P(i0);
	return;
	se21:
	php_printf("");
	RETURN_NULL();
}

/**
 * Php_Exception
 *
 * All framework exceptions should use this exception
 */

/**
 * Php_Exception constructor
 *
 * @param string $message
 * @param int $errorCode
 * @param boolean $showTrace
 * @param array $backtrace
 */
PHP_METHOD(Phalcon_Exception, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL }, *p2[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zzz", &v0, &v1, &v2, &v3) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_LONG(v1, 0);
	}
	if (!v2) {
		PHALCON_INIT_BOOL(v2, 1);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_numeric", 1, p0);
	if (zend_is_true(r0)) {
		Z_ADDREF_P(v0);
		p1[0] = v0;
		Z_ADDREF_P(v1);
		p1[1] = v1;
		PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Exception", "__construct", 2, p1);
	} else {
		Z_ADDREF_P(v0);
		p2[0] = v0;
		PHALCON_PARAM_LONG(p2[1], 0);
		PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Exception", "__construct", 2, p2);
	}
	RETURN_NULL();
}

/**
 * Php_Transaction
 *
 * Transactions are protective blocks where SQL statements are only permanent if they can
 * all succeed as one atomic action. Php_Transaction is intended to be used with Php_Model_Base.
 * Phalcon Transactions should be created using Php_Transaction_Manager.
 *
 *
 */

/**
 * Php_Transaction constructor
 *
 * @param boolean $autoBegin
 */
PHP_METHOD(Phalcon_Transaction, __construct){

	zval *a0 = NULL, *a1 = NULL;
	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_property(phalcon_transaction_class_entry, this_ptr, "_dependencies", strlen("_dependencies"), a0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(a1);
	array_init(a1);
	zend_update_property(phalcon_transaction_class_entry, this_ptr, "_messages", strlen("_messages"), a1 TSRMLS_CC);
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_BOOL(v0, 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_PARAM_BOOL(p0[0], 1);
	PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_db_pool", "getconnection", 1, p0);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_connection", strlen("_connection"), copy TSRMLS_CC);
	}
	PHALCON_INIT_TRUE(t0);
	PHALCON_EQUAL_FUNCTION(r1, v0, t0);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_CALL_METHOD_NORETURN(t1, "begin", PHALCON_CALL_DEFAULT);
	}
	RETURN_NULL();
}

/**
 * Sets transaction manager related to the transaction
 *
 * @param Php_Transaction_Manager $manager
 */
PHP_METHOD(Phalcon_Transaction, setTransactionManager){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_manager", strlen("_manager"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Starts the transaction
 */
PHP_METHOD(Phalcon_Transaction, begin){

	zval *r0 = NULL;
	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r0, t0, "begin", PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Commits the transaction
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction, commit){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL, NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!phalcon_compare_strict_string(t0, "")) {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_next_index_zval(a0, t1);
		add_next_index_string(a0, "notifyCommit", 1);
		Z_ADDREF_P(a0);
		p0[0] = a0;
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		{
			zval *orig_ptr = a1;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a1);
				*a1 = *orig_ptr;
				zval_copy_ctor(a1);
				Z_SET_REFCOUNT_P(a1, 1);
				Z_UNSET_ISREF_P(a1);
			}
		}
		add_next_index_zval(a1, this_ptr);
		Z_ADDREF_P(a1);
		p0[1] = a1;
		PHALCON_CALL_FUNC_PARAMS_NORETURN("call_user_func_array", 2, p0);
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	PHALCON_CALL_METHOD(r0, t2, "commit", PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Rollbacks the transaction
 *
 * @param  string $rollbackMessage
 * @param  Php_Model_Base $rollbackRecord
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction, rollback){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL, NULL }, *p3[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	Z_ADDREF_P(t0);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS_NORETURN("var_dump", 1, p0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (!phalcon_compare_strict_string(t1, "")) {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_next_index_zval(a0, t2);
		add_next_index_string(a0, "notifyRollback", 1);
		Z_ADDREF_P(a0);
		p1[0] = a0;
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		{
			zval *orig_ptr = a1;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a1);
				*a1 = *orig_ptr;
				zval_copy_ctor(a1);
				Z_SET_REFCOUNT_P(a1, 1);
				Z_UNSET_ISREF_P(a1);
			}
		}
		add_next_index_zval(a1, this_ptr);
		Z_ADDREF_P(a1);
		p1[1] = a1;
		PHALCON_CALL_FUNC_PARAMS_NORETURN("call_user_func_array", 2, p1);
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_read_property(t3, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	PHALCON_CALL_METHOD(r0, t3, "rollback", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	PHALCON_INIT_TRUE(t4);
	PHALCON_EQUAL_FUNCTION(r1, v2, t4);
	if (zend_is_true(r1)) {
		if (phalcon_compare_strict_string(v0, "")) {
			PHALCON_ALLOC_ZVAL(v0);
			ZVAL_STRING(v0, "Transaction aborted", 0);
		}
		if (Z_TYPE_P(v1) != IS_NULL) {
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v1, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_rollbackRecord", strlen("_rollbackRecord"), copy TSRMLS_CC);
			}
		}
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_transaction_failed_class_entry);
		Z_ADDREF_P(v0);
		p3[0] = v0;
		PHALCON_ALLOC_ZVAL(t5);
		phalcon_read_property(t5, this_ptr, "_rollbackRecord", sizeof("_rollbackRecord")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t5);
		Z_ADDREF_P(t5);
		p3[1] = t5;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 2, p3, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
 * Returns connection related to transaction
 *
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Transaction, getConnection){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;
	zval *p1[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_rollbackOnAbort", sizeof("_rollbackOnAbort")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_FUNC(r0, "connection_aborted");
		if (zend_is_true(r0)) {
			PHALCON_ALLOC_ZVAL(p1[0]);
			ZVAL_STRING(p1[0], "The request was aborted", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "rollback", 1, p1, PHALCON_CALL_DEFAULT);
		}
	}
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets if is a reused transaction or new once
 *
 * @param boolean $isNew
 */
PHP_METHOD(Phalcon_Transaction, setIsNewTransaction){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_isNewTransaction", strlen("_isNewTransaction"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sets flag to rollback on abort the HTTP connection
 *
 * @param boolean $rollbackOnAbort
 */
PHP_METHOD(Phalcon_Transaction, setRollbackOnAbort){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_rollbackOnAbort", strlen("_rollbackOnAbort"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Checks whether transaction is managed by a transaction manager
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction, isManaged){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_INIT_NULL(t1);
	PHALCON_EQUAL_FUNCTION(r1, t0, t1);
	if (zend_is_true(r1)) {
		PHALCON_INIT_FALSE(t2);
		r0 = t2;
	} else {
		PHALCON_INIT_TRUE(t3);
		r0 = t3;
	}
	if (Z_TYPE_P(r0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(r0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Changes dependency internal pointer
 *
 * @param int $pointer
 */
PHP_METHOD(Phalcon_Transaction, setDependencyPointer){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_pointer", strlen("_pointer"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Attaches Php_Model_Base object to the active transaction
 *
 * @param int $pointer
 * @param Php_Model_Base $object
 */
PHP_METHOD(Phalcon_Transaction, attachDependency){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *r0 = NULL;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_INIT_NULL(t0);
	PHALCON_EQUAL_FUNCTION(r0, v0, t0);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_pointer", sizeof("_pointer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		{
			zval *orig_ptr = t1;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(t1);
				*t1 = *orig_ptr;
				zval_copy_ctor(t1);
				Z_SET_REFCOUNT_P(t1, 1);
				Z_UNSET_ISREF_P(t1);
			}
		}
		increment_function(t1);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_pointer", strlen("_pointer"), copy TSRMLS_CC);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t1);
		v0 = t1;
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_dependencies", sizeof("_dependencies")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		{
			zval *orig_ptr = t2;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(t2);
				*t2 = *orig_ptr;
				zval_copy_ctor(t2);
				Z_SET_REFCOUNT_P(t2, 1);
				Z_UNSET_ISREF_P(t2);
			}
		}
		phalcon_array_update(t2, v0, v1 TSRMLS_CC);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_dependencies", strlen("_dependencies"), copy TSRMLS_CC);
		}
		if (Z_TYPE_P(v0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_dependencies", sizeof("_dependencies")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		eval_int = phalcon_array_isset(t3, v0);
		if (!eval_int) {
			PHALCON_ALLOC_ZVAL(t4);
			phalcon_read_property(t4, this_ptr, "_dependencies", sizeof("_dependencies")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t4);
			{
				zval *orig_ptr = t4;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t4);
					*t4 = *orig_ptr;
					zval_copy_ctor(t4);
					Z_SET_REFCOUNT_P(t4, 1);
					Z_UNSET_ISREF_P(t4);
				}
			}
			phalcon_array_update(t4, v0, v1 TSRMLS_CC);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t4, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_dependencies", strlen("_dependencies"), copy TSRMLS_CC);
			}
			if (Z_TYPE_P(v0) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(v0);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(v0);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		} else {
			PHALCON_ALLOC_ZVAL(t5);
			phalcon_read_property(t5, this_ptr, "_pointer", sizeof("_pointer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t5);
			{
				zval *orig_ptr = t5;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t5);
					*t5 = *orig_ptr;
					zval_copy_ctor(t5);
					Z_SET_REFCOUNT_P(t5, 1);
					Z_UNSET_ISREF_P(t5);
				}
			}
			increment_function(t5);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t5, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_pointer", strlen("_pointer"), copy TSRMLS_CC);
			}
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(t5);
			v0 = t5;
			PHALCON_ALLOC_ZVAL(t6);
			phalcon_read_property(t6, this_ptr, "_dependencies", sizeof("_dependencies")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t6);
			{
				zval *orig_ptr = t6;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t6);
					*t6 = *orig_ptr;
					zval_copy_ctor(t6);
					Z_SET_REFCOUNT_P(t6, 1);
					Z_UNSET_ISREF_P(t6);
				}
			}
			phalcon_array_update(t6, v0, v1 TSRMLS_CC);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t6, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_dependencies", strlen("_dependencies"), copy TSRMLS_CC);
			}
			if (Z_TYPE_P(v0) > IS_BOOL) {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(v0);
					zval_copy_ctor(return_value);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			} else {
				{
					zend_uchar is_ref = Z_ISREF_P(return_value);
					zend_uint refcount = Z_REFCOUNT_P(return_value);
					*(return_value) = *(v0);
					Z_SET_ISREF_TO_P(return_value, is_ref);
			 		Z_SET_REFCOUNT_P(return_value, refcount);
				}
			}
			return;
		}
	}
	RETURN_NULL();
}

/**
 * Make a bulk save on all attached objects
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction, save){

	zval *a0 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *ac0 = NULL;
	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, a0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_messages", strlen("_messages"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dependencies", sizeof("_dependencies")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	FOREACH_V(t0, ac0, fes22, fee22, ah0, hp0, v0)
		if (!r0) {
			PHALCON_ALLOC_ZVAL(r0);
		} else {
			if (Z_REFCOUNT_P(r0) > 1) {
				{
					zval *orig_ptr = r0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r0);
						*r0 = *orig_ptr;
						zval_copy_ctor(r0);
						Z_SET_REFCOUNT_P(r0, 1);
						Z_UNSET_ISREF_P(r0);
					}
				}
			} else {
				FREE_ZVAL(r0);
				PHALCON_ALLOC_ZVAL(r0);
			}
		}
		PHALCON_CALL_METHOD(r0, v0, "save", PHALCON_CALL_DEFAULT);
		PHALCON_INIT_FALSE(t1);
		PHALCON_EQUAL_FUNCTION(r1, r0, t1);
		if (zend_is_true(r1)) {
			if (!r2) {
				PHALCON_ALLOC_ZVAL(r2);
			} else {
				if (Z_REFCOUNT_P(r2) > 1) {
					{
						zval *orig_ptr = r2;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r2);
							*r2 = *orig_ptr;
							zval_copy_ctor(r2);
							Z_SET_REFCOUNT_P(r2, 1);
							Z_UNSET_ISREF_P(r2);
						}
					}
				} else {
					FREE_ZVAL(r2);
					PHALCON_ALLOC_ZVAL(r2);
				}
			}
			PHALCON_CALL_METHOD(r2, v0, "getmessages", PHALCON_CALL_DEFAULT);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_messages", strlen("_messages"), copy TSRMLS_CC);
			}
			RETURN_FALSE;
		}
	END_FOREACH(ac0, fes22, fee22, ah0, hp0);
	RETURN_TRUE;
}

/**
 * Returns validations messages from last save try
 *
 * @return array
 */
PHP_METHOD(Phalcon_Transaction, getMessages){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_messages", sizeof("_messages")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
     * Checks whether internal connection is under an active transaction
     *
     * @return boolean
     */
PHP_METHOD(Phalcon_Transaction, isValid){

	zval *r0 = NULL;
	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r0, t0, "isundertransaction", PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Sets object which generates rollback action
 *
 * @param Php_Model_Base $record
 */
PHP_METHOD(Phalcon_Transaction, setRollbackedRecord){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_rollbackRecord", strlen("_rollbackRecord"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Php_Transaction_Failed
 *
 * Php_Transaction_Failed will thrown to exit a try/catch block for transactions
 *
 */

/**
 * Php_Transaction_Failed constructor
 *
 * @param string $message
 * @param Php_Model_Base $record
 */
PHP_METHOD(Phalcon_Transaction_Failed, __construct){

	zval *v0 = NULL, *v1 = NULL;
	zval *p0[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_record", strlen("_record"), copy TSRMLS_CC);
	}
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_PARAM_LONG(p0[1], 0);
	PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Transaction_Failed", "__construct", 2, p0);
	RETURN_NULL();
}

/**
 * Returns validation record messages which stop the transaction
 *
 * @return string
 */
PHP_METHOD(Phalcon_Transaction_Failed, getRecordMessages){

	zval *v0 = NULL;
	zval *t0 = NULL;
	zval *r0 = NULL, *r1 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_record", sizeof("_record")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(t0);
	v0 = t0;
	if (Z_TYPE_P(v0) != IS_NULL) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, v0, "getmessages", PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r0, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_METHOD(r1, this_ptr, "getmessage", PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r1, 1, 0);
	}
	RETURN_NULL();
}

/**
 * Returns validation record messages which stop the transaction
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Transaction_Failed, getRecord){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_record", sizeof("_record")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Transaction_Manager
 *
 * A transaction acts on a single database connection. If you have multiple class-specific
 * databases, the transaction will not protect interaction among them
 *
 *
 *
 */

/**
 * Checks whether manager has an active transaction
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction_Manager, has){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;

	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 0);
	PHALCON_SMALLER_FUNCTION(r0, t1, t0);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r0);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Returns a new Php_Transaction or an already created once
 *
 * @param boolean $autoBegin
 * @return Php_Transaction
 */
PHP_METHOD(Phalcon_Transaction_Manager, get){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL;
	zval *a0 = NULL;
	zval *i0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL }, *p4[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_BOOL(v0, 1);
	}
	
	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_initialized", sizeof("_initialized")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		add_next_index_string(a0, "Phalcon_Transaction_Manager", 1);
		add_next_index_string(a0, "rollbackPendent", 1);
		p0[0] = a0;
		PHALCON_CALL_FUNC_PARAMS_NORETURN("register_shutdown_function", 1, p0);
		PHALCON_INIT_TRUE(t1);
		zend_update_static_property(phalcon_transaction_manager_class_entry, "_initialized", sizeof("_initialized")-1, t1 TSRMLS_CC);
	}
	t2 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(t2);
	v1 = t2;
	if (!zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_transaction_class_entry);
		Z_ADDREF_P(v0);
		p1[0] = v0;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(i0);
		v2 = i0;
		PHALCON_ALLOC_ZVAL(p2[0]);
		ZVAL_STRING(p2[0], "Phalcon_Transaction_Manager", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "settransactionmanager", 1, p2, PHALCON_CALL_DEFAULT);
		t3 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_dependencyPointer", sizeof("_dependencyPointer")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		Z_ADDREF_P(t3);
		p3[0] = t3;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "setdependencypointer", 1, p3, PHALCON_CALL_DEFAULT);
		t4 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_dependencyPointer", sizeof("_dependencyPointer")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(t5);
		ZVAL_LONG(t5, 2048);
		PHALCON_ADD_FUNCTION(r0, t4, t5);
		zend_update_static_property(phalcon_transaction_manager_class_entry, "_dependencyPointer", sizeof("_dependencyPointer")-1, r0 TSRMLS_CC);
		t6 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		if (Z_TYPE_P(t6) != IS_ARRAY) {
			convert_to_array(t6);
		}
		phalcon_array_append(t6, v2 TSRMLS_CC);
		zend_update_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, t6 TSRMLS_CC);
		
		t7 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		{
			zval *orig_ptr = t7;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(t7);
				*t7 = *orig_ptr;
				zval_copy_ctor(t7);
				Z_SET_REFCOUNT_P(t7, 1);
				Z_UNSET_ISREF_P(t7);
			}
		}
		increment_function(t7);
		zend_update_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, t7 TSRMLS_CC);
	} else {
		t8 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(t10);
		ZVAL_LONG(t10, 1);
		PHALCON_ALLOC_ZVAL(r1);
		sub_function(r1, v1, t10 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(t9);
		phalcon_array_fetch(t9, t8, r1, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(t9);
		v2 = t9;
		PHALCON_PARAM_BOOL(p4[0], 0);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "setisnewtransaction", 1, p4, PHALCON_CALL_DEFAULT);
	}
	if (Z_TYPE_P(v2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Rollbacks active transactions whithin the manager
 *
 */
PHP_METHOD(Phalcon_Transaction_Manager, rollbackPendent){


	PHALCON_CALL_SELF_NORETURN(this_ptr, "rollback");
	RETURN_NULL();
}

/**
 * Commmits active transactions whithin the manager
 *
 */
PHP_METHOD(Phalcon_Transaction_Manager, commit){

	zval *t0 = NULL;
	zval *ac0 = NULL;
	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	FOREACH_V(t0, ac0, fes23, fee23, ah0, hp0, v0)
		if (!r0) {
			PHALCON_ALLOC_ZVAL(r0);
		} else {
			if (Z_REFCOUNT_P(r0) > 1) {
				{
					zval *orig_ptr = r0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r0);
						*r0 = *orig_ptr;
						zval_copy_ctor(r0);
						Z_SET_REFCOUNT_P(r0, 1);
						Z_UNSET_ISREF_P(r0);
					}
				}
			} else {
				FREE_ZVAL(r0);
				PHALCON_ALLOC_ZVAL(r0);
			}
		}
		PHALCON_CALL_METHOD(r0, v0, "getconnection", PHALCON_CALL_DEFAULT);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r0);
		v1 = r0;
		if (!r1) {
			PHALCON_ALLOC_ZVAL(r1);
		} else {
			if (Z_REFCOUNT_P(r1) > 1) {
				{
					zval *orig_ptr = r1;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r1);
						*r1 = *orig_ptr;
						zval_copy_ctor(r1);
						Z_SET_REFCOUNT_P(r1, 1);
						Z_UNSET_ISREF_P(r1);
					}
				}
			} else {
				FREE_ZVAL(r1);
				PHALCON_ALLOC_ZVAL(r1);
			}
		}
		PHALCON_CALL_METHOD(r1, v1, "isundertransaction", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r1)) {
			PHALCON_CALL_METHOD_NORETURN(v1, "commit", PHALCON_CALL_DEFAULT);
		}
	END_FOREACH(ac0, fes23, fee23, ah0, hp0);
	RETURN_NULL();
}

/**
 * Rollbacks active transactions whithin the manager
 * Collect will remove transaction from the manager
 *
 * @param boolean $collect
 */
PHP_METHOD(Phalcon_Transaction_Manager, rollback){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *t0 = NULL;
	zval *ac0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *p4[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_BOOL(v0, 0);
	}
	
	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	FOREACH_V(t0, ac0, fes24, fee24, ah0, hp0, v1)
		if (!r0) {
			PHALCON_ALLOC_ZVAL(r0);
		} else {
			if (Z_REFCOUNT_P(r0) > 1) {
				{
					zval *orig_ptr = r0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r0);
						*r0 = *orig_ptr;
						zval_copy_ctor(r0);
						Z_SET_REFCOUNT_P(r0, 1);
						Z_UNSET_ISREF_P(r0);
					}
				}
			} else {
				FREE_ZVAL(r0);
				PHALCON_ALLOC_ZVAL(r0);
			}
		}
		PHALCON_CALL_METHOD(r0, v1, "getconnection", PHALCON_CALL_DEFAULT);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
		if (!r1) {
			PHALCON_ALLOC_ZVAL(r1);
		} else {
			if (Z_REFCOUNT_P(r1) > 1) {
				{
					zval *orig_ptr = r1;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r1);
						*r1 = *orig_ptr;
						zval_copy_ctor(r1);
						Z_SET_REFCOUNT_P(r1, 1);
						Z_UNSET_ISREF_P(r1);
					}
				}
			} else {
				FREE_ZVAL(r1);
				PHALCON_ALLOC_ZVAL(r1);
			}
		}
		PHALCON_CALL_METHOD(r1, v2, "isundertransaction", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r1)) {
			PHALCON_CALL_METHOD_NORETURN(v2, "rollback", PHALCON_CALL_DEFAULT);
			PHALCON_CALL_METHOD_NORETURN(v2, "close", PHALCON_CALL_DEFAULT);
		}
		if (zend_is_true(v0)) {
			Z_ADDREF_P(v1);
			p4[0] = v1;
			PHALCON_CALL_SELF_PARAMS_NORETURN(this_ptr, "_collecttransaction", 1, p4);
		}
	END_FOREACH(ac0, fes24, fee24, ah0, hp0);
	RETURN_NULL();
}

/**
 * Notifies the manager about a rollbacked transaction
 *
 * @param Php_Transaction $transaction
 */
PHP_METHOD(Phalcon_Transaction_Manager, notifyRollback){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_SELF_PARAMS_NORETURN(this_ptr, "_collecttransaction", 1, p0);
	RETURN_NULL();
}

/**
 * Notifies the manager about a commited transaction
 *
 * @param Php_Transaction $transaction
 */
PHP_METHOD(Phalcon_Transaction_Manager, notifyCommit){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_SELF_PARAMS_NORETURN(this_ptr, "_collecttransaction", 1, p0);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Transaction_Manager, _collectTransaction){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *a0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "count", 1, p0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 0);
	PHALCON_SMALLER_FUNCTION(r1, t1, r0);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_LONG(v1, 0);
		t2 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		FOREACH_V(t2, ac0, fes25, fee25, ah0, hp0, v2)
			PHALCON_EQUAL_FUNCTION(r2, v2, v0);
			if (zend_is_true(r2)) {
				t3 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
				{
					zval *orig_ptr = t3;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t3);
						*t3 = *orig_ptr;
						zval_copy_ctor(t3);
						Z_SET_REFCOUNT_P(t3, 1);
						Z_UNSET_ISREF_P(t3);
					}
				}
				decrement_function(t3);
				zend_update_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, t3 TSRMLS_CC);
			}
			{
				zval *orig_ptr = v1;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v1);
					*v1 = *orig_ptr;
					zval_copy_ctor(v1);
					Z_SET_REFCOUNT_P(v1, 1);
					Z_UNSET_ISREF_P(v1);
				}
			}
			increment_function(v1);
		END_FOREACH(ac0, fes25, fee25, ah0, hp0);
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(a0);
		v3 = a0;
		t4 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		FOREACH_V(t4, ac1, fes26, fee26, ah1, hp1, v2)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = v3;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v3);
						*v3 = *orig_ptr;
						zval_copy_ctor(v3);
						Z_SET_REFCOUNT_P(v3, 1);
						Z_UNSET_ISREF_P(v3);
					}
				}
				phalcon_array_append(v3, copy TSRMLS_CC);
			}
		END_FOREACH(ac1, fes26, fee26, ah1, hp1);
		zend_update_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, v3 TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Remove all the transactions from the manager
 *
 */
PHP_METHOD(Phalcon_Transaction_Manager, collectTransactions){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *v0 = NULL, *v1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	PHALCON_ALLOC_ZVAL(r0);
	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "count", 1, p0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 0);
	PHALCON_SMALLER_FUNCTION(r1, t1, r0);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_LONG(v0, 0);
		t2 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		FOREACH_V(t2, ac0, fes27, fee27, ah0, hp0, v1)
			t3 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
			{
				zval *orig_ptr = t3;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(t3);
					*t3 = *orig_ptr;
					zval_copy_ctor(t3);
					Z_SET_REFCOUNT_P(t3, 1);
					Z_UNSET_ISREF_P(t3);
				}
			}
			decrement_function(t3);
			zend_update_static_property(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, t3 TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			increment_function(v0);
		END_FOREACH(ac0, fes27, fee27, ah0, hp0);
	}
	RETURN_NULL();
}

/**
 * Checks whether manager will inject an automatic transaction to all newly
 * created instances of Php_Model_base
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Transaction_Manager, isAutomatic){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;

	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_automaticTransaction", sizeof("_automaticTransaction")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_INIT_NULL(t1);
	PHALCON_NOT_EQUAL_FUNCTION(r0, t0, t1);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r0);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Returns automatic transaction for instances of Php_Model_base
 *
 * @return Php_Transaction
 */
PHP_METHOD(Phalcon_Transaction_Manager, getAutomatic){

	zval *t0 = NULL;

	t0 = zend_read_static_property(phalcon_transaction_manager_class_entry, "_automaticTransaction", sizeof("_automaticTransaction")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Config_Adapter_Ini, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *i0 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *p0[] = { NULL, NULL }, *p2[] = { NULL }, *p1[] = { NULL }, *p3[] = { NULL, NULL }, *p4[] = { NULL, NULL }, *p5[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	zend_bool silence;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(a0);
	v1 = a0;
	silence = PG(display_errors);
	PG(display_errors) = 0;
	PHALCON_ALLOC_ZVAL(r0);
	p0[0] = v0;
	PHALCON_PARAM_BOOL(p0[1], 1);
	PHALCON_CALL_FUNC_PARAMS(r0, "parse_ini_file", 2, p0);
	PG(display_errors) = silence;
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r0);
	v2 = r0;
	if (Z_TYPE_P(v2) == IS_BOOL && !Z_BVAL_P(v2)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_config_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r1);
		Z_ADDREF_P(v0);
		p2[0] = v0;
		PHALCON_CALL_FUNC_PARAMS(r1, "basename", 1, p2);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_both(r2,  "Configuration file ", r1, " can't be loaded" TSRMLS_CC);
		Z_ADDREF_P(r2);
		p1[0] = r2;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	FOREACH_KV(v2, ac0, fes28, fee28, ah0, hp0, v4, v3)
		FOREACH_KV(v3, ac1, fes29, fee29, ah1, hp1, v6, v5)
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			p3[0] = v6;
			if (!p3[1]) {
				PHALCON_ALLOC_ZVAL(p3[1]);
			} else {
				if (Z_REFCOUNT_P(p3[1]) > 1) {
					SEPARATE_ZVAL(&p3[1]);
				} else {
					if (Z_TYPE_P(p3[1]) != IS_STRING) {
						FREE_ZVAL(p3[1]);
						PHALCON_ALLOC_ZVAL(p3[1]);
					}
				}
			}
			ZVAL_STRING(p3[1], ".", 1);
			PHALCON_CALL_FUNC_PARAMS(r3, "strpos", 2, p3);
			PHALCON_INIT_FALSE(t0);
			PHALCON_NOT_IDENTICAL_FUNCTION(r4, r3, t0);
			if (zend_is_true(r4)) {
				if (!r5) {
					PHALCON_ALLOC_ZVAL(r5);
				} else {
					if (Z_REFCOUNT_P(r5) > 1) {
						{
							zval *orig_ptr = r5;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r5);
								*r5 = *orig_ptr;
								zval_copy_ctor(r5);
								Z_SET_REFCOUNT_P(r5, 1);
								Z_UNSET_ISREF_P(r5);
							}
						}
					} else {
						FREE_ZVAL(r5);
						PHALCON_ALLOC_ZVAL(r5);
					}
				}
				if (!p4[0]) {
					PHALCON_ALLOC_ZVAL(p4[0]);
				} else {
					if (Z_REFCOUNT_P(p4[0]) > 1) {
						SEPARATE_ZVAL(&p4[0]);
					} else {
						if (Z_TYPE_P(p4[0]) != IS_STRING) {
							FREE_ZVAL(p4[0]);
							PHALCON_ALLOC_ZVAL(p4[0]);
						}
					}
				}
				ZVAL_STRING(p4[0], ".", 1);
				p4[1] = v6;
				PHALCON_CALL_FUNC_PARAMS(r5, "explode", 2, p4);
				if (v7) {
					Z_DELREF_P(v7);
					if (!Z_REFCOUNT_P(v7)) {
						FREE_ZVAL(v7);
					}
				}
				Z_ADDREF_P(r5);
				v7 = r5;
				if (Z_TYPE_P(v1) == IS_ARRAY) {
					if (!t1) {
						PHALCON_ALLOC_ZVAL(t1);
					} else {
						if (Z_REFCOUNT_P(t1) > 1) {
							{
								zval *orig_ptr = t1;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t1);
									*t1 = *orig_ptr;
									zval_copy_ctor(t1);
									Z_SET_REFCOUNT_P(t1, 1);
									Z_UNSET_ISREF_P(t1);
								}
							}
						} else {
							FREE_ZVAL(t1);
							PHALCON_ALLOC_ZVAL(t1);
						}
					}
					phalcon_array_fetch(t1, v1, v4, PHALCON_SILENT_FETCH TSRMLS_CC);
				}
				if (Z_TYPE_P(t1) != IS_ARRAY) {
					convert_to_array(t1);
					phalcon_array_update(v1, v4, t1 TSRMLS_CC);
					Z_ADDREF_P(t1);
				}
				if (!r6) {
					PHALCON_ALLOC_ZVAL(r6);
				} else {
					if (Z_REFCOUNT_P(r6) > 1) {
						{
							zval *orig_ptr = r6;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r6);
								*r6 = *orig_ptr;
								zval_copy_ctor(r6);
								Z_SET_REFCOUNT_P(r6, 1);
								Z_UNSET_ISREF_P(r6);
							}
						}
					} else {
						FREE_ZVAL(r6);
						PHALCON_ALLOC_ZVAL(r6);
					}
				}
				phalcon_array_fetch_long(r6, v7, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				if (Z_TYPE_P(t1) == IS_ARRAY) {
					if (!t2) {
						PHALCON_ALLOC_ZVAL(t2);
					} else {
						if (Z_REFCOUNT_P(t2) > 1) {
							{
								zval *orig_ptr = t2;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t2);
									*t2 = *orig_ptr;
									zval_copy_ctor(t2);
									Z_SET_REFCOUNT_P(t2, 1);
									Z_UNSET_ISREF_P(t2);
								}
							}
						} else {
							FREE_ZVAL(t2);
							PHALCON_ALLOC_ZVAL(t2);
						}
					}
					phalcon_array_fetch(t2, t1, r6, PHALCON_SILENT_FETCH TSRMLS_CC);
				}
				if (Z_TYPE_P(t2) != IS_ARRAY) {
					convert_to_array(t2);
					phalcon_array_update(t1, r6, t2 TSRMLS_CC);
					Z_ADDREF_P(t2);
				}
				if (!r7) {
					PHALCON_ALLOC_ZVAL(r7);
				} else {
					if (Z_REFCOUNT_P(r7) > 1) {
						{
							zval *orig_ptr = r7;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r7);
								*r7 = *orig_ptr;
								zval_copy_ctor(r7);
								Z_SET_REFCOUNT_P(r7, 1);
								Z_UNSET_ISREF_P(r7);
							}
						}
					} else {
						FREE_ZVAL(r7);
						PHALCON_ALLOC_ZVAL(r7);
					}
				}
				phalcon_array_fetch_long(r7, v7, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, v5, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_array_update(t2, r7, copy TSRMLS_CC);
				}
			} else {
				if (Z_TYPE_P(v1) == IS_ARRAY) {
					if (!t3) {
						PHALCON_ALLOC_ZVAL(t3);
					} else {
						if (Z_REFCOUNT_P(t3) > 1) {
							{
								zval *orig_ptr = t3;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t3);
									*t3 = *orig_ptr;
									zval_copy_ctor(t3);
									Z_SET_REFCOUNT_P(t3, 1);
									Z_UNSET_ISREF_P(t3);
								}
							}
						} else {
							FREE_ZVAL(t3);
							PHALCON_ALLOC_ZVAL(t3);
						}
					}
					phalcon_array_fetch(t3, v1, v4, PHALCON_SILENT_FETCH TSRMLS_CC);
				}
				if (Z_TYPE_P(t3) != IS_ARRAY) {
					convert_to_array(t3);
					phalcon_array_update(v1, v4, t3 TSRMLS_CC);
					Z_ADDREF_P(t3);
				}
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, v5, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_array_update(t3, v6, copy TSRMLS_CC);
				}
			}
		END_FOREACH(ac1, fes29, fee29, ah1, hp1);
	END_FOREACH(ac0, fes28, fee28, ah0, hp0);
	Z_ADDREF_P(v1);
	p5[0] = v1;
	PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Config_Adapter_Ini", "__construct", 1, p5);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Config_Exception, __construct){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_PARENT_PARAMS_NORETURN(this_ptr, "Phalcon_Config_Exception", "__construct", 1, p0);
	RETURN_NULL();
}

/**
 * Php_Controller
 *
 * Every application controller should extends this class that encapsulates all the controller functionality
 *
 * Controllers provide the “flow” between models and views. Controllers are responsible
 * for processing the incoming requests from the web browser, interrogating the models for data,
 * and passing that data on to the views for presentation.
 *
 *
 */

/**
 * Constructor for Php_Controller
 *
 * @param Php_Dispatcher $dispatcher
 * @param Php_Request $request
 * @param Php_Response $response
 * @param Php_View $view
 * @param Php_Model_Manager $model
 */
PHP_METHOD(Phalcon_Controller, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz|zz", &v0, &v1, &v2, &v3, &v4) == FAILURE) {
		RETURN_NULL();
	}

	if (!v3) {
		PHALCON_INIT_NULL(v3);
	}
	if (!v4) {
		PHALCON_INIT_NULL(v4);
	}
	
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_dispatcher", strlen("_dispatcher"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "request", strlen("request"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v2, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "response", strlen("response"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v3, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "view", strlen("view"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v4, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_model", strlen("_model"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Forwards execution flow to another controller/action.
 *
 * @param string $uri
 */
PHP_METHOD(Phalcon_Controller, _forward){

	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dispatcher", sizeof("_dispatcher")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_METHOD_PARAMS(r0, t0, "forward", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Magic method __get
 *
 * @param string $propertyName
 */
PHP_METHOD(Phalcon_Controller, __get){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *i0 = NULL;
	zval *p2[] = { NULL }, *p3[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (phalcon_compare_strict_string(v0, "view")) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, this_ptr, "_getviewcomponent", PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "view", strlen("view"), copy TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(t0);
		phalcon_read_property(t0, this_ptr, "view", sizeof("view")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t0);
		if (Z_TYPE_P(t0) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t0);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	}
	if (phalcon_compare_strict_string(v0, "filter")) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_filter_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, i0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "filter", strlen("filter"), copy TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "filter", sizeof("filter")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		if (Z_TYPE_P(t1) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t1);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	if (zend_is_true(t2)) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		Z_ADDREF_P(v0);
		p2[0] = v0;
		PHALCON_CALL_METHOD_PARAMS(r1, t3, "ismodel", 1, p2, PHALCON_CALL_DEFAULT);
		if (zend_is_true(r1)) {
			PHALCON_ALLOC_ZVAL(r2);
			PHALCON_ALLOC_ZVAL(t4);
			phalcon_read_property(t4, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t4);
			Z_ADDREF_P(v0);
			p3[0] = v0;
			PHALCON_CALL_METHOD_PARAMS(r2, t4, "getmodel", 1, p3, PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r2, 1, 0);
		}
	}
	RETURN_NULL();
}

/**
 * Php_Model_Message
 *
 * Encapsulates validation info generated before save/delete records fails
 *
 * 
 *
 */

/**
     * Php_Model_Message message
     *
     * @param string $message
     * @param string $field
     * @param string $type
     */
PHP_METHOD(Phalcon_Model_Message, __construct){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zz", &v0, &v1, &v2) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	if (!v2) {
		PHALCON_INIT_NULL(v2);
	}
	
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_message", strlen("_message"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_field", strlen("_field"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v2, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_type", strlen("_type"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
     * Sets message type
     *
     * @param string $type
     */
PHP_METHOD(Phalcon_Model_Message, setType){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_type", strlen("_type"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
     * Returns message type
     *
     * @return string
     */
PHP_METHOD(Phalcon_Model_Message, getType){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_type", sizeof("_type")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
     * Sets verbose message
     *
     * @param string $message
     */
PHP_METHOD(Phalcon_Model_Message, setMessage){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_message", strlen("_message"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
     * Returns verbose message
     *
     * @return string
     */
PHP_METHOD(Phalcon_Model_Message, getMessage){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_message", sizeof("_message")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
     * Sets field name related to message
     *
     * @param string $field
     */
PHP_METHOD(Phalcon_Model_Message, setField){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_field", strlen("_field"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
     * Returns field name related to message
     *
     * @return string
     */
PHP_METHOD(Phalcon_Model_Message, getField){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_field", sizeof("_field")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
     * Magic __toString method returns verbose message
     *
     * @return string
     */
PHP_METHOD(Phalcon_Model_Message, __toString){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_message", sizeof("_message")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
     * Magic __set_state helps to recover messsages from serialization
     *
     * @param array $message
     * @return Php_Model_Message
     */
PHP_METHOD(Phalcon_Model_Message, __set_state){

	zval *v0 = NULL;
	zval *i0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *p0[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(i0);
	object_init_ex(i0, phalcon_model_message_class_entry);
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_array_fetch_string(r0, v0, "_message", strlen("_message"), PHALCON_NOISY_FETCH TSRMLS_CC);
	Z_ADDREF_P(r0);
	p0[0] = r0;
	PHALCON_ALLOC_ZVAL(r1);
	phalcon_array_fetch_string(r1, v0, "_field", strlen("_field"), PHALCON_NOISY_FETCH TSRMLS_CC);
	Z_ADDREF_P(r1);
	p0[1] = r1;
	PHALCON_ALLOC_ZVAL(r2);
	phalcon_array_fetch_string(r2, v0, "_type", strlen("_type"), PHALCON_NOISY_FETCH TSRMLS_CC);
	Z_ADDREF_P(r2);
	p0[2] = r2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 3, p0, PHALCON_CALL_CHECK);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(i0);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Php_Model_Manager
 *
 * Manages the creation of models inside application and their relationships.
 * Phacon_Model_Manager helps to control the creation of models across a request execution.
 *
 * 
 */

/**
 * Constructor for Php_Model_Manager
     *
 */
PHP_METHOD(Phalcon_Model_Manager, __construct){

	zval *a0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *orig_ptr = a0;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			Z_DELREF_P(orig_ptr);
			ALLOC_ZVAL(a0);
			*a0 = *orig_ptr;
			zval_copy_ctor(a0);
			Z_SET_REFCOUNT_P(a0, 1);
			Z_UNSET_ISREF_P(a0);
		}
	}
	add_next_index_zval(a0, this_ptr);
	add_next_index_string(a0, "autoload", 1);
	Z_ADDREF_P(a0);
	p0[0] = a0;
	PHALCON_CALL_FUNC_PARAMS_NORETURN("spl_autoload_register", 1, p0);
	RETURN_NULL();
}

/**
 * Overwrittes default meta-data manager
 *
 * @param Php_Model_Metadata $metadata
 */
PHP_METHOD(Phalcon_Model_Manager, setMetaData){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_metadata", strlen("_metadata"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns active meta-data manager. If not exists any will be created one
 *
 * @return Php_Model_Metadata
 */
PHP_METHOD(Phalcon_Model_Manager, getMetaData){

	zval *t0 = NULL, *t1 = NULL;
	zval *i0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_metadata", sizeof("_metadata")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_model_metadata_class_entry);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, i0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_metadata", strlen("_metadata"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_metadata", sizeof("_metadata")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets the models directory
 *
 * @param string $modelsDir
 */
PHP_METHOD(Phalcon_Model_Manager, setModelsDir){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_modelsDir", strlen("_modelsDir"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets active models directory
 *
 * @return string
 */
PHP_METHOD(Phalcon_Model_Manager, getModelsDir){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_modelsDir", sizeof("_modelsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Checks wheater given name is a existent model
 *
 * 
 *
 * @param string $modelName
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Manager, isModel){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t0, v0, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (zend_is_true(t1)) {
		RETURN_TRUE;
	} else {
		PHALCON_ALLOC_ZVAL(r0);
		Z_ADDREF_P(v0);
		p0[0] = v0;
		PHALCON_PARAM_BOOL(p0[1], 0);
		PHALCON_CALL_FUNC_PARAMS(r0, "class_exists", 2, p0);
		if (!zend_is_true(r0)) {
			PHALCON_ALLOC_ZVAL(t2);
			phalcon_read_property(t2, this_ptr, "_modelsDir", sizeof("_modelsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t2);
			PHALCON_ALLOC_ZVAL(r1);
			concat_function(r1, t2, v0 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_concat_right(r2, r1, ".php" TSRMLS_CC);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r2);
			v1 = r2;
			if (phalcon_file_exists(v1 TSRMLS_CC) == SUCCESS) {
				PHALCON_ALLOC_ZVAL(r3);
				Z_ADDREF_P(v0);
				p1[0] = v0;
				PHALCON_CALL_METHOD_PARAMS(r3, this_ptr, "initializemodel", 1, p1, PHALCON_CALL_DEFAULT);
				RETURN_ZVAL(r3, 1, 0);
			} else {
				RETURN_FALSE;
			}
		} else {
			RETURN_TRUE;
		}
	}
	RETURN_NULL();
}

/**
 * Initializes a model looking for its file and initializing them
 *
 * @param string $modelName
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Manager, initializeModel){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *i0 = NULL, *i1 = NULL, *i2 = NULL;
	zval *p0[] = { NULL, NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL };
	zend_class_entry *ce0;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t0, v0, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (zend_is_true(t1)) {
		RETURN_TRUE;
	} else {
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_modelsDir", sizeof("_modelsDir")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		PHALCON_ALLOC_ZVAL(r0);
		concat_function(r0, t2, v0 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_concat_right(r1, r0, ".php" TSRMLS_CC);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r1);
		v1 = r1;
		if (phalcon_file_exists(v1 TSRMLS_CC) == SUCCESS) {
			phalcon_require(v1 TSRMLS_CC);
			if (EG(exception) || EG(exit_status) == 255) {
				return;
			}
			PHALCON_ALLOC_ZVAL(r2);
			Z_ADDREF_P(v0);
			p0[0] = v0;
			PHALCON_PARAM_BOOL(p0[1], 0);
			PHALCON_CALL_FUNC_PARAMS(r2, "class_exists", 2, p0);
			if (!zend_is_true(r2)) {
				PHALCON_ALLOC_ZVAL(i0);
				object_init_ex(i0, phalcon_model_exception_class_entry);
				PHALCON_ALLOC_ZVAL(r3);
				phalcon_concat_both(r3,  "Class \"", v0, "\" was not found on model file" TSRMLS_CC);
				Z_ADDREF_P(r3);
				p1[0] = r3;
				PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
				zend_throw_exception_object(i0 TSRMLS_CC);
				Z_ADDREF_P(i0);
				return;
			}
			ce0 = zend_fetch_class(Z_STRVAL_P(v0), Z_STRLEN_P(v0), ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(i1);
			object_init_ex(i1, ce0);
			Z_ADDREF_P(this_ptr);
			p2[0] = this_ptr;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p2, PHALCON_CALL_CHECK);
			t3 = zend_read_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
			if (Z_TYPE_P(t3) != IS_ARRAY) {
				convert_to_array(t3);
			}
			phalcon_array_update(t3, v0, i1 TSRMLS_CC);
			zend_update_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, t3 TSRMLS_CC);
			
			RETURN_TRUE;
		} else {
			PHALCON_ALLOC_ZVAL(i2);
			object_init_ex(i2, phalcon_model_exception_class_entry);
			PHALCON_ALLOC_ZVAL(r4);
			phalcon_concat_both(r4,  "Class file path for model \"", v0, "\" was not found" TSRMLS_CC);
			Z_ADDREF_P(r4);
			p3[0] = r4;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i2, "__construct", 1, p3, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i2 TSRMLS_CC);
			Z_ADDREF_P(i2);
			return;
		}
	}
	RETURN_NULL();
}

/**
 * Gets/Instantiates model from directory
 *
 * 
 *
 * @param string $modelName
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Manager, getModel){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t0, v0, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (!zend_is_true(t1)) {
		Z_ADDREF_P(v0);
		p0[0] = v0;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "initializemodel", 1, p0, PHALCON_CALL_DEFAULT);
	}
	t2 = zend_read_static_property(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t2, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t3) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t3);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Gets the posibly source model name from its class name
 *
 * @param string $modelName
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Manager, getSource){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *r0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_model_manager_class_entry, "_sourceNames", sizeof("_sourceNames")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t0, v0, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (!zend_is_true(t1)) {
		PHALCON_ALLOC_ZVAL(r0);
		Z_ADDREF_P(v0);
		p0[0] = v0;
		PHALCON_CALL_STATIC_PARAMS(r0, "phalcon_utils", "uncamelize", 1, p0);
		t2 = zend_read_static_property(phalcon_model_manager_class_entry, "_sourceNames", sizeof("_sourceNames")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		if (Z_TYPE_P(t2) != IS_ARRAY) {
			convert_to_array(t2);
		}
		phalcon_array_update(t2, v0, r0 TSRMLS_CC);
		zend_update_static_property(phalcon_model_manager_class_entry, "_sourceNames", sizeof("_sourceNames")-1, t2 TSRMLS_CC);
		
	}
	t3 = zend_read_static_property(phalcon_model_manager_class_entry, "_sourceNames", sizeof("_sourceNames")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t4);
	phalcon_array_fetch(t4, t3, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t4) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t4);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t4);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Gets default connection to database. All models by default will use connection returned by this method
 *
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Model_Manager, getConnection){

	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_STATIC(r0, "phalcon_db_pool", "hasdefaultdescriptor");
	if (!zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "There is not defined database connection parameters", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_STATIC(r1, "phalcon_transaction_manager", "isautomatic");
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_STATIC(r3, "phalcon_transaction_manager", "get");
		PHALCON_CALL_METHOD(r2, r3, "getconnection", PHALCON_CALL_DEFAULT);
		RETURN_ZVAL(r2, 1, 0);
	} else {
		PHALCON_ALLOC_ZVAL(r4);
		PHALCON_CALL_STATIC(r4, "phalcon_db_pool", "getconnection");
		RETURN_ZVAL(r4, 1, 0);
	}
	RETURN_NULL();
}

/**
 * Autoload function for model lazy loading
 *
 * @param string $className
 */
PHP_METHOD(Phalcon_Model_Manager, autoload){

	zval *v0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	Z_ADDREF_P(v0);
	p0[0] = v0;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "ismodel", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_NULL();
}

/**
 * Php_Model_MetaData
 *
 * <p>Because Php_Model requires meta-data as field names, data types, primary keys, etc.
 * Php_Model_MetaData recopiles them and store for further querying by Php_Model_Base.
 * This component can also be used to store metadata adapters temporarily or permanently.</p>
 *
 * <p>A standard Php_Model_MetaData can be used to query model attributes:</p>
 *
 * 
 *
 */

/**
 * Php_Model_MetaData constructor
 *
 * @param string $adapter
 */
PHP_METHOD(Phalcon_Model_MetaData, __construct){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };
	zend_class_entry *ce0;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "Memory", 0);
	}
	
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_concat_left(r0, "Phalcon_Model_MetaData_", v0 TSRMLS_CC);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	Z_ADDREF_P(v1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r1, "class_exists", 1, p0);
	if (zend_is_true(r1)) {
		ce0 = zend_fetch_class(Z_STRVAL_P(v1), Z_STRLEN_P(v1), ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, ce0);
		PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, i0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_adapter", strlen("_adapter"), copy TSRMLS_CC);
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Model_MetaData, _initializeMetaData){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL, *v10 = NULL, *v11 = NULL, *v12 = NULL, *v13 = NULL;
	zval *v14 = NULL, *v15 = NULL, *v16 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL, *t11 = NULL, *t12 = NULL, *t13 = NULL;
	zval *t14 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL;
	zval *i0 = NULL;
	zval *a0 = NULL, *a1 = NULL, *a2 = NULL, *a3 = NULL, *a4 = NULL, *a5 = NULL, *a6 = NULL;
	zval *ac0 = NULL;
	zval *p2[] = { NULL, NULL }, *p3[] = { NULL, NULL }, *p5[] = { NULL }, *p4[] = { NULL }, *p6[] = { NULL, NULL }, *p7[] = { NULL, NULL }, *p8[] = { NULL, NULL }, *p9[] = { NULL, NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &v0, &v1, &v2) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t0, v2, PHALCON_SILENT_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, v1, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (!zend_is_true(t1)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, v0, "getconnection", PHALCON_CALL_DEFAULT);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r0);
		v3 = r0;
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_METHOD(r1, v0, "isview", PHALCON_CALL_DEFAULT);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r1);
		v4 = r1;
		if (!zend_is_true(v4)) {
			PHALCON_ALLOC_ZVAL(r2);
			Z_ADDREF_P(v1);
			p2[0] = v1;
			Z_ADDREF_P(v2);
			p2[1] = v2;
			PHALCON_CALL_METHOD_PARAMS(r2, v3, "tableexists", 2, p2, PHALCON_CALL_DEFAULT);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r2);
			v5 = r2;
		} else {
			PHALCON_ALLOC_ZVAL(r3);
			Z_ADDREF_P(v1);
			p3[0] = v1;
			Z_ADDREF_P(v2);
			p3[1] = v2;
			PHALCON_CALL_METHOD_PARAMS(r3, v3, "viewexists", 2, p3, PHALCON_CALL_DEFAULT);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r3);
			v5 = r3;
		}
		if (!zend_is_true(v5)) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_model_exception_class_entry);
			PHALCON_ALLOC_ZVAL(r6);
			phalcon_concat_left(r6, "Table \"", v2 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r5);
			phalcon_concat_vboth(r5, r6, "\".\"", v1 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r7);
			p5[0] = v0;
			PHALCON_CALL_FUNC_PARAMS(r7, "get_class", 1, p5);
			PHALCON_ALLOC_ZVAL(r4);
			phalcon_concat_vboth(r4, r5, "\" doesn't exists on database when dumping information for ", r7 TSRMLS_CC);
			Z_ADDREF_P(r4);
			p4[0] = r4;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p4, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		} else {
			if (zend_is_true(v4)) {
				PHALCON_ALLOC_ZVAL(r8);
				Z_ADDREF_P(v1);
				p6[0] = v1;
				Z_ADDREF_P(v2);
				p6[1] = v2;
				PHALCON_CALL_METHOD_PARAMS(r8, v3, "describeview", 2, p6, PHALCON_CALL_DEFAULT);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r8);
				v6 = r8;
			} else {
				PHALCON_ALLOC_ZVAL(r9);
				Z_ADDREF_P(v1);
				p7[0] = v1;
				Z_ADDREF_P(v2);
				p7[1] = v2;
				PHALCON_CALL_METHOD_PARAMS(r9, v3, "describetable", 2, p7, PHALCON_CALL_DEFAULT);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r9);
				v6 = r9;
			}
			PHALCON_ALLOC_ZVAL(a0);
			array_init(a0);
			if (v7) {
				Z_DELREF_P(v7);
				if (!Z_REFCOUNT_P(v7)) {
					FREE_ZVAL(v7);
				}
			}
			Z_ADDREF_P(a0);
			v7 = a0;
			PHALCON_ALLOC_ZVAL(a1);
			array_init(a1);
			if (v8) {
				Z_DELREF_P(v8);
				if (!Z_REFCOUNT_P(v8)) {
					FREE_ZVAL(v8);
				}
			}
			Z_ADDREF_P(a1);
			v8 = a1;
			PHALCON_ALLOC_ZVAL(a2);
			array_init(a2);
			if (v9) {
				Z_DELREF_P(v9);
				if (!Z_REFCOUNT_P(v9)) {
					FREE_ZVAL(v9);
				}
			}
			Z_ADDREF_P(a2);
			v9 = a2;
			PHALCON_ALLOC_ZVAL(a3);
			array_init(a3);
			if (v10) {
				Z_DELREF_P(v10);
				if (!Z_REFCOUNT_P(v10)) {
					FREE_ZVAL(v10);
				}
			}
			Z_ADDREF_P(a3);
			v10 = a3;
			PHALCON_ALLOC_ZVAL(a4);
			array_init(a4);
			if (v11) {
				Z_DELREF_P(v11);
				if (!Z_REFCOUNT_P(v11)) {
					FREE_ZVAL(v11);
				}
			}
			Z_ADDREF_P(a4);
			v11 = a4;
			PHALCON_ALLOC_ZVAL(a5);
			array_init(a5);
			if (v12) {
				Z_DELREF_P(v12);
				if (!Z_REFCOUNT_P(v12)) {
					FREE_ZVAL(v12);
				}
			}
			Z_ADDREF_P(a5);
			v12 = a5;
			FOREACH_V(v6, ac0, fes30, fee30, ah0, hp0, v13)
				if (!r10) {
					PHALCON_ALLOC_ZVAL(r10);
				} else {
					if (Z_REFCOUNT_P(r10) > 1) {
						{
							zval *orig_ptr = r10;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r10);
								*r10 = *orig_ptr;
								zval_copy_ctor(r10);
								Z_SET_REFCOUNT_P(r10, 1);
								Z_UNSET_ISREF_P(r10);
							}
						}
					} else {
						FREE_ZVAL(r10);
						PHALCON_ALLOC_ZVAL(r10);
					}
				}
				phalcon_array_fetch_string(r10, v13, "Field", strlen("Field"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v14) {
					Z_DELREF_P(v14);
					if (!Z_REFCOUNT_P(v14)) {
						FREE_ZVAL(v14);
					}
				}
				Z_ADDREF_P(r10);
				v14 = r10;
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, v14, 1, 0);
					Z_SET_REFCOUNT_P(copy, 1);
					{
						zval *orig_ptr = v7;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(v7);
							*v7 = *orig_ptr;
							zval_copy_ctor(v7);
							Z_SET_REFCOUNT_P(v7, 1);
							Z_UNSET_ISREF_P(v7);
						}
					}
					phalcon_array_append(v7, copy TSRMLS_CC);
				}
				if (!r11) {
					PHALCON_ALLOC_ZVAL(r11);
				} else {
					if (Z_REFCOUNT_P(r11) > 1) {
						{
							zval *orig_ptr = r11;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r11);
								*r11 = *orig_ptr;
								zval_copy_ctor(r11);
								Z_SET_REFCOUNT_P(r11, 1);
								Z_UNSET_ISREF_P(r11);
							}
						}
					} else {
						FREE_ZVAL(r11);
						PHALCON_ALLOC_ZVAL(r11);
					}
				}
				phalcon_array_fetch_string(r11, v13, "Key", strlen("Key"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (phalcon_compare_strict_string(r11, "PRI")) {
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, v14, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v8;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v8);
								*v8 = *orig_ptr;
								zval_copy_ctor(v8);
								Z_SET_REFCOUNT_P(v8, 1);
								Z_UNSET_ISREF_P(v8);
							}
						}
						phalcon_array_append(v8, copy TSRMLS_CC);
					}
				} else {
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, v14, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v9;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v9);
								*v9 = *orig_ptr;
								zval_copy_ctor(v9);
								Z_SET_REFCOUNT_P(v9, 1);
								Z_UNSET_ISREF_P(v9);
							}
						}
						phalcon_array_append(v9, copy TSRMLS_CC);
					}
				}
				if (!r12) {
					PHALCON_ALLOC_ZVAL(r12);
				} else {
					if (Z_REFCOUNT_P(r12) > 1) {
						{
							zval *orig_ptr = r12;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r12);
								*r12 = *orig_ptr;
								zval_copy_ctor(r12);
								Z_SET_REFCOUNT_P(r12, 1);
								Z_UNSET_ISREF_P(r12);
							}
						}
					} else {
						FREE_ZVAL(r12);
						PHALCON_ALLOC_ZVAL(r12);
					}
				}
				phalcon_array_fetch_string(r12, v13, "Type", strlen("Type"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v15) {
					Z_DELREF_P(v15);
					if (!Z_REFCOUNT_P(v15)) {
						FREE_ZVAL(v15);
					}
				}
				Z_ADDREF_P(r12);
				v15 = r12;
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, v15, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					{
						zval *orig_ptr = v12;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(v12);
							*v12 = *orig_ptr;
							zval_copy_ctor(v12);
							Z_SET_REFCOUNT_P(v12, 1);
							Z_UNSET_ISREF_P(v12);
						}
					}
					phalcon_array_update(v12, v14, copy TSRMLS_CC);
				}
				if (!r13) {
					PHALCON_ALLOC_ZVAL(r13);
				} else {
					if (Z_REFCOUNT_P(r13) > 1) {
						{
							zval *orig_ptr = r13;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r13);
								*r13 = *orig_ptr;
								zval_copy_ctor(r13);
								Z_SET_REFCOUNT_P(r13, 1);
								Z_UNSET_ISREF_P(r13);
							}
						}
					} else {
						FREE_ZVAL(r13);
						PHALCON_ALLOC_ZVAL(r13);
					}
				}
				p8[0] = v15;
				if (!p8[1]) {
					PHALCON_ALLOC_ZVAL(p8[1]);
				} else {
					if (Z_REFCOUNT_P(p8[1]) > 1) {
						SEPARATE_ZVAL(&p8[1]);
					} else {
						if (Z_TYPE_P(p8[1]) != IS_STRING) {
							FREE_ZVAL(p8[1]);
							PHALCON_ALLOC_ZVAL(p8[1]);
						}
					}
				}
				ZVAL_STRING(p8[1], "int", 1);
				PHALCON_CALL_FUNC_PARAMS(r13, "strpos", 2, p8);
				PHALCON_INIT_FALSE(t3);
				PHALCON_NOT_IDENTICAL_FUNCTION(r14, r13, t3);
				if (zend_is_true(r14)) {
					PHALCON_INIT_TRUE(t4);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, t4, 1, 0);
						Z_SET_REFCOUNT_P(copy, 0);
						{
							zval *orig_ptr = v10;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v10);
								*v10 = *orig_ptr;
								zval_copy_ctor(v10);
								Z_SET_REFCOUNT_P(v10, 1);
								Z_UNSET_ISREF_P(v10);
							}
						}
						phalcon_array_update(v10, v14, copy TSRMLS_CC);
					}
				} else {
					if (!r15) {
						PHALCON_ALLOC_ZVAL(r15);
					} else {
						if (Z_REFCOUNT_P(r15) > 1) {
							{
								zval *orig_ptr = r15;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r15);
									*r15 = *orig_ptr;
									zval_copy_ctor(r15);
									Z_SET_REFCOUNT_P(r15, 1);
									Z_UNSET_ISREF_P(r15);
								}
							}
						} else {
							FREE_ZVAL(r15);
							PHALCON_ALLOC_ZVAL(r15);
						}
					}
					p9[0] = v15;
					if (!p9[1]) {
						PHALCON_ALLOC_ZVAL(p9[1]);
					} else {
						if (Z_REFCOUNT_P(p9[1]) > 1) {
							SEPARATE_ZVAL(&p9[1]);
						} else {
							if (Z_TYPE_P(p9[1]) != IS_STRING) {
								FREE_ZVAL(p9[1]);
								PHALCON_ALLOC_ZVAL(p9[1]);
							}
						}
					}
					ZVAL_STRING(p9[1], "decimal", 1);
					PHALCON_CALL_FUNC_PARAMS(r15, "strpos", 2, p9);
					PHALCON_INIT_FALSE(t5);
					PHALCON_NOT_IDENTICAL_FUNCTION(r16, r15, t5);
					if (zend_is_true(r16)) {
						PHALCON_INIT_TRUE(t6);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, t6, 1, 0);
							Z_SET_REFCOUNT_P(copy, 0);
							{
								zval *orig_ptr = v10;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(v10);
									*v10 = *orig_ptr;
									zval_copy_ctor(v10);
									Z_SET_REFCOUNT_P(v10, 1);
									Z_UNSET_ISREF_P(v10);
								}
							}
							phalcon_array_update(v10, v14, copy TSRMLS_CC);
						}
					}
				}
				if (!r17) {
					PHALCON_ALLOC_ZVAL(r17);
				} else {
					if (Z_REFCOUNT_P(r17) > 1) {
						{
							zval *orig_ptr = r17;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r17);
								*r17 = *orig_ptr;
								zval_copy_ctor(r17);
								Z_SET_REFCOUNT_P(r17, 1);
								Z_UNSET_ISREF_P(r17);
							}
						}
					} else {
						FREE_ZVAL(r17);
						PHALCON_ALLOC_ZVAL(r17);
					}
				}
				phalcon_array_fetch_string(r17, v13, "Null", strlen("Null"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (phalcon_compare_strict_string(r17, "NO")) {
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, v14, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v11;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v11);
								*v11 = *orig_ptr;
								zval_copy_ctor(v11);
								Z_SET_REFCOUNT_P(v11, 1);
								Z_UNSET_ISREF_P(v11);
							}
						}
						phalcon_array_append(v11, copy TSRMLS_CC);
					}
				}
			END_FOREACH(ac0, fes30, fee30, ah0, hp0);
			PHALCON_ALLOC_ZVAL(a6);
			array_init(a6);
			if (v16) {
				Z_DELREF_P(v16);
				if (!Z_REFCOUNT_P(v16)) {
					FREE_ZVAL(v16);
				}
			}
			Z_ADDREF_P(a6);
			v16 = a6;
			PHALCON_GET_CLASS_CONSTANT(t7, phalcon_model_metadata_class_entry, "MODELS_ATTRIBUTES");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t7, v7 TSRMLS_CC);
			PHALCON_GET_CLASS_CONSTANT(t8, phalcon_model_metadata_class_entry, "MODELS_PRIMARY_KEY");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t8, v8 TSRMLS_CC);
			PHALCON_GET_CLASS_CONSTANT(t9, phalcon_model_metadata_class_entry, "MODELS_NON_PRIMARY_KEY");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t9, v9 TSRMLS_CC);
			PHALCON_GET_CLASS_CONSTANT(t10, phalcon_model_metadata_class_entry, "MODELS_NOT_NULL");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t10, v11 TSRMLS_CC);
			PHALCON_GET_CLASS_CONSTANT(t11, phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t11, v12 TSRMLS_CC);
			PHALCON_GET_CLASS_CONSTANT(t12, phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE_NUMERIC");
			{
				zval *orig_ptr = v16;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v16);
					*v16 = *orig_ptr;
					zval_copy_ctor(v16);
					Z_SET_REFCOUNT_P(v16, 1);
					Z_UNSET_ISREF_P(v16);
				}
			}
			phalcon_array_update(v16, t12, v10 TSRMLS_CC);
			t13 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
			if (Z_TYPE_P(t13) != IS_ARRAY) {
				convert_to_array(t13);
			}
			if (Z_TYPE_P(t13) == IS_ARRAY) {
				PHALCON_ALLOC_ZVAL(t14);
				phalcon_array_fetch(t14, t13, v2, PHALCON_SILENT_FETCH TSRMLS_CC);
			}
			if (Z_TYPE_P(t14) != IS_ARRAY) {
				convert_to_array(t14);
				phalcon_array_update(t13, v2, t14 TSRMLS_CC);
				Z_ADDREF_P(t14);
			}
			phalcon_array_update(t14, v1, v16 TSRMLS_CC);
			zend_update_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, t13 TSRMLS_CC);
			
		}
	}
	RETURN_NULL();
}

/**
 * Returns table attributes name (fields)
 *
     * @paramPhp_Model_Base $model
 * @return array
 */
PHP_METHOD(Phalcon_Model_MetaData, getAttributes){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_ATTRIBUTES");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns table attributes which are part of primary key
 *
 * @paramPhp_Model_Base $model
 * @returnarray
 */
PHP_METHOD(Phalcon_Model_MetaData, getPrimaryKeyAttributes){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_PRIMARY_KEY");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns table attributes which are non part of primary key
 *
     * @paramPhp_Model_Base $model
 * @return array
 */
PHP_METHOD(Phalcon_Model_MetaData, getNonPrimaryKeyAttributes){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_NON_PRIMARY_KEY");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns not null attributes
 *
     * @paramPhp_Model_Base $model
 * @return array
 */
PHP_METHOD(Phalcon_Model_MetaData, getNotNullAttributes){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_NOT_NULL");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns attributes and their data types
 *
     * @paramPhp_Model_Base $model
 * @return array
 */
PHP_METHOD(Phalcon_Model_MetaData, getDataTypes){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Returns attributes which types are numerical
 *
     * @param  Php_Model_Base $model
 * @return array
 */
PHP_METHOD(Phalcon_Model_MetaData, getDataTypesNumeric){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p2[] = { NULL, NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, v0, "getsource", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, v0, "getschema", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	Z_ADDREF_P(v0);
	p2[0] = v0;
	Z_ADDREF_P(v1);
	p2[1] = v1;
	Z_ADDREF_P(v2);
	p2[2] = v2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_initializemetadata", 3, p2, PHALCON_CALL_DEFAULT);
	t0 = zend_read_static_property(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_array_fetch(t3, t0, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_array_fetch(t2, t3, v1, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE_NUMERIC");
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t2, t4, PHALCON_NOISY_FETCH TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Model_Row
 *
 * This component allows to Php_Model_Base returns grouped resultsets.
 */

PHP_METHOD(Phalcon_Model_Row, __construct){

	zval *a0 = NULL;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_property(phalcon_model_row_class_entry, this_ptr, "_columns", strlen("_columns"), a0 TSRMLS_CC);
}

/**
 * Overwrittes default connection
 *
 * @param Php_Db $connection
 */
PHP_METHOD(Phalcon_Model_Row, setConnection){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_connection", strlen("_connection"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns default connection
 *
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Model_Row, getConnection){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Assigns values to a row from an array returning a new row
 *
 *
 *
 * @param array $result
 * @return Php_Model $result
 */
PHP_METHOD(Phalcon_Model_Row, dumpResult){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	zval *s0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	phalcon_clone(&s0, this_ptr TSRMLS_CC);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(s0);
	v1 = s0;
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_columns", sizeof("_columns")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	p0[0] = t0;
	PHALCON_CALL_FUNC_PARAMS(r0, "count", 1, p0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 0);
	PHALCON_EQUAL_FUNCTION(r1, r0, t1);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(a0);
		v2 = a0;
		FOREACH_KV(v0, ac0, fes31, fee31, ah0, hp0, v4, v3)
			PHALCON_INIT_TRUE(t2);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				{
					zval *orig_ptr = v2;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v2);
						*v2 = *orig_ptr;
						zval_copy_ctor(v2);
						Z_SET_REFCOUNT_P(v2, 1);
						Z_UNSET_ISREF_P(v2);
					}
				}
				phalcon_array_update(v2, v4, copy TSRMLS_CC);
			}
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v3, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(v1, Z_STRVAL_P(v4), Z_STRLEN_P(v4), copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes31, fee31, ah0, hp0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(v1, "_columns", strlen("_columns"), copy TSRMLS_CC);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_columns", strlen("_columns"), copy TSRMLS_CC);
		}
	} else {
		FOREACH_KV(v0, ac1, fes32, fee32, ah1, hp1, v4, v3)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v3, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(v1, Z_STRVAL_P(v4), Z_STRLEN_P(v4), copy TSRMLS_CC);
			}
		END_FOREACH(ac1, fes32, fee32, ah1, hp1);
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_columns", sizeof("_columns")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t3, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(v1, "_columns", strlen("_columns"), copy TSRMLS_CC);
		}
	}
	if (Z_TYPE_P(v1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Reads an attribute value by its name
 *
 * 
 *
 * @param string $attribute
 * @return mixed
 */
PHP_METHOD(Phalcon_Model_Row, readAttribute){

	zval *v0 = NULL;
	zval *t0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property_zval(t0, this_ptr, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Magic method sleep
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Row, sleep){

	zval *a0 = NULL;

	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	add_next_index_string(a0, "_columns", 1);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(a0);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Php_Model_Base
 *
 * <p>Php_Model connects business objects and database tables to create
 * a persistable domain model where logic and data are presented in one wrapping.
 * It‘s an implementation of the object- relational mapping (ORM).</p>
 *
 * <p>A model represents the information (data) of the application and the rules to manipulate that data.
 * Models are primarily used for managing the rules of interaction with a corresponding database table.
 * In most cases, each table in your database will correspond to one model in your application.
 * The bulk of your application’s business logic will be concentrated in the models.</p>
 *
 * <p>Php_Model is the first ORM written in C-language for PHP, giving to developers high performance
 * when interact with databases while is also easy to use.</p>
 *
 * 
 *
 */

/**
 * Php_Model_Base constructor
 *
 * @param Php_Model_Manager $manager
 *
 */
PHP_METHOD(Phalcon_Model_Base, __construct){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_NULL(v0);
	}
	
	if (!zend_is_true(v0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_STATIC(r1, "phalcon_controller_front", "getinstance");
		PHALCON_CALL_METHOD(r0, r1, "getmodelcomponent", PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_manager", strlen("_manager"), copy TSRMLS_CC);
		}
	} else {
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_manager", strlen("_manager"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, "initialize", 1);
	if (phalcon_method_exists(this_ptr, t0 TSRMLS_CC) == SUCCESS) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "initialize", PHALCON_CALL_DEFAULT);
	}
	RETURN_NULL();
}

/**
 * Internal method for make a connection. Automatically dumps mapped table meta-data
 *
 */
PHP_METHOD(Phalcon_Model_Base, _connect){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_CALL_METHOD(r0, t1, "getconnection", PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_connection", strlen("_connection"), copy TSRMLS_CC);
		}
	}
	PHALCON_CALL_METHOD_NORETURN(this_ptr, "dump", PHALCON_CALL_DEFAULT);
	RETURN_NULL();
}

/**
 * Internal method for get table mapped attributes
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getAttributes){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getattributes", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Internal method for get attributes which are part of table mapped primary key
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getPrimaryKeyAttributes){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getprimarykeyattributes", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Internal method for get attributes which are not part of table mapped primary key
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getNonPrimaryKeyAttributes){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getnonprimarykeyattributes", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Internal method for get attributes which are part of table mapped primary key
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getNotNullAttributes){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getnotnullattributes", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Internal method for get numerical-typed attributes
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getDataTypesNumeric){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getdatatypesnumeric", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Internal method for get data-types attributes
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, _getDataTypes){

	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r1, t0, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getdatatypes", 1, p0, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

/**
 * Dumps mapped table meta-data
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Base, dump){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	int eval_int;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dumped", sizeof("_dumped")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		RETURN_FALSE;
	}
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumped", strlen("_dumped"), 1 TSRMLS_CC);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	PHALCON_CALL_METHOD(r1, t1, "getmetadata", PHALCON_CALL_DEFAULT);
	Z_ADDREF_P(this_ptr);
	p0[0] = this_ptr;
	PHALCON_CALL_METHOD_PARAMS(r0, r1, "getattributes", 1, p0, PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	FOREACH_V(v0, ac0, fes33, fee33, ah0, hp0, v1)
		eval_int = phalcon_isset_property_zval(this_ptr, v1 TSRMLS_CC);
		if (!eval_int) {
			if (!t2) {
				PHALCON_ALLOC_ZVAL(t2);
			} else {
				if (Z_REFCOUNT_P(t2) > 1) {
					SEPARATE_ZVAL(&t2);
				} else {
					if (Z_TYPE_P(t2) != IS_STRING) {
						FREE_ZVAL(t2);
						PHALCON_ALLOC_ZVAL(t2);
					}
				}
			}
			ZVAL_STRING(t2, "", 1);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, Z_STRVAL_P(v1), Z_STRLEN_P(v1), copy TSRMLS_CC);
			}
		}
	END_FOREACH(ac0, fes33, fee33, ah0, hp0);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 0 TSRMLS_CC);
	if (Z_TYPE_P(this_ptr) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(this_ptr);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(this_ptr);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Model_Base, _createSQLSelectMulti){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL, *r37 = NULL, *r38 = NULL, *r39 = NULL, *r40 = NULL, *r41 = NULL;
	zval *r42 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *a0 = NULL;
	zval *p1[] = { NULL, NULL }, *p4[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL }, *p7[] = { NULL }, *p8[] = { NULL, NULL }, *p9[] = { NULL }, *p10[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_STRING(v1, "SELECT ", 0);
	eval_int = phalcon_array_isset_string(v0, "columns", strlen("columns")+1);
	if (eval_int) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "clear", PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, v0, "columns", strlen("columns"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r1, v1, r0);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r1);
		v1 = r1;
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], ", ", 1);
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_METHOD(r3, this_ptr, "_getattributes", PHALCON_CALL_DEFAULT);
		p1[1] = r3;
		PHALCON_CALL_FUNC_PARAMS(r2, "join", 2, p1);
		PHALCON_CONCAT_FUNCTION(r4, v1, r2);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r4);
		v1 = r4;
	}
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_concat_left(r6, " FROM ", t1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_vboth(r5, r6, ".", t2 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r7, v1, r5);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r7);
		v1 = r7;
	} else {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		PHALCON_ALLOC_ZVAL(r8);
		phalcon_concat_left(r8, " FROM ", t3 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r9, v1, r8);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r9);
		v1 = r9;
	}
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "n", 0);
	eval_int = phalcon_array_isset_string(v0, "conditions", strlen("conditions")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r10);
		phalcon_array_fetch_string(r10, v0, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r11);
		phalcon_concat_both(r11,  " WHERE ", r10, " " TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r12, v1, r11);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r12);
		v1 = r12;
	} else {
		PHALCON_ALLOC_ZVAL(r13);
		PHALCON_CALL_METHOD(r13, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r13);
		v3 = r13;
		eval_int = phalcon_array_isset_long(v3, 0);
		if (!eval_int) {
			PHALCON_ALLOC_ZVAL(t4);
			phalcon_read_property(t4, this_ptr, "_isView", sizeof("_isView")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t4);
			if (zend_is_true(t4)) {
				PHALCON_ALLOC_ZVAL(t5);
				ZVAL_STRING(t5, "id", 1);
				{
					zval *orig_ptr = v3;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v3);
						*v3 = *orig_ptr;
						zval_copy_ctor(v3);
						Z_SET_REFCOUNT_P(v3, 1);
						Z_UNSET_ISREF_P(v3);
					}
				}
				phalcon_array_update_long(v3, 0, t5 TSRMLS_CC);
			}
		}
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r14);
			PHALCON_ALLOC_ZVAL(r15);
			phalcon_array_fetch_long(r15, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r15);
			p4[0] = r15;
			PHALCON_CALL_FUNC_PARAMS(r14, "is_numeric", 1, p4);
			if (zend_is_true(r14)) {
				eval_int = phalcon_array_isset_long(v3, 0);
				if (eval_int) {
					PHALCON_ALLOC_ZVAL(r17);
					phalcon_array_fetch_long(r17, v3, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					PHALCON_ALLOC_ZVAL(r18);
					PHALCON_ALLOC_ZVAL(t6);
					phalcon_read_property(t6, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
					zval_copy_ctor(t6);
					PHALCON_ALLOC_ZVAL(r19);
					phalcon_array_fetch_long(r19, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r19);
					p5[0] = r19;
					PHALCON_CALL_METHOD_PARAMS(r18, t6, "addquotes", 1, p5, PHALCON_CALL_DEFAULT);
					PHALCON_ALLOC_ZVAL(r16);
					phalcon_concat_vboth(r16, r17, " = ", r18 TSRMLS_CC);
					{
						zval *orig_ptr = v0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							ALLOC_ZVAL(v0);
							*v0 = *orig_ptr;
							zval_copy_ctor(v0);
							Z_SET_REFCOUNT_P(v0, 1);
							Z_UNSET_ISREF_P(v0);
						}
					}
					phalcon_array_update_string(v0, "conditions", strlen("conditions"), r16 TSRMLS_CC);
					PHALCON_ALLOC_ZVAL(v2);
					ZVAL_STRING(v2, "1", 0);
				} else {
					PHALCON_ALLOC_ZVAL(i0);
					object_init_ex(i0, phalcon_model_exception_class_entry);
					PHALCON_ALLOC_ZVAL(p6[0]);
					ZVAL_STRING(p6[0], "Has not been defined a primary key for source related to this model", 1);
					PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p6, PHALCON_CALL_CHECK);
					zend_throw_exception_object(i0 TSRMLS_CC);
					Z_ADDREF_P(i0);
					return;
				}
			} else {
				PHALCON_ALLOC_ZVAL(r20);
				phalcon_array_fetch_long(r20, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(t7);
				ZVAL_STRING(t7, "", 1);
				PHALCON_IDENTICAL_FUNCTION(r21, r20, t7);
				if (zend_is_true(r21)) {
					eval_int = phalcon_array_isset_long(v3, 0);
					if (eval_int) {
						PHALCON_ALLOC_ZVAL(r22);
						phalcon_array_fetch_long(r22, v3, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
						PHALCON_ALLOC_ZVAL(r23);
						phalcon_concat_right(r23, r22, " = ''" TSRMLS_CC);
						{
							zval *orig_ptr = v0;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								ALLOC_ZVAL(v0);
								*v0 = *orig_ptr;
								zval_copy_ctor(v0);
								Z_SET_REFCOUNT_P(v0, 1);
								Z_UNSET_ISREF_P(v0);
							}
						}
						phalcon_array_update_string(v0, "conditions", strlen("conditions"), r23 TSRMLS_CC);
					} else {
						PHALCON_ALLOC_ZVAL(i1);
						object_init_ex(i1, phalcon_model_exception_class_entry);
						PHALCON_ALLOC_ZVAL(p7[0]);
						ZVAL_STRING(p7[0], "Has not been defined a primary key for source related to this model", 1);
						PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p7, PHALCON_CALL_CHECK);
						zend_throw_exception_object(i1 TSRMLS_CC);
						Z_ADDREF_P(i1);
						return;
					}
				} else {
					PHALCON_ALLOC_ZVAL(r24);
					phalcon_array_fetch_long(r24, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					{
						zval *orig_ptr = v0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							ALLOC_ZVAL(v0);
							*v0 = *orig_ptr;
							zval_copy_ctor(v0);
							Z_SET_REFCOUNT_P(v0, 1);
							Z_UNSET_ISREF_P(v0);
						}
					}
					phalcon_array_update_string(v0, "conditions", strlen("conditions"), r24 TSRMLS_CC);
				}
				PHALCON_ALLOC_ZVAL(v2);
				ZVAL_STRING(v2, "n", 0);
			}
		}
		eval_int = phalcon_array_isset_string(v0, "conditions", strlen("conditions")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r25);
			phalcon_array_fetch_string(r25, v0, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r26);
			phalcon_concat_left(r26, " WHERE ", r25 TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r27, v1, r26);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r27);
			v1 = r27;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "group", strlen("group")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r28);
		phalcon_array_fetch_string(r28, v0, "group", strlen("group"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r28)) {
			PHALCON_ALLOC_ZVAL(r29);
			phalcon_array_fetch_string(r29, v0, "group", strlen("group"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r30);
			phalcon_concat_left(r30, " GROUP BY ", r29 TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r31, v1, r30);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r31);
			v1 = r31;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "order", strlen("order")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r32);
		phalcon_array_fetch_string(r32, v0, "order", strlen("order"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r32)) {
			PHALCON_ALLOC_ZVAL(r33);
			phalcon_array_fetch_string(r33, v0, "order", strlen("order"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r34);
			phalcon_concat_left(r34, " ORDER BY ", r33 TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r35, v1, r34);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r35);
			v1 = r35;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "limit", strlen("limit")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r36);
		phalcon_array_fetch_string(r36, v0, "limit", strlen("limit"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r36)) {
			PHALCON_ALLOC_ZVAL(r37);
			PHALCON_ALLOC_ZVAL(t8);
			phalcon_read_property(t8, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t8);
			Z_ADDREF_P(v1);
			p8[0] = v1;
			PHALCON_ALLOC_ZVAL(r38);
			phalcon_array_fetch_string(r38, v0, "limit", strlen("limit"), PHALCON_NOISY_FETCH TSRMLS_CC);
			Z_ADDREF_P(r38);
			p8[1] = r38;
			PHALCON_CALL_METHOD_PARAMS(r37, t8, "limit", 2, p8, PHALCON_CALL_DEFAULT);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r37);
			v1 = r37;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "for_update", strlen("for_update")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r39);
		phalcon_array_fetch_string(r39, v0, "for_update", strlen("for_update"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r39)) {
			PHALCON_ALLOC_ZVAL(r40);
			PHALCON_ALLOC_ZVAL(t9);
			phalcon_read_property(t9, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t9);
			Z_ADDREF_P(v1);
			p9[0] = v1;
			PHALCON_CALL_METHOD_PARAMS(r40, t9, "forupdate", 1, p9, PHALCON_CALL_DEFAULT);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r40);
			v1 = r40;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "shared_lock", strlen("shared_lock")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r41);
		phalcon_array_fetch_string(r41, v0, "shared_lock", strlen("shared_lock"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r41)) {
			PHALCON_ALLOC_ZVAL(r42);
			PHALCON_ALLOC_ZVAL(t10);
			phalcon_read_property(t10, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t10);
			Z_ADDREF_P(v1);
			p10[0] = v1;
			PHALCON_CALL_METHOD_PARAMS(r42, t10, "sharedlock", 1, p10, PHALCON_CALL_DEFAULT);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r42);
			v1 = r42;
		}
	}
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v2, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_assoc_zval(a0, "return", copy);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		{
			zval *orig_ptr = a0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(a0);
				*a0 = *orig_ptr;
				zval_copy_ctor(a0);
				Z_SET_REFCOUNT_P(a0, 1);
				Z_UNSET_ISREF_P(a0);
			}
		}
		add_assoc_zval(a0, "sql", copy);
	}
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(a0);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

PHP_METHOD(Phalcon_Model_Base, _createSQLSelectOne){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL, *r37 = NULL, *r38 = NULL, *r39 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *p1[] = { NULL }, *p2[] = { NULL, NULL }, *p3[] = { NULL }, *p4[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	if (Z_TYPE_P(v1) == IS_ARRAY) { 
		eval_int = phalcon_array_isset_string(v1, "conditions", strlen("conditions")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r0);
			phalcon_array_fetch_string(r0, v1, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (zend_is_true(r0)) {
				PHALCON_ALLOC_ZVAL(r1);
				phalcon_array_fetch_string(r1, v1, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r2);
				phalcon_concat_both(r2,  " WHERE ", r1, " " TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r3, v0, r2);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r3);
				v0 = r3;
			}
		} else {
			PHALCON_ALLOC_ZVAL(r4);
			PHALCON_CALL_METHOD(r4, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r4);
			v2 = r4;
			eval_int = phalcon_array_isset_long(v1, 0);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(r5);
				PHALCON_ALLOC_ZVAL(r6);
				phalcon_array_fetch_long(r6, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				Z_ADDREF_P(r6);
				p1[0] = r6;
				PHALCON_CALL_FUNC_PARAMS(r5, "is_numeric", 1, p1);
				if (zend_is_true(r5)) {
					PHALCON_ALLOC_ZVAL(r8);
					phalcon_array_fetch_long(r8, v2, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					PHALCON_ALLOC_ZVAL(r9);
					phalcon_array_fetch_long(r9, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					PHALCON_ALLOC_ZVAL(r7);
					phalcon_concat_vboth(r7, r8, " = '", r9 TSRMLS_CC);
					PHALCON_ALLOC_ZVAL(r10);
					phalcon_concat_right(r10, r7, "'" TSRMLS_CC);
					{
						zval *orig_ptr = v1;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							ALLOC_ZVAL(v1);
							*v1 = *orig_ptr;
							zval_copy_ctor(v1);
							Z_SET_REFCOUNT_P(v1, 1);
							Z_UNSET_ISREF_P(v1);
						}
					}
					phalcon_array_update_string(v1, "conditions", strlen("conditions"), r10 TSRMLS_CC);
				} else {
					PHALCON_ALLOC_ZVAL(r11);
					phalcon_array_fetch_long(r11, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					if (phalcon_compare_strict_string(r11, "")) {
						PHALCON_ALLOC_ZVAL(r12);
						phalcon_array_fetch_long(r12, v2, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
						PHALCON_ALLOC_ZVAL(r13);
						phalcon_concat_right(r13, r12, " = ''" TSRMLS_CC);
						{
							zval *orig_ptr = v1;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								ALLOC_ZVAL(v1);
								*v1 = *orig_ptr;
								zval_copy_ctor(v1);
								Z_SET_REFCOUNT_P(v1, 1);
								Z_UNSET_ISREF_P(v1);
							}
						}
						phalcon_array_update_string(v1, "conditions", strlen("conditions"), r13 TSRMLS_CC);
					} else {
						PHALCON_ALLOC_ZVAL(r14);
						phalcon_array_fetch_long(r14, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
						{
							zval *orig_ptr = v1;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								ALLOC_ZVAL(v1);
								*v1 = *orig_ptr;
								zval_copy_ctor(v1);
								Z_SET_REFCOUNT_P(v1, 1);
								Z_UNSET_ISREF_P(v1);
							}
						}
						phalcon_array_update_string(v1, "conditions", strlen("conditions"), r14 TSRMLS_CC);
					}
				}
			}
			eval_int = phalcon_array_isset_string(v1, "conditions", strlen("conditions")+1);
			if (eval_int) {
				PHALCON_ALLOC_ZVAL(r15);
				phalcon_array_fetch_string(r15, v1, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r16);
				phalcon_concat_left(r16, " WHERE ", r15 TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r17, v0, r16);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r17);
				v0 = r17;
			}
		}
		eval_int = phalcon_array_isset_string(v1, "order", strlen("order")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r18);
			phalcon_array_fetch_string(r18, v1, "order", strlen("order"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (zend_is_true(r18)) {
				PHALCON_ALLOC_ZVAL(r19);
				phalcon_array_fetch_string(r19, v1, "order", strlen("order"), PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r20);
				phalcon_concat_left(r20, " ORDER BY ", r19 TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r21, v0, r20);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r21);
				v0 = r21;
			}
		} else {
			PHALCON_ALLOC_ZVAL(t0);
			ZVAL_STRING(t0, " ORDER BY 1", 1);
			PHALCON_CONCAT_FUNCTION(r22, v0, t0);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(r22);
			v0 = r22;
		}
		eval_int = phalcon_array_isset_string(v1, "limit", strlen("limit")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r23);
			phalcon_array_fetch_string(r23, v1, "limit", strlen("limit"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (zend_is_true(r23)) {
				PHALCON_ALLOC_ZVAL(r24);
				PHALCON_ALLOC_ZVAL(t1);
				phalcon_read_property(t1, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t1);
				Z_ADDREF_P(v0);
				p2[0] = v0;
				PHALCON_ALLOC_ZVAL(r25);
				phalcon_array_fetch_string(r25, v1, "limit", strlen("limit"), PHALCON_NOISY_FETCH TSRMLS_CC);
				Z_ADDREF_P(r25);
				p2[1] = r25;
				PHALCON_CALL_METHOD_PARAMS(r24, t1, "limit", 2, p2, PHALCON_CALL_DEFAULT);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r24);
				v0 = r24;
			}
		}
		eval_int = phalcon_array_isset_string(v1, "for_update", strlen("for_update")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r26);
			phalcon_array_fetch_string(r26, v1, "for_update", strlen("for_update"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (zend_is_true(r26)) {
				PHALCON_ALLOC_ZVAL(r27);
				PHALCON_ALLOC_ZVAL(t2);
				phalcon_read_property(t2, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t2);
				Z_ADDREF_P(v0);
				p3[0] = v0;
				PHALCON_CALL_METHOD_PARAMS(r27, t2, "forupdate", 1, p3, PHALCON_CALL_DEFAULT);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r27);
				v0 = r27;
			}
		}
		eval_int = phalcon_array_isset_string(v1, "shared_lock", strlen("shared_lock")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r28);
			phalcon_array_fetch_string(r28, v1, "shared_lock", strlen("shared_lock"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (zend_is_true(r28)) {
				PHALCON_ALLOC_ZVAL(r29);
				PHALCON_ALLOC_ZVAL(t3);
				phalcon_read_property(t3, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t3);
				Z_ADDREF_P(v0);
				p4[0] = v0;
				PHALCON_CALL_METHOD_PARAMS(r29, t3, "sharedlock", 1, p4, PHALCON_CALL_DEFAULT);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r29);
				v0 = r29;
			}
		}
	} else {
		PHALCON_ALLOC_ZVAL(r30);
		p5[0] = v1;
		PHALCON_CALL_FUNC_PARAMS(r30, "strlen", 1, p5);
		PHALCON_ALLOC_ZVAL(t4);
		ZVAL_LONG(t4, 0);
		PHALCON_SMALLER_FUNCTION(r31, t4, r30);
		if (zend_is_true(r31)) {
			PHALCON_ALLOC_ZVAL(r32);
			Z_ADDREF_P(v1);
			p6[0] = v1;
			PHALCON_CALL_FUNC_PARAMS(r32, "is_numeric", 1, p6);
			if (zend_is_true(r32)) {
				PHALCON_ALLOC_ZVAL(r34);
				phalcon_array_fetch_long(r34, v2, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r35);
				phalcon_concat_left(r35, "WHERE ", r34 TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r33);
				phalcon_concat_vboth(r33, r35, " = '", v1 TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r36);
				phalcon_concat_right(r36, r33, "'" TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r37, v0, r36);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r37);
				v0 = r37;
			} else {
				PHALCON_ALLOC_ZVAL(r38);
				phalcon_concat_left(r38, "WHERE ", v1 TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r39, v0, r38);
				if (v0) {
					if (!Z_REFCOUNT_P(v0)) {
						FREE_ZVAL(v0);
					}
				}
				Z_ADDREF_P(r39);
				v0 = r39;
			}
		}
	}
	if (Z_TYPE_P(v0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

PHP_METHOD(Phalcon_Model_Base, _createResultset){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL }, *p4[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL }, *p7[] = { NULL, NULL }, *p8[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(t0);
	v2 = t0;
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_array_fetch_string(r0, v0, "return", strlen("return"), PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_STRING(t1, "1", 1);
	PHALCON_EQUAL_FUNCTION(r1, r0, t1);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(r2);
		Z_ADDREF_P(v1);
		p0[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r2, v2, "numrows", 1, p0, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(t2);
		ZVAL_LONG(t2, 0);
		PHALCON_EQUAL_FUNCTION(r3, r2, t2);
		if (zend_is_true(r3)) {
			phalcon_update_property_long(this_ptr, "_count", strlen("_count"), 0 TSRMLS_CC);
			RETURN_FALSE;
		} else {
			PHALCON_GET_CLASS_CONSTANT(t3, phalcon_db_class_entry, "DB_ASSOC");
			Z_ADDREF_P(t3);
			p1[0] = t3;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "setfetchmode", 1, p1, PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(r4);
			Z_ADDREF_P(v1);
			p2[0] = v1;
			PHALCON_CALL_METHOD_PARAMS(r4, v2, "fetcharray", 1, p2, PHALCON_CALL_DEFAULT);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(r4);
			v3 = r4;
			PHALCON_GET_CLASS_CONSTANT(t4, phalcon_db_class_entry, "DB_BOTH");
			Z_ADDREF_P(t4);
			p3[0] = t4;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(v2, "setfetchmode", 1, p3, PHALCON_CALL_DEFAULT);
			Z_ADDREF_P(v3);
			p4[0] = v3;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "dumpresultself", 1, p4, PHALCON_CALL_DEFAULT);
			phalcon_update_property_long(this_ptr, "_count", strlen("_count"), 1 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r5);
			Z_ADDREF_P(v3);
			p5[0] = v3;
			PHALCON_CALL_METHOD_PARAMS(r5, this_ptr, "dumpresult", 1, p5, PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r5, 1, 0);
		}
	} else {
		PHALCON_ALLOC_ZVAL(r6);
		Z_ADDREF_P(v1);
		p6[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r6, v2, "numrows", 1, p6, PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r6, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_count", strlen("_count"), copy TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(t5);
		phalcon_read_property(t5, this_ptr, "_count", sizeof("_count")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t5);
		PHALCON_ALLOC_ZVAL(t6);
		ZVAL_LONG(t6, 0);
		PHALCON_SMALLER_FUNCTION(r7, t6, t5);
		if (zend_is_true(r7)) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_model_resultset_class_entry);
			Z_ADDREF_P(this_ptr);
			p7[0] = this_ptr;
			Z_ADDREF_P(v1);
			p7[1] = v1;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 2, p7, PHALCON_CALL_CHECK);
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(i0);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
				Z_SET_REFCOUNT_P(return_value, refcount);
			}
			return;
		} else {
			PHALCON_ALLOC_ZVAL(i1);
			object_init_ex(i1, phalcon_model_resultset_class_entry);
			Z_ADDREF_P(this_ptr);
			p8[0] = this_ptr;
			PHALCON_PARAM_BOOL(p8[1], 0);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 2, p8, PHALCON_CALL_CHECK);
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(i1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
				Z_SET_REFCOUNT_P(return_value, refcount);
			}
			return;
		}
	}
	RETURN_NULL();
}

/**
 * Overwrittes default model manager
 *
 * @param Php_Model_Manager $manager
 */
PHP_METHOD(Phalcon_Model_Base, setManager){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_manager", strlen("_manager"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Sets a transaction related to the Model instance
 *
 *
 *
 * @param Php_Transaction $transaction
 */
PHP_METHOD(Phalcon_Model_Base, setTransaction){

	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) == IS_OBJECT) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, v0, "getconnection", PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_connection", strlen("_connection"), copy TSRMLS_CC);
		}
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_model_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "Transaction should be an object", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	if (Z_TYPE_P(this_ptr) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(this_ptr);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(this_ptr);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Checks wheter model is mapped to a database view
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Base, isView){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_isView", sizeof("_isView")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets table name which model should be mapped
 *
 * @param string $source
 */
PHP_METHOD(Phalcon_Model_Base, setSource){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_source", strlen("_source"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns table name mapped in the model
 *
 * @return string
 */
PHP_METHOD(Phalcon_Model_Base, getSource){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *p1[] = { NULL }, *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (phalcon_compare_strict_string(t0, "")) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_manager", sizeof("_manager")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(r1);
		p1[0] = this_ptr;
		PHALCON_CALL_FUNC_PARAMS(r1, "get_class", 1, p1);
		Z_ADDREF_P(r1);
		p0[0] = r1;
		PHALCON_CALL_METHOD_PARAMS(r0, t1, "getsource", 1, p0, PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_source", strlen("_source"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	if (Z_TYPE_P(t2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Sets schema name where table mapped is located
 *
 * @param string $schema
 */
PHP_METHOD(Phalcon_Model_Base, setSchema){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_schema", strlen("_schema"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns schema name where table mapped is located
 *
 * @return string
 */
PHP_METHOD(Phalcon_Model_Base, getSchema){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (phalcon_compare_strict_string(t0, "")) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_CALL_METHOD(r0, t1, "getdatabasename", PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_schema", strlen("_schema"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	if (Z_TYPE_P(t2) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t2);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Gets internal Php_Db connection
 *
 * @return Php_Db
 */
PHP_METHOD(Phalcon_Model_Base, getConnection){

	zval *t0 = NULL;

	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Assigns values to a model from an array returning a new model
 *
 *
 *
 * @param array $result
 * @return Php_Model $result
 */
PHP_METHOD(Phalcon_Model_Base, dumpResult){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *s0 = NULL;
	zval *ac0 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	phalcon_clone(&s0, this_ptr TSRMLS_CC);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(s0);
	v1 = s0;
	zend_update_property_bool(Z_OBJCE_P(v1), v1, "_forceExists", strlen("_forceExists"), 1 TSRMLS_CC);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 1 TSRMLS_CC);
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		FOREACH_KV(v0, ac0, fes34, fee34, ah0, hp0, v3, v2)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v2, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(v1, Z_STRVAL_P(v3), Z_STRLEN_P(v3), copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes34, fee34, ah0, hp0);
	}
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 0 TSRMLS_CC);
	if (Z_TYPE_P(v1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Assigns values to a model from an array
 *
 *
 *
 * @param array $result
 */
PHP_METHOD(Phalcon_Model_Base, dumpResultSelf){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *ac0 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 1 TSRMLS_CC);
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		FOREACH_KV(v0, ac0, fes35, fee35, ah0, hp0, v2, v1)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v1, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, Z_STRVAL_P(v2), Z_STRLEN_P(v2), copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes35, fee35, ah0, hp0);
	}
	zend_update_property_bool(Z_OBJCE_P(this_ptr), this_ptr, "_dumpLock", strlen("_dumpLock"), 0 TSRMLS_CC);
	RETURN_NULL();
}

/**
 * Allows to query a set of records that match the specified conditions
 *
 * 
 *
 * @param array $params
 * @return Php_Model_Resulset
 */
PHP_METHOD(Phalcon_Model_Base, find){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *t0 = NULL;
	zval *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_NULL(v0);
	}
	
	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		if (Z_TYPE_P(v0) != IS_NULL) {
			PHALCON_ALLOC_ZVAL(a0);
			array_init(a0);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v0, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				{
					zval *orig_ptr = a0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(a0);
						*a0 = *orig_ptr;
						zval_copy_ctor(a0);
						Z_SET_REFCOUNT_P(a0, 1);
						Z_UNSET_ISREF_P(a0);
					}
				}
				add_next_index_zval(a0, copy);
			}
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a0);
			v0 = a0;
		} else {
			PHALCON_ALLOC_ZVAL(a1);
			array_init(a1);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a1);
			v0 = a1;
		}
	}
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p1[0] = v0;
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "_createsqlselectmulti", 1, p1, PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_ALLOC_ZVAL(r2);
	phalcon_array_fetch_string(r2, v1, "sql", strlen("sql"), PHALCON_NOISY_FETCH TSRMLS_CC);
	Z_ADDREF_P(r2);
	p2[0] = r2;
	PHALCON_CALL_METHOD_PARAMS(r1, t0, "query", 1, p2, PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	PHALCON_ALLOC_ZVAL(r3);
	Z_ADDREF_P(v1);
	p3[0] = v1;
	Z_ADDREF_P(v2);
	p3[1] = v2;
	PHALCON_CALL_METHOD_PARAMS(r3, this_ptr, "_createresultset", 2, p3, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r3, 1, 0);
}

/**
 * Allows to query the first record that match the specified conditions
 *
 * 
 *
 * @param array $params
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Base, findFirst){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *p4[] = { NULL, NULL }, *p6[] = { NULL, NULL }, *p7[] = { NULL }, *p8[] = { NULL }, *p9[] = { NULL }, *p10[] = { NULL }, *p11[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_INIT_NULL(v0);
	}
	
	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		if (Z_TYPE_P(v0) != IS_NULL) {
			PHALCON_ALLOC_ZVAL(a0);
			array_init(a0);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v0, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				{
					zval *orig_ptr = a0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(a0);
						*a0 = *orig_ptr;
						zval_copy_ctor(a0);
						Z_SET_REFCOUNT_P(a0, 1);
						Z_UNSET_ISREF_P(a0);
					}
				}
				add_next_index_zval(a0, copy);
			}
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a0);
			v0 = a0;
		} else {
			PHALCON_ALLOC_ZVAL(a1);
			array_init(a1);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a1);
			v0 = a1;
		}
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getschema", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, this_ptr, "getsource", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	PHALCON_ALLOC_ZVAL(v3);
	ZVAL_STRING(v3, "SELECT ", 0);
	eval_int = phalcon_array_isset_string(v0, "columns", strlen("columns")+1);
	if (eval_int) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "clear", PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_string(r2, v0, "columns", strlen("columns"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r3, v3, r2);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r3);
		v3 = r3;
	} else {
		PHALCON_ALLOC_ZVAL(r4);
		PHALCON_ALLOC_ZVAL(p4[0]);
		ZVAL_STRING(p4[0], ", ", 1);
		PHALCON_ALLOC_ZVAL(r5);
		PHALCON_CALL_METHOD(r5, this_ptr, "_getattributes", PHALCON_CALL_DEFAULT);
		p4[1] = r5;
		PHALCON_CALL_FUNC_PARAMS(r4, "join", 2, p4);
		PHALCON_CONCAT_FUNCTION(r6, v3, r4);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r6);
		v3 = r6;
	}
	if (!phalcon_compare_strict_string(v1, "")) {
		PHALCON_ALLOC_ZVAL(r8);
		phalcon_concat_left(r8, " FROM ", v1 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r7);
		phalcon_concat_vboth(r7, r8, ".", v2 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r9, v3, r7);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r9);
		v3 = r9;
	} else {
		PHALCON_ALLOC_ZVAL(r10);
		phalcon_concat_left(r10, " FROM ", v2 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r11, v3, r10);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r11);
		v3 = r11;
	}
	eval_int = phalcon_array_isset_string(v0, "limit", strlen("limit")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(t0);
		ZVAL_LONG(t0, 1);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "limit", strlen("limit"), t0 TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(v4);
	ZVAL_BOOL(v4, 0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (v5) {
		Z_DELREF_P(v5);
		if (!Z_REFCOUNT_P(v5)) {
			FREE_ZVAL(v5);
		}
	}
	Z_ADDREF_P(t1);
	v5 = t1;
	PHALCON_ALLOC_ZVAL(r12);
	Z_ADDREF_P(v3);
	p6[0] = v3;
	Z_ADDREF_P(v0);
	p6[1] = v0;
	PHALCON_CALL_METHOD_PARAMS(r12, this_ptr, "_createsqlselectone", 2, p6, PHALCON_CALL_DEFAULT);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r12);
	v3 = r12;
	PHALCON_GET_CLASS_CONSTANT(t2, phalcon_db_class_entry, "DB_ASSOC");
	Z_ADDREF_P(t2);
	p7[0] = t2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v5, "setfetchmode", 1, p7, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r13);
	Z_ADDREF_P(v3);
	p8[0] = v3;
	PHALCON_CALL_METHOD_PARAMS(r13, v5, "fetchone", 1, p8, PHALCON_CALL_DEFAULT);
	if (v6) {
		Z_DELREF_P(v6);
		if (!Z_REFCOUNT_P(v6)) {
			FREE_ZVAL(v6);
		}
	}
	Z_ADDREF_P(r13);
	v6 = r13;
	if (zend_is_true(v6)) {
		Z_ADDREF_P(v6);
		p9[0] = v6;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "dumpresultself", 1, p9, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r14);
		Z_ADDREF_P(v6);
		p10[0] = v6;
		PHALCON_CALL_METHOD_PARAMS(r14, this_ptr, "dumpresult", 1, p10, PHALCON_CALL_DEFAULT);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r14);
		v4 = r14;
	}
	PHALCON_GET_CLASS_CONSTANT(t3, phalcon_db_class_entry, "DB_BOTH");
	Z_ADDREF_P(t3);
	p11[0] = t3;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v5, "setfetchmode", 1, p11, PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(v4) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v4);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v4);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Allows to query whether exists at least one record that match the specified conditions
 *
 * 
 *
 * @param array $params
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Base, exists){

	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v0);
	p1[0] = v0;
	PHALCON_CALL_METHOD_PARAMS(r0, this_ptr, "_exists", 1, p1, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r0, 1, 0);
}

PHP_METHOD(Phalcon_Model_Base, _exists){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL, *t11 = NULL, *t12 = NULL, *t13 = NULL;
	zval *t14 = NULL, *t15 = NULL, *t16 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL, *r37 = NULL, *r38 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *p4[] = { NULL }, *p5[] = { NULL }, *p6[] = { NULL, NULL }, *p7[] = { NULL }, *p8[] = { NULL }, *p11[] = { NULL }, *p12[] = { NULL }, *p13[] = { NULL, NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_forceExists", sizeof("_forceExists")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, this_ptr, "getschema", PHALCON_CALL_DEFAULT);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r0);
		v1 = r0;
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_METHOD(r1, this_ptr, "getsource", PHALCON_CALL_DEFAULT);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r1);
		v2 = r1;
		if (zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_concat_vboth(r2, v1, ".", v2 TSRMLS_CC);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(r2);
			v3 = r2;
		} else {
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(v2);
			v3 = v2;
		}
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_both(r3,  "SELECT COUNT(*) AS rowcount FROM ", v3, " WHERE " TSRMLS_CC);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r3);
		v4 = r3;
		if (phalcon_compare_strict_string(v0, "")) {
			PHALCON_ALLOC_ZVAL(a0);
			array_init(a0);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a0);
			v0 = a0;
			PHALCON_ALLOC_ZVAL(r4);
			PHALCON_CALL_METHOD(r4, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r4);
			v5 = r4;
			PHALCON_ALLOC_ZVAL(r5);
			PHALCON_CALL_METHOD(r5, this_ptr, "_getdatatypesnumeric", PHALCON_CALL_DEFAULT);
			if (v6) {
				Z_DELREF_P(v6);
				if (!Z_REFCOUNT_P(v6)) {
					FREE_ZVAL(v6);
				}
			}
			Z_ADDREF_P(r5);
			v6 = r5;
			PHALCON_ALLOC_ZVAL(r6);
			p4[0] = v5;
			PHALCON_CALL_FUNC_PARAMS(r6, "count", 1, p4);
			PHALCON_ALLOC_ZVAL(t1);
			ZVAL_LONG(t1, 0);
			PHALCON_SMALLER_FUNCTION(r7, t1, r6);
			if (zend_is_true(r7)) {
				FOREACH_V(v5, ac0, fes36, fee36, ah0, hp0, v7)
					if (!t2) {
						PHALCON_ALLOC_ZVAL(t2);
					} else {
						if (Z_REFCOUNT_P(t2) > 1) {
							{
								zval *orig_ptr = t2;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t2);
									*t2 = *orig_ptr;
									zval_copy_ctor(t2);
									Z_SET_REFCOUNT_P(t2, 1);
									Z_UNSET_ISREF_P(t2);
								}
							}
						} else {
							FREE_ZVAL(t2);
							PHALCON_ALLOC_ZVAL(t2);
						}
					}
					phalcon_read_property_zval(t2, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
					zval_copy_ctor(t2);
					PHALCON_INIT_NULL(t3);
					PHALCON_NOT_IDENTICAL_FUNCTION(r8, t2, t3);
					if (!t4) {
						PHALCON_ALLOC_ZVAL(t4);
					} else {
						if (Z_REFCOUNT_P(t4) > 1) {
							{
								zval *orig_ptr = t4;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t4);
									*t4 = *orig_ptr;
									zval_copy_ctor(t4);
									Z_SET_REFCOUNT_P(t4, 1);
									Z_UNSET_ISREF_P(t4);
								}
							}
						} else {
							FREE_ZVAL(t4);
							PHALCON_ALLOC_ZVAL(t4);
						}
					}
					phalcon_read_property_zval(t4, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
					zval_copy_ctor(t4);
					if (!t5) {
						PHALCON_ALLOC_ZVAL(t5);
					} else {
						if (Z_REFCOUNT_P(t5) > 1) {
							SEPARATE_ZVAL(&t5);
						} else {
							if (Z_TYPE_P(t5) != IS_STRING) {
								FREE_ZVAL(t5);
								PHALCON_ALLOC_ZVAL(t5);
							}
						}
					}
					ZVAL_STRING(t5, "", 1);
					PHALCON_NOT_IDENTICAL_FUNCTION(r9, t4, t5);
					PHALCON_AND_FUNCTION(r10, r8, r9);
					if (zend_is_true(r10)) {
						eval_int = phalcon_array_isset(v6, v7);
						if (eval_int) {
							if (!r12) {
								PHALCON_ALLOC_ZVAL(r12);
							} else {
								if (Z_REFCOUNT_P(r12) > 1) {
									{
										zval *orig_ptr = r12;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r12);
											*r12 = *orig_ptr;
											zval_copy_ctor(r12);
											Z_SET_REFCOUNT_P(r12, 1);
											Z_UNSET_ISREF_P(r12);
										}
									}
								} else {
									FREE_ZVAL(r12);
									PHALCON_ALLOC_ZVAL(r12);
								}
							}
							phalcon_concat_left(r12, " ", v7 TSRMLS_CC);
							if (!t6) {
								PHALCON_ALLOC_ZVAL(t6);
							} else {
								if (Z_REFCOUNT_P(t6) > 1) {
									{
										zval *orig_ptr = t6;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(t6);
											*t6 = *orig_ptr;
											zval_copy_ctor(t6);
											Z_SET_REFCOUNT_P(t6, 1);
											Z_UNSET_ISREF_P(t6);
										}
									}
								} else {
									FREE_ZVAL(t6);
									PHALCON_ALLOC_ZVAL(t6);
								}
							}
							phalcon_read_property_zval(t6, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
							zval_copy_ctor(t6);
							if (!r11) {
								PHALCON_ALLOC_ZVAL(r11);
							} else {
								if (Z_REFCOUNT_P(r11) > 1) {
									{
										zval *orig_ptr = r11;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r11);
											*r11 = *orig_ptr;
											zval_copy_ctor(r11);
											Z_SET_REFCOUNT_P(r11, 1);
											Z_UNSET_ISREF_P(r11);
										}
									}
								} else {
									FREE_ZVAL(r11);
									PHALCON_ALLOC_ZVAL(r11);
								}
							}
							phalcon_concat_vboth(r11, r12, " = ", t6 TSRMLS_CC);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r11, 1, 0);
								Z_SET_REFCOUNT_P(copy, 1);
								{
									zval *orig_ptr = v0;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										ALLOC_ZVAL(v0);
										*v0 = *orig_ptr;
										zval_copy_ctor(v0);
										Z_SET_REFCOUNT_P(v0, 1);
										Z_UNSET_ISREF_P(v0);
									}
								}
								phalcon_array_append(v0, copy TSRMLS_CC);
							}
						} else {
							if (!r14) {
								PHALCON_ALLOC_ZVAL(r14);
							} else {
								if (Z_REFCOUNT_P(r14) > 1) {
									{
										zval *orig_ptr = r14;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r14);
											*r14 = *orig_ptr;
											zval_copy_ctor(r14);
											Z_SET_REFCOUNT_P(r14, 1);
											Z_UNSET_ISREF_P(r14);
										}
									}
								} else {
									FREE_ZVAL(r14);
									PHALCON_ALLOC_ZVAL(r14);
								}
							}
							phalcon_concat_left(r14, " ", v7 TSRMLS_CC);
							if (!t7) {
								PHALCON_ALLOC_ZVAL(t7);
							} else {
								if (Z_REFCOUNT_P(t7) > 1) {
									{
										zval *orig_ptr = t7;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(t7);
											*t7 = *orig_ptr;
											zval_copy_ctor(t7);
											Z_SET_REFCOUNT_P(t7, 1);
											Z_UNSET_ISREF_P(t7);
										}
									}
								} else {
									FREE_ZVAL(t7);
									PHALCON_ALLOC_ZVAL(t7);
								}
							}
							phalcon_read_property_zval(t7, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
							zval_copy_ctor(t7);
							if (!r13) {
								PHALCON_ALLOC_ZVAL(r13);
							} else {
								if (Z_REFCOUNT_P(r13) > 1) {
									{
										zval *orig_ptr = r13;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r13);
											*r13 = *orig_ptr;
											zval_copy_ctor(r13);
											Z_SET_REFCOUNT_P(r13, 1);
											Z_UNSET_ISREF_P(r13);
										}
									}
								} else {
									FREE_ZVAL(r13);
									PHALCON_ALLOC_ZVAL(r13);
								}
							}
							phalcon_concat_vboth(r13, r14, " = '", t7 TSRMLS_CC);
							if (!r15) {
								PHALCON_ALLOC_ZVAL(r15);
							} else {
								if (Z_REFCOUNT_P(r15) > 1) {
									{
										zval *orig_ptr = r15;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r15);
											*r15 = *orig_ptr;
											zval_copy_ctor(r15);
											Z_SET_REFCOUNT_P(r15, 1);
											Z_UNSET_ISREF_P(r15);
										}
									}
								} else {
									FREE_ZVAL(r15);
									PHALCON_ALLOC_ZVAL(r15);
								}
							}
							phalcon_concat_right(r15, r13, "'" TSRMLS_CC);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r15, 1, 0);
								Z_SET_REFCOUNT_P(copy, 1);
								{
									zval *orig_ptr = v0;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										ALLOC_ZVAL(v0);
										*v0 = *orig_ptr;
										zval_copy_ctor(v0);
										Z_SET_REFCOUNT_P(v0, 1);
										Z_UNSET_ISREF_P(v0);
									}
								}
								phalcon_array_append(v0, copy TSRMLS_CC);
							}
						}
					}
				END_FOREACH(ac0, fes36, fee36, ah0, hp0);
				PHALCON_ALLOC_ZVAL(r16);
				p5[0] = v0;
				PHALCON_CALL_FUNC_PARAMS(r16, "count", 1, p5);
				if (zend_is_true(r16)) {
					PHALCON_ALLOC_ZVAL(r17);
					PHALCON_ALLOC_ZVAL(p6[0]);
					ZVAL_STRING(p6[0], " AND ", 1);
					p6[1] = v0;
					PHALCON_CALL_FUNC_PARAMS(r17, "join", 2, p6);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, r17, 1, 0);
						Z_SET_REFCOUNT_P(copy, 0);
						phalcon_update_property_zval(this_ptr, "_uniqueKey", strlen("_uniqueKey"), copy TSRMLS_CC);
					}
				} else {
					RETURN_LONG(0);
				}
				PHALCON_ALLOC_ZVAL(t8);
				phalcon_read_property(t8, this_ptr, "_uniqueKey", sizeof("_uniqueKey")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t8);
				PHALCON_CONCAT_FUNCTION(r18, v4, t8);
				if (v4) {
					Z_DELREF_P(v4);
					if (!Z_REFCOUNT_P(v4)) {
						FREE_ZVAL(v4);
					}
				}
				Z_ADDREF_P(r18);
				v4 = r18;
			} else {
				RETURN_LONG(0);
			}
		} else {
			PHALCON_ALLOC_ZVAL(r19);
			Z_ADDREF_P(v0);
			p7[0] = v0;
			PHALCON_CALL_FUNC_PARAMS(r19, "is_numeric", 1, p7);
			if (zend_is_true(r19)) {
				PHALCON_ALLOC_ZVAL(r20);
				phalcon_concat_left(r20, "id = ", v0 TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r21, v4, r20);
				if (v4) {
					Z_DELREF_P(v4);
					if (!Z_REFCOUNT_P(v4)) {
						FREE_ZVAL(v4);
					}
				}
				Z_ADDREF_P(r21);
				v4 = r21;
			} else {
				PHALCON_CONCAT_FUNCTION(r22, v4, v0);
				if (v4) {
					Z_DELREF_P(v4);
					if (!Z_REFCOUNT_P(v4)) {
						FREE_ZVAL(v4);
					}
				}
				Z_ADDREF_P(r22);
				v4 = r22;
			}
		}
		PHALCON_ALLOC_ZVAL(r23);
		PHALCON_ALLOC_ZVAL(t9);
		phalcon_read_property(t9, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t9);
		Z_ADDREF_P(v4);
		p8[0] = v4;
		PHALCON_CALL_METHOD_PARAMS(r23, t9, "fetchone", 1, p8, PHALCON_CALL_DEFAULT);
		if (v8) {
			Z_DELREF_P(v8);
			if (!Z_REFCOUNT_P(v8)) {
				FREE_ZVAL(v8);
			}
		}
		Z_ADDREF_P(r23);
		v8 = r23;
		PHALCON_ALLOC_ZVAL(r24);
		phalcon_array_fetch_string(r24, v8, "rowcount", strlen("rowcount"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r24) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r24);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r24);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a1);
		v0 = a1;
		PHALCON_ALLOC_ZVAL(r25);
		PHALCON_CALL_METHOD(r25, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r25);
		v5 = r25;
		PHALCON_ALLOC_ZVAL(r26);
		PHALCON_CALL_METHOD(r26, this_ptr, "_getdatatypesnumeric", PHALCON_CALL_DEFAULT);
		if (v9) {
			Z_DELREF_P(v9);
			if (!Z_REFCOUNT_P(v9)) {
				FREE_ZVAL(v9);
			}
		}
		Z_ADDREF_P(r26);
		v9 = r26;
		PHALCON_ALLOC_ZVAL(r27);
		p11[0] = v5;
		PHALCON_CALL_FUNC_PARAMS(r27, "count", 1, p11);
		PHALCON_ALLOC_ZVAL(t10);
		ZVAL_LONG(t10, 0);
		PHALCON_SMALLER_FUNCTION(r28, t10, r27);
		if (zend_is_true(r28)) {
			FOREACH_V(v5, ac1, fes37, fee37, ah1, hp1, v7)
				if (!t11) {
					PHALCON_ALLOC_ZVAL(t11);
				} else {
					if (Z_REFCOUNT_P(t11) > 1) {
						{
							zval *orig_ptr = t11;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(t11);
								*t11 = *orig_ptr;
								zval_copy_ctor(t11);
								Z_SET_REFCOUNT_P(t11, 1);
								Z_UNSET_ISREF_P(t11);
							}
						}
					} else {
						FREE_ZVAL(t11);
						PHALCON_ALLOC_ZVAL(t11);
					}
				}
				phalcon_read_property_zval(t11, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t11);
				PHALCON_INIT_NULL(t12);
				PHALCON_NOT_IDENTICAL_FUNCTION(r29, t11, t12);
				if (!t13) {
					PHALCON_ALLOC_ZVAL(t13);
				} else {
					if (Z_REFCOUNT_P(t13) > 1) {
						{
							zval *orig_ptr = t13;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(t13);
								*t13 = *orig_ptr;
								zval_copy_ctor(t13);
								Z_SET_REFCOUNT_P(t13, 1);
								Z_UNSET_ISREF_P(t13);
							}
						}
					} else {
						FREE_ZVAL(t13);
						PHALCON_ALLOC_ZVAL(t13);
					}
				}
				phalcon_read_property_zval(t13, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t13);
				if (!t14) {
					PHALCON_ALLOC_ZVAL(t14);
				} else {
					if (Z_REFCOUNT_P(t14) > 1) {
						SEPARATE_ZVAL(&t14);
					} else {
						if (Z_TYPE_P(t14) != IS_STRING) {
							FREE_ZVAL(t14);
							PHALCON_ALLOC_ZVAL(t14);
						}
					}
				}
				ZVAL_STRING(t14, "", 1);
				PHALCON_NOT_IDENTICAL_FUNCTION(r30, t13, t14);
				PHALCON_AND_FUNCTION(r31, r29, r30);
				if (zend_is_true(r31)) {
					eval_int = phalcon_array_isset(v9, v7);
					if (eval_int) {
						if (!r33) {
							PHALCON_ALLOC_ZVAL(r33);
						} else {
							if (Z_REFCOUNT_P(r33) > 1) {
								{
									zval *orig_ptr = r33;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r33);
										*r33 = *orig_ptr;
										zval_copy_ctor(r33);
										Z_SET_REFCOUNT_P(r33, 1);
										Z_UNSET_ISREF_P(r33);
									}
								}
							} else {
								FREE_ZVAL(r33);
								PHALCON_ALLOC_ZVAL(r33);
							}
						}
						phalcon_concat_left(r33, " ", v7 TSRMLS_CC);
						if (!t15) {
							PHALCON_ALLOC_ZVAL(t15);
						} else {
							if (Z_REFCOUNT_P(t15) > 1) {
								{
									zval *orig_ptr = t15;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(t15);
										*t15 = *orig_ptr;
										zval_copy_ctor(t15);
										Z_SET_REFCOUNT_P(t15, 1);
										Z_UNSET_ISREF_P(t15);
									}
								}
							} else {
								FREE_ZVAL(t15);
								PHALCON_ALLOC_ZVAL(t15);
							}
						}
						phalcon_read_property_zval(t15, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
						zval_copy_ctor(t15);
						if (!r32) {
							PHALCON_ALLOC_ZVAL(r32);
						} else {
							if (Z_REFCOUNT_P(r32) > 1) {
								{
									zval *orig_ptr = r32;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r32);
										*r32 = *orig_ptr;
										zval_copy_ctor(r32);
										Z_SET_REFCOUNT_P(r32, 1);
										Z_UNSET_ISREF_P(r32);
									}
								}
							} else {
								FREE_ZVAL(r32);
								PHALCON_ALLOC_ZVAL(r32);
							}
						}
						phalcon_concat_vboth(r32, r33, " = ", t15 TSRMLS_CC);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, r32, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v0;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									ALLOC_ZVAL(v0);
									*v0 = *orig_ptr;
									zval_copy_ctor(v0);
									Z_SET_REFCOUNT_P(v0, 1);
									Z_UNSET_ISREF_P(v0);
								}
							}
							phalcon_array_append(v0, copy TSRMLS_CC);
						}
					} else {
						if (!r35) {
							PHALCON_ALLOC_ZVAL(r35);
						} else {
							if (Z_REFCOUNT_P(r35) > 1) {
								{
									zval *orig_ptr = r35;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r35);
										*r35 = *orig_ptr;
										zval_copy_ctor(r35);
										Z_SET_REFCOUNT_P(r35, 1);
										Z_UNSET_ISREF_P(r35);
									}
								}
							} else {
								FREE_ZVAL(r35);
								PHALCON_ALLOC_ZVAL(r35);
							}
						}
						phalcon_concat_left(r35, " ", v7 TSRMLS_CC);
						if (!t16) {
							PHALCON_ALLOC_ZVAL(t16);
						} else {
							if (Z_REFCOUNT_P(t16) > 1) {
								{
									zval *orig_ptr = t16;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(t16);
										*t16 = *orig_ptr;
										zval_copy_ctor(t16);
										Z_SET_REFCOUNT_P(t16, 1);
										Z_UNSET_ISREF_P(t16);
									}
								}
							} else {
								FREE_ZVAL(t16);
								PHALCON_ALLOC_ZVAL(t16);
							}
						}
						phalcon_read_property_zval(t16, this_ptr, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
						zval_copy_ctor(t16);
						if (!r34) {
							PHALCON_ALLOC_ZVAL(r34);
						} else {
							if (Z_REFCOUNT_P(r34) > 1) {
								{
									zval *orig_ptr = r34;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r34);
										*r34 = *orig_ptr;
										zval_copy_ctor(r34);
										Z_SET_REFCOUNT_P(r34, 1);
										Z_UNSET_ISREF_P(r34);
									}
								}
							} else {
								FREE_ZVAL(r34);
								PHALCON_ALLOC_ZVAL(r34);
							}
						}
						phalcon_concat_vboth(r34, r35, " = '", t16 TSRMLS_CC);
						if (!r36) {
							PHALCON_ALLOC_ZVAL(r36);
						} else {
							if (Z_REFCOUNT_P(r36) > 1) {
								{
									zval *orig_ptr = r36;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r36);
										*r36 = *orig_ptr;
										zval_copy_ctor(r36);
										Z_SET_REFCOUNT_P(r36, 1);
										Z_UNSET_ISREF_P(r36);
									}
								}
							} else {
								FREE_ZVAL(r36);
								PHALCON_ALLOC_ZVAL(r36);
							}
						}
						phalcon_concat_right(r36, r34, "'" TSRMLS_CC);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, r36, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v0;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									ALLOC_ZVAL(v0);
									*v0 = *orig_ptr;
									zval_copy_ctor(v0);
									Z_SET_REFCOUNT_P(v0, 1);
									Z_UNSET_ISREF_P(v0);
								}
							}
							phalcon_array_append(v0, copy TSRMLS_CC);
						}
					}
				}
			END_FOREACH(ac1, fes37, fee37, ah1, hp1);
			PHALCON_ALLOC_ZVAL(r37);
			p12[0] = v0;
			PHALCON_CALL_FUNC_PARAMS(r37, "count", 1, p12);
			if (zend_is_true(r37)) {
				PHALCON_ALLOC_ZVAL(r38);
				PHALCON_ALLOC_ZVAL(p13[0]);
				ZVAL_STRING(p13[0], " AND ", 1);
				p13[1] = v0;
				PHALCON_CALL_FUNC_PARAMS(r38, "join", 2, p13);
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, r38, 1, 0);
					Z_SET_REFCOUNT_P(copy, 0);
					phalcon_update_property_zval(this_ptr, "_uniqueKey", strlen("_uniqueKey"), copy TSRMLS_CC);
				}
				RETURN_TRUE;
			} else {
				RETURN_LONG(0);
			}
		} else {
			RETURN_LONG(0);
		}
	}
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Model_Base, _getGroupResult){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *i0 = NULL, *i1 = NULL, *i2 = NULL, *i3 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p3[] = { NULL }, *p4[] = { NULL, NULL }, *p5[] = { NULL, NULL }, *p6[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &v0, &v1, &v2) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(t0);
	v3 = t0;
	eval_int = phalcon_array_isset_string(v0, "group", strlen("group")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		Z_ADDREF_P(v1);
		p0[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r0, v3, "query", 1, p0, PHALCON_CALL_DEFAULT);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r0);
		v4 = r0;
		PHALCON_ALLOC_ZVAL(r1);
		Z_ADDREF_P(v4);
		p1[0] = v4;
		PHALCON_CALL_METHOD_PARAMS(r1, v3, "numrows", 1, p1, PHALCON_CALL_DEFAULT);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r1);
		v5 = r1;
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_LONG(t1, 0);
		PHALCON_SMALLER_FUNCTION(r2, t1, v5);
		if (zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_model_row_class_entry);
			PHALCON_CALL_METHOD_NORETURN(i0, "__construct", PHALCON_CALL_CHECK);
			if (v6) {
				Z_DELREF_P(v6);
				if (!Z_REFCOUNT_P(v6)) {
					FREE_ZVAL(v6);
				}
			}
			Z_ADDREF_P(i0);
			v6 = i0;
			Z_ADDREF_P(v3);
			p3[0] = v3;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(v6, "setconnection", 1, p3, PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(i1);
			object_init_ex(i1, phalcon_model_resultset_class_entry);
			Z_ADDREF_P(v6);
			p4[0] = v6;
			Z_ADDREF_P(v4);
			p4[1] = v4;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 2, p4, PHALCON_CALL_CHECK);
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(i1);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
				Z_SET_REFCOUNT_P(return_value, refcount);
			}
			return;
		} else {
			PHALCON_ALLOC_ZVAL(i2);
			object_init_ex(i2, phalcon_model_resultset_class_entry);
			PHALCON_ALLOC_ZVAL(i3);
			object_init(i3);
			Z_ADDREF_P(i3);
			p5[0] = i3;
			PHALCON_PARAM_BOOL(p5[1], 0);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i2, "__construct", 2, p5, PHALCON_CALL_CHECK);
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(i2);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
				Z_SET_REFCOUNT_P(return_value, refcount);
			}
			return;
		}
	} else {
		PHALCON_ALLOC_ZVAL(r3);
		Z_ADDREF_P(v1);
		p6[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r3, v3, "fetchone", 1, p6, PHALCON_CALL_DEFAULT);
		if (v7) {
			Z_DELREF_P(v7);
			if (!Z_REFCOUNT_P(v7)) {
				FREE_ZVAL(v7);
			}
		}
		Z_ADDREF_P(r3);
		v7 = r3;
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch(r4, v7, v2, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r4) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r4);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(r4);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	}
	RETURN_NULL();
}

/**
 * Allows to count how many records match the specified conditions
 *
 * 
 *
 * @param array $params
 * @return int
 */
PHP_METHOD(Phalcon_Model_Base, count){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *a0 = NULL, *a1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *p3[] = { NULL }, *p5[] = { NULL, NULL }, *p6[] = { NULL, NULL, NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		if (Z_TYPE_P(v0) != IS_NULL) {
			PHALCON_ALLOC_ZVAL(a0);
			array_init(a0);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v0, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				{
					zval *orig_ptr = a0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(a0);
						*a0 = *orig_ptr;
						zval_copy_ctor(a0);
						Z_SET_REFCOUNT_P(a0, 1);
						Z_UNSET_ISREF_P(a0);
					}
				}
				add_next_index_zval(a0, copy);
			}
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a0);
			v0 = a0;
		} else {
			PHALCON_ALLOC_ZVAL(a1);
			array_init(a1);
			if (v0) {
				if (!Z_REFCOUNT_P(v0)) {
					FREE_ZVAL(v0);
				}
			}
			Z_ADDREF_P(a1);
			v0 = a1;
		}
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "getschema", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, this_ptr, "getsource", PHALCON_CALL_DEFAULT);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	if (zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_concat_vboth(r2, v1, ".", v2 TSRMLS_CC);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r2);
		v3 = r2;
	} else {
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(v2);
		v3 = v2;
	}
	eval_int = phalcon_array_isset_string(v0, "distinct", strlen("distinct")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch_string(r4, v0, "distinct", strlen("distinct"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_left(r5, "SELECT COUNT(DISTINCT ", r4 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r3);
		phalcon_concat_vboth(r3, r5, ") AS rowcount FROM ", v3 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_concat_right(r6, r3, " " TSRMLS_CC);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r6);
		v4 = r6;
	} else {
		eval_int = phalcon_array_isset_string(v0, "group", strlen("group")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r8);
			phalcon_array_fetch_string(r8, v0, "group", strlen("group"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r9);
			phalcon_concat_left(r9, "SELECT ", r8 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r7);
			phalcon_concat_vboth(r7, r9, ", COUNT(*) AS rowcount FROM ", v3 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r10);
			phalcon_concat_right(r10, r7, " " TSRMLS_CC);
			if (v4) {
				Z_DELREF_P(v4);
				if (!Z_REFCOUNT_P(v4)) {
					FREE_ZVAL(v4);
				}
			}
			Z_ADDREF_P(r10);
			v4 = r10;
		} else {
			PHALCON_ALLOC_ZVAL(r11);
			phalcon_concat_both(r11,  "SELECT COUNT(*) AS rowcount FROM ", v3, " " TSRMLS_CC);
			if (v4) {
				Z_DELREF_P(v4);
				if (!Z_REFCOUNT_P(v4)) {
					FREE_ZVAL(v4);
				}
			}
			Z_ADDREF_P(r11);
			v4 = r11;
		}
	}
	eval_int = phalcon_array_isset_string(v0, "conditions", strlen("conditions")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r12);
		phalcon_array_fetch_string(r12, v0, "conditions", strlen("conditions"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r13);
		phalcon_concat_both(r13,  " WHERE ", r12, " " TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r14, v4, r13);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r14);
		v4 = r14;
	} else {
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r15);
			phalcon_array_fetch_long(r15, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v5) {
				Z_DELREF_P(v5);
				if (!Z_REFCOUNT_P(v5)) {
					FREE_ZVAL(v5);
				}
			}
			Z_ADDREF_P(r15);
			v5 = r15;
			PHALCON_ALLOC_ZVAL(r16);
			Z_ADDREF_P(v5);
			p3[0] = v5;
			PHALCON_CALL_FUNC_PARAMS(r16, "is_numeric", 1, p3);
			if (zend_is_true(r16)) {
				PHALCON_ALLOC_ZVAL(r17);
				PHALCON_CALL_METHOD(r17, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
				if (v6) {
					Z_DELREF_P(v6);
					if (!Z_REFCOUNT_P(v6)) {
						FREE_ZVAL(v6);
					}
				}
				Z_ADDREF_P(r17);
				v6 = r17;
				PHALCON_ALLOC_ZVAL(t0);
				phalcon_read_property(t0, this_ptr, "_isView", sizeof("_isView")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t0);
				if (zend_is_true(t0)) {
					PHALCON_ALLOC_ZVAL(t1);
					ZVAL_STRING(t1, "id", 1);
					{
						zval *orig_ptr = v6;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(v6);
							*v6 = *orig_ptr;
							zval_copy_ctor(v6);
							Z_SET_REFCOUNT_P(v6, 1);
							Z_UNSET_ISREF_P(v6);
						}
					}
					phalcon_array_update_long(v6, 0, t1 TSRMLS_CC);
				}
				PHALCON_ALLOC_ZVAL(r19);
				phalcon_array_fetch_long(r19, v6, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r20);
				phalcon_concat_left(r20, " WHERE ", r19 TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r18);
				phalcon_concat_vboth(r18, r20, " = '", v5 TSRMLS_CC);
				PHALCON_ALLOC_ZVAL(r21);
				phalcon_concat_right(r21, r18, "'" TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r22, v4, r21);
				if (v4) {
					Z_DELREF_P(v4);
					if (!Z_REFCOUNT_P(v4)) {
						FREE_ZVAL(v4);
					}
				}
				Z_ADDREF_P(r22);
				v4 = r22;
			} else {
				if (zend_is_true(v5)) {
					PHALCON_ALLOC_ZVAL(r23);
					phalcon_concat_left(r23, " WHERE ", v5 TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r24, v4, r23);
					if (v4) {
						Z_DELREF_P(v4);
						if (!Z_REFCOUNT_P(v4)) {
							FREE_ZVAL(v4);
						}
					}
					Z_ADDREF_P(r24);
					v4 = r24;
				}
			}
		}
	}
	eval_int = phalcon_array_isset_string(v0, "group", strlen("group")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r25);
		phalcon_array_fetch_string(r25, v0, "group", strlen("group"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r26);
		phalcon_concat_both(r26,  " GROUP BY ", r25, " " TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r27, v4, r26);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r27);
		v4 = r27;
	}
	eval_int = phalcon_array_isset_string(v0, "having", strlen("having")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r28);
		phalcon_array_fetch_string(r28, v0, "having", strlen("having"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r29);
		phalcon_concat_both(r29,  " HAVING ", r28, " " TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r30, v4, r29);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r30);
		v4 = r30;
	}
	eval_int = phalcon_array_isset_string(v0, "order", strlen("order")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r31);
		phalcon_array_fetch_string(r31, v0, "order", strlen("order"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r32);
		phalcon_concat_both(r32,  " ORDER BY ", r31, " " TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r33, v4, r32);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r33);
		v4 = r33;
	}
	eval_int = phalcon_array_isset_string(v0, "limit", strlen("limit")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r34);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "connection", sizeof("connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		Z_ADDREF_P(v4);
		p5[0] = v4;
		PHALCON_ALLOC_ZVAL(r35);
		phalcon_array_fetch_string(r35, v0, "limit", strlen("limit"), PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r35);
		p5[1] = r35;
		PHALCON_CALL_METHOD_PARAMS(r34, t2, "_limit", 2, p5, PHALCON_CALL_DEFAULT);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r34);
		v4 = r34;
	}
	PHALCON_ALLOC_ZVAL(r36);
	Z_ADDREF_P(v0);
	p6[0] = v0;
	Z_ADDREF_P(v4);
	p6[1] = v4;
	PHALCON_ALLOC_ZVAL(p6[2]);
	ZVAL_STRING(p6[2], "rowcount", 1);
	PHALCON_CALL_METHOD_PARAMS(r36, this_ptr, "_getgroupresult", 3, p6, PHALCON_CALL_DEFAULT);
	RETURN_ZVAL(r36, 1, 0);
}

PHP_METHOD(Phalcon_Model_Base, _callEvent){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL;
	zval *t0 = NULL;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (phalcon_method_exists(this_ptr, v0 TSRMLS_CC) == SUCCESS) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_CALL_METHOD(r0, this_ptr, Z_STRVAL_P(v0), PHALCON_CALL_DEFAULT);
		if (Z_TYPE_P(r0) == IS_BOOL && !Z_BVAL_P(r0)) {
			RETURN_FALSE;
		}
	} else {
		eval_int = phalcon_isset_property_zval(this_ptr, v0 TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(t0);
			phalcon_read_property_zval(t0, this_ptr, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t0);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(t0);
			v1 = t0;
			PHALCON_ALLOC_ZVAL(r1);
			PHALCON_CALL_METHOD(r1, this_ptr, Z_STRVAL_P(v1), PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r1) == IS_BOOL && !Z_BVAL_P(r1)) {
				RETURN_FALSE;
			}
		}
	}
	RETURN_TRUE;
}

PHP_METHOD(Phalcon_Model_Base, _cancelOperation){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *v0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p4[] = { NULL };

	t0 = zend_read_static_property(phalcon_model_base_class_entry, "_disableEvents", sizeof("_disableEvents")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_INIT_FALSE(t1);
	PHALCON_EQUAL_FUNCTION(r0, t0, t1);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_operationMade", sizeof("_operationMade")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		PHALCON_GET_CLASS_CONSTANT(t3, phalcon_model_base_class_entry, "OP_DELETE");
		PHALCON_EQUAL_FUNCTION(r1, t2, t3);
		if (zend_is_true(r1)) {
			PHALCON_ALLOC_ZVAL(p0[0]);
			ZVAL_STRING(p0[0], "notDeleted", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p0, PHALCON_CALL_DEFAULT);
		} else {
			PHALCON_ALLOC_ZVAL(p1[0]);
			ZVAL_STRING(p1[0], "notSaved", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p1, PHALCON_CALL_DEFAULT);
		}
	}
	PHALCON_ALLOC_ZVAL(r2);
	PHALCON_CALL_STATIC(r2, "phalcon_transaction_manager", "isautomatic");
	if (zend_is_true(r2)) {
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_STATIC(r3, "phalcon_transaction_manager", "getautomatic");
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(r3);
		v0 = r3;
		Z_ADDREF_P(this_ptr);
		p4[0] = this_ptr;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setrollbackedrecord", 1, p4, PHALCON_CALL_DEFAULT);
		PHALCON_CALL_METHOD_NORETURN(v0, "rollback", PHALCON_CALL_DEFAULT);
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Appends a customized message on the validation process
 *
 * 
 *
 * @param Php_Model_Message $message
 */
PHP_METHOD(Phalcon_Model_Base, appendMessage){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *i0 = NULL, *i1 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL }, *p2[] = { NULL }, *p1[] = { NULL }, *p4[] = { NULL }, *p3[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) == IS_OBJECT) {
		PHALCON_ALLOC_ZVAL(r0);
		p0[0] = v0;
		PHALCON_CALL_FUNC_PARAMS(r0, "get_class", 1, p0);
		if (!phalcon_compare_strict_string(r0, "Phalcon_Model_Message")) {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_model_exception_class_entry);
			PHALCON_ALLOC_ZVAL(r1);
			p2[0] = v0;
			PHALCON_CALL_FUNC_PARAMS(r1, "get_class", 1, p2);
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_concat_both(r2,  "Invalid message format '", r1, "'" TSRMLS_CC);
			Z_ADDREF_P(r2);
			p1[0] = r2;
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		}
	} else {
		PHALCON_ALLOC_ZVAL(i1);
		object_init_ex(i1, phalcon_model_exception_class_entry);
		PHALCON_ALLOC_ZVAL(r3);
		Z_ADDREF_P(v0);
		p4[0] = v0;
		PHALCON_CALL_FUNC_PARAMS(r3, "gettype", 1, p4);
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_concat_both(r4,  "Invalid message format '", r3, "'" TSRMLS_CC);
		Z_ADDREF_P(r4);
		p3[0] = r4;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p3, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i1 TSRMLS_CC);
		Z_ADDREF_P(i1);
		return;
	}
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_errorMessages", sizeof("_errorMessages")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	{
		zval *orig_ptr = t0;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			Z_DELREF_P(orig_ptr);
			ALLOC_ZVAL(t0);
			*t0 = *orig_ptr;
			zval_copy_ctor(t0);
			Z_SET_REFCOUNT_P(t0, 1);
			Z_UNSET_ISREF_P(t0);
		}
	}
	phalcon_array_append(t0, v0 TSRMLS_CC);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_errorMessages", strlen("_errorMessages"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Returns all the validation messages
 *
 * 
 *
 * @return array
 */
PHP_METHOD(Phalcon_Model_Base, getMessages){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_errorMessages", sizeof("_errorMessages")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Inserts or updates a model instance. Returns true on success or else false .
 *
 * 
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Base, save){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL, *v10 = NULL, *v11 = NULL, *v12 = NULL, *v13 = NULL;
	zval *v14 = NULL, *v15 = NULL, *v16 = NULL, *v17 = NULL, *v18 = NULL, *v19 = NULL, *v20 = NULL;
	zval *v21 = NULL, *v22 = NULL, *v23 = NULL, *v24 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL, *r37 = NULL, *r38 = NULL, *r39 = NULL, *r40 = NULL, *r41 = NULL;
	zval *r42 = NULL, *r43 = NULL, *r44 = NULL, *r45 = NULL, *r46 = NULL, *r47 = NULL, *r48 = NULL;
	zval *r49 = NULL, *r50 = NULL, *r51 = NULL, *r52 = NULL, *r53 = NULL, *r54 = NULL, *r55 = NULL;
	zval *r56 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL, *t10 = NULL, *t11 = NULL, *t12 = NULL, *t13 = NULL;
	zval *t14 = NULL, *t15 = NULL, *t16 = NULL, *t17 = NULL, *t18 = NULL, *t19 = NULL, *t20 = NULL;
	zval *t21 = NULL, *t22 = NULL, *t23 = NULL, *t24 = NULL, *t25 = NULL, *t26 = NULL;
	zval *a0 = NULL, *a1 = NULL, *a2 = NULL, *a3 = NULL, *a4 = NULL;
	zval *i0 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *p2[] = { NULL }, *p4[] = { NULL }, *p6[] = { NULL }, *p10[] = { NULL }, *p11[] = { NULL }, *p12[] = { NULL, NULL, NULL }, *p13[] = { NULL, NULL, NULL }, *p14[] = { NULL }, *p16[] = { NULL }, *p17[] = { NULL }, *p19[] = { NULL }, *p21[] = { NULL }, *p23[] = { NULL }, *p25[] = { NULL }, *p27[] = { NULL }, *p29[] = { NULL }, *p38[] = { NULL }, *p39[] = { NULL, NULL, NULL, NULL }, *p42[] = { NULL }, *p43[] = { NULL }, *p44[] = { NULL, NULL, NULL }, *p48[] = { NULL }, *p49[] = { NULL }, *p50[] = { NULL }, *p51[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	int eval_int;

	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "_exists", PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	if (!zend_is_true(v0)) {
		PHALCON_GET_CLASS_CONSTANT(t0, phalcon_model_base_class_entry, "OP_CREATE");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_operationMade", strlen("_operationMade"), copy TSRMLS_CC);
		}
	} else {
		PHALCON_GET_CLASS_CONSTANT(t1, phalcon_model_base_class_entry, "OP_UPDATE");
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, t1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_operationMade", strlen("_operationMade"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, a0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_errorMessages", strlen("_errorMessages"), copy TSRMLS_CC);
	}
	t2 = zend_read_static_property(phalcon_model_base_class_entry, "_disableEvents", sizeof("_disableEvents")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(t2);
	v1 = t2;
	if (!zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(p2[0]);
		ZVAL_STRING(p2[0], "beforeValidation", 1);
		PHALCON_CALL_METHOD_PARAMS(r1, this_ptr, "_callevent", 1, p2, PHALCON_CALL_DEFAULT);
		if (Z_TYPE_P(r1) == IS_BOOL && !Z_BVAL_P(r1)) {
			PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
			RETURN_FALSE;
		}
		if (!zend_is_true(v0)) {
			PHALCON_ALLOC_ZVAL(r2);
			PHALCON_ALLOC_ZVAL(p4[0]);
			ZVAL_STRING(p4[0], "beforeValidationOnCreate", 1);
			PHALCON_CALL_METHOD_PARAMS(r2, this_ptr, "_callevent", 1, p4, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r2) == IS_BOOL && !Z_BVAL_P(r2)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		} else {
			PHALCON_ALLOC_ZVAL(r3);
			PHALCON_ALLOC_ZVAL(p6[0]);
			ZVAL_STRING(p6[0], "beforeValidationOnUpdate", 1);
			PHALCON_CALL_METHOD_PARAMS(r3, this_ptr, "_callevent", 1, p6, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r3) == IS_BOOL && !Z_BVAL_P(r3)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		}
	}
	PHALCON_INIT_NULL(v2);
	PHALCON_ALLOC_ZVAL(r4);
	PHALCON_CALL_METHOD(r4, this_ptr, "_getnotnullattributes", PHALCON_CALL_DEFAULT);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r4);
	v3 = r4;
	PHALCON_ALLOC_ZVAL(r5);
	PHALCON_CALL_METHOD(r5, this_ptr, "_getdatatypesnumeric", PHALCON_CALL_DEFAULT);
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r5);
	v4 = r5;
	if (Z_TYPE_P(v3) == IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(v5);
		ZVAL_BOOL(v5, 0);
		PHALCON_ALLOC_ZVAL(r6);
		p10[0] = v3;
		PHALCON_CALL_FUNC_PARAMS(r6, "count", 1, p10);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(r6);
		v6 = r6;
		PHALCON_ALLOC_ZVAL(v7);
		ZVAL_LONG(v7, 0);
		fs38:
		PHALCON_SMALLER_FUNCTION(r7, v7, v6);
		if (!zend_is_true(r7)) {
			goto fe38;
		}
		if (!v8) {
			PHALCON_ALLOC_ZVAL(v8);
		} else {
			if (Z_REFCOUNT_P(v8) > 1) {
				SEPARATE_ZVAL(&v8);
			} else {
				if (Z_TYPE_P(v8) != IS_BOOL) {
					FREE_ZVAL(v8);
					PHALCON_ALLOC_ZVAL(v8);
				}
			}
		}
		ZVAL_BOOL(v8, 0);
		if (!r8) {
			PHALCON_ALLOC_ZVAL(r8);
		} else {
			if (Z_REFCOUNT_P(r8) > 1) {
				{
					zval *orig_ptr = r8;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(r8);
						*r8 = *orig_ptr;
						zval_copy_ctor(r8);
						Z_SET_REFCOUNT_P(r8, 1);
						Z_UNSET_ISREF_P(r8);
					}
				}
			} else {
				FREE_ZVAL(r8);
				PHALCON_ALLOC_ZVAL(r8);
			}
		}
		phalcon_array_fetch(r8, v3, v7, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v9) {
			Z_DELREF_P(v9);
			if (!Z_REFCOUNT_P(v9)) {
				FREE_ZVAL(v9);
			}
		}
		Z_ADDREF_P(r8);
		v9 = r8;
		if (!t3) {
			PHALCON_ALLOC_ZVAL(t3);
		} else {
			if (Z_REFCOUNT_P(t3) > 1) {
				{
					zval *orig_ptr = t3;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t3);
						*t3 = *orig_ptr;
						zval_copy_ctor(t3);
						Z_SET_REFCOUNT_P(t3, 1);
						Z_UNSET_ISREF_P(t3);
					}
				}
			} else {
				FREE_ZVAL(t3);
				PHALCON_ALLOC_ZVAL(t3);
			}
		}
		phalcon_read_property_zval(t3, this_ptr, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		if (v10) {
			Z_DELREF_P(v10);
			if (!Z_REFCOUNT_P(v10)) {
				FREE_ZVAL(v10);
			}
		}
		Z_ADDREF_P(t3);
		v10 = t3;
		eval_int = phalcon_array_isset(v4, v9);
		if (!eval_int) {
			PHALCON_INIT_NULL(t4);
			PHALCON_IDENTICAL_FUNCTION(r9, v10, t4);
			if (!t5) {
				PHALCON_ALLOC_ZVAL(t5);
			} else {
				if (Z_REFCOUNT_P(t5) > 1) {
					SEPARATE_ZVAL(&t5);
				} else {
					if (Z_TYPE_P(t5) != IS_STRING) {
						FREE_ZVAL(t5);
						PHALCON_ALLOC_ZVAL(t5);
					}
				}
			}
			ZVAL_STRING(t5, "", 1);
			PHALCON_IDENTICAL_FUNCTION(r10, v10, t5);
			PHALCON_OR_FUNCTION(r11, r9, r10);
			if (zend_is_true(r11)) {
				if (!v8) {
					PHALCON_ALLOC_ZVAL(v8);
				} else {
					if (Z_REFCOUNT_P(v8) > 1) {
						SEPARATE_ZVAL(&v8);
					} else {
						if (Z_TYPE_P(v8) != IS_BOOL) {
							FREE_ZVAL(v8);
							PHALCON_ALLOC_ZVAL(v8);
						}
					}
				}
				ZVAL_BOOL(v8, 1);
			}
		} else {
			if (!r12) {
				PHALCON_ALLOC_ZVAL(r12);
			} else {
				if (Z_REFCOUNT_P(r12) > 1) {
					{
						zval *orig_ptr = r12;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r12);
							*r12 = *orig_ptr;
							zval_copy_ctor(r12);
							Z_SET_REFCOUNT_P(r12, 1);
							Z_UNSET_ISREF_P(r12);
						}
					}
				} else {
					FREE_ZVAL(r12);
					PHALCON_ALLOC_ZVAL(r12);
				}
			}
			Z_ADDREF_P(v10);
			p11[0] = v10;
			PHALCON_CALL_FUNC_PARAMS(r12, "is_numeric", 1, p11);
			if (!zend_is_true(r12)) {
				if (!v8) {
					PHALCON_ALLOC_ZVAL(v8);
				} else {
					if (Z_REFCOUNT_P(v8) > 1) {
						SEPARATE_ZVAL(&v8);
					} else {
						if (Z_TYPE_P(v8) != IS_BOOL) {
							FREE_ZVAL(v8);
							PHALCON_ALLOC_ZVAL(v8);
						}
					}
				}
				ZVAL_BOOL(v8, 1);
			}
		}
		PHALCON_INIT_TRUE(t6);
		PHALCON_EQUAL_FUNCTION(r13, v8, t6);
		if (zend_is_true(r13)) {
			PHALCON_BOOLEAN_NOT_FUNCTION(r14, v0);
			if (!t7) {
				PHALCON_ALLOC_ZVAL(t7);
			} else {
				if (Z_REFCOUNT_P(t7) > 1) {
					SEPARATE_ZVAL(&t7);
				} else {
					if (Z_TYPE_P(t7) != IS_STRING) {
						FREE_ZVAL(t7);
						PHALCON_ALLOC_ZVAL(t7);
					}
				}
			}
			ZVAL_STRING(t7, "id", 1);
			PHALCON_EQUAL_FUNCTION(r15, v9, t7);
			PHALCON_AND_FUNCTION(r16, r14, r15);
			if (zend_is_true(r16)) {
				goto fi38;
			}
			if (!r17) {
				PHALCON_ALLOC_ZVAL(r17);
			} else {
				if (Z_REFCOUNT_P(r17) > 1) {
					{
						zval *orig_ptr = r17;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r17);
							*r17 = *orig_ptr;
							zval_copy_ctor(r17);
							Z_SET_REFCOUNT_P(r17, 1);
							Z_UNSET_ISREF_P(r17);
						}
					}
				} else {
					FREE_ZVAL(r17);
					PHALCON_ALLOC_ZVAL(r17);
				}
			}
			if (!p12[0]) {
				PHALCON_ALLOC_ZVAL(p12[0]);
			} else {
				if (Z_REFCOUNT_P(p12[0]) > 1) {
					SEPARATE_ZVAL(&p12[0]);
				} else {
					if (Z_TYPE_P(p12[0]) != IS_STRING) {
						FREE_ZVAL(p12[0]);
						PHALCON_ALLOC_ZVAL(p12[0]);
					}
				}
			}
			ZVAL_STRING(p12[0], "_id", 1);
			if (!p12[1]) {
				PHALCON_ALLOC_ZVAL(p12[1]);
			} else {
				if (Z_REFCOUNT_P(p12[1]) > 1) {
					SEPARATE_ZVAL(&p12[1]);
				} else {
					if (Z_TYPE_P(p12[1]) != IS_STRING) {
						FREE_ZVAL(p12[1]);
						PHALCON_ALLOC_ZVAL(p12[1]);
					}
				}
			}
			ZVAL_STRING(p12[1], "", 1);
			p12[2] = v9;
			PHALCON_CALL_FUNC_PARAMS(r17, "str_replace", 3, p12);
			if (v11) {
				Z_DELREF_P(v11);
				if (!Z_REFCOUNT_P(v11)) {
					FREE_ZVAL(v11);
				}
			}
			Z_ADDREF_P(r17);
			v11 = r17;
			if (!i0) {
				PHALCON_ALLOC_ZVAL(i0);
			} else {
				if (Z_REFCOUNT_P(i0) > 1) {
					{
						zval *orig_ptr = i0;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(i0);
							*i0 = *orig_ptr;
							zval_copy_ctor(i0);
							Z_SET_REFCOUNT_P(i0, 1);
							Z_UNSET_ISREF_P(i0);
						}
					}
				} else {
					FREE_ZVAL(i0);
					PHALCON_ALLOC_ZVAL(i0);
				}
			}
			object_init_ex(i0, phalcon_model_message_class_entry);
			if (!r18) {
				PHALCON_ALLOC_ZVAL(r18);
			} else {
				if (Z_REFCOUNT_P(r18) > 1) {
					{
						zval *orig_ptr = r18;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r18);
							*r18 = *orig_ptr;
							zval_copy_ctor(r18);
							Z_SET_REFCOUNT_P(r18, 1);
							Z_UNSET_ISREF_P(r18);
						}
					}
				} else {
					FREE_ZVAL(r18);
					PHALCON_ALLOC_ZVAL(r18);
				}
			}
			phalcon_concat_right(r18, v11, " is required" TSRMLS_CC);
			Z_ADDREF_P(r18);
			p13[0] = r18;
			Z_ADDREF_P(v9);
			p13[1] = v9;
			if (!p13[2]) {
				PHALCON_ALLOC_ZVAL(p13[2]);
			} else {
				if (Z_REFCOUNT_P(p13[2]) > 1) {
					SEPARATE_ZVAL(&p13[2]);
				} else {
					if (Z_TYPE_P(p13[2]) != IS_STRING) {
						FREE_ZVAL(p13[2]);
						PHALCON_ALLOC_ZVAL(p13[2]);
					}
				}
			}
			ZVAL_STRING(p13[2], "PresenceOf", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 3, p13, PHALCON_CALL_CHECK);
			if (v12) {
				Z_DELREF_P(v12);
				if (!Z_REFCOUNT_P(v12)) {
					FREE_ZVAL(v12);
				}
			}
			Z_ADDREF_P(i0);
			v12 = i0;
			if (!t8) {
				PHALCON_ALLOC_ZVAL(t8);
			} else {
				if (Z_REFCOUNT_P(t8) > 1) {
					{
						zval *orig_ptr = t8;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(t8);
							*t8 = *orig_ptr;
							zval_copy_ctor(t8);
							Z_SET_REFCOUNT_P(t8, 1);
							Z_UNSET_ISREF_P(t8);
						}
					}
				} else {
					FREE_ZVAL(t8);
					PHALCON_ALLOC_ZVAL(t8);
				}
			}
			phalcon_read_property(t8, this_ptr, "_errorMessages", sizeof("_errorMessages")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t8);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v12, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = t8;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(t8);
						*t8 = *orig_ptr;
						zval_copy_ctor(t8);
						Z_SET_REFCOUNT_P(t8, 1);
						Z_UNSET_ISREF_P(t8);
					}
				}
				phalcon_array_append(t8, copy TSRMLS_CC);
			}
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, t8, 1, 0);
				Z_SET_REFCOUNT_P(copy, 0);
				phalcon_update_property_zval(this_ptr, "_errorMessages", strlen("_errorMessages"), copy TSRMLS_CC);
			}
			if (!v5) {
				PHALCON_ALLOC_ZVAL(v5);
			} else {
				if (Z_REFCOUNT_P(v5) > 1) {
					SEPARATE_ZVAL(&v5);
				} else {
					if (Z_TYPE_P(v5) != IS_BOOL) {
						FREE_ZVAL(v5);
						PHALCON_ALLOC_ZVAL(v5);
					}
				}
			}
			ZVAL_BOOL(v5, 1);
		}
		fi38:
		{
			zval *orig_ptr = v7;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				Z_DELREF_P(orig_ptr);
				ALLOC_ZVAL(v7);
				*v7 = *orig_ptr;
				zval_copy_ctor(v7);
				Z_SET_REFCOUNT_P(v7, 1);
				Z_UNSET_ISREF_P(v7);
			}
		}
		increment_function(v7);
		goto fs38;
		fe38:
		r7 = NULL;
		if (zend_is_true(v5)) {
			if (!zend_is_true(v1)) {
				PHALCON_ALLOC_ZVAL(p14[0]);
				ZVAL_STRING(p14[0], "onValidationFails", 1);
				PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p14, PHALCON_CALL_DEFAULT);
			}
			PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
			RETURN_FALSE;
		}
	}
	PHALCON_ALLOC_ZVAL(r19);
	PHALCON_ALLOC_ZVAL(p16[0]);
	ZVAL_STRING(p16[0], "validation", 1);
	PHALCON_CALL_METHOD_PARAMS(r19, this_ptr, "_callevent", 1, p16, PHALCON_CALL_DEFAULT);
	if (Z_TYPE_P(r19) == IS_BOOL && !Z_BVAL_P(r19)) {
		if (!zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(p17[0]);
			ZVAL_STRING(p17[0], "onValidationFails", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p17, PHALCON_CALL_DEFAULT);
		}
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
		RETURN_FALSE;
	}
	if (!zend_is_true(v1)) {
		if (!zend_is_true(v0)) {
			PHALCON_ALLOC_ZVAL(r20);
			PHALCON_ALLOC_ZVAL(p19[0]);
			ZVAL_STRING(p19[0], "afterValidationOnCreate", 1);
			PHALCON_CALL_METHOD_PARAMS(r20, this_ptr, "_callevent", 1, p19, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r20) == IS_BOOL && !Z_BVAL_P(r20)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		} else {
			PHALCON_ALLOC_ZVAL(r21);
			PHALCON_ALLOC_ZVAL(p21[0]);
			ZVAL_STRING(p21[0], "afterValidationOnUpdate", 1);
			PHALCON_CALL_METHOD_PARAMS(r21, this_ptr, "_callevent", 1, p21, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r21) == IS_BOOL && !Z_BVAL_P(r21)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		}
		PHALCON_ALLOC_ZVAL(r22);
		PHALCON_ALLOC_ZVAL(p23[0]);
		ZVAL_STRING(p23[0], "afterValidation", 1);
		PHALCON_CALL_METHOD_PARAMS(r22, this_ptr, "_callevent", 1, p23, PHALCON_CALL_DEFAULT);
		if (Z_TYPE_P(r22) == IS_BOOL && !Z_BVAL_P(r22)) {
			PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
			RETURN_FALSE;
		}
		PHALCON_ALLOC_ZVAL(r23);
		PHALCON_ALLOC_ZVAL(p25[0]);
		ZVAL_STRING(p25[0], "beforeSave", 1);
		PHALCON_CALL_METHOD_PARAMS(r23, this_ptr, "_callevent", 1, p25, PHALCON_CALL_DEFAULT);
		if (Z_TYPE_P(r23) == IS_BOOL && !Z_BVAL_P(r23)) {
			PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
			RETURN_FALSE;
		}
		if (zend_is_true(v0)) {
			PHALCON_ALLOC_ZVAL(r24);
			PHALCON_ALLOC_ZVAL(p27[0]);
			ZVAL_STRING(p27[0], "beforeUpdate", 1);
			PHALCON_CALL_METHOD_PARAMS(r24, this_ptr, "_callevent", 1, p27, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r24) == IS_BOOL && !Z_BVAL_P(r24)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		} else {
			PHALCON_ALLOC_ZVAL(r25);
			PHALCON_ALLOC_ZVAL(p29[0]);
			ZVAL_STRING(p29[0], "beforeCreate", 1);
			PHALCON_CALL_METHOD_PARAMS(r25, this_ptr, "_callevent", 1, p29, PHALCON_CALL_DEFAULT);
			if (Z_TYPE_P(r25) == IS_BOOL && !Z_BVAL_P(r25)) {
				PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
				RETURN_FALSE;
			}
		}
	}
	PHALCON_ALLOC_ZVAL(r26);
	PHALCON_CALL_METHOD(r26, this_ptr, "getschema", PHALCON_CALL_DEFAULT);
	if (v13) {
		Z_DELREF_P(v13);
		if (!Z_REFCOUNT_P(v13)) {
			FREE_ZVAL(v13);
		}
	}
	Z_ADDREF_P(r26);
	v13 = r26;
	PHALCON_ALLOC_ZVAL(r27);
	PHALCON_CALL_METHOD(r27, this_ptr, "getsource", PHALCON_CALL_DEFAULT);
	if (v14) {
		Z_DELREF_P(v14);
		if (!Z_REFCOUNT_P(v14)) {
			FREE_ZVAL(v14);
		}
	}
	Z_ADDREF_P(r27);
	v14 = r27;
	if (zend_is_true(v13)) {
		PHALCON_ALLOC_ZVAL(r28);
		phalcon_concat_vboth(r28, v13, ".", v14 TSRMLS_CC);
		if (v15) {
			Z_DELREF_P(v15);
			if (!Z_REFCOUNT_P(v15)) {
				FREE_ZVAL(v15);
			}
		}
		Z_ADDREF_P(r28);
		v15 = r28;
	} else {
		if (v15) {
			Z_DELREF_P(v15);
			if (!Z_REFCOUNT_P(v15)) {
				FREE_ZVAL(v15);
			}
		}
		Z_ADDREF_P(v14);
		v15 = v14;
	}
	PHALCON_ALLOC_ZVAL(t9);
	phalcon_read_property(t9, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t9);
	if (v16) {
		Z_DELREF_P(v16);
		if (!Z_REFCOUNT_P(v16)) {
			FREE_ZVAL(v16);
		}
	}
	Z_ADDREF_P(t9);
	v16 = t9;
	PHALCON_ALLOC_ZVAL(r29);
	PHALCON_CALL_METHOD(r29, this_ptr, "_getdatatypes", PHALCON_CALL_DEFAULT);
	if (v17) {
		Z_DELREF_P(v17);
		if (!Z_REFCOUNT_P(v17)) {
			FREE_ZVAL(v17);
		}
	}
	Z_ADDREF_P(r29);
	v17 = r29;
	PHALCON_ALLOC_ZVAL(r30);
	PHALCON_CALL_METHOD(r30, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
	if (v18) {
		Z_DELREF_P(v18);
		if (!Z_REFCOUNT_P(v18)) {
			FREE_ZVAL(v18);
		}
	}
	Z_ADDREF_P(r30);
	v18 = r30;
	PHALCON_ALLOC_ZVAL(r31);
	PHALCON_CALL_METHOD(r31, this_ptr, "_getdatatypesnumeric", PHALCON_CALL_DEFAULT);
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r31);
	v4 = r31;
	if (zend_is_true(v0)) {
		PHALCON_ALLOC_ZVAL(a1);
		array_init(a1);
		if (v19) {
			Z_DELREF_P(v19);
			if (!Z_REFCOUNT_P(v19)) {
				FREE_ZVAL(v19);
			}
		}
		Z_ADDREF_P(a1);
		v19 = a1;
		PHALCON_ALLOC_ZVAL(a2);
		array_init(a2);
		if (v20) {
			Z_DELREF_P(v20);
			if (!Z_REFCOUNT_P(v20)) {
				FREE_ZVAL(v20);
			}
		}
		Z_ADDREF_P(a2);
		v20 = a2;
		PHALCON_ALLOC_ZVAL(r32);
		PHALCON_CALL_METHOD(r32, this_ptr, "_getnonprimarykeyattributes", PHALCON_CALL_DEFAULT);
		if (v21) {
			Z_DELREF_P(v21);
			if (!Z_REFCOUNT_P(v21)) {
				FREE_ZVAL(v21);
			}
		}
		Z_ADDREF_P(r32);
		v21 = r32;
		FOREACH_V(v21, ac0, fes39, fee39, ah0, hp0, v9)
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, v9, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = v19;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v19);
						*v19 = *orig_ptr;
						zval_copy_ctor(v19);
						Z_SET_REFCOUNT_P(v19, 1);
						Z_UNSET_ISREF_P(v19);
					}
				}
				phalcon_array_append(v19, copy TSRMLS_CC);
			}
			if (!t10) {
				PHALCON_ALLOC_ZVAL(t10);
			} else {
				if (Z_REFCOUNT_P(t10) > 1) {
					{
						zval *orig_ptr = t10;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(t10);
							*t10 = *orig_ptr;
							zval_copy_ctor(t10);
							Z_SET_REFCOUNT_P(t10, 1);
							Z_UNSET_ISREF_P(t10);
						}
					}
				} else {
					FREE_ZVAL(t10);
					PHALCON_ALLOC_ZVAL(t10);
				}
			}
			phalcon_read_property_zval(t10, this_ptr, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t10);
			if (v10) {
				Z_DELREF_P(v10);
				if (!Z_REFCOUNT_P(v10)) {
					FREE_ZVAL(v10);
				}
			}
			Z_ADDREF_P(t10);
			v10 = t10;
			if (Z_TYPE_P(v10) == IS_OBJECT) {
				if (!r33) {
					PHALCON_ALLOC_ZVAL(r33);
				} else {
					if (Z_REFCOUNT_P(r33) > 1){ 
						{
							zval *orig_ptr = r33;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r33);
								*r33 = *orig_ptr;
								zval_copy_ctor(r33);
								Z_SET_REFCOUNT_P(r33, 1);
								Z_UNSET_ISREF_P(r33);
							}
						}
					}
				}
				phalcon_instance_of(r33, v10, phalcon_db_rawvalue_class_entry TSRMLS_CC);
				if (zend_is_true(r33)) {
					if (!r34) {
						PHALCON_ALLOC_ZVAL(r34);
					} else {
						if (Z_REFCOUNT_P(r34) > 1) {
							{
								zval *orig_ptr = r34;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r34);
									*r34 = *orig_ptr;
									zval_copy_ctor(r34);
									Z_SET_REFCOUNT_P(r34, 1);
									Z_UNSET_ISREF_P(r34);
								}
							}
						} else {
							FREE_ZVAL(r34);
							PHALCON_ALLOC_ZVAL(r34);
						}
					}
					PHALCON_CALL_METHOD(r34, v10, "getvalue", PHALCON_CALL_DEFAULT);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, r34, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v20;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v20);
								*v20 = *orig_ptr;
								zval_copy_ctor(v20);
								Z_SET_REFCOUNT_P(v20, 1);
								Z_UNSET_ISREF_P(v20);
							}
						}
						phalcon_array_append(v20, copy TSRMLS_CC);
					}
				} else {
					if (!t11) {
						PHALCON_ALLOC_ZVAL(t11);
					} else {
						if (Z_REFCOUNT_P(t11) > 1) {
							{
								zval *orig_ptr = t11;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(t11);
									*t11 = *orig_ptr;
									zval_copy_ctor(t11);
									Z_SET_REFCOUNT_P(t11, 1);
									Z_UNSET_ISREF_P(t11);
								}
							}
						} else {
							FREE_ZVAL(t11);
							PHALCON_ALLOC_ZVAL(t11);
						}
					}
					phalcon_read_property_zval(t11, this_ptr, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
					zval_copy_ctor(t11);
					convert_to_string(t11);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, t11, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v20;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v20);
								*v20 = *orig_ptr;
								zval_copy_ctor(v20);
								Z_SET_REFCOUNT_P(v20, 1);
								Z_UNSET_ISREF_P(v20);
							}
						}
						phalcon_array_append(v20, copy TSRMLS_CC);
					}
				}
			} else {
				if (!t12) {
					PHALCON_ALLOC_ZVAL(t12);
				} else {
					if (Z_REFCOUNT_P(t12) > 1) {
						SEPARATE_ZVAL(&t12);
					} else {
						if (Z_TYPE_P(t12) != IS_STRING) {
							FREE_ZVAL(t12);
							PHALCON_ALLOC_ZVAL(t12);
						}
					}
				}
				ZVAL_STRING(t12, "", 1);
				PHALCON_IDENTICAL_FUNCTION(r35, v10, t12);
				PHALCON_INIT_NULL(t13);
				PHALCON_IDENTICAL_FUNCTION(r36, v10, t13);
				PHALCON_OR_FUNCTION(r37, r35, r36);
				if (zend_is_true(r37)) {
					if (!t14) {
						PHALCON_ALLOC_ZVAL(t14);
					} else {
						if (Z_REFCOUNT_P(t14) > 1) {
							SEPARATE_ZVAL(&t14);
						} else {
							if (Z_TYPE_P(t14) != IS_STRING) {
								FREE_ZVAL(t14);
								PHALCON_ALLOC_ZVAL(t14);
							}
						}
					}
					ZVAL_STRING(t14, "NULL", 1);
					{
						zval *copy;
						ALLOC_ZVAL(copy);
						ZVAL_ZVAL(copy, t14, 1, 0);
						Z_SET_REFCOUNT_P(copy, 1);
						{
							zval *orig_ptr = v20;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(v20);
								*v20 = *orig_ptr;
								zval_copy_ctor(v20);
								Z_SET_REFCOUNT_P(v20, 1);
								Z_UNSET_ISREF_P(v20);
							}
						}
						phalcon_array_append(v20, copy TSRMLS_CC);
					}
				} else {
					eval_int = phalcon_array_isset(v4, v9);
					if (eval_int) {
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, v10, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v20;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(v20);
									*v20 = *orig_ptr;
									zval_copy_ctor(v20);
									Z_SET_REFCOUNT_P(v20, 1);
									Z_UNSET_ISREF_P(v20);
								}
							}
							phalcon_array_append(v20, copy TSRMLS_CC);
						}
					} else {
						if (!r38) {
							PHALCON_ALLOC_ZVAL(r38);
						} else {
							if (Z_REFCOUNT_P(r38) > 1) {
								{
									zval *orig_ptr = r38;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r38);
										*r38 = *orig_ptr;
										zval_copy_ctor(r38);
										Z_SET_REFCOUNT_P(r38, 1);
										Z_UNSET_ISREF_P(r38);
									}
								}
							} else {
								FREE_ZVAL(r38);
								PHALCON_ALLOC_ZVAL(r38);
							}
						}
						phalcon_array_fetch(r38, v17, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
						if (phalcon_compare_strict_string(r38, "date")) {
							if (!r39) {
								PHALCON_ALLOC_ZVAL(r39);
							} else {
								if (Z_REFCOUNT_P(r39) > 1) {
									{
										zval *orig_ptr = r39;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r39);
											*r39 = *orig_ptr;
											zval_copy_ctor(r39);
											Z_SET_REFCOUNT_P(r39, 1);
											Z_UNSET_ISREF_P(r39);
										}
									}
								} else {
									FREE_ZVAL(r39);
									PHALCON_ALLOC_ZVAL(r39);
								}
							}
							Z_ADDREF_P(v10);
							p38[0] = v10;
							PHALCON_CALL_METHOD_PARAMS(r39, v16, "getdateusingformat", 1, p38, PHALCON_CALL_DEFAULT);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r39, 1, 0);
								Z_SET_REFCOUNT_P(copy, 1);
								{
									zval *orig_ptr = v20;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(v20);
										*v20 = *orig_ptr;
										zval_copy_ctor(v20);
										Z_SET_REFCOUNT_P(v20, 1);
										Z_UNSET_ISREF_P(v20);
									}
								}
								phalcon_array_append(v20, copy TSRMLS_CC);
							}
						} else {
							if (!r40) {
								PHALCON_ALLOC_ZVAL(r40);
							} else {
								if (Z_REFCOUNT_P(r40) > 1) {
									{
										zval *orig_ptr = r40;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r40);
											*r40 = *orig_ptr;
											zval_copy_ctor(r40);
											Z_SET_REFCOUNT_P(r40, 1);
											Z_UNSET_ISREF_P(r40);
										}
									}
								} else {
									FREE_ZVAL(r40);
									PHALCON_ALLOC_ZVAL(r40);
								}
							}
							phalcon_concat_both(r40,  "'", v10, "'" TSRMLS_CC);
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, r40, 1, 0);
								Z_SET_REFCOUNT_P(copy, 1);
								{
									zval *orig_ptr = v20;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(v20);
										*v20 = *orig_ptr;
										zval_copy_ctor(v20);
										Z_SET_REFCOUNT_P(v20, 1);
										Z_UNSET_ISREF_P(v20);
									}
								}
								phalcon_array_append(v20, copy TSRMLS_CC);
							}
						}
					}
				}
			}
		END_FOREACH(ac0, fes39, fee39, ah0, hp0);
		PHALCON_ALLOC_ZVAL(r41);
		Z_ADDREF_P(v15);
		p39[0] = v15;
		Z_ADDREF_P(v19);
		p39[1] = v19;
		Z_ADDREF_P(v20);
		p39[2] = v20;
		PHALCON_ALLOC_ZVAL(t15);
		phalcon_read_property(t15, this_ptr, "_uniqueKey", sizeof("_uniqueKey")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t15);
		Z_ADDREF_P(t15);
		p39[3] = t15;
		PHALCON_CALL_METHOD_PARAMS(r41, v16, "update", 4, p39, PHALCON_CALL_DEFAULT);
		if (v22) {
			Z_DELREF_P(v22);
			if (!Z_REFCOUNT_P(v22)) {
				FREE_ZVAL(v22);
			}
		}
		Z_ADDREF_P(r41);
		v22 = r41;
	} else {
		PHALCON_ALLOC_ZVAL(a3);
		array_init(a3);
		if (v19) {
			Z_DELREF_P(v19);
			if (!Z_REFCOUNT_P(v19)) {
				FREE_ZVAL(v19);
			}
		}
		Z_ADDREF_P(a3);
		v19 = a3;
		PHALCON_ALLOC_ZVAL(a4);
		array_init(a4);
		if (v20) {
			Z_DELREF_P(v20);
			if (!Z_REFCOUNT_P(v20)) {
				FREE_ZVAL(v20);
			}
		}
		Z_ADDREF_P(a4);
		v20 = a4;
		PHALCON_ALLOC_ZVAL(r42);
		PHALCON_CALL_METHOD(r42, this_ptr, "_getattributes", PHALCON_CALL_DEFAULT);
		if (v23) {
			Z_DELREF_P(v23);
			if (!Z_REFCOUNT_P(v23)) {
				FREE_ZVAL(v23);
			}
		}
		Z_ADDREF_P(r42);
		v23 = r42;
		FOREACH_V(v23, ac1, fes40, fee40, ah1, hp1, v9)
			if (!phalcon_compare_strict_string(v9, "id")) {
				{
					zval *copy;
					ALLOC_ZVAL(copy);
					ZVAL_ZVAL(copy, v9, 1, 0);
					Z_SET_REFCOUNT_P(copy, 1);
					{
						zval *orig_ptr = v19;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(v19);
							*v19 = *orig_ptr;
							zval_copy_ctor(v19);
							Z_SET_REFCOUNT_P(v19, 1);
							Z_UNSET_ISREF_P(v19);
						}
					}
					phalcon_array_append(v19, copy TSRMLS_CC);
				}
				if (!t16) {
					PHALCON_ALLOC_ZVAL(t16);
				} else {
					if (Z_REFCOUNT_P(t16) > 1) {
						{
							zval *orig_ptr = t16;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(t16);
								*t16 = *orig_ptr;
								zval_copy_ctor(t16);
								Z_SET_REFCOUNT_P(t16, 1);
								Z_UNSET_ISREF_P(t16);
							}
						}
					} else {
						FREE_ZVAL(t16);
						PHALCON_ALLOC_ZVAL(t16);
					}
				}
				phalcon_read_property_zval(t16, this_ptr, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
				zval_copy_ctor(t16);
				if (v10) {
					Z_DELREF_P(v10);
					if (!Z_REFCOUNT_P(v10)) {
						FREE_ZVAL(v10);
					}
				}
				Z_ADDREF_P(t16);
				v10 = t16;
				if (Z_TYPE_P(v10) == IS_OBJECT) {
					if (!r43) {
						PHALCON_ALLOC_ZVAL(r43);
					} else {
						if (Z_REFCOUNT_P(r43) > 1){ 
							{
								zval *orig_ptr = r43;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r43);
									*r43 = *orig_ptr;
									zval_copy_ctor(r43);
									Z_SET_REFCOUNT_P(r43, 1);
									Z_UNSET_ISREF_P(r43);
								}
							}
						}
					}
					phalcon_instance_of(r43, v10, phalcon_db_rawvalue_class_entry TSRMLS_CC);
					if (zend_is_true(r43)) {
						if (!r44) {
							PHALCON_ALLOC_ZVAL(r44);
						} else {
							if (Z_REFCOUNT_P(r44) > 1) {
								{
									zval *orig_ptr = r44;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r44);
										*r44 = *orig_ptr;
										zval_copy_ctor(r44);
										Z_SET_REFCOUNT_P(r44, 1);
										Z_UNSET_ISREF_P(r44);
									}
								}
							} else {
								FREE_ZVAL(r44);
								PHALCON_ALLOC_ZVAL(r44);
							}
						}
						PHALCON_CALL_METHOD(r44, v10, "getvalue", PHALCON_CALL_DEFAULT);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, r44, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v20;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(v20);
									*v20 = *orig_ptr;
									zval_copy_ctor(v20);
									Z_SET_REFCOUNT_P(v20, 1);
									Z_UNSET_ISREF_P(v20);
								}
							}
							phalcon_array_append(v20, copy TSRMLS_CC);
						}
					} else {
						convert_to_string(v10);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, v10, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v20;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(v20);
									*v20 = *orig_ptr;
									zval_copy_ctor(v20);
									Z_SET_REFCOUNT_P(v20, 1);
									Z_UNSET_ISREF_P(v20);
								}
							}
							phalcon_array_append(v20, copy TSRMLS_CC);
						}
					}
				} else {
					if (!t17) {
						PHALCON_ALLOC_ZVAL(t17);
					} else {
						if (Z_REFCOUNT_P(t17) > 1) {
							SEPARATE_ZVAL(&t17);
						} else {
							if (Z_TYPE_P(t17) != IS_STRING) {
								FREE_ZVAL(t17);
								PHALCON_ALLOC_ZVAL(t17);
							}
						}
					}
					ZVAL_STRING(t17, "", 1);
					PHALCON_IDENTICAL_FUNCTION(r45, v10, t17);
					PHALCON_INIT_NULL(t18);
					PHALCON_IDENTICAL_FUNCTION(r46, v10, t18);
					PHALCON_OR_FUNCTION(r47, r45, r46);
					if (zend_is_true(r47)) {
						if (!t19) {
							PHALCON_ALLOC_ZVAL(t19);
						} else {
							if (Z_REFCOUNT_P(t19) > 1) {
								SEPARATE_ZVAL(&t19);
							} else {
								if (Z_TYPE_P(t19) != IS_STRING) {
									FREE_ZVAL(t19);
									PHALCON_ALLOC_ZVAL(t19);
								}
							}
						}
						ZVAL_STRING(t19, "NULL", 1);
						{
							zval *copy;
							ALLOC_ZVAL(copy);
							ZVAL_ZVAL(copy, t19, 1, 0);
							Z_SET_REFCOUNT_P(copy, 1);
							{
								zval *orig_ptr = v20;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(v20);
									*v20 = *orig_ptr;
									zval_copy_ctor(v20);
									Z_SET_REFCOUNT_P(v20, 1);
									Z_UNSET_ISREF_P(v20);
								}
							}
							phalcon_array_append(v20, copy TSRMLS_CC);
						}
					} else {
						eval_int = phalcon_array_isset(v4, v9);
						if (eval_int) {
							{
								zval *copy;
								ALLOC_ZVAL(copy);
								ZVAL_ZVAL(copy, v10, 1, 0);
								Z_SET_REFCOUNT_P(copy, 1);
								{
									zval *orig_ptr = v20;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(v20);
										*v20 = *orig_ptr;
										zval_copy_ctor(v20);
										Z_SET_REFCOUNT_P(v20, 1);
										Z_UNSET_ISREF_P(v20);
									}
								}
								phalcon_array_append(v20, copy TSRMLS_CC);
							}
						} else {
							if (!r48) {
								PHALCON_ALLOC_ZVAL(r48);
							} else {
								if (Z_REFCOUNT_P(r48) > 1) {
									{
										zval *orig_ptr = r48;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(r48);
											*r48 = *orig_ptr;
											zval_copy_ctor(r48);
											Z_SET_REFCOUNT_P(r48, 1);
											Z_UNSET_ISREF_P(r48);
										}
									}
								} else {
									FREE_ZVAL(r48);
									PHALCON_ALLOC_ZVAL(r48);
								}
							}
							phalcon_array_fetch(r48, v17, v9, PHALCON_NOISY_FETCH TSRMLS_CC);
							if (phalcon_compare_strict_string(r48, "date")) {
								if (!r49) {
									PHALCON_ALLOC_ZVAL(r49);
								} else {
									if (Z_REFCOUNT_P(r49) > 1) {
										{
											zval *orig_ptr = r49;
											if (Z_REFCOUNT_P(orig_ptr) > 1) {
												Z_DELREF_P(orig_ptr);
												ALLOC_ZVAL(r49);
												*r49 = *orig_ptr;
												zval_copy_ctor(r49);
												Z_SET_REFCOUNT_P(r49, 1);
												Z_UNSET_ISREF_P(r49);
											}
										}
									} else {
										FREE_ZVAL(r49);
										PHALCON_ALLOC_ZVAL(r49);
									}
								}
								if (!t20) {
									PHALCON_ALLOC_ZVAL(t20);
								} else {
									if (Z_REFCOUNT_P(t20) > 1) {
										{
											zval *orig_ptr = t20;
											if (Z_REFCOUNT_P(orig_ptr) > 1) {
												Z_DELREF_P(orig_ptr);
												ALLOC_ZVAL(t20);
												*t20 = *orig_ptr;
												zval_copy_ctor(t20);
												Z_SET_REFCOUNT_P(t20, 1);
												Z_UNSET_ISREF_P(t20);
											}
										}
									} else {
										FREE_ZVAL(t20);
										PHALCON_ALLOC_ZVAL(t20);
									}
								}
								phalcon_read_property(t20, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
								zval_copy_ctor(t20);
								Z_ADDREF_P(v10);
								p42[0] = v10;
								PHALCON_CALL_METHOD_PARAMS(r49, t20, "getdateusingformat", 1, p42, PHALCON_CALL_DEFAULT);
								{
									zval *copy;
									ALLOC_ZVAL(copy);
									ZVAL_ZVAL(copy, r49, 1, 0);
									Z_SET_REFCOUNT_P(copy, 1);
									{
										zval *orig_ptr = v20;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(v20);
											*v20 = *orig_ptr;
											zval_copy_ctor(v20);
											Z_SET_REFCOUNT_P(v20, 1);
											Z_UNSET_ISREF_P(v20);
										}
									}
									phalcon_array_append(v20, copy TSRMLS_CC);
								}
							} else {
								if (!r50) {
									PHALCON_ALLOC_ZVAL(r50);
								} else {
									if (Z_REFCOUNT_P(r50) > 1) {
										{
											zval *orig_ptr = r50;
											if (Z_REFCOUNT_P(orig_ptr) > 1) {
												Z_DELREF_P(orig_ptr);
												ALLOC_ZVAL(r50);
												*r50 = *orig_ptr;
												zval_copy_ctor(r50);
												Z_SET_REFCOUNT_P(r50, 1);
												Z_UNSET_ISREF_P(r50);
											}
										}
									} else {
										FREE_ZVAL(r50);
										PHALCON_ALLOC_ZVAL(r50);
									}
								}
								phalcon_concat_both(r50,  "'", v10, "'" TSRMLS_CC);
								{
									zval *copy;
									ALLOC_ZVAL(copy);
									ZVAL_ZVAL(copy, r50, 1, 0);
									Z_SET_REFCOUNT_P(copy, 1);
									{
										zval *orig_ptr = v20;
										if (Z_REFCOUNT_P(orig_ptr) > 1) {
											Z_DELREF_P(orig_ptr);
											ALLOC_ZVAL(v20);
											*v20 = *orig_ptr;
											zval_copy_ctor(v20);
											Z_SET_REFCOUNT_P(v20, 1);
											Z_UNSET_ISREF_P(v20);
										}
									}
									phalcon_array_append(v20, copy TSRMLS_CC);
								}
							}
						}
					}
				}
			}
		END_FOREACH(ac1, fes40, fee40, ah1, hp1);
		PHALCON_ALLOC_ZVAL(v24);
		ZVAL_STRING(v24, "", 0);
		eval_int = phalcon_isset_property(this_ptr, "id", strlen("id") TSRMLS_CC);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r51);
			PHALCON_ALLOC_ZVAL(t21);
			phalcon_read_property(t21, this_ptr, "id", sizeof("id")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t21);
			Z_ADDREF_P(t21);
			p43[0] = t21;
			PHALCON_CALL_FUNC_PARAMS(r51, "is_null", 1, p43);
			PHALCON_ALLOC_ZVAL(t22);
			phalcon_read_property(t22, this_ptr, "id", sizeof("id")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t22);
			PHALCON_ALLOC_ZVAL(t23);
			ZVAL_STRING(t23, "", 1);
			PHALCON_IDENTICAL_FUNCTION(r52, t22, t23);
			PHALCON_OR_FUNCTION(r53, r51, r52);
			if (zend_is_true(r53)) {
				phalcon_update_property_string(this_ptr, "id", strlen("id"), "null" TSRMLS_CC);
			}
			PHALCON_ALLOC_ZVAL(t24);
			ZVAL_STRING(t24, "id", 1);
			{
				zval *orig_ptr = v19;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v19);
					*v19 = *orig_ptr;
					zval_copy_ctor(v19);
					Z_SET_REFCOUNT_P(v19, 1);
					Z_UNSET_ISREF_P(v19);
				}
			}
			phalcon_array_append(v19, t24 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(t25);
			phalcon_read_property(t25, this_ptr, "id", sizeof("id")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t25);
			{
				zval *orig_ptr = v20;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(v20);
					*v20 = *orig_ptr;
					zval_copy_ctor(v20);
					Z_SET_REFCOUNT_P(v20, 1);
					Z_UNSET_ISREF_P(v20);
				}
			}
			phalcon_array_append(v20, t25 TSRMLS_CC);
		}
		PHALCON_ALLOC_ZVAL(r54);
		PHALCON_ALLOC_ZVAL(t26);
		phalcon_read_property(t26, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t26);
		Z_ADDREF_P(v15);
		p44[0] = v15;
		Z_ADDREF_P(v20);
		p44[1] = v20;
		Z_ADDREF_P(v19);
		p44[2] = v19;
		PHALCON_CALL_METHOD_PARAMS(r54, t26, "insert", 3, p44, PHALCON_CALL_DEFAULT);
		if (v22) {
			Z_DELREF_P(v22);
			if (!Z_REFCOUNT_P(v22)) {
				FREE_ZVAL(v22);
			}
		}
		Z_ADDREF_P(r54);
		v22 = r54;
	}
	PHALCON_ALLOC_ZVAL(r55);
	PHALCON_CALL_METHOD(r55, v16, "isundertransaction", PHALCON_CALL_DEFAULT);
	if (!zend_is_true(r55)) {
		PHALCON_ALLOC_ZVAL(r56);
		PHALCON_CALL_METHOD(r56, v16, "gethaveautocommit", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r56)) {
			PHALCON_CALL_METHOD_NORETURN(v16, "commit", PHALCON_CALL_DEFAULT);
		}
	}
	if (zend_is_true(v22)) {
		if (zend_is_true(v0)) {
			if (!zend_is_true(v1)) {
				PHALCON_ALLOC_ZVAL(p48[0]);
				ZVAL_STRING(p48[0], "afterUpdate", 1);
				PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p48, PHALCON_CALL_DEFAULT);
			}
		} else {
			if (!zend_is_true(v1)) {
				PHALCON_ALLOC_ZVAL(p49[0]);
				ZVAL_STRING(p49[0], "afterCreate", 1);
				PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p49, PHALCON_CALL_DEFAULT);
			}
		}
		if (!zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(p50[0]);
			ZVAL_STRING(p50[0], "afterSave", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p50, PHALCON_CALL_DEFAULT);
		}
		if (Z_TYPE_P(v22) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v22);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(v22);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		if (!zend_is_true(v1)) {
			PHALCON_ALLOC_ZVAL(p51[0]);
			ZVAL_STRING(p51[0], "notSave", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p51, PHALCON_CALL_DEFAULT);
		}
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "_canceloperation", PHALCON_CALL_DEFAULT);
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Deletes a model instance. Returns true on success or else false .
 *
 * 
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Base, delete){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL;
	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL;
	zval *a0 = NULL;
	zval *ac0 = NULL;
	zval *p2[] = { NULL }, *p3[] = { NULL, NULL }, *p4[] = { NULL }, *p5[] = { NULL, NULL }, *p6[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;

	PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(t1);
		phalcon_read_property(t1, this_ptr, "_schema", sizeof("_schema")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_concat_vboth(r0, t1, ".", t2 TSRMLS_CC);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(r0);
		v0 = r0;
	} else {
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_source", sizeof("_source")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(t3);
		v0 = t3;
	}
	PHALCON_GET_CLASS_CONSTANT(t4, phalcon_model_base_class_entry, "OP_DELETE");
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t4, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_operationMade", strlen("_operationMade"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_CALL_METHOD(r1, this_ptr, "_getprimarykeyattributes", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r1);
	v1 = r1;
	PHALCON_ALLOC_ZVAL(r2);
	p2[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r2, "count", 1, p2);
	PHALCON_ALLOC_ZVAL(t5);
	ZVAL_LONG(t5, 1);
	PHALCON_EQUAL_FUNCTION(r3, r2, t5);
	if (zend_is_true(r3)) {
		PHALCON_ALLOC_ZVAL(t6);
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch_long(r4, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		phalcon_read_property_zval(t6, this_ptr, r4, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t6);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(t6);
		v2 = t6;
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_array_fetch_long(r6, v1, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r5);
		phalcon_concat_vboth(r5, r6, " = '", v2 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r7);
		phalcon_concat_right(r7, r5, "'" TSRMLS_CC);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r7);
		v3 = r7;
	} else {
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(a0);
		v3 = a0;
		FOREACH_V(v1, ac0, fes41, fee41, ah0, hp0, v4)
			if (!t7) {
				PHALCON_ALLOC_ZVAL(t7);
			} else {
				if (Z_REFCOUNT_P(t7) > 1) {
					{
						zval *orig_ptr = t7;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(t7);
							*t7 = *orig_ptr;
							zval_copy_ctor(t7);
							Z_SET_REFCOUNT_P(t7, 1);
							Z_UNSET_ISREF_P(t7);
						}
					}
				} else {
					FREE_ZVAL(t7);
					PHALCON_ALLOC_ZVAL(t7);
				}
			}
			phalcon_read_property_zval(t7, this_ptr, v4, PHALCON_NOISY_FETCH TSRMLS_CC);
			zval_copy_ctor(t7);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(t7);
			v2 = t7;
			if (!r8) {
				PHALCON_ALLOC_ZVAL(r8);
			} else {
				if (Z_REFCOUNT_P(r8) > 1) {
					{
						zval *orig_ptr = r8;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r8);
							*r8 = *orig_ptr;
							zval_copy_ctor(r8);
							Z_SET_REFCOUNT_P(r8, 1);
							Z_UNSET_ISREF_P(r8);
						}
					}
				} else {
					FREE_ZVAL(r8);
					PHALCON_ALLOC_ZVAL(r8);
				}
			}
			phalcon_concat_vboth(r8, v4, " = '", v2 TSRMLS_CC);
			if (!r9) {
				PHALCON_ALLOC_ZVAL(r9);
			} else {
				if (Z_REFCOUNT_P(r9) > 1) {
					{
						zval *orig_ptr = r9;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r9);
							*r9 = *orig_ptr;
							zval_copy_ctor(r9);
							Z_SET_REFCOUNT_P(r9, 1);
							Z_UNSET_ISREF_P(r9);
						}
					}
				} else {
					FREE_ZVAL(r9);
					PHALCON_ALLOC_ZVAL(r9);
				}
			}
			phalcon_concat_right(r9, r8, "'" TSRMLS_CC);
			{
				zval *copy;
				ALLOC_ZVAL(copy);
				ZVAL_ZVAL(copy, r9, 1, 0);
				Z_SET_REFCOUNT_P(copy, 1);
				{
					zval *orig_ptr = v3;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						Z_DELREF_P(orig_ptr);
						ALLOC_ZVAL(v3);
						*v3 = *orig_ptr;
						zval_copy_ctor(v3);
						Z_SET_REFCOUNT_P(v3, 1);
						Z_UNSET_ISREF_P(v3);
					}
				}
				phalcon_array_append(v3, copy TSRMLS_CC);
			}
		END_FOREACH(ac0, fes41, fee41, ah0, hp0);
		PHALCON_ALLOC_ZVAL(r10);
		PHALCON_ALLOC_ZVAL(p3[0]);
		ZVAL_STRING(p3[0], " AND ", 1);
		p3[1] = v3;
		PHALCON_CALL_FUNC_PARAMS(r10, "join", 2, p3);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r10);
		v3 = r10;
	}
	t8 = zend_read_static_property(phalcon_model_base_class_entry, "_disableEvents", sizeof("_disableEvents")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (v5) {
		Z_DELREF_P(v5);
		if (!Z_REFCOUNT_P(v5)) {
			FREE_ZVAL(v5);
		}
	}
	Z_ADDREF_P(t8);
	v5 = t8;
	if (!zend_is_true(v5)) {
		PHALCON_ALLOC_ZVAL(r11);
		PHALCON_ALLOC_ZVAL(p4[0]);
		ZVAL_STRING(p4[0], "beforeDelete", 1);
		PHALCON_CALL_METHOD_PARAMS(r11, this_ptr, "_callevent", 1, p4, PHALCON_CALL_DEFAULT);
		if (Z_TYPE_P(r11) == IS_BOOL && !Z_BVAL_P(r11)) {
			RETURN_FALSE;
		}
	}
	PHALCON_ALLOC_ZVAL(r12);
	PHALCON_ALLOC_ZVAL(t9);
	phalcon_read_property(t9, this_ptr, "_connection", sizeof("_connection")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t9);
	Z_ADDREF_P(v0);
	p5[0] = v0;
	Z_ADDREF_P(v3);
	p5[1] = v3;
	PHALCON_CALL_METHOD_PARAMS(r12, t9, "delete", 2, p5, PHALCON_CALL_DEFAULT);
	if (v6) {
		Z_DELREF_P(v6);
		if (!Z_REFCOUNT_P(v6)) {
			FREE_ZVAL(v6);
		}
	}
	Z_ADDREF_P(r12);
	v6 = r12;
	if (zend_is_true(v6)) {
		if (!zend_is_true(v5)) {
			PHALCON_ALLOC_ZVAL(p6[0]);
			ZVAL_STRING(p6[0], "afterDelete", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "_callevent", 1, p6, PHALCON_CALL_DEFAULT);
		}
	}
	if (Z_TYPE_P(v6) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v6);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(v6);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Reads an attribute value by its name
 *
 * 
 *
 * @param string $attribute
 * @return mixed
 */
PHP_METHOD(Phalcon_Model_Base, readAttribute){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dump", sizeof("_dump")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	}
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property_zval(t1, this_ptr, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Writes an attribute value by its name
 *
 * 
 *
 * @param string $attribute
 * @param mixed $value
 */
PHP_METHOD(Phalcon_Model_Base, writeAttribute){

	zval *v0 = NULL, *v1 = NULL;
	zval *t0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_dumped", sizeof("_dumped")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (!zend_is_true(t0)) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "_connect", PHALCON_CALL_DEFAULT);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, Z_STRVAL_P(v0), Z_STRLEN_P(v0), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Php_Model_MetaData_Memory
 *
 * Stores models meta-data in memory. Data will be ereased at the end of request
 */

/**
 * Php_Model_Resultset
 *
 * This component allows to Php_Model_Base returns large resulsets with the minimum memory comsuption
 *
 * Resulsets can be traversed using a standard foreach or a while statement.
 *
 * 
 *
 */

/**
 * Php_Model_Resultset constructor
 *
 * @param Php_Model_Base $model
 * @param resource $resultResource
 */
PHP_METHOD(Phalcon_Model_Resultset, __construct){

	zval *v0 = NULL, *v1 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_model", strlen("_model"), copy TSRMLS_CC);
	}
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v1, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_resultResource", strlen("_resultResource"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Check whether internal resource has rows to fetch
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Model_Resultset, valid){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *p1[] = { NULL }, *p2[] = { NULL }, *p3[] = { NULL }, *p4[] = { NULL }, *p5[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) == IS_BOOL && !Z_BVAL_P(t0)) {
		RETURN_FALSE;
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	PHALCON_CALL_METHOD(r0, t1, "getconnection", PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_GET_CLASS_CONSTANT(t2, phalcon_db_class_entry, "DB_ASSOC");
	Z_ADDREF_P(t2);
	p1[0] = t2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setfetchmode", 1, p1, PHALCON_CALL_DEFAULT);
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(t3);
	phalcon_read_property(t3, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t3);
	Z_ADDREF_P(t3);
	p2[0] = t3;
	PHALCON_CALL_METHOD_PARAMS(r1, v0, "fetcharray", 1, p2, PHALCON_CALL_DEFAULT);
	if (v1) {
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r1);
	v1 = r1;
	if (zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_ALLOC_ZVAL(t4);
		phalcon_read_property(t4, this_ptr, "_activeRow", sizeof("_activeRow")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t4);
		Z_ADDREF_P(t4);
		p3[0] = t4;
		PHALCON_CALL_FUNC_PARAMS(r2, "is_object", 1, p3);
		if (zend_is_true(r2)) {
			//Unset ObjectProperty
		}
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_ALLOC_ZVAL(t5);
		phalcon_read_property(t5, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t5);
		Z_ADDREF_P(v1);
		p4[0] = v1;
		PHALCON_CALL_METHOD_PARAMS(r3, t5, "dumpresult", 1, p4, PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r3, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_activeRow", strlen("_activeRow"), copy TSRMLS_CC);
		}
		RETURN_TRUE;
	} else {
		PHALCON_GET_CLASS_CONSTANT(t6, phalcon_db_class_entry, "DB_BOTH");
		Z_ADDREF_P(t6);
		p5[0] = t6;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "setfetchmode", 1, p5, PHALCON_CALL_DEFAULT);
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Returns current row in the resultset
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Resultset, current){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_activeRow", sizeof("_activeRow")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Moves cursor to next row in the resultset
 *
 */
PHP_METHOD(Phalcon_Model_Resultset, next){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_pointer", sizeof("_pointer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	{
		zval *orig_ptr = t0;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			Z_DELREF_P(orig_ptr);
			ALLOC_ZVAL(t0);
			*t0 = *orig_ptr;
			zval_copy_ctor(t0);
			Z_SET_REFCOUNT_P(t0, 1);
			Z_UNSET_ISREF_P(t0);
		}
	}
	increment_function(t0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, t0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_pointer", strlen("_pointer"), copy TSRMLS_CC);
	}
	RETURN_NULL();
}

/**
 * Gets pointer number to active row in the resultset
 *
 */
PHP_METHOD(Phalcon_Model_Resultset, key){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_pointer", sizeof("_pointer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Rewinds resultset to its beginning
 *
 */
PHP_METHOD(Phalcon_Model_Resultset, rewind){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *v0 = NULL;
	zval *r0 = NULL;
	zval *p1[] = { NULL, NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) == IS_BOOL && !Z_BVAL_P(t0)) {
		RETURN_FALSE;
	}
	phalcon_update_property_long(this_ptr, "pointer", strlen("pointer"), 1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	PHALCON_CALL_METHOD(r0, t1, "getconnection", PHALCON_CALL_DEFAULT);
	if (v0) {
		if (!Z_REFCOUNT_P(v0)) {
			FREE_ZVAL(v0);
		}
	}
	Z_ADDREF_P(r0);
	v0 = r0;
	PHALCON_PARAM_LONG(p1[0], 0);
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	Z_ADDREF_P(t2);
	p1[1] = t2;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v0, "dataseek", 2, p1, PHALCON_CALL_DEFAULT);
	RETURN_NULL();
}

/**
 * Changes interal pointer to a specifically position in the resultset
 */
PHP_METHOD(Phalcon_Model_Resultset, seek){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *p1[] = { NULL, NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	convert_to_long(v0);
	{
		zval *copy;
		ALLOC_ZVAL(copy);
		ZVAL_ZVAL(copy, v0, 1, 0);
		Z_SET_REFCOUNT_P(copy, 0);
		phalcon_update_property_zval(this_ptr, "_pointer", strlen("_pointer"), copy TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_CALL_METHOD(r0, t0, "getconnection", PHALCON_CALL_DEFAULT);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	convert_to_long(v0);
	Z_ADDREF_P(v0);
	p1[0] = v0;
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	Z_ADDREF_P(t1);
	p1[1] = t1;
	PHALCON_CALL_METHOD_PARAMS_NORETURN(v1, "dataseek", 2, p1, PHALCON_CALL_DEFAULT);
	RETURN_NULL();
}

/**
 * Counts how many rows are in the resultset
 *
 * @return int
 */
PHP_METHOD(Phalcon_Model_Resultset, count){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL;
	zval *v0 = NULL;
	zval *p0[] = { NULL }, *p2[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) == IS_BOOL && !Z_BVAL_P(t0)) {
		RETURN_LONG(0);
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_read_property(t1, this_ptr, "_count", sizeof("_count")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t1);
	Z_ADDREF_P(t1);
	p0[0] = t1;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_null", 1, p0);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_ALLOC_ZVAL(t2);
		phalcon_read_property(t2, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t2);
		PHALCON_CALL_METHOD(r1, t2, "getconnection", PHALCON_CALL_DEFAULT);
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(r1);
		v0 = r1;
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_read_property(t3, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
		zval_copy_ctor(t3);
		Z_ADDREF_P(t3);
		p2[0] = t3;
		PHALCON_CALL_METHOD_PARAMS(r2, v0, "numrows", 1, p2, PHALCON_CALL_DEFAULT);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, r2, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			phalcon_update_property_zval(this_ptr, "_count", strlen("_count"), copy TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(t4);
	phalcon_read_property(t4, this_ptr, "_count", sizeof("_count")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t4);
	if (Z_TYPE_P(t4) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t4);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t4);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Checks whether offset exists in the resultset
 *
 * @param int $index
 */
PHP_METHOD(Phalcon_Model_Resultset, offsetExists){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "count", PHALCON_CALL_DEFAULT);
	PHALCON_SMALLER_FUNCTION(r1, v0, r0);
	if (zend_is_true(r1)) {
		RETURN_TRUE;
	}
	RETURN_NULL();
}

/**
 * Gets row in a specifically position of the resultset
 *
 * @param int $index
 */
PHP_METHOD(Phalcon_Model_Resultset, offsetGet){

	zval *v0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval *i0 = NULL;
	zval *p1[] = { NULL }, *p4[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_METHOD(r0, this_ptr, "count", PHALCON_CALL_DEFAULT);
	PHALCON_SMALLER_FUNCTION(r1, v0, r0);
	if (zend_is_true(r1)) {
		Z_ADDREF_P(v0);
		p1[0] = v0;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "seek", 1, p1, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_CALL_METHOD(r2, this_ptr, "valid", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(r3);
			PHALCON_CALL_METHOD(r3, this_ptr, "current", PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r3, 1, 0);
		} else {
			RETURN_FALSE;
		}
	} else {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_model_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p4[0]);
		ZVAL_STRING(p4[0], "The index does not exist in the cursor", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p4, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	RETURN_NULL();
}

/**
 * Resulsets cannot be changed. It has only been implemented to meet the definition of the interface
 *
 * @param int $index
 */
PHP_METHOD(Phalcon_Model_Resultset, offsetSet){

	zval *v0 = NULL, *v1 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(i0);
	object_init_ex(i0, phalcon_model_exception_class_entry);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "Cursor is read only", 1);
	PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p0, PHALCON_CALL_CHECK);
	zend_throw_exception_object(i0 TSRMLS_CC);
	Z_ADDREF_P(i0);
	return;
}

/**
 * Resulsets cannot be changed. It has only been implemented to meet the definition of the interface
 *
 * @param int $index
 */
PHP_METHOD(Phalcon_Model_Resultset, offsetUnset){

	zval *v0 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(i0);
	object_init_ex(i0, phalcon_model_exception_class_entry);
	PHALCON_ALLOC_ZVAL(p0[0]);
	ZVAL_STRING(p0[0], "Cursor is read only", 1);
	PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p0, PHALCON_CALL_CHECK);
	zend_throw_exception_object(i0 TSRMLS_CC);
	Z_ADDREF_P(i0);
	return;
}

/**
 * Get first row in the resultset
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Resultset, getFirst){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_pointer", sizeof("_pointer")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_LONG(t1, 1);
	PHALCON_NOT_EQUAL_FUNCTION(r0, t0, t1);
	if (zend_is_true(r0)) {
		PHALCON_CALL_METHOD_NORETURN(this_ptr, "rewind", PHALCON_CALL_DEFAULT);
	}
	PHALCON_ALLOC_ZVAL(t2);
	phalcon_read_property(t2, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t2);
	PHALCON_INIT_FALSE(t3);
	PHALCON_NOT_IDENTICAL_FUNCTION(r1, t2, t3);
	if (zend_is_true(r1)) {
		PHALCON_ALLOC_ZVAL(r2);
		PHALCON_CALL_METHOD(r2, this_ptr, "valid", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(r3);
			PHALCON_CALL_METHOD(r3, this_ptr, "current", PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r3, 1, 0);
		} else {
			RETURN_FALSE;
		}
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Get last row in the resultset
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Resultset, getLast){

	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
	zval *p0[] = { NULL };

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_resultResource", sizeof("_resultResource")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	PHALCON_INIT_FALSE(t1);
	PHALCON_NOT_IDENTICAL_FUNCTION(r0, t0, t1);
	if (zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_METHOD(r1, this_ptr, "count", PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(t2);
		ZVAL_LONG(t2, 1);
		PHALCON_ALLOC_ZVAL(r2);
		sub_function(r2, r1, t2 TSRMLS_CC);
		Z_ADDREF_P(r2);
		p0[0] = r2;
		PHALCON_CALL_METHOD_PARAMS_NORETURN(this_ptr, "seek", 1, p0, PHALCON_CALL_DEFAULT);
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_CALL_METHOD(r3, this_ptr, "valid", PHALCON_CALL_DEFAULT);
		if (zend_is_true(r3)) {
			PHALCON_ALLOC_ZVAL(r4);
			PHALCON_CALL_METHOD(r4, this_ptr, "current", PHALCON_CALL_DEFAULT);
			RETURN_ZVAL(r4, 1, 0);
		} else {
			RETURN_FALSE;
		}
	} else {
		RETURN_FALSE;
	}
	RETURN_NULL();
}

/**
 * Get model which originates the resultset
 *
 * @return Php_Model_Base
 */
PHP_METHOD(Phalcon_Model_Resultset, getModel){

	zval *t0 = NULL;

	PHALCON_ALLOC_ZVAL(t0);
	phalcon_read_property(t0, this_ptr, "_model", sizeof("_model")-1, PHALCON_NOISY_FETCH TSRMLS_CC);
	zval_copy_ctor(t0);
	if (Z_TYPE_P(t0) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t0);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Php_Tag
 *
 * Php_Tag is designed to simplify building of HTML tags.
 * It provides a set of helpers to generate HTML in a dynamic way.
 * This component is an abstract class that you can extend to add more helpers.
 */

/**
 * Sets the request dispatcher. A valid dispatcher is required to generated absolute paths
 *
 * @param Php_Dispatcher $dipatcher
 */
PHP_METHOD(Phalcon_Tag, setDispatcher){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	zend_update_static_property(phalcon_tag_class_entry, "_dispatcher", sizeof("_dispatcher")-1, v0 TSRMLS_CC);
	RETURN_NULL();
}

/**
 * Internally gets the request dispatcher
 *
 * @return Php_Dispatcher
 */
PHP_METHOD(Phalcon_Tag, _getDispatcher){

	zval *t0 = NULL, *t1 = NULL;
	zval *r0 = NULL, *r1 = NULL;

	t0 = zend_read_static_property(phalcon_tag_class_entry, "_dispatcher", sizeof("_dispatcher")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (!zend_is_true(t0)) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(r1);
		PHALCON_CALL_STATIC(r1, "phalcon_controller_front", "getinstance");
		PHALCON_CALL_METHOD(r0, r1, "getdispatcher", PHALCON_CALL_DEFAULT);
		zend_update_static_property(phalcon_tag_class_entry, "_dispatcher", sizeof("_dispatcher")-1, r0 TSRMLS_CC);
	}
	t1 = zend_read_static_property(phalcon_tag_class_entry, "_dispatcher", sizeof("_dispatcher")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t1) > IS_BOOL) {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			zval_copy_ctor(return_value);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	} else {
		{
			zend_uchar is_ref = Z_ISREF_P(return_value);
			zend_uint refcount = Z_REFCOUNT_P(return_value);
			*(return_value) = *(t1);
			Z_SET_ISREF_TO_P(return_value, is_ref);
	 		Z_SET_REFCOUNT_P(return_value, refcount);
		}
	}
	return;
}

/**
 * Assigns values to tags
 *
 * 
 *
 * @param string $id
 * @param string $value
 */
PHP_METHOD(Phalcon_Tag, displayTo){

	zval *v0 = NULL, *v1 = NULL;
	zval *r0 = NULL;
	zval *i0 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	PHALCON_ALLOC_ZVAL(r0);
	Z_ADDREF_P(v1);
	p0[0] = v1;
	PHALCON_CALL_FUNC_PARAMS(r0, "is_scalar", 1, p0);
	if (!zend_is_true(r0)) {
		PHALCON_ALLOC_ZVAL(i0);
		object_init_ex(i0, phalcon_tag_exception_class_entry);
		PHALCON_ALLOC_ZVAL(p1[0]);
		ZVAL_STRING(p1[0], "Only scalar values can be assigned to UI components", 1);
		PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
		zend_throw_exception_object(i0 TSRMLS_CC);
		Z_ADDREF_P(i0);
		return;
	}
	t0 = zend_read_static_property(phalcon_tag_class_entry, "_displayValues", sizeof("_displayValues")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	if (Z_TYPE_P(t0) != IS_ARRAY) {
		convert_to_array(t0);
	}
	phalcon_array_update(t0, v0, v1 TSRMLS_CC);
	zend_update_static_property(phalcon_tag_class_entry, "_displayValues", sizeof("_displayValues")-1, t0 TSRMLS_CC);
	
	RETURN_NULL();
}

/**
 * Every helper call this function to check whether a component has a predefined
 * value using Php_Tag::displayTo or value from $_POST
 *
 * @param string $name
 * @return mixed
 */
PHP_METHOD(Phalcon_Tag, _getValueFromAction){

	zval *v0 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	zval **gv0;
	zval *p1[] = { NULL };
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_tag_class_entry, "_displayValues", sizeof("_displayValues")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(t1);
	phalcon_array_fetch(t1, t0, v0, PHALCON_SILENT_FETCH TSRMLS_CC);
	if (zend_is_true(t1)) {
		t2 = zend_read_static_property(phalcon_tag_class_entry, "_displayValues", sizeof("_displayValues")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(t3);
		phalcon_array_fetch(t3, t2, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(t3) > IS_BOOL) {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t3);
				zval_copy_ctor(return_value);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		} else {
			{
				zend_uchar is_ref = Z_ISREF_P(return_value);
				zend_uint refcount = Z_REFCOUNT_P(return_value);
				*(return_value) = *(t3);
				Z_SET_ISREF_TO_P(return_value, is_ref);
		 		Z_SET_REFCOUNT_P(return_value, refcount);
			}
		}
		return;
	} else {
		phalcon_init_global("_POST" TSRMLS_CC);
		if (&EG(symbol_table)) {
			if( zend_hash_find(&EG(symbol_table), "_POST", sizeof("_POST"), (void **) &gv0) == SUCCESS) {
				if(Z_TYPE_PP(gv0)==IS_ARRAY){
					a0 = *gv0;
				} else {
					PHALCON_INIT_ARRAY(a0);
				}
			}
		} else {
			PHALCON_INIT_ARRAY(a0);
		}
		eval_int = phalcon_array_isset(a0, v0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r0);
			PHALCON_CALL_FUNC(r0, "get_magic_quotes_gpc");
			if (!zend_is_true(r0)) {
				PHALCON_ALLOC_ZVAL(r1);
				phalcon_array_fetch(r1, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
				if (Z_TYPE_P(r1) > IS_BOOL) {
					{
						zend_uchar is_ref = Z_ISREF_P(return_value);
						zend_uint refcount = Z_REFCOUNT_P(return_value);
						*(return_value) = *(r1);
						zval_copy_ctor(return_value);
						Z_SET_ISREF_TO_P(return_value, is_ref);
				 		Z_SET_REFCOUNT_P(return_value, refcount);
					}
				} else {
					{
						zend_uchar is_ref = Z_ISREF_P(return_value);
						zend_uint refcount = Z_REFCOUNT_P(return_value);
						*(return_value) = *(r1);
						Z_SET_ISREF_TO_P(return_value, is_ref);
				 		Z_SET_REFCOUNT_P(return_value, refcount);
					}
				}
				return;
			} else {
				PHALCON_ALLOC_ZVAL(r2);
				PHALCON_ALLOC_ZVAL(r3);
				phalcon_array_fetch(r3, a0, v0, PHALCON_NOISY_FETCH TSRMLS_CC);
				Z_ADDREF_P(r3);
				p1[0] = r3;
				PHALCON_CALL_FUNC_PARAMS(r2, "stripslashes", 1, p1);
				RETURN_ZVAL(r2, 1, 0);
			}
		}
	}
	RETURN_NULL();
}

/**
 * Builds a HTML A tag using framework conventions
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, linkTo){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL;
	zval *t0 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_INIT_NULL(v1);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "", 0);
	eval_int = phalcon_array_isset_long(v0, 0);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_long(r0, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
	} else {
		eval_int = phalcon_array_isset_string(v0, "action", strlen("action")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r1);
			phalcon_array_fetch_string(r1, v0, "action", strlen("action"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r1);
			v2 = r1;
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_unset_string(v0, "action", strlen("action")+1);
		}
	}
	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_STRING(v1, "", 0);
	eval_int = phalcon_array_isset_long(v0, 1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_long(r2, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v1) {
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r2);
		v1 = r2;
	} else {
		eval_int = phalcon_array_isset_string(v0, "text", strlen("text")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch_string(r3, v0, "text", strlen("text"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v1) {
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r3);
			v1 = r3;
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_unset_string(v0, "text", strlen("text")+1);
		}
	}
	PHALCON_ALLOC_ZVAL(v3);
	ZVAL_STRING(v3, "", 0);
	if (!phalcon_compare_strict_string(v2, "")) {
		eval_int = phalcon_array_isset_string(v0, "confirm", strlen("confirm")+1);
		if (eval_int) {
			eval_int = phalcon_array_isset_string(v0, "onclick", strlen("onclick")+1);
			if (!eval_int) {
				PHALCON_ALLOC_ZVAL(t0);
				ZVAL_STRING(t0, "", 1);
				{
					zval *orig_ptr = v0;
					if (Z_REFCOUNT_P(orig_ptr) > 1) {
						ALLOC_ZVAL(v0);
						*v0 = *orig_ptr;
						zval_copy_ctor(v0);
						Z_SET_REFCOUNT_P(v0, 1);
						Z_UNSET_ISREF_P(v0);
					}
				}
				phalcon_array_update_string(v0, "onclick", strlen("onclick"), t0 TSRMLS_CC);
			}
			PHALCON_ALLOC_ZVAL(r5);
			phalcon_array_fetch_string(r5, v0, "confirm", strlen("confirm"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r6);
			phalcon_concat_left(r6, "if(!confirm('", r5 TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r7);
			phalcon_array_fetch_string(r7, v0, "onclick", strlen("onclick"), PHALCON_NOISY_FETCH TSRMLS_CC);
			PHALCON_ALLOC_ZVAL(r4);
			phalcon_concat_vboth(r4, r6, "')) { return false; }; ", r7 TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "onclick", strlen("onclick"), r4 TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_unset_string(v0, "confirm", strlen("confirm")+1);
		}
		PHALCON_ALLOC_ZVAL(r8);
		Z_ADDREF_P(v2);
		p0[0] = v2;
		PHALCON_CALL_STATIC_PARAMS(r8, "phalcon_utils", "geturl", 1, p0);
		PHALCON_ALLOC_ZVAL(r9);
		phalcon_concat_both(r9,  "<a href=\"", r8, "\" " TSRMLS_CC);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r9);
		v3 = r9;
		FOREACH_KV(v0, ac0, fes42, fee42, ah0, hp0, v5, v4)
			if (Z_TYPE_P(v5) != IS_LONG) {
				if (!r11) {
					PHALCON_ALLOC_ZVAL(r11);
				} else {
					if (Z_REFCOUNT_P(r11) > 1) {
						{
							zval *orig_ptr = r11;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r11);
								*r11 = *orig_ptr;
								zval_copy_ctor(r11);
								Z_SET_REFCOUNT_P(r11, 1);
								Z_UNSET_ISREF_P(r11);
							}
						}
					} else {
						FREE_ZVAL(r11);
						PHALCON_ALLOC_ZVAL(r11);
					}
				}
				phalcon_concat_left(r11, " ", v5 TSRMLS_CC);
				if (!r10) {
					PHALCON_ALLOC_ZVAL(r10);
				} else {
					if (Z_REFCOUNT_P(r10) > 1) {
						{
							zval *orig_ptr = r10;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r10);
								*r10 = *orig_ptr;
								zval_copy_ctor(r10);
								Z_SET_REFCOUNT_P(r10, 1);
								Z_UNSET_ISREF_P(r10);
							}
						}
					} else {
						FREE_ZVAL(r10);
						PHALCON_ALLOC_ZVAL(r10);
					}
				}
				phalcon_concat_vboth(r10, r11, "=\"", v4 TSRMLS_CC);
				if (!r12) {
					PHALCON_ALLOC_ZVAL(r12);
				} else {
					if (Z_REFCOUNT_P(r12) > 1) {
						{
							zval *orig_ptr = r12;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r12);
								*r12 = *orig_ptr;
								zval_copy_ctor(r12);
								Z_SET_REFCOUNT_P(r12, 1);
								Z_UNSET_ISREF_P(r12);
							}
						}
					} else {
						FREE_ZVAL(r12);
						PHALCON_ALLOC_ZVAL(r12);
					}
				}
				phalcon_concat_right(r12, r10, "\" " TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r13, v3, r12);
				if (v3) {
					Z_DELREF_P(v3);
					if (!Z_REFCOUNT_P(v3)) {
						FREE_ZVAL(v3);
					}
				}
				Z_ADDREF_P(r13);
				v3 = r13;
			}
		END_FOREACH(ac0, fes42, fee42, ah0, hp0);
		PHALCON_ALLOC_ZVAL(r14);
		phalcon_concat_both(r14,  ">", v1, "</a>" TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r15, v3, r14);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r15);
		v3 = r15;
	}
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v3);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML input[type="text"] tag
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, textField){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_long(v0, 0);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, v0, "id", strlen("id"), PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_long(v0, 0, r0 TSRMLS_CC);
	}
	eval_int = phalcon_array_isset_string(v0, "name", strlen("name")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "name", strlen("name"), r1 TSRMLS_CC);
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_string(r2, v0, "name", strlen("name"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (!zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch_long(r3, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "name", strlen("name"), r3 TSRMLS_CC);
		}
	}
	eval_int = phalcon_array_isset_string(v0, "value", strlen("value")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch_string(r4, v0, "value", strlen("value"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r4);
		v1 = r4;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "value", strlen("value")+1);
	} else {
		PHALCON_ALLOC_ZVAL(r5);
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_array_fetch_long(r6, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r6);
		p0[0] = r6;
		PHALCON_CALL_SELF_PARAMS(r5, this_ptr, "_getvaluefromaction", 1, p0);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r5);
		v1 = r5;
	}
	PHALCON_ALLOC_ZVAL(r8);
	phalcon_array_fetch_long(r8, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r9);
	phalcon_concat_left(r9, "<input type=\"text\" id=\"", r8 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r7);
	phalcon_concat_vboth(r7, r9, "\" value=\"", v1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r10);
	phalcon_concat_right(r10, r7, "\" " TSRMLS_CC);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r10);
	v2 = r10;
	FOREACH_KV(v0, ac0, fes43, fee43, ah0, hp0, v3, v1)
		if (Z_TYPE_P(v3) != IS_LONG) {
			if (!r11) {
				PHALCON_ALLOC_ZVAL(r11);
			} else {
				if (Z_REFCOUNT_P(r11) > 1) {
					{
						zval *orig_ptr = r11;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r11);
							*r11 = *orig_ptr;
							zval_copy_ctor(r11);
							Z_SET_REFCOUNT_P(r11, 1);
							Z_UNSET_ISREF_P(r11);
						}
					}
				} else {
					FREE_ZVAL(r11);
					PHALCON_ALLOC_ZVAL(r11);
				}
			}
			phalcon_concat_vboth(r11, v3, "=\"", v1 TSRMLS_CC);
			if (!r12) {
				PHALCON_ALLOC_ZVAL(r12);
			} else {
				if (Z_REFCOUNT_P(r12) > 1) {
					{
						zval *orig_ptr = r12;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r12);
							*r12 = *orig_ptr;
							zval_copy_ctor(r12);
							Z_SET_REFCOUNT_P(r12, 1);
							Z_UNSET_ISREF_P(r12);
						}
					}
				} else {
					FREE_ZVAL(r12);
					PHALCON_ALLOC_ZVAL(r12);
				}
			}
			phalcon_concat_right(r12, r11, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r13, v2, r12);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r13);
			v2 = r13;
		}
	END_FOREACH(ac0, fes43, fee43, ah0, hp0);
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, " />", 1);
	PHALCON_CONCAT_FUNCTION(r14, v2, t0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r14);
	v2 = r14;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML input[type="submit"] tag
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, submitButton){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *ac0 = NULL;
	zval *t0 = NULL;
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_string(v0, "value", strlen("value")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, v0, "value", strlen("value"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r0);
		v1 = r0;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "value", strlen("value")+1);
	} else {
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r1);
			phalcon_array_fetch_long(r1, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r1);
			v1 = r1;
		} else {
			PHALCON_ALLOC_ZVAL(v1);
			ZVAL_STRING(v1, "", 0);
		}
	}
	PHALCON_ALLOC_ZVAL(r2);
	phalcon_concat_both(r2,  "<input type=\"submit\" value=\"", v1, "\" " TSRMLS_CC);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r2);
	v2 = r2;
	FOREACH_KV(v0, ac0, fes44, fee44, ah0, hp0, v3, v1)
		if (Z_TYPE_P(v3) != IS_LONG) {
			if (!r3) {
				PHALCON_ALLOC_ZVAL(r3);
			} else {
				if (Z_REFCOUNT_P(r3) > 1) {
					{
						zval *orig_ptr = r3;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r3);
							*r3 = *orig_ptr;
							zval_copy_ctor(r3);
							Z_SET_REFCOUNT_P(r3, 1);
							Z_UNSET_ISREF_P(r3);
						}
					}
				} else {
					FREE_ZVAL(r3);
					PHALCON_ALLOC_ZVAL(r3);
				}
			}
			phalcon_concat_vboth(r3, v3, "=\"", v1 TSRMLS_CC);
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_concat_right(r4, r3, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r5, v2, r4);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r5);
			v2 = r5;
		}
	END_FOREACH(ac0, fes44, fee44, ah0, hp0);
	PHALCON_ALLOC_ZVAL(t0);
	ZVAL_STRING(t0, " />", 1);
	PHALCON_CONCAT_FUNCTION(r6, v2, t0);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r6);
	v2 = r6;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML SELECT tag using an array for options
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, selectStatic){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL;
	zval *i0 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "", 0);
	eval_int = phalcon_array_isset_string(v0, "value", strlen("value")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r1);
		p0[0] = r1;
		PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_getvaluefromaction", 1, p0);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "value", strlen("value")+1);
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_string(r2, v0, "value", strlen("value"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r2);
		v2 = r2;
	}
	PHALCON_ALLOC_ZVAL(r4);
	phalcon_array_fetch_long(r4, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r5);
	phalcon_concat_left(r5, "<select id=\"", r4 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r6);
	phalcon_array_fetch_long(r6, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r3);
	phalcon_concat_vboth(r3, r5, "\" name=\"", r6 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r7);
	phalcon_concat_right(r7, r3, "\" " TSRMLS_CC);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r7);
	v3 = r7;
	eval_int = phalcon_array_isset_string(v0, "dummyValue", strlen("dummyValue")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(v4);
		ZVAL_STRING(v4, "@", 0);
	} else {
		PHALCON_ALLOC_ZVAL(r8);
		phalcon_array_fetch_string(r8, v0, "dummyValue", strlen("dummyValue"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v4) {
			Z_DELREF_P(v4);
			if (!Z_REFCOUNT_P(v4)) {
				FREE_ZVAL(v4);
			}
		}
		Z_ADDREF_P(r8);
		v4 = r8;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "dummyValue", strlen("dummyValue")+1);
	}
	eval_int = phalcon_array_isset_string(v0, "dummyText", strlen("dummyText")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(v5);
		ZVAL_STRING(v5, "Choose...", 0);
	} else {
		PHALCON_ALLOC_ZVAL(r9);
		phalcon_array_fetch_string(r9, v0, "dummyText", strlen("dummyText"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r9);
		v5 = r9;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "dummyText", strlen("dummyText")+1);
	}
	FOREACH_KV(v0, ac0, fes45, fee45, ah0, hp0, v7, v6)
		if (Z_TYPE_P(v7) != IS_LONG) {
			if (Z_TYPE_P(v6) != IS_ARRAY) { 
				if (!r10) {
					PHALCON_ALLOC_ZVAL(r10);
				} else {
					if (Z_REFCOUNT_P(r10) > 1) {
						{
							zval *orig_ptr = r10;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r10);
								*r10 = *orig_ptr;
								zval_copy_ctor(r10);
								Z_SET_REFCOUNT_P(r10, 1);
								Z_UNSET_ISREF_P(r10);
							}
						}
					} else {
						FREE_ZVAL(r10);
						PHALCON_ALLOC_ZVAL(r10);
					}
				}
				phalcon_concat_vboth(r10, v7, "=\"", v6 TSRMLS_CC);
				if (!r11) {
					PHALCON_ALLOC_ZVAL(r11);
				} else {
					if (Z_REFCOUNT_P(r11) > 1) {
						{
							zval *orig_ptr = r11;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r11);
								*r11 = *orig_ptr;
								zval_copy_ctor(r11);
								Z_SET_REFCOUNT_P(r11, 1);
								Z_UNSET_ISREF_P(r11);
							}
						}
					} else {
						FREE_ZVAL(r11);
						PHALCON_ALLOC_ZVAL(r11);
					}
				}
				phalcon_concat_right(r11, r10, "\" " TSRMLS_CC);
				PHALCON_CONCAT_FUNCTION(r12, v3, r11);
				if (v3) {
					Z_DELREF_P(v3);
					if (!Z_REFCOUNT_P(v3)) {
						FREE_ZVAL(v3);
					}
				}
				Z_ADDREF_P(r12);
				v3 = r12;
			}
		}
	END_FOREACH(ac0, fes45, fee45, ah0, hp0);
	PHALCON_GET_CONSTANT(t0, "PHP_EOL");
	PHALCON_ALLOC_ZVAL(r13);
	phalcon_concat_left(r13, ">", t0 TSRMLS_CC);
	PHALCON_CONCAT_FUNCTION(r14, v3, r13);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r14);
	v3 = r14;
	eval_int = phalcon_array_isset_string(v0, "useDummy", strlen("useDummy")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_STRING(t1, "\t", 1);
		PHALCON_ALLOC_ZVAL(r17);
		phalcon_concat_vboth(r17, t1, "<option value=\"", v4 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r16);
		phalcon_concat_vboth(r16, r17, "\">", v5 TSRMLS_CC);
		PHALCON_GET_CONSTANT(t2, "PHP_EOL");
		PHALCON_ALLOC_ZVAL(r15);
		phalcon_concat_vboth(r15, r16, "</option>", t2 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r18, v3, r15);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r18);
		v3 = r18;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "useDummy", strlen("useDummy")+1);
	}
	eval_int = phalcon_array_isset_long(v0, 1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r19);
		phalcon_array_fetch_long(r19, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (Z_TYPE_P(r19) == IS_ARRAY) { 
			PHALCON_ALLOC_ZVAL(r20);
			phalcon_array_fetch_long(r20, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
			FOREACH_KV(r20, ac1, fes46, fee46, ah1, hp1, v9, v8)
				PHALCON_EQUAL_FUNCTION(r21, v9, v2);
				if (!t3) {
					PHALCON_ALLOC_ZVAL(t3);
				} else {
					if (Z_REFCOUNT_P(t3) > 1) {
						SEPARATE_ZVAL(&t3);
					} else {
						if (Z_TYPE_P(t3) != IS_STRING) {
							FREE_ZVAL(t3);
							PHALCON_ALLOC_ZVAL(t3);
						}
					}
				}
				ZVAL_STRING(t3, "", 1);
				PHALCON_NOT_IDENTICAL_FUNCTION(r22, v2, t3);
				PHALCON_AND_FUNCTION(r23, r21, r22);
				if (zend_is_true(r23)) {
					if (!t4) {
						PHALCON_ALLOC_ZVAL(t4);
					} else {
						if (Z_REFCOUNT_P(t4) > 1) {
							SEPARATE_ZVAL(&t4);
						} else {
							if (Z_TYPE_P(t4) != IS_STRING) {
								FREE_ZVAL(t4);
								PHALCON_ALLOC_ZVAL(t4);
							}
						}
					}
					ZVAL_STRING(t4, "\t", 1);
					if (!r26) {
						PHALCON_ALLOC_ZVAL(r26);
					} else {
						if (Z_REFCOUNT_P(r26) > 1) {
							{
								zval *orig_ptr = r26;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r26);
									*r26 = *orig_ptr;
									zval_copy_ctor(r26);
									Z_SET_REFCOUNT_P(r26, 1);
									Z_UNSET_ISREF_P(r26);
								}
							}
						} else {
							FREE_ZVAL(r26);
							PHALCON_ALLOC_ZVAL(r26);
						}
					}
					phalcon_concat_vboth(r26, t4, "<option value=\"", v9 TSRMLS_CC);
					if (!r25) {
						PHALCON_ALLOC_ZVAL(r25);
					} else {
						if (Z_REFCOUNT_P(r25) > 1) {
							{
								zval *orig_ptr = r25;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r25);
									*r25 = *orig_ptr;
									zval_copy_ctor(r25);
									Z_SET_REFCOUNT_P(r25, 1);
									Z_UNSET_ISREF_P(r25);
								}
							}
						} else {
							FREE_ZVAL(r25);
							PHALCON_ALLOC_ZVAL(r25);
						}
					}
					phalcon_concat_vboth(r25, r26, "\" selected=\"selected\">", v8 TSRMLS_CC);
					PHALCON_GET_CONSTANT(t5, "PHP_EOL");
					if (!r24) {
						PHALCON_ALLOC_ZVAL(r24);
					} else {
						if (Z_REFCOUNT_P(r24) > 1) {
							{
								zval *orig_ptr = r24;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r24);
									*r24 = *orig_ptr;
									zval_copy_ctor(r24);
									Z_SET_REFCOUNT_P(r24, 1);
									Z_UNSET_ISREF_P(r24);
								}
							}
						} else {
							FREE_ZVAL(r24);
							PHALCON_ALLOC_ZVAL(r24);
						}
					}
					phalcon_concat_vboth(r24, r25, "</option>", t5 TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r27, v3, r24);
					if (v3) {
						Z_DELREF_P(v3);
						if (!Z_REFCOUNT_P(v3)) {
							FREE_ZVAL(v3);
						}
					}
					Z_ADDREF_P(r27);
					v3 = r27;
				} else {
					if (!t6) {
						PHALCON_ALLOC_ZVAL(t6);
					} else {
						if (Z_REFCOUNT_P(t6) > 1) {
							SEPARATE_ZVAL(&t6);
						} else {
							if (Z_TYPE_P(t6) != IS_STRING) {
								FREE_ZVAL(t6);
								PHALCON_ALLOC_ZVAL(t6);
							}
						}
					}
					ZVAL_STRING(t6, "\t", 1);
					if (!r30) {
						PHALCON_ALLOC_ZVAL(r30);
					} else {
						if (Z_REFCOUNT_P(r30) > 1) {
							{
								zval *orig_ptr = r30;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r30);
									*r30 = *orig_ptr;
									zval_copy_ctor(r30);
									Z_SET_REFCOUNT_P(r30, 1);
									Z_UNSET_ISREF_P(r30);
								}
							}
						} else {
							FREE_ZVAL(r30);
							PHALCON_ALLOC_ZVAL(r30);
						}
					}
					phalcon_concat_vboth(r30, t6, "<option value=\"", v9 TSRMLS_CC);
					if (!r29) {
						PHALCON_ALLOC_ZVAL(r29);
					} else {
						if (Z_REFCOUNT_P(r29) > 1) {
							{
								zval *orig_ptr = r29;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r29);
									*r29 = *orig_ptr;
									zval_copy_ctor(r29);
									Z_SET_REFCOUNT_P(r29, 1);
									Z_UNSET_ISREF_P(r29);
								}
							}
						} else {
							FREE_ZVAL(r29);
							PHALCON_ALLOC_ZVAL(r29);
						}
					}
					phalcon_concat_vboth(r29, r30, "\">", v8 TSRMLS_CC);
					PHALCON_GET_CONSTANT(t7, "PHP_EOL");
					if (!r28) {
						PHALCON_ALLOC_ZVAL(r28);
					} else {
						if (Z_REFCOUNT_P(r28) > 1) {
							{
								zval *orig_ptr = r28;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r28);
									*r28 = *orig_ptr;
									zval_copy_ctor(r28);
									Z_SET_REFCOUNT_P(r28, 1);
									Z_UNSET_ISREF_P(r28);
								}
							}
						} else {
							FREE_ZVAL(r28);
							PHALCON_ALLOC_ZVAL(r28);
						}
					}
					phalcon_concat_vboth(r28, r29, "</option>", t7 TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r31, v3, r28);
					if (v3) {
						Z_DELREF_P(v3);
						if (!Z_REFCOUNT_P(v3)) {
							FREE_ZVAL(v3);
						}
					}
					Z_ADDREF_P(r31);
					v3 = r31;
				}
			END_FOREACH(ac1, fes46, fee46, ah1, hp1);
		} else {
			PHALCON_ALLOC_ZVAL(i0);
			object_init_ex(i0, phalcon_tag_exception_class_entry);
			PHALCON_ALLOC_ZVAL(p1[0]);
			ZVAL_STRING(p1[0], "Data supplied for Phalcon_Tag::selectStatic is not valid", 1);
			PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
			zend_throw_exception_object(i0 TSRMLS_CC);
			Z_ADDREF_P(i0);
			return;
		}
	}
	PHALCON_ALLOC_ZVAL(t8);
	ZVAL_STRING(t8, "</select>", 1);
	PHALCON_CONCAT_FUNCTION(r32, v3, t8);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r32);
	v3 = r32;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v3);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML SELECT tag using a resultset as options
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, select){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *v7 = NULL, *v8 = NULL, *v9 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL, *r25 = NULL, *r26 = NULL, *r27 = NULL;
	zval *r28 = NULL, *r29 = NULL, *r30 = NULL, *r31 = NULL, *r32 = NULL, *r33 = NULL, *r34 = NULL;
	zval *r35 = NULL, *r36 = NULL, *r37 = NULL, *r38 = NULL, *r39 = NULL, *r40 = NULL, *r41 = NULL;
	zval *r42 = NULL, *r43 = NULL, *r44 = NULL, *r45 = NULL, *r46 = NULL, *r47 = NULL, *r48 = NULL;
	zval *r49 = NULL, *r50 = NULL, *r51 = NULL, *r52 = NULL, *r53 = NULL;
	zval *ac0 = NULL, *ac1 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL, *t3 = NULL, *t4 = NULL, *t5 = NULL, *t6 = NULL;
	zval *t7 = NULL, *t8 = NULL, *t9 = NULL;
	zval *i0 = NULL, *i1 = NULL, *i2 = NULL;
	zval *p0[] = { NULL }, *p1[] = { NULL }, *p2[] = { NULL }, *p6[] = { NULL }, *p7[] = { NULL }, *p8[] = { NULL }, *p9[] = { NULL }, *p10[] = { NULL }, *p12[] = { NULL };
	HashTable *ah0, *ah1;
	HashPosition hp0, hp1;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v1) {
		PHALCON_ALLOC_ZVAL(v1);
		ZVAL_STRING(v1, "", 0);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_string(v0, "value", strlen("value")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r1);
		p0[0] = r1;
		PHALCON_CALL_SELF_PARAMS(r0, this_ptr, "_getvaluefromaction", 1, p0);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r0);
		v2 = r0;
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_string(r2, v0, "value", strlen("value"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v2) {
			Z_DELREF_P(v2);
			if (!Z_REFCOUNT_P(v2)) {
				FREE_ZVAL(v2);
			}
		}
		Z_ADDREF_P(r2);
		v2 = r2;
	}
	PHALCON_ALLOC_ZVAL(r4);
	phalcon_array_fetch_long(r4, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r5);
	phalcon_concat_left(r5, "<select id='", r4 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r6);
	phalcon_array_fetch_long(r6, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r3);
	phalcon_concat_vboth(r3, r5, "' name='", r6 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r7);
	phalcon_concat_right(r7, r3, "' " TSRMLS_CC);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r7);
	v3 = r7;
	if (Z_TYPE_P(v0) == IS_ARRAY) { 
		FOREACH_KV(v0, ac0, fes47, fee47, ah0, hp0, v4, v2)
			if (Z_TYPE_P(v4) != IS_LONG) {
				if (Z_TYPE_P(v2) != IS_ARRAY) { 
					if (!r8) {
						PHALCON_ALLOC_ZVAL(r8);
					} else {
						if (Z_REFCOUNT_P(r8) > 1) {
							{
								zval *orig_ptr = r8;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r8);
									*r8 = *orig_ptr;
									zval_copy_ctor(r8);
									Z_SET_REFCOUNT_P(r8, 1);
									Z_UNSET_ISREF_P(r8);
								}
							}
						} else {
							FREE_ZVAL(r8);
							PHALCON_ALLOC_ZVAL(r8);
						}
					}
					phalcon_concat_vboth(r8, v4, "='", v2 TSRMLS_CC);
					if (!r9) {
						PHALCON_ALLOC_ZVAL(r9);
					} else {
						if (Z_REFCOUNT_P(r9) > 1) {
							{
								zval *orig_ptr = r9;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r9);
									*r9 = *orig_ptr;
									zval_copy_ctor(r9);
									Z_SET_REFCOUNT_P(r9, 1);
									Z_UNSET_ISREF_P(r9);
								}
							}
						} else {
							FREE_ZVAL(r9);
							PHALCON_ALLOC_ZVAL(r9);
						}
					}
					phalcon_concat_right(r9, r8, "' " TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r10, v3, r9);
					if (v3) {
						Z_DELREF_P(v3);
						if (!Z_REFCOUNT_P(v3)) {
							FREE_ZVAL(v3);
						}
					}
					Z_ADDREF_P(r10);
					v3 = r10;
				}
			}
		END_FOREACH(ac0, fes47, fee47, ah0, hp0);
	}
	PHALCON_GET_CONSTANT(t0, "PHP_EOL");
	PHALCON_ALLOC_ZVAL(r11);
	phalcon_concat_left(r11, ">", t0 TSRMLS_CC);
	PHALCON_CONCAT_FUNCTION(r12, v3, r11);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r12);
	v3 = r12;
	eval_int = phalcon_array_isset_string(v0, "dummyValue", strlen("dummyValue")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(v5);
		ZVAL_STRING(v5, "", 0);
	} else {
		PHALCON_ALLOC_ZVAL(r13);
		phalcon_array_fetch_string(r13, v0, "dummyValue", strlen("dummyValue"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v5) {
			Z_DELREF_P(v5);
			if (!Z_REFCOUNT_P(v5)) {
				FREE_ZVAL(v5);
			}
		}
		Z_ADDREF_P(r13);
		v5 = r13;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "dummyValue", strlen("dummyValue")+1);
	}
	eval_int = phalcon_array_isset_string(v0, "dummyText", strlen("dummyText")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(v6);
		ZVAL_STRING(v6, "Choose...", 0);
	} else {
		PHALCON_ALLOC_ZVAL(r14);
		phalcon_array_fetch_string(r14, v0, "dummyText", strlen("dummyText"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v6) {
			Z_DELREF_P(v6);
			if (!Z_REFCOUNT_P(v6)) {
				FREE_ZVAL(v6);
			}
		}
		Z_ADDREF_P(r14);
		v6 = r14;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "dummyText", strlen("dummyText")+1);
	}
	eval_int = phalcon_array_isset_string(v0, "useDummy", strlen("useDummy")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_STRING(t1, "\t", 1);
		PHALCON_ALLOC_ZVAL(r17);
		phalcon_concat_vboth(r17, t1, "<option value=\"", v5 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r16);
		phalcon_concat_vboth(r16, r17, "\">", v6 TSRMLS_CC);
		PHALCON_GET_CONSTANT(t2, "PHP_EOL");
		PHALCON_ALLOC_ZVAL(r15);
		phalcon_concat_vboth(r15, r16, "</option>", t2 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r18, v3, r15);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r18);
		v3 = r18;
	}
	eval_int = phalcon_array_isset_long(v0, 1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r19);
		phalcon_array_fetch_long(r19, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (zend_is_true(r19)) {
			PHALCON_ALLOC_ZVAL(r20);
			phalcon_array_fetch_long(r20, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
			if (Z_TYPE_P(r20) == IS_OBJECT) {
				eval_int = phalcon_array_isset_string(v0, "using", strlen("using")+1);
				if (!eval_int) {
					PHALCON_ALLOC_ZVAL(i0);
					object_init_ex(i0, phalcon_tag_exception_class_entry);
					PHALCON_ALLOC_ZVAL(p1[0]);
					ZVAL_STRING(p1[0], "The 'using' parameter is required", 1);
					PHALCON_CALL_METHOD_PARAMS_NORETURN(i0, "__construct", 1, p1, PHALCON_CALL_CHECK);
					zend_throw_exception_object(i0 TSRMLS_CC);
					Z_ADDREF_P(i0);
					return;
				} else {
					PHALCON_ALLOC_ZVAL(r21);
					phalcon_array_fetch_string(r21, v0, "using", strlen("using"), PHALCON_NOISY_FETCH TSRMLS_CC);
					if (Z_TYPE_P(r21) != IS_ARRAY) { 
						PHALCON_ALLOC_ZVAL(i1);
						object_init_ex(i1, phalcon_tag_exception_class_entry);
						PHALCON_ALLOC_ZVAL(p2[0]);
						ZVAL_STRING(p2[0], "The 'using' parameter should be an Array", 1);
						PHALCON_CALL_METHOD_PARAMS_NORETURN(i1, "__construct", 1, p2, PHALCON_CALL_CHECK);
						zend_throw_exception_object(i1 TSRMLS_CC);
						Z_ADDREF_P(i1);
						return;
					}
				}
				PHALCON_ALLOC_ZVAL(r22);
				phalcon_array_fetch_string(r22, v0, "using", strlen("using"), PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v7) {
					Z_DELREF_P(v7);
					if (!Z_REFCOUNT_P(v7)) {
						FREE_ZVAL(v7);
					}
				}
				Z_ADDREF_P(r22);
				v7 = r22;
				PHALCON_ALLOC_ZVAL(r23);
				phalcon_array_fetch_long(r23, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
				if (v8) {
					Z_DELREF_P(v8);
					if (!Z_REFCOUNT_P(v8)) {
						FREE_ZVAL(v8);
					}
				}
				Z_ADDREF_P(r23);
				v8 = r23;
				PHALCON_CALL_METHOD_NORETURN(v8, "rewind", PHALCON_CALL_DEFAULT);
				ws48:
				PHALCON_ALLOC_ZVAL(r24);
				PHALCON_CALL_METHOD(r24, v8, "valid", PHALCON_CALL_DEFAULT);
				if (!zend_is_true(r24)) {
					goto we48;
				}
				if (!r25) {
					PHALCON_ALLOC_ZVAL(r25);
				} else {
					if (Z_REFCOUNT_P(r25) > 1) {
						{
							zval *orig_ptr = r25;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r25);
								*r25 = *orig_ptr;
								zval_copy_ctor(r25);
								Z_SET_REFCOUNT_P(r25, 1);
								Z_UNSET_ISREF_P(r25);
							}
						}
					} else {
						FREE_ZVAL(r25);
						PHALCON_ALLOC_ZVAL(r25);
					}
				}
				PHALCON_CALL_METHOD(r25, v8, "current", PHALCON_CALL_DEFAULT);
				if (v9) {
					Z_DELREF_P(v9);
					if (!Z_REFCOUNT_P(v9)) {
						FREE_ZVAL(v9);
					}
				}
				Z_ADDREF_P(r25);
				v9 = r25;
				if (!r26) {
					PHALCON_ALLOC_ZVAL(r26);
				} else {
					if (Z_REFCOUNT_P(r26) > 1) {
						{
							zval *orig_ptr = r26;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r26);
								*r26 = *orig_ptr;
								zval_copy_ctor(r26);
								Z_SET_REFCOUNT_P(r26, 1);
								Z_UNSET_ISREF_P(r26);
							}
						}
					} else {
						FREE_ZVAL(r26);
						PHALCON_ALLOC_ZVAL(r26);
					}
				}
				if (!r27) {
					PHALCON_ALLOC_ZVAL(r27);
				} else {
					if (Z_REFCOUNT_P(r27) > 1) {
						{
							zval *orig_ptr = r27;
							if (Z_REFCOUNT_P(orig_ptr) > 1) {
								Z_DELREF_P(orig_ptr);
								ALLOC_ZVAL(r27);
								*r27 = *orig_ptr;
								zval_copy_ctor(r27);
								Z_SET_REFCOUNT_P(r27, 1);
								Z_UNSET_ISREF_P(r27);
							}
						}
					} else {
						FREE_ZVAL(r27);
						PHALCON_ALLOC_ZVAL(r27);
					}
				}
				phalcon_array_fetch_long(r27, v7, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
				Z_ADDREF_P(r27);
				p6[0] = r27;
				PHALCON_CALL_METHOD_PARAMS(r26, v9, "readattribute", 1, p6, PHALCON_CALL_DEFAULT);
				PHALCON_EQUAL_FUNCTION(r28, v2, r26);
				if (zend_is_true(r28)) {
					if (!t3) {
						PHALCON_ALLOC_ZVAL(t3);
					} else {
						if (Z_REFCOUNT_P(t3) > 1) {
							SEPARATE_ZVAL(&t3);
						} else {
							if (Z_TYPE_P(t3) != IS_STRING) {
								FREE_ZVAL(t3);
								PHALCON_ALLOC_ZVAL(t3);
							}
						}
					}
					ZVAL_STRING(t3, "\t", 1);
					if (!r32) {
						PHALCON_ALLOC_ZVAL(r32);
					} else {
						if (Z_REFCOUNT_P(r32) > 1) {
							{
								zval *orig_ptr = r32;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r32);
									*r32 = *orig_ptr;
									zval_copy_ctor(r32);
									Z_SET_REFCOUNT_P(r32, 1);
									Z_UNSET_ISREF_P(r32);
								}
							}
						} else {
							FREE_ZVAL(r32);
							PHALCON_ALLOC_ZVAL(r32);
						}
					}
					if (!r33) {
						PHALCON_ALLOC_ZVAL(r33);
					} else {
						if (Z_REFCOUNT_P(r33) > 1) {
							{
								zval *orig_ptr = r33;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r33);
									*r33 = *orig_ptr;
									zval_copy_ctor(r33);
									Z_SET_REFCOUNT_P(r33, 1);
									Z_UNSET_ISREF_P(r33);
								}
							}
						} else {
							FREE_ZVAL(r33);
							PHALCON_ALLOC_ZVAL(r33);
						}
					}
					phalcon_array_fetch_long(r33, v7, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r33);
					p7[0] = r33;
					PHALCON_CALL_METHOD_PARAMS(r32, v9, "readattribute", 1, p7, PHALCON_CALL_DEFAULT);
					if (!r31) {
						PHALCON_ALLOC_ZVAL(r31);
					} else {
						if (Z_REFCOUNT_P(r31) > 1) {
							{
								zval *orig_ptr = r31;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r31);
									*r31 = *orig_ptr;
									zval_copy_ctor(r31);
									Z_SET_REFCOUNT_P(r31, 1);
									Z_UNSET_ISREF_P(r31);
								}
							}
						} else {
							FREE_ZVAL(r31);
							PHALCON_ALLOC_ZVAL(r31);
						}
					}
					phalcon_concat_vboth(r31, t3, "<option selected=\"selected\" value=\"", r32 TSRMLS_CC);
					if (!r34) {
						PHALCON_ALLOC_ZVAL(r34);
					} else {
						if (Z_REFCOUNT_P(r34) > 1) {
							{
								zval *orig_ptr = r34;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r34);
									*r34 = *orig_ptr;
									zval_copy_ctor(r34);
									Z_SET_REFCOUNT_P(r34, 1);
									Z_UNSET_ISREF_P(r34);
								}
							}
						} else {
							FREE_ZVAL(r34);
							PHALCON_ALLOC_ZVAL(r34);
						}
					}
					if (!r35) {
						PHALCON_ALLOC_ZVAL(r35);
					} else {
						if (Z_REFCOUNT_P(r35) > 1) {
							{
								zval *orig_ptr = r35;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r35);
									*r35 = *orig_ptr;
									zval_copy_ctor(r35);
									Z_SET_REFCOUNT_P(r35, 1);
									Z_UNSET_ISREF_P(r35);
								}
							}
						} else {
							FREE_ZVAL(r35);
							PHALCON_ALLOC_ZVAL(r35);
						}
					}
					phalcon_array_fetch_long(r35, v7, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r35);
					p8[0] = r35;
					PHALCON_CALL_METHOD_PARAMS(r34, v9, "readattribute", 1, p8, PHALCON_CALL_DEFAULT);
					if (!r30) {
						PHALCON_ALLOC_ZVAL(r30);
					} else {
						if (Z_REFCOUNT_P(r30) > 1) {
							{
								zval *orig_ptr = r30;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r30);
									*r30 = *orig_ptr;
									zval_copy_ctor(r30);
									Z_SET_REFCOUNT_P(r30, 1);
									Z_UNSET_ISREF_P(r30);
								}
							}
						} else {
							FREE_ZVAL(r30);
							PHALCON_ALLOC_ZVAL(r30);
						}
					}
					phalcon_concat_vboth(r30, r31, "'>", r34 TSRMLS_CC);
					PHALCON_GET_CONSTANT(t4, "PHP_EOL");
					if (!r29) {
						PHALCON_ALLOC_ZVAL(r29);
					} else {
						if (Z_REFCOUNT_P(r29) > 1) {
							{
								zval *orig_ptr = r29;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r29);
									*r29 = *orig_ptr;
									zval_copy_ctor(r29);
									Z_SET_REFCOUNT_P(r29, 1);
									Z_UNSET_ISREF_P(r29);
								}
							}
						} else {
							FREE_ZVAL(r29);
							PHALCON_ALLOC_ZVAL(r29);
						}
					}
					phalcon_concat_vboth(r29, r30, "</option>", t4 TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r36, v3, r29);
					if (v3) {
						Z_DELREF_P(v3);
						if (!Z_REFCOUNT_P(v3)) {
							FREE_ZVAL(v3);
						}
					}
					Z_ADDREF_P(r36);
					v3 = r36;
				} else {
					if (!t5) {
						PHALCON_ALLOC_ZVAL(t5);
					} else {
						if (Z_REFCOUNT_P(t5) > 1) {
							SEPARATE_ZVAL(&t5);
						} else {
							if (Z_TYPE_P(t5) != IS_STRING) {
								FREE_ZVAL(t5);
								PHALCON_ALLOC_ZVAL(t5);
							}
						}
					}
					ZVAL_STRING(t5, "\t", 1);
					if (!r40) {
						PHALCON_ALLOC_ZVAL(r40);
					} else {
						if (Z_REFCOUNT_P(r40) > 1) {
							{
								zval *orig_ptr = r40;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r40);
									*r40 = *orig_ptr;
									zval_copy_ctor(r40);
									Z_SET_REFCOUNT_P(r40, 1);
									Z_UNSET_ISREF_P(r40);
								}
							}
						} else {
							FREE_ZVAL(r40);
							PHALCON_ALLOC_ZVAL(r40);
						}
					}
					if (!r41) {
						PHALCON_ALLOC_ZVAL(r41);
					} else {
						if (Z_REFCOUNT_P(r41) > 1) {
							{
								zval *orig_ptr = r41;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r41);
									*r41 = *orig_ptr;
									zval_copy_ctor(r41);
									Z_SET_REFCOUNT_P(r41, 1);
									Z_UNSET_ISREF_P(r41);
								}
							}
						} else {
							FREE_ZVAL(r41);
							PHALCON_ALLOC_ZVAL(r41);
						}
					}
					phalcon_array_fetch_long(r41, v7, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r41);
					p9[0] = r41;
					PHALCON_CALL_METHOD_PARAMS(r40, v9, "readattribute", 1, p9, PHALCON_CALL_DEFAULT);
					if (!r39) {
						PHALCON_ALLOC_ZVAL(r39);
					} else {
						if (Z_REFCOUNT_P(r39) > 1) {
							{
								zval *orig_ptr = r39;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r39);
									*r39 = *orig_ptr;
									zval_copy_ctor(r39);
									Z_SET_REFCOUNT_P(r39, 1);
									Z_UNSET_ISREF_P(r39);
								}
							}
						} else {
							FREE_ZVAL(r39);
							PHALCON_ALLOC_ZVAL(r39);
						}
					}
					phalcon_concat_vboth(r39, t5, "<option value=\"", r40 TSRMLS_CC);
					if (!r42) {
						PHALCON_ALLOC_ZVAL(r42);
					} else {
						if (Z_REFCOUNT_P(r42) > 1) {
							{
								zval *orig_ptr = r42;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r42);
									*r42 = *orig_ptr;
									zval_copy_ctor(r42);
									Z_SET_REFCOUNT_P(r42, 1);
									Z_UNSET_ISREF_P(r42);
								}
							}
						} else {
							FREE_ZVAL(r42);
							PHALCON_ALLOC_ZVAL(r42);
						}
					}
					if (!r43) {
						PHALCON_ALLOC_ZVAL(r43);
					} else {
						if (Z_REFCOUNT_P(r43) > 1) {
							{
								zval *orig_ptr = r43;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r43);
									*r43 = *orig_ptr;
									zval_copy_ctor(r43);
									Z_SET_REFCOUNT_P(r43, 1);
									Z_UNSET_ISREF_P(r43);
								}
							}
						} else {
							FREE_ZVAL(r43);
							PHALCON_ALLOC_ZVAL(r43);
						}
					}
					phalcon_array_fetch_long(r43, v7, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
					Z_ADDREF_P(r43);
					p10[0] = r43;
					PHALCON_CALL_METHOD_PARAMS(r42, v9, "readattribute", 1, p10, PHALCON_CALL_DEFAULT);
					if (!r38) {
						PHALCON_ALLOC_ZVAL(r38);
					} else {
						if (Z_REFCOUNT_P(r38) > 1) {
							{
								zval *orig_ptr = r38;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r38);
									*r38 = *orig_ptr;
									zval_copy_ctor(r38);
									Z_SET_REFCOUNT_P(r38, 1);
									Z_UNSET_ISREF_P(r38);
								}
							}
						} else {
							FREE_ZVAL(r38);
							PHALCON_ALLOC_ZVAL(r38);
						}
					}
					phalcon_concat_vboth(r38, r39, "\">", r42 TSRMLS_CC);
					PHALCON_GET_CONSTANT(t6, "PHP_EOL");
					if (!r37) {
						PHALCON_ALLOC_ZVAL(r37);
					} else {
						if (Z_REFCOUNT_P(r37) > 1) {
							{
								zval *orig_ptr = r37;
								if (Z_REFCOUNT_P(orig_ptr) > 1) {
									Z_DELREF_P(orig_ptr);
									ALLOC_ZVAL(r37);
									*r37 = *orig_ptr;
									zval_copy_ctor(r37);
									Z_SET_REFCOUNT_P(r37, 1);
									Z_UNSET_ISREF_P(r37);
								}
							}
						} else {
							FREE_ZVAL(r37);
							PHALCON_ALLOC_ZVAL(r37);
						}
					}
					phalcon_concat_vboth(r37, r38, "</option>", t6 TSRMLS_CC);
					PHALCON_CONCAT_FUNCTION(r44, v3, r37);
					if (v3) {
						Z_DELREF_P(v3);
						if (!Z_REFCOUNT_P(v3)) {
							FREE_ZVAL(v3);
						}
					}
					Z_ADDREF_P(r44);
					v3 = r44;
				}
				PHALCON_CALL_METHOD_NORETURN(v8, "next", PHALCON_CALL_DEFAULT);
				goto ws48;
				we48:
				r24 = NULL;
			} else {
				PHALCON_ALLOC_ZVAL(r45);
				phalcon_array_fetch_long(r45, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
				if (Z_TYPE_P(r45) == IS_ARRAY) { 
					PHALCON_ALLOC_ZVAL(r46);
					phalcon_array_fetch_long(r46, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
					FOREACH_V(r46, ac1, fes49, fee49, ah1, hp1, v9)
						if (!t7) {
							PHALCON_ALLOC_ZVAL(t7);
						} else {
							if (Z_REFCOUNT_P(t7) > 1) {
								SEPARATE_ZVAL(&t7);
							} else {
								if (Z_TYPE_P(t7) != IS_STRING) {
									FREE_ZVAL(t7);
									PHALCON_ALLOC_ZVAL(t7);
								}
							}
						}
						ZVAL_STRING(t7, "\t", 1);
						if (!r50) {
							PHALCON_ALLOC_ZVAL(r50);
						} else {
							if (Z_REFCOUNT_P(r50) > 1) {
								{
									zval *orig_ptr = r50;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r50);
										*r50 = *orig_ptr;
										zval_copy_ctor(r50);
										Z_SET_REFCOUNT_P(r50, 1);
										Z_UNSET_ISREF_P(r50);
									}
								}
							} else {
								FREE_ZVAL(r50);
								PHALCON_ALLOC_ZVAL(r50);
							}
						}
						phalcon_array_fetch_long(r50, v9, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
						if (!r49) {
							PHALCON_ALLOC_ZVAL(r49);
						} else {
							if (Z_REFCOUNT_P(r49) > 1) {
								{
									zval *orig_ptr = r49;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r49);
										*r49 = *orig_ptr;
										zval_copy_ctor(r49);
										Z_SET_REFCOUNT_P(r49, 1);
										Z_UNSET_ISREF_P(r49);
									}
								}
							} else {
								FREE_ZVAL(r49);
								PHALCON_ALLOC_ZVAL(r49);
							}
						}
						phalcon_concat_vboth(r49, t7, "<option value=\"", r50 TSRMLS_CC);
						if (!r51) {
							PHALCON_ALLOC_ZVAL(r51);
						} else {
							if (Z_REFCOUNT_P(r51) > 1) {
								{
									zval *orig_ptr = r51;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r51);
										*r51 = *orig_ptr;
										zval_copy_ctor(r51);
										Z_SET_REFCOUNT_P(r51, 1);
										Z_UNSET_ISREF_P(r51);
									}
								}
							} else {
								FREE_ZVAL(r51);
								PHALCON_ALLOC_ZVAL(r51);
							}
						}
						phalcon_array_fetch_long(r51, v9, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
						if (!r48) {
							PHALCON_ALLOC_ZVAL(r48);
						} else {
							if (Z_REFCOUNT_P(r48) > 1) {
								{
									zval *orig_ptr = r48;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r48);
										*r48 = *orig_ptr;
										zval_copy_ctor(r48);
										Z_SET_REFCOUNT_P(r48, 1);
										Z_UNSET_ISREF_P(r48);
									}
								}
							} else {
								FREE_ZVAL(r48);
								PHALCON_ALLOC_ZVAL(r48);
							}
						}
						phalcon_concat_vboth(r48, r49, "\">", r51 TSRMLS_CC);
						PHALCON_GET_CONSTANT(t8, "PHP_EOL");
						if (!r47) {
							PHALCON_ALLOC_ZVAL(r47);
						} else {
							if (Z_REFCOUNT_P(r47) > 1) {
								{
									zval *orig_ptr = r47;
									if (Z_REFCOUNT_P(orig_ptr) > 1) {
										Z_DELREF_P(orig_ptr);
										ALLOC_ZVAL(r47);
										*r47 = *orig_ptr;
										zval_copy_ctor(r47);
										Z_SET_REFCOUNT_P(r47, 1);
										Z_UNSET_ISREF_P(r47);
									}
								}
							} else {
								FREE_ZVAL(r47);
								PHALCON_ALLOC_ZVAL(r47);
							}
						}
						phalcon_concat_vboth(r47, r48, "</option>", t8 TSRMLS_CC);
						PHALCON_CONCAT_FUNCTION(r52, v3, r47);
						if (v3) {
							Z_DELREF_P(v3);
							if (!Z_REFCOUNT_P(v3)) {
								FREE_ZVAL(v3);
							}
						}
						Z_ADDREF_P(r52);
						v3 = r52;
					END_FOREACH(ac1, fes49, fee49, ah1, hp1);
				} else {
					PHALCON_ALLOC_ZVAL(i2);
					object_init_ex(i2, phalcon_tag_exception_class_entry);
					PHALCON_ALLOC_ZVAL(p12[0]);
					ZVAL_STRING(p12[0], "Options value should be an Array or Object", 1);
					PHALCON_CALL_METHOD_PARAMS_NORETURN(i2, "__construct", 1, p12, PHALCON_CALL_CHECK);
					zend_throw_exception_object(i2 TSRMLS_CC);
					Z_ADDREF_P(i2);
					return;
				}
			}
		}
	}
	PHALCON_ALLOC_ZVAL(t9);
	ZVAL_STRING(t9, "</select>", 1);
	PHALCON_CONCAT_FUNCTION(r53, v3, t9);
	if (v3) {
		Z_DELREF_P(v3);
		if (!Z_REFCOUNT_P(v3)) {
			FREE_ZVAL(v3);
		}
	}
	Z_ADDREF_P(r53);
	v3 = r53;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v3);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML TEXTAREA tag
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, textArea){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_long(v0, 0);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r0);
		phalcon_array_fetch_string(r0, v0, "id", strlen("id"), PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_long(v0, 0, r0 TSRMLS_CC);
	}
	eval_int = phalcon_array_isset_string(v0, "name", strlen("name")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "name", strlen("name"), r1 TSRMLS_CC);
	} else {
		PHALCON_ALLOC_ZVAL(r2);
		phalcon_array_fetch_string(r2, v0, "name", strlen("name"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (!zend_is_true(r2)) {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch_long(r3, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "name", strlen("name"), r3 TSRMLS_CC);
		}
	}
	eval_int = phalcon_array_isset_string(v0, "value", strlen("value")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch_string(r4, v0, "value", strlen("value"), PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r4);
		v1 = r4;
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "value", strlen("value")+1);
	} else {
		PHALCON_ALLOC_ZVAL(r5);
		PHALCON_ALLOC_ZVAL(r6);
		phalcon_array_fetch_long(r6, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r6);
		p0[0] = r6;
		PHALCON_CALL_SELF_PARAMS(r5, this_ptr, "_getvaluefromaction", 1, p0);
		if (v1) {
			Z_DELREF_P(v1);
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r5);
		v1 = r5;
	}
	PHALCON_ALLOC_ZVAL(r7);
	phalcon_array_fetch_long(r7, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r8);
	phalcon_concat_both(r8,  "<textarea id=\"", r7, "\" " TSRMLS_CC);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r8);
	v2 = r8;
	FOREACH_KV(v0, ac0, fes50, fee50, ah0, hp0, v4, v3)
		if (Z_TYPE_P(v4) != IS_LONG) {
			if (!r9) {
				PHALCON_ALLOC_ZVAL(r9);
			} else {
				if (Z_REFCOUNT_P(r9) > 1) {
					{
						zval *orig_ptr = r9;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r9);
							*r9 = *orig_ptr;
							zval_copy_ctor(r9);
							Z_SET_REFCOUNT_P(r9, 1);
							Z_UNSET_ISREF_P(r9);
						}
					}
				} else {
					FREE_ZVAL(r9);
					PHALCON_ALLOC_ZVAL(r9);
				}
			}
			phalcon_concat_vboth(r9, v4, "=\"", v3 TSRMLS_CC);
			if (!r10) {
				PHALCON_ALLOC_ZVAL(r10);
			} else {
				if (Z_REFCOUNT_P(r10) > 1) {
					{
						zval *orig_ptr = r10;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r10);
							*r10 = *orig_ptr;
							zval_copy_ctor(r10);
							Z_SET_REFCOUNT_P(r10, 1);
							Z_UNSET_ISREF_P(r10);
						}
					}
				} else {
					FREE_ZVAL(r10);
					PHALCON_ALLOC_ZVAL(r10);
				}
			}
			phalcon_concat_right(r10, r9, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r11, v2, r10);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r11);
			v2 = r11;
		}
	END_FOREACH(ac0, fes50, fee50, ah0, hp0);
	PHALCON_ALLOC_ZVAL(r12);
	phalcon_concat_both(r12,  ">", v1, "</textarea>" TSRMLS_CC);
	PHALCON_CONCAT_FUNCTION(r13, v2, r12);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r13);
	v2 = r13;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML FORM tag
 *
 * 
 *
 * @paramarray $params
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, form){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL, *v5 = NULL, *v6 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL, *r10 = NULL, *r11 = NULL, *r12 = NULL, *r13 = NULL;
	zval *r14 = NULL, *r15 = NULL, *r16 = NULL, *r17 = NULL, *r18 = NULL, *r19 = NULL, *r20 = NULL;
	zval *r21 = NULL, *r22 = NULL, *r23 = NULL, *r24 = NULL;
	zval *t0 = NULL;
	zval *ac0 = NULL;
	zval *p1[] = { NULL, NULL }, *p5[] = { NULL }, *p6[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	PHALCON_ALLOC_ZVAL(r0);
	PHALCON_CALL_SELF(r0, this_ptr, "_getdispatcher");
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r0);
	v1 = r0;
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(p1[0]);
	ZVAL_STRING(p1[0], "/", 1);
	PHALCON_ALLOC_ZVAL(r2);
	PHALCON_CALL_METHOD(r2, v1, "getparams", PHALCON_CALL_DEFAULT);
	p1[1] = r2;
	PHALCON_CALL_FUNC_PARAMS(r1, "join", 2, p1);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r1);
	v2 = r1;
	eval_int = phalcon_array_isset_long(v0, 0);
	if (!eval_int) {
		eval_int = phalcon_array_isset_string(v0, "action", strlen("action")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r3);
			phalcon_array_fetch_string(r3, v0, "action", strlen("action"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(r3);
			v3 = r3;
		} else {
			PHALCON_ALLOC_ZVAL(r5);
			PHALCON_CALL_METHOD(r5, v1, "getcontrollername", PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(r6);
			PHALCON_CALL_METHOD(r6, v1, "getactionname", PHALCON_CALL_DEFAULT);
			PHALCON_ALLOC_ZVAL(r4);
			phalcon_concat_vboth(r4, r5, "/", r6 TSRMLS_CC);
			if (v3) {
				Z_DELREF_P(v3);
				if (!Z_REFCOUNT_P(v3)) {
					FREE_ZVAL(v3);
				}
			}
			Z_ADDREF_P(r4);
			v3 = r4;
		}
	} else {
		PHALCON_ALLOC_ZVAL(r7);
		phalcon_array_fetch_long(r7, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r7);
		v3 = r7;
	}
	eval_int = phalcon_array_isset_string(v0, "method", strlen("method")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(t0);
		ZVAL_STRING(t0, "post", 1);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "method", strlen("method"), t0 TSRMLS_CC);
	}
	eval_int = phalcon_array_isset_string(v0, "confirm", strlen("confirm")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r8);
		phalcon_array_fetch_string(r8, v0, "onsubmit", strlen("onsubmit"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r10);
		phalcon_array_fetch_string(r10, v0, "onsubmit", strlen("onsubmit"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r11);
		phalcon_array_fetch_string(r11, v0, "confirm", strlen("confirm"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r9);
		phalcon_concat_vboth(r9, r10, ";if(!confirm(\"", r11 TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r12);
		phalcon_concat_right(r12, r9, "\")) { return false; }" TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r13, r8, r12);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "onsubmit", strlen("onsubmit"), r13 TSRMLS_CC);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_unset_string(v0, "confirm", strlen("confirm")+1);
	}
	if (zend_is_true(v2)) {
		PHALCON_ALLOC_ZVAL(r14);
		Z_ADDREF_P(v3);
		p5[0] = v3;
		PHALCON_CALL_STATIC_PARAMS(r14, "phalcon_utils", "geturl", 1, p5);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r14);
		v3 = r14;
	} else {
		PHALCON_ALLOC_ZVAL(r15);
		PHALCON_ALLOC_ZVAL(r16);
		phalcon_concat_vboth(r16, v3, "/", v2 TSRMLS_CC);
		Z_ADDREF_P(r16);
		p6[0] = r16;
		PHALCON_CALL_STATIC_PARAMS(r15, "phalcon_utils", "geturl", 1, p6);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r15);
		v3 = r15;
	}
	eval_int = phalcon_array_isset_string(v0, "parameters", strlen("parameters")+1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r17);
		phalcon_array_fetch_string(r17, v0, "parameters", strlen("parameters"), PHALCON_NOISY_FETCH TSRMLS_CC);
		PHALCON_ALLOC_ZVAL(r18);
		phalcon_concat_left(r18, "?", r17 TSRMLS_CC);
		PHALCON_CONCAT_FUNCTION(r19, v3, r18);
		if (v3) {
			Z_DELREF_P(v3);
			if (!Z_REFCOUNT_P(v3)) {
				FREE_ZVAL(v3);
			}
		}
		Z_ADDREF_P(r19);
		v3 = r19;
	}
	PHALCON_ALLOC_ZVAL(r20);
	phalcon_concat_both(r20,  "<form action=\"", v3, "\" " TSRMLS_CC);
	if (v4) {
		Z_DELREF_P(v4);
		if (!Z_REFCOUNT_P(v4)) {
			FREE_ZVAL(v4);
		}
	}
	Z_ADDREF_P(r20);
	v4 = r20;
	FOREACH_KV(v0, ac0, fes51, fee51, ah0, hp0, v6, v5)
		if (Z_TYPE_P(v6) != IS_LONG) {
			if (!r21) {
				PHALCON_ALLOC_ZVAL(r21);
			} else {
				if (Z_REFCOUNT_P(r21) > 1) {
					{
						zval *orig_ptr = r21;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r21);
							*r21 = *orig_ptr;
							zval_copy_ctor(r21);
							Z_SET_REFCOUNT_P(r21, 1);
							Z_UNSET_ISREF_P(r21);
						}
					}
				} else {
					FREE_ZVAL(r21);
					PHALCON_ALLOC_ZVAL(r21);
				}
			}
			phalcon_concat_vboth(r21, v6, "= \"", v5 TSRMLS_CC);
			if (!r22) {
				PHALCON_ALLOC_ZVAL(r22);
			} else {
				if (Z_REFCOUNT_P(r22) > 1) {
					{
						zval *orig_ptr = r22;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r22);
							*r22 = *orig_ptr;
							zval_copy_ctor(r22);
							Z_SET_REFCOUNT_P(r22, 1);
							Z_UNSET_ISREF_P(r22);
						}
					}
				} else {
					FREE_ZVAL(r22);
					PHALCON_ALLOC_ZVAL(r22);
				}
			}
			phalcon_concat_right(r22, r21, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r23, v4, r22);
			if (v4) {
				Z_DELREF_P(v4);
				if (!Z_REFCOUNT_P(v4)) {
					FREE_ZVAL(v4);
				}
			}
			Z_ADDREF_P(r23);
			v4 = r23;
		}
	END_FOREACH(ac0, fes51, fee51, ah0, hp0);
	PHALCON_ALLOC_ZVAL(r24);
	phalcon_concat_right(r24, v4, ">" TSRMLS_CC);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r24);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds a HTML close FORM tag
 *
 * @returnstring
 */
PHP_METHOD(Phalcon_Tag, endForm){


	RETURN_STRING("</form>", 1);
}

PHP_METHOD(Phalcon_Tag, setTitle){

	zval *v0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	zend_update_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, v0 TSRMLS_CC);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Tag, appendTitle){

	zval *v0 = NULL;
	zval *t0 = NULL;
	zval *r0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_CONCAT_FUNCTION(r0, t0, v0);
	zend_update_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, r0 TSRMLS_CC);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Tag, prependTitle){

	zval *v0 = NULL;
	zval *t0 = NULL;
	zval *r0 = NULL;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	t0 = zend_read_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r0);
	concat_function(r0, v0, t0 TSRMLS_CC);
	zend_update_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, r0 TSRMLS_CC);
	RETURN_NULL();
}

PHP_METHOD(Phalcon_Tag, getTitle){

	zval *t0 = NULL;
	zval *r0 = NULL;

	t0 = zend_read_static_property(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, (zend_bool) ZEND_FETCH_CLASS_SILENT TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r0);
	phalcon_concat_both(r0,  "<title>", t0, "</title>" TSRMLS_CC);
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(r0);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

PHP_METHOD(Phalcon_Tag, stylesheetLink){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL, *r9 = NULL;
	zval *t0 = NULL, *t1 = NULL, *t2 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zz", &v0, &v1) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	if (!v1) {
		PHALCON_INIT_BOOL(v1, 1);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v1, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_string(v0, "href", strlen("href")+1);
	if (!eval_int) {
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r0);
			phalcon_array_fetch_long(r0, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "href", strlen("href"), r0 TSRMLS_CC);
		} else {
			PHALCON_ALLOC_ZVAL(t0);
			ZVAL_STRING(t0, "", 1);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "href", strlen("href"), t0 TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_STRING(v1, "", 0);
	eval_int = phalcon_array_isset_long(v0, 1);
	if (eval_int) {
		PHALCON_ALLOC_ZVAL(r1);
		phalcon_array_fetch_long(r1, v0, 1, PHALCON_NOISY_FETCH TSRMLS_CC);
		if (v1) {
			if (!Z_REFCOUNT_P(v1)) {
				FREE_ZVAL(v1);
			}
		}
		Z_ADDREF_P(r1);
		v1 = r1;
	} else {
		eval_int = phalcon_array_isset_string(v0, "local", strlen("local")+1);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r2);
			phalcon_array_fetch_string(r2, v0, "local", strlen("local"), PHALCON_NOISY_FETCH TSRMLS_CC);
			if (v1) {
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r2);
			v1 = r2;
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_unset_string(v0, "local", strlen("local")+1);
		}
	}
	eval_int = phalcon_array_isset_string(v0, "type", strlen("type")+1);
	if (!eval_int) {
		PHALCON_ALLOC_ZVAL(t1);
		ZVAL_STRING(t1, "text/css", 1);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "type", strlen("type"), t1 TSRMLS_CC);
	}
	if (zend_is_true(v1)) {
		PHALCON_ALLOC_ZVAL(r3);
		PHALCON_ALLOC_ZVAL(r4);
		phalcon_array_fetch_string(r4, v0, "href", strlen("href"), PHALCON_NOISY_FETCH TSRMLS_CC);
		Z_ADDREF_P(r4);
		p0[0] = r4;
		PHALCON_CALL_STATIC_PARAMS(r3, "phalcon_utils", "geturl", 1, p0);
		{
			zval *orig_ptr = v0;
			if (Z_REFCOUNT_P(orig_ptr) > 1) {
				ALLOC_ZVAL(v0);
				*v0 = *orig_ptr;
				zval_copy_ctor(v0);
				Z_SET_REFCOUNT_P(v0, 1);
				Z_UNSET_ISREF_P(v0);
			}
		}
		phalcon_array_update_string(v0, "href", strlen("href"), r3 TSRMLS_CC);
	}
	PHALCON_ALLOC_ZVAL(v2);
	ZVAL_STRING(v2, "<link rel=\"stylesheet\" ", 0);
	FOREACH_KV(v0, ac0, fes52, fee52, ah0, hp0, v4, v3)
		if (Z_TYPE_P(v4) != IS_LONG) {
			if (!r6) {
				PHALCON_ALLOC_ZVAL(r6);
			} else {
				if (Z_REFCOUNT_P(r6) > 1) {
					{
						zval *orig_ptr = r6;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r6);
							*r6 = *orig_ptr;
							zval_copy_ctor(r6);
							Z_SET_REFCOUNT_P(r6, 1);
							Z_UNSET_ISREF_P(r6);
						}
					}
				} else {
					FREE_ZVAL(r6);
					PHALCON_ALLOC_ZVAL(r6);
				}
			}
			phalcon_concat_left(r6, " ", v4 TSRMLS_CC);
			if (!r5) {
				PHALCON_ALLOC_ZVAL(r5);
			} else {
				if (Z_REFCOUNT_P(r5) > 1) {
					{
						zval *orig_ptr = r5;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r5);
							*r5 = *orig_ptr;
							zval_copy_ctor(r5);
							Z_SET_REFCOUNT_P(r5, 1);
							Z_UNSET_ISREF_P(r5);
						}
					}
				} else {
					FREE_ZVAL(r5);
					PHALCON_ALLOC_ZVAL(r5);
				}
			}
			phalcon_concat_vboth(r5, r6, "=\"", v3 TSRMLS_CC);
			if (!r7) {
				PHALCON_ALLOC_ZVAL(r7);
			} else {
				if (Z_REFCOUNT_P(r7) > 1) {
					{
						zval *orig_ptr = r7;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r7);
							*r7 = *orig_ptr;
							zval_copy_ctor(r7);
							Z_SET_REFCOUNT_P(r7, 1);
							Z_UNSET_ISREF_P(r7);
						}
					}
				} else {
					FREE_ZVAL(r7);
					PHALCON_ALLOC_ZVAL(r7);
				}
			}
			phalcon_concat_right(r7, r5, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r8, v2, r7);
			if (v2) {
				Z_DELREF_P(v2);
				if (!Z_REFCOUNT_P(v2)) {
					FREE_ZVAL(v2);
				}
			}
			Z_ADDREF_P(r8);
			v2 = r8;
		}
	END_FOREACH(ac0, fes52, fee52, ah0, hp0);
	PHALCON_ALLOC_ZVAL(t2);
	ZVAL_STRING(t2, ">", 1);
	PHALCON_CONCAT_FUNCTION(r9, v2, t2);
	if (v2) {
		Z_DELREF_P(v2);
		if (!Z_REFCOUNT_P(v2)) {
			FREE_ZVAL(v2);
		}
	}
	Z_ADDREF_P(r9);
	v2 = r9;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v2);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

/**
 * Builds HTML IMG tags
 *
 * @param  array $params
 * @return string
 */
PHP_METHOD(Phalcon_Tag, image){

	zval *v0 = NULL, *v1 = NULL, *v2 = NULL, *v3 = NULL;
	zval *a0 = NULL;
	zval *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL, *r5 = NULL, *r6 = NULL;
	zval *r7 = NULL, *r8 = NULL;
	zval *t0 = NULL, *t1 = NULL;
	zval *ac0 = NULL;
	zval *p0[] = { NULL };
	HashTable *ah0;
	HashPosition hp0;
	zval **hd;
	char *index;
	uint index_len;
	ulong num;
	int htype;
	int eval_int;

	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &v0) == FAILURE) {
		RETURN_NULL();
	}

	if (!v0) {
		PHALCON_ALLOC_ZVAL(v0);
		ZVAL_STRING(v0, "", 0);
	}
	
	if (Z_TYPE_P(v0) != IS_ARRAY) { 
		PHALCON_ALLOC_ZVAL(a0);
		array_init(a0);
		{
			zval *copy;
			ALLOC_ZVAL(copy);
			ZVAL_ZVAL(copy, v0, 1, 0);
			Z_SET_REFCOUNT_P(copy, 0);
			{
				zval *orig_ptr = a0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					Z_DELREF_P(orig_ptr);
					ALLOC_ZVAL(a0);
					*a0 = *orig_ptr;
					zval_copy_ctor(a0);
					Z_SET_REFCOUNT_P(a0, 1);
					Z_UNSET_ISREF_P(a0);
				}
			}
			add_next_index_zval(a0, copy);
		}
		if (v0) {
			if (!Z_REFCOUNT_P(v0)) {
				FREE_ZVAL(v0);
			}
		}
		Z_ADDREF_P(a0);
		v0 = a0;
	}
	eval_int = phalcon_array_isset_string(v0, "src", strlen("src")+1);
	if (!eval_int) {
		eval_int = phalcon_array_isset_long(v0, 0);
		if (eval_int) {
			PHALCON_ALLOC_ZVAL(r0);
			phalcon_array_fetch_long(r0, v0, 0, PHALCON_NOISY_FETCH TSRMLS_CC);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "src", strlen("src"), r0 TSRMLS_CC);
		} else {
			PHALCON_ALLOC_ZVAL(t0);
			ZVAL_STRING(t0, "", 1);
			{
				zval *orig_ptr = v0;
				if (Z_REFCOUNT_P(orig_ptr) > 1) {
					ALLOC_ZVAL(v0);
					*v0 = *orig_ptr;
					zval_copy_ctor(v0);
					Z_SET_REFCOUNT_P(v0, 1);
					Z_UNSET_ISREF_P(v0);
				}
			}
			phalcon_array_update_string(v0, "src", strlen("src"), t0 TSRMLS_CC);
		}
	}
	PHALCON_ALLOC_ZVAL(r1);
	PHALCON_ALLOC_ZVAL(r2);
	phalcon_array_fetch_string(r2, v0, "src", strlen("src"), PHALCON_NOISY_FETCH TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(r3);
	phalcon_concat_left(r3, "img/", r2 TSRMLS_CC);
	Z_ADDREF_P(r3);
	p0[0] = r3;
	PHALCON_CALL_STATIC_PARAMS(r1, "phalcon_utils", "geturl", 1, p0);
	{
		zval *orig_ptr = v0;
		if (Z_REFCOUNT_P(orig_ptr) > 1) {
			ALLOC_ZVAL(v0);
			*v0 = *orig_ptr;
			zval_copy_ctor(v0);
			Z_SET_REFCOUNT_P(v0, 1);
			Z_UNSET_ISREF_P(v0);
		}
	}
	phalcon_array_update_string(v0, "src", strlen("src"), r1 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(v1);
	ZVAL_STRING(v1, "<img ", 0);
	FOREACH_KV(v0, ac0, fes53, fee53, ah0, hp0, v3, v2)
		if (Z_TYPE_P(v3) != IS_LONG) {
			if (!r5) {
				PHALCON_ALLOC_ZVAL(r5);
			} else {
				if (Z_REFCOUNT_P(r5) > 1) {
					{
						zval *orig_ptr = r5;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r5);
							*r5 = *orig_ptr;
							zval_copy_ctor(r5);
							Z_SET_REFCOUNT_P(r5, 1);
							Z_UNSET_ISREF_P(r5);
						}
					}
				} else {
					FREE_ZVAL(r5);
					PHALCON_ALLOC_ZVAL(r5);
				}
			}
			phalcon_concat_left(r5, " ", v3 TSRMLS_CC);
			if (!r4) {
				PHALCON_ALLOC_ZVAL(r4);
			} else {
				if (Z_REFCOUNT_P(r4) > 1) {
					{
						zval *orig_ptr = r4;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r4);
							*r4 = *orig_ptr;
							zval_copy_ctor(r4);
							Z_SET_REFCOUNT_P(r4, 1);
							Z_UNSET_ISREF_P(r4);
						}
					}
				} else {
					FREE_ZVAL(r4);
					PHALCON_ALLOC_ZVAL(r4);
				}
			}
			phalcon_concat_vboth(r4, r5, "=\"", v2 TSRMLS_CC);
			if (!r6) {
				PHALCON_ALLOC_ZVAL(r6);
			} else {
				if (Z_REFCOUNT_P(r6) > 1) {
					{
						zval *orig_ptr = r6;
						if (Z_REFCOUNT_P(orig_ptr) > 1) {
							Z_DELREF_P(orig_ptr);
							ALLOC_ZVAL(r6);
							*r6 = *orig_ptr;
							zval_copy_ctor(r6);
							Z_SET_REFCOUNT_P(r6, 1);
							Z_UNSET_ISREF_P(r6);
						}
					}
				} else {
					FREE_ZVAL(r6);
					PHALCON_ALLOC_ZVAL(r6);
				}
			}
			phalcon_concat_right(r6, r4, "\" " TSRMLS_CC);
			PHALCON_CONCAT_FUNCTION(r7, v1, r6);
			if (v1) {
				Z_DELREF_P(v1);
				if (!Z_REFCOUNT_P(v1)) {
					FREE_ZVAL(v1);
				}
			}
			Z_ADDREF_P(r7);
			v1 = r7;
		}
	END_FOREACH(ac0, fes53, fee53, ah0, hp0);
	PHALCON_ALLOC_ZVAL(t1);
	ZVAL_STRING(t1, ">", 1);
	PHALCON_CONCAT_FUNCTION(r8, v1, t1);
	if (v1) {
		Z_DELREF_P(v1);
		if (!Z_REFCOUNT_P(v1)) {
			FREE_ZVAL(v1);
		}
	}
	Z_ADDREF_P(r8);
	v1 = r8;
	{
		zend_uchar is_ref = Z_ISREF_P(return_value);
		zend_uint refcount = Z_REFCOUNT_P(return_value);
		*(return_value) = *(v1);
		zval_copy_ctor(return_value);
		Z_SET_ISREF_TO_P(return_value, is_ref);
		Z_SET_REFCOUNT_P(return_value, refcount);
	}
	return;
}

int phalcon_test_static_init(zend_class_entry *ce TSRMLS_DC){
	zval *a0;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_static_property(ce, "p", sizeof("p")-1, a0 TSRMLS_CC);
	return 0;
}

int phalcon_transaction_manager_static_init(zend_class_entry *ce TSRMLS_DC){
	zval *a0;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_static_property(ce, "_transactions", sizeof("_transactions")-1, a0 TSRMLS_CC);
	return 0;
}

int phalcon_model_manager_static_init(zend_class_entry *ce TSRMLS_DC){
	zval *a0, *a1;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_static_property(ce, "_models", sizeof("_models")-1, a0 TSRMLS_CC);
	PHALCON_ALLOC_ZVAL(a1);
	array_init(a1);
	zend_update_static_property(ce, "_sourceNames", sizeof("_sourceNames")-1, a1 TSRMLS_CC);
	return 0;
}

int phalcon_tag_static_init(zend_class_entry *ce TSRMLS_DC){
	zval *a0;
	PHALCON_ALLOC_ZVAL(a0);
	array_init(a0);
	zend_update_static_property(ce, "_displayValues", sizeof("_displayValues")-1, a0 TSRMLS_CC);
	return 0;
}

PHP_MINIT_FUNCTION(phalcon){

	zend_class_entry ce0, ce1, ce2, ce3, ce4, ce5, ce6, ce7, ce8, ce9, ce10, ce11, ce12, ce13, ce14, ce15, ce16, ce17, ce18, ce19, ce20, ce21, ce22, ce23, ce24, ce25, ce26, ce27, ce28, ce29, ce30, ce31, ce32, ce33, ce34, ce35, ce36, ce37;

	if(!zend_ce_iterator){
		fprintf(stderr, "Phalcon Error: Interface Iterator was not found");
		return FAILURE;
	}
	if(!spl_ce_SeekableIterator){
		fprintf(stderr, "Phalcon Error: Interface SeekableIterator was not found");
		return FAILURE;
	}
	if(!spl_ce_Countable){
		fprintf(stderr, "Phalcon Error: Interface Countable was not found");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce0, "Phalcon_Dispatcher", phalcon_dispatcher_functions);
	phalcon_dispatcher_class_entry = zend_register_internal_class(&ce0 TSRMLS_CC);
	zend_declare_property_null(phalcon_dispatcher_class_entry, "_finished", sizeof("_finished")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_dispatcher_class_entry, "_controllers", sizeof("_controllers")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_controllersDir", sizeof("_controllersDir")-1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_basePath", sizeof("_basePath")-1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_controllerName", sizeof("_controllerName")-1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_actionName", sizeof("_actionName")-1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_dispatcher_class_entry, "_params", sizeof("_params")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_dispatcher_class_entry, "_returnedValue", sizeof("_returnedValue")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_dispatcher_class_entry, "_lastController", sizeof("_lastController")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_defaultController", sizeof("_defaultController")-1, "index", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_dispatcher_class_entry, "_defaultAction", sizeof("_defaultAction")-1, "index", ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce1, "Phalcon_Response", phalcon_response_functions);
	phalcon_response_class_entry = zend_register_internal_class(&ce1 TSRMLS_CC);
	zend_declare_property_null(phalcon_response_class_entry, "_instance", sizeof("_instance")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce2, "Phalcon_Request", phalcon_request_functions);
	phalcon_request_class_entry = zend_register_internal_class(&ce2 TSRMLS_CC);
	zend_declare_property_null(phalcon_request_class_entry, "_filter", sizeof("_filter")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_request_class_entry, "_instance", sizeof("_instance")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce3, "Phalcon_Controller_Front", phalcon_controller_front_functions);
	phalcon_controller_front_class_entry = zend_register_internal_class(&ce3 TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_instance", sizeof("_instance")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_controllersDir", sizeof("_controllersDir")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_modelsDir", sizeof("_modelsDir")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_viewsDir", sizeof("_viewsDir")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_basePath", sizeof("_basePath")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_baseUri", sizeof("_baseUri")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_dispatcher", sizeof("_dispatcher")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_model", sizeof("_model")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_view", sizeof("_view")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_request", sizeof("_request")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_front_class_entry, "_response", sizeof("_response")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce4, "Phalcon_View", phalcon_view_functions);
	phalcon_view_class_entry = zend_register_internal_class(&ce4 TSRMLS_CC);
	zend_declare_property_null(phalcon_view_class_entry, "_content", sizeof("_content")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_view_class_entry, "_params", sizeof("_params")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(phalcon_view_class_entry, "_layoutsDir", sizeof("_layoutsDir")-1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_view_class_entry, "_templatesBefore", sizeof("_templatesBefore")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_view_class_entry, "_templatesAfter", sizeof("_templatesAfter")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce5, "Phalcon_Test", phalcon_test_functions);
	phalcon_test_class_entry = zend_register_internal_class(&ce5 TSRMLS_CC);
	zend_declare_property_null(phalcon_test_class_entry, "p", sizeof("p")-1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);

	INIT_CLASS_ENTRY(ce6, "Phalcon_Utils", phalcon_utils_functions);
	phalcon_utils_class_entry = zend_register_internal_class(&ce6 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce7, "Phalcon_Db", phalcon_db_functions);
	phalcon_db_class_entry = zend_register_internal_class(&ce7 TSRMLS_CC);
	zend_declare_property_null(phalcon_db_class_entry, "_descriptor", sizeof("_descriptor")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_db_class_entry, "_fetchMode", sizeof("_fetchMode")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_autoCommit", sizeof("_autoCommit")-1, 1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_idConnection", sizeof("_idConnection")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_underTransaction", sizeof("_underTransaction")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_logger", sizeof("_logger")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_ASSOC", sizeof("DB_ASSOC")-1, 1 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_BOTH", sizeof("DB_BOTH")-1, 2 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_NUM", sizeof("DB_NUM")-1, 3 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce8, "Phalcon_Router_Rewrite", phalcon_router_rewrite_functions);
	phalcon_router_rewrite_class_entry = zend_register_internal_class(&ce8 TSRMLS_CC);
	zend_declare_property_null(phalcon_router_rewrite_class_entry, "_controller", sizeof("_controller")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_router_rewrite_class_entry, "_action", sizeof("_action")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_router_rewrite_class_entry, "_params", sizeof("_params")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce9, "Phalcon_Db_Pool", phalcon_db_pool_functions);
	phalcon_db_pool_class_entry = zend_register_internal_class(&ce9 TSRMLS_CC);
	zend_declare_property_null(phalcon_db_pool_class_entry, "_persistentConnection", sizeof("_persistentConnection")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_db_pool_class_entry, "_defaultDescriptor", sizeof("_defaultDescriptor")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce10, "Phalcon_Db_Dialect_Mysql", phalcon_db_dialect_mysql_functions);
	phalcon_db_dialect_mysql_class_entry = zend_register_internal_class(&ce10 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce11, "Phalcon_Db_RawValue", phalcon_db_rawvalue_functions);
	phalcon_db_rawvalue_class_entry = zend_register_internal_class(&ce11 TSRMLS_CC);
	zend_declare_property_null(phalcon_db_rawvalue_class_entry, "_value", sizeof("_value")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce12, "Phalcon_Flash", phalcon_flash_functions);
	phalcon_flash_class_entry = zend_register_internal_class(&ce12 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce13, "Phalcon_Logger", phalcon_logger_functions);
	phalcon_logger_class_entry = zend_register_internal_class(&ce13 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "SPECIAL", sizeof("SPECIAL")-1, 9 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "CUSTOM", sizeof("CUSTOM")-1, 8 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "DEBUG", sizeof("DEBUG")-1, 7 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "INFO", sizeof("INFO")-1, 6 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "NOTICE", sizeof("NOTICE")-1, 5 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "WARNING", sizeof("WARNING")-1, 4 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "ERROR", sizeof("ERROR")-1, 3 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "ALERT", sizeof("ALERT")-1, 2 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "CRITICAL", sizeof("CRITICAL")-1, 1 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_logger_class_entry, "EMERGENCE", sizeof("EMERGENCE")-1, 0 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce14, "Phalcon_Config", phalcon_config_functions);
	phalcon_config_class_entry = zend_register_internal_class(&ce14 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce15, "Phalcon_Filter", phalcon_filter_functions);
	phalcon_filter_class_entry = zend_register_internal_class(&ce15 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce16, "Phalcon_Transaction", phalcon_transaction_functions);
	phalcon_transaction_class_entry = zend_register_internal_class(&ce16 TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_class_entry, "_connection", sizeof("_connection")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_bool(phalcon_transaction_class_entry, "_activeTransaction", sizeof("_activeTransaction")-1, 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_bool(phalcon_transaction_class_entry, "_isNewTransaction", sizeof("_isNewTransaction")-1, 1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_bool(phalcon_transaction_class_entry, "_rollbackOnAbort", sizeof("_rollbackOnAbort")-1, 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_class_entry, "_manager", sizeof("_manager")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(phalcon_transaction_class_entry, "_pointer", sizeof("_pointer")-1, 255, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_class_entry, "_dependencies", sizeof("_dependencies")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_class_entry, "_messages", sizeof("_messages")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_class_entry, "_rollbackRecord", sizeof("_rollbackRecord")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce17, "Phalcon_Transaction_Manager", phalcon_transaction_manager_functions);
	phalcon_transaction_manager_class_entry = zend_register_internal_class(&ce17 TSRMLS_CC);
	zend_declare_property_bool(phalcon_transaction_manager_class_entry, "_initialized", sizeof("_initialized")-1, 0, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(phalcon_transaction_manager_class_entry, "_number", sizeof("_number")-1, 0, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_manager_class_entry, "_transactions", sizeof("_transactions")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(phalcon_transaction_manager_class_entry, "_dependencyPointer", sizeof("_dependencyPointer")-1, 0, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_transaction_manager_class_entry, "_automaticTransaction", sizeof("_automaticTransaction")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce18, "Phalcon_Controller", phalcon_controller_functions);
	phalcon_controller_class_entry = zend_register_internal_class(&ce18 TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_class_entry, "_dispatcher", sizeof("_dispatcher")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_class_entry, "_view", sizeof("_view")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_class_entry, "request", sizeof("request")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_class_entry, "response", sizeof("response")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(phalcon_controller_class_entry, "view", sizeof("view")-1, ZEND_ACC_PUBLIC TSRMLS_CC);

	INIT_CLASS_ENTRY(ce19, "Phalcon_Model_Message", phalcon_model_message_functions);
	phalcon_model_message_class_entry = zend_register_internal_class(&ce19 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_message_class_entry, "_type", sizeof("_type")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_model_message_class_entry, "_message", sizeof("_message")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce20, "Phalcon_Model_Manager", phalcon_model_manager_functions);
	phalcon_model_manager_class_entry = zend_register_internal_class(&ce20 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_manager_class_entry, "_metadata", sizeof("_metadata")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_model_manager_class_entry, "_modelsDir", sizeof("_modelsDir")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_model_manager_class_entry, "_models", sizeof("_models")-1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(phalcon_model_manager_class_entry, "_sourceNames", sizeof("_sourceNames")-1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);

	INIT_CLASS_ENTRY(ce21, "Phalcon_Model_MetaData", phalcon_model_metadata_functions);
	phalcon_model_metadata_class_entry = zend_register_internal_class(&ce21 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_metadata_class_entry, "_metaData", sizeof("_metaData")-1, ZEND_ACC_STATIC|ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_ATTRIBUTES", sizeof("MODELS_ATTRIBUTES")-1, 0 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_PRIMARY_KEY", sizeof("MODELS_PRIMARY_KEY")-1, 1 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_NON_PRIMARY_KEY", sizeof("MODELS_NON_PRIMARY_KEY")-1, 2 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_NOT_NULL", sizeof("MODELS_NOT_NULL")-1, 3 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE", sizeof("MODELS_DATA_TYPE")-1, 4 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_DATA_TYPE_NUMERIC", sizeof("MODELS_DATA_TYPE_NUMERIC")-1, 5 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_DATE_AT", sizeof("MODELS_DATE_AT")-1, 6 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_metadata_class_entry, "MODELS_DATE_IN", sizeof("MODELS_DATE_IN")-1, 7 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce22, "Phalcon_Model_Row", phalcon_model_row_functions);
	phalcon_model_row_class_entry = zend_register_internal_class(&ce22 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_row_class_entry, "_connection", sizeof("_connection")-1, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(phalcon_model_row_class_entry, "_columns", sizeof("_columns")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce23, "Phalcon_Model_Base", phalcon_model_base_functions);
	phalcon_model_base_class_entry = zend_register_internal_class(&ce23 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_base_class_entry, "_manager", sizeof("_manager")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(phalcon_model_base_class_entry, "_uniqueKey", sizeof("_uniqueKey")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_connection", sizeof("_connection")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(phalcon_model_base_class_entry, "_schema", sizeof("_schema")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(phalcon_model_base_class_entry, "_source", sizeof("_source")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_long(phalcon_model_base_class_entry, "_count", sizeof("_count")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_isView", sizeof("_isView")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_dumped", sizeof("_dumped")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_dumpLock", sizeof("_dumpLock")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(phalcon_model_base_class_entry, "_errorMessages", sizeof("_errorMessages")-1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_model_base_class_entry, "_operationMade", sizeof("_operationMade")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_forceExists", sizeof("_forceExists")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_defaultConnection", sizeof("_defaultConnection")-1, 1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_model_base_class_entry, "_connectionName", sizeof("_connectionName")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_disableEvents", sizeof("_disableEvents")-1, 0, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_bool(phalcon_model_base_class_entry, "_refreshPersistance", sizeof("_refreshPersistance")-1, 1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_base_class_entry, "OP_CREATE", sizeof("OP_CREATE")-1, 1 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_base_class_entry, "OP_UPDATE", sizeof("OP_UPDATE")-1, 2 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_model_base_class_entry, "OP_DELETE", sizeof("OP_DELETE")-1, 3 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce24, "Phalcon_Model_MetaData_Memory", phalcon_model_metadata_memory_functions);
	phalcon_model_metadata_memory_class_entry = zend_register_internal_class(&ce24 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce25, "Phalcon_Model_Resultset", phalcon_model_resultset_functions);
	phalcon_model_resultset_class_entry = zend_register_internal_class(&ce25 TSRMLS_CC);
	zend_declare_property_null(phalcon_model_resultset_class_entry, "_model", sizeof("_model")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_model_resultset_class_entry, "_resultResource", sizeof("_resultResource")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_long(phalcon_model_resultset_class_entry, "_pointer", sizeof("_pointer")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_model_resultset_class_entry, "_count", sizeof("_count")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_model_resultset_class_entry, "_activeRow", sizeof("_activeRow")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_class_implements(phalcon_model_resultset_class_entry TSRMLS_CC, 3, zend_ce_iterator, spl_ce_SeekableIterator, spl_ce_Countable);

	INIT_CLASS_ENTRY(ce26, "Phalcon_Tag", phalcon_tag_functions);
	phalcon_tag_class_entry = zend_register_internal_class(&ce26 TSRMLS_CC);
	zend_declare_property_null(phalcon_tag_class_entry, "_displayValues", sizeof("_displayValues")-1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_string(phalcon_tag_class_entry, "_documentTitle", sizeof("_documentTitle")-1, "", ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(phalcon_tag_class_entry, "_dispatcher", sizeof("_dispatcher")-1, ZEND_ACC_PRIVATE|ZEND_ACC_STATIC TSRMLS_CC);

	INIT_CLASS_ENTRY(ce27, "Phalcon_Exception", phalcon_exception_functions);
	phalcon_exception_class_entry = zend_register_internal_class_ex(&ce27, NULL, "exception" TSRMLS_CC);
	if(!phalcon_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Exception' not found when registering class 'Phalcon_Exception'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce28, "Phalcon_Db", phalcon_db_functions);
	phalcon_db_class_entry = zend_register_internal_class(&ce28 TSRMLS_CC);
	zend_declare_property_null(phalcon_db_class_entry, "_descriptor", sizeof("_descriptor")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(phalcon_db_class_entry, "_fetchMode", sizeof("_fetchMode")-1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_autoCommit", sizeof("_autoCommit")-1, 1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_idConnection", sizeof("_idConnection")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_underTransaction", sizeof("_underTransaction")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(phalcon_db_class_entry, "_logger", sizeof("_logger")-1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_ASSOC", sizeof("DB_ASSOC")-1, 1 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_BOTH", sizeof("DB_BOTH")-1, 2 TSRMLS_CC);
	zend_declare_class_constant_long(phalcon_db_class_entry, "DB_NUM", sizeof("DB_NUM")-1, 3 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce29, "Phalcon_Config", phalcon_config_functions);
	phalcon_config_class_entry = zend_register_internal_class(&ce29 TSRMLS_CC);

	INIT_CLASS_ENTRY(ce30, "Phalcon_Tag_Exception", phalcon_tag_exception_functions);
	phalcon_tag_exception_class_entry = zend_register_internal_class_ex(&ce30, NULL, "phalcon_exception" TSRMLS_CC);
	if(!phalcon_tag_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Exception' not found when registering class 'Phalcon_Tag_Exception'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce31, "Phalcon_Db_Exception", phalcon_db_exception_functions);
	phalcon_db_exception_class_entry = zend_register_internal_class_ex(&ce31, NULL, "phalcon_exception" TSRMLS_CC);
	if(!phalcon_db_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Exception' not found when registering class 'Phalcon_Db_Exception'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce32, "Phalcon_Db_Mysql", phalcon_db_mysql_functions);
	phalcon_db_mysql_class_entry = zend_register_internal_class_ex(&ce32, NULL, "phalcon_db" TSRMLS_CC);
	if(!phalcon_db_mysql_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Db' not found when registering class 'Phalcon_Db_Mysql'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce33, "Phalcon_Transaction_Failed", phalcon_transaction_failed_functions);
	phalcon_transaction_failed_class_entry = zend_register_internal_class_ex(&ce33, NULL, "exception" TSRMLS_CC);
	if(!phalcon_transaction_failed_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Exception' not found when registering class 'Phalcon_Transaction_Failed'");
		return FAILURE;
	}
	zend_declare_property_null(phalcon_transaction_failed_class_entry, "_record", sizeof("_record")-1, ZEND_ACC_PRIVATE TSRMLS_CC);

	INIT_CLASS_ENTRY(ce34, "Phalcon_Config_Adapter_Ini", phalcon_config_adapter_ini_functions);
	phalcon_config_adapter_ini_class_entry = zend_register_internal_class_ex(&ce34, NULL, "phalcon_config" TSRMLS_CC);
	if(!phalcon_config_adapter_ini_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Config' not found when registering class 'Phalcon_Config_Adapter_Ini'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce35, "Phalcon_Config_Exception", phalcon_config_exception_functions);
	phalcon_config_exception_class_entry = zend_register_internal_class_ex(&ce35, NULL, "phalcon_exception" TSRMLS_CC);
	if(!phalcon_config_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Exception' not found when registering class 'Phalcon_Config_Exception'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce36, "Phalcon_Model_Exception", phalcon_model_exception_functions);
	phalcon_model_exception_class_entry = zend_register_internal_class_ex(&ce36, NULL, "phalcon_exception" TSRMLS_CC);
	if(!phalcon_model_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Exception' not found when registering class 'Phalcon_Model_Exception'");
		return FAILURE;
	}

	INIT_CLASS_ENTRY(ce37, "Phalcon_View_Exception", phalcon_view_exception_functions);
	phalcon_view_exception_class_entry = zend_register_internal_class_ex(&ce37, NULL, "phalcon_exception" TSRMLS_CC);
	if(!phalcon_view_exception_class_entry){
		fprintf(stderr, "Phalcon Error: Extended class 'Phalcon_Exception' not found when registering class 'Phalcon_View_Exception'");
		return FAILURE;
	}
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phalcon){
	return SUCCESS;
}

zend_module_entry phalcon_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_PHALCON_EXTNAME,
	main_functions,
	PHP_MINIT(phalcon),
	PHP_MSHUTDOWN(phalcon),
	NULL,
	NULL,
	NULL,
#if ZEND_MODULE_API_NO >= 20010901
	PHP_PHALCON_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHALCON
ZEND_GET_MODULE(phalcon)
#endif

