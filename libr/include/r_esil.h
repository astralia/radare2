/* radare2 - LGPL - Copyright 2022-2024 - pancake */

#ifndef R_ESIL_H
#define R_ESIL_H

#include <r_reg.h>
#include <r_vec.h>
#include <r_util.h>
#include <r_lib.h>
#include <sdb/ht_uu.h>
#include <sdb/ht_up.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	USE_NEW_ESIL	0

#define esilprintf(op, fmt, ...) r_strbuf_setf (&op->esil, fmt, ##__VA_ARGS__)
// only flags that affect control flow
enum {
	R_ESIL_FLAG_ZERO = 1,
	R_ESIL_FLAG_CARRY = 2,
	R_ESIL_FLAG_OVERFLOW = 4,
	R_ESIL_FLAG_PARITY = 8,
	R_ESIL_FLAG_SIGN = 16,
	// ...
};

#define ESIL_INTERNAL_PREFIX '$'
#define ESIL_STACK_NAME "esil.ram"
enum {
	R_ESIL_OP_TYPE_UNKNOWN = 0x1,
	R_ESIL_OP_TYPE_CONTROL_FLOW,
	R_ESIL_OP_TYPE_MEM_READ = 0x4,
	R_ESIL_OP_TYPE_MEM_WRITE = 0x8,
	R_ESIL_OP_TYPE_REG_WRITE = 0x10,
	R_ESIL_OP_TYPE_MATH = 0x20,
	R_ESIL_OP_TYPE_CUSTOM = 0x40,
	R_ESIL_OP_TYPE_FLAG = 0x80,
	R_ESIL_OP_TYPE_TRAP = 0x100, // syscall, interrupts, breakpoints, ...
	R_ESIL_OP_TYPE_CRYPTO = 0x200,
};

typedef struct r_esil_t ESIL;

typedef bool (*REsilHandlerCB)(ESIL *esil, ut32 h, void *user);

typedef struct r_esil_handler_t {
	REsilHandlerCB cb;
	void *user;
} REsilHandler;

typedef struct r_esil_change_reg_t {
	int idx;
	ut64 data;
} REsilRegChange;

typedef struct r_esil_change_mem_t {
	int idx;
	ut8 data;
} REsilMemChange;

typedef struct {
	const char *name;
	ut64 value;
	// TODO: size
} REsilRegAccess;

typedef struct {
	char *data;
	ut64 addr;
	// TODO: size
} REsilMemoryAccess;

typedef struct {
	union {
		REsilRegAccess reg;
		REsilMemoryAccess mem;
	};
	bool is_write;
	bool is_reg;
} REsilTraceAccess;

typedef struct {
	ut64 addr;
	ut32 start;
	ut32 end; // 1 past the end of the op for this index
} REsilTraceOp;

static inline void fini_access(REsilTraceAccess *access) {
	if (access->is_reg) {
		return;
	}
	free (access->mem.data);
}

R_VEC_TYPE(RVecTraceOp, REsilTraceOp);
R_VEC_TYPE_WITH_FINI(RVecAccess, REsilTraceAccess, fini_access);

typedef struct {
	RVecTraceOp ops;
	RVecAccess accesses;
	HtUU *loop_counts;
} REsilTraceDB;

typedef struct r_esil_trace_t {
	REsilTraceDB db;
	int idx;
	int end_idx;
	int cur_idx;
	HtUP *registers;
	HtUP *memory;
	RRegArena *arena[R_REG_TYPE_LAST];
	ut64 stack_addr;
	ut64 stack_size;
	ut8 *stack_data;
} REsilTrace;

typedef bool (*REsilHookRegWriteCB)(ESIL *esil, const char *name, ut64 *val);

typedef struct r_esil_callbacks_t {
	void *user;
	/* callbacks */
//	bool (*hook_flag_read)(ESIL *esil, const char *flag, ut64 *num);
	bool (*hook_command)(ESIL *esil, const char *op);
	bool (*hook_mem_read)(ESIL *esil, ut64 addr, ut8 *buf, int len);
	bool (*mem_read)(ESIL *esil, ut64 addr, ut8 *buf, int len);
	bool (*hook_mem_write)(ESIL *esil, ut64 addr, const ut8 *buf, int len);
	bool (*mem_write)(ESIL *esil, ut64 addr, const ut8 *buf, int len);
	bool (*hook_reg_read)(ESIL *esil, const char *name, ut64 *res, int *size);
	bool (*reg_read)(ESIL *esil, const char *name, ut64 *res, int *size);
	REsilHookRegWriteCB hook_reg_write;
	bool (*reg_write)(ESIL *esil, const char *name, ut64 val);
} REsilCallbacks;

typedef bool (*REsilMemSwitch)(void *mem, ut32 idx);
typedef bool (*REsilMemRead)(void *mem, ut64 addr, ut8 *buf, int len);
typedef bool (*REsilMemWrite)(void *mem, ut64 addr, const ut8 *buf, int len);

typedef struct r_esil_memory_interface_t {
	union {
		void *mem;
		void *user;
	};
	REsilMemSwitch mem_switch;
	REsilMemRead mem_read;
	REsilMemWrite mem_write;
} REsilMemInterface;

//check if name is a valid register identifier
typedef bool (*REsilIsReg)(void *reg, const char *name);
typedef bool (*REsilRegRead)(void *reg, const char *name, ut64 *val);
typedef bool (*REsilRegWrite)(void *reg, const char *name, ut64 val);
typedef ut32 (*REsilRegSize)(void *reg, const char *name);
// typedef bool (*REsilRegAlias)(void *reg, int kind, const char *name);

typedef struct r_esil_register_interface_t {
	union {
		void *reg;
		void *user;
	};
	REsilIsReg is_reg; /// IsReg breaks the REsilReg prefix naming
	REsilRegRead reg_read;
	REsilRegWrite reg_write;
	REsilRegSize reg_size;
	// REsilRegAlias reg_alias;
} REsilRegInterface;

typedef void (*REsilVoyeurRegRead)(void *user, const char *name, ut64 val);
typedef void (*REsilVoyeurRegWrite)(void *user, const char *name, ut64 old, ut64 val);
typedef void (*REsilVoyeurMemRead)(void *user, ut64 addr, const ut8 *buf, int len);
typedef	void (*REsilVoyeurMemWrite)(void *user, ut64 addr, const ut8 *old, const ut8 *buf, int len);
typedef void (*REsilVoyeurOp)(void *user, const char *op);

typedef struct r_esil_voyeur_t {
	void *user;
	union {
		REsilVoyeurRegRead reg_read;
		REsilVoyeurRegWrite reg_write;
		REsilVoyeurMemRead mem_read;
		REsilVoyeurMemWrite mem_write;
		REsilVoyeurOp op;
		void *vfn;
	};
} REsilVoyeur;

typedef enum {
	R_ESIL_VOYEUR_REG_READ = 0,
	R_ESIL_VOYEUR_REG_WRITE,
	R_ESIL_VOYEUR_MEM_READ,
	R_ESIL_VOYEUR_MEM_WRITE,
	R_ESIL_VOYEUR_OP,
	R_ESIL_VOYEUR_LAST,
	R_ESIL_VOYEUR_HIGH_MASK = 0x7,
	R_ESIL_VOYEUR_ERR = UT32_MAX,
} REsilVoyeurType;

#define	VOYEUR_TYPE_BITS	3
#define	VOYEUR_SHIFT_LEFT	((sizeof (ut32) << 3) - VOYEUR_TYPE_BITS)
#define	VOYEUR_TYPE_MASK	(R_ESIL_VOYEUR_HIGH_MASK << VOYEUR_SHIFT_LEFT)
#define	MAX_VOYEURS	(UT32_MAX ^ VOYEUR_TYPE_MASK)

typedef struct r_esil_options_t {
	int nowrite;
	int iotrap;
	int exectrap;
} REsilOptions;

typedef struct r_esil_t {
	struct r_anal_t *anal; // required for io, reg, and call esil_init/fini of the selected arch plugin
	char **stack;
	ut64 addrmask;
	int stacksize;
	int stackptr;
	ut32 skip;
	int nowrite;
	int iotrap;
	int exectrap;
	int parse_stop;
	int parse_goto;
	int parse_goto_count;
	int verbose;
	ut64 flags;
	ut64 addr;
	ut64 stack_addr;
	ut32 stack_size;
	int delay; 		// mapped to $ds in ESIL
	ut64 jump_target; 	// mapped to $jt in ESIL
	int jump_target_set; 	// mapped to $js in ESIL
	int trap;
	int data_align;
	ut32 trap_code; // extend into a struct to store more exception info?
	// parity flag? done with cur
	ut64 old;	//used for carry-flagging and borrow-flagging
	ut64 cur;	//used for carry-flagging and borrow-flagging
	ut8 lastsz;	//in bits //used for signature-flag
	/* native ops and custom ops */
	HtPP *ops;
	struct r_esil_plugin_t *curplug; // ???
	char *current_opstr;
	SdbMini *interrupts;
	SdbMini *syscalls;
	//this is a disgusting workaround, because we have no ht-like storage without magic keys, that you cannot use, with int-keys
	REsilHandler *intr0;
	REsilHandler *sysc0;
	RList *plugins;
	RList *active_plugins;
	/* deep esil parsing fills this */
	Sdb *stats;
	REsilTrace *trace;
	REsilRegInterface reg_if;
	REsilMemInterface mem_if;
	RIDStorage voyeur[R_ESIL_VOYEUR_LAST];
	REsilCallbacks cb;
	REsilCallbacks ocb;
	bool ocb_set;
	// this is so cursed, can we please remove external commands from esil internals.
	// Function pointers are fine, but not commands
	char *cmd_step; // r2 (external) command to run before a step is performed
	char *cmd_step_out; // r2 (external) command to run after a step is performed
	char *cmd_intr; // r2 (external) command to run when an interrupt occurs
	char *cmd_trap; // r2 (external) command to run when a trap occurs
	char *cmd_mdev; // r2 (external) command to run when an memory mapped device address is used
	char *cmd_todo; // r2 (external) command to run when esil expr contains TODO
	char *cmd_ioer; // r2 (external) command to run when esil fails to IO
	char *mdev_range; // string containing the r_str_range to match for read/write accesses
	bool (*cmd)(ESIL *esil, const char *name, ut64 a0, ut64 a1);
	void *user;
	int stack_fd;	// ahem, let's not do this
	bool in_cmd_step;
#if 0
	bool trace_enabled;
#endif
} REsil;

enum {
	R_ESIL_PARM_INVALID = 0,
	R_ESIL_PARM_REG,
	R_ESIL_PARM_NUM,
};

typedef struct r_anal_ref_char_t {
	char *str;
	char *cols;
} RAnalRefStr;

typedef struct r_esil_plugin_t {
	RPluginMeta meta;
	char *arch;
	void *(*init)(REsil *esil);			// can allocate stuff and return that
	void (*fini)(REsil *esil, void *user);	// deallocates allocated things from init
} REsilPlugin;

// Some kind of container, pointer to plugin + pointer to user
typedef struct r_esil_active_plugin_t {
	REsilPlugin *plugin;
	void *user;
} REsilActivePlugin;

R_API REsil *r_esil_new(int stacksize, int iotrap, unsigned int addrsize);
R_API bool r_esil_init(REsil *esil, int stacksize, bool iotrap,
	ut32 addrsize, REsilRegInterface *reg_if, REsilMemInterface *mem_if);
R_API REsil *r_esil_new_ex(int stacksize, bool iotrap, ut32 addrsize,
	REsilRegInterface *reg_if, REsilMemInterface *mem_if);
//this should replace existing r_esil_new
R_API REsil *r_esil_new_simple(ut32 addrsize, void *reg, void *iob);
//R_API REsil *r_esil_new_simple(ut32 addrsize, struct r_reg_t *reg, struct r_io_bind_t *iob);
R_API ut32 r_esil_add_voyeur(REsil *esil, void *user, void *vfn, REsilVoyeurType vt);
R_API void r_esil_del_voyeur(REsil *esil, ut32 vid);
R_API void r_esil_reset(REsil *esil);
R_API void r_esil_set_pc(REsil *esil, ut64 addr);
R_API bool r_esil_setup(REsil *esil, struct r_anal_t *anal, bool romem, bool stats, bool nonull);
R_API bool r_esil_setup_ops(REsil *esil);
R_API void r_esil_fini(REsil *esil);
R_API void r_esil_free(REsil *esil);
R_API bool r_esil_runword(REsil *esil, const char *word);
R_API bool r_esil_parse(REsil *esil, const char *str);
R_API bool r_esil_dumpstack(REsil *esil);
// XXX must be internal imho
R_API bool r_esil_mem_read(REsil *esil, ut64 addr, ut8 *buf, int len);
R_API bool r_esil_mem_read_silent(REsil *esil, ut64 addr, ut8 *buf, int len);
R_API bool r_esil_mem_write(REsil *esil, ut64 addr, const ut8 *buf, int len);
R_API bool r_esil_mem_write_silent(REsil *esil, ut64 addr, const ut8 *buf, int len);
R_API bool r_esil_reg_read(REsil *esil, const char *regname, ut64 *val, ut32 *size);
R_API bool r_esil_reg_read_silent(REsil *esil, const char *name, ut64 *val, ut32 *size);
R_API bool r_esil_reg_write(REsil *esil, const char *name, ut64 val);
R_API bool r_esil_reg_write_silent(REsil *esil, const char *dst, ut64 val);
R_API bool r_esil_pushnum(REsil *esil, ut64 num);
R_API bool r_esil_push(REsil *esil, const char *str);
#if R2_590
R_API const char *r_esil_pop(REsil *esil);
#else
R_API char *r_esil_pop(REsil *esil);
#endif
typedef bool (*REsilOpCb)(REsil *esil);

typedef struct r_esil_operation_t {
	REsilOpCb code;
	ut32 push; // amount of operands pushed
	ut32 pop; // amount of operands popped
	ut32 type;
	const char *info;
} REsilOp;

// esil2c
typedef struct {
	REsil *esil;
	RStrBuf *sb;
	void *anal; // RAnal*
} REsilC;

R_API REsilC *r_esil_toc_new(struct r_anal_t *anal, int bits);
R_API void r_esil_toc_free(REsilC *ec);
R_API char *r_esil_toc(REsilC *esil, const char *expr);

R_API char*r_esil_opstr(REsil*, int mode);

R_API bool r_esil_set_op(REsil *esil, const char *op, REsilOpCb code, ut32 push, ut32 pop, ut32 type, const char *info);
R_API REsilOp *r_esil_get_op(REsil *esil, const char *op);
R_API void r_esil_del_op(REsil *esil, const char *op);
R_API void r_esil_stack_free(REsil *esil);
R_API int r_esil_condition(REsil *esil, const char *str);

// R2_600 - unify these 3 functions into one
R_API int r_esil_get_parm_type(REsil *esil, const char *str);
R_API int r_esil_get_parm(REsil *esil, const char *str, ut64 *num);
R_API bool r_esil_get_parm_size(REsil *esil, const char *str, ut64 *num, int *size);

// esil_handler.c
R_API bool r_esil_handlers_init(REsil *esil);
R_API bool r_esil_set_interrupt(REsil *esil, ut32 intr_num, REsilHandlerCB cb, void *user);
R_API REsilHandlerCB r_esil_get_interrupt(REsil *esil, ut32 intr_num);
R_API void r_esil_del_interrupt(REsil *esil, ut32 intr_num);
R_API bool r_esil_set_syscall(REsil *esil, ut32 sysc_num, REsilHandlerCB cb, void *user);
R_API REsilHandlerCB r_esil_get_syscall(REsil *esil, ut32 sysc_num);
R_API void r_esil_del_syscall(REsil *esil, ut32 sysc_num);
R_API int r_esil_fire_interrupt(REsil *esil, ut32 intr_num);
R_API int r_esil_do_syscall(REsil *esil, ut32 sysc_num);
R_API void r_esil_handlers_fini(REsil *esil);

// esil_compiler.c

typedef struct {
	REsil *esil;
	char *str;
	void *priv;
} REsilCompiler;

R_API REsilCompiler *r_esil_compiler_new(void);
R_API void r_esil_compiler_reset(REsilCompiler *ec);
R_API void r_esil_compiler_free(REsilCompiler *ec);
R_API char *r_esil_compiler_tostring(REsilCompiler *ec);
R_API bool r_esil_compiler_parse(REsilCompiler *ec, const char *expr);
R_API char *r_esil_compiler_unparse(REsilCompiler *ec, const char *expr);
R_API void r_esil_compiler_use(REsilCompiler *ec, REsil *esil);

// esil_plugin.c
R_API bool r_esil_plugins_init(REsil *esil);
R_API void r_esil_plugins_fini(REsil *esil);
R_API bool r_esil_plugin_add(REsil *esil, REsilPlugin *plugin);
R_API void r_esil_plugin_del(REsil *esil, const char *name);
R_API bool r_esil_plugin_remove(REsil *esil, REsilPlugin *plugin);
R_API bool r_esil_plugin_activate(REsil *esil, const char *name);
R_API void r_esil_plugin_deactivate(REsil *esil, const char *name);

typedef struct r_esil_stats_t {
	Sdb *db;
	ut32 vid[R_ESIL_VOYEUR_LAST];
} REsilStats;

R_API void r_esil_mem_ro(REsil *esil, bool mem_readonly);
//R_API void r_esil_stats(REsil *esil, bool enable);
R_API void r_esil_stats(REsil *esil, REsilStats *stats, bool enable);

/* trace */
R_API REsilTrace *r_esil_trace_new(REsil *esil);
R_API void r_esil_trace_free(REsilTrace *trace);
#include <r_arch.h>
R_API void r_esil_trace_op(REsil *esil, struct r_anal_op_t *op);
R_API void r_esil_trace_list(REsil *esil, int format);
R_API void r_esil_trace_show(REsil *esil, int idx, int format);
R_API void r_esil_trace_restore(REsil *esil, int idx);
R_API ut64 r_esil_trace_loopcount(REsilTrace *etrace, ut64 addr);
R_API void r_esil_trace_loopcount_increment(REsilTrace *etrace, ut64 addr);

R_API bool r_esil_reg_write_silent(REsil *esil, const char *name, ut64 num);
R_API bool r_esil_reg_read_nocallback(REsil *esil, const char *regname, ut64 *num, int *size);

extern REsilPlugin r_esil_plugin_null;
extern REsilPlugin r_esil_plugin_dummy;
extern REsilPlugin r_esil_plugin_forth;

#ifdef __cplusplus
}
#endif

#endif
