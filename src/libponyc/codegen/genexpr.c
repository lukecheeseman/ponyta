#include "genexpr.h"
#include "genname.h"
#include "genbox.h"
#include "gencontrol.h"
#include "genident.h"
#include "genmatch.h"
#include "genoperator.h"
#include "genreference.h"
#include "gencall.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include <assert.h>

LLVMValueRef gen_expr(compile_t* c, ast_t* ast)
{
  LLVMValueRef ret;
  bool has_scope = ast_has_scope(ast);

  if(has_scope)
    codegen_pushscope(c, ast);

  switch(ast_id(ast))
  {
    case TK_SEQ:
      ret = gen_seq(c, ast);
      break;

    case TK_FVARREF:
    case TK_FLETREF:
      ret = gen_fieldload(c, ast);
      break;

    case TK_EMBEDREF:
      ret = gen_fieldembed(c, ast);
      break;

    case TK_PARAMREF:
      ret = gen_param(c, ast);
      break;

    case TK_VAR:
    case TK_LET:
    case TK_MATCH_CAPTURE:
      ret = gen_localdecl(c, ast);
      break;

    case TK_VARREF:
    case TK_LETREF:
      ret = gen_localload(c, ast);
      break;

    case TK_IF:
      ret = gen_if(c, ast);
      break;

    case TK_WHILE:
      ret = gen_while(c, ast);
      break;

    case TK_REPEAT:
      ret = gen_repeat(c, ast);
      break;

    case TK_TRY:
    case TK_TRY_NO_CHECK:
      ret = gen_try(c, ast);
      break;

    case TK_MATCH:
      ret = gen_match(c, ast);
      break;

    case TK_CALL:
      ret = gen_call(c, ast);
      break;

    case TK_CONSUME:
    {
      ast_t* ref = ast_childidx(ast, 1);
      ret = gen_expr(c, ref);
      switch(ast_id(ref))
      {
        case TK_LETREF:
        case TK_VARREF:
        {
          const char* name = ast_name(ast_child(ref));
          codegen_local_lifetime_end(c, name);
          break;
        }
        case TK_THIS:
        case TK_PARAMREF:
          break;
        default:
          assert(0);
          break;
      }
      break;
    }

    case TK_RECOVER:
      ret = gen_expr(c, ast_childidx(ast, 1));
      break;

    case TK_BREAK:
      ret = gen_break(c, ast);
      break;

    case TK_CONTINUE:
      ret = gen_continue(c, ast);
      break;

    case TK_RETURN:
      ret = gen_return(c, ast);
      break;

    case TK_ERROR:
      ret = gen_error(c, ast);
      break;

    case TK_IS:
      ret = gen_is(c, ast);
      break;

    case TK_ISNT:
      ret = gen_isnt(c, ast);
      break;

    case TK_ASSIGN:
      ret = gen_assign(c, ast);
      break;

    case TK_THIS:
      ret = gen_this(c, ast);
      break;

    case TK_TRUE:
      ret = LLVMConstInt(c->ibool, 1, false);
      break;

    case TK_FALSE:
      ret = LLVMConstInt(c->ibool, 0, false);
      break;

    case TK_INT:
      ret = gen_int(c, ast);
      break;

    case TK_FLOAT:
      ret = gen_float(c, ast);
      break;

    case TK_STRING:
      ret = gen_string(c, ast);
      break;

    case TK_TUPLE:
      ret = gen_tuple(c, ast);
      break;

    case TK_FFICALL:
      ret = gen_ffi(c, ast);
      break;

    case TK_ADDRESS:
      ret = gen_addressof(c, ast);
      break;

    case TK_DIGESTOF:
      ret = gen_digestof(c, ast);
      break;

    case TK_DONTCARE:
      ret = GEN_NOVALUE;
      break;

    case TK_COMPILE_INTRINSIC:
      ast_error(c->opt->check.errors, ast, "unimplemented compile intrinsic");
      return NULL;

    case TK_COMPILE_ERROR:
    {
      ast_t* reason_seq = ast_child(ast);
      ast_t* reason = ast_child(reason_seq);
      ast_error(c->opt->check.errors, ast, "compile error: %s",
        ast_name(reason));
      return NULL;
    }

    case TK_CONSTANT_OBJECT:
      ret = gen_constant_object(c, ast);
      break;

    case TK_VECTOR:
      ret = gen_vector(c, ast);
      break;

    default:
      ast_error(c->opt->check.errors, ast, "not implemented (codegen unknown)");
      return NULL;
  }

  if(has_scope)
  {
    LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(c->builder);
    LLVMValueRef terminator = LLVMGetBasicBlockTerminator(current_bb);
    if(terminator == NULL)
      codegen_scope_lifetime_end(c);
    codegen_popscope(c);
  }

  return ret;
}

static LLVMValueRef assign_to_tuple(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  // Cast each component.
  assert(ast_id(type) == TK_TUPLETYPE);

  int count = LLVMCountStructElementTypes(l_type);
  size_t buf_size = count * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);
  LLVMGetStructElementTypes(l_type, elements);

  LLVMValueRef result = LLVMGetUndef(l_type);

  ast_t* type_child = ast_child(type);
  int i = 0;

  while(type_child != NULL)
  {
    LLVMValueRef r_child = LLVMBuildExtractValue(c->builder, r_value, i, "");
    LLVMValueRef cast_value = gen_assign_cast(c, elements[i], r_child,
      type_child);

    if(cast_value == NULL)
    {
      ponyint_pool_free_size(buf_size, elements);
      return NULL;
    }

    result = LLVMBuildInsertValue(c->builder, result, cast_value, i, "");
    type_child = ast_sibling(type_child);
    i++;
  }

  ponyint_pool_free_size(buf_size, elements);
  return result;
}

LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type)
{
  if(r_value <= GEN_NOVALUE)
    return r_value;

  LLVMTypeRef r_type = LLVMTypeOf(r_value);

  if(r_type == l_type)
    return r_value;

  switch(LLVMGetTypeKind(l_type))
  {
    case LLVMIntegerTypeKind:
    case LLVMHalfTypeKind:
    case LLVMFloatTypeKind:
    case LLVMDoubleTypeKind:
    {
      // This can occur if an LLVM intrinsic returns an i1 or a tuple that
      // contains an i1. Extend the i1 to an ibool.
      if((r_type == c->i1) && (l_type == c->ibool))
        return LLVMBuildZExt(c->builder, r_value, l_type, "");

      assert(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind);
      return gen_unbox(c, type, r_value);
    }

    case LLVMPointerTypeKind:
    {
      r_value = gen_box(c, type, r_value);

      if(r_value == NULL)
        return NULL;

      return LLVMBuildBitCast(c->builder, r_value, l_type, "");
    }

    case LLVMStructTypeKind:
    {
      if(LLVMGetTypeKind(r_type) == LLVMPointerTypeKind)
      {
        r_value = gen_unbox(c, type, r_value);
        assert(LLVMGetTypeKind(LLVMTypeOf(r_value)) == LLVMStructTypeKind);
      }

      return assign_to_tuple(c, l_type, r_value, type);
    }

    default: {}
  }

  assert(0);
  return NULL;
}

static LLVMValueRef gen_constant_vector(compile_t* c, ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, members);
  const char* obj_name = ast_name(name);
  LLVMValueRef obj = LLVMGetNamedGlobal(c->module, obj_name);
  if(obj != NULL)
    return obj;

  ast_t* type = ast_type(ast);
  AST_GET_CHILDREN(type, package, id, typeargs);

  reach_type_t* t = reach_type(c->reach, type);
  reach_type_t* elem_t = reach_type(c->reach, ast_child(typeargs));

  uint32_t elem_count = (uint32_t)ast_childcount(members);
  LLVMValueRef elems[elem_count];

  uint32_t elem = 0;
  for(ast_t* member = ast_child(members);
      member != NULL;
      member = ast_sibling(member))
  {
    elems[elem++] = gen_expr(c, member);
  }

  LLVMValueRef array = LLVMConstArray(elem_t->use_type, elems, elem_count);

  LLVMValueRef args[2] = {t->desc, array};
  LLVMValueRef inst = LLVMConstNamedStruct(t->structure, args, 2);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, t->structure, obj_name);
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMInternalLinkage);
  return g_inst;
}

LLVMValueRef gen_constant_object(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  if(is_vector(type))
    return gen_constant_vector(c, ast);

  AST_GET_CHILDREN(ast, name, members);
  const char* obj_name = ast_name(name);
  LLVMValueRef obj = LLVMGetNamedGlobal(c->module, obj_name);
  if(obj != NULL)
    return obj;

  AST_GET_CHILDREN(type, package, id);
  reach_type_t* t = reach_type(c->reach, type);

  uint32_t field_count = t->field_count + 1;
  LLVMValueRef args[field_count];
  args[0] = t->desc;

  uint32_t field = 0;
  for(ast_t* member = ast_child(members);
      member != NULL;
      field++, member = ast_sibling(member))
  {
    // All field objects must also be compile time objects.
    // When we assign to an embedded field we know we must have
    if(t->fields[field].embed)
    {
      LLVMValueRef constant = gen_expr(c, member);
      args[field + 1] = LLVMGetInitializer(constant);
      LLVMDeleteGlobal(constant);
    }
    else
    {
      args[field + 1] = gen_expr(c, member);
    }
  }

  LLVMValueRef inst = LLVMConstNamedStruct(t->structure, args, field_count);
  LLVMValueRef g_inst = LLVMAddGlobal(c->module, t->structure, obj_name);
  LLVMSetInitializer(g_inst, inst);
  LLVMSetGlobalConstant(g_inst, true);
  LLVMSetLinkage(g_inst, LLVMInternalLinkage);
  return g_inst;
}

LLVMValueRef gen_vector(compile_t* c, ast_t* ast)
{
  ast_t* type = ast_type(ast);
  reach_type_t* t = reach_type(c->reach, type);

  // Static or virtual dispatch.
  token_id cap = cap_dispatch(type);
  LLVMValueRef func = reach_method(t, cap, stringtab("_update"), NULL)->func;

  LLVMTypeRef f_type = LLVMGetElementType(LLVMTypeOf(func));
  LLVMTypeRef params[3];
  LLVMGetParamTypes(f_type, params);

  LLVMValueRef args[3];
  args[0] = gencall_alloc(c, t);

  size_t index = 0;
  ast_t* elem = ast_child(ast);
  elem = ast_sibling(elem);
  while(elem != NULL)
  {
    args[1] = LLVMConstInt(c->i64, index++, false);
    args[2] = gen_assign_cast(c, params[2], gen_expr(c, elem), ast_type(elem));
    codegen_call(c, func, args, 3);
    codegen_debugloc(c, NULL);
    elem = ast_sibling(elem);
  }

  return args[0];
}
