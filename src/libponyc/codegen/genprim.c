#include "genprim.h"
#include "genname.h"
#include "genfun.h"
#include "gencall.h"
#include "gentrace.h"
#include "genopt.h"
#include "../pkg/platformfuns.h"
#include "../pass/names.h"
#include "../type/assemble.h"

#include <assert.h>

#define FIND_METHOD(name) \
  reachable_method_t* m = reach_method(t, stringtab(name), NULL); \
  if(m == NULL) return; \
  m->intrinsic = true;

static void start_function(compile_t* c, reachable_method_t* m,
  LLVMTypeRef result, LLVMTypeRef* params, unsigned count)
{
  m->func_type = LLVMFunctionType(result, params, count, false);
  m->func = codegen_addfun(c, m->full_name, m->func_type);
  codegen_startfun(c, m->func, NULL, NULL);
}

static void pointer_create(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("create");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMValueRef result = LLVMConstNull(t->use_type);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_alloc(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_alloc");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef len = LLVMGetParam(m->func, 1);
  LLVMValueRef args[2];
  args[0] = codegen_ctx(c);
  args[1] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_alloc", args, 2, "");
  result = LLVMBuildBitCast(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_realloc(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_realloc");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef args[3];
  args[0] = codegen_ctx(c);
  args[1] = LLVMGetParam(m->func, 0);

  LLVMValueRef len = LLVMGetParam(m->func, 1);
  args[2] = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef result = gencall_runtime(c, "pony_realloc", args, 3, "");
  result = LLVMBuildBitCast(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_unsafe(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("_unsafe");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMGetParam(m->func, 0));
  codegen_finishfun(c);
}

static void pointer_apply(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_apply");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t_elem->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_update(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_update");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = t_elem->use_type;
  start_function(c, m, t_elem->use_type, params, 3);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index = LLVMGetParam(m->func, 1);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef loc = LLVMBuildGEP(c->builder, elem_ptr, &index, 1, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  LLVMBuildStore(c->builder, LLVMGetParam(m->func, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_offset(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_offset");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t->use_type, params, 2);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);

  // Return ptr + (n * sizeof(len)).
  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef result = LLVMBuildAdd(c->builder, src, offset, "");
  result = LLVMBuildIntToPtr(c->builder, result, t->use_type, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_insert(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_insert");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef src = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef dst = LLVMBuildAdd(c->builder, src, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;

  // memmove(ptr + (n * sizeof(elem)), ptr, len * sizeof(elem))
  gencall_runtime(c, "memmove", args, 3, "");

  // Return ptr.
  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_delete(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_delete");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = c->intptr;
  start_function(c, m, t_elem->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef n = LLVMGetParam(m->func, 1);
  LLVMValueRef len = LLVMGetParam(m->func, 2);

  LLVMValueRef elem_ptr = LLVMBuildBitCast(c->builder, ptr,
    LLVMPointerType(t_elem->use_type, 0), "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, elem_ptr, "");

  LLVMValueRef dst = LLVMBuildPtrToInt(c->builder, elem_ptr, c->intptr, "");
  LLVMValueRef offset = LLVMBuildMul(c->builder, n, l_size, "");
  LLVMValueRef src = LLVMBuildAdd(c->builder, dst, offset, "");
  LLVMValueRef elen = LLVMBuildMul(c->builder, len, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildIntToPtr(c->builder, dst, c->void_ptr, "");
  args[1] = LLVMBuildIntToPtr(c->builder, src, c->void_ptr, "");
  args[2] = elen;

  // memmove(ptr, ptr + (n * sizeof(elem)), len * sizeof(elem))
  gencall_runtime(c, "memmove", args, 3, "");

  // Return ptr[0].
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void pointer_copy_to(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_copy_to");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = t->use_type;
  params[2] = c->intptr;
  start_function(c, m, t->use_type, params, 3);

  // Set up a constant integer for the allocation size.
  size_t size = (size_t)LLVMABISizeOfType(c->target_data, t_elem->use_type);
  LLVMValueRef l_size = LLVMConstInt(c->intptr, size, false);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef ptr2 = LLVMGetParam(m->func, 1);
  LLVMValueRef n = LLVMGetParam(m->func, 2);
  LLVMValueRef elen = LLVMBuildMul(c->builder, n, l_size, "");

  LLVMValueRef args[3];
  args[0] = LLVMBuildBitCast(c->builder, ptr2, c->void_ptr, "");
  args[1] = LLVMBuildBitCast(c->builder, ptr, c->void_ptr, "");
  args[2] = elen;

  // memcpy(ptr2, ptr, n * sizeof(elem))
  gencall_runtime(c, "memcpy", args, 3, "");

  LLVMBuildRet(c->builder, ptr);
  codegen_finishfun(c);
}

static void pointer_usize(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("usize");
  start_function(c, m, c->intptr, &t->use_type, 1);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef result = LLVMBuildPtrToInt(c->builder, ptr, c->intptr, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_pointer_methods(compile_t* c, reachable_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reachable_type_t* t_elem = reach_type(c->reachable, typearg);

  pointer_create(c, t);
  pointer_alloc(c, t, t_elem);

  pointer_realloc(c, t, t_elem);
  pointer_unsafe(c, t);
  pointer_apply(c, t, t_elem);
  pointer_update(c, t, t_elem);
  pointer_offset(c, t, t_elem);
  pointer_insert(c, t, t_elem);
  pointer_delete(c, t, t_elem);
  pointer_copy_to(c, t, t_elem);
  pointer_usize(c, t);
}

static void vector_apply(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_apply");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = c->intptr;
  start_function(c, m, t_elem->use_type, params, 2);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index[2];
  index[0] = LLVMConstInt(c->intptr, 0, false);
  index[1] = LLVMGetParam(m->func, 1);

  LLVMValueRef loc = LLVMBuildInBoundsGEP(c->builder, ptr, index, 2, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void vector_update(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("_update");

  LLVMTypeRef params[3];
  params[0] = t->use_type;
  params[1] = c->intptr;
  params[2] = t_elem->use_type;
  start_function(c, m, t_elem->use_type, params, 3);

  LLVMValueRef ptr = LLVMGetParam(m->func, 0);
  LLVMValueRef index[2];
  index[0] = LLVMConstInt(c->intptr, 0, false);
  index[1] = LLVMGetParam(m->func, 1);

  LLVMValueRef loc = LLVMBuildInBoundsGEP(c->builder, ptr, index, 2, "");
  LLVMValueRef result = LLVMBuildLoad(c->builder, loc, "");
  LLVMBuildStore(c->builder, LLVMGetParam(m->func, 2), loc);

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_vector_methods(compile_t* c, reachable_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reachable_type_t* t_elem = reach_type(c->reachable, typearg);

  vector_apply(c, t, t_elem);
  vector_update(c, t, t_elem);
}

static void maybe_create(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  FIND_METHOD("create");

  LLVMTypeRef params[2];
  params[0] = t->use_type;
  params[1] = t_elem->use_type;
  start_function(c, m, t->use_type, params, 2);

  LLVMValueRef param = LLVMGetParam(m->func, 1);
  LLVMValueRef result = LLVMBuildBitCast(c->builder, param, t->use_type, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void maybe_none(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("none");
  start_function(c, m, t->use_type, &t->use_type, 1);

  LLVMBuildRet(c->builder, LLVMConstNull(t->use_type));
  codegen_finishfun(c);
}

static void maybe_apply(compile_t* c, reachable_type_t* t,
  reachable_type_t* t_elem)
{
  // Returns the receiver if it isn't null.
  FIND_METHOD("apply");
  start_function(c, m, t_elem->use_type, &t->use_type, 1);

  LLVMValueRef result = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, result, "");

  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  result = LLVMBuildBitCast(c->builder, result, t_elem->use_type, "");
  LLVMBuildRet(c->builder, result);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gencall_throw(c);

  codegen_finishfun(c);
}

static void maybe_is_none(compile_t* c, reachable_type_t* t)
{
  // Returns true if the receiver is null.
  FIND_METHOD("is_none");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef receiver = LLVMGetParam(m->func, 0);
  LLVMValueRef test = LLVMBuildIsNull(c->builder, receiver, "");

  LLVMBuildRet(c->builder, test);
  codegen_finishfun(c);
}

void genprim_maybe_methods(compile_t* c, reachable_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);
  reachable_type_t* t_elem = reach_type(c->reachable, typearg);

  maybe_create(c, t, t_elem);
  maybe_none(c, t);
  maybe_apply(c, t, t_elem);
  maybe_is_none(c, t);
}

void genprim_array_trace(compile_t* c, reachable_type_t* t)
{
  // Get the type argument for the array. This will be used to generate the
  // per-element trace call.
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typearg = ast_child(typeargs);

  codegen_startfun(c, t->trace_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->trace_fn, LLVMCCallConv);
  LLVMValueRef ctx = LLVMGetParam(t->trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->trace_fn, 1);

  // Read the base pointer.
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->use_type,
    "array");
  LLVMValueRef pointer_ptr = LLVMBuildStructGEP(c->builder, object, 3, "");
  LLVMValueRef pointer = LLVMBuildLoad(c->builder, pointer_ptr, "pointer");

  // Trace the base pointer.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = pointer;
  gencall_runtime(c, "pony_trace", args, 2, "");

  if(!gentrace_needed(typearg))
  {
    LLVMBuildRetVoid(c->builder);
    codegen_finishfun(c);
    return;
  }

  reachable_type_t* t_elem = reach_type(c->reachable, typearg);
  pointer = LLVMBuildBitCast(c->builder, pointer,
    LLVMPointerType(t_elem->use_type, 0), "");

  LLVMBasicBlockRef entry_block = LLVMGetInsertBlock(c->builder);
  LLVMBasicBlockRef cond_block = codegen_block(c, "cond");
  LLVMBasicBlockRef body_block = codegen_block(c, "body");
  LLVMBasicBlockRef post_block = codegen_block(c, "post");

  // Read the count.
  LLVMValueRef count_ptr = LLVMBuildStructGEP(c->builder, object, 1, "");
  LLVMValueRef count = LLVMBuildLoad(c->builder, count_ptr, "count");
  LLVMBuildBr(c->builder, cond_block);

  // While the index is less than the count, trace an element. The initial
  // index when coming from the entry block is zero.
  LLVMPositionBuilderAtEnd(c->builder, cond_block);
  LLVMValueRef phi = LLVMBuildPhi(c->builder, c->intptr, "");
  LLVMValueRef zero = LLVMConstInt(c->intptr, 0, false);
  LLVMAddIncoming(phi, &zero, &entry_block, 1);
  LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntULT, phi, count, "");
  LLVMBuildCondBr(c->builder, test, body_block, post_block);

  // The phi node is the index. Get the element and trace it.
  LLVMPositionBuilderAtEnd(c->builder, body_block);
  LLVMValueRef elem_ptr = LLVMBuildGEP(c->builder, pointer, &phi, 1, "elem");
  LLVMValueRef elem = LLVMBuildLoad(c->builder, elem_ptr, "");
  gentrace(c, ctx, elem, typearg);

  // Add one to the phi node and branch back to the cond block.
  LLVMValueRef one = LLVMConstInt(c->intptr, 1, false);
  LLVMValueRef inc = LLVMBuildAdd(c->builder, phi, one, "");
  body_block = LLVMGetInsertBlock(c->builder);
  LLVMAddIncoming(phi, &inc, &body_block, 1);
  LLVMBuildBr(c->builder, cond_block);

  LLVMPositionBuilderAtEnd(c->builder, post_block);
  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
}

static void platform_freebsd(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("freebsd");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_freebsd(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_linux(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("linux");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_linux(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_osx(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("osx");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_macosx(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_windows(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("windows");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_windows(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_x86(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("x86");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_x86(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_arm(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("arm");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_arm(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_lp64(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("lp64");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_lp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_llp64(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("llp64");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_llp64(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_ilp32(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("ilp32");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_ilp32(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_native128(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("native128");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result =
    LLVMConstInt(c->i1, target_is_native128(c->opt->triple), false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void platform_debug(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("debug");
  start_function(c, m, c->i1, &t->use_type, 1);

  LLVMValueRef result = LLVMConstInt(c->i1, !c->opt->release, false);
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

void genprim_platform_methods(compile_t* c, reachable_type_t* t)
{
  platform_freebsd(c, t);
  platform_linux(c, t);
  platform_osx(c, t);
  platform_windows(c, t);
  platform_x86(c, t);
  platform_arm(c, t);
  platform_lp64(c, t);
  platform_llp64(c, t);
  platform_ilp32(c, t);
  platform_native128(c, t);
  platform_debug(c, t);
}

typedef struct num_conv_t
{
  const char* type_name;
  const char* fun_name;
  LLVMTypeRef type;
  int size;
  bool is_signed;
  bool is_float;
} num_conv_t;

static void number_conversion(compile_t* c, num_conv_t* from, num_conv_t* to,
  bool native128)
{
  if(!native128 &&
    ((from->is_float && (to->size > 64)) ||
    (to->is_float && (from->size > 64)))
    )
  {
    return;
  }

  reachable_type_t* t = reach_type_name(c->reachable, from->type_name);

  if(t == NULL)
    return;

  FIND_METHOD(to->fun_name);
  start_function(c, m, to->type, &from->type, 1);

  LLVMValueRef arg = LLVMGetParam(m->func, 0);
  LLVMValueRef result;

  if(from->is_float)
  {
    if(to->is_float)
    {
      if(from->size < to->size)
        result = LLVMBuildFPExt(c->builder, arg, to->type, "");
      else if(from->size > to->size)
        result = LLVMBuildFPTrunc(c->builder, arg, to->type, "");
      else
        result = arg;
    } else if(to->is_signed) {
      result = LLVMBuildFPToSI(c->builder, arg, to->type, "");
    } else {
      result = LLVMBuildFPToUI(c->builder, arg, to->type, "");
    }
  } else if(to->is_float) {
    if(from->is_signed)
      result = LLVMBuildSIToFP(c->builder, arg, to->type, "");
    else
      result = LLVMBuildUIToFP(c->builder, arg, to->type, "");
  } else if(from->size > to->size) {
      result = LLVMBuildTrunc(c->builder, arg, to->type, "");
  } else if(from->size < to->size) {
    if(from->is_signed)
      result = LLVMBuildSExt(c->builder, arg, to->type, "");
    else
      result = LLVMBuildZExt(c->builder, arg, to->type, "");
  } else {
    result = arg;
  }

  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void number_conversions(compile_t* c)
{
  num_conv_t ilp32_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i32, 32, true, false},
    {"USize", "usize", c->i32, 32, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  num_conv_t lp64_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i64, 64, true, false},
    {"ULong", "ulong", c->i64, 64, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  num_conv_t llp64_conv[] =
  {
    {"I8", "i8", c->i8, 8, true, false},
    {"I16", "i16", c->i16, 16, true, false},
    {"I32", "i32", c->i32, 32, true, false},
    {"I64", "i64", c->i64, 64, true, false},

    {"U8", "u8", c->i8, 8, false, false},
    {"U16", "u16", c->i16, 16, false, false},
    {"U32", "u32", c->i32, 32, false, false},
    {"U64", "u64", c->i64, 64, false, false},
    {"I128", "i128", c->i128, 128, true, false},
    {"U128", "u128", c->i128, 128, false, false},

    {"ILong", "ilong", c->i32, 32, true, false},
    {"ULong", "ulong", c->i32, 32, false, false},
    {"ISize", "isize", c->i64, 64, true, false},
    {"USize", "usize", c->i64, 64, false, false},

    {"F32", "f32", c->f32, 32, false, true},
    {"F64", "f64", c->f64, 64, false, true},

    {NULL, NULL, NULL, false, false, false}
  };

  num_conv_t* conv = NULL;

  if(target_is_ilp32(c->opt->triple))
    conv = ilp32_conv;
  else if(target_is_lp64(c->opt->triple))
    conv = lp64_conv;
  else if(target_is_llp64(c->opt->triple))
    conv = llp64_conv;

  assert(conv != NULL);
  bool native128 = target_is_native128(c->opt->triple);

  for(num_conv_t* from = conv; from->type_name != NULL; from++)
  {
    for(num_conv_t* to = conv; to->type_name != NULL; to++)
      number_conversion(c, from, to, native128);
  }
}

static void f32_from_bits(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("from_bits");

  LLVMTypeRef params[2];
  params[0] = c->f32;
  params[1] = c->i32;
  start_function(c, m, c->f32, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f32_bits(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("bits");
  start_function(c, m, c->i32, &c->f32, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i32, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_from_bits(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("from_bits");

  LLVMTypeRef params[2];
  params[0] = c->f64;
  params[1] = c->i64;
  start_function(c, m, c->f64, params, 2);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 1),
    c->f64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void f64_bits(compile_t* c, reachable_type_t* t)
{
  FIND_METHOD("bits");
  start_function(c, m, c->i64, &c->f64, 1);

  LLVMValueRef result = LLVMBuildBitCast(c->builder, LLVMGetParam(m->func, 0),
    c->i64, "");
  LLVMBuildRet(c->builder, result);
  codegen_finishfun(c);
}

static void fp_as_bits(compile_t* c)
{
  reachable_type_t* t;

  if((t = reach_type_name(c->reachable, "F32")) != NULL)
  {
    f32_from_bits(c, t);
    f32_bits(c, t);
  }

  if((t = reach_type_name(c->reachable, "F64")) != NULL)
  {
    f64_from_bits(c, t);
    f64_bits(c, t);
  }
}

static void make_cpuid(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    LLVMTypeRef elems[4] = {c->i32, c->i32, c->i32, c->i32};
    LLVMTypeRef r_type = LLVMStructTypeInContext(c->context, elems, 4, false);
    LLVMTypeRef f_type = LLVMFunctionType(r_type, &c->i32, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.cpuid", f_type);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, NULL, NULL);

    LLVMValueRef cpuid = LLVMConstInlineAsm(f_type,
      "cpuid", "={ax},={bx},={cx},={dx},{ax}", false, false);
    LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);

    LLVMValueRef result = LLVMBuildCall(c->builder, cpuid, &zero, 1, "");
    LLVMBuildRet(c->builder, result);

    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

static void make_rdtscp(compile_t* c)
{
  if(target_is_x86(c->opt->triple))
  {
    // i64 @llvm.x86.rdtscp(i8*)
    LLVMTypeRef f_type = LLVMFunctionType(c->i64, &c->void_ptr, 1, false);
    LLVMValueRef rdtscp = LLVMAddFunction(c->module, "llvm.x86.rdtscp",
      f_type);

    // i64 @internal.x86.rdtscp(i32*)
    LLVMTypeRef i32_ptr = LLVMPointerType(c->i32, 0);
    f_type = LLVMFunctionType(c->i64, &i32_ptr, 1, false);
    LLVMValueRef fun = codegen_addfun(c, "internal.x86.rdtscp", f_type);
    LLVMSetFunctionCallConv(fun, LLVMCCallConv);
    codegen_startfun(c, fun, NULL, NULL);

    // Cast i32* to i8* and call the intrinsic.
    LLVMValueRef arg = LLVMGetParam(fun, 0);
    arg = LLVMBuildBitCast(c->builder, arg, c->void_ptr, "");
    LLVMValueRef result = LLVMBuildCall(c->builder, rdtscp, &arg, 1, "");
    LLVMBuildRet(c->builder, result);

    codegen_finishfun(c);
  } else {
    (void)c;
  }
}

void genprim_builtins(compile_t* c)
{
  number_conversions(c);
  fp_as_bits(c);
  make_cpuid(c);
  make_rdtscp(c);
}

void genprim_reachable_init(compile_t* c, ast_t* program)
{
  // Look for primitives in all packages that have _init or _final methods.
  // Mark them as reachable.
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_PRIMITIVE)
        {
          AST_GET_CHILDREN(entity, id, typeparams);

          if(ast_id(typeparams) == TK_NONE)
          {
            ast_t* type = type_builtin(c->opt, entity, ast_name(id));
            ast_t* finit = ast_get(entity, c->str__init, NULL);
            ast_t* ffinal = ast_get(entity, c->str__final, NULL);

            if(finit != NULL)
            {
              reach(c->reachable, &c->next_type_id, type, c->str__init, NULL);
              ast_free_unattached(finit);
            }

            if(ffinal != NULL)
            {
              reach(c->reachable, &c->next_type_id, type, c->str__final, NULL);
              ast_free_unattached(ffinal);
            }

            ast_free_unattached(type);
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }
}
