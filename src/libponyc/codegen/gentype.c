#include "gentype.h"
#include "genname.h"
#include "gendesc.h"
#include "genprim.h"
#include "gentrace.h"
#include "genfun.h"
#include "genopt.h"
#include "genserialise.h"
#include "../ast/id.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static bool make_opaque_struct(compile_t* c, reach_type_t* t)
{
  switch(ast_id(t->ast))
  {
    case TK_NOMINAL:
    {
      switch(t->underlying)
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          t->use_type = c->object_ptr;
          return true;

        default: {}
      }

      // Find the primitive type, if there is one.
      AST_GET_CHILDREN(t->ast, pkg, id);
      const char* package = ast_name(pkg);
      const char* name = ast_name(id);

      bool ilp32 = target_is_ilp32(c->opt->triple);
      bool llp64 = target_is_llp64(c->opt->triple);
      bool lp64 = target_is_lp64(c->opt->triple);

      if(package == c->str_builtin)
      {
        if(name == c->str_Bool)
          t->primitive = c->ibool;
        else if(name == c->str_I8)
          t->primitive = c->i8;
        else if(name == c->str_U8)
          t->primitive = c->i8;
        else if(name == c->str_I16)
          t->primitive = c->i16;
        else if(name == c->str_U16)
          t->primitive = c->i16;
        else if(name == c->str_I32)
          t->primitive = c->i32;
        else if(name == c->str_U32)
          t->primitive = c->i32;
        else if(name == c->str_I64)
          t->primitive = c->i64;
        else if(name == c->str_U64)
          t->primitive = c->i64;
        else if(name == c->str_I128)
          t->primitive = c->i128;
        else if(name == c->str_U128)
          t->primitive = c->i128;
        else if(ilp32 && name == c->str_ILong)
          t->primitive = c->i32;
        else if(ilp32 && name == c->str_ULong)
          t->primitive = c->i32;
        else if(ilp32 && name == c->str_ISize)
          t->primitive = c->i32;
        else if(ilp32 && name == c->str_USize)
          t->primitive = c->i32;
        else if(lp64 && name == c->str_ILong)
          t->primitive = c->i64;
        else if(lp64 && name == c->str_ULong)
          t->primitive = c->i64;
        else if(lp64 && name == c->str_ISize)
          t->primitive = c->i64;
        else if(lp64 && name == c->str_USize)
          t->primitive = c->i64;
        else if(llp64 && name == c->str_ILong)
          t->primitive = c->i32;
        else if(llp64 && name == c->str_ULong)
          t->primitive = c->i32;
        else if(llp64 && name == c->str_ISize)
          t->primitive = c->i64;
        else if(llp64 && name == c->str_USize)
          t->primitive = c->i64;
        else if(name == c->str_F32)
          t->primitive = c->f32;
        else if(name == c->str_F64)
          t->primitive = c->f64;
        else if(name == c->str_Pointer)
        {
          t->use_type = c->void_ptr;
          return true;
        }
        else if(name == c->str_Maybe)
        {
          t->use_type = c->void_ptr;
          return true;
        }
      }

      t->structure = LLVMStructCreateNamed(c->context, t->name);
      t->structure_ptr = LLVMPointerType(t->structure, 0);

      if(t->primitive != NULL)
        t->use_type = t->primitive;
      else
        t->use_type = t->structure_ptr;

      return true;
    }

    case TK_TUPLETYPE:
      t->primitive = LLVMStructCreateNamed(c->context, t->name);
      t->use_type = t->primitive;
      t->field_count = (uint32_t)ast_childcount(t->ast);
      return true;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      // Just a raw object pointer.
      t->use_type = c->object_ptr;
      return true;

    default: {}
  }

  assert(0);
  return false;
}

static void make_debug_basic(compile_t* c, reach_type_t* t)
{
  uint64_t size = LLVMABISizeOfType(c->target_data, t->primitive);
  uint64_t align = LLVMABIAlignmentOfType(c->target_data, t->primitive);
  unsigned encoding;

  if(is_bool(t->ast))
  {
    encoding = DW_ATE_boolean;
  } else if(is_float(t->ast)) {
    encoding = DW_ATE_float;
  } else if(is_signed(t->ast)) {
    encoding = DW_ATE_signed;
  } else {
    encoding = DW_ATE_unsigned;
  }

  t->di_type = LLVMDIBuilderCreateBasicType(c->di, t->name,
    8 * size, 8 * align, encoding);
}

static void make_debug_prototype(compile_t* c, reach_type_t* t)
{
  t->di_type = LLVMDIBuilderCreateReplaceableStruct(c->di,
    t->name, c->di_unit, t->di_file, (unsigned)ast_line(t->ast));

  if(t->underlying != TK_TUPLETYPE)
  {
    t->di_type_embed = t->di_type;
    t->di_type = LLVMDIBuilderCreatePointerType(c->di, t->di_type_embed, 0, 0);
  }
}

static void make_debug_info(compile_t* c, reach_type_t* t)
{
  ast_t* def = (ast_t*)ast_data(t->ast);
  source_t* source;

  if(def != NULL)
    source = ast_source(def);
  else
    source = ast_source(t->ast);

  t->di_file = LLVMDIBuilderCreateFile(c->di, source->file);

  switch(t->underlying)
  {
    case TK_TUPLETYPE:
    case TK_STRUCT:
      make_debug_prototype(c, t);
      return;

    case TK_PRIMITIVE:
    {
      if(t->primitive != NULL)
        make_debug_basic(c, t);
      else
        make_debug_prototype(c, t);
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      make_debug_prototype(c, t);
      return;

    default: {}
  }

  assert(0);
}

static void make_box_type(compile_t* c, reach_type_t* t)
{
  if(t->primitive == NULL)
    return;

  const char* box_name = genname_box(t->name);
  t->structure = LLVMStructCreateNamed(c->context, box_name);

  LLVMTypeRef elements[2];
  elements[0] = LLVMPointerType(t->desc_type, 0);
  elements[1] = t->primitive;
  LLVMStructSetBody(t->structure, elements, 2, false);

  t->structure_ptr = LLVMPointerType(t->structure, 0);
}

static void make_global_instance(compile_t* c, reach_type_t* t)
{
  // Not a primitive type.
  if(t->underlying != TK_PRIMITIVE)
    return;

  // No instance for machine word types.
  if(t->primitive != NULL)
    return;

  // Create a unique global instance.
  const char* inst_name = genname_instance(t->name);

  LLVMValueRef args[1];
  args[0] = t->desc;
  LLVMValueRef value = LLVMConstNamedStruct(t->structure, args, 1);

  t->instance = LLVMAddGlobal(c->module, t->structure, inst_name);
  LLVMSetInitializer(t->instance, value);
  LLVMSetGlobalConstant(t->instance, true);
  LLVMSetLinkage(t->instance, LLVMPrivateLinkage);
}

static void make_dispatch(compile_t* c, reach_type_t* t)
{
  // Do nothing if we're not an actor.
  if(t->underlying != TK_ACTOR)
    return;

  // Create a dispatch function.
  const char* dispatch_name = genname_dispatch(t->name);
  t->dispatch_fn = codegen_addfun(c, dispatch_name, c->dispatch_type);
  LLVMSetFunctionCallConv(t->dispatch_fn, LLVMCCallConv);
  LLVMSetLinkage(t->dispatch_fn, LLVMExternalLinkage);
  codegen_startfun(c, t->dispatch_fn, NULL, NULL);

  LLVMBasicBlockRef unreachable = codegen_block(c, "unreachable");

  // Read the message ID.
  LLVMValueRef msg = LLVMGetParam(t->dispatch_fn, 2);
  LLVMValueRef id_ptr = LLVMBuildStructGEP(c->builder, msg, 1, "");
  LLVMValueRef id = LLVMBuildLoad(c->builder, id_ptr, "id");

  // Store a reference to the dispatch switch. When we build behaviours, we
  // will add cases to this switch statement based on message ID.
  t->dispatch_switch = LLVMBuildSwitch(c->builder, id, unreachable, 0);

  // Mark the default case as unreachable.
  LLVMPositionBuilderAtEnd(c->builder, unreachable);
  LLVMBuildUnreachable(c->builder);
  codegen_finishfun(c);
}

static bool make_vector_struct(compile_t* c, reach_type_t* t)
{
  ast_t* typeargs = ast_childidx(t->ast, 2);
  AST_GET_CHILDREN(typeargs, elem_type, size);
  lexint_t* size_val = ast_int(ast_child(size));

  size_t buf_size = 2 * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);

  // Create the type descriptor as element 0.
  elements[0] = LLVMPointerType(t->desc_type, 0);

  reach_type_t* elem_reach_type = reach_type(c->reach, elem_type);

  assert(lexint_cmp64(size_val, UINT32_MAX) <= 0);
  elements[1] = LLVMArrayType(elem_reach_type->use_type,
                              (unsigned int) size_val->low);

  LLVMStructSetBody(t->structure, elements, 2, false);
  ponyint_pool_free_size(buf_size, elements);
  return true;
}

static bool make_struct(compile_t* c, reach_type_t* t)
{
  LLVMTypeRef type;
  int extra = 0;

  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
      return true;

    case TK_TUPLETYPE:
      type = t->primitive;
      break;

    case TK_STRUCT:
      // Pointer and Maybe will have no structure.
      if(t->structure == NULL)
        return true;

      type = t->structure;
      break;

    case TK_PRIMITIVE:
      // Machine words will have a primitive.
      if(t->primitive != NULL)
      {
        // The ABI size for machine words and tuples is the boxed size.
        t->abi_size = (size_t)LLVMABISizeOfType(c->target_data, t->structure);
        return true;
      }

      extra = 1;
      type = t->structure;
      break;

    case TK_CLASS:
      if(is_vector(t->ast))
        return make_vector_struct(c, t);
      extra = 1;
      type = t->structure;
      break;

    case TK_ACTOR:
      extra = 2;
      type = t->structure;
      break;

    default:
      assert(0);
      return false;
  }

  size_t buf_size = (t->field_count + extra) * sizeof(LLVMTypeRef);
  LLVMTypeRef* elements = (LLVMTypeRef*)ponyint_pool_alloc_size(buf_size);

  // Create the type descriptor as element 0.
  if(extra > 0)
    elements[0] = LLVMPointerType(t->desc_type, 0);

  // Create the actor pad as element 1.
  if(extra > 1)
    elements[1] = c->actor_pad;

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    if(t->fields[i].embed)
      elements[i + extra] = t->fields[i].type->structure;
    else
      elements[i + extra] = t->fields[i].type->use_type;

    if(elements[i + extra] == NULL)
    {
      assert(0);
      return false;
    }
  }

  LLVMStructSetBody(type, elements, t->field_count + extra, false);
  ponyint_pool_free_size(buf_size, elements);
  return true;
}

static LLVMMetadataRef make_debug_field(compile_t* c, reach_type_t* t,
  uint32_t i)
{
  const char* name;
  char buf[32];
  unsigned flags = 0;
  uint64_t offset = 0;
  ast_t* ast;

  if(t->underlying != TK_TUPLETYPE)
  {
    ast_t* def = (ast_t*)ast_data(t->ast);
    ast_t* members = ast_childidx(def, 4);
    ast = ast_childidx(members, i);
    name = ast_name(ast_child(ast));

    if(is_name_private(name))
      flags |= DW_FLAG_Private;

    uint32_t extra = 0;

    if(t->underlying != TK_STRUCT)
      extra++;

    if(t->underlying == TK_ACTOR)
      extra++;

    offset = LLVMOffsetOfElement(c->target_data, t->structure, i + extra);
  } else {
    snprintf(buf, 32, "_%d", i + 1);
    name = buf;
    ast = t->ast;
    offset = LLVMOffsetOfElement(c->target_data, t->primitive, i);
  }

  LLVMTypeRef type;
  LLVMMetadataRef di_type;

  if(t->fields[i].embed)
  {
    type = t->fields[i].type->structure;
    di_type = t->fields[i].type->di_type_embed;
  } else {
    type = t->fields[i].type->use_type;
    di_type = t->fields[i].type->di_type;
  }

  uint64_t size = LLVMABISizeOfType(c->target_data, type);
  uint64_t align = LLVMABIAlignmentOfType(c->target_data, type);

  return LLVMDIBuilderCreateMemberType(c->di, c->di_unit, name, t->di_file,
    (unsigned)ast_line(ast), 8 * size, 8 * align, 8 * offset, flags, di_type);
}

static void make_debug_fields(compile_t* c, reach_type_t* t)
{
  LLVMMetadataRef fields = NULL;

  if(t->field_count > 0)
  {
    size_t buf_size = t->field_count * sizeof(LLVMMetadataRef);
    LLVMMetadataRef* data = (LLVMMetadataRef*)ponyint_pool_alloc_size(
      buf_size);

    for(uint32_t i = 0; i < t->field_count; i++)
      data[i] = make_debug_field(c, t, i);

    fields = LLVMDIBuilderGetOrCreateArray(c->di, data, t->field_count);
    ponyint_pool_free_size(buf_size, data);
  }

  LLVMTypeRef type;

  if(t->underlying != TK_TUPLETYPE)
    type = t->structure;
  else
    type = t->primitive;

  uint64_t size = 0;
  uint64_t align = 0;

  if(type != NULL)
  {
    size = LLVMABISizeOfType(c->target_data, type);
    align = LLVMABIAlignmentOfType(c->target_data, type);
  }

  LLVMMetadataRef di_type = LLVMDIBuilderCreateStructType(c->di, c->di_unit,
    t->name, t->di_file, (unsigned) ast_line(t->ast), 8 * size, 8 * align,
    fields);

  if(t->underlying != TK_TUPLETYPE)
  {
    LLVMMetadataReplaceAllUsesWith(t->di_type_embed, di_type);
    t->di_type_embed = di_type;
  } else {
    LLVMMetadataReplaceAllUsesWith(t->di_type, di_type);
    t->di_type = di_type;
  }
}

static void make_debug_final(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      make_debug_fields(c, t);
      return;

    case TK_PRIMITIVE:
    {
      if(t->primitive == NULL)
        make_debug_fields(c, t);
      return;
    }

    default: {}
  }

  assert(0);
}

static void make_intrinsic_methods(compile_t* c, reach_type_t* t)
{
  if(ast_id(t->ast) != TK_NOMINAL)
    return;

  // Find the primitive type, if there is one.
  AST_GET_CHILDREN(t->ast, pkg, id);
  const char* package = ast_name(pkg);
  const char* name = ast_name(id);

  if(package == c->str_builtin)
  {
    if(name == c->str_Pointer)
      genprim_pointer_methods(c, t);
    else if(name == c->str_Maybe)
      genprim_maybe_methods(c, t);
    else if(name == c->str_DoNotOptimise)
      genprim_donotoptimise_methods(c, t);
    else if(name == c->str_Platform)
      genprim_platform_methods(c, t);
    else if(name == c->str_Vector)
      genprim_vector_methods(c, t);
  }
}

static bool make_trace(compile_t* c, reach_type_t* t)
{
  if(t->trace_fn == NULL)
    return true;

  if(t->underlying == TK_CLASS)
  {
    // Special case the array trace function.
    AST_GET_CHILDREN(t->ast, pkg, id);
    const char* package = ast_name(pkg);
    const char* name = ast_name(id);

    if(package == c->str_builtin)
    {
      if(name == c->str_Array)
      {
        genprim_array_trace(c, t);
        return true;
      }
      else if(name == c->str_Vector)
      {
        genprim_vector_trace(c, t);
        return true;
      }
    }
  }

  // Generate the trace function.
  codegen_startfun(c, t->trace_fn, NULL, NULL);
  LLVMSetFunctionCallConv(t->trace_fn, LLVMCCallConv);
  LLVMSetLinkage(t->trace_fn, LLVMExternalLinkage);

  LLVMValueRef ctx = LLVMGetParam(t->trace_fn, 0);
  LLVMValueRef arg = LLVMGetParam(t->trace_fn, 1);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, arg, t->structure_ptr,
    "object");

  int extra = 0;

  // Non-structs have a type descriptor.
  if(t->underlying != TK_STRUCT)
    extra++;

  // Actors have a pad.
  if(t->underlying == TK_ACTOR)
    extra++;

  for(uint32_t i = 0; i < t->field_count; i++)
  {
    LLVMValueRef field = LLVMBuildStructGEP(c->builder, object, i + extra, "");

    if(!t->fields[i].embed)
    {
      // Call the trace function indirectly depending on rcaps.
      LLVMValueRef value = LLVMBuildLoad(c->builder, field, "");
      gentrace(c, ctx, value, t->fields[i].ast);
    } else {
      // Call the trace function directly without marking the field.
      LLVMValueRef trace_fn = t->fields[i].type->trace_fn;

      if(trace_fn != NULL)
      {
        LLVMValueRef args[2];
        args[0] = ctx;
        args[1] = LLVMBuildBitCast(c->builder, field, c->object_ptr, "");

        LLVMBuildCall(c->builder, trace_fn, args, 2, "");
      }
    }
  }

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  return true;
}

bool gentypes(compile_t* c)
{
  reach_type_t* t;
  size_t i;

  genprim_builtins(c);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Data prototypes\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_opaque_struct(c, t))
      return false;

    gendesc_type(c, t);
    make_debug_info(c, t);
    make_box_type(c, t);
    make_dispatch(c, t);
    gentrace_prototype(c, t);
  }

  gendesc_table(c);

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Data types\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_struct(c, t))
      return false;

    make_global_instance(c, t);
  }

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Function prototypes\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    // The ABI size for machine words and tuples is the boxed size.
    if(t->structure != NULL)
      t->abi_size = (size_t)LLVMABISizeOfType(c->target_data, t->structure);

    make_debug_final(c, t);
    make_intrinsic_methods(c, t);

    if(!genfun_method_sigs(c, t))
      return false;
  }

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Functions\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!genfun_method_bodies(c, t))
      return false;
  }

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Descriptors\n");

  i = HASHMAP_BEGIN;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(!make_trace(c, t))
      return false;

    if(!genserialise(c, t))
      return false;

    gendesc_init(c, t);
  }

  return true;
}
