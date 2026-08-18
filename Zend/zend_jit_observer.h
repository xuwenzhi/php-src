/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

/* JIT observer API (new in PHP 8.7). The tracing JIT notifies registered
 * observers on trace side exits (deopts) and trace-compile events, so tooling
 * can see why hot code leaves native code. Multiplexed, mirroring zend_observer;
 * NULL-gated, so unsubscribed builds pay nothing. Event fields are core types. */

#ifndef ZEND_JIT_OBSERVER_H
#define ZEND_JIT_OBSERVER_H

#include "zend_compile.h"

BEGIN_EXTERN_C()

typedef struct _zend_jit_trace_exit_event {
	uint32_t             trace_id;
	uint32_t             exit_num;
	const zend_op_array *op_array;
	const zend_op       *opline;
	zend_execute_data   *execute_data;   /* live frame; read CV types here */
} zend_jit_trace_exit_event;

typedef void (*zend_jit_trace_exit_hook_t)(const zend_jit_trace_exit_event *event);

/* opcache maps its internal ZEND_JIT_EXIT_* codegen bits onto these. */
#define ZEND_JIT_DEOPT_TO_VM         (1 << 0)
#define ZEND_JIT_DEOPT_POLYMORPHISM  (1 << 1)
#define ZEND_JIT_DEOPT_METHOD_CALL   (1 << 2)
#define ZEND_JIT_DEOPT_CLOSURE_CALL  (1 << 3)
#define ZEND_JIT_DEOPT_PACKED_GUARD  (1 << 4)
#define ZEND_JIT_DEOPT_BLACKLISTED   (1 << 5)

typedef struct _zend_jit_exit_desc {
	uint32_t             flags;      /* ZEND_JIT_DEOPT_* */
	const char          *opcode;
	const zend_op_array *op_array;
	const zend_op       *opline;
} zend_jit_exit_desc;

typedef struct _zend_jit_trace_compiled_event {
	uint32_t                  trace_id;
	uint32_t                  exit_count;
	const zend_jit_exit_desc *exits;   /* [exit_count]; valid only during the callback */
} zend_jit_trace_compiled_event;

typedef void (*zend_jit_trace_compiled_hook_t)(const zend_jit_trace_compiled_event *event);

typedef enum _zend_jit_trace_outcome {
	ZEND_JIT_TRACE_OUTCOME_STOP = 0,
	ZEND_JIT_TRACE_OUTCOME_COMPILED,
	ZEND_JIT_TRACE_OUTCOME_ABORT,
	ZEND_JIT_TRACE_OUTCOME_BLACKLIST,
	ZEND_JIT_TRACE_OUTCOME_EXIT_BLACKLIST
} zend_jit_trace_outcome;

/* Mirrors opcache's ZEND_JIT_TRACE_STOP order (core can't include the opcache
 * header); zend_jit_trace.c static_asserts they stay in lock-step. */
#define ZEND_JIT_TRACE_REASON(_) \
	_(LOOP,              "loop") \
	_(RECURSIVE_CALL,    "recursive call") \
	_(RECURSIVE_RET,     "recursive return") \
	_(RETURN,            "return") \
	_(LINK,              "link to another trace") \
	_(INTERPRETER,       "exit to VM interpreter") \
	_(TRAMPOLINE,        "trampoline call") \
	_(PROP_HOOK_CALL,    "property hook call") \
	_(BAD_FUNC,          "bad function call") \
	_(COMPILED,          "compiled") \
	_(ALREADY_DONE,      "already processed") \
	_(ERROR,             "error") \
	_(NOT_SUPPORTED,     "not supported instructions") \
	_(EXCEPTION,         "exception") \
	_(TOO_LONG,          "trace too long") \
	_(TOO_DEEP,          "trace too deep") \
	_(TOO_DEEP_RET,      "trace too deep return") \
	_(DEEP_RECURSION,    "deep recursion") \
	_(LOOP_UNROLL,       "loop unroll limit reached") \
	_(LOOP_EXIT,         "exit from loop") \
	_(RECURSION_EXIT,    "return from recursive function") \
	_(BLACK_LIST,        "trace blacklisted") \
	_(INNER_LOOP,        "inner loop") \
	_(COMPILED_LOOP,     "compiled loop") \
	_(COMPILER_ERROR,    "JIT compilation error") \
	_(NO_SHM,            "insufficient shared memory") \
	_(TOO_MANY_TRACES,   "too many traces") \
	_(TOO_MANY_CHILDREN, "too many side traces") \
	_(TOO_MANY_EXITS,    "too many side exits")

typedef enum _zend_jit_trace_reason {
#define ZEND_JIT_REASON_ENUM(name, desc) ZEND_JIT_REASON_ ## name,
	ZEND_JIT_TRACE_REASON(ZEND_JIT_REASON_ENUM)
#undef ZEND_JIT_REASON_ENUM
	ZEND_JIT_REASON_COUNT
} zend_jit_trace_reason;

ZEND_API const char *zend_jit_trace_reason_name(zend_jit_trace_reason reason);

typedef void (*zend_jit_trace_outcome_hook_t)(uint32_t trace_id,
	zend_jit_trace_outcome outcome, zend_jit_trace_reason reason,
	const zend_op_array *op_array, const zend_op *opline);

typedef struct _zend_jit_observer_handlers {
	zend_jit_trace_exit_hook_t     exit;
	zend_jit_trace_compiled_hook_t compiled;
	zend_jit_trace_outcome_hook_t  outcome;
} zend_jit_observer_handlers;

/* true iff >=1 observer is registered; the JIT tests this before building events. */
extern ZEND_API bool zend_jit_observers_active;

/* Register/unregister at MINIT/MSHUTDOWN only (not from within a callback). */
ZEND_API void zend_jit_observer_register(const zend_jit_observer_handlers *handlers);
ZEND_API void zend_jit_observer_unregister(const zend_jit_observer_handlers *handlers);

ZEND_API void zend_jit_observer_notify_exit(const zend_jit_trace_exit_event *event);
ZEND_API void zend_jit_observer_notify_compiled(const zend_jit_trace_compiled_event *event);
ZEND_API void zend_jit_observer_notify_outcome(uint32_t trace_id,
	zend_jit_trace_outcome outcome, zend_jit_trace_reason reason,
	const zend_op_array *op_array, const zend_op *opline);

END_EXTERN_C()

#endif /* ZEND_JIT_OBSERVER_H */
